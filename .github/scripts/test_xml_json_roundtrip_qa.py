#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# See LICENSE.md in the repository root for the BSD-3-Clause license.

"""Deterministic runner contracts, plus an explicitly selected native control."""

import contextlib
import argparse
import csv
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import iccdev_xml_json_roundtrip_qa as qa


BASELINE_MATCH = (
    "summary-metrics:\nstatus=match\n"
    "diagnostics:\nstatus=match\n"
    "profile-results:\nstatus=match\n"
)


class RoundtripContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="iccdev-roundtrip-contract-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.inputs = self.root / "inputs"
        self.inputs.mkdir()
        self.profile = self.inputs / "control.icc"
        self.profile.write_bytes(b"profile-data")
        self.tools = {name: self.root / name for name in qa.TOOLS}
        for tool in self.tools.values():
            tool.write_bytes(b"test-tool")
        self.behavior = {}
        self.log_messages = {}
        self.enterContext(patch.object(qa, "resolve_tools", return_value=self.tools))
        self.enterContext(patch.object(qa.subprocess, "run", side_effect=self.fake_run))

    # unittest.enterContext was added after the minimum Python version.
    def enterContext(self, manager):
        value = manager.__enter__()
        self.addCleanup(manager.__exit__, None, None, None)
        return value

    def fake_run(self, command, stdout, **kwargs):
        name = Path(command[0]).name
        source = Path(command[2] if name == "dump" else command[1])
        key = ("returned-" if source.is_relative_to(self.root) and
               any(part in ("from-xml", "from-json") for part in source.parts) else "") + name
        code = self.behavior.get(key, 0)
        stdout.write(self.log_messages.get(key, b""))
        if isinstance(code, Exception):
            raise code
        if name != "dump" and code != "missing":
            shutil.copyfile(command[1], command[2])
            if code == "empty":
                Path(command[2]).write_bytes(b"")
        return subprocess.CompletedProcess(command, 0 if code in ("missing", "empty") else code)

    def invoke(self, output="out", options=(), profile=None):
        path = self.root / output
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = qa.main([*options, str(profile or self.inputs), str(path)])
        return result, path

    def rows(self, output):
        with (output / "results.tsv").open(encoding="ascii", newline="") as stream:
            return list(csv.DictReader(stream, delimiter="\t"))

    def test_clean_roundtrip_and_ascii_evidence(self):
        code, output = self.invoke()
        self.assertEqual(code, 0)
        row = self.rows(output)[0]
        self.assertEqual(row["overall"], "pass")
        self.assertEqual(row["from_xml_identical"], "yes")
        self.assertEqual(row["from_json_identical"], "yes")
        self.assertTrue(json.loads((output / "run.json").read_text())["complete"])
        self.assertTrue(list((output / "logs").rglob("*.command.json")))

    def test_conversion_rejection_with_output_is_not_signal(self):
        self.behavior["from_xml"] = 255
        code, output = self.invoke()
        self.assertEqual(code, 0)
        self.assertEqual(self.rows(output)[0]["from_xml_status"], "warning-output")
        self.assertIn("signal_terminations=0", (output / "summary.txt").read_text())

    def test_rejected_source_validation_is_not_fidelity_success(self):
        self.behavior["dump"] = 255
        code, output = self.invoke()
        self.assertEqual(code, 1)
        row = self.rows(output)[0]
        self.assertEqual(row["source_validation_status"], "tool-rejection")
        self.assertEqual(row["from_xml_source_fidelity"], "n/a")

    def test_rejected_returned_validation_is_not_fidelity_success(self):
        self.behavior["returned-dump"] = 1
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["from_json_source_fidelity"], "n/a")

    def test_signal_after_output_cannot_pass(self):
        self.behavior["from_xml"] = -11
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["from_xml_status"], "signal")
        self.assertIn("signal_terminations=1", (output / "summary.txt").read_text())

    def test_timeout_is_preserved_separately(self):
        self.behavior["to_json"] = subprocess.TimeoutExpired(["to_json"], 1)
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["to_json_status"], "timeout")
        self.assertIn("timeouts=1", (output / "summary.txt").read_text())

    def test_sanitizer_exit_zero_cannot_pass(self):
        self.log_messages["returned-dump"] = b"runtime error: invalid shift\n"
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertIn("sanitizer_findings=2", (output / "summary.txt").read_text())

    def test_loader_failure_has_explicit_evidence(self):
        self.behavior["to_json"] = OSError("loader missing")
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["to_json_status"], "launch-error")

    def test_loader_diagnostic_and_exit_127_is_a_launch_failure(self):
        self.behavior["to_json"] = 127
        self.log_messages["to_json"] = (
            b"to_json: error while loading shared libraries: libmissing.so: "
            b"cannot open shared object file: No such file or directory\n")
        code, output = self.invoke("loader-failure")
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["to_json_status"], "launch-error")
        self.assertIn("launch_errors=1", (output / "summary.txt").read_text())
        self.assertIn("error while loading shared libraries",
                      (output / "diagnostics.tsv").read_text())

        self.log_messages["to_json"] = b""
        code, output = self.invoke("exit-127-only")
        self.assertEqual(code, 0)
        self.assertEqual(self.rows(output)[0]["to_json_status"], "warning-output")
        self.assertIn("launch_errors=0", (output / "summary.txt").read_text())

    def test_missing_conversion_output(self):
        self.behavior["to_xml"] = "missing"
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["from_xml_status"], "not-run")
        self.assertEqual(self.rows(output)[0]["from_xml_size_delta"], "n/a")

    def test_empty_output_cannot_be_validated_as_success(self):
        self.behavior["from_json"] = "empty"
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["from_json_validation_status"], "not-run")

    def test_matching_critical_diagnostics_are_retained(self):
        message = b"Critical Error! - new critical validation problem\n"
        self.log_messages.update({"dump": message, "returned-dump": message})
        code, output = self.invoke()
        self.assertEqual(code, 3)
        self.assertEqual(self.rows(output)[0]["from_xml_source_fidelity"], "same")
        self.assertIn("Critical Error!", (output / "diagnostics.tsv").read_text())

    def test_profile_headers_are_not_diagnostics_or_sanitizer_findings(self):
        self.log_messages["dump"] = (
            b"Profile: '/input/ERROR: AddressSanitizer:DEADLYSIGNAL failed.icc'\n")
        self.log_messages["returned-dump"] = (
            b"Profile: '/evidence/from-xml/ERROR: AddressSanitizer:DEADLYSIGNAL failed.icc'\n")
        code, output = self.invoke()
        self.assertEqual(code, 0)
        self.assertEqual((output / "diagnostics.tsv").read_text().count("\n"), 1)
        self.assertIn("sanitizer_findings=0", (output / "summary.txt").read_text())

    def test_rejected_sanitizer_named_profile_is_not_a_sanitizer_finding(self):
        if os.name == "nt":
            self.skipTest("Windows disallows colons in filenames")
        rejected = self.inputs / "AddressSanitizer:DEADLYSIGNAL.icc"
        self.profile.rename(rejected)
        self.behavior["dump"] = 255
        self.log_messages["dump"] = (
            f"Unable to parse '{rejected}' as ICC profile!\n".encode("ascii"))
        code, output = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(self.rows(output)[0]["source_validation_status"], "tool-rejection")
        self.assertIn("sanitizer_findings=0", (output / "summary.txt").read_text())

    def test_ascii_reports_escape_filename_control_characters(self):
        if os.name == "nt":
            self.skipTest("Windows disallows control characters in filenames")
        self.profile.rename(self.inputs / "space tab\tline\n.icc")
        code, output = self.invoke()
        self.assertEqual(code, 0)
        row = self.rows(output)[0]
        self.assertEqual(row["profile"], r"space tab\tline\n.icc")
        (output / "developer-summary.txt").read_bytes().decode("ascii")

    def test_returned_diagnostics_drive_parity_and_fidelity(self):
        self.log_messages["dump"] = b"Warning! - Negative Z value\n"
        code, output = self.invoke()
        self.assertEqual(code, 0)
        row = self.rows(output)[0]
        self.assertEqual(row["overall"], "warning")
        self.assertEqual(row["from_xml_source_fidelity"], "masked")
        self.assertEqual(row["xml_json_diagnostic_parity"], "same")

    def test_broad_known_tag_match_does_not_hide_new_diagnostic(self):
        self.log_messages["dump"] = (
            b"Error! - Unknown 'aapy': Invalid data flag encoding and a new defect\n")
        code, output = self.invoke()
        self.assertEqual(code, 3)
        self.assertIn("untriaged", (output / "diagnostics.tsv").read_text())

    def test_same_stem_case_extensions_keep_distinct_artifacts(self):
        alternate = self.inputs / "control.ICC"
        if alternate.exists():
            self.skipTest("filesystem is case-insensitive")
        alternate.write_bytes(b"second-profile")
        code, output = self.invoke(options=("--jobs", "2"))
        self.assertEqual(code, 0)
        self.assertEqual(len(self.rows(output)), 2)
        self.assertEqual((output / "from-json/control.ICC").read_bytes(), b"second-profile")

    def test_nested_corpus_names_do_not_collide_with_serialized_outputs(self):
        (self.inputs / "a.icc").write_bytes(b"top-level")
        xml_nested = self.inputs / "a.icc.xml"
        json_nested = self.inputs / "a.icc.json"
        xml_nested.mkdir()
        json_nested.mkdir()
        (xml_nested / "b.icc").write_bytes(b"xml-nested")
        (json_nested / "c.icc").write_bytes(b"json-nested")

        code, output = self.invoke()
        self.assertEqual(code, 0)
        self.assertEqual(len(self.rows(output)), 4)
        self.assertEqual((output / "xml/a.icc/profile.xml").read_bytes(), b"top-level")
        self.assertEqual((output / "xml/a.icc.xml/b.icc/profile.xml").read_bytes(),
                         b"xml-nested")
        self.assertEqual((output / "json/a.icc/profile.json").read_bytes(), b"top-level")
        self.assertEqual((output / "json/a.icc.json/c.icc/profile.json").read_bytes(),
                         b"json-nested")

    def test_existing_output_and_dangling_symlink_are_rejected(self):
        existing = self.root / "existing"
        existing.mkdir()
        sentinel = existing / "keep"
        sentinel.write_text("sentinel")
        self.assertEqual(self.invoke("existing")[0], 2)
        self.assertEqual(sentinel.read_text(), "sentinel")
        link = self.root / "link"
        try:
            link.symlink_to(self.root / "missing")
        except OSError:
            self.skipTest("symlink privilege unavailable")
        self.assertEqual(self.invoke("link")[0], 2)
        self.assertFalse((self.root / "missing").exists())

    def test_empty_input_does_not_create_output(self):
        self.profile.unlink()
        code, output = self.invoke()
        self.assertEqual(code, 2)
        self.assertFalse(output.exists())

    def test_explicit_external_output_is_required(self):
        fallback = self.root / "fallback"
        with patch.dict(os.environ, {"ICCDEV_TEST_OUTDIR": str(fallback)}), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            qa.main([str(self.inputs)])
        self.assertFalse(fallback.exists())
        nested = self.inputs / "evidence"
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            code = qa.main([str(self.inputs), str(nested)])
        self.assertEqual(code, 2)
        self.assertFalse(nested.exists())

    def test_baseline_ignores_jobs_but_detects_input_content(self):
        self.assertEqual(self.invoke("baseline")[0], 0)
        options = ("--compare-baseline", str(self.root / "baseline"), "--jobs", "2")
        code, output = self.invoke("repeat", options=options)
        self.assertEqual(code, 0)
        self.assertEqual((output / "baseline-comparison.txt").read_text(), BASELINE_MATCH)
        self.profile.write_bytes(b"changed-data")
        code, output = self.invoke("changed", options=options)
        self.assertEqual(code, 0)
        self.assertIn("profile-results:\nstatus=changed", (output / "baseline-comparison.txt").read_text())

    def test_baseline_normalizes_evidence_paths_but_preserves_raw_logs(self):
        first = self.root / "first"
        self.log_messages["from_xml"] = (
            f"Unable to Parse '{first}/xml/control.icc/profile.xml'\n".encode("ascii"))
        self.assertEqual(self.invoke("first")[0], 3)
        diagnostic = (first / "diagnostics.tsv").read_text()
        self.assertIn("<evidence>/xml/control.icc/profile.xml", diagnostic)
        raw_log = first / "logs/control.icc/xml-to-icc.log"
        self.assertIn(str(first), raw_log.read_text())

        repeat = self.root / "repeat"
        self.log_messages["from_xml"] = (
            f"Unable to Parse '{repeat}/xml/control.icc/profile.xml'\n".encode("ascii"))
        options = ("--compare-baseline", str(first))
        self.assertEqual(self.invoke("repeat", options=options)[0], 3)
        self.assertEqual((repeat / "baseline-comparison.txt").read_text(), BASELINE_MATCH)

    def test_invalid_baseline_fails_before_output_creation(self):
        baseline = self.root / "baseline"
        baseline.mkdir()
        (baseline / "run.json").write_text('{"schema_version": 1, "complete": false}')
        code, output = self.invoke(options=("--compare-baseline", str(baseline)))
        self.assertEqual(code, 2)
        self.assertFalse(output.exists())

    def test_malformed_completed_baseline_is_setup_error(self):
        self.assertEqual(self.invoke("baseline")[0], 0)
        baseline = self.root / "baseline"
        (baseline / "summary.txt").write_text("not-profiles=1\n")
        code, output = self.invoke(options=("--compare-baseline", str(baseline)))
        self.assertEqual(code, 2)
        self.assertFalse(output.exists())
        (baseline / "run.json").write_text("[]")
        self.assertEqual(self.invoke(options=("--compare-baseline", str(baseline)))[0], 2)

    def test_real_subprocess_timeout_and_positive_exit(self):
        with patch.object(qa.subprocess, "run", self.real_subprocess_run):
            result = qa.run_stage(
                [sys.executable, "-c", "raise SystemExit(255)"], self.root / "exit.log", 10)
            self.assertEqual(result.status, "tool-rejection")
            self.assertFalse(result.signal)
            loader_code = (
                "import sys; "
                "sys.stderr.write('tool: error while loading shared libraries: libmissing.so: "
                "cannot open shared object file: No such file or directory\\n'); "
                "raise SystemExit(127)"
            )
            result = qa.run_stage(
                [sys.executable, "-c", loader_code], self.root / "loader.log", 10)
            self.assertEqual(result.status, "launch-error")
            self.assertTrue(result.diagnostics)
            result = qa.run_stage(
                [sys.executable, "-c", "raise SystemExit(127)"],
                self.root / "exit-127.log", 10)
            self.assertEqual(result.status, "tool-rejection")
            result = qa.run_stage(
                [sys.executable, "-c", "import time; time.sleep(10)"],
                self.root / "timeout.log", 0.1)
            self.assertEqual(result.status, "timeout")
            if os.name != "nt":
                result = qa.run_stage(
                    [sys.executable, "-c", "import os, signal; os.kill(os.getpid(), signal.SIGTERM)"],
                    self.root / "signal.log", 10)
                self.assertEqual(result.status, "signal")

    def test_windows_native_rejections_are_not_exception_exits(self):
        for code in (0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD):
            with self.subTest(code=code):
                self.assertFalse(qa.is_signal_exit(code, "nt"))
        self.assertTrue(qa.is_signal_exit(0xC0000005, "nt"))

    def test_numeric_limits_and_decimal_parsing(self):
        self.assertEqual(qa.number("08"), 8)
        for value in ("-1", "x", "99999999999999999999", "2147483648"):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                qa.number(value)

    def test_flat_tools_and_explicit_invalid_override(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            suffix = ".exe" if os.name == "nt" else ""
            for _, _, name in qa.TOOLS.values():
                path = directory / (name + suffix)
                path.write_bytes(b"tool")
                path.chmod(0o755)
            environment = {key: value for key, value in os.environ.items()
                           if key not in {item[0] for item in qa.TOOLS.values()}}
            environment["ICCDEV_TOOLS_DIR"] = str(directory)
            with patch.dict(os.environ, environment, clear=True):
                # Call the real resolver rather than the per-test stub.
                resolved = self.real_resolver()
                self.assertEqual(len(resolved), 5)
                os.environ["ICC_TO_XML"] = ""
                with self.assertRaises(ValueError):
                    self.real_resolver()


RoundtripContracts.real_resolver = staticmethod(qa.resolve_tools)
RoundtripContracts.real_subprocess_run = staticmethod(subprocess.run)


def native_control(profile):
    parent = os.environ.get("ICCDEV_TEST_OUTDIR")
    if parent:
        Path(parent).mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="iccdev-native-roundtrip-", dir=parent))
    print(f"[EVIDENCE] {root}", flush=True)
    for name, jobs in (("first", "1"), ("repeat", "2")):
        arguments = ["--timeout", "10", "--jobs", jobs, str(profile), str(root / name)]
        if name == "repeat":
            arguments[:0] = ["--compare-baseline", str(root / "first")]
        if qa.main(arguments) != 0:
            raise RuntimeError("Native round-trip control failed")
        with (root / name / "results.tsv").open(encoding="ascii", newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
        if len(rows) != 1 or rows[0]["overall"] != "pass" or any(
            rows[0]["from_" + side + "_identical"] != "yes" or
            rows[0]["from_" + side + "_source_fidelity"] != "same"
            for side in ("xml", "json")
        ):
            raise RuntimeError(f"Native control did not preserve fidelity: {rows}")
    comparison = (root / "repeat/baseline-comparison.txt").read_text()
    if comparison != BASELINE_MATCH:
        raise RuntimeError(f"Native baseline mismatch: {comparison}")
    print("[PASS] Native XML/JSON identity, validation, and baseline contracts")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--native-control":
        native_control(Path(sys.argv[2]))
    else:
        unittest.main()
