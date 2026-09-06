# Copyright (c) 2026 International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.
"""Focused container scanner contracts, using executable stubs, not a corpus."""

import collections
import csv
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest

SCRIPTS = Path(__file__).resolve().parents[2] / ".github" / "scripts"
spec = importlib.util.spec_from_file_location("pawg_classifier", SCRIPTS / "icc-pawg-qa-classify.py")
classifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(classifier)


def report(mode, states=("OK", "N/A")):
    counts = collections.Counter(classifier.STATES[state] for state in states)
    summary = {key: counts[key] for key in set(classifier.STATES.values())}
    summary["total"] = len(states)
    # Benign questions/details deliberately contain historical false positives.
    detail = "Zip compressed compression invalid warning FAIL:0 [FAIL]"
    items = [{"id": f"S{i + 1}", "verdict": state, "detail": detail}
             for i, state in enumerate(states)]
    if mode == "json":
        return json.dumps({"tool": "iccPawgReport", "summary": summary, "items": items})
    return "\n".join(
        ["  [{verdict:<4}] {id} ".format(**item) + detail for item in items]
        + [f"  Total checklist items:  {len(items)}"]
        + [f"  {state}:  {summary[key]}" for state, key in classifier.SUMMARY_STATES.items()]
    ) + "\n"


@pytest.mark.parametrize("mode", ["text", "json"])
@pytest.mark.parametrize("state,index", [("OK", None), ("PASS", None), ("N/A", None),
                                        ("FAIL", 1), ("WARN", 2), ("GAP", 3), ("NOT RUN", 4)])
def test_structured_states(mode, state, index):
    expected = [0] * 5
    if index is not None:
        expected[index] = 1
    assert classifier.classify(report(mode, (state,)), mode) == tuple(expected)


@pytest.mark.parametrize("text", ["", "{}", "[]", "null", "{", "PASS: 0", "garbage"])
@pytest.mark.parametrize("mode", ["text", "json"])
def test_invalid_reports_do_not_pass(text, mode):
    assert classifier.classify(text, mode)[0] > 0


def test_incomplete_text_retains_all_issue_states():
    text = report("text", ("FAIL", "WARN", "GAP", "NOT RUN"))
    text = text.split("  Total checklist items:")[0]
    counts = classifier.classify(text, "text")
    assert counts[0] > 0 and counts[1:] == (1, 1, 1, 1)


def test_inconsistent_text_summary_is_invalid():
    text = report("text").replace("  FAIL:  0", "  FAIL:  1")
    assert classifier.classify(text, "text")[0] > 0


@pytest.mark.parametrize("mutation", ["summary", "duplicate", "unknown", "empty", "boolean", "negative"])
def test_invalid_json_contract(mutation):
    data = json.loads(report("json"))
    if mutation == "summary":
        data["summary"]["fail"] = 1
    elif mutation == "duplicate":
        data["items"][1]["id"] = "S1"
    elif mutation == "unknown":
        data["items"][0]["verdict"] = "UNKNOWN"
    elif mutation == "empty":
        data["items"] = []
    elif mutation == "boolean":
        data["summary"]["fail"] = False
    else:
        data["summary"]["fail"] = -1
    assert classifier.classify(json.dumps(data), "json")[0] > 0


@pytest.fixture
def scanner_tree(tmp_path):
    scripts = tmp_path / "repo" / ".github" / "scripts"
    scripts.mkdir(parents=True)
    for path in SCRIPTS.glob("icc-*-qa-*"):
        shutil.copy(path, scripts / path.name)
    inputs = tmp_path / "inputs"
    inputs.mkdir()
    (inputs / "test.icc").write_bytes(b"profile stub")
    return scripts, inputs


