#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################

"""Tests for matched coverage candidate-probe classification."""

from __future__ import annotations

import csv
import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "Build/Cmake/Testing/iccdev-matched-coverage-report.sh"


def write_executable(path: Path, content: str) -> None:
    path.write_text(content, encoding="ascii")
    path.chmod(0o755)


def test_candidate_mismatches_are_classified_not_published_as_errors(
    tmp_path: Path,
) -> None:
    build_dir = tmp_path / "build"
    tools_dir = build_dir / "Tools" / "CoverageFixtures"
    profraw_dir = tmp_path / "profraw"
    report_dir = tmp_path / "report"
    bin_dir = tmp_path / "bin"
    tools_dir.mkdir(parents=True)
    profraw_dir.mkdir()
    bin_dir.mkdir()

    for name in ("command-failure", "mismatch", "owner", "zero-coverage"):
        write_executable(tools_dir / name, "coverage fixture\n")
    (profraw_dir / "fixture-123456.profraw").write_bytes(b"profile")

    fake_cov = bin_dir / "fake-llvm-cov"
    write_executable(
        fake_cov,
        """#!/usr/bin/env python3
import pathlib
import sys


TOTAL = "TOTAL 10 1 90.00% 5 1 80.00% 20 2 90.00% 4 1 75.00%"
ZERO = "TOTAL 0 0 - 0 0 - 0 0 - 0 0 -"


command = sys.argv[1]
if command == "report":
    object_name = pathlib.Path(sys.argv[2]).name
    if object_name == "command-failure":
        print("fixture llvm-cov failure", file=sys.stderr)
        raise SystemExit(3)
    if object_name == "mismatch":
        print(TOTAL)
        print("warning: 107 functions have mismatched data", file=sys.stderr)
        raise SystemExit(0)
    if object_name == "zero-coverage":
        print(ZERO)
        raise SystemExit(0)
    print(TOTAL)
    raise SystemExit(0)
if command == "show":
    output_arg = next(arg for arg in sys.argv if arg.startswith("-output-dir="))
    output_dir = pathlib.Path(output_arg.split("=", 1)[1])
    output_dir.mkdir(parents=True)
    (output_dir / "index.html").write_text("coverage", encoding="ascii")
    raise SystemExit(0)
if command == "export":
    print("SF:/source/fixture.cpp")
    print("DA:1,1")
    print("end_of_record")
    raise SystemExit(0)
raise SystemExit(2)
""",
    )

    fake_profdata = bin_dir / "fake-llvm-profdata"
    write_executable(
        fake_profdata,
        """#!/usr/bin/env python3
import pathlib
import sys


output_index = sys.argv.index("-o") + 1
pathlib.Path(sys.argv[output_index]).write_bytes(b"profdata")
""",
    )

    env = os.environ.copy()
    env["LLVM_COV"] = str(fake_cov)
    env["LLVM_PROFDATA"] = str(fake_profdata)
    subprocess.run(
        [str(HELPER), str(build_dir), str(profraw_dir), str(report_dir)],
        check=True,
        env=env,
        capture_output=True,
        text=True,
    )

    with (report_dir / "probe-candidates.tsv").open(
        encoding="ascii", newline=""
    ) as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    by_object = {Path(row["object"]).name: row for row in rows}

    assert by_object["command-failure"]["result"] == "llvm-cov-error"
    assert by_object["mismatch"]["result"] == "mismatched-data"
    assert by_object["mismatch"]["mismatch_functions"] == "107"
    assert by_object["owner"]["result"] == "compatible"
    assert by_object["owner"]["selected"] == "yes"
    assert by_object["zero-coverage"]["result"] == "zero-coverage"
    assert sum(row["selected"] == "yes" for row in rows) == 1

    metadata = dict(
        line.split("=", 1)
        for line in (report_dir / "coverage-env.txt").read_text(
            encoding="ascii"
        ).splitlines()
    )
    assert metadata["mismatch_count"] == "0"
    assert metadata["probe_candidate_count"] == "4"
    assert metadata["probe_mismatch_count"] == "1"
    assert metadata["probe_mismatched_function_count"] == "107"
    assert metadata["probe_command_failure_count"] == "1"
    assert metadata["probe_zero_coverage_count"] == "1"
    assert metadata["probe_compatible_count"] == "1"

    assert not (report_dir / "probes").exists()
    nonempty_errors = [
        path for path in report_dir.rglob("*.err") if path.stat().st_size
    ]
    assert nonempty_errors == []
    command_failures = (report_dir / "probe-command-failures.txt").read_text(
        encoding="ascii"
    )
    assert "fixture llvm-cov failure" in command_failures
    assert "mismatched data" not in command_failures
    summary = (report_dir / "coverage-summary.txt").read_text(encoding="ascii")
    assert "Mismatched function records in rejected probes: 107" in summary
    assert "Selected coverage mismatches: 0" in summary
