#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# https://color.org
#
# Intent: Verify trusted CI-completion label refresh and PR-head status proof.
###############################################################

import json
import os
from pathlib import Path
import subprocess
import tempfile

import yaml


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/update-labels.yml"


def labeler_step():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="ascii"))
    return next(
        step for step in workflow["jobs"]["label-status"]["steps"]
        if step.get("name") == "Evaluate and label PRs"
    )


def pr_list_step():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="ascii"))
    return next(
        step for step in workflow["jobs"]["label-status"]["steps"]
        if step.get("name") == "Build PR list"
    )


def test_completion_refresh_uses_trusted_metadata_only():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="ascii"))
    triggers = workflow.get("on", workflow.get(True))
    assert triggers["workflow_run"]["types"] == ["completed"]
    assert "labeled" in triggers["pull_request_target"]["types"]
    assert set(triggers["workflow_run"]["workflows"]) == {
        "ci-pr-action", "ci-codeql-security"
    }
    job = workflow["jobs"]["label-status"]
    assert "workflow_run.head_repository.full_name == github.repository" in job["if"]
    assert "github.event.label.name == 'ci:codeql'" in job["if"]
    assert "github.event.label.name == 'codeql-ready'" in job["if"]
    assert job["permissions"]["actions"] == "read"
    assert "github.ref == 'refs/heads/master'" in job["if"]
    assert all("pull_request.head" not in str(step.get("with", ""))
               for step in job["steps"])


MOCK_GH = r'''
gh() {
  case "$1:$2" in
    pr:view)
      if [[ "$*" == *"--json headRefOid,labels"* ]]; then
        printf '%s\n' "$LATEST_PR_DATA"
      else
        printf '%s\n' "$PR_DATA"
      fi
      ;;
    pr:edit)
      printf '%s\n' "$*" >> "$WRITE_LOG"
      ;;
    api:repos/example/repo/commits/head/status)
      status_query_count=0
      if [[ -f "$STATUS_QUERY_COUNT_FILE" ]]; then
        status_query_count=$(cat "$STATUS_QUERY_COUNT_FILE")
      fi
      status_query_count=$(( status_query_count + 1 ))
      printf '%s\n' "$status_query_count" > "$STATUS_QUERY_COUNT_FILE"
      if (( status_query_count > 1 )) && [[ "${STATUS_QUERY_FAIL_AFTER:-0}" == 1 ]]; then
        return 1
      fi
      if (( status_query_count > 1 )) && [[ -n "${STATUS_LINE_AFTER:-}" ]]; then
        printf '%s\n' "$STATUS_LINE_AFTER"
      else
        printf '%s\n' "$STATUS_LINE"
      fi
      ;;
    api:repos/example/repo/actions/runs/42)
      printf '%s\n' "$RUN_LINE"
      ;;
    api:repos/example/repo/actions/runs\?check_suite_id=*)
      suite_id="${2#*check_suite_id=}"
      suite_id="${suite_id%%&*}"
      if [[ "$suite_id" == "$CODEQL_FAIL_SUITE" ]]; then
        return 2
      fi
      jq -c --arg suite "$suite_id" '{workflow_runs: (.[$suite] // [])}' <<< "$CODEQL_RUNS"
      ;;
    api:--paginate)
      if [[ "$*" == *"check_name=CodeQL%20Security%20Analysis"* ]]; then
        if [[ "$*" != *"filter=all"* ]]; then
          printf 'CodeQL check query must include all runs\n' >&2
          return 2
        fi
        printf '%s\n' "$CODEQL_PAGES"
      else
        printf '%s\n' "$CHECK_PAGES"
      fi
      ;;
    *)
      printf 'Unexpected gh call: %s\n' "$*" >&2
      return 2
      ;;
  esac
}
'''


