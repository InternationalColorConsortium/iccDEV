"""Validate the exact cibuildwheel output before release upload.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

import argparse
from email.parser import BytesParser
from pathlib import Path
import re
import zipfile


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
    if expected.startswith(("manylinux_", "musllinux_")):
        family, arch = expected.split("_", 1)
        return platform.startswith(family) and platform.endswith("_" + arch)
    if expected.startswith("macosx_"):
        return platform.startswith("macosx_") and platform.endswith(expected[6:])
    return platform == expected


def validate_wheels(wheel_dir, identifiers, version):
    expected = set(identifiers)
    if not expected or any(
        not re.fullmatch(r"cp3(?:9|10|11|12|13)-[a-z0-9_]+", item)
        for item in expected
    ):
        raise ValueError("Missing or unsupported cibuildwheel identifiers")
    wheels = sorted(wheel_dir.glob("*.whl"))
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
            and any(matches_platform(tag, identifier.split("-", 1)[1])
                    for tag in platforms.split("."))
        }
        if len(matching) != 1 or matching & covered:
            raise ValueError(f"Unexpected or duplicate wheel target: {wheel.name}")
        with zipfile.ZipFile(wheel) as archive:
            if archive.testzip() is not None:
                raise ValueError(f"Corrupt wheel: {wheel.name}")
            names = set(archive.namelist())
            metadata_path = f"iccdev-{version}.dist-info/METADATA"
            metadata = BytesParser().parsebytes(archive.read(metadata_path))
            if metadata["Name"] != "iccdev" or metadata["Version"] != version:
                raise ValueError(f"Unexpected package metadata: {wheel.name}")
            required = {
                "iccdev/__init__.py", "iccdev/py.typed", "iccdev/_iccdev.pyi",
                f"iccdev-{version}.dist-info/licenses/LICENSE.md",
            }
            if not required <= names or not any(
                name.startswith("iccdev/_iccdev.") and name.endswith((".so", ".pyd"))
                for name in names
            ):
                raise ValueError(f"Missing native extension, types, or license: {wheel.name}")
        covered.update(matching)
    if covered != expected:
        raise ValueError(f"Missing wheel targets: {sorted(expected - covered)}")
    return wheels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wheel_dir", type=Path)
    parser.add_argument("--identifiers", type=Path, required=True)
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    try:
        wheels = validate_wheels(
            args.wheel_dir,
            args.identifiers.read_text(encoding="ascii").split(),
            package_version(repo_root),
        )
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"[FAIL] {error}\n")
    print(f"[OK] Validated {len(wheels)} release wheels in {args.wheel_dir}")


if __name__ == "__main__":
    main()
