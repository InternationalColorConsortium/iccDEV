#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

"""Build a small carrier whose repeated ICC associations exceed the output cap."""

import struct
import sys
from pathlib import Path


def box(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I4s", len(payload) + 8, kind) + payload


def main() -> int:
    if len(sys.argv) != 3:
        return 2

    count = int(sys.argv[2])
    if count < 1 or count > 255:
        return 2

    # 65 references to one 4 MiB property request 260 MiB of extracted bytes.
    profile = bytes(4 * 1024 * 1024)
    ipco = box(b"ipco", box(b"colr", b"prof" + profile))
    associations = b"\x01" * count
    ipma = box(
        b"ipma",
        bytes(4) + struct.pack(">IHB", 1, 1, len(associations)) + associations,
    )
    meta = box(b"meta", bytes(4) + box(b"iprp", ipco + ipma))
    Path(sys.argv[1]).write_bytes(meta)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
