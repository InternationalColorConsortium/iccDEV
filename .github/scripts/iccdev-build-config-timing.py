#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################
"""Measure iccDEV configure, build, and focused CTest timing by configuration."""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


CONFIGURATIONS = {
    "debug": ("Debug", []),
    "release": ("Release", []),
    "relwithdebinfo": ("RelWithDebInfo", []),
    "minsizerel": ("MinSizeRel", []),
    "coverage": ("Debug", ["-DENABLE_COVERAGE=ON"]),
    "asan": ("Debug", ["-DENABLE_ASAN=ON"]),
    "sanitizers": ("Debug", ["-DENABLE_SANITIZERS=ON"]),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Build each selected configuration once, then run a focused CTest "
            "in interleaved rounds and report distribution statistics."
        )
    )
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="iccDEV repository root (default: inferred from this script)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="new result directory (default: out/build-config-timing-<UTC timestamp>)",
    )
    parser.add_argument(
        "--configs",
        default="debug,release,relwithdebinfo,minsizerel,coverage,asan",
        help="comma-separated configuration names",
    )
    parser.add_argument("--jobs", type=int, default=4, help="parallel build jobs")
    parser.add_argument(
        "--test-iterations",
        type=int,
        default=3,
        help="focused CTest repetitions per configuration",
    )
    parser.add_argument(
        "--ctest-filter",
        default=r"^iccdev\.hybrid-pipeline$",
        help="CTest regular expression used for runtime measurements",
    )
    parser.add_argument("--cc", default="clang-22")
    parser.add_argument("--cxx", default="clang++-22")
    parser.add_argument(
        "--generator",
        default="Unix Makefiles",
        help="CMake generator; use Ninja for faster developer-only builds",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the planned commands without creating build directories",
    )
    args = parser.parse_args()
    if args.jobs < 1 or args.test_iterations < 1:
        parser.error("--jobs and --test-iterations must be positive")
    return args


def run_capture(command: list[str], cwd: Path) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    first = result.stdout.splitlines()
    return first[0] if first else "unavailable"


def meminfo_kib(field: str) -> int | None:
    try:
        for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
            if line.startswith(f"{field}:"):
                return int(line.split()[1])
    except (OSError, ValueError, IndexError):
        return None
    return None


def cpu_model() -> str:
    try:
        for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except (OSError, ValueError, IndexError):
        pass
    return platform.processor() or "unavailable"


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = max(0, math.ceil(fraction * len(ordered)) - 1)
    return ordered[index]


def summarize(records: list[dict[str, object]]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, str], list[float]] = defaultdict(list)
    for record in records:
        if record["returncode"] == 0:
            grouped[(str(record["configuration"]), str(record["phase"]))].append(
                float(record["seconds"])
            )

    rows: list[dict[str, object]] = []
    for (configuration, phase), values in sorted(grouped.items()):
        mean = statistics.fmean(values)
        stdev = statistics.stdev(values) if len(values) > 1 else 0.0
        rows.append(
            {
                "configuration": configuration,
                "phase": phase,
                "samples": len(values),
                "mean_seconds": mean,
                "median_seconds": statistics.median(values),
                "stdev_seconds": stdev,
                "cv_percent": (stdev / mean * 100.0) if mean else 0.0,
                "min_seconds": min(values),
                "max_seconds": max(values),
                "spread_seconds": max(values) - min(values),
                "p95_seconds": percentile(values, 0.95),
            }
        )
    return rows


