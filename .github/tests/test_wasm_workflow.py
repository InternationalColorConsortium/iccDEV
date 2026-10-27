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


def test_release_windows_build_uses_manifest_dependencies():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )
    windows_steps = {
        step["name"]: step for step in workflow["jobs"]["windows"]["steps"]
    }
    build = windows_steps["Install manifest dependencies and build"]["run"]
    assert "vcpkg integrate install" in build
    assert "vcpkg install" in build
    assert "(Get-Command vcpkg -CommandType Application -ErrorAction Stop).Source" in build
    assert "$env:VCPKG_ROOT = Split-Path -Parent $vcpkgExecutable" in build
    assert "Join-Path $env:VCPKG_ROOT 'scripts\\buildsystems\\vcpkg.cmake'" in build
    assert "vcpkg toolchain file not found: $vcpkgToolchain" in build
    assert "cmake --preset vs2022-x64 -B msvc -S Build/Cmake" in build
    assert "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" in build
    assert "cmake --build msvc --config Release --parallel" in build
    assert "vcpkg-win-2324-libheif-deps" not in build
    assert "VCPKG_MANIFEST_MODE=OFF" not in build
    assert 'fullfile(pwd, \'msvc\')' in windows_steps["Build MATLAB MEX gateway"]["with"]["command"]
    assert '"msvc/bin/Release"' in windows_steps["Stage bundle into Testing directory"]["run"]


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


def test_release_uses_opt_in_npm_trusted_publishing():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )
    dispatch = workflow["on"]["workflow_dispatch"]["inputs"]
    assert dispatch["npm_packages"] == {
        "description": "Validated npm distributions to publish with OIDC",
        "required": True,
        "default": "none",
        "type": "choice",
        "options": ["none", "iccdev"],
    }
    assert dispatch["npm_version"]["type"] == "string"
    assert dispatch["npm_dist_tag"] == {
        "description": "npm dist-tag for the selected distribution",
        "required": True,
        "default": "latest",
        "type": "string",
    }

    wasm_steps = {
        step["name"]: step for step in workflow["jobs"]["wasm"]["steps"]
    }
    stage_wasm = wasm_steps["Stage npm-shaped WASM bundle (issue #830)"]
    assert stage_wasm["env"]["NPM_PACKAGES"] == "${{ inputs.npm_packages }}"
    assert stage_wasm["env"]["NPM_VERSION"] == "${{ inputs.npm_version }}"
    assert stage_wasm["env"]["NPM_DIST_TAG"] == "${{ inputs.npm_dist_tag }}"
    assert 'VERSION="$NPM_VERSION"' in stage_wasm["run"]
    assert 'npm_version must be a SemVer version without build metadata' in stage_wasm["run"]
    assert (
        r"^[0-9]+(\.[0-9]+){2}(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$"
        in stage_wasm["run"]
    )
    assert 'Prerelease npm_version values require a non-latest npm_dist_tag' in stage_wasm["run"]

    release = workflow["jobs"]["release"]
    assert release["permissions"]["id-token"] == "write"
    release_step_names = [step["name"] for step in release["steps"]]
    steps = {step["name"]: step for step in release["steps"]}
    setup_node = steps["Set up Node.js for npm publication"]
    assert setup_node["uses"] == "actions/setup-node@249970729cb0ef3589644e2896645e5dc5ba9c38"
    assert setup_node["with"]["node-version"] == "24"
    assert setup_node["with"]["package-manager-cache"] is False

    stage = steps["Stage validated npm distribution"]
    assert "artifacts/iccdev-wasm-release/package.json" in stage["run"]
    assert 'npm pack --dry-run --json' in stage["run"]
    assert 'npm pack --pack-destination "$GITHUB_WORKSPACE/npm-iccdev"' in stage["run"]

    publish = steps["Publish iccdev to npm"]
    assert publish["id"] == "npm_iccdev"
    assert 'npm publish "./$tarball" --provenance --tag "$NPM_DIST_TAG"' in publish["run"]
    assert "unset NODE_AUTH_TOKEN || true" in publish["run"]
    assert "NODE_AUTH_TOKEN" not in publish["env"]

    smoke = steps["Verify public iccdev npm package"]
    assert smoke["continue-on-error"] is True
    assert "!cancelled()" in smoke["if"]
    assert "steps.npm_iccdev.outcome == 'success'" in smoke["if"]
    assert "for attempt in {1..30}" in smoke["run"]
    assert 'npm pack --pack-destination "$npm_environment"' in smoke["run"]
    assert "Published npm package tarball was not available after 450 seconds" in smoke["run"]
    assert 'tar -xzf "$tarball" --strip-components=1' in smoke["run"]
    assert '"/package.json"' in smoke["run"]

    summary = steps["Summary Report"]
    assert summary["env"]["NPM_ICCDEV_OUTCOME"] == "${{ steps.npm_iccdev.outcome }}"
    last_publication = max(
        release_step_names.index(name)
        for name in (
            "Publish iccdev to PyPI",
            "Publish iccdev-mcp to PyPI",
            "Publish iccdev to npm",
        )
    )
    first_verification = min(
        release_step_names.index(name)
        for name in (
            "Verify public iccdev installation",
            "Verify public iccdev-mcp installation",
            "Verify public iccdev npm package",
        )
    )
    assert last_publication < first_verification < release_step_names.index("Summary Report")
