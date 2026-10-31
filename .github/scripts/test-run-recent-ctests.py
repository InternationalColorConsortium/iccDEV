#!/usr/bin/env python3
"""Shim tests for run-recent-ctests.py selection and workflow plumbing."""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
RUNNER = REPO_ROOT / ".github" / "scripts" / "run-recent-ctests.py"
TOOL_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "ci-iccdev-tool-tests.yml"
SMOKE_TESTS = [
    "iccdev.parser-restore-calls",
    "iccdev.profile-write-failure",
    "iccdev.json-reader-strictness",
    "iccdev.xml-json-roundtrip-qa",
    "iccdev.clut-eight-output-regression",
    "iccdev.applyprofiles-headless-receipts",
]


def make_ctest_shim(directory: Path) -> tuple[Path, Path]:
    calls = directory / "ctest-calls.txt"
    shim = directory / "ctest"
    shim.write_text(
        """#!/usr/bin/env python3
import os
import sys
from pathlib import Path

calls = Path(os.environ["CTEST_CALLS"])
with calls.open("a", encoding="ascii") as handle:
    handle.write("\\0".join(sys.argv[1:]) + "\\n")

if "-N" in sys.argv:
    print("Test project /tmp/fake")
    names = os.environ.get("CTEST_TESTS")
    if names is None:
        names = [f"iccdev.test.{idx:02d}" for idx in range(1, 13)]
    else:
        names = names.split("|") if names else []
    for idx, name in enumerate(names, 1):
        print(f"  Test #{idx}: {name}")
    sys.exit(0)

sys.exit(0)
""",
        encoding="ascii",
    )
    shim.chmod(0o755)
    return shim, calls