MOCK_PR_LIST_GH = r'''
gh() {
  case "$1:$2" in
    api:repos/example/repo/pulls/7)
      printf '%s\n' "$PR_DATA"
      ;;
    pr:list)
      printf '7\n8\n'
      ;;
    *)
      printf 'Unexpected gh call: %s\n' "$*" >&2
      return 2
      ;;
  esac
}
'''


HEAD_SHA = "a" * 40


def run_pr_list(*, event="workflow_dispatch", pr_number="7",
                expected_head_sha=HEAD_SHA, state="open", base="master",
                repo="example/repo", actual_head=HEAD_SHA):
    with tempfile.TemporaryDirectory() as temp:
        scratch = Path(temp)
        env = dict(
            os.environ, BASH_ENV="/dev/null", GIT_CONFIG_GLOBAL=str(scratch / "gitconfig"),
            GH_REPO="example/repo", GH_EVENT_NAME=event, EVENT_PR_NUMBER="",
            DISPATCH_PR_NUMBER=pr_number, EXPECTED_HEAD_SHA=expected_head_sha,
            GITHUB_OUTPUT=str(scratch / "output"),
            PR_DATA=json.dumps({
                "state": state, "base": {"ref": base},
                "head": {"sha": actual_head, "repo": {"full_name": repo}},
            }),
        )
        result = subprocess.run(
            ["bash", "--noprofile", "--norc", "-c", MOCK_PR_LIST_GH + pr_list_step()["run"]],
            cwd=scratch, env=env, capture_output=True, text=True,
        )
        output = scratch / "output"
        return result, output.read_text().splitlines() if output.exists() else []


def test_focused_manual_refresh_requires_exact_open_same_repo_master_head():
    result, output = run_pr_list()
    assert result.returncode == 0, result.stdout + result.stderr
    assert output == ["prs=7", "mode=single", f"expected_head_sha={HEAD_SHA}"]

    invalid = (
        {"pr_number": ""}, {"pr_number": "7 8"},
        {"expected_head_sha": ""}, {"expected_head_sha": "bad"},
        {"state": "closed"}, {"base": "develop"},
        {"repo": "other/repo"}, {"actual_head": "b" * 40},
    )
    for options in invalid:
        result, output = run_pr_list(**options)
        assert result.returncode != 0, (options, result.stdout, result.stderr)
        assert output == [], options


def test_manual_refresh_without_inputs_scans_open_prs():
    result, output = run_pr_list(pr_number="", expected_head_sha="")
    assert result.returncode == 0, result.stdout + result.stderr
    assert output == ["prs=7 8", "mode=batch"]


