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


def require_release_build(build_dir, label):
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.is_file():
        raise FileNotFoundError(f"{label} CMake cache not found: {cache_path}")

    build_type_prefix = "CMAKE_BUILD_TYPE:STRING="
    config_types_prefix = "CMAKE_CONFIGURATION_TYPES:STRING="
    build_type = ""
    config_types = ""
    with cache_path.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if line.startswith(build_type_prefix):
                build_type = line[len(build_type_prefix) :].strip()
            elif line.startswith(config_types_prefix):
                config_types = line[len(config_types_prefix) :].strip()

    is_release = build_type == "Release"
    if not is_release and config_types:
        is_release = "Release" in config_types.split(";")
    if not is_release:
        configured = build_type or config_types or "unset"
        raise RuntimeError(
            f"A/B benchmarks require Release builds; {label} is {configured}"
        )


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
        "--baseline-build-dir",
        type=Path,
        help="optional baseline build to interleave with --build-dir",
    )
    parser.add_argument(
        "--output", type=Path, default=Path("iccapplyprofiles-threading.tsv")
    )
    parser.add_argument(
        "--profile",
        type=Path,
        help="existing ICC profile to use instead of generating the matrix fixture",
    )
    parser.add_argument(
        "--profile-repetitions",
        type=int,
        choices=(1, 2),
        default=2,
        help="number of times to append the selected profile to the transform chain",
    )
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument(
        "--threads",
        nargs="+",
        type=int,
        default=[1, 2, 0],
        help="thread counts to test (0 selects automatic sizing)",
    )
    parser.add_argument(
        "--sizes",
        nargs="+",
        type=parse_size,
        default=[(256, 256), (512, 512), (1024, 1024), (4096, 2048)],
    )
    args = parser.parse_args()

    if args.repetitions <= 0:
        parser.error("--repetitions must be positive")
    if not args.threads or any(value < 0 or value > 256 for value in args.threads):
        parser.error("--threads values must be in the range 0..256")

    build_dir = args.build_dir.resolve()
    builds = [("candidate", find_executable(build_dir, "iccApplyProfiles"))]
    if args.baseline_build_dir:
        baseline_dir = args.baseline_build_dir.resolve()
        require_release_build(build_dir, "candidate")
        require_release_build(baseline_dir, "baseline")
        builds.append(
            ("baseline", find_executable(baseline_dir, "iccApplyProfiles"))
        )
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    with tempfile.TemporaryDirectory(prefix="iccdev-thread-benchmark-") as temp_name:
        temp_dir = Path(temp_name)
        if args.profile:
            profile = args.profile.resolve()
            if not profile.is_file():
                raise FileNotFoundError(f"profile not found: {profile}")
        else:
            from_xml = find_executable(build_dir, "iccFromXml")
            profile_xml = repo_root / "Testing" / "Display" / "sRGB_D65_MAT.xml"
            profile = temp_dir / "sRGB_D65_MAT.icc"
            run_command([from_xml, profile_xml, profile])
        profile_args = []
        for _ in range(args.profile_repetitions):
            profile_args.extend((profile, "1"))

        for width, height in args.sizes:
            source = temp_dir / f"source-{width}x{height}.tif"
            create_tiff(source, width, height)

            for repetition in range(1, args.repetitions + 1):
                build_order = builds if repetition % 2 else list(reversed(builds))
                thread_order = (
                    args.threads
                    if repetition % 2
                    else list(reversed(args.threads))
                )
                for variant, apply_profiles in build_order:
                    destination = temp_dir / f"output-{variant}.tif"
                    for threads in thread_order:
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
                        ] + profile_args
                        start = time.perf_counter_ns()
                        run_command(command)
                        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
                        rows.append(
                            (
                                variant,
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
        if args.baseline_build_dir:
            stream.write(
                "variant\tplatform\tmachine\twidth\theight\tthreads\t"
                "repetition\telapsed_ms\tsha256\n"
            )
            for row in rows:
                stream.write(
                    "{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.3f}\t{}\n".format(*row)
                )
        else:
            stream.write(
                "platform\tmachine\twidth\theight\tthreads\trepetition\t"
                "elapsed_ms\tsha256\n"
            )
            for row in rows:
                stream.write(
                    "{}\t{}\t{}\t{}\t{}\t{}\t{:.3f}\t{}\n".format(*row[1:])
                )

    for width, height in args.sizes:
        size_rows = [row for row in rows if row[3:5] == (width, height)]
        hashes = {row[8] for row in size_rows}
        if len(hashes) != 1:
            raise RuntimeError(f"output mismatch for {width}x{height}")
        for variant, _ in builds:
            variant_rows = [row for row in size_rows if row[0] == variant]
            for threads in args.threads:
                samples = [row[7] for row in variant_rows if row[5] == threads]
                print(
                    f"{width}x{height}\tvariant={variant}\tthreads={threads}\t"
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