def stub(path, output="", code=0, sleep=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("#!/usr/bin/env python3\nimport sys,time\n"
                    + ("time.sleep(5)\n" if sleep else "")
                    + f"print({output!r})\nsys.exit({code})\n", encoding="ascii")
    path.chmod(0o755)
    return path


def scan(tree, tmp_path, tool="pawg", env=None, args=()):
    scripts, inputs = tree
    clean_env = {key: value for key, value in os.environ.items()
                 if not key.startswith(("ICC_", "ICCDEV_"))}
    clean_env["PATH"] = "/usr/bin:/bin"
    clean_env.update(env or {})
    out = tmp_path / "out"
    result = subprocess.run(["bash", str(scripts / f"icc-{tool}-qa-scan.sh"),
                             "--out-dir", str(out), "--timeout", "1", *args, str(inputs)],
                            env=clean_env, capture_output=True, text=True, timeout=15)
    rows = []
    if (out / "results.tsv").exists():
        with (out / "results.tsv").open() as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
    return result, rows


@pytest.mark.parametrize("output,code,expected", [
    (report("text"), 0, "PASS"), (report("text", ("WARN",)), 0, "QA-ISSUE"),
    ("", 0, "QA-ISSUE"), ("error", 1, "FAIL"),
    ("AddressSanitizer: test", 0, "CRASH"), ("runtime error: test", 0, "CRASH"),
    ("SIGSEGV", 0, "CRASH"), ("", 132, "CRASH"), ("", 124, "TIMEOUT"),
])
def test_scanner_classification(scanner_tree, tmp_path, output, code, expected):
    binary = stub(tmp_path / "tool", output, code)
    result, rows = scan(scanner_tree, tmp_path, args=("--tool", str(binary), "--variant", "text"))
    assert rows[0]["status"] == expected, result.stdout + result.stderr
    assert result.returncode == (1 if expected in {"CRASH", "TIMEOUT"} else 0)


def test_real_timeout(scanner_tree, tmp_path):
    binary = stub(tmp_path / "tool", sleep=True)
    result, rows = scan(scanner_tree, tmp_path, args=("--tool", str(binary), "--variant", "text"))
    assert result.returncode == 1 and rows[0]["status"] == "TIMEOUT"


@pytest.mark.parametrize("tool,variable,name,variant", [
    ("pawg", "ICC_PAWG_REPORT", "iccPawgReport", "text"),
    ("dumpprofile", "ICC_DUMP_PROFILE", "iccDumpProfile", "basic"),
    ("roundtrip", "ICC_ROUND_TRIP", "iccRoundTrip", "intent-1"),
])
@pytest.mark.parametrize("source", ["argument", "explicit", "configured", "flat", "checkout", "path",
                                    "bad-explicit", "empty-explicit", "bad-argument", "missing"])
def test_discovery_precedence(scanner_tree, tmp_path, tool, variable, name, variant, source):
    scripts, _ = scanner_tree
    configured = tmp_path / "configured"
    checkout = scripts.parents[1] / "Build" / "Tools"
    path_dir = tmp_path / "bin"
    env = {"ICCDEV_TOOLS_DIR": str(configured), "PATH": f"{path_dir}:/usr/bin:/bin"}
    args = ["--variant", variant]
    subdir = name.replace("icc", "Icc", 1)
    candidates = {
        "argument": tmp_path / "arg-tool", "explicit": tmp_path / "env-tool",
        "configured": configured / subdir / name, "flat": configured / name,
        "checkout": checkout / subdir / name, "path": path_dir / name,
    }
    order = list(candidates)
    if source in order:
        start = order.index(source)
        for key in order[start:]:
            # Lower precedence alternatives must never be selected.
            stub(candidates[key], report("text") if tool == "pawg" else "clean",
                 code=0 if key == source else 7)
        if source == "argument":
            args += ["--tool", str(candidates[source])]
            env[variable] = "/missing-override"
        elif source == "explicit":
            env[variable] = str(candidates[source])
    elif source != "missing":
        stub(candidates["path"], report("text"))
        if source == "bad-argument":
            args += ["--tool", "/missing-argument"]
        else:
            env[variable] = "" if source == "empty-explicit" else "/missing-override"
    result, rows = scan(scanner_tree, tmp_path, tool, env, args)
    if source in order:
        assert result.returncode == 0 and rows[0]["status"] == "PASS", result.stdout + result.stderr
    else:
        assert result.returncode == 2 and not rows, result.stdout + result.stderr


def test_non_pawg_diagnostics_unchanged(scanner_tree, tmp_path):
    binary = stub(tmp_path / "tool", "Invalid compressed data Warning")
    result, rows = scan(scanner_tree, tmp_path, "dumpprofile",
                        args=("--tool", str(binary), "--variant", "basic"))
    assert result.returncode == 0 and rows[0]["status"] == "QA-ISSUE"
    assert rows[0]["errors"] == "1" and rows[0]["warnings"] == "1"
