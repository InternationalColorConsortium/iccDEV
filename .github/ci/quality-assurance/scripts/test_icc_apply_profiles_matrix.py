#!/usr/bin/env python3
################################################################################
# .github/ci/quality-assurance/scripts/test_icc_apply_profiles_matrix.py
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
################################################################################
"""Focused classification and contract checks for the ApplyProfiles matrix."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("icc_apply_profiles_matrix.py")
SPEC = importlib.util.spec_from_file_location("icc_apply_profiles_matrix", MODULE_PATH)
MATRIX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MATRIX)


class ApplyProfilesMatrixTests(unittest.TestCase):
    def test_diagnostic_precedence_and_memory_summary(self):
        self.assertEqual(
            MATRIX.classify_process(0, True, "AddressSanitizer: heap-use-after-free"),
            "SANITIZER_FINDING",
        )
        self.assertEqual(
            MATRIX.classify_process(0, False,
                                    "ERROR SUMMARY: 0 errors\nERROR SUMMARY: 1 errors"),
            "MEMORY_TOOL_FINDING",
        )
        self.assertIsNone(
            MATRIX.classify_process(0, False, "ERROR SUMMARY: 0 errors")
        )

    def test_rgb_contract_rejects_lab_photometric(self):
        contract = {"photometric": 2, "extra_samples": [],
                    "pixel_sha256_equals_source": True}
        source = {"pixel_sha256": "source-pixels"}
        output = {"photometric": 9, "extra_samples": [],
                  "pixel_sha256": "other-pixels"}
        differences = MATRIX.validate_output_contract(contract, source, output, [])
        self.assertEqual(
            {difference["field"] for difference in differences},
            {"photometric", "pixel_sha256"},
        )
        output["photometric"] = 2
        output["pixel_sha256"] = "source-pixels"
        self.assertEqual(
            MATRIX.validate_output_contract(contract, source, output, []), []
        )

    def test_profile_warning_must_be_owned_and_exact(self):
        digest = "a" * 64
        profile = {
            "role": "destination",
            "sha256": digest,
            "pawg": {"status": "warn", "checklist_warn_ids": ["C1", "S3"],
                     "summary": {"gap": 0}},
        }
        known = {digest: {"owner": "QA", "purpose": "smoke",
                          "expected_checklist_warn_ids": ["S3", "C1"]}}
        self.assertEqual(MATRIX.profile_quality_differences([profile], known), [])
        self.assertEqual(len(MATRIX.profile_quality_differences([profile], {})), 1)
        profile["pawg"]["summary"]["gap"] = 1
        self.assertEqual(len(MATRIX.profile_quality_differences([profile], known)), 1)

    def test_pawg_failure_json_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            evidence_dir = Path(directory)

            def write_failed_report(argv, timeout, stdout, stderr):
                del argv, timeout, stderr
                stdout.write_text(json.dumps({
                    "summary": {"fail": 1, "warn": 1, "gap": 0},
                    "items": [{"id": "C1", "verdict": "FAIL"},
                              {"id": "S3", "verdict": "WARN"}],
                }), encoding="ascii")
                return 1, False, 0.01

            with patch.object(MATRIX, "run_process", side_effect=write_failed_report):
                assessment = MATRIX.assess_profile(
                    evidence_dir / "bad.icc", "destination", evidence_dir / "tool",
                    evidence_dir, 30, {},
                )
            self.assertEqual(assessment["status"], "fail")
            self.assertEqual(assessment["checklist_warn_ids"], ["S3"])
            self.assertEqual(assessment["return_code"], 1)

    def test_expected_rejection_requires_absent_precondition(self):
        row = {"id": "missing-profile", "expected_outcome": "EXPECTED_REJECTION",
               "profiles": [{"role": "source_embedded", "path": "", "intent": 1}],
               "output_contract": {"output_absent": True}}
        manifest = {"schema": "iccdev-applyprofiles-matrix/v1", "rows": [row]}
        with self.assertRaises(MATRIX.MatrixError):
            MATRIX.validate_manifest(manifest)
        row["missing_precondition"] = "source_embedded_profile"
        row["expected_message"] = "Source image doesn't have embedded profile!"
        self.assertEqual(MATRIX.validate_manifest(manifest), [row])


if __name__ == "__main__":
    unittest.main()
