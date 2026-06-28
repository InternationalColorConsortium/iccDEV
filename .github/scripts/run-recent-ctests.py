#!/usr/bin/env python3
"""Run the most recently registered CTest tests."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


TEST_LINE_RE = re.compile(r"^\s*Test\s+#\d+:\s+(.+?)\s*$")


def list_tests(test_dir: str, config: str | None) -> list[str]:
    command = ["ctest", "--test-dir", test_dir, "-N", "--no-tests=error"]
    if config:
        command.extend(["-C", config])

    result = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    sys.stdout.write(result.stdout)
    if result.returncode != 0:
        raise RuntimeError(f"ctest listing failed with exit code {result.returncode}")

    tests = []
    for line in result.stdout.splitlines():
        match = TEST_LINE_RE.match(line)
        if match:
            tests.append(match.group(1).strip())

    if not tests:
        raise RuntimeError("ctest listing did not include any test names")

    return tests


def write_selection(path_text: str | None, selected_tests: list[str]) -> None:
    if not path_text:
        return

    path = Path(path_text)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(selected_tests) + "\n", encoding="ascii")


def build_regex(test_names: list[str]) -> str:
    return "^(" + "|".join(re.escape(name) for name in test_names) + ")$"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run only the last N tests from the CTest registration order."
    )
    parser.add_argument("--test-dir", required=True, help="CTest build directory")
    parser.add_argument("--limit", type=int, default=5, help="Number of tests to run")
    parser.add_argument("--config", default="", help="CTest configuration for multi-config generators")
    parser.add_argument(
        "--label-exclude",
        action="append",
        default=[],
        help="CTest label exclusion regex; may be repeated",
    )
    parser.add_argument("--output-junit", default="", help="Optional CTest JUnit output path")
    parser.add_argument("--selection-output", default="", help="Write selected test names to this file")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.limit < 1:
        print("--limit must be at least 1", file=sys.stderr)
        return 2

    try:
        tests = list_tests(args.test_dir, args.config or None)
    except RuntimeError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        return 1

    selected_tests = tests[-args.limit :]
    write_selection(args.selection_output or None, selected_tests)

    print("Selected recent CTests:")
    for test_name in selected_tests:
        print(f"  {test_name}")

    command = [
        "ctest",
        "--test-dir",
        args.test_dir,
        "--output-on-failure",
        "--no-tests=error",
        "-R",
        build_regex(selected_tests),
    ]
    if args.config:
        command.extend(["-C", args.config])
    for label_exclude in args.label_exclude:
        command.extend(["--label-exclude", label_exclude])
    if args.output_junit:
        command.extend(["--output-junit", args.output_junit])

    print("+ " + " ".join(command))
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
