#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################

"""Tests for audit-lint-findings.py."""

from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "audit-lint-findings.py"


class AuditLintFindingsTests(unittest.TestCase):
    def run_audit(
        self,
        add_baseline: bool,
        change_source: bool = False,
        emit_finding: bool = True,
        scope_entries: list[str] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "IccProfLib" / "case.cpp"
            source.parent.mkdir(parents=True)
            source_line = "values[index] = 1;"
            source.write_text(source_line + "\n", encoding="ascii")
            digest = hashlib.sha256(source_line.encode("ascii")).hexdigest()

            cppcheck = root / "cppcheck.txt"
            cppcheck.write_text("", encoding="ascii")
            clang_tidy = root / "clang-tidy.txt"
            finding = (
                f"{source}:1:1: warning: out of bounds "
                "[clang-analyzer-security.ArrayBound]\n"
            )
            clang_tidy.write_text(finding if emit_finding else "", encoding="ascii")
            baseline = root / "baseline.json"
            reviewed = []
            if add_baseline:
                reviewed.append(
                    {
                        "tool": "clang-tidy",
                        "path": "IccProfLib/case.cpp",
                        "line": 1,
                        "checker": "clang-analyzer-security.ArrayBound",
                        "source_sha256": digest,
                        "issue": "#test",
                        "rationale": "test baseline",
                    }
                )
            baseline.write_text(json.dumps({"reviewed_findings": reviewed}), encoding="ascii")
            if change_source:
                source.write_text("values[index] = 2;\n", encoding="ascii")

            command = [
                "python3",
                str(SCRIPT),
                "--cppcheck",
                str(cppcheck),
                "--clang-tidy",
                str(clang_tidy),
                "--baseline",
                str(baseline),
                "--repo-root",
                str(root),
                "--output-dir",
                str(root / "out"),
            ]
            if scope_entries is not None:
                scope = root / "scope.txt"
                scope.write_bytes(b"\0".join(item.encode("ascii") for item in scope_entries))
                command.extend(["--scope-files", str(scope)])

            return subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
            )

    def test_reviewed_finding_passes(self) -> None:
        result = self.run_audit(add_baseline=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 reviewed, 0 unreviewed, 0 stale", result.stdout)

    def test_unreviewed_finding_fails(self) -> None:
        result = self.run_audit(add_baseline=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("1 unreviewed", result.stdout)

    def test_changed_source_invalidates_baseline(self) -> None:
        result = self.run_audit(add_baseline=True, change_source=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("1 unreviewed, 1 stale", result.stdout)

    def test_out_of_scope_baseline_is_not_stale(self) -> None:
        result = self.run_audit(
            add_baseline=True,
            emit_finding=False,
            scope_entries=["IccProfLib/other.cpp"],
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("0 reviewed, 0 unreviewed, 0 stale", result.stdout)


if __name__ == "__main__":
    unittest.main()
