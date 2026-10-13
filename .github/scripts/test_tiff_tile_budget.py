#!/usr/bin/env python3
"""Check aggregate tiled-input memory and mixed-channel pixel parity."""

import argparse
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile


def run(*args, success=True):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, timeout=60)
    if success and result.returncode:
        raise AssertionError(f"{args}: {result.returncode}\n{result.stderr!r}")
    clean_rejection = 0 < result.returncode < 128 or result.returncode == 255
    clean_rejection |= os.name == "nt" and result.returncode == 0xFFFFFFFF
    if not success and not clean_rejection:
        raise AssertionError(f"Expected clean rejection: {args}: {result.returncode}\n{result.stderr!r}")
    if b"ERROR: AddressSanitizer" in result.stderr or b"runtime error:" in result.stderr:
        raise AssertionError(result.stderr.decode("ascii", errors="backslashreplace"))
    return result


def max_tile_geometry(source):
    data = bytearray(source.read_bytes())
    if data[:4] != b"II*\x00":
        raise AssertionError("Expected little-endian classic TIFF fixture")
    offset = struct.unpack_from("<I", data, 4)[0]
    count = struct.unpack_from("<H", data, offset)[0]
    updated = set()
    for index in range(count):
        entry = offset + 2 + index * 12
        tag, kind, length = struct.unpack_from("<HHI", data, entry)
        if tag in (322, 323):
            if kind != 3 or length != 1:
                raise AssertionError("Unexpected tile geometry encoding")
            struct.pack_into("<H", data, entry + 8, 8192 if tag == 322 else 32768)
            updated.add(tag)
    if updated != {322, 323}:
        raise AssertionError("Missing tile geometry")
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--specsep", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="iccdev-tiff-tile-budget-") as temp:
        root = pathlib.Path(temp)
        run(args.fixture, "--fixtures", root)
        tiled = root / "gray8_1"
        stripped = root / "single-strip.tif"
        run(args.specsep, stripped, 0, 0, root / "gray8_", 1, 1, 1)

        for channel in range(1, 17):
            shutil.copyfile(tiled if channel % 2 else stripped,
                            root / f"mixed_{channel}")
        strip_output = root / "mixed-strip.tif"
        tile_output = root / "mixed-tile.tif"
        run(args.specsep, strip_output, 0, 1, root / "mixed_", 1, 16, 1)
        run(args.specsep, "--tiles", 16, 32, tile_output, 0, 1,
            root / "mixed_", 1, 16, 1)
        run(args.fixture, "--compare", strip_output, tile_output)

        shutil.copyfile(tiled, root / "huge_1")
        (root / "huge_2").write_bytes(max_tile_geometry(tiled))
        rejected = root / "rejected.tif"
        result = run(args.specsep, rejected, 0, 1, root / "huge_", 1, 2, 1,
                     success=False)
        if b"caller memory limit" not in result.stderr or rejected.exists():
            raise AssertionError("Aggregate tile-band limit did not reject before output creation")
    print("[OK] 16 mixed inputs match and aggregate tile bands reject before allocation")


if __name__ == "__main__":
    main()
