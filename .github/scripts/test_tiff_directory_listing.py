#!/usr/bin/env python3
"""Check TIFF directory inventory and its 4096-IFD bound."""

import argparse
import json
import pathlib
import struct
import subprocess
import tempfile


def make_chain(path, count):
    tags = ((256, 3, 1), (257, 3, 1), (258, 3, 8), (259, 3, 1),
            (262, 3, 1), (273, 4, 8), (277, 3, 1), (278, 4, 1),
            (279, 4, 1))
    first = 10
    size = 2 + len(tags) * 12 + 4
    data = bytearray(struct.pack("<2sHI", b"II", 42, first) + b"\x00\x00")
    for index in range(count):
        data.extend(struct.pack("<H", len(tags)))
        for tag, kind, value in tags:
            data.extend(struct.pack("<HHII", tag, kind, 1, value))
        data.extend(struct.pack("<I", first + (index + 1) * size
                                if index + 1 < count else 0))
    path.write_bytes(data)


def inventory(tool, path, evidence):
    options = ["--evidence-json", "--list-directories"] if evidence else ["--list-directories"]
    return subprocess.run([tool, *options, str(path)], capture_output=True, timeout=60)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dump", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="iccdev-tiff-ifd-list-") as temp:
        root = pathlib.Path(temp)
        allowed = root / "chain-4096.tif"
        make_chain(allowed, 4096)
        plain = inventory(args.dump, allowed, False)
        if plain.returncode:
            raise AssertionError(f"Plain listing failed: {plain.stderr!r}")
        lines = plain.stdout.decode("ascii").splitlines()
        if len(lines) != 4096 or not lines[0].startswith("IFD 0: 1 x 1,") or \
                not lines[-1].startswith("IFD 4095: 1 x 1,"):
            raise AssertionError("Plain inventory missed a main IFD")

        evidence = inventory(args.dump, allowed, True)
        if evidence.returncode:
            raise AssertionError(f"JSON listing failed: {evidence.stderr!r}")
        directories = json.loads(evidence.stdout)["directories"]
        if len(directories) != 4096 or directories[0]["directory"] != 0 or \
                directories[-1]["directory"] != 4095 or \
                any(item["width"] != 1 or item["height"] != 1 or
                    item["blockCount"] != 1 for item in directories):
            raise AssertionError("JSON inventory lost main-IFD metadata")

        excessive = root / "chain-4097.tif"
        make_chain(excessive, 4097)
        rejected = inventory(args.dump, excessive, True)
        if rejected.returncode == 0 or b"more than 4096 directories" not in rejected.stderr:
            raise AssertionError("4097-IFD chain was not rejected")
    print("[OK] plain/JSON 4096-IFD inventory and 4097-IFD rejection")


if __name__ == "__main__":
    main()
