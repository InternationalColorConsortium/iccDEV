#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Reject workflow commands that can split one logical report across repeated
# fixed-output invocations or overflow the process argument limit.
###############################################################################

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

import yaml


XARGS_SINGLE_OUTPUT = re.compile(
    r"\bxargs\b[^\n]*(?:"
    r"llvm-profdata(?:-[0-9]+)?\s+merge|"
    r"gcovr\b[^\n]*(?:\s-o(?:\s|=)|--output(?:\s|=))|"
    r"genhtml\b[^\n]*(?:\s-o(?:\s|=)|--output-directory(?:\s|=))|"
    r"lcov\b[^\n]*(?:\s-o(?:\s|=)|--output-file(?:\s|=))"
    r")",
    re.IGNORECASE,
)
LLVM_PROFILE_GLOB = re.compile(
    r"\bllvm-profdata(?:-[0-9]+)?\s+merge\b[^\n]*(?:\*|\?)[^\s]*\.prof(?:raw|data)\b",
    re.IGNORECASE,
)


def workflow_run_blocks(path: Path) -> list[tuple[str, str]]:
    with path.open("r", encoding="utf-8") as handle:
        workflow = yaml.safe_load(handle) or {}

    jobs = workflow.get("jobs", {}) if isinstance(workflow, dict) else {}
    if not isinstance(jobs, dict):
        return []

    blocks: list[tuple[str, str]] = []
    for job_name, job in jobs.items():
        if not isinstance(job, dict):
            continue
        steps = job.get("steps", [])
        if not isinstance(steps, list):
            continue
        for index, step in enumerate(steps, start=1):
            if not isinstance(step, dict) or not isinstance(step.get("run"), str):
                continue
            name = str(step.get("name", f"step {index}"))
            blocks.append((f"job {job_name}, {name}", step["run"]))
    return blocks


def normalized_shell(run: str) -> str:
    unfolded = re.sub(r"\\\s*\n\s*", " ", run)
    return re.sub(r"\|\s*\n\s*", "| ", unfolded)


def violations(path: Path) -> list[str]:
    found: list[str] = []
    for label, run in workflow_run_blocks(path):
        normalized = normalized_shell(run)
        if XARGS_SINGLE_OUTPUT.search(normalized):
            found.append(
                f"{path}: {label}: xargs may split a single fixed-output report"
            )
        if LLVM_PROFILE_GLOB.search(normalized):
            found.append(
                f"{path}: {label}: llvm-profdata glob may exceed the argument limit"
            )
    return found


def self_test() -> int:
    fixtures = {
        "xargs-profdata": (
            "find profraw -print0 | xargs -0 llvm-profdata-22 merge -sparse "
            "-o merged.profdata",
            1,
        ),
        "split-pipe-profdata": (
            "find profraw -print0 |\n  xargs -0 llvm-profdata merge "
            "-o merged.profdata",
            1,
        ),
        "glob-profdata": (
            "llvm-profdata merge profiles/*.profraw -o merged.profdata",
            1,
        ),
        "input-file": (
            "llvm-profdata merge --input-files=profiles.txt -o merged.profdata",
            0,
        ),
        "streamed-checksums": (
            "find artifacts -print0 | xargs -0 sha256sum > SHA256SUMS",
            0,
        ),
    }
    with tempfile.TemporaryDirectory(prefix="iccdev-fixed-output-") as temp_dir:
        root = Path(temp_dir)
        for name, (run, expected) in fixtures.items():
            path = root / f"{name}.yml"
            path.write_text(
                "jobs:\n  test:\n    steps:\n      - name: fixture\n"
                f"        run: {run!r}\n",
                encoding="utf-8",
            )
            actual = len(violations(path))
            if actual != expected:
                print(
                    f"self-test {name}: expected {expected}, found {actual}",
                    file=sys.stderr,
                )
                return 1
    print("fixed-output batching checker self-test passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("workflows", nargs="*")
    args = parser.parse_args()

    if args.self_test:
        return self_test()
    if not args.workflows:
        parser.error("at least one workflow is required")

    all_violations: list[str] = []
    for workflow in args.workflows:
        all_violations.extend(violations(Path(workflow)))
    for violation in all_violations:
        print(violation, file=sys.stderr)
    return 1 if all_violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
