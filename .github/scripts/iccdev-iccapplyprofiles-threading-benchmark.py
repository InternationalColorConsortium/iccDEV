#!/usr/bin/env python3
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

import argparse
import hashlib
import platform
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def find_executable(build_dir, name):
    candidates = [
        build_dir / "bin" / "Release" / f"{name}.exe",
        build_dir / "bin" / f"{name}.exe",
        build_dir / "Tools" / "IccApplyProfiles" / name,
        build_dir / "Tools" / "IccFromXml" / name,
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"{name} not found under {build_dir}")


def parse_size(value):
    parts = value.lower().split("x", 1)
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"invalid image size: {value}")
    width, height = (int(part) for part in parts)
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError(f"invalid image size: {value}")
    return width, height


def create_tiff(path, width, height):
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError("Pillow is required to generate benchmark TIFFs") from exc

    x_axis = Image.linear_gradient("L").rotate(90, expand=True).resize((width, height))
    y_axis = Image.linear_gradient("L").resize((width, height))
    blue = Image.blend(x_axis, y_axis, 0.5)
    Image.merge("RGB", (x_axis, y_axis, blue)).save(
        path, format="TIFF", compression="raw"
    )


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_command(command):
    result = subprocess.run(
        command,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip()
        raise RuntimeError(
            f"command failed with exit {result.returncode}: {' '.join(map(str, command))}"
            + (f"\n{detail}" if detail else "")
        )


def main():
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Benchmark iccApplyProfiles thread scaling with TIFF input."
    )
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument(
        "--output", type=Path, default=Path("iccapplyprofiles-threading.tsv")
    )
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument(
        "--sizes",
        nargs="+",
        type=parse_size,
        default=[(256, 256), (512, 512), (1024, 1024), (4096, 2048)],
    )
    args = parser.parse_args()

    if args.repetitions <= 0:
        parser.error("--repetitions must be positive")

    build_dir = args.build_dir.resolve()
    apply_profiles = find_executable(build_dir, "iccApplyProfiles")
    from_xml = find_executable(build_dir, "iccFromXml")
    profile_xml = repo_root / "Testing" / "Display" / "sRGB_D65_MAT.xml"
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    with tempfile.TemporaryDirectory(prefix="iccdev-thread-benchmark-") as temp_name:
        temp_dir = Path(temp_name)
        profile = temp_dir / "sRGB_D65_MAT.icc"
        run_command([from_xml, profile_xml, profile])

        for width, height in args.sizes:
            source = temp_dir / f"source-{width}x{height}.tif"
            destination = temp_dir / "output.tif"
            create_tiff(source, width, height)

            for repetition in range(1, args.repetitions + 1):
                order = [1, 2, 0] if repetition % 2 else [0, 2, 1]
                for threads in order:
                    command = [
                        apply_profiles,
                        "-threads",
                        str(threads),
                        source,
                        destination,
                        "0",
                        "0",
                        "0",
                        "0",
                        "1",
                        profile,
                        "1",
                        profile,
                        "1",
                    ]
                    start = time.perf_counter_ns()
                    run_command(command)
                    elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
                    rows.append(
                        (
                            platform.system(),
                            platform.machine(),
                            width,
                            height,
                            threads,
                            repetition,
                            elapsed_ms,
                            file_sha256(destination),
                        )
                    )
                    destination.unlink()

    with output.open("w", encoding="ascii", newline="\n") as stream:
        stream.write(
            "platform\tmachine\twidth\theight\tthreads\trepetition\t"
            "elapsed_ms\tsha256\n"
        )
        for row in rows:
            stream.write(
                "{}\t{}\t{}\t{}\t{}\t{}\t{:.3f}\t{}\n".format(*row)
            )

    for width, height in args.sizes:
        size_rows = [row for row in rows if row[2:4] == (width, height)]
        hashes = {row[7] for row in size_rows}
        if len(hashes) != 1:
            raise RuntimeError(f"output mismatch for {width}x{height}")
        for threads in (0, 1, 2):
            samples = [row[6] for row in size_rows if row[4] == threads]
            print(
                f"{width}x{height}\tthreads={threads}\t"
                f"median_ms={statistics.median(samples):.3f}"
            )

    print(f"[PASS] benchmark report: {output}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FileNotFoundError, RuntimeError, ValueError) as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        sys.exit(1)
