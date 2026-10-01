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
from pathlib import Path
import subprocess
import tempfile


def run_tool(arguments):
    result = subprocess.run(arguments, capture_output=True, text=True,
                            timeout=30, check=False)
    if result.returncode:
        raise RuntimeError("ApplyProfiles failed: %s %s" %
                           (result.stdout, result.stderr))


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
    print("ApplyProfiles receipt smoke passed")


if __name__ == "__main__":
    main()
