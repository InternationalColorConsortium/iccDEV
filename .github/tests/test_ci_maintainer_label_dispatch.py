#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# https://color.org
#
# Intent: Verify maintainer-label PR validation dispatch boundaries.
###############################################################

import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]


def load_workflow(name):
    return yaml.safe_load(
        (ROOT / ".github/workflows" / name).read_text(encoding="ascii")
    )


def test_maintainer_label_dispatch_is_trusted_and_one_shot():
    workflow = load_workflow("ci-maintainer-pr-dispatch.yml")
    triggers = workflow.get("on", workflow.get(True))
    assert triggers["pull_request_target"]["types"] == ["labeled"]
    # Independent label events must reach the dispatcher even when several
    # controls are applied before any prior run has started.
    assert "concurrency" not in workflow

    dispatch = workflow["jobs"]["dispatch"]
    assert "ci:run-auto" in dispatch["if"]
    assert "ci:ctest-last-" in dispatch["if"]
    assert dispatch["permissions"] == {
        "actions": "write",
        "pull-requests": "write",
        "statuses": "write",
    }

    step = dispatch["steps"][0]
    run = step["run"]
    assert "admin|maintain|write" in run
    assert 'head_repository" != "$GH_REPOSITORY"' in run
    assert 'head_sha" != "$EVENT_HEAD_SHA"' in run
    assert "gh workflow run ci-pr-action.yml" in run
    assert "gh workflow run update-labels.yml" in run
    assert "--ref master" in run
    assert "head_ref=" not in run
    assert "-f pr_number=\"$PR_NUMBER\"" in run
    assert "-f expected_head_sha=\"$EVENT_HEAD_SHA\"" in run
    assert '-f request_status_id="$request_status_id"' in run
    assert "ci:docker" not in workflow["jobs"]["dispatch"]["if"]
    assert "ci:docker" not in run
    assert "gh workflow run ci-docker.yml" not in run
    assert "--remove-label \"$EVENT_LABEL\"" in run


SHA = "a" * 40


def run_label_dispatch(label, *, labels=None, permission="write", head=SHA,
                       head_repository="example/repo"):
    workflow = load_workflow("ci-maintainer-pr-dispatch.yml")
    run = workflow["jobs"]["dispatch"]["steps"][0]["run"]
    mock = r'''
git() { return 0; }
gh() {
  case "$*" in
    "api repos/example/repo/collaborators/xsscx/permission --jq .permission")
      printf '%s\n' "$ACTOR_PERMISSION" ;;
    "api repos/example/repo/pulls/123")
      printf '%s\n' "$PR_JSON" ;;
    "api --method POST repos/example/repo/statuses/"*)
      printf '%s\n' "$*" >> "$CALL_LOG"
      printf '555\n' ;;
    "workflow run "*|"pr edit "*)
      printf '%s\n' "$*" >> "$CALL_LOG" ;;
    *)
      printf 'Unexpected gh call: %s\n' "$*" >&2
      return 2 ;;
  esac
}
'''
    with tempfile.TemporaryDirectory() as temp:
        log = Path(temp) / "calls"
        env = dict(
            os.environ,
            BASH_ENV="/dev/null",
            GH_TOKEN="test-token",
            GH_REPOSITORY="example/repo",
            PR_NUMBER="123",
            EVENT_ACTOR="xsscx",
            EVENT_HEAD_SHA=SHA,
            EVENT_LABEL=label,
            RUN_URL="https://example.test/run/1",
            ACTOR_PERMISSION=permission,
            CALL_LOG=str(log),
            PR_JSON=json.dumps({
                "state": "open",
                "base": {"ref": "master"},
                "head": {"repo": {"full_name": head_repository}, "sha": head},
                "labels": [{"name": name} for name in (labels or [label])],
            }),
        )
        result = subprocess.run(
            ["bash", "--noprofile", "--norc", "-c", mock + run],
            env=env, capture_output=True, text=True,
        )
        return result, log.read_text().splitlines() if log.exists() else []


@pytest.mark.parametrize(
    ("label", "scope", "ctest_limit"),
    [
        ("ci:run-auto", "auto", None),
        ("ci:full", "full", None),
        ("ci:fast-lane", "fast-lane", "1"),
        ("ci:ctest-last-3", "fast-lane", "3"),
        ("ci:ctest-last-10", "fast-lane", "10"),
        ("ci:no-windows", "fast-lane", "1"),
        ("ci:warnings-fail", "fast-lane", "1"),
    ],
)
def test_each_ci_control_label_dispatches_exact_head(label, scope, ctest_limit):
    result, calls = run_label_dispatch(label)
    assert result.returncode == 0, result.stdout + result.stderr
    summary = next(call for call in calls if "context=PR Summary" in call)
    assert "state=pending" in summary
    refresh = next(call for call in calls if "workflow run update-labels.yml" in call)
    assert "--ref master" in refresh
    assert "-f pr_number=123" in refresh
    assert f"-f expected_head_sha={SHA}" in refresh
    validation = next(call for call in calls if "workflow run ci-pr-action.yml" in call)
    assert "--ref master" in validation
    assert "-f pr_number=123" in validation
    assert f"-f expected_head_sha={SHA}" in validation
    assert "-f request_status_id=555" in validation
    assert f"-f ci_scope={scope}" in validation
    if ctest_limit is not None:
        assert f"-f ctest_recent_limit={ctest_limit}" in validation
        assert "-f include_windows=false" in validation
        assert "-f warning_policy=fail" in validation
    assert calls[-1].endswith("--remove-label " + label)


