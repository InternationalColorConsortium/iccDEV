#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Regression tests for workflow action pin ownership."""

import runpy
from pathlib import Path


SCRIPT = (
    Path(__file__).resolve().parents[1]
    / "scripts"
    / "check-workflow-action-pins.py"
)
CHECKER = runpy.run_path(str(SCRIPT))
check_repository = CHECKER["check_repository"]
SHA_A = "a" * 40
SHA_B = "b" * 40
DIGEST = "c" * 64


def write(root: Path, relative: str, content: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="ascii")


def workflow(*action_refs: str) -> str:
    steps = "\n".join(
        f"      - uses: {action_ref}" for action_ref in action_refs
    )
    return (
        "name: action-pin-test\n"
        "on: workflow_dispatch\n"
        "jobs:\n"
        "  test:\n"
        "    runs-on: ubuntu-latest\n"
        "    steps:\n"
        f"{steps}\n"
    )


def test_accepts_consistent_sha_pins_and_non_remote_actions(tmp_path):
    checkout = "actions/checkout@" + SHA_A
    write(
        tmp_path,
        ".github/workflows/first.yml",
        workflow(
            checkout,
            "./.github/workflows/local.yml",
            "docker://example/image@sha256:" + DIGEST,
        ),
    )
    write(
        tmp_path,
        ".github/workflows/second.yml",
        workflow(checkout),
    )

    errors, counts = check_repository(tmp_path)

    assert errors == []
    assert counts == {
        "workflow_files": 2,
        "action_occurrences": 2,
        "action_repositories": 1,
        "docker_occurrences": 1,
    }


def test_rejects_tagged_remote_action(tmp_path):
    write(
        tmp_path,
        ".github/workflows/tag.yml",
        workflow("actions/checkout@v7"),
    )

    errors, _ = check_repository(tmp_path)

    assert any("not pinned" in error for error in errors)


def test_rejects_inconsistent_pins_for_one_repository(tmp_path):
    write(
        tmp_path,
        ".github/workflows/first.yml",
        workflow("github/codeql-action/init@" + SHA_A),
    )
    write(
        tmp_path,
        ".github/workflows/second.yml",
        workflow("github/codeql-action/upload-sarif@" + SHA_B),
    )

    errors, _ = check_repository(tmp_path)

    assert any(
        "github/codeql-action: inconsistent workflow action pins" in error
        for error in errors
    )


def test_rejects_exact_action_pin_mirrored_outside_workflows(tmp_path):
    action_ref = "actions/setup-node@" + SHA_A
    write(
        tmp_path,
        ".github/workflows/release.yml",
        workflow(action_ref),
    )
    write(
        tmp_path,
        ".github/tests/test_release.py",
        f'EXPECTED_ACTION = "{action_ref}"\n',
    )

    errors, _ = check_repository(tmp_path)

    assert any("exact workflow action pin is mirrored" in error for error in errors)


def test_accepts_elastic_action_identity_assertion(tmp_path):
    write(
        tmp_path,
        ".github/workflows/release.yml",
        workflow("actions/setup-python@" + SHA_A),
    )
    write(
        tmp_path,
        ".github/tests/test_release.py",
        'PATTERN = r"actions/setup-python@[0-9a-f]{40}"\n',
    )

    errors, _ = check_repository(tmp_path)

    assert errors == []


def test_rejects_docker_action_without_digest(tmp_path):
    write(
        tmp_path,
        ".github/workflows/docker.yml",
        workflow("docker://example/image:latest"),
    )

    errors, _ = check_repository(tmp_path)

    assert any("Docker action is not digest-pinned" in error for error in errors)
