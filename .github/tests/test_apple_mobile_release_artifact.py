# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Contracts for the Apple mobile static-core latest-release payload."""

from __future__ import annotations

import importlib.util
import re
from pathlib import Path

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github/scripts/validate-apple-mobile-core-artifact.py"
SPEC = importlib.util.spec_from_file_location("validate_apple_mobile_core_artifact", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)


def _artifact(root: Path, built: str = "apple-ios-device-core") -> Path:
    (root / "LICENSE.md").write_bytes((ROOT / "LICENSE.md").read_bytes())
    lines = []
    for preset, sdk in VALIDATOR.expected_presets().items():
        result = "built" if preset == built else "skipped: SDK unavailable"
        lines.append(f"{preset}|{sdk}|{result}")
    (root / "manifest.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    if built:
        output_dir = root / built
        output_dir.mkdir()
        flavor = "extended-core" if built.endswith("-extended-core") else "core"
        for library, header in VALIDATOR.COMPONENTS[flavor]:
            (output_dir / f"lib{library}.a").write_bytes(b"archive")
            (output_dir / header).write_bytes(b"header")
    return root


def test_apple_release_artifact_requires_built_archives_and_headers(tmp_path: Path) -> None:
    artifact = _artifact(tmp_path)
    assert VALIDATOR.validate_artifact(artifact) == 1
    (artifact / "apple-ios-device-core/IccProfLibVer.h").unlink()
    with pytest.raises(ValueError, match="Missing Apple mobile core payload"):
        VALIDATOR.validate_artifact(artifact)


def test_apple_release_artifact_requires_complete_license(tmp_path: Path) -> None:
    artifact = _artifact(tmp_path)
    license_file = artifact / "LICENSE.md"
    license_file.unlink()
    with pytest.raises(ValueError, match="Missing or altered Apple mobile core LICENSE"):
        VALIDATOR.validate_artifact(artifact)
    license_file.write_text("Incomplete license", encoding="utf-8")
    with pytest.raises(ValueError, match="Missing or altered Apple mobile core LICENSE"):
        VALIDATOR.validate_artifact(artifact)


def test_apple_release_artifact_requires_complete_unique_manifest(tmp_path: Path) -> None:
    artifact = _artifact(tmp_path)
    manifest = artifact / "manifest.txt"
    lines = manifest.read_text(encoding="utf-8").splitlines()
    manifest.write_text("\n".join(lines[:-1]) + "\n", encoding="utf-8")
    with pytest.raises(ValueError, match="omitted presets"):
        VALIDATOR.validate_artifact(artifact)
    manifest.write_text("\n".join(lines + [lines[0]]) + "\n", encoding="utf-8")
    with pytest.raises(ValueError, match="duplicate Apple preset"):
        VALIDATOR.validate_artifact(artifact)


def test_apple_release_artifact_rejects_empty_build(tmp_path: Path) -> None:
    artifact = _artifact(tmp_path, built="")
    with pytest.raises(ValueError, match="no built SDK presets"):
        VALIDATOR.validate_artifact(artifact)


def test_latest_release_consumes_validated_apple_artifact() -> None:
    release = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="utf-8")
    )
    apple = yaml.safe_load(
        (ROOT / ".github/workflows/ci-apple-mobile-core.yml").read_text(encoding="utf-8")
    )
    assert release["jobs"]["apple-mobile-core"]["with"]["release-artifact"] is True
    assert "apple-mobile-core" in release["jobs"]["release"]["needs"]
    assert any(
        step.get("with", {}).get("name") == "iccdev-apple-mobile-core"
        for step in release["jobs"]["release"]["steps"]
    )
    assert "release-artifact" in apple["on"]["workflow_call"]["inputs"]
    build_step = next(
        step for step in apple["jobs"]["build"]["steps"]
        if step.get("name") == "Build available Apple mobile core libraries"
    )
    assert "cp LICENSE.md artifacts/LICENSE.md" in build_step["run"]


def test_apple_runtime_smoke_routes_to_pr_without_simulators() -> None:
    release = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="utf-8")
    )
    mobile = yaml.safe_load(
        (ROOT / ".github/workflows/ci-apple-mobile-core.yml").read_text(encoding="utf-8")
    )
    platform = yaml.safe_load(
        (ROOT / ".github/workflows/ci-apple-platform-smoke.yml").read_text(encoding="utf-8")
    )
    pr = yaml.safe_load(
        (ROOT / ".github/workflows/ci-pr-action.yml").read_text(encoding="utf-8")
    )

    assert release["jobs"]["apple-mobile-core"]["with"]["release-artifact"] is True
    mobile_steps = {
        step["name"]: step
        for step in mobile["jobs"]["build"]["steps"]
        if "name" in step
    }
    for name in (
        "Exercise Apple simulator hosts",
        "Exercise Xcode CTest runtime layouts",
        "Summarize native Apple smoke results",
    ):
        assert mobile_steps[name]["if"] == "${{ !inputs.release-artifact }}"

    pr_job = pr["jobs"]["apple-xcode-ctest"]
    assert pr_job["uses"] == "./.github/workflows/ci-apple-platform-smoke.yml"
    assert pr_job["with"]["scope"] == "xcode"
    assert pr_job["with"]["target-sha"] == "${{ needs.detect-src.outputs.target_sha }}"
    assert pr_job["with"]["base-sha"] == "${{ needs.detect-src.outputs.base_sha }}"
    assert "apple_xcode_changed" in pr["jobs"]["detect-src"]["outputs"]
    assert "apple_xcode_changed" in pr_job["if"]
    detector = next(
        step for step in pr["jobs"]["detect-src"]["steps"]
        if step.get("name") == "Check for source changes"
    )["run"]
    route = re.search(r"if has_changed_file '([^']*ci-apple-platform-smoke[^']*)'; then", detector)
    assert route is not None
    for path in (
        ".github/workflows/ci-apple-platform-smoke.yml",
        ".github/workflows/ci-apple-mobile-core.yml",
        ".github/workflows/ci-pr-action.yml",
        ".github/scripts/iccdev-xcode-ctest-smoke.sh",
    ):
        assert re.search(route.group(1), path)
    for path in (".github/scripts/iccdev-apple-simulator-smoke.sh", "docs/build.md"):
        assert not re.search(route.group(1), path)
    assert "apple-xcode-ctest" in pr["jobs"]["finalize"]["needs"]

    assert {"scope", "target-sha", "base-sha"} <= set(platform["on"]["workflow_call"]["inputs"])
    platform_steps = {
        step["name"]: step
        for step in platform["jobs"]["smoke"]["steps"]
        if "name" in step
    }
    simulator = platform_steps["Exercise Apple simulator hosts"]
    xcode = platform_steps["Exercise Xcode CTest runtime layouts"]
    examples = platform_steps["Exercise manual iOS examples with sanitizers"]
    examples_summary = platform_steps["Summarize iOS example results"]
    assert "xcode" not in simulator["if"]
    assert "xcode" not in examples["if"]
    assert "xcode" not in examples_summary["if"]
    assert "iccdev-apple-simulator-smoke.sh" not in xcode["run"]
    assert "iccdev-xcode-ctest-smoke.sh" in xcode["run"]
    assert "'xcode'" in xcode["if"]
