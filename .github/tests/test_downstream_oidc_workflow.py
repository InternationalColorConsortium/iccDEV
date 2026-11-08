#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Contracts that keep downstream OIDC selection separate from GitHub releases."""

import re
from pathlib import Path

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/ci-latest-release.yml"
PLANNER_ATTEMPT_GUARD = (
    "needs.downstream-plan.outputs.planner_run_attempt == "
    "format('{0}', github.run_attempt)"
)
PUBLISHERS = {
    "pypi_iccdev": ("pypi_iccdev", "stage_pypi", "target_pypi_iccdev"),
    "pypi_mcp": ("pypi_mcp", "stage_pypi", "target_pypi_mcp"),
    "npm_iccdev": ("npm_iccdev", "stage_npm", "target_npm_iccdev"),
}


def load_jobs():
    return yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))["jobs"]


def steps_by_id(job):
    return {step["id"]: step for step in job["steps"] if "id" in step}


def required_terms(condition):
    """Split conjunction-only publisher conditions to detect alternate paths."""
    expression = condition.strip().removeprefix("${{").removesuffix("}}").strip()
    assert "||" not in expression, expression
    return set(re.split(r"\s*&&\s*", expression))


def test_provenance_planning_has_no_publishing_authority():
    jobs = load_jobs()
    plan = jobs["downstream-plan"]
    assert plan["permissions"] == {"contents": "read"}
    assert "environment" not in plan
    assert "id-token" not in plan["permissions"]

    checkout = plan["steps"][0]
    assert checkout["with"]["fetch-depth"] == 0
    assert checkout["with"]["persist-credentials"] is False
    assert all(
        "gh-action-pypi-publish" not in step.get("uses", "")
        and "npm publish" not in step.get("run", "")
        and "gh release create" not in step.get("run", "")
        for step in plan["steps"]
    )

    gate = steps_by_id(plan)["oidc_gate"]
    assert gate["continue-on-error"] is True
    assert "!cancelled()" in gate["if"]
    verifier = next(step for step in plan["steps"]
                    if step["name"] == "Install downstream provenance verifier")
    assert "inputs.npm_packages == 'iccdev'" in verifier["if"]
    for output in ("pypi_iccdev", "pypi_mcp", "npm_iccdev", "selected_but_blocked"):
        assert plan["outputs"][output] == (
            "${{ steps.oidc_gate.outputs." + output + " }}"
        )
    assert plan["outputs"]["planner_run_attempt"] == (
        "${{ steps.oidc_gate.outputs.planner_run_attempt }}"
    )
    assert "printf 'planner_run_attempt=%s\\n' \"$(sanitize_line \"$GITHUB_RUN_ATTEMPT\")\" >> \"$GITHUB_OUTPUT\"" in gate["run"]


def test_planner_failure_cannot_release_incomplete_producer_outputs():
    jobs = load_jobs()
    release = jobs["release"]
    assert "downstream-plan" in release["needs"]
    assert "!cancelled()" in release["if"]
    assert "needs.downstream-plan.result == 'success'" not in release["if"]
    for producer in set(release["needs"]) - {"downstream-plan"}:
        assert f"needs.{producer}.result == 'success'" in release["if"]
    assert release["permissions"]["id-token"] == "write"
    assert release["permissions"]["attestations"] == "write"
    assert "release" in release["environment"]["name"]

    steps = release["steps"]
    attest = next(i for i, step in enumerate(steps) if step.get("id") == "attest")
    publish = next(i for i, step in enumerate(steps) if step.get("id") == "publish")
    stage_pypi = next(i for i, step in enumerate(steps) if step.get("id") == "stage_pypi")
    assert attest < publish < stage_pypi
    assert "gh release create" in steps[publish]["run"]


@pytest.mark.parametrize("publisher, expected", PUBLISHERS.items())
def test_each_publisher_needs_its_own_gate_and_validated_stage(publisher, expected):
    gate_output, stage_id, target_id = expected
    release = load_jobs()["release"]
    steps = steps_by_id(release)
    publish = steps[publisher]
    assert required_terms(publish["if"]) == {
        "!cancelled()",
        "github.ref == 'refs/heads/master'",
        PLANNER_ATTEMPT_GUARD,
        f"needs.downstream-plan.outputs.{gate_output} == 'true'",
        f"steps.{stage_id}.outcome == 'success'",
        f"steps.{target_id}.outputs.{target_id}_available == 'true'",
    }


@pytest.mark.parametrize("publisher, expected", PUBLISHERS.items())
def test_target_version_is_rechecked_immediately_before_publish(publisher, expected):
    gate_output, stage_id, target_id = expected
    release_steps = load_jobs()["release"]["steps"]
    by_id = steps_by_id({"steps": release_steps})
    target = by_id[target_id]
    ids = [step.get("id") for step in release_steps]
    assert ids.index(stage_id) < ids.index(target_id) < ids.index(publisher)
    assert ids.index(target_id) + 1 == ids.index(publisher)
    assert target["continue-on-error"] is True
    assert "!cancelled()" in target["if"]
    assert f"needs.downstream-plan.outputs.{gate_output} == 'true'" in target["if"]
    assert "--check-target-only" in target["run"]
    assert "--github-output" in target["run"]
    assert "npm publish" not in target["run"]


