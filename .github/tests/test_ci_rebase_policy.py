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
            ("direct-older-pending", "", status(101, "pending", request_url, "2026-11-08T10:00:00Z"), 0, 1),
            ("direct-older-pending-failure", "", status(101, "pending", request_url,
                                                        "2026-11-08T10:00:00Z"), 0, 1),
            ("direct-skip", "", status(101, "pending", request_url, "2026-11-08T10:00:00Z",
                                           "Maintainer label ci:skip: validation requested"), 0, 1),
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
                           VALIDATION_RESULT="failure" if name == "direct-older-pending-failure"
                           else "success", LATEST_STATUS=latest_status,
                           QUERY_STATUS="1" if name == "query-failure" else "0",
                           WRITE_LOG=str(write_log),
                           RUN_URL=run_url)
                result = subprocess.run(["bash", "-eu", "-c", mock + step["run"]],
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode != 0, bool(should_fail),
                                 result.stdout + result.stderr)
                writes = write_log.read_text().splitlines() if write_log.exists() else []
                self.assertEqual(len(writes), expected_writes, (name, writes))
                if name == "direct-older-pending-failure":
                    self.assertIn("state=failure", writes[0])
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
    "pr view "*"--json headRefOid,labels") printf '%s\\n' "$LATEST_PR_DATA" ;;
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
                    WRITE_STATUS=write_status,
                    LATEST_PR_DATA=json.dumps({
                        "headRefOid": latest_head,
                        "labels": [{"name": label} for label in labels],
                    }),
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
            "ci-json-roundtrip.yml",
            "ci-mcp-server.yml",
            "ci-preflight-safety.yml",
            "ci-matlab.yml",
            "ci-pr-risk-security-analysis.yml",
        ):
            with self.subTest(workflow=name):
                workflow = load_workflow(name)
                triggers = workflow.get("on", workflow.get(True))
                self.assertIn("workflow_dispatch", triggers)
                if "pull_request" in triggers:
                    self.assertNotIn(
                        "synchronize", triggers["pull_request"]["types"]
                    )

    def test_initial_pr_debug_sanitizers_keep_retired_path_scope(self):
        workflow = load_workflow("ci-pr-action.yml")
        detect = workflow["jobs"]["detect-src"]
        self.assertEqual(
            detect["outputs"]["run_debug_sanitizers"],
            "${{ steps.diffcheck.outputs.run_debug_sanitizers }}",
        )
        self.assertIn("github.event.pull_request.head.repo.full_name == github.repository",
                      detect["if"])
        run = next(step for step in detect["steps"]
                   if step.get("id") == "diffcheck")["run"]
        self.assertIn(
            'write_github_output run_debug_sanitizers "$run_debug_sanitizers"',
            run,
        )
        initial_start = run.index("run_debug_sanitizers=false\n")
        initial_end = run.index("include_windows=true\n", initial_start)
        initial = run[initial_start:initial_end]
        helper_start = run.index("has_changed_file() {\n")
        helper_end = run.index("\n}\n", helper_start) + len("\n}\n")
        helper = run[helper_start:helper_end]
        scope_start = run.index("# Preserve the retired standalone gate's path scope")
        scope_end = run.index("if has_changed_file '^\\.github/(ci|workflows|scripts|actions|codeql-queries)/",
                              scope_start)
        scope = run[scope_start:scope_end]
        selector = (
            "set -euo pipefail\n"
            'changed_files="$CHANGED_FILES"\n'
            + helper + initial + scope
            + '\nprintf "%s" "$run_debug_sanitizers"\n'
        )

        original_paths = (
            "IccProfLib/example.cpp",
            "IccXML/example.xml",
            "IccJSON/example.json",
            "IccConnect/example.h",
            "Tools/example/README.md",
            "Testing/example.icc",
            "Build/Cmake/example.cmake",
            ".github/ci/regression/example.txt",
            ".github/ci/test-data/example.icc",
            ".github/scripts/example.sh",
            ".github/workflows/ci-iccdev-tool-tests.yml",
            ".github/workflows/ci-pr-action.yml",
        )
        for path in original_paths:
            with self.subTest(path=path):
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c", selector],
                    env=dict(os.environ, BASH_ENV="/dev/null",
                             CHANGED_FILES=path, GH_EVENT_NAME="pull_request",
                             PR_ACTION="opened"),
                    capture_output=True, text=True, check=True,
                )
                self.assertEqual(result.stdout, "true")

        for name, event, action, path, expected in (
            ("reopened", "pull_request", "reopened", original_paths[0], "true"),
            ("ready", "pull_request", "ready_for_review", original_paths[0], "true"),
            ("docs-only", "pull_request", "opened", "docs/build.md", "false"),
            ("unrelated-workflow", "pull_request", "opened",
             ".github/workflows/ci-json-roundtrip.yml", "false"),
            ("synchronize", "pull_request", "synchronize", original_paths[0], "false"),
            ("manual", "workflow_dispatch", "", original_paths[0], "false"),
        ):
            with self.subTest(case=name):
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c", selector],
                    env=dict(os.environ, BASH_ENV="/dev/null",
                             CHANGED_FILES=path, GH_EVENT_NAME=event,
                             PR_ACTION=action),
                    capture_output=True, text=True, check=True,
                )
                self.assertEqual(result.stdout, expected, result.stderr)

        # Both conservative exits happen before path filtering and write the
        # initialized flag, so an initial PR still receives this gate.
        self.assertLess(initial_start, run.index('if ! validate_sha "$head_sha"'))
        self.assertLess(run.index('if ! validate_sha "$head_sha"'), scope_start)
        self.assertLess(run.index('if ! git merge-base "origin/${base_ref}" HEAD'),
                        scope_start)

    def test_selected_debug_sanitizer_controls_pr_summary(self):
        workflow = load_workflow("ci-pr-action.yml")
        jobs = workflow["jobs"]
        sanitizer = jobs["sanitizer-regression"]
        self.assertEqual(sanitizer["needs"], ["detect-src"])
        self.assertIn("run_debug_sanitizers == 'true'", sanitizer["if"])
        self.assertEqual(sanitizer["uses"],
                         "./.github/workflows/ci-iccdev-tool-tests.yml")
        tool_concurrency = load_workflow(
            "ci-iccdev-tool-tests.yml")["concurrency"]["group"]
        self.assertIn("inputs.tool_build_type", tool_concurrency)
        self.assertIn("inputs.instrumentation", tool_concurrency)
        self.assertEqual(
            sanitizer["with"],
            {
                "target_sha": "${{ needs.detect-src.outputs.target_sha }}",
                "pr_number": "${{ needs.detect-src.outputs.pr_number }}",
                "tool_build_type": "Debug",
                "ctest_mode": "full",
                "ctest_recent_limit": "0",
                "exclude_extended_pr_tests": True,
                "instrumentation": "sanitizers",
                "warning_policy": "fail",
            },
        )
        finalizer = jobs["finalize"]
        self.assertIn("sanitizer-regression", finalizer["needs"])
        step = next(step for step in finalizer["steps"]
                    if step.get("name") == "PR Details")
        self.assertEqual(
            step["env"]["SANITIZER_RESULT"],
            "${{ needs['sanitizer-regression'].result }}",
        )
        self.assertEqual(
            step["env"]["RUN_DEBUG_SANITIZERS"],
            "${{ needs.detect-src.outputs.run_debug_sanitizers }}",
        )
        run = step["run"]
        start = run.index("# --- normalize upstream job results")
        end = run.index("# --- write header", start)
        result_logic = run[start:end]
        script = "set -euo pipefail\n" + result_logic + '\nprintf "%s" "$job_res"\n'
        for selected, sanitizer_result, expected in (
            ("true", "success", "success"),
            ("true", "skipped", "failure"),
            ("true", "", "failure"),
            ("true", "failure", "failure"),
            ("true", "cancelled", "failure"),
            ("false", "skipped", "success"),
            ("false", "", "success"),
            ("false", "failure", "failure"),
        ):
            with self.subTest(selected=selected, result=sanitizer_result):
                env = dict(os.environ, BASH_ENV="/dev/null",
                           DETECT_RESULT="success", SETUP_RESULT="success",
                           VALIDATE_RESULT="success",
                           RUN_DEBUG_SANITIZERS=selected,
                           SANITIZER_RESULT=sanitizer_result)
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c", script],
                    env=env, capture_output=True, text=True, check=True,
                )
                self.assertEqual(result.stdout, expected, result.stderr)

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


