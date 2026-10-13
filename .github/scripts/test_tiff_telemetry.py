#!/usr/bin/env python3
"""TIFF telemetry contracts using durable inputs and the native TIFF fixture tool."""

import argparse
import json
import math
import os
import pathlib
import subprocess
import tempfile


def run(*args, success=True):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, timeout=60)
    if success:
        assert result.returncode == 0, (args, result.returncode, result.stderr)
    else:
        assert result.returncode in (1, 255, -1, 0xFFFFFFFF), (
            args, result.returncode, result.stderr
        )
    assert b"runtime error:" not in result.stderr, result.stderr
    assert b"ERROR: AddressSanitizer" not in result.stderr, result.stderr
    return result


def receipt(path, success=True):
    events = [json.loads(line) for line in path.read_text(encoding="ascii").splitlines()]
    assert len(events) >= 2
    assert events[0]["event"] == "run_started"
    assert events[-1]["event"] == ("run_completed" if success else "run_failed")
    assert sum(event["event"] in ("run_completed", "run_failed") for event in events) == 1
    assert [event["sequence"] for event in events] == list(range(1, len(events) + 1))
    elapsed = [event["monotonic_elapsed_ms"] for event in events]
    assert elapsed == sorted(elapsed)
    for event in events:
        assert event["schema"] == "iccdev-tiff-telemetry/v1"
        assert event["timestamp_utc"].endswith("Z")
        for key, value in event.items():
            if key.endswith("_ms"):
                assert isinstance(value, (int, float)) and math.isfinite(value) and value >= 0
    return events


def fields(events):
    merged = {}
    for event in events:
        merged.update(event)
    return merged