def test_selected_package_staging_is_independent():
    release = load_jobs()["release"]
    steps = steps_by_id(release)
    pypi = steps["stage_pypi"]
    assert PLANNER_ATTEMPT_GUARD in pypi["if"]
    assert "steps.attest.outcome == 'success'" in pypi["if"]
    assert "steps.publish.outcome == 'success'" in pypi["if"]
    assert "needs.downstream-plan.outputs.pypi_iccdev == 'true'" in pypi["if"]
    assert "needs.downstream-plan.outputs.pypi_mcp == 'true'" in pypi["if"]
    assert pypi["env"]["PYPI_ICCDEV_ALLOWED"] == (
        "${{ needs.downstream-plan.outputs.pypi_iccdev }}"
    )
    assert pypi["env"]["PYPI_MCP_ALLOWED"] == (
        "${{ needs.downstream-plan.outputs.pypi_mcp }}"
    )
    assert 'if [[ "$PYPI_ICCDEV_ALLOWED" == "true" ]]; then' in pypi["run"]
    assert 'if [[ "$PYPI_MCP_ALLOWED" == "true" ]]; then' in pypi["run"]

    node = steps["setup_npm_node"]
    npm = steps["stage_npm"]
    assert "needs.downstream-plan.outputs.npm_iccdev == 'true'" in node["if"]
    assert "needs.downstream-plan.outputs.npm_iccdev == 'true'" in npm["if"]
    assert "steps.setup_npm_node.outcome == 'success'" in npm["if"]
    for step in (node, npm):
        assert PLANNER_ATTEMPT_GUARD in step["if"]
        assert "steps.attest.outcome == 'success'" in step["if"]
        assert "steps.publish.outcome == 'success'" in step["if"]
    for later_step in (
        "target_pypi_mcp",
        "pypi_mcp",
        "setup_npm_node",
        "stage_npm",
        "target_npm_iccdev",
        "npm_iccdev",
    ):
        assert "!cancelled()" in steps[later_step]["if"]


@pytest.mark.parametrize(
    ("smoke_id", "publisher_id", "gate_output"),
    [
        ("verify_public_iccdev", "pypi_iccdev", "pypi_iccdev"),
        ("verify_public_iccdev_mcp", "pypi_mcp", "pypi_mcp"),
        ("verify_public_iccdev_npm", "npm_iccdev", "npm_iccdev"),
    ],
)
def test_public_smoke_is_diagnostic_and_tied_to_actual_publication(
    smoke_id, publisher_id, gate_output
):
    steps = steps_by_id(load_jobs()["release"])
    smoke = steps[smoke_id]
    assert smoke["continue-on-error"] is True
    assert "!cancelled()" in smoke["if"]
    assert f"steps.{publisher_id}.outcome == 'success'" in smoke["if"]
    assert f"needs.downstream-plan.outputs.{gate_output} == 'true'" in smoke["if"]


def test_incomplete_selected_publication_fails_separately_after_github_release():
    jobs = load_jobs()
    rejected = jobs["downstream-selection-rejected"]
    assert rejected["needs"] == "release"
    condition = rejected["if"]
    for required in (
        "!cancelled()",
        "github.event_name == 'workflow_dispatch'",
        "github.ref == 'refs/heads/master'",
        "needs.release.result == 'success'",
    ):
        assert required in condition
    compact = " ".join(condition.split())
    assert (
        "((inputs.pypi_packages == 'iccdev' || inputs.pypi_packages == 'both') "
        "&& needs.release.outputs.pypi_iccdev_publication != 'success')"
    ) in compact
    assert (
        "((inputs.pypi_packages == 'iccdev-mcp' || inputs.pypi_packages == 'both') "
        "&& needs.release.outputs.pypi_mcp_publication != 'success')"
    ) in compact
    assert (
        "(inputs.npm_packages == 'iccdev' "
        "&& needs.release.outputs.npm_iccdev_publication != 'success')"
    ) in compact
    for publisher, _ in PUBLISHERS.items():
        assert jobs["release"]["outputs"][f"{publisher}_publication"] == (
            "${{ steps." + publisher + ".outcome }}"
        )
    assert rejected["permissions"] == {"contents": "read"}
    report = rejected["steps"][-1]["run"]
    assert "::error::A selected downstream package was not published" in report
    assert re.search(r"(?m)^\s*exit 1\s*$", report)


def test_release_summary_identifies_reused_planner_decisions():
    summary = next(step for step in load_jobs()["release"]["steps"]
                   if step["name"] == "Summary Report")
    assert summary["env"]["OIDC_PLAN_ATTEMPT"] == (
        "${{ needs.downstream-plan.outputs.planner_run_attempt }}"
    )
    assert summary["env"]["OIDC_PLAN_CURRENT"] == (
        "${{ " + PLANNER_ATTEMPT_GUARD + " }}"
    )
    assert "current approval: ${OIDC_PLAN_CURRENT:-false}" in summary["run"]
