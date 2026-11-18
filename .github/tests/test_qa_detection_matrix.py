#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################

"""Tests for qa-detection-matrix.py."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "qa-detection-matrix.py"


class QaDetectionMatrixTests(unittest.TestCase):
    def test_valid_matrix_renders_issue_link(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "source.cpp"
            evidence.write_text("int main() { return 0; }\n", encoding="ascii")
            matrix = root / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "issues": [
                            {
                                "issue": 1,
                                "state": "open",
                                "defect_class": "test",
                                "primary_detector": "ASan",
                                "static_signal": "none",
                                "runtime_gate": "gap",
                                "coverage_role": "not-covered",
                                "evidence_paths": ["source.cpp"],
                            }
                        ],
                        "tracking_issue": 3094,
                    }
                ),
                encoding="ascii",
            )
            output = root / "matrix.md"
            result = subprocess.run(
                [
                    "python3",
                    str(SCRIPT),
                    "--input",
                    str(matrix),
                    "--repo-root",
                    str(root),
                    "--markdown",
                    str(output),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            rendered = output.read_text(encoding="ascii")
            self.assertIn("Tracking: [#3094]", rendered)
            self.assertIn("[#1]", rendered)

    def test_missing_evidence_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "issues": [
                            {
                                "issue": 1,
                                "state": "open",
                                "defect_class": "test",
                                "primary_detector": "ASan",
                                "static_signal": "none",
                                "runtime_gate": "gap",
                                "coverage_role": "not-covered",
                                "evidence_paths": ["missing.cpp"],
                            }
                        ],
                        "tracking_issue": 3094,
                    }
                ),
                encoding="ascii",
            )
            result = subprocess.run(
                [
                    "python3",
                    str(SCRIPT),
                    "--input",
                    str(matrix),
                    "--repo-root",
                    str(root),
                    "--markdown",
                    str(root / "matrix.md"),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 1)
            self.assertIn("evidence path is missing", result.stderr)


if __name__ == "__main__":
    unittest.main()