class GitHookIntegrationTests(unittest.TestCase):
    @staticmethod
    def git(repo, *args, env=None, check=True):
        return subprocess.run(
            ["git", "-C", str(repo), *args], env=env,
            capture_output=True, text=True, check=check,
        )

    def make_repo(self, root, *, configure_hooks=True):
        repo = root / "repo"
        repo.mkdir()
        subprocess.run(
            ["git", "init", "-q", "-b", "master", str(repo)],
            capture_output=True, text=True, check=True,
        )
        self.git(repo, "config", "user.name", "Hook Test")
        self.git(repo, "config", "user.email", "hook-test@example.invalid")
        self.git(repo, "config", "commit.gpgsign", "false")
        hooks = repo / ".githooks"
        hooks.mkdir()
        for name in ("pre-commit", "pre-push"):
            shutil.copy2(ROOT / ".githooks" / name, hooks / name)
        script_dir = repo / ".github/scripts"
        script_dir.mkdir(parents=True)
        fake_preflight = script_dir / "preflight-safety-checks.sh"
        fake_preflight.write_text(
            "#!/bin/bash\n"
            "set -euo pipefail\n"
            'printf "%s|%s|%s\\n" "$PWD" "${PREFLIGHT_BASE_REF:-}" "$*" '
            '>> "$HOOK_RECORD"\n'
            'exit "${HOOK_EXIT:-0}"\n',
            encoding="ascii",
        )
        fake_preflight.chmod(0o755)
        shutil.copy2(
            ROOT / ".github/scripts/install-git-hooks.sh",
            script_dir / "install-git-hooks.sh",
        )
        workflow = repo / ".github/workflows/fixture.yml"
        workflow.parent.mkdir(parents=True)
        workflow.write_text("name: baseline\n", encoding="ascii")
        self.git(repo, "add", "--all")
        self.git(repo, "-c", "core.hooksPath=/dev/null",
                 "commit", "-qm", "baseline")
        if configure_hooks:
            self.git(repo, "config", "core.hooksPath", ".githooks")
        return repo, workflow, root / "hook-calls"

    @staticmethod
    def hook_env(record, *, exit_code=0):
        return dict(os.environ, HOOK_RECORD=str(record),
                    HOOK_EXIT=str(exit_code), BASH_ENV="/dev/null")

    def test_pre_commit_checks_index_tree_and_blocks_failed_scan(self):
        with tempfile.TemporaryDirectory() as temp:
            repo, workflow, record = self.make_repo(Path(temp))
            baseline = self.git(repo, "rev-parse", "HEAD").stdout.strip()
            workflow.write_text("name: changed\n", encoding="ascii")
            self.git(repo, "add", ".github/workflows/fixture.yml")
            self.assertEqual(
                self.git(repo, "diff", "--quiet", "--",
                         ".github/workflows/fixture.yml", check=False).returncode,
                0,
            )
            failed = self.git(
                repo, "commit", "-qm", "gated change",
                env=self.hook_env(record, exit_code=1), check=False,
            )
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(self.git(repo, "rev-parse", "HEAD").stdout.strip(),
                             baseline)
            self.assertEqual(record.read_text().splitlines(),
                             [f"{repo}|HEAD|--require-tools"])
            passed = self.git(
                repo, "commit", "-qm", "gated change",
                env=self.hook_env(record), check=False,
            )
            self.assertEqual(passed.returncode, 0,
                             passed.stdout + passed.stderr)
            self.assertNotEqual(self.git(repo, "rev-parse", "HEAD").stdout.strip(),
                                baseline)
            self.assertEqual(record.read_text().splitlines(),
                             [f"{repo}|HEAD|--require-tools"] * 2)

    def test_pre_commit_rejects_divergent_or_whitespace_staged_workflow(self):
        for name, staged, worktree in (
            ("divergent", "name: staged\n", "name: repaired\n"),
            ("whitespace", "name: bad  \n", "name: bad  \n"),
        ):
            with self.subTest(case=name), tempfile.TemporaryDirectory() as temp:
                repo, workflow, record = self.make_repo(Path(temp))
                baseline = self.git(repo, "rev-parse", "HEAD").stdout.strip()
                workflow.write_text(staged, encoding="ascii")
                self.git(repo, "add", ".github/workflows/fixture.yml")
                workflow.write_text(worktree, encoding="ascii")
                result = self.git(
                    repo, "commit", "-qm", "invalid staged workflow",
                    env=self.hook_env(record), check=False,
                )
                self.assertNotEqual(result.returncode, 0, name)
                self.assertFalse(record.exists(), name)
                self.assertEqual(self.git(repo, "rev-parse", "HEAD").stdout.strip(),
                                 baseline)
                self.assertEqual(
                    self.git(repo, "show", ":.github/workflows/fixture.yml").stdout,
                    staged,
                )

    def test_pre_push_uses_outgoing_base_and_blocks_dirty_or_failed_gate(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, workflow, record = self.make_repo(root)
            remote = root / "remote.git"
            subprocess.run(["git", "init", "--bare", "-q", str(remote)],
                           capture_output=True, text=True, check=True)
            self.git(repo, "remote", "add", "origin", str(remote))
            self.git(repo, "checkout", "-qb", "topic")
            self.git(repo, "push", "--no-verify", "origin", "HEAD:refs/heads/topic")
            baseline = self.git(repo, "rev-parse", "HEAD").stdout.strip()
            workflow.write_text("name: outgoing\n", encoding="ascii")
            self.git(repo, "add", ".github/workflows/fixture.yml")
            self.git(repo, "commit", "-qm", "outgoing workflow",
                     env=self.hook_env(record))
            record.unlink()
            outgoing = self.git(repo, "rev-parse", "HEAD").stdout.strip()

            workflow.write_text("name: unstaged repair\n", encoding="ascii")
            for dirty in ("worktree", "index"):
                if dirty == "index":
                    self.git(repo, "add", ".github/workflows/fixture.yml")
                result = self.git(
                    repo, "push", "origin", "HEAD:refs/heads/topic",
                    env=self.hook_env(record), check=False,
                )
                self.assertNotEqual(result.returncode, 0, dirty)
                self.assertFalse(record.exists(), dirty)
            self.git(repo, "restore", "--staged", "--worktree",
                     ".github/workflows/fixture.yml")

            self.git(repo, "checkout", "-q", "master")
            non_head = self.git(
                repo, "push", "origin", "topic:refs/heads/topic",
                env=self.hook_env(record), check=False,
            )
            self.assertNotEqual(non_head.returncode, 0)
            self.assertFalse(record.exists())
            self.git(repo, "checkout", "-q", "topic")

            failed = self.git(
                repo, "push", "origin", "HEAD:refs/heads/topic",
                env=self.hook_env(record, exit_code=1), check=False,
            )
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(record.read_text().splitlines(),
                             [f"{repo}|{baseline}|--require-tools"])
            self.assertEqual(
                self.git(remote, "rev-parse", "refs/heads/topic").stdout.strip(),
                baseline,
            )
            passed = self.git(
                repo, "push", "origin", "HEAD:refs/heads/topic",
                env=self.hook_env(record), check=False,
            )
            self.assertEqual(passed.returncode, 0,
                             passed.stdout + passed.stderr)
            self.assertEqual(
                self.git(remote, "rev-parse", "refs/heads/topic").stdout.strip(),
                outgoing,
            )
            self.assertEqual(record.read_text().splitlines(),
                             [f"{repo}|{baseline}|--require-tools"] * 2)

    def test_pre_push_scans_new_root_branch_and_installer_is_checkout_relative(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, workflow, record = self.make_repo(root, configure_hooks=False)
            outside = root / "outside"
            outside.mkdir()
            installer = repo / ".github/scripts/install-git-hooks.sh"
            for configured in (False, True):
                if configured:
                    self.git(repo, "config", "core.hooksPath", ".githooks")
                for _ in range(2):
                    installed = subprocess.run(
                        ["bash", str(installer)], cwd=outside,
                        capture_output=True, text=True,
                    )
                    self.assertEqual(installed.returncode, 0,
                                     installed.stdout + installed.stderr)
            for hook in ("pre-commit", "pre-push"):
                self.assertEqual(
                    (repo / ".git/hooks" / hook).read_bytes(),
                    (repo / ".githooks" / hook).read_bytes(),
                )
            self.git(repo, "config", "--unset", "core.hooksPath")
            workflow.write_text("name: installed hook\n", encoding="ascii")
            self.git(repo, "add", ".github/workflows/fixture.yml")
            committed = self.git(
                repo, "commit", "-qm", "installed hook works",
                env=self.hook_env(record), check=False,
            )
            self.assertEqual(committed.returncode, 0,
                             committed.stdout + committed.stderr)
            self.assertEqual(record.read_text().splitlines(),
                             [f"{repo}|HEAD|--require-tools"])
            record.unlink()
            source = repo / "source.cpp"
            source.write_text("int main() { return 0; }\n", encoding="ascii")
            self.git(repo, "add", "source.cpp")
            self.git(repo, "commit", "-qm", "latest source-only commit",
                     env=self.hook_env(record))
            self.assertFalse(record.exists())

            remote = root / "empty-remote.git"
            subprocess.run(["git", "init", "--bare", "-q", str(remote)],
                           capture_output=True, text=True, check=True)
            self.git(repo, "remote", "add", "origin", str(remote))
            blocked = self.git(
                repo, "push", "origin", "HEAD:refs/heads/topic",
                env=self.hook_env(record, exit_code=1), check=False,
            )
            self.assertNotEqual(blocked.returncode, 0)
            self.assertEqual(record.read_text().splitlines(),
                             [f"{repo}|{'0' * 40}|--require-tools"])
            self.assertNotEqual(
                self.git(remote, "rev-parse", "--verify", "refs/heads/topic",
                         check=False).returncode,
                0,
            )


class SharedWorkflowPolicyTests(unittest.TestCase):
    @staticmethod
    def write_fixture(path, lines):
        path.parent.mkdir(parents=True, exist_ok=True)
        body = "".join(f"          {line}\n" for line in lines)
        path.write_text(
            "name: Policy fixture\n"
            "'on': workflow_dispatch\n"
            "jobs:\n"
            "  check:\n"
            "    runs-on: ubuntu-latest\n"
            "    steps:\n"
            "      - run: |\n" + body,
            encoding="ascii",
        )

    def test_workflow_risk_only_uses_shared_policy_without_full_preflight(self):
        preflight = ROOT / ".github/scripts/preflight-safety-checks.sh"
        cases = (
            ("literal-newline",
             ('printf "\\n" >> "$GITHUB_STEP_SUMMARY"',), 1),
            ("dynamic-value",
             ('printf "%s\\n" "$value" >> "$GITHUB_STEP_SUMMARY"',), 1),
            ("sanitized-group",
             ('{', '  sanitize_print "$value"', '  printf "\\n"',
              '} >> "$GITHUB_STEP_SUMMARY"  # elements-sanitized'), 0),
        )
        for name, lines, expected_exit in cases:
            with self.subTest(case=name), tempfile.TemporaryDirectory() as temp:
                scan_root = Path(temp)
                self.write_fixture(
                    scan_root / ".github/workflows/fixture.yml", lines,
                )
                result = subprocess.run(
                    ["bash", str(preflight), "--workflow-risk-only"],
                    cwd=scan_root,
                    env=dict(os.environ, BASH_ENV="/dev/null",
                             PREFLIGHT_SCAN_ROOT=str(scan_root),
                             PREFLIGHT_BASE_REF="0" * 40),
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, expected_exit,
                                 result.stdout + result.stderr)
                self.assertIn("workflow files: 1", result.stdout)
                if expected_exit:
                    self.assertIn(
                        "unsanitized GITHUB_STEP_SUMMARY/GITHUB_OUTPUT write",
                        result.stderr,
                    )
                    self.assertIn("unsanitized output writes: 1", result.stdout)
                else:
                    self.assertIn("unsanitized output writes: 0", result.stdout)
                self.assertNotIn("Doxygen documentation warnings", result.stdout)
                self.assertNotIn("CodeQL Actions analysis", result.stdout)

        with tempfile.TemporaryDirectory() as temp:
            scan_root = Path(temp)
            missing = subprocess.run(
                ["bash", str(preflight), "--workflow-risk-only"],
                cwd=scan_root,
                env=dict(os.environ, BASH_ENV="/dev/null",
                         PREFLIGHT_SCAN_ROOT=str(scan_root),
                         PREFLIGHT_BASE_REF="0" * 40),
                capture_output=True, text=True,
            )
            self.assertNotEqual(missing.returncode, 0)
            self.assertIn("No workflows found for the shared workflow policy",
                          missing.stderr)

    def test_hosted_shared_policy_report_and_enforcement_match_local_result(self):
        workflow = load_workflow("ci-pr-risk-security-analysis.yml")
        steps = workflow["jobs"]["workflow-security-audit"]["steps"]
        shared = next(step for step in steps
                      if step.get("name") == "Shared Workflow Policy")
        enforce = next(step for step in steps
                       if step.get("name") == "12. Enforce Blocking Findings")
        executive = next(step for step in steps
                         if step.get("name") == "15. Executive Risk Summary")
        self.assertIn("inputs.fail_on_findings == true", enforce["if"])
        self.assertIn("github.event_name == 'pull_request'", enforce["if"])
        self.assertIn(
            r"^\[FAIL\] Shared workflow policy check failed",
            enforce["run"],
        )
        self.assertIn(
            r"^\[FAIL\] Shared workflow policy check failed",
            executive["run"],
        )
        self.assertIn("--workflow-risk-only", shared["run"])
        mock_git = (
            "git() {\n"
            '  [[ "$#" -eq 4 && "$1" == config && "$2" == --global &&\n'
            '     "$3" == credential.helper && "$4" == "" ]]\n'
            "}\n"
        )
        for name, lines, bad in (
            ("safe", ('{', '  sanitize_print "$value"', '  printf "\\n"',
                      '} >> "$GITHUB_STEP_SUMMARY"  # elements-sanitized'),
             False),
            ("bad", ('printf "\\n" >> "$GITHUB_STEP_SUMMARY"',), True),
            ("legacy-helper", (), False),
        ):
            with self.subTest(case=name), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                scan_root = root / "scan"
                self.write_fixture(
                    scan_root / ".github/workflows/fixture.yml", lines,
                )
                trusted = root / "trusted"
                trusted.mkdir()
                for script_name in ("preflight-safety-checks.sh",
                                    "sanitize-sed.sh"):
                    shutil.copy2(
                        ROOT / ".github/scripts" / script_name,
                        trusted / script_name,
                    )
                if name == "legacy-helper":
                    (trusted / "preflight-safety-checks.sh").write_text(
                        '#!/bin/bash\n'
                        '[ "$#" -eq 1 ] && [ "$1" = --help ] || exit 2\n'
                        'echo "Usage: preflight-safety-checks.sh [--fast-lane]"\n',
                        encoding="ascii",
                    )
                report = root / "risk-report.md"
                summary = root / "step-summary.md"
                env = dict(os.environ, BASH_ENV="/dev/null",
                           TMPDIR=str(root), SCAN_ROOT=str(scan_root),
                           TRUSTED_SCRIPTS=str(trusted),
                           RISK_REPORT_PATH=str(report),
                           RUNNER_TEMP=str(root),
                           GITHUB_STEP_SUMMARY=str(summary),
                           FAIL_ON_FINDINGS="false")

                def run_step(step):
                    return subprocess.run(
                        ["bash", "--noprofile", "--norc", "-c",
                         mock_git + step["run"]],
                        cwd=root, env=env, capture_output=True, text=True,
                    )

                result = run_step(shared)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                report_text = report.read_text(encoding="utf-8")
                marker = (
                    "[FAIL] Shared workflow policy check failed" if bad else
                    "[OK] Shared workflow policy check passed"
                )
                if name == "legacy-helper":
                    marker = "[SKIP] Trusted base lacks shared workflow policy mode"
                self.assertIn(marker, report_text)
                if name != "legacy-helper":
                    self.assertIn("workflow files: 1", report_text)
                if bad:
                    self.assertIn(
                        "unsanitized GITHUB_STEP_SUMMARY/GITHUB_OUTPUT write",
                        report_text,
                    )
                elif name != "legacy-helper":
                    self.assertIn("unsanitized output writes: 0", report_text)

                advisory = run_step(executive)
                self.assertEqual(advisory.returncode, 0,
                                 advisory.stdout + advisory.stderr)
                advisory_text = summary.read_text(encoding="utf-8")
                self.assertIn("| Enforcement enabled | false |", advisory_text)
                self.assertIn(
                    "| Result | [FAIL] |" if bad else "| Result | [PASS] |",
                    advisory_text,
                )
                summary.unlink()
                env["FAIL_ON_FINDINGS"] = "true"
                enforced = run_step(enforce)
                self.assertEqual(enforced.returncode, 1 if bad else 0,
                                 enforced.stdout + enforced.stderr)
                if bad:
                    self.assertIn(
                        "[FAIL] Blocking governance findings: 1",
                        report.read_text(encoding="utf-8"),
                    )
                final = run_step(executive)
                self.assertEqual(final.returncode, 0,
                                 final.stdout + final.stderr)
                final_text = summary.read_text(encoding="utf-8")
                self.assertIn("| Enforcement enabled | true |", final_text)
                self.assertIn(
                    "| Result | [FAIL] |" if bad else "| Result | [PASS] |",
                    final_text,
                )


class CodeQLWorkflowSignalTests(unittest.TestCase):
    @staticmethod
    def sarif(*, finding=False, execution_successful=True):
        run = {
            "tool": {"driver": {"name": "CodeQL", "rules": [
                {"id": "iccdev/demo", "properties": {"problem.severity": "error"}}
            ]}},
            "invocations": [{"executionSuccessful": execution_successful}],
            "results": [],
        }
        if finding:
            run["results"].append({
                "ruleId": "iccdev/demo",
                "level": "error",
                "message": {"text": "Example finding"},
                "locations": [{"physicalLocation": {
                    "artifactLocation": {"uri": "IccProfLib/example.cpp"},
                    "region": {"startLine": 7},
                }}],
            })
        return {"version": "2.1.0", "runs": [run]}

    def test_query_workflow_checks_positive_fixture_before_codeql(self):
        workflow = load_workflow("ci-codeql-query-tests.yml")
        step = next(
            step for step in workflow["jobs"]["query-tests"]["steps"]
            if step.get("name") == "Run query unit tests"
        )
        mock_commands = r'''
git() {
  [[ "$1" == config && "$2" == --global && "$3" == credential.helper ]]
}
codeql() {
  printf '%s\n' "$*" >> "$CODEQL_CALLS"
}
'''
        for case in ("valid", "no-refs", "missing-source", "empty-expected",
                     "blank-expected", "empty-row"):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                fixtures = root / ".github/codeql-queries/test/example"
                fixtures.mkdir(parents=True)
                if case != "no-refs":
                    (fixtures / "Example.qlref").write_text(
                        "query: ../Example.ql\n", encoding="ascii",
                    )
                    expected = {
                        "empty-expected": "",
                        "blank-expected": "\n \t\n",
                        "empty-row": "|  |\n",
                    }.get(case, "| result |\n")
                    (fixtures / "Example.expected").write_text(
                        expected, encoding="ascii",
                    )
                    if case != "missing-source":
                        (fixtures / "sample.cpp").write_text(
                            "int main() { return 0; }\n", encoding="ascii",
                        )
                calls = root / "codeql-calls"
                env = dict(os.environ, BASH_ENV="/dev/null",
                           CODEQL_CALLS=str(calls))
                result = subprocess.run(
                    ["bash", "--noprofile", "--norc", "-c",
                     mock_commands + step["run"]],
                    cwd=root, env=env, capture_output=True, text=True,
                )
                if case == "valid":
                    self.assertEqual(result.returncode, 0,
                                     result.stdout + result.stderr)
                    self.assertIn("CodeQL query fixtures selected: 1",
                                  result.stdout)
                    self.assertIn(
                        "test run .github/codeql-queries/test",
                        calls.read_text(encoding="ascii"),
                    )
                else:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(calls.exists(),
                                     result.stdout + result.stderr)
                    expected_error = {
                        "no-refs": "No CodeQL query fixtures found",
                        "missing-source": "no source file beside it",
                        "empty-expected": "needs an .expected result row",
                        "blank-expected": "needs an .expected result row",
                        "empty-row": "needs an .expected result row",
                    }[case]
                    self.assertIn(expected_error, result.stderr)

    def test_codeql_report_rejects_invalid_inputs_and_reports_real_results(self):
        report_script = ROOT / ".github/scripts/codeql-report.sh"
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            valid = root / "valid.sarif"
            malformed = root / "malformed.sarif"
            incomplete = root / "incomplete.sarif"
            failed = root / "failed.sarif"
            missing = root / "missing.sarif"
            malformed.write_text("{", encoding="ascii")
            incomplete.write_text(
                json.dumps({"version": "2.1.0", "runs": []}), encoding="ascii",
            )
            failed.write_text(
                json.dumps(self.sarif(execution_successful=False)),
                encoding="ascii",
            )
            for name, path, contents, expected_exit in (
                ("empty-valid", valid, self.sarif(), 0),
                ("finding-valid", valid, self.sarif(finding=True), 0),
                ("missing", missing, None, 1),
                ("malformed", malformed, None, 1),
                ("empty-runs", incomplete, None, 1),
                ("failed-invocation", failed, None, 1),
            ):
                with self.subTest(case=name):
                    if contents is not None:
                        path.write_text(json.dumps(contents), encoding="ascii")
                    result = subprocess.run(
                        ["bash", str(report_script), str(path)],
                        capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, expected_exit,
                                     result.stdout + result.stderr)
                    if expected_exit:
                        self.assertIn("ERROR: Failed to parse", result.stderr)
                        self.assertNotIn("[OK] No findings detected", result.stdout)
                    elif name == "empty-valid":
                        self.assertIn("Total findings: 0", result.stdout)
                        self.assertIn("[OK] No findings detected", result.stdout)
                    else:
                        self.assertIn("Total findings: 1", result.stdout)
                        self.assertIn("iccdev/demo", result.stdout)
                        self.assertIn("IccProfLib/example.cpp:7", result.stdout)
                        self.assertNotIn("[OK] No findings detected", result.stdout)

    def test_local_codeql_runner_reports_only_selected_current_sarif(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            scripts = root / ".github/scripts"
            scripts.mkdir(parents=True)
            for name in ("run-codeql-local.sh", "codeql-report.sh"):
                shutil.copy2(ROOT / ".github/scripts" / name, scripts / name)
            queries = root / ".github/codeql-queries"
            queries.mkdir()
            curated = "iccdev-cpp-security-and-quality.qls"
            (queries / curated).write_text("placeholder\n", encoding="ascii")
            db = root / "db"
            db.mkdir()
            results = root / "codeql-results"
            results.mkdir()
            (results / "iccdev-security.sarif").write_text(
                json.dumps(self.sarif(finding=True)), encoding="ascii",
            )
            (results / "iccdev-jsonlib-security.sarif").write_text(
                json.dumps(self.sarif(finding=True)), encoding="ascii",
            )
            (results / "report.txt").write_text(
                "STALE REPORT\n", encoding="ascii",
            )
            bin_dir = root / "bin"
            bin_dir.mkdir()
            fake_codeql = bin_dir / "codeql"
            fake_codeql.write_text(
                "#!/usr/bin/env python3\n"
                "import os, pathlib, sys\n"
                "args = sys.argv[1:]\n"
                "if args == ['version']:\n"
                "    print('CodeQL 2.27.5')\n"
                "elif args[:2] == ['pack', 'install']:\n"
                "    pass\n"
                "elif args[:2] == ['database', 'analyze']:\n"
                "    with open(os.environ['CODEQL_CALLS'], 'a') as log:\n"
                "        log.write(' '.join(args) + '\\n')\n"
                "    out = next(a.split('=', 1)[1] for a in args "
                "if a.startswith('--output='))\n"
                "    if os.environ.get('CODEQL_NO_OUTPUT') != '1':\n"
                "        pathlib.Path(out).write_text(os.environ['CODEQL_SARIF'])\n"
                "else:\n"
                "    raise SystemExit(2)\n",
                encoding="ascii",
            )
            fake_codeql.chmod(0o755)
            calls = root / "codeql-calls"
            env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"],
                       CODEQL_CALLS=str(calls),
                       CODEQL_SARIF=json.dumps(self.sarif()))
            result = subprocess.run(
                ["bash", str(scripts / "run-codeql-local.sh"),
                 "--skip-build", "--db-dir", str(db), "--standard-only"],
                cwd=root, env=env, capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0,
                             result.stdout + result.stderr)
            self.assertIn("[OK] CodeQL analysis complete", result.stdout)
            report = (results / "report.txt").read_text(encoding="utf-8")
            self.assertIn("SARIF files analyzed: 1", report)
            self.assertIn("Total findings: 0", report)
            self.assertNotIn("STALE REPORT", report)
            self.assertNotIn("iccdev/demo", report)
            command = calls.read_text(encoding="utf-8")
            self.assertIn(
                ".github/codeql-queries/" + curated, command,
            )
            self.assertNotIn(
                "codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls",
                command,
            )
            self.assertNotIn("iccdev-security.sarif", result.stdout)
            self.assertNotIn("iccdev-jsonlib-security.sarif", result.stdout)

            # An analyzer that exits zero without writing output must not
            # reuse a previous selected SARIF to produce a clean report.
            env["CODEQL_NO_OUTPUT"] = "1"
            failed = subprocess.run(
                ["bash", str(scripts / "run-codeql-local.sh"),
                 "--skip-build", "--db-dir", str(db), "--standard-only"],
                cwd=root, env=env, capture_output=True, text=True,
            )
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(
                (results / "cpp-security-and-quality.sarif").stat().st_size, 0,
            )
            self.assertNotIn(
                "[OK] No findings detected",
                (results / "report.txt").read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
