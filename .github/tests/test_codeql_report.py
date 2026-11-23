#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################

"""Tests for codeql-report.sh security-finding detail output."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "codeql-report.sh"


class CodeqlReportTests(unittest.TestCase):
    def run_report(self, rule_id: str) -> subprocess.CompletedProcess[str]:
        sarif = {
            "version": "2.1.0",
            "runs": [
                {
                    "tool": {
                        "driver": {
                            "name": "CodeQL",
                            "rules": [
                                {
                                    "id": rule_id,
                                    "properties": {
                                        "problem.severity": "warning",
                                        "security-severity": "7.5",
                                        "tags": ["security", "external/cwe/cwe-125"],
                                    },
                                }
                            ],
                        }
                    },
                    "results": [
                        {
                            "ruleId": rule_id,
                            "level": "warning",
                            "message": {"text": "test finding"},
                            "locations": [
                                {
                                    "physicalLocation": {
                                        "artifactLocation": {"uri": "IccProfLib/test.cpp"},
                                        "region": {"startLine": 42},
                                    }
                                }
                            ],
                        }
                    ],
                }
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.sarif"
            path.write_text(json.dumps(sarif), encoding="ascii")
            return subprocess.run(
                [str(SCRIPT), str(path)],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_warning_level_custom_finding_has_location_detail(self) -> None:
        result = self.run_report("iccdev/test-rule")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SECURITY AND ICCDEV FINDINGS", result.stdout)
        self.assertIn("IccProfLib/test.cpp:42", result.stdout)
        self.assertIn("Security severity: 7.5", result.stdout)

    def test_warning_level_standard_security_finding_has_detail(self) -> None:
        result = self.run_report("cpp/test-rule")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SECURITY AND ICCDEV FINDINGS", result.stdout)
        self.assertIn("IccProfLib/test.cpp:42", result.stdout)

    def test_invalid_sarif_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.sarif"
            path.write_text("{}\n", encoding="ascii")
            result = subprocess.run(
                [str(SCRIPT), str(path)],
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn("expected a SARIF 2.1.0 object", result.stderr)


if __name__ == "__main__":
    unittest.main()
