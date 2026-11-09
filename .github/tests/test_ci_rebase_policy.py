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
    def test_current_head_validation_publishes_actual_commit_status(self):
        workflow = load_workflow("ci-pr-action.yml")
        self.assertTrue(workflow["concurrency"]["cancel-in-progress"])
        self.assertTrue(workflow["concurrency"]["group"].endswith(
            "${{ inputs.request_status_id || 'direct' }}"))
        job = workflow["jobs"]["finalize"]
        self.assertEqual(job["permissions"]["statuses"], "write")
        self.assertEqual(job["permissions"]["actions"], "read")
        step = next(step for step in job["steps"]
                    if step.get("name") == "Publish current-head validation status")
        self.assertIn("always()", step["if"])
        self.assertIn("github.event_name == 'workflow_dispatch'", step["if"])
        self.assertNotIn("github.event.action == 'synchronize'", step["if"])
        self.assertIn("inputs.pr_number == ''", step["if"])
        self.assertIn("needs.detect-src.outputs.target_sha != ''", step["if"])
        self.assertEqual(step["env"]["TARGET_SHA"],
                         "${{ needs.detect-src.outputs.target_sha || github.event.pull_request.head.sha || github.sha }}")
        self.assertNotIn("GH_EVENT_NAME", step["env"])
        self.assertEqual(step["env"]["CURRENT_RUN_ID"], "${{ github.run_id }}")
        self.assertEqual(step["env"]["REQUEST_STATUS_ID"], "${{ inputs.request_status_id || '' }}")
        dispatcher = load_workflow("ci-maintainer-pr-dispatch.yml")["jobs"]["dispatch"]
        self.assertFalse(dispatcher["concurrency"]["cancel-in-progress"])
        self.assertFalse(job["concurrency"]["cancel-in-progress"])
        self.assertEqual(dispatcher["concurrency"]["queue"], "max")
        self.assertEqual(job["concurrency"]["queue"], "max")
        self.assertIn("pr-summary-", dispatcher["concurrency"]["group"])
        self.assertIn("pr-summary-", job["concurrency"]["group"])
        mock = r'''
gh() {
  if [[ "$1" == api && "$2" == repos/example/repo/commits/*/status ]]; then
    if [[ "${QUERY_STATUS:-0}" != 0 ]]; then return 1; fi
    printf '%s\n' '{"statuses":[]}'
  else
    printf '%s\n' "$@"
  fi
}
'''
        for result in ("success", "failure", "cancelled"):
            with self.subTest(result=result), tempfile.TemporaryDirectory() as temp:
                env = dict(os.environ, HOME=temp, BASH_ENV="/dev/null",
                           GH_REPO="example/repo", TARGET_SHA="a" * 40,
                           GH_EVENT_NAME="workflow_dispatch",
                           CURRENT_RUN_ID="42", REQUEST_STATUS_ID="",
                           VALIDATION_RESULT=result,
                           RUN_URL="https://github.com/example/repo/actions/runs/42")
                output = subprocess.run(
                    ["bash", "-eu", "-c", mock + step["run"]],
                    env=env, capture_output=True, text=True, check=True,
                ).stdout.splitlines()
                self.assertIn("repos/example/repo/statuses/" + "a" * 40, output)
                self.assertIn("context=PR Summary", output)
                self.assertIn("state=" + ("success" if result == "success"
                                         else "failure"), output)
                self.assertIn("description=Manual validation: " + result, output)
        with tempfile.TemporaryDirectory() as temp:
            env = dict(os.environ, HOME=temp, GH_REPO="example/repo",
                       TARGET_SHA="a" * 40, GH_EVENT_NAME="workflow_dispatch",
                       CURRENT_RUN_ID="42", REQUEST_STATUS_ID="",
                       VALIDATION_RESULT="success",
                       RUN_URL="https://github.com/example/repo/actions/runs/42")
            failed = subprocess.run(
                ["bash", "-eu", "-c", "gh() { return 1; }; " + step["run"]],
                env=env, capture_output=True, text=True,
            )
            self.assertNotEqual(failed.returncode, 0)
            env["TARGET_SHA"] = "invalid-head"
            invalid = subprocess.run(
                ["bash", "-eu", "-c", mock + step["run"]],
                env=env, capture_output=True, text=True,
            )
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn("not an exact commit SHA", invalid.stderr)

    def test_stale_finalizer_cannot_replace_newer_validation_request(self):
        workflow = load_workflow("ci-pr-action.yml")
        step = next(step for step in workflow["jobs"]["finalize"]["steps"]
                    if step.get("name") == "Publish current-head validation status")
        mock = r'''
gh() {
  if [[ "$1" == api && "$2" == repos/example/repo/commits/*/status ]]; then
    if [[ "${QUERY_STATUS:-0}" != 0 ]]; then return 1; fi
    printf '%s\n' "$LATEST_STATUS"
  elif [[ "$1" == api && "$2" == repos/example/repo/actions/runs/42 ]]; then
    printf '%s\n' "$RUN_CREATED_AT"
  elif [[ "$1" == api && "$2" == --method ]]; then
    printf '%s\n' "$*" >> "$WRITE_LOG"
  else
    return 2
  fi
}
'''
        run_url = "https://github.com/example/repo/actions/runs/42"
        request_url = "https://github.com/example/repo/actions/runs/41"
        newer_url = "https://github.com/example/repo/actions/runs/43"
        def status(status_id, state, url, created_at, description="Maintainer label ci:full: validation requested"):
            return json.dumps({"statuses": [{"id": status_id, "context": "PR Summary",
                                            "state": state, "target_url": url,
                                            "created_at": created_at,
                                            "description": description}]})
        cases = (
            # Older label-triggered fast lane must leave a later full or skip
            # request pending, even when it reaches finalization afterward.
            ("newer-full", "101", status(102, "pending", newer_url, "2026-11-08T10:02:00Z"), 0, 0),
            ("newer-skip", "101", status(103, "pending", newer_url, "2026-11-08T10:02:00Z"), 0, 0),
            ("own-request", "101", status(101, "pending", request_url, "2026-11-08T10:00:00Z"), 0, 1),
            ("other-completed", "101", status(101, "success", request_url, "2026-11-08T10:00:00Z"), 0, 0),
            ("own-failed-rerun", "101", status(201, "failure", run_url, "2026-11-08T10:03:00Z",
                                             "Manual validation: failure"), 0, 1),
            ("own-success-rerun", "101", status(202, "success", run_url, "2026-11-08T10:03:00Z",
                                              "Manual validation: success"), 0, 1),
            ("own-cancelled-rerun", "101", status(203, "failure", run_url, "2026-11-08T10:03:00Z",
                                                "Manual validation: cancelled"), 0, 1),
            ("rerun-wrong-description", "101", status(204, "failure", run_url, "2026-11-08T10:03:00Z",
                                                   "Unrelated validation: failure"), 0, 0),
            ("rerun-newer-request", "101", status(205, "pending", newer_url, "2026-11-08T10:04:00Z"), 0, 0),
            ("skip-is-unowned", "101", status(101, "pending", request_url, "2026-11-08T10:00:00Z",
                                               "Maintainer label ci:skip: validation requested"), 0, 0),
            ("wrong-description", "101", status(101, "pending", request_url, "2026-11-08T10:00:00Z",
                                                  "Unrelated request"), 0, 0),
            ("wrong-url", "101", status(101, "pending", "https://example.invalid/run",
                                           "2026-11-08T10:00:00Z"), 0, 0),
            ("missing-request", "101", json.dumps({"statuses": []}), 0, 0),
            ("invalid-request-id", "bad", status(101, "pending", request_url, "2026-11-08T10:00:00Z"), 1, 0),
            # Direct manual runs have no dispatcher token. Later
            # statuses supersede them; timestamp ties fail closed.
            ("direct-newer", "", status(102, "pending", newer_url, "2026-11-08T10:02:00Z"), 0, 0),
            ("direct-tie", "", status(102, "pending", newer_url, "2026-11-08T10:01:00Z"), 0, 0),
            ("direct-older-pending", "", status(101, "pending", request_url, "2026-11-08T10:00:00Z"), 0, 0),
            ("direct-skip", "", status(101, "pending", request_url, "2026-11-08T10:00:00Z",
                                           "Maintainer label ci:skip: validation requested"), 0, 0),
            ("direct-older-terminal", "", status(101, "success", request_url, "2026-11-08T10:00:00Z"), 0, 1),
            ("direct-newer-terminal", "", status(102, "success", newer_url, "2026-11-08T10:02:00Z"), 0, 0),
            ("same-run", "", status(101, "success", run_url, "2026-11-08T10:02:00Z"), 0, 1),
            ("invalid-time", "", status(102, "success", newer_url, "invalid"), 1, 0),
            ("query-failure", "", "", 1, 0),
        )
        for name, request_status_id, latest_status, should_fail, expected_writes in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temp:
                write_log = Path(temp) / "writes"
                env = dict(os.environ, HOME=temp, BASH_ENV="/dev/null",
                           GH_REPO="example/repo", TARGET_SHA="a" * 40,
                           GH_EVENT_NAME="workflow_dispatch", CURRENT_RUN_ID="42",
                           REQUEST_STATUS_ID=request_status_id,
                           RUN_CREATED_AT="2026-11-08T10:01:00Z",
                           VALIDATION_RESULT="success", LATEST_STATUS=latest_status,
                           QUERY_STATUS="1" if name == "query-failure" else "0",
                           WRITE_LOG=str(write_log),
                           RUN_URL=run_url)
                result = subprocess.run(["bash", "-eu", "-c", mock + step["run"]],
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode != 0, bool(should_fail),
                                 result.stdout + result.stderr)
                writes = write_log.read_text().splitlines() if write_log.exists() else []
                self.assertEqual(len(writes), expected_writes, (name, writes))
                if name == "own-failed-rerun":
                    self.assertIn("state=success", writes[0])

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
    "api "*"status"*)
      if [[ -n "${SECOND_STATUS:-}" ]]; then
        if [[ -e "$STATUS_QUERY_LOG" ]]; then
          printf '%s\\n' "$SECOND_STATUS"
          return 0
        fi
        : > "$STATUS_QUERY_LOG"
      fi
      printf '%s\\n' "$LEGACY_STATUS" ;;
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
            ("changed-after-read", ["pending"], "head", "0", True, "0", 0),
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
                    STATUS_QUERY_LOG=str(root / "status-queries"),
                    SECOND_STATUS="pending|1" if name == "changed-after-read" else "",
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
                if name == "changed-after-read":
                    self.assertIn("Status changed during evaluation", (root / "pr_results.txt").read_text())

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
            (None, "missing-summary"),
            ("skipped", "missing-summary"),
            ("neutral", "missing-summary"),
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
                               check_conclusion=result.stdout.strip(),
                               summary_verified="false", require_codeql="false",
                               codeql_conclusion="not-required")
                    combined = subprocess.run(
                        ["bash", "-eu", "-c", combine + '\nprintf "%s" "$combined"'],
                        env=env, capture_output=True, text=True, check=True,
                    )
                    self.assertEqual(combined.stdout,
                                     "pending" if expected == "missing-summary"
                                     else expected)
        for check_conclusion in ("none", "unknown"):
            env = dict(os.environ, state="success", status_count="1",
                       check_conclusion=check_conclusion,
                       summary_verified="false", require_codeql="false",
                       codeql_conclusion="not-required")
            combined = subprocess.run(
                ["bash", "-eu", "-c", combine + '\nprintf "%s" "$combined"'],
                env=env, capture_output=True, text=True, check=True,
            )
            self.assertEqual(combined.stdout, "pending")
        for state, expected in (("unknown", "pending"), ("error", "failure")):
            env = dict(os.environ, state=state, status_count="0",
                       check_conclusion="success", summary_verified="false",
                       require_codeql="false", codeql_conclusion="not-required")
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

    def test_pr_synchronize_selects_bounded_fast_lane(self):
        workflow = load_workflow("ci-pr-action.yml")
        triggers = workflow.get("on", workflow.get(True))
        self.assertIn("synchronize", triggers["pull_request"]["types"])
        detect = workflow["jobs"]["detect-src"]
        self.assertIn("github.event.pull_request.head.repo.full_name == github.repository",
                      detect["if"])
        self.assertIn("inputs.pr_number", workflow["concurrency"]["group"])
        diffcheck = next(step for step in detect["steps"] if step.get("id") == "diffcheck")
        self.assertEqual(diffcheck["env"]["PR_ACTION"], "${{ github.event.action || '' }}")
        run = diffcheck["run"]
        self.assertIn('"$PR_ACTION" == "synchronize"', run)
        self.assertIn('"$ci_scope" == "auto"', run)
        self.assertIn("apply_fast_lane_defaults", run)
        self.assertIn('"$GH_REF_NAME" != "master"', run)
        self.assertIn('"$EXPECTED_HEAD_SHA_INPUT" != "$DISPATCH_HEAD_SHA"', run)
        self.assertIn('elif ! validate_fast_lane_actor "$GH_ACTOR"', run)

        route = "is_trusted_maintainer() {" + run.split(
            "is_trusted_maintainer() {", 1
        )[1].split("# Validate that both SHAs", 1)[0]
        harness = (
            "set -euo pipefail\n"
            "gh() { printf 'write\\n'; }\n"
            "ci_scope=\"$CI_SCOPE\"\n"
            "target_repository=\"$PR_HEAD_REPO\"\n"
            "head_sha=\"$PR_HEAD_SHA\"\n"
            "base_sha=\"$PR_BASE_SHA\"\n"
            "pr_number=\"\"\n"
            "fast_lane=false\ninclude_windows=true\nctest_recent_limit=0\n"
            "ctest_mode=full\nwarning_policy=fail\n"
            + route
            + "\nprintf '%s|%s|%s|%s|%s' \"$ci_scope\" \"$fast_lane\" "
              "\"$include_windows\" \"$ctest_recent_limit\" \"$warning_policy\"\n"
        )
        base_env = dict(
            os.environ,
            GH_TOKEN="mock",
            GH_REPOSITORY="example/repo",
            PR_HEAD_REPO="example/repo",
            PR_HEAD_SHA="a" * 40,
            PR_BASE_SHA="b" * 40,
            DISPATCH_PR_NUMBER="",
            DISPATCH_BASE_REF="master",
            DISPATCH_BASE_SHA="b" * 40,
            DISPATCH_HEAD_SHA="a" * 40,
            DISPATCH_TARGET_REPOSITORY="example/repo",
            EXPECTED_HEAD_SHA_INPUT="",
            GH_REF_NAME="master",
            CTEST_RECENT_LIMIT_INPUT="1",
            INCLUDE_WINDOWS_INPUT="false",
            WARNING_POLICY_INPUT="fail",
        )
        for name, changes, expected_status in (
            ("synchronize", dict(CI_SCOPE="auto", GH_EVENT_NAME="pull_request",
                                 PR_ACTION="synchronize", GH_ACTOR="contributor"), 0),
            ("trusted-bot", dict(CI_SCOPE="fast-lane",
                                 GH_EVENT_NAME="workflow_dispatch", PR_ACTION="",
                                 GH_ACTOR="github-actions[bot]", DISPATCH_PR_NUMBER="12",
                                 EXPECTED_HEAD_SHA_INPUT="a" * 40), 0),
            ("bot-wrong-head", dict(CI_SCOPE="fast-lane",
                                   GH_EVENT_NAME="workflow_dispatch", PR_ACTION="",
                                   GH_ACTOR="github-actions[bot]", DISPATCH_PR_NUMBER="12",
                                   EXPECTED_HEAD_SHA_INPUT="c" * 40), 1),
            ("bot-wrong-ref", dict(CI_SCOPE="fast-lane",
                                  GH_EVENT_NAME="workflow_dispatch", PR_ACTION="",
                                  GH_ACTOR="github-actions[bot]", DISPATCH_PR_NUMBER="12",
                                  EXPECTED_HEAD_SHA_INPUT="a" * 40,
                                  GH_REF_NAME="branch"), 1),
            ("manual-untrusted", dict(CI_SCOPE="fast-lane",
                                      GH_EVENT_NAME="workflow_dispatch", PR_ACTION="",
                                      GH_ACTOR="contributor"), 1),
            ("fork-synchronize", dict(CI_SCOPE="auto",
                                      GH_EVENT_NAME="pull_request", PR_ACTION="synchronize",
                                      GH_ACTOR="contributor",
                                      PR_HEAD_REPO="other/repo"), 1),
        ):
            with self.subTest(name=name):
                env = dict(base_env, **changes)
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c", harness],
                    env=env, capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, expected_status,
                                 result.stdout + result.stderr)
                if expected_status == 0:
                    self.assertEqual(result.stdout,
                                     "fast-lane|true|false|1|fail")

    def test_fast_lane_runs_release_sanitizers_without_full_native_matrix(self):
        jobs = load_workflow("ci-pr-action.yml")["jobs"]
        self.assertIn("run_native_matrix == 'true'", jobs["gcc15-lto-ci"]["if"])
        tool = jobs["tool-tests"]
        self.assertIn("fast_lane == 'true' && 'Release'", tool["with"]["tool_build_type"])
        self.assertIn("fast_lane == 'true' && 'sanitizers'", tool["with"]["instrumentation"])
        for name in ("mcp-server", "macos-release-lto", "macos-debug",
                     "apple-xcode-ctest"):
            with self.subTest(name=name):
                self.assertIn("fast_lane != 'true'", jobs[name]["if"])

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