def run_runner(args: list[str], workdir: Path,
               test_names: list[str] | None = None) -> subprocess.CompletedProcess[str]:
    _shim, calls = make_ctest_shim(workdir)
    env = os.environ.copy()
    env["PATH"] = f"{workdir}{os.pathsep}{env['PATH']}"
    env["CTEST_CALLS"] = str(calls)
    if test_names is not None:
        env["CTEST_TESTS"] = "|".join(test_names)
    return subprocess.run(
        [sys.executable, str(RUNNER), "--test-dir", "build", *args],
        cwd=REPO_ROOT,
        env=env,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def read_calls(workdir: Path) -> list[list[str]]:
    calls = workdir / "ctest-calls.txt"
    return [line.rstrip("\n").split("\0") for line in calls.read_text(encoding="ascii").splitlines()]


def assert_true(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_no_limit_selects_all() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--selection-output", str(workdir / "selected.txt")], workdir)
        assert_true(result.returncode == 0, result.stderr)
        selected = (workdir / "selected.txt").read_text(encoding="ascii").splitlines()
        calls = read_calls(workdir)
        assert_true(len(selected) == 12, f"expected all tests, got {selected}")
        assert_true("-R" not in calls[1], f"unexpected -R in full run: {calls[1]}")


def test_limit_one_selects_last_test() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--limit", "1", "--selection-output", str(workdir / "selected.txt")], workdir)
        assert_true(result.returncode == 0, result.stderr)
        selected = (workdir / "selected.txt").read_text(encoding="ascii").splitlines()
        calls = read_calls(workdir)
        assert_true(selected == ["iccdev.test.12"], f"wrong selected tests: {selected}")
        assert_true("-R" in calls[1], f"missing -R in limited run: {calls[1]}")
        regex = calls[1][calls[1].index("-R") + 1]
        assert_true(regex == "^(iccdev\\.test\\.12)$", f"wrong regex: {regex}")


def test_limit_ten_selects_last_ten_tests() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--limit", "10", "--selection-output", str(workdir / "selected.txt")], workdir)
        assert_true(result.returncode == 0, result.stderr)
        selected = (workdir / "selected.txt").read_text(encoding="ascii").splitlines()
        assert_true(selected[0] == "iccdev.test.03", f"wrong first selected test: {selected}")
        assert_true(selected[-1] == "iccdev.test.12", f"wrong last selected test: {selected}")
        assert_true(len(selected) == 10, f"wrong selection length: {selected}")


def test_smoke_plus_recent_selects_core_coverage() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        tests = ["iccdev.test.01", *SMOKE_TESTS, "iccdev.describe-sink-api"]
        result = run_runner(
            ["--smoke", "--limit", "1", "--selection-output", str(workdir / "selected.txt")],
            workdir, tests,
        )
        assert_true(result.returncode == 0, result.stderr)
        selected = (workdir / "selected.txt").read_text(encoding="ascii").splitlines()
        assert_true(selected == [*SMOKE_TESTS, "iccdev.describe-sink-api"],
                    f"wrong smoke selection: {selected}")
        calls = read_calls(workdir)
        assert_true("-R" in calls[1], f"missing -R in smoke run: {calls[1]}")
        regex = calls[1][calls[1].index("-R") + 1]
        for name in selected:
            assert_true(re.escape(name) in regex, f"missing selected test {name}: {regex}")


def test_smoke_deduplicates_recent_test() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(
            ["--smoke", "--limit", "1", "--selection-output", str(workdir / "selected.txt")],
            workdir, [*SMOKE_TESTS],
        )
        assert_true(result.returncode == 0, result.stderr)
        selected = (workdir / "selected.txt").read_text(encoding="ascii").splitlines()
        assert_true(selected == SMOKE_TESTS, f"smoke test duplicated: {selected}")


def test_smoke_without_limit_keeps_full_suite() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        tests = ["iccdev.test.01", *SMOKE_TESTS, "iccdev.describe-sink-api"]
        result = run_runner(
            ["--smoke", "--selection-output", str(workdir / "selected.txt")],
            workdir, tests,
        )
        assert_true(result.returncode == 0, result.stderr)
        selected = (workdir / "selected.txt").read_text(encoding="ascii").splitlines()
        assert_true(selected == tests, f"full suite order changed: {selected}")
        assert_true("-R" not in read_calls(workdir)[1], "full suite must not be filtered")


def test_missing_smoke_fails_before_execution() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--smoke", "--limit", "1"],
                            workdir, SMOKE_TESTS[:-1])
        assert_true(result.returncode == 1, f"missing smoke returned {result.returncode}")
        assert_true("core smoke CTests not registered: iccdev.applyprofiles-headless-receipts"
                    in result.stderr, result.stderr)
        assert_true(len(read_calls(workdir)) == 1, "CTest execution should not run")


def test_empty_listing_fails_before_execution() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--limit", "1"], workdir, [])
        assert_true(result.returncode == 1, f"empty listing returned {result.returncode}")
        assert_true("ctest listing did not include any test names" in result.stderr,
                    result.stderr)
        assert_true(len(read_calls(workdir)) == 1, "CTest execution should not run")


def test_invalid_limits_fail_before_ctest() -> None:
    for limit in ("0", "-1"):
        with tempfile.TemporaryDirectory() as tmp:
            workdir = Path(tmp)
            result = run_runner(["--limit", limit], workdir)
            assert_true(result.returncode == 2, f"limit {limit} returned {result.returncode}")
            assert_true("--limit must be at least 1" in result.stderr, result.stderr)
            assert_true(not (workdir / "ctest-calls.txt").exists(), "ctest should not run")


def test_parallel_level_is_validated_and_forwarded() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--parallel", "7"], workdir)
        assert_true(result.returncode == 0, result.stderr)
        calls = read_calls(workdir)
        assert_true("--parallel" in calls[1], f"missing --parallel in run: {calls[1]}")
        parallel = calls[1][calls[1].index("--parallel") + 1]
        assert_true(parallel == "7", f"wrong parallel level: {parallel}")

    for level in ("0", "-1"):
        with tempfile.TemporaryDirectory() as tmp:
            workdir = Path(tmp)
            result = run_runner(["--parallel", level], workdir)
            assert_true(result.returncode == 2, f"parallel {level} returned {result.returncode}")
            assert_true("--parallel must be at least 1" in result.stderr, result.stderr)
            assert_true(not (workdir / "ctest-calls.txt").exists(), "ctest should not run")


def test_label_excludes_are_combined() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        result = run_runner(["--label-exclude", "slow", "--label-exclude", "calculator"], workdir)
        assert_true(result.returncode == 0, result.stderr)
        calls = read_calls(workdir)
        for call in calls:
            assert_true("-LE" in call, f"missing -LE in call: {call}")
            label = call[call.index("-LE") + 1]
            assert_true(label == "(slow)|(calculator)", f"wrong label exclude regex: {label}")


def test_workflow_wires_limit_and_smoke() -> None:
    workflow = TOOL_WORKFLOW.read_text(encoding="utf-8")
    assert_true('case "$CTEST_RECENT_LIMIT" in' in workflow, "missing ctest limit validation")
    assert_true('--parallel "$(nproc)"' in workflow, "Linux workflow must use detected CTest concurrency")
    assert_true('ctest_args+=(--limit "$CTEST_RECENT_LIMIT")' in workflow, "missing limit plumbing")
    assert_true('"0")' in workflow, "workflow must treat 0 as omit-limit")
    assert_true('--limit "$CTEST_RECENT_LIMIT"' not in workflow.replace('ctest_args+=(--limit "$CTEST_RECENT_LIMIT")', ''), "limit must be passed only through guarded ctest_args")
    assert_true(re.search(r'case "\$CTEST_MODE" in\s+fast\)\s+ctest_args\+=\(--smoke\b',
                          workflow) is not None,
                "fast mode must add the core smoke set")


def main() -> int:
    tests = [
        test_no_limit_selects_all,
        test_limit_one_selects_last_test,
        test_limit_ten_selects_last_ten_tests,
        test_smoke_plus_recent_selects_core_coverage,
        test_smoke_deduplicates_recent_test,
        test_smoke_without_limit_keeps_full_suite,
        test_missing_smoke_fails_before_execution,
        test_empty_listing_fails_before_execution,
        test_invalid_limits_fail_before_ctest,
        test_parallel_level_is_validated_and_forwarded,
        test_label_excludes_are_combined,
        test_workflow_wires_limit_and_smoke,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