def run_labeler(*, status_line, later_status_line="", fail_later_status=False,
                run_line="ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z|PR #1 @ head",
                checks=None, codeql_checks=None, codeql_runs=None,
                codeql_fail_suite="", labels=None,
                draft=False, merge_state="CLEAN", latest_head="head",
                later_labels=None,
                expected_head_sha=""):
    if checks is None:
        checks = [{"name": "Rebase Safety", "status": "completed",
                   "conclusion": "success"}]
    if codeql_checks is None:
        codeql_checks = []
    codeql_checks = [
        {"id": check.get("id", index),
         "app": {"slug": "github-actions"},
         "check_suite": {"id": check.get("id", index)}, **check}
        for index, check in enumerate(codeql_checks, 1)
    ]
    if codeql_runs is None:
        codeql_runs = {
            str(check["check_suite"]["id"]): [{
                "check_suite_id": check["check_suite"]["id"],
                "path": ".github/workflows/ci-codeql-security.yml",
                "head_sha": "head",
            }] for check in codeql_checks
        }
    if labels is None:
        labels = ["pending"]
    if later_labels is None:
        later_labels = labels
    # Existing cases vary a single field in the run metadata; supply the
    # matching PR/head title unless a case explicitly tests that field.
    if run_line.count("|") == 7:
        run_line += "|PR #1 @ head"
    with tempfile.TemporaryDirectory() as temp:
        scratch = Path(temp)
        env = dict(
            os.environ, BASH_ENV="/dev/null", GIT_CONFIG_GLOBAL=str(scratch / "gitconfig"),
            GH_REPO="example/repo", PR_LIST="1", GITHUB_OUTPUT=str(scratch / "output"),
            EXPECTED_HEAD_SHA=expected_head_sha,
            GITHUB_WORKSPACE=temp, WRITE_LOG=str(scratch / "writes"),
            STATUS_LINE=status_line, STATUS_LINE_AFTER=later_status_line,
            STATUS_QUERY_FAIL_AFTER="1" if fail_later_status else "0",
            STATUS_QUERY_COUNT_FILE=str(scratch / "status-query-count"),
            RUN_LINE=run_line,
            LATEST_PR_DATA=json.dumps({
                "headRefOid": latest_head,
                "labels": [{"name": label} for label in later_labels],
            }),
            CHECK_PAGES=json.dumps([{"check_runs": checks}]),
            CODEQL_PAGES=json.dumps([{"check_runs": codeql_checks}]),
            CODEQL_RUNS=json.dumps(codeql_runs), CODEQL_FAIL_SUITE=codeql_fail_suite,
            PR_DATA=json.dumps({
                "isCrossRepository": False, "headRefOid": "head",
                "isDraft": draft, "mergeStateStatus": merge_state,
                "labels": [{"name": label} for label in labels],
            }),
        )
        result = subprocess.run(
            ["bash", "--noprofile", "--norc", "-c", MOCK_GH + labeler_step()["run"]],
            cwd=scratch, env=env, capture_output=True, text=True,
        )
        writes = scratch / "writes"
        return result, writes.read_text().splitlines() if writes.exists() else []


TRUSTED_STATUS = (
    "success|1|success||https://github.com/example/repo/actions/runs/42|"
    "Manual validation: success|2026-11-08T17:03:57Z|101"
)


def test_trusted_master_ref_dispatch_can_mark_current_head_merge_ready():
    result, writes = run_labeler(status_line=TRUSTED_STATUS)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(writes) == 1
    assert "--add-label passed,Merge Ready" in writes[0]
    assert "--remove-label failed,pending" in writes[0]


