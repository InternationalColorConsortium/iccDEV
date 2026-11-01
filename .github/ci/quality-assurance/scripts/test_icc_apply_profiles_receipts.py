#!/usr/bin/env python3
################################################################################
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
################################################################################
"""Cross-platform ApplyProfiles telemetry and evidence smoke test."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import tempfile


def run_tool(arguments, env=None):
    result = subprocess.run(arguments, capture_output=True, text=True,
                            timeout=30, check=False, env=env)
    if result.returncode:
        raise RuntimeError("ApplyProfiles failed: %s %s" %
                           (result.stdout, result.stderr))


def write_wide_rgb8_tiff(path):
    width = 100_000
    bits_offset = 8 + 2 + 10 * 12 + 4
    pixels_offset = bits_offset + 6
    tags = (
        (256, 4, 1, width), (257, 4, 1, 1), (258, 3, 3, bits_offset),
        (259, 3, 1, 1), (262, 3, 1, 2), (273, 4, 1, pixels_offset),
        (277, 3, 1, 3), (278, 4, 1, 1), (279, 4, 1, width * 3),
        (284, 3, 1, 1),
    )
    with path.open("wb") as stream:
        stream.write(struct.pack("<2sHIH", b"II", 42, 8, len(tags)))
        for tag in tags:
            stream.write(struct.pack("<HHII", *tag))
        stream.write(struct.pack("<I3H", 0, 8, 8, 8))
        stream.write(bytes(width * 3))


def tiff_tags(path):
    data = path.read_bytes()
    endian = {b"II": "<", b"MM": ">"}[data[:2]]
    version, directory = struct.unpack_from(endian + "HI", data, 2)
    if version != 42:
        raise RuntimeError("expected classic TIFF output")
    count = struct.unpack_from(endian + "H", data, directory)[0]
    tags = {}
    for index in range(count):
        entry = directory + 2 + index * 12
        tag, kind, length, offset = struct.unpack_from(endian + "HHII", data, entry)
        if kind not in (3, 4):
            continue
        size = length * (2 if kind == 3 else 4)
        value = data[entry + 8:entry + 8 + size] if size <= 4 else data[offset:offset + size]
        tags[tag] = struct.unpack(endian + ("H" if kind == 3 else "I") * length, value)
    return tags


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply-tool", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--profile", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="iccdev-apply-receipts-") as directory:
        root = Path(directory)
        baseline = root / "baseline.tif"
        output = root / "output.tif"
        telemetry = root / "telemetry.jsonl"
        evidence = root / "evidence.json"
        tail = ["1", "0", "0", "1", "0", str(args.profile), "1"]
        run_tool([str(args.apply_tool), "--threads", "4", "--quiet",
                  str(args.source), str(baseline)] + tail)
        run_tool([str(args.apply_tool), "--threads", "4", "--quiet",
                  "--telemetry=jsonl", "--telemetry-file", str(telemetry),
                  "--evidence-file", str(evidence), str(args.source),
                  str(output)] + tail)
        if baseline.read_bytes() != output.read_bytes():
            raise RuntimeError("telemetry changed TIFF output bytes")
        with telemetry.open(encoding="ascii") as stream:
            events = [json.loads(line) for line in stream if line.strip()]
        with evidence.open(encoding="ascii") as stream:
            receipt = json.load(stream)
        if (not events or events[0].get("event") != "run_started" or
                events[-1].get("event") != "run_completed" or
                "requested_argv" in events[0]):
            raise RuntimeError("invalid telemetry lifecycle or exposed argv")
        digest = "sha256:" + hashlib.sha256(output.read_bytes()).hexdigest()
        if (receipt.get("status") != "success" or
                receipt.get("output_digest") != digest or
                receipt.get("completed_pixels") != receipt.get("total_pixels")):
            raise RuntimeError("evidence receipt does not match output")
        rejected = subprocess.run(
            [str(args.apply_tool), "--telemetry=jsonl", "--telemetry-file",
             str(telemetry), str(args.source), str(root / "rejected.tif")] + tail,
            capture_output=True, text=True, timeout=30, check=False,
        )
        if rejected.returncode == 0 or (root / "rejected.tif").exists():
            raise RuntimeError("existing telemetry file was accepted")

        # The input strip fits 1 MiB, while its RGB float output strip does not.
        wide_input = root / "wide-rgb8.tif"
        wide_output = root / "wide-rgb-float.tif"
        write_wide_rgb8_tiff(wide_input)
        run_tool([str(args.apply_tool), "--quiet", str(wide_input),
                  str(wide_output), "3", "0", "0", "0", "0",
                  str(args.profile), "1"],
                 env={**os.environ, "ICCDEV_TIFF_MAX_DECODE_MIB": "1"})
        tags = tiff_tags(wide_output)
        expected = {256: (100_000,), 257: (1,), 258: (32, 32, 32),
                    277: (3,), 278: (1,), 279: (1_200_000,)}
        if any(tags.get(tag) != value for tag, value in expected.items()):
            raise RuntimeError("wide float TIFF output has unexpected geometry or strip size")
    print("ApplyProfiles receipt smoke passed")


if __name__ == "__main__":
    main()
