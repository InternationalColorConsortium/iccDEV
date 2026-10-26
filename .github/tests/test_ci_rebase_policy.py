#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# https://color.org
#
# Intent: Verify rebase check identity and manual audit enforcement.
###############################################################

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import yaml


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github/workflows"


def load_workflow(name):
    return yaml.safe_load((WORKFLOWS / name).read_text(encoding="ascii"))


class RebasePolicyTests(unittest.TestCase):
    def test_ancestry_cannot_supply_protected_summary(self):
        workflow = load_workflow("ci-rebase-safety.yml")
        triggers = workflow.get("on", workflow.get(True))
        self.assertEqual(triggers["pull_request"]["types"], ["synchronize"])
        self.assertEqual(workflow["jobs"]["rebase-safety"]["name"], "Rebase Safety")
        self.assertNotIn(
            "PR Summary", [job.get("name") for job in workflow["jobs"].values()]
        )

    def test_expensive_lanes_do_not_run_on_synchronize(self):
        for name in (
            "ci-pr-action.yml",
            "ci-sanitizer-regression.yml",
            "ci-json-roundtrip.yml",
            "ci-mcp-server.yml",
            "ci-preflight-safety.yml",
            "ci-matlab.yml",
            "ci-risk-analysis-gate.yml",
        ):
            with self.subTest(workflow=name):
                workflow = load_workflow(name)
                triggers = workflow.get("on", workflow.get(True))
                self.assertIn("workflow_dispatch", triggers)
                if "pull_request" in triggers:
                    self.assertNotIn(
                        "synchronize", triggers["pull_request"]["types"]
                    )

    def test_manual_prologue_audit_rejects_invalid_unchanged_workflow(self):
        workflow = load_workflow("ci-pr-risk-security-analysis.yml")
        step = next(
            step
            for step in workflow["jobs"]["workflow-security-audit"]["steps"]
            if step.get("name") == "1c. Bash Prologue Governance Audit"
        )
        self.assertEqual(
            step["env"]["AUDIT_EVENT_NAME"], "${{ github.event_name }}"
        )
        for valid in (True, False):
            with self.subTest(valid=valid), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                (root / ".github/workflows").mkdir(parents=True)
                trusted = root / "trusted"
                trusted.mkdir()
                shutil.copy2(
                    ROOT / ".github/scripts/check-workflow-bash-prologue.py",
                    trusted,
                )
                run = 'set -euo pipefail\ngit config --global credential.helper ""\n'
                if valid:
                    run += "unset GITHUB_TOKEN || true\n"
                (root / ".github/workflows/fixture.yml").write_text(
                    yaml.safe_dump(
                        {"jobs": {"check": {"runs-on": "ubuntu-latest",
                                           "steps": [{"run": run}]}}}
                    ),
                    encoding="ascii",
                )
                env = dict(
                    os.environ,
                    HOME=str(root),
                    SCAN_ROOT=str(root),
                    TRUSTED_SCRIPTS=str(trusted),
                    RISK_REPORT_PATH=str(root / "report.txt"),
                    AUDIT_EVENT_NAME="workflow_dispatch",
                    BASE_SHA="HEAD",
                    BASH_ENV="/dev/null",
                )
                subprocess.run(
                    ["git", "init", "--quiet", str(root)], env=env, check=True
                )
                subprocess.run(
                    ["git", "-C", str(root), "add", "."], env=env, check=True
                )
                subprocess.run(
                    ["git", "-C", str(root), "-c", "user.name=CI Test",
                     "-c", "user.email=ci-test@example.invalid",
                     "-c", "commit.gpgsign=false", "commit", "--quiet", "-m",
                     "test: unchanged workflow fixture"],
                    env=env, check=True,
                )
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c", step["run"]],
                    cwd=root, env=env, capture_output=True, text=True,
                )
                self.assertEqual(
                    result.returncode, 0 if valid else 1,
                    result.stdout + result.stderr,
                )
                report = (root / "report.txt").read_text(encoding="ascii")
                self.assertIn(
                    "[OK] All Bash" if valid else "[HIGH] Bash prologue", report
                )


if __name__ == "__main__":
    unittest.main()
