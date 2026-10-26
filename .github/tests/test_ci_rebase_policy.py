#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# https://color.org
#
# Intent: Verify rebase check identity and manual audit enforcement.
###############################################################

import os
import json
from pathlib import Path
import re
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
    def test_manual_validation_publishes_actual_commit_status(self):
        workflow = load_workflow("ci-pr-action.yml")
        job = workflow["jobs"]["finalize"]
        self.assertEqual(job["permissions"]["statuses"], "write")
        step = next(step for step in job["steps"]
                    if step.get("name") == "Publish manual validation status")
        self.assertIn("always()", step["if"])
        self.assertIn("github.event_name == 'workflow_dispatch'", step["if"])
        self.assertIn("inputs.pr_number == ''", step["if"])
        self.assertIn("needs.detect-src.outputs.target_sha != ''", step["if"])
        self.assertEqual(step["env"]["TARGET_SHA"],
                         "${{ needs.detect-src.outputs.target_sha || github.sha }}")
        mock = 'gh() { printf "%s\\n" "$@"; }; '
        for result in ("success", "failure", "cancelled"):
            with self.subTest(result=result), tempfile.TemporaryDirectory() as temp:
                env = dict(os.environ, HOME=temp, BASH_ENV="/dev/null",
                           GH_REPO="example/repo", TARGET_SHA="validated-head",
                           VALIDATION_RESULT=result, RUN_URL="https://example.invalid/run")
                output = subprocess.run(
                    ["bash", "-eu", "-c", mock + step["run"]],
                    env=env, capture_output=True, text=True, check=True,
                ).stdout.splitlines()
                self.assertIn("repos/example/repo/statuses/validated-head", output)
                self.assertIn("context=PR Summary", output)
                self.assertIn("state=" + ("success" if result == "success"
                                         else "failure"), output)
        with tempfile.TemporaryDirectory() as temp:
            env = dict(os.environ, HOME=temp, GH_REPO="example/repo",
                       TARGET_SHA="head", VALIDATION_RESULT="success", RUN_URL="url")
            failed = subprocess.run(
                ["bash", "-eu", "-c", "gh() { return 1; }; " + step["run"]],
                env=env, capture_output=True, text=True,
            )
            self.assertNotEqual(failed.returncode, 0)

    def test_status_label_writes_are_bounded_and_fail_explicitly(self):
        workflow = load_workflow("update-labels.yml")
        step = next(
            step for step in workflow["jobs"]["label-status"]["steps"]
            if step.get("name") == "Evaluate and label PRs"
        )
        mock = """
gh() {
  case "$*" in
    "pr view "*"--json isCrossRepository,"*) printf '%s\\n' "$PR_DATA" ;;
    "pr view "*"--json headRefOid -q .headRefOid") printf '%s\\n' "$LATEST_HEAD" ;;
    "api "*"check-runs"*)
      if [[ "$#" != 4 || "$2" != "--paginate" || "$3" != "--slurp" ]]; then
        printf 'Invalid paginated gh api arguments: %s\\n' "$*" >&2
        return 2
      fi
      printf '%s\\n' "$CHECK_PAGES"
      return "$CHECK_STATUS"
      ;;
    "api "*"status"*) printf '%s\\n' "$LEGACY_STATUS" ;;
    "pr edit "*) printf '%s\\n' "$*" >> "$WRITE_LOG"; return "$WRITE_STATUS" ;;
    *) printf 'Unexpected gh call: %s\\n' "$*" >&2; return 2 ;;
  esac
}
"""
        for name, labels, latest_head, write_status, summary, check_status, expected_writes in (
            ("unchanged", ["pending"], "head", "0", False, "0", 0),
            ("update", ["passed", "Merge Ready"], "head", "0", False, "0", 1),
            ("moved-head", ["passed"], "new-head", "0", False, "0", 0),
            ("write-failure", ["passed"], "head", "1", False, "0", 1),
            ("validated", ["pending"], "head", "0", True, "0", 1),
            ("validated-unchanged", ["passed", "Merge Ready"], "head", "0", True, "0", 0),
            ("query-failure", ["passed"], "head", "0", True, "1", 1),
            ("late-failure-no-summary", ["passed"], "head", "0", False, "1", 1),
            ("late-failure-with-summary", ["passed"], "head", "0", True, "1", 1),
        ):
            with self.subTest(case=name), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                env = dict(
                    os.environ, HOME=temp, BASH_ENV="/dev/null",
                    GH_REPO="example/repo", PR_LIST="1",
                    GITHUB_OUTPUT=str(root / "output"),
                    GITHUB_WORKSPACE=temp, WRITE_LOG=str(root / "writes"),
                    WRITE_STATUS=write_status, LATEST_HEAD=latest_head,
                    CHECK_STATUS=check_status,
                    LEGACY_STATUS="success|1" if name.startswith("late-failure")
                    else "pending|0",
                    CHECK_PAGES=json.dumps([
                        {"check_runs": [{"name": "Rebase Safety",
                                         "status": "completed",
                                         "conclusion": "success"}]},
                        {"check_runs": [{"name": "PR Summary",
                                         "status": "completed",
                                         "conclusion": "success"}] if summary else []},
                    ]),
                    PR_DATA=json.dumps({
                        "isCrossRepository": False, "headRefOid": "head",
                        "isDraft": False, "mergeStateStatus": "CLEAN",
                        "reviewDecision": "",
                        "labels": [{"name": label} for label in labels],
                    }),
                )
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c", mock + step["run"]],
                    cwd=root, env=env, capture_output=True, text=True,
                )
                self.assertEqual(
                    result.returncode, int(write_status),
                    result.stdout + result.stderr,
                )
                writes = root / "writes"
                actual = writes.read_text().splitlines() if writes.exists() else []
                self.assertEqual(len(actual), expected_writes)
                if name in ("update", "query-failure") or name.startswith("late-failure"):
                    self.assertIn("--add-label pending", actual[0])
                    self.assertIn("--remove-label passed,failed,Merge Ready", actual[0])
                if check_status != "0":
                    self.assertIn("Check-run query failed", result.stderr)
                if name == "validated":
                    self.assertIn("--add-label passed,Merge Ready", actual[0])

    def test_status_labels_require_successful_current_head_summary(self):
        workflow = load_workflow("update-labels.yml")
        step = next(
            step for step in workflow["jobs"]["label-status"]["steps"]
            if step.get("name") == "Evaluate and label PRs"
        )
        run = step["run"]
        query = re.search(
            r"\| jq -r '([\s\S]*?)' 2>/dev/null",
            run.split("check_conclusion=$(gh api", 1)[1],
        ).group(1)
        combine = run.split("local combined", 1)[1].split(
            'local merge_ready="false"', 1
        )[0]
        self.assertIn("--paginate --slurp", run)
        self.assertIn("commits/$head/check-runs", run)
        self.assertNotIn("reviewDecision", run)
        self.assertNotIn("APPROVED", run)
        for conclusion, expected in (
            (None, "pending"),
            ("skipped", "pending"),
            ("neutral", "pending"),
            ("success", "success"),
            ("failure", "failure"),
            ("cancelled", "failure"),
            ("pending", "pending"),
        ):
            with self.subTest(summary=conclusion):
                checks = [{"name": "Rebase Safety", "status": "completed",
                           "conclusion": "success"}]
                summary = []
                if conclusion is not None:
                    summary = [{"name": "PR Summary",
                                "status": "in_progress" if conclusion == "pending"
                                else "completed", "conclusion": conclusion}]
                result = subprocess.run(
                    ["jq", "-r", query],
                    input=json.dumps([{"check_runs": checks},
                                      {"check_runs": summary}]),
                    capture_output=True, text=True, check=True,
                )
                self.assertEqual(result.stdout.strip(), expected)
                for state, count in (("pending", "0"), ("success", "1")):
                    env = dict(os.environ, state=state, status_count=count,
                               check_conclusion=result.stdout.strip())
                    combined = subprocess.run(
                        ["bash", "-eu", "-c", combine + '\nprintf "%s" "$combined"'],
                        env=env, capture_output=True, text=True, check=True,
                    )
                    self.assertEqual(combined.stdout, expected)
        for check_conclusion in ("none", "unknown"):
            env = dict(os.environ, state="success", status_count="1",
                       check_conclusion=check_conclusion)
            combined = subprocess.run(
                ["bash", "-eu", "-c", combine + '\nprintf "%s" "$combined"'],
                env=env, capture_output=True, text=True, check=True,
            )
            self.assertEqual(combined.stdout, "pending")
        for state, expected in (("unknown", "pending"), ("error", "failure")):
            env = dict(os.environ, state=state, status_count="0",
                       check_conclusion="success")
            combined = subprocess.run(
                ["bash", "-eu", "-c", combine + '\nprintf "%s" "$combined"'],
                env=env, capture_output=True, text=True, check=True,
            )
            self.assertEqual(combined.stdout, expected)

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
