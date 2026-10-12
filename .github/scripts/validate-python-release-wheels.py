# Copyright (c) International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     1. Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#
#     2. Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#
#     3. Neither the name of International Color Consortium (ICC) nor the names of
#        its contributors may be used to endorse or promote products derived from
#        this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Validate the exact cibuildwheel output before release upload."""

import argparse
import base64
import csv
from email.parser import BytesParser
import hashlib
import io
from pathlib import Path
import re
import stat
import zipfile


LANE_PLATFORMS = {
    "linux-x86_64": ("manylinux_x86_64", "musllinux_x86_64"),
    "linux-aarch64": ("manylinux_aarch64", "musllinux_aarch64"),
    "macos-x86_64": ("macosx_x86_64",),
    "macos-arm64": ("macosx_arm64",),
    "macos-universal2": ("macosx_universal2",),
    "windows-amd64": ("win_amd64",),
    "windows-arm64": ("win_arm64",),
}
PYTHON_TAGS = ("cp39", "cp310", "cp311", "cp312", "cp313")


def lane_identifiers(lane):
    return {f"{python}-{platform}" for python in PYTHON_TAGS
            for platform in LANE_PLATFORMS[lane]}


def package_version(repo_root):
    values = []
    for path, field in (
        ("python/pyproject.toml", "version"),
        ("python/iccdev/__init__.py", "__version__"),
        ("python/setup.py", "version"),
    ):
        content = (repo_root / path).read_text(encoding="utf-8")
        match = re.search(r'(?m)^\s*' + field + r'\s*=\s*"([^"]+)"', content)
        if not match:
            raise ValueError(f"Missing version in {path}")
        values.append(match.group(1))
    if len(set(values)) != 1:
        raise ValueError(f"Package versions disagree: {values}")
    return values[0]


def matches_platform(platform, expected):
    if expected.startswith("manylinux_"):
        arch = re.escape(expected.removeprefix("manylinux_"))
        return re.fullmatch(r"manylinux(?:1|2010|2014|_[0-9]+_[0-9]+)_" + arch, platform) is not None
    if expected.startswith("musllinux_"):
        arch = re.escape(expected.removeprefix("musllinux_"))
        return re.fullmatch(r"musllinux_[0-9]+_[0-9]+_" + arch, platform) is not None
    if expected.startswith("macosx_"):
        arch = re.escape(expected.removeprefix("macosx_"))
        return re.fullmatch(r"macosx_[0-9]+_[0-9]+_" + arch, platform) is not None
    return platform == expected


def read_headers(archive, path):
    headers = BytesParser().parsebytes(archive.read(path))
    if headers.defects:
        raise ValueError(f"Malformed wheel metadata: {path}")
    return headers


def require_header(headers, name, expected):
    if headers.get_all(name, []) != [expected]:
        raise ValueError(f"Unexpected wheel metadata {name}")


def validate_record(archive, dist_info):
    record = f"{dist_info}/RECORD"
    signatures = {record + ".jws", record + ".p7s"}
    files = {member.filename for member in archive.infolist() if not member.is_dir()}
    expected = files - signatures
    recorded = set()
    try:
        rows = csv.reader(io.StringIO(archive.read(record).decode("utf-8")), strict=True)
        for row in rows:
            if len(row) != 3:
                raise ValueError("Malformed wheel RECORD row")
            name, checksum, size = row
            if name not in expected or name in recorded:
                raise ValueError(f"Unexpected or duplicate wheel RECORD entry: {name}")
            recorded.add(name)
            if name == record:
                if checksum or size:
                    raise ValueError("Wheel RECORD must not hash itself")
                continue
            algorithm, separator, digest = checksum.partition("=")
            if not separator or algorithm not in ("sha256", "sha384", "sha512"):
                raise ValueError(f"Missing or unsupported wheel RECORD hash: {name}")
            hasher = hashlib.new(algorithm)
            with archive.open(name) as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    hasher.update(chunk)
            actual = base64.urlsafe_b64encode(hasher.digest()).rstrip(b"=").decode("ascii")
            if digest != actual or size != str(archive.getinfo(name).file_size):
                raise ValueError(f"Wheel RECORD hash or size mismatch: {name}")
    except csv.Error as error:
        raise ValueError("Malformed wheel RECORD CSV") from error
    if recorded != expected:
        raise ValueError(f"Missing wheel RECORD entries: {sorted(expected - recorded)}")


