#!/usr/bin/env python3
###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Keep CodeQL bundle version and checksum pins synchronized.
#
###############################################################
"""Verify the CodeQL bundle pin is identical in every bootstrap location."""

import argparse
from pathlib import Path
import re
import sys


PIN_SITES = {
    "Dockerfile": {
        "version": r"^ARG CODEQL_VERSION=([0-9]+(?:\.[0-9]+){2})$",
        "sha256": r"^ARG CODEQL_SHA256=([0-9a-f]{64})$",
    },
    ".github/workflows/ci-codeql-query-tests.yml": {
        "version": r'^\s*CODEQL_VER="([0-9]+(?:\.[0-9]+){2})"$',
        "sha256": r'^\s*CODEQL_SHA256="([0-9a-f]{64})"$',
    },
    ".github/workflows/ci-codeql-security.yml": {
        "version": r'^\s*CODEQL_VER="([0-9]+(?:\.[0-9]+){2})"$',
        "sha256": r'^\s*CODEQL_SHA256="([0-9a-f]{64})"$',
    },
    ".github/workflows/ci-preflight-safety.yml": {
        "version": r'^\s*CODEQL_VER="([0-9]+(?:\.[0-9]+){2})"$',
        "sha256": r'^\s*CODEQL_SHA256="([0-9a-f]{64})"$',
    },
}


def extract_pin(path, name, pattern):
    matches = re.findall(pattern, path.read_text(encoding="ascii"), re.MULTILINE)
    if len(matches) != 1:
        raise ValueError(f"{path}: expected one CodeQL {name} pin, found {len(matches)}")
    return matches[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, help="repository root to validate")
    args = parser.parse_args()

    root = args.root.resolve() if args.root else Path(__file__).resolve().parents[2]
    pins = {}

    try:
        for relative_path, patterns in PIN_SITES.items():
            path = root / relative_path
            pins[relative_path] = {
                name: extract_pin(path, name, pattern)
                for name, pattern in patterns.items()
            }
    except (OSError, ValueError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1

    expected = next(iter(pins.values()))
    mismatches = [
        f"{path}: version={pin['version']} sha256={pin['sha256']}"
        for path, pin in pins.items()
        if pin != expected
    ]
    if mismatches:
        print("[FAIL] CodeQL bundle pins differ:", file=sys.stderr)
        for mismatch in mismatches:
            print(f"  {mismatch}", file=sys.stderr)
        return 1

    print(
        "[OK] CodeQL bundle pins: "
        f"version={expected['version']} sha256={expected['sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
