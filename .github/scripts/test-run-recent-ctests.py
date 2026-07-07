#!/usr/bin/env python3
"""Shim tests for run-recent-ctests.py selection and workflow plumbing."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
RUNNER = REPO_ROOT / ".github" / "scripts" / "run-recent-ctests.py"
TOOL_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "ci-iccdev-tool-tests.yml"


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
    for idx in range(1, 13):
        print(f"  Test #{idx}: iccdev.test.{idx:02d}")
    sys.exit(0)

sys.exit(0)
""",
        encoding="ascii",
    )
    shim.chmod(0o755)
    return shim, calls


def run_runner(args: list[str], workdir: Path) -> subprocess.CompletedProcess[str]:
    _shim, calls = make_ctest_shim(workdir)
    env = os.environ.copy()
    env["PATH"] = f"{workdir}{os.pathsep}{env['PATH']}"
    env["CTEST_CALLS"] = str(calls)
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


def test_invalid_limits_fail_before_ctest() -> None:
    for limit in ("0", "-1"):
        with tempfile.TemporaryDirectory() as tmp:
            workdir = Path(tmp)
            result = run_runner(["--limit", limit], workdir)
            assert_true(result.returncode == 2, f"limit {limit} returned {result.returncode}")
            assert_true("--limit must be at least 1" in result.stderr, result.stderr)
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


def test_workflow_omits_zero_limit() -> None:
    workflow = TOOL_WORKFLOW.read_text(encoding="utf-8")
    assert_true('case "$CTEST_RECENT_LIMIT" in' in workflow, "missing ctest limit validation")
    assert_true('ctest_args+=(--limit "$CTEST_RECENT_LIMIT")' in workflow, "missing limit plumbing")
    assert_true('"0")' in workflow, "workflow must treat 0 as omit-limit")
    assert_true('--limit "$CTEST_RECENT_LIMIT"' not in workflow.replace('ctest_args+=(--limit "$CTEST_RECENT_LIMIT")', ''), "limit must be passed only through guarded ctest_args")


def main() -> int:
    tests = [
        test_no_limit_selects_all,
        test_limit_one_selects_last_test,
        test_limit_ten_selects_last_ten_tests,
        test_invalid_limits_fail_before_ctest,
        test_label_excludes_are_combined,
        test_workflow_omits_zero_limit,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