def test_status_provenance_and_current_checks_fail_closed():
    cases = (
        ("wrong-creator", TRUSTED_STATUS.replace("success||https", "success|attacker|https"),
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("wrong-repository", TRUSTED_STATUS.replace("example/repo", "other/repo"),
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("wrong-run", TRUSTED_STATUS,
         "ci-codeql-security|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("unfinished-run", TRUSTED_STATUS,
         "ci-pr-action|in_progress||workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("untrusted-ref", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|feature|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("wrong-workflow-path", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/other.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("not-dispatch", TRUSTED_STATUS,
         "ci-pr-action|completed|success|pull_request|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("wrong-pr-title", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z|PR #2 @ head", None, "pending"),
        ("wrong-head-title", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z|PR #1 @ older-head", None, "pending"),
        ("missing-title", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z|", None, "pending"),
        ("after-completion", TRUSTED_STATUS.replace("17:03:57Z", "17:05:00Z"),
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("wrong-description", TRUSTED_STATUS.replace("Manual validation", "Forged validation"),
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("pending-status", TRUSTED_STATUS.replace("success|1", "pending|1"),
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z", None, "pending"),
        ("failed-check", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z",
         [{"name": "Rebase Safety", "status": "completed", "conclusion": "failure"}],
         "failed"),
        ("pending-check", TRUSTED_STATUS,
         "ci-pr-action|completed|success|workflow_dispatch|master|.github/workflows/ci-pr-action.yml|2026-11-08T17:03:11Z|2026-11-08T17:04:00Z",
         [{"name": "Rebase Safety", "status": "in_progress", "conclusion": None}],
         "pending"),
    )
    for name, status_line, run_line, checks, expected in cases:
        result, writes = run_labeler(status_line=status_line, run_line=run_line,
                                     checks=checks)
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        if expected == "pending":
            assert writes == [], (name, writes)  # Already pending; no API write.
        else:
            assert len(writes) == 1, (name, writes)
            assert f"--add-label {expected}" in writes[0], (name, writes)
            assert "Merge Ready" not in writes[0].split("--add-label", 1)[1].split("--remove-label", 1)[0]


def test_merge_ready_requires_clean_nondraft_current_head():
    for options in ({"draft": True}, {"merge_state": "DIRTY"}):
        result, writes = run_labeler(status_line=TRUSTED_STATUS, **options)
        assert result.returncode == 0, result.stdout + result.stderr
        assert len(writes) == 1
        assert "--add-label passed" in writes[0]
        assert "--add-label passed,Merge Ready" not in writes[0]

    result, writes = run_labeler(status_line=TRUSTED_STATUS, latest_head="new-head")
    assert result.returncode == 0, result.stdout + result.stderr
    assert writes == []


def test_focused_refresh_does_not_update_a_head_changed_after_pr_selection():
    result, writes = run_labeler(
        status_line=TRUSTED_STATUS, expected_head_sha=HEAD_SHA,
        labels=["passed", "Merge Ready"],
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert writes == []


def test_new_pending_status_clears_stale_ready_for_skip_and_ordinary_ci_labels():
    pending_status = "pending|1|pending|||||102"
    for control_label in ("ci:skip", "ci:full"):
        result, writes = run_labeler(
            status_line=pending_status,
            labels=["passed", "Merge Ready", control_label],
        )
        assert result.returncode == 0, (control_label, result.stdout, result.stderr)
        assert len(writes) == 1, (control_label, writes)
        assert "--add-label pending" in writes[0]
        assert "--remove-label passed,failed,Merge Ready" in writes[0]


def test_status_change_during_batch_or_focused_evaluation_skips_stale_labels():
    pending_status = "pending|1|pending|||||102"
    same_second_new_request = TRUSTED_STATUS.rsplit("|", 1)[0] + "|103"
    for expected_head_sha in ("", "head"):
        for later_status in (pending_status, same_second_new_request):
            result, writes = run_labeler(
                status_line=TRUSTED_STATUS, later_status_line=later_status,
                expected_head_sha=expected_head_sha,
            )
            assert result.returncode == 0, result.stdout + result.stderr
            assert writes == []

        result, writes = run_labeler(
            status_line=pending_status, later_status_line=TRUSTED_STATUS,
            labels=["passed", "Merge Ready"], expected_head_sha=expected_head_sha,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        assert writes == []

        result, writes = run_labeler(
            status_line=TRUSTED_STATUS, fail_later_status=True,
            expected_head_sha=expected_head_sha,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        assert writes == []


def test_codeql_requirement_change_during_evaluation_skips_stale_labels():
    for expected_head_sha in ("", "head"):
        for control_label in ("ci:codeql", "codeql-ready"):
            result, writes = run_labeler(
                status_line=TRUSTED_STATUS, labels=["pending"],
                later_labels=["pending", control_label],
                expected_head_sha=expected_head_sha,
            )
            assert result.returncode == 0, (control_label, result.stdout, result.stderr)
            assert writes == [], (control_label, writes)

        result, writes = run_labeler(
            status_line=TRUSTED_STATUS,
            labels=["pending", "ci:codeql"], later_labels=["pending"],
            codeql_checks=[{
                "name": "CodeQL Security Analysis", "status": "completed",
                "conclusion": "success",
            }],
            expected_head_sha=expected_head_sha,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        assert writes == []


def test_codeql_label_gates_merge_ready_until_current_head_check_succeeds():
    for label in ("ci:codeql", "codeql-ready"):
        for conclusion, expected in ((None, "pending"), ("pending", "pending"),
                                     ("failure", "failed"), ("success", "passed")):
            codeql_checks = [] if conclusion is None else [{
                "name": "CodeQL Security Analysis",
                "status": "in_progress" if conclusion == "pending" else "completed",
                "conclusion": None if conclusion == "pending" else conclusion,
            }]
            result, writes = run_labeler(
                status_line=TRUSTED_STATUS, labels=["pending", label],
                codeql_checks=codeql_checks,
            )
            assert result.returncode == 0, (label, conclusion, result.stderr)
            if expected == "pending":
                assert writes == [], (label, conclusion, writes)
            elif expected == "failed":
                assert len(writes) == 1
                assert "--add-label failed" in writes[0]
            else:
                assert len(writes) == 1
                assert "--add-label passed,Merge Ready" in writes[0]


def test_codeql_skipped_label_runs_do_not_hide_latest_analysis():
    def check(run_id, status, conclusion):
        return {
            "id": run_id, "name": "CodeQL Security Analysis",
            "status": status, "conclusion": conclusion,
        }

    cases = (
        ([check(2, "completed", "skipped"), check(1, "completed", "success")],
         "passed"),
        ([check(2, "completed", "skipped"), check(1, "completed", "failure")],
         "failed"),
        ([check(2, "in_progress", None), check(1, "completed", "success")],
         "pending"),
        ([check(2, "completed", "failure"), check(1, "completed", "success")],
         "failed"),
        ([check(2, "completed", "success"), check(1, "completed", "failure")],
         "passed"),
        ([check(1, "completed", "skipped")], "pending"),
    )
    for codeql_checks, expected in cases:
        result, writes = run_labeler(
            status_line=TRUSTED_STATUS, labels=["pending", "ci:codeql"],
            codeql_checks=codeql_checks,
        )
        assert result.returncode == 0, (codeql_checks, result.stderr)
        if expected == "pending":
            assert writes == [], (codeql_checks, writes)
        else:
            assert len(writes) == 1, (codeql_checks, writes)
            assert f"--add-label {expected}" in writes[0]


def test_codeql_same_named_checks_require_verified_workflow_and_head():
    def check(run_id, conclusion):
        return {
            "id": run_id, "name": "CodeQL Security Analysis",
            "status": "completed", "conclusion": conclusion,
        }

    def workflow_run(suite_id, *, path=".github/workflows/ci-codeql-security.yml",
                     head="head"):
        return {"check_suite_id": suite_id, "path": path, "head_sha": head}

    real_failure = [check(2, "success"), check(1, "failure")]
    real_success = [check(2, "failure"), check(1, "success")]
    cases = (
        (real_failure, {"2": [workflow_run(2, path=".github/workflows/other.yml")],
                        "1": [workflow_run(1)]}, "", "failed"),
        (real_success, {"2": [workflow_run(2, path=".github/workflows/other.yml")],
                        "1": [workflow_run(1)]}, "", "passed"),
        ([check(1, "success")],
         {"1": [workflow_run(1, path=".github/workflows/other.yml")]},
         "", "pending"),
        ([check(1, "success")], {"1": [workflow_run(1, head="other-head")]},
         "", "pending"),
        ([check(1, "success")], {"1": [workflow_run(999)]},
         "", "pending"),
        ([check(1, "success")], {"1": [workflow_run(1)]},
         "1", "pending"),
    )
    for codeql_checks, codeql_runs, failing_suite, expected in cases:
        result, writes = run_labeler(
            status_line=TRUSTED_STATUS, labels=["pending", "ci:codeql"],
            codeql_checks=codeql_checks, codeql_runs=codeql_runs,
            codeql_fail_suite=failing_suite,
        )
        assert result.returncode == 0, (codeql_checks, codeql_runs, result.stderr)
        if expected == "pending":
            assert writes == [], (codeql_checks, codeql_runs, writes)
        else:
            assert len(writes) == 1, (codeql_checks, codeql_runs, writes)
            assert f"--add-label {expected}" in writes[0]