def main():
    parser = argparse.ArgumentParser()
    for name in ("fixture", "dump", "specsep", "source", "enabled"):
        parser.add_argument(f"--{name}", required=True)
    args = parser.parse_args()
    source = pathlib.Path(args.source)
    enabled = args.enabled.upper() in ("ON", "TRUE", "1")
    for tool in (args.dump, args.specsep):
        binary = pathlib.Path(tool).read_bytes()
        assert (b"iccdev-tiff-telemetry/v1" in binary) == enabled
        help_text = run(tool, "--help").stdout
        assert (b"--telemetry=" in help_text) == enabled
        if not enabled:
            for option in ("--telemetry=human", "--telemetry-file"):
                rejected = run(tool, option, success=False)
                assert b"Telemetry is not built" in rejected.stderr
    if not enabled:
        return

    with tempfile.TemporaryDirectory(prefix="iccdev-tiff-telemetry-") as temp:
        root = pathlib.Path(temp)
        run(args.fixture, "--fixtures", root)
        prefix = source / ".github" / "ci" / "test-data" / "spectral" / "spec_"
        output = root / "spectral.tif"
        baseline = run(args.specsep, output, 0, 0, prefix, 1, 10, 1)
        original = output.read_bytes()
        log = root / "spectral.jsonl"
        diagnosed = run(args.specsep, "--telemetry=jsonl", "--telemetry-file", log,
                        output, 0, 0, prefix, 1, 10, 1)
        assert diagnosed.stdout == baseline.stdout
        assert diagnosed.stderr == baseline.stderr
        assert output.read_bytes() == original
        events = receipt(log)
        values = fields(events)
        width, height = values["width"], values["height"]
        assert width > 0 and height > 0
        assert values["samples_per_pixel"] == 10
        assert values["rows_written"] == height
        assert values["channel_rows_read"] == 10 * height
        expected_bytes = width * height * 10 * (values["bits_per_sample"] // 8)
        assert values["logical_bytes_read"] == expected_bytes
        assert values["logical_bytes_written"] == expected_bytes

        # An opted-in build without a runtime request must remain silent.
        off = run(args.specsep, "--telemetry=off", output, 0, 0, prefix, 1, 10, 1)
        assert (off.stdout, off.stderr) == (baseline.stdout, baseline.stderr)
        human = run(args.specsep, "--telemetry=human", output, 0, 0, prefix, 1, 10, 1)
        assert human.stdout == baseline.stdout and b"run_completed" in human.stderr
        assert output.read_bytes() == original

        dump_log = root / "dump.jsonl"
        plain_dump = run(args.dump, "--evidence-json", output)
        observed_dump = run(args.dump, "--telemetry=jsonl", "--telemetry-file", dump_log,
                            "--evidence-json", output)
        assert (plain_dump.stdout, plain_dump.stderr) == (observed_dump.stdout, observed_dump.stderr)
        dump_values = fields(receipt(dump_log))
        assert dump_values["width"] == width and dump_values["height"] == height
        assert dump_values["decoded_pixels"] == 0 and dump_values["decoded_bytes"] == 0

        for bits in (8, 16, 32):
            for planar in (0, 1):
                strip = root / f"strip-{bits}-{planar}.tif"
                tile = root / f"tile-{bits}-{planar}.tif"
                tile_log = root / f"tile-{bits}-{planar}.jsonl"
                run(args.specsep, strip, 1, planar, root / f"gray{bits}_", 1, 3, 1)
                run(args.specsep, "--telemetry=jsonl", "--telemetry-file", tile_log,
                    "--tiles", 16, 32, "--bigtiff", tile, 1, planar,
                    root / f"gray{bits}_", 1, 3, 1)
                run(args.fixture, "--compare", strip, tile)
                info = json.loads(run(args.dump, "--evidence-json", tile).stdout)
                assert info["bigTiff"] and info["tiled"]
                tile_values = fields(receipt(tile_log))
                assert tile_values["bits_per_sample"] == bits
                assert tile_values["rows_written"] == tile_values["height"]
                assert tile_values["channel_rows_read"] == tile_values["height"] * 3

        listing = root / "inventory.jsonl"
        multi = root / "multi.tif"
        ordinary = run(args.dump, "--evidence-json", "--list-directories", multi)
        diagnosed = run(args.dump, "--telemetry=jsonl", "--telemetry-file", listing,
                        "--evidence-json", "--list-directories", multi)
        assert (ordinary.stdout, ordinary.stderr) == (diagnosed.stdout, diagnosed.stderr)
        assert fields(receipt(listing))["directories_visited"] == 2

        for tool in (args.dump, args.specsep):
            for switches in (
                ("--telemetry=invalid",),
                ("--telemetry=jsonl",),
                ("--telemetry-file",),
                ("--telemetry=human", "--telemetry-file", root / "unused.jsonl"),
                ("--telemetry=off", "--telemetry=human"),
            ):
                operands = (output,) if tool == args.dump else (
                    root / "invalid.tif", 0, 0, prefix, 1, 10, 1
                )
                run(tool, *switches, *operands, success=False)
        before = log.read_bytes()
        run(args.specsep, "--telemetry=jsonl", "--telemetry-file", log,
            root / "refused.tif", 0, 0, prefix, 1, 10, 1, success=False)
        assert log.read_bytes() == before and not (root / "refused.tif").exists()
        alias = root / "alias.tif"
        run(args.specsep, "--telemetry=jsonl", "--telemetry-file", alias,
            alias, 0, 0, prefix, 1, 10, 1, success=False)
        assert not alias.exists()
        run(args.dump, "--telemetry=jsonl", "--telemetry-file", output,
            output, success=False)
        assert output.read_bytes() == original
        run(args.dump, "--telemetry=jsonl", "--telemetry-file", root / "missing" / "log",
            output, success=False)

        late_log = root / "late.jsonl"
        partial = root / "partial.tif"
        truncated = source / ".github" / "ci" / "test-data" / "specsep-truncated" / "spec_"
        run(args.specsep, "--telemetry=jsonl", "--telemetry-file", late_log,
            partial, 0, 0, truncated, 1, 1, 1, success=False)
        assert not partial.exists()
        assert receipt(late_log, success=False)[-1].get("stage")

        forensic_log = root / "forensic.jsonl"
        forensic = root / "forensic.icc"
        run(args.dump, "--telemetry=jsonl", "--telemetry-file", forensic_log,
            multi, forensic, success=False)
        assert forensic.read_bytes() == b"malformed-first-icc"
        assert receipt(forensic_log, success=False)[-1].get("stage")
        if os.name == "posix":
            # A human diagnostic stream failure must not turn into tool success.
            for tool in (args.dump, args.specsep):
                operands = (output,) if tool == args.dump else (
                    root / "stderr-failed.tif", 0, 0, prefix, 1, 10, 1
                )
                with open("/dev/full", "wb") as full:
                    failed = subprocess.run(
                        [tool, "--telemetry=human", *map(str, operands)],
                        stdout=subprocess.PIPE, stderr=full, timeout=60
                    )
                assert failed.returncode in (1, 255)
                assert not failed.stdout
            assert not (root / "stderr-failed.tif").exists()
        print("TIFF telemetry geometry, counters, parity and failure contracts passed")


if __name__ == "__main__":
    main()