def test_fast_lane_uses_single_existing_ctest_modifier():
    result, calls = run_label_dispatch(
        "ci:fast-lane", labels=["ci:fast-lane", "ci:ctest-last-4"])
    assert result.returncode == 0, result.stdout + result.stderr
    validation = next(call for call in calls if "workflow run ci-pr-action.yml" in call)
    assert "-f ctest_recent_limit=4" in validation


def test_skip_records_pending_without_success_or_dispatch():
    result, calls = run_label_dispatch("ci:skip")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "no validation dispatched" in result.stdout
    assert any("context=PR Summary" in call and "state=pending" in call
               for call in calls)
    assert any("workflow run update-labels.yml" in call for call in calls)
    assert not any("workflow run ci-pr-action.yml" in call for call in calls)
    assert not any("workflow run ci-docker.yml" in call for call in calls)
    assert not any("state=success" in call for call in calls)


@pytest.mark.parametrize(
    ("permission", "head", "head_repository"),
    [
        ("read", SHA, "example/repo"),
        ("write", "b" * 40, "example/repo"),
        ("write", SHA, "other/repo"),
    ],
)
def test_untrusted_or_stale_label_is_rejected_without_mutation(
    permission, head, head_repository,
):
    result, calls = run_label_dispatch(
        "ci:full", permission=permission, head=head,
        head_repository=head_repository,
    )
    assert result.returncode != 0
    assert calls == []


def test_codeql_alias_is_managed_and_maintainer_gated():
    labels = yaml.safe_load((ROOT / ".github/labels.yml").read_text())
    assert "ci:codeql" in {label["name"] for label in labels}
    workflow = load_workflow("ci-codeql-security.yml")
    analyze = workflow["jobs"]["analyze"]
    assert "codeql-ready" in analyze["if"]
    assert "ci:codeql" in analyze["if"]
    guard = next(step for step in analyze["steps"]
                 if step.get("name") == "Verify maintainer-applied CodeQL label")
    assert "admin|maintain|write" in guard["run"]
    mock = r'''
git() { return 0; }
gh() {
  if [[ "$*" == "api repos/example/repo/collaborators/xsscx/permission --jq .permission" ]]; then
    printf '%s\n' "$ACTOR_PERMISSION"
  else
    return 2
  fi
}
'''
    for permission, expected in (("write", 0), ("read", 1)):
        env = dict(os.environ, BASH_ENV="/dev/null", GH_TOKEN="test-token",
                   GH_REPOSITORY="example/repo", LABEL_ACTOR="xsscx",
                   ACTOR_PERMISSION=permission)
        result = subprocess.run(
            ["bash", "--noprofile", "--norc", "-c", mock + guard["run"]],
            env=env, capture_output=True, text=True,
        )
        assert result.returncode == expected, result.stdout + result.stderr


def test_dispatch_pr_number_resolves_the_exact_head_for_every_scope():
    workflow = load_workflow("ci-pr-action.yml")
    detect = workflow["jobs"]["detect-src"]
    steps = detect["steps"]
    resolve = next(
        step for step in steps if step.get("name") == "Resolve workflow-dispatch PR"
    )
    assert resolve["if"] == (
        "github.event_name == 'workflow_dispatch' && inputs.pr_number != ''"
    )
    triggers = workflow.get("on", workflow.get(True))
    assert (
        triggers["workflow_dispatch"]["inputs"]["expected_head_sha"]["type"]
        == "string"
    )
    assert resolve["env"]["EXPECTED_HEAD_SHA"] == "${{ inputs.expected_head_sha }}"
    assert 'PR #{0} @ {1}' in workflow["run-name"]
    assert 'if [[ ! "$EXPECTED_HEAD_SHA" =~ ^[0-9a-fA-F]{40}$ ]]' in resolve["run"]
    assert 'PR head changed after dispatch request' in resolve["run"]
    diffcheck = next(step for step in steps if step.get("id") == "diffcheck")
    assert re.search(
        r'if \[ -n "\$DISPATCH_PR_NUMBER" \]; then\s+'
        r'resolve_dispatch_pr "\$DISPATCH_PR_NUMBER"',
        diffcheck["run"],
    )

    for job in workflow["jobs"].values():
        for step in job.get("steps", []):
            if step.get("name", "").startswith("Checkout resolved workflow-dispatch base"):
                assert step["if"] == (
                    "github.event_name == 'workflow_dispatch' && inputs.pr_number != ''"
                )
