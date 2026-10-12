#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Validate the universal MCP wheel and source distribution before release."""

import argparse
import configparser
from email.parser import BytesParser
from pathlib import Path
import re
import runpy
import stat
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]
VALIDATE_RECORD = runpy.run_path(str(Path(__file__).with_name("validate-python-release-wheels.py")))["validate_record"]


def package_version():
    text = (ROOT / "iccdev-mcp/iccdev_mcp/__init__.py").read_text(encoding="utf-8")
    return re.search(r'^__version__ = "([^"]+)"$', text, re.M).group(1)


def safe_path(name):
    return (all(part not in ("", ".", "..") for part in name.rstrip("/").split("/"))
            and "\\" not in name and ":" not in name
            and not any(ord(char) < 32 or ord(char) == 127 for char in name))


def metadata(data, version):
    headers = BytesParser().parsebytes(data)
    if headers.defects:
        raise ValueError("Malformed MCP package metadata")
    for key, value in {"Name": "iccdev-mcp", "Version": version,
                       "Requires-Python": ">=3.10", "License-Expression": "BSD-3-Clause",
                       "License-File": "LICENSE.md"}.items():
        if headers.get_all(key, []) != [value]:
            raise ValueError(f"Unexpected MCP package metadata: {key}")


def canonical_license(data):
    if data.decode("utf-8").split() != (ROOT / "LICENSE.md").read_text(encoding="utf-8").split():
        raise ValueError("MCP package license differs from canonical LICENSE.md")


def validate_packages(directory, version):
    wheel = directory / f"iccdev_mcp-{version}-py3-none-any.whl"
    sdist = directory / f"iccdev_mcp-{version}.tar.gz"
    actual = {path for path in directory.iterdir() if path.name.startswith(("iccdev_mcp-", "iccdev-mcp-"))}
    if actual != {wheel, sdist} or not all(path.is_file() for path in actual):
        raise ValueError("Missing or unexpected MCP release package inventory")
    modules = {path.name for path in (ROOT / "iccdev-mcp/iccdev_mcp").glob("*.py")}
    dist_info = f"iccdev_mcp-{version}.dist-info"
    with zipfile.ZipFile(wheel) as archive:
        members = archive.infolist()
        names = {member.filename for member in members}
        if len(names) != len(members) or archive.testzip() is not None:
            raise ValueError("Duplicate or corrupt MCP wheel member")
        for member in members:
            if (member.orig_filename != member.filename or not safe_path(member.orig_filename)
                    or stat.S_IFMT(member.external_attr >> 16) not in (0, stat.S_IFDIR, stat.S_IFREG)):
                raise ValueError("Unsafe MCP wheel archive member")
            if member.filename.split("/", 1)[0] not in ("iccdev_mcp", dist_info):
                raise ValueError("MCP wheel installs an unexpected namespace")
            if member.filename.endswith((".so", ".pyd", ".dll", ".a", ".lib")):
                raise ValueError("MCP wheel must remain pure Python")
        required = {f"iccdev_mcp/{name}" for name in modules} | {
            f"{dist_info}/{name}" for name in ("METADATA", "WHEEL", "RECORD", "entry_points.txt", "licenses/LICENSE.md")}
        if not required <= names:
            raise ValueError("Incomplete MCP wheel")
        metadata(archive.read(f"{dist_info}/METADATA"), version)
        headers = BytesParser().parsebytes(archive.read(f"{dist_info}/WHEEL"))
        for key, value in {"Wheel-Version": "1.0", "Root-Is-Purelib": "true", "Tag": "py3-none-any"}.items():
            if headers.get_all(key, []) != [value]:
                raise ValueError(f"Unexpected MCP wheel metadata: {key}")
        entries = configparser.ConfigParser()
        entries.read_string(archive.read(f"{dist_info}/entry_points.txt").decode("utf-8"))
        if dict(entries["console_scripts"]) != {
            "iccdev-mcp": "iccdev_mcp.server:main", "iccdev-mcp-rest": "iccdev_mcp.rest_api:main"}:
            raise ValueError("MCP console entry points disagree with package contract")
        canonical_license(archive.read(f"{dist_info}/licenses/LICENSE.md"))
        VALIDATE_RECORD(archive, dist_info)
    prefix = f"iccdev_mcp-{version}/"
    with tarfile.open(sdist, "r:gz") as archive:
        members = archive.getmembers()
        names = {member.name for member in members}
        if len(names) != len(members):
            raise ValueError("Duplicate MCP sdist member")
        for member in members:
            if (not safe_path(member.name) or not member.name.startswith(prefix)
                    or not (member.isfile() or member.isdir())):
                raise ValueError("Unsafe MCP sdist member")
            if member.name.startswith(prefix + "iccdev/"):
                raise ValueError("MCP sdist includes the conflicting iccdev namespace")
        required = {prefix + name for name in ("pyproject.toml", "README.md", "LICENSE.md", "PKG-INFO")}
        required.update(prefix + "iccdev_mcp/" + name for name in modules)
        if not required <= names or not all(archive.getmember(name).isfile() for name in required):
            raise ValueError("Incomplete MCP source distribution")
        metadata(archive.extractfile(prefix + "PKG-INFO").read(), version)
        canonical_license(archive.extractfile(prefix + "LICENSE.md").read())
    return [wheel, sdist]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    try:
        packages = validate_packages(args.directory, package_version())
    except (ValueError, OSError, KeyError, configparser.Error, tarfile.TarError, zipfile.BadZipFile) as error:
        parser.exit(1, f"[FAIL] {error}\n")
    print(f"[OK] Validated {len(packages)} MCP release packages")


if __name__ == "__main__":
    main()
