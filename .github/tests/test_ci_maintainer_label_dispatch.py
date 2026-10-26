#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# https://color.org
#
# Intent: Verify maintainer-label PR validation dispatch boundaries.
###############################################################

import re
from pathlib import Path

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

    dispatch = workflow["jobs"]["dispatch"]
    assert "github.event.label.name == 'ci:run-auto'" in dispatch["if"]
    assert dispatch["permissions"] == {
        "actions": "write",
        "pull-requests": "write",
    }

    step = dispatch["steps"][0]
    run = step["run"]
    assert "admin|maintain|write" in run
    assert 'head_repository" != "$GH_REPOSITORY"' in run
    assert 'head_sha" != "$EVENT_HEAD_SHA"' in run
    assert "gh workflow run ci-pr-action.yml" in run
    assert "--ref master" in run
    assert "head_ref=" not in run
    assert "-f pr_number=\"$PR_NUMBER\"" in run
    assert "-f expected_head_sha=\"$EVENT_HEAD_SHA\"" in run
    assert "-f ci_scope=auto" in run
    assert "--remove-label ci:run-auto" in run


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
