#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Check or normalize owned C/C++ notices and reject legacy ICC v0.2 markers."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".mm",
}
SCRIPT_SUFFIXES = {".py", ".sh"}
EXCLUDED_FILES = {
    Path("IccProfLib/IccConvertUTF.cpp"),
    Path("IccProfLib/IccConvertUTF.h"),
    Path("IccProfLib/IccMD5.cpp"),
    Path("IccProfLib/IccMD5.h"),
    Path("IccProfLib/icProfileHeader.h"),
}
PACKAGE_LICENSE_FILES = {
    Path("python/LICENSE.md"),
    Path("iccdev-mcp/LICENSE.md"),
}
LEGACY_MARKER = "Redistribution and use in source and binary forms"
LEGACY_V02_MARKER = "ICC Software License, Version " + "0.2"
HEADER_SCAN_BYTES = 16 * 1024


def repository_root() -> Path:
    return Path(__file__).resolve().parents[2]


def tracked_files(root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=True,
        capture_output=True,
    )
    return sorted(Path(name.decode("utf-8")) for name in result.stdout.split(b"\0") if name)


def tracked_source_files(root: Path) -> list[Path]:
    files = [path for path in tracked_files(root) if path.suffix in SOURCE_SUFFIXES]
    return sorted(path for path in files if path not in EXCLUDED_FILES)


def tracked_executable_files(root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-s", "-z"],
        check=True,
        capture_output=True,
    )
    return sorted(
        Path(entry.partition(b"\t")[2].decode("utf-8"))
        for entry in result.stdout.split(b"\0")
        if entry and entry.split(b" ", 1)[0] == b"100755"
    )


def tracked_notice_files(root: Path) -> list[Path]:
    return sorted(
        set(tracked_source_files(root))
        | {path for path in tracked_files(root) if path.suffix in SCRIPT_SUFFIXES}
        | set(tracked_executable_files(root))
    )


def canonical_notice(root: Path) -> str:
    license_text = (root / "LICENSE.md").read_text(encoding="ascii").strip()
    if not license_text.startswith("Copyright (c) International Color Consortium."):
        raise ValueError("LICENSE.md does not begin with the ICC copyright notice")

    lines = license_text.splitlines()
    return "\n".join(
        [
            "/*",
            f" * {lines[0]}",
            " * SPDX-License-Identifier: BSD-3-Clause",
            *[f" * {line}" if line else " *" for line in lines[1:]],
            " */",
            "",
        ]
    )


def normalized_license_contents(contents: bytes) -> bytes:
    return contents.rstrip(b" \t\r\n") + b"\n"


def has_legacy_v02_marker(text: str) -> bool:
    return LEGACY_V02_MARKER in text


def replace_legacy_notice(text: str, notice: str, path: Path) -> str:
    marker_index = text.find(LEGACY_MARKER)
    if marker_index < 0:
        return text

    block_start = text.rfind("/*", 0, marker_index)
    block_end = text.find("*/", marker_index)
    if block_start < 0 or block_end < 0:
        raise ValueError(f"{path}: cannot locate complete legacy license block")

    block = text[block_start : block_end + 2]
    if "International Color Consortium" not in block:
        return text

    return text[:block_start] + notice + text[block_end + 2 :].lstrip("\r\n")


def normalized_text(text: str, notice: str, path: Path) -> str:
    first_newline = text.find("\n")
    if first_newline > 0 and text[first_newline - 1] == "\r":
        notice = notice.replace("\n", "\r\n")

    if notice in text[:HEADER_SCAN_BYTES]:
        return text

    replaced = replace_legacy_notice(text, notice, path)
    if replaced != text:
        return replaced

    return notice + text


def check_or_write(root: Path, write: bool) -> int:
    notice = canonical_notice(root)
    failures = []

    for relative_path in tracked_source_files(root):
        path = root / relative_path
        text = path.read_bytes().decode("latin-1")
        normalized = normalized_text(text, notice, relative_path)

        if normalized == text:
            continue

        if write:
            path.write_bytes(normalized.encode("latin-1"))
        else:
            failures.append(relative_path)

    canonical_license = normalized_license_contents((root / "LICENSE.md").read_bytes())
    for relative_path in sorted(PACKAGE_LICENSE_FILES):
        package_license = root / relative_path
        if not package_license.is_file():
            failures.append(relative_path)
            continue
        if normalized_license_contents(package_license.read_bytes()) != canonical_license:
            failures.append(relative_path)

    for relative_path in tracked_notice_files(root):
        if has_legacy_v02_marker((root / relative_path).read_bytes().decode("latin-1")):
            failures.append(relative_path)

    if failures:
        for path in sorted(set(failures)):
            print(f"[FAIL] missing, noncanonical, or legacy BSD notice: {path}")
        return 1

    print(
        "[OK] canonical BSD-3-Clause notices cover owned tracked C/C++ "
        "sources and scripts, with no legacy ICC v0.2 marker; package license "
        "copies match the repository license"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check canonical BSD-3-Clause C/C++ notices and legacy ICC v0.2 markers."
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="replace legacy notices and add missing canonical notices",
    )
    args = parser.parse_args()

    try:
        return check_or_write(repository_root(), args.write)
    except (OSError, UnicodeDecodeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
