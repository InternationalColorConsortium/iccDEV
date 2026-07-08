#!/usr/bin/env python3
"""
vendor_iccproflib.py - Copy IccProfLib sources into vendor/ for sdist packaging.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.

Run before `python -m build --sdist` to create a self-contained source
distribution that compiles IccProfLib as part of `pip install`.

Usage:
    python vendor_iccproflib.py          # copies sources to vendor/IccProfLib/
    python vendor_iccproflib.py --clean  # removes vendor/IccProfLib/
"""

import os
import hashlib
import shutil
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
SRC_DIR = os.path.join(REPO_ROOT, "IccProfLib")
DST_DIR = os.path.join(SCRIPT_DIR, "vendor", "IccProfLib")
MANIFEST = os.path.join(DST_DIR, "MANIFEST.sha256")


def vendor():
    if not os.path.isdir(SRC_DIR):
        print(f"ERROR: IccProfLib source not found at {SRC_DIR}", file=sys.stderr)
        sys.exit(1)

    if os.path.isdir(DST_DIR):
        shutil.rmtree(DST_DIR)
    os.makedirs(DST_DIR, exist_ok=True)

    copied = 0
    manifest_rows = []
    for name in sorted(os.listdir(SRC_DIR)):
        if name.endswith((".cpp", ".h")):
            src = os.path.join(SRC_DIR, name)
            dst = os.path.join(DST_DIR, name)
            shutil.copy2(src, dst)
            with open(dst, "rb") as f:
                digest = hashlib.sha256(f.read()).hexdigest()
            manifest_rows.append(f"{digest}  {name}\n")
            copied += 1

    with open(MANIFEST, "w", encoding="ascii", newline="\n") as f:
        f.writelines(manifest_rows)

    print(f"Vendored {copied} files from IccProfLib -> vendor/IccProfLib/")
    print(f"Wrote SHA-256 manifest: {os.path.relpath(MANIFEST, SCRIPT_DIR)}")


def clean():
    if os.path.isdir(DST_DIR):
        shutil.rmtree(DST_DIR)
        print("Removed vendor/IccProfLib/")
    else:
        print("vendor/IccProfLib/ does not exist")


if __name__ == "__main__":
    if "--clean" in sys.argv:
        clean()
    else:
        vendor()