def write_summaries(output_dir: Path, rows: list[dict[str, object]]) -> None:
    fields = [
        "configuration",
        "phase",
        "samples",
        "mean_seconds",
        "median_seconds",
        "stdev_seconds",
        "cv_percent",
        "min_seconds",
        "max_seconds",
        "spread_seconds",
        "p95_seconds",
    ]
    with (output_dir / "summary.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    with (output_dir / "summary.md").open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# iccDEV build-configuration timing\n\n")
        handle.write(
            "Configure and build phases have one sample per invocation. The focused "
            "CTest phase is interleaved across configurations to reduce order bias.\n\n"
        )
        handle.write(
            "| Configuration | Phase | n | Mean (s) | Median (s) | SD (s) | CV | "
            "Min (s) | Max (s) | Spread (s) | P95 (s) |\n"
        )
        handle.write("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for row in rows:
            handle.write(
                f"| {row['configuration']} | {row['phase']} | {row['samples']} | "
                f"{row['mean_seconds']:.3f} | {row['median_seconds']:.3f} | "
                f"{row['stdev_seconds']:.3f} | {row['cv_percent']:.2f}% | "
                f"{row['min_seconds']:.3f} | {row['max_seconds']:.3f} | "
                f"{row['spread_seconds']:.3f} | {row['p95_seconds']:.3f} |\n"
            )


def main() -> int:
    args = parse_args()
    source_dir = args.source_dir.resolve()
    cmake_source = source_dir / "Build" / "Cmake"
    if not (source_dir / ".git").exists() or not (cmake_source / "CMakeLists.txt").exists():
        print(f"error: not an iccDEV checkout: {source_dir}", file=sys.stderr)
        return 2

    configs = [item.strip().lower() for item in args.configs.split(",") if item.strip()]
    unknown = sorted(set(configs) - set(CONFIGURATIONS))
    if unknown:
        print(f"error: unknown configurations: {', '.join(unknown)}", file=sys.stderr)
        return 2
    if len(configs) != len(set(configs)):
        print("error: duplicate configuration name", file=sys.stderr)
        return 2

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_dir = (args.output_dir or source_dir / "out" / f"build-config-timing-{timestamp}").resolve()
    if output_dir.exists():
        print(f"error: output directory already exists: {output_dir}", file=sys.stderr)
        return 2

    commands: dict[str, dict[str, list[str]]] = {}
    for name in configs:
        build_type, extra = CONFIGURATIONS[name]
        build_dir = output_dir / "builds" / name
        commands[name] = {
            "configure": [
                "cmake",
                "-S",
                str(cmake_source),
                "-B",
                str(build_dir),
                "-G",
                args.generator,
                f"-DCMAKE_BUILD_TYPE={build_type}",
                f"-DCMAKE_C_COMPILER={args.cc}",
                f"-DCMAKE_CXX_COMPILER={args.cxx}",
                "-DENABLE_TOOLS=ON",
                "-DENABLE_TESTS=ON",
                "-DENABLE_WXWIDGETS=OFF",
                "-DENABLE_SHARED_LIBS=ON",
                "-DENABLE_STATIC_LIBS=ON",
                "-DICCDEV_CTEST_PARALLEL_LEVEL=1",
                "-Wno-dev",
                *extra,
            ],
            "build": ["cmake", "--build", str(build_dir), "--parallel", str(args.jobs)],
            "ctest": [
                "ctest",
                "--test-dir",
                str(build_dir),
                "-R",
                args.ctest_filter,
                "--output-on-failure",
                "--no-tests=error",
            ],
        }

    if args.dry_run:
        print(json.dumps(commands, indent=2))
        return 0

    output_dir.mkdir(parents=True)
    (output_dir / "logs").mkdir()
    (output_dir / "profraw").mkdir()
    records_path = output_dir / "results.jsonl"
    records: list[dict[str, object]] = []
    failures = 0

    environment = {
        "timestamp_utc": timestamp,
        "source_dir": str(source_dir),
        "git_commit": run_capture(["git", "rev-parse", "HEAD"], source_dir),
        "kernel": platform.platform(),
        "cpu": cpu_model(),
        "logical_cpus": os.cpu_count(),
        "memory_total_kib": meminfo_kib("MemTotal"),
        "jobs": args.jobs,
        "generator": args.generator,
        "cc": run_capture([args.cc, "--version"], source_dir),
        "cxx": run_capture([args.cxx, "--version"], source_dir),
        "cmake": run_capture(["cmake", "--version"], source_dir),
        "ctest_filter": args.ctest_filter,
        "configurations": configs,
        "commands": commands,
    }
    (output_dir / "environment.json").write_text(
        json.dumps(environment, indent=2) + "\n", encoding="utf-8"
    )

    def measure(
        configuration: str,
        phase: str,
        iteration: int,
        command: list[str],
        env: dict[str, str] | None = None,
    ) -> None:
        nonlocal failures
        log_path = output_dir / "logs" / f"{phase}-{configuration}-{iteration}.log"
        load_before = os.getloadavg()
        mem_before = meminfo_kib("MemAvailable")
        started = time.perf_counter()
        with log_path.open("w", encoding="utf-8", newline="\n") as log:
            result = subprocess.run(
                command,
                cwd=source_dir,
                env=env,
                check=False,
                stdout=log,
                stderr=subprocess.STDOUT,
                text=True,
            )
        elapsed = time.perf_counter() - started
        record = {
            "configuration": configuration,
            "phase": phase,
            "iteration": iteration,
            "seconds": elapsed,
            "returncode": result.returncode,
            "load_before": load_before,
            "load_after": os.getloadavg(),
            "mem_available_kib_before": mem_before,
            "mem_available_kib_after": meminfo_kib("MemAvailable"),
            "log": str(log_path.relative_to(output_dir)),
        }
        records.append(record)
        with records_path.open("a", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(record, sort_keys=True) + "\n")
        print(
            f"{configuration:16s} {phase:10s} {elapsed:9.3f}s rc={result.returncode}",
            flush=True,
        )
        if result.returncode:
            failures += 1

    runnable: list[str] = []
    for name in configs:
        measure(name, "configure", 1, commands[name]["configure"])
        if records[-1]["returncode"] != 0:
            continue
        measure(name, "build", 1, commands[name]["build"])
        if records[-1]["returncode"] == 0:
            runnable.append(name)

    for iteration in range(1, args.test_iterations + 1):
        if not runnable:
            break
        # Pair each rotation with its reverse so every configuration reaches
        # both ends of the schedule instead of assigning odd/even entries to
        # consistently early or late positions.
        offset = ((iteration - 1) // 2) % len(runnable)
        order = runnable[offset:] + runnable[:offset]
        if iteration % 2 == 0:
            order.reverse()
        for name in order:
            env = os.environ.copy()
            if name == "coverage":
                raw_dir = output_dir / "profraw" / f"test-{iteration}"
                raw_dir.mkdir(parents=True, exist_ok=True)
                env["LLVM_PROFILE_FILE"] = str(raw_dir / "coverage-%4m.profraw")
            if name in {"asan", "sanitizers"}:
                env["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
            if name == "sanitizers":
                env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
            measure(name, "ctest", iteration, commands[name]["ctest"], env)

    rows = summarize(records)
    write_summaries(output_dir, rows)
    shutil.copyfile(output_dir / "summary.md", output_dir / "README.md")
    print(f"results: {output_dir}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
