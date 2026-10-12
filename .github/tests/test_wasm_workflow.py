# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""WASM dependency-download workflow regression tests."""

from pathlib import Path

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    ("workflow_name", "job_name"),
    [
        ("ci-latest-release.yml", "wasm"),
        ("ci-pr-wasm.yml", "wasm-build-and-validate"),
    ],
)
def test_wasm_archives_use_bounded_retries_and_checksums(workflow_name, job_name):
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows" / workflow_name).read_text(encoding="ascii")
    )
    steps = workflow["jobs"][job_name]["steps"]
    build = next(
        step["run"] for step in steps
        if step["name"].startswith("Build third-party dependencies")
    )
    assert "curl -sLO" not in build
    for archive, url, checksum in (
        (
            "libpng-1.6.57.tar.gz",
            "https://download.sourceforge.net/libpng/libpng-1.6.57.tar.gz",
            "01fdab044cdc575c74a9ef62a4042fbe24fea8bedfe8aa5c89f4467b781bbb60",
        ),
        (
            "jpegsrc.v9e.tar.gz",
            "https://ijg.org/files/jpegsrc.v9e.tar.gz",
            "4077d6a6a75aeb01884f708919d25934c93305e49f7e3f36db9129320e6f4f3d",
        ),
    ):
        command_end = build.index(url) + len(url)
        command_start = build.rfind(
            "curl --fail --location --silent --show-error", 0, command_end
        )
        assert command_start >= 0
        command = build[command_start:command_end]
        assert "--retry 5 --retry-all-errors --retry-delay 5 --retry-max-time 180" in command
        assert "--connect-timeout 30 --max-time 60" in command
        assert f"--output {archive}" in command
        assert f"{checksum}  {archive}" in build


def test_release_publication_is_not_cancelled():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )

    assert workflow["concurrency"]["cancel-in-progress"] is False


def test_wasm_workflow_regression_runs_in_shared_wheel_gate():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/_build-python-wheels.yml").read_text(
            encoding="ascii"
        )
    )
    steps = workflow["jobs"]["wheels"]["steps"]
    test_step = next(
        step for step in steps if step["name"] == "Build and test standalone wheels"
    )

    assert ".github/tests/test_wasm_workflow.py" in test_step["run"]