def validate_archive(archive, version, python_tag, abi, platforms):
    members = archive.infolist()
    names = {member.filename for member in members}
    if len(names) != len(members):
        raise ValueError("Duplicate wheel archive member")
    for member in members:
        # ZipInfo normalizes Windows separators and truncates embedded NULs.
        # Reject the original archive path rather than accepting that rewrite.
        name = member.orig_filename
        components = name.rstrip("/").split("/")
        mode = stat.S_IFMT(member.external_attr >> 16)
        if (name != member.filename
                or any(part in ("", ".", "..") for part in components)
                or "\\" in name or ":" in name
                or any(ord(char) < 32 or ord(char) == 127 for char in name)
                or mode not in (0, stat.S_IFREG, stat.S_IFDIR)):
            raise ValueError(f"Unsafe wheel archive member: {name!r}")
    if archive.testzip() is not None:
        raise ValueError("Corrupt wheel archive")
    dist_info = f"iccdev-{version}.dist-info"
    if {name.split("/", 1)[0] for name in names if ".dist-info/" in name} != {dist_info}:
        raise ValueError("Unexpected wheel dist-info directory")
    required = {
        "iccdev/__init__.py", "iccdev/py.typed", "iccdev/_iccdev.pyi",
        f"{dist_info}/METADATA", f"{dist_info}/WHEEL", f"{dist_info}/RECORD",
        f"{dist_info}/licenses/LICENSE.md",
    }
    if not required <= names or not any(
        re.fullmatch(r"iccdev/_iccdev\.[^/]+\.(so|pyd)", name)
        for name in names
    ):
        raise ValueError("Missing wheel metadata, native extension, types, or license")
    metadata = read_headers(archive, f"{dist_info}/METADATA")
    require_header(metadata, "Name", "iccdev")
    require_header(metadata, "Version", version)
    require_header(metadata, "License-Expression", "BSD-3-Clause")
    require_header(metadata, "License-File", "LICENSE.md")
    wheel_metadata = read_headers(archive, f"{dist_info}/WHEEL")
    require_header(wheel_metadata, "Wheel-Version", "1.0")
    require_header(wheel_metadata, "Root-Is-Purelib", "false")
    tags = wheel_metadata.get_all("Tag", [])
    expected_tags = {f"{python_tag}-{abi}-{platform}" for platform in platforms}
    if len(tags) != len(expected_tags) or set(tags) != expected_tags:
        raise ValueError("Wheel metadata tags disagree with filename")
    license_text = archive.read(f"{dist_info}/licenses/LICENSE.md").decode("utf-8")
    canonical = (Path(__file__).resolve().parents[2] / "LICENSE.md").read_text(encoding="utf-8")
    if license_text.split() != canonical.split():
        raise ValueError("Wheel license does not match canonical LICENSE.md")
    validate_record(archive, dist_info)


def validate_wheels(wheel_dir, identifiers, version, lane=None, allow_mcp=False):
    identifiers = list(identifiers)
    expected = set(identifiers)
    supported = set().union(*(lane_identifiers(name) for name in LANE_PLATFORMS))
    if not expected or not expected <= supported:
        raise ValueError("Missing or unsupported cibuildwheel identifiers")
    if len(expected) != len(identifiers):
        raise ValueError("Duplicate cibuildwheel identifiers")
    if lane is not None and (lane not in LANE_PLATFORMS or expected != lane_identifiers(lane)):
        raise ValueError(f"Identifier inventory disagrees with lane: {lane}")
    wheels = sorted(path for path in wheel_dir.glob("*.whl")
                    if not (allow_mcp and path.name.startswith("iccdev_mcp-")))
    covered = set()
    for wheel in wheels:
        parts = wheel.name[:-4].split("-")
        if len(parts) != 5 or parts[:2] != ["iccdev", version]:
            raise ValueError(f"Unexpected wheel name or version: {wheel.name}")
        python_tag, abi, platforms = parts[2:]
        if abi != python_tag:
            raise ValueError(f"Unexpected wheel ABI: {wheel.name}")
        matching = {
            identifier for identifier in expected
            if identifier.split("-", 1)[0] == python_tag
            and all(matches_platform(tag, identifier.split("-", 1)[1])
                    for tag in platforms.split("."))
        }
        if len(matching) != 1 or matching & covered:
            raise ValueError(f"Unexpected or duplicate wheel target: {wheel.name}")
        with zipfile.ZipFile(wheel) as archive:
            validate_archive(archive, version, python_tag, abi, platforms.split("."))
        covered.update(matching)
    if covered != expected:
        raise ValueError(f"Missing wheel targets: {sorted(expected - covered)}")
    return wheels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wheel_dir", type=Path)
    inventory = parser.add_mutually_exclusive_group(required=True)
    inventory.add_argument("--identifiers", type=Path)
    inventory.add_argument("--release", action="store_true",
                           help="Require all 45 iccdev binding wheels; validate MCP separately")
    parser.add_argument("--lane", choices=sorted(LANE_PLATFORMS),
                        help="Require the complete repository inventory for this CI lane")
    args = parser.parse_args()
    if args.release and args.lane:
        parser.error("--lane requires --identifiers, not --release")
    repo_root = Path(__file__).resolve().parents[2]
    try:
        wheels = validate_wheels(
            args.wheel_dir,
            (sorted(set().union(*(lane_identifiers(lane) for lane in LANE_PLATFORMS)))
             if args.release else args.identifiers.read_text(encoding="ascii").split()),
            package_version(repo_root),
            lane=args.lane,
            allow_mcp=args.release,
        )
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"[FAIL] {error}\n")
    print(f"[OK] Validated {len(wheels)} release wheels in {args.wheel_dir}")


if __name__ == "__main__":
    main()
