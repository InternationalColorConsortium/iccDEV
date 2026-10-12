"""Release wheel intake regression tests.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

from pathlib import Path
import os
import runpy
import shlex
import subprocess
import sys
import zipfile

import pytest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/validate-python-release-wheels.py"
VALIDATE = runpy.run_path(str(SCRIPT))["validate_wheels"]
WORKFLOWS = SCRIPT.parents[1] / "workflows"


def load_workflow(name):
    yaml = pytest.importorskip("yaml")
    return yaml.safe_load((WORKFLOWS / name).read_text(encoding="ascii"))


def shared_wheel_job():
    return load_workflow("_build-python-wheels.yml")["jobs"]["wheels"]


def make_wheel(directory, platform="manylinux_2_17_x86_64.manylinux2014_x86_64",
               version="0.1.0", native=True, metadata_version=None):
    path = directory / f"iccdev-{version}-cp313-cp313-{platform}.whl"
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr(f"iccdev-{version}.dist-info/METADATA",
                         f"Name: iccdev\nVersion: {metadata_version or version}\n")
        for name in ("__init__.py", "py.typed", "_iccdev.pyi"):
            archive.writestr("iccdev/" + name, "")
        archive.writestr(f"iccdev-{version}.dist-info/licenses/LICENSE.md", "BSD-3-Clause")
        if native:
            archive.writestr("iccdev/_iccdev.cp313.so", "native fixture")
    return path


@pytest.mark.parametrize("platform,identifier", [
    ("manylinux_2_17_x86_64.manylinux2014_x86_64", "cp313-manylinux_x86_64"),
    ("musllinux_1_2_aarch64", "cp313-musllinux_aarch64"),
    ("macosx_10_13_universal2", "cp313-macosx_universal2"),
    ("win_amd64", "cp313-win_amd64"),
    ("win_arm64", "cp313-win_arm64"),
])
def test_accepts_repaired_platform_tags(tmp_path, platform, identifier):
    wheel = make_wheel(tmp_path, platform)
    assert VALIDATE(tmp_path, [identifier], "0.1.0") == [wheel]


@pytest.mark.parametrize("kwargs", [
    {"version": "0.2.0"}, {"native": False}, {"metadata_version": "0.2.0"},
    {"platform": "win_amd64"},
])
def test_rejects_wrong_version_contents_or_target(tmp_path, kwargs):
    make_wheel(tmp_path, **kwargs)
    with pytest.raises(ValueError):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


def test_rejects_missing_and_duplicate_wheels(tmp_path):
    with pytest.raises(ValueError, match="Missing wheel targets"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")
    make_wheel(tmp_path)
    make_wheel(tmp_path, "manylinux_2_28_x86_64")
    with pytest.raises(ValueError, match="duplicate wheel target"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


def test_rejects_empty_identifier_inventory(tmp_path):
    with pytest.raises(ValueError, match="Missing or unsupported"):
        VALIDATE(tmp_path, [], "0.1.0")


def test_release_workflow_connects_all_wheel_artifacts():
    workflow = load_workflow("ci-latest-release.yml")
    jobs = workflow["jobs"]
    call = jobs["python-wheels"]
    assert call["uses"] == "./.github/workflows/_build-python-wheels.yml"
    assert call["with"]["upload-wheels"] is True
    assert call["needs"] == "cache-policy"
    build = shared_wheel_job()
    release = jobs["release"]
    lanes = build["strategy"]["matrix"]["include"]
    assert len(lanes) == 7
    assert build["permissions"] == {"contents": "read"}
    assert "python-wheels" in release["needs"]
    expected = {"iccdev-python-" + lane["id"] for lane in lanes}
    downloads = {
        step["with"]["name"] for step in release["steps"]
        if step.get("uses", "").startswith("actions/download-artifact@")
        and step["with"]["name"].startswith("iccdev-python-")
    }
    assert downloads == expected
    steps = {step["name"]: step for step in release["steps"]}
    assert "release-assets/*.whl" in steps["Create latest release"]["run"]
    assert "-name '*.whl'" in steps["Generate release checksums"]["run"]
    assert "validate-python-release-wheels.py" in steps["Package release archives"]["run"]
    assert steps["Attest release archive provenance"]["with"]["subject-checksums"] == "release-assets/SHA256SUMS"
    assert '"pytest>=7,<9"' in build["env"]["CIBW_BEFORE_TEST"]
    assert "--only-binary=numpy" in build["env"]["CIBW_BEFORE_TEST"]


def test_primary_uses_identical_wheel_builder_without_uploads():
    jobs = load_workflow("ci-json-python.yml")["jobs"]
    call = jobs["python-wheels"]
    assert call["uses"] == "./.github/workflows/_build-python-wheels.yml"
    assert call["with"]["upload-wheels"] is False
    for name in ("linux", "macos", "windows"):
        assert jobs[name]["with"]["run-wheel-smoke"] is False


def test_shared_uploads_are_opt_in_and_exclude_pr_events():
    workflow = load_workflow("_build-python-wheels.yml")
    assert workflow["on"]["workflow_call"]["inputs"]["upload-wheels"]["default"] is False
    uploads = [step for step in shared_wheel_job()["steps"]
               if step.get("uses", "").startswith("actions/upload-artifact@")]
    assert len(uploads) == 1
    assert uploads[0]["if"] == (
        "inputs.upload-wheels && github.event_name != 'pull_request' "
        "&& github.event_name != 'pull_request_target'"
    )
    assert uploads[0]["with"]["if-no-files-found"] == "error"
    assert uploads[0]["with"]["name"] == "iccdev-python-${{ matrix.id }}"


def test_shared_matrix_builds_every_declared_wheel_identifier():
    pytest.importorskip("cibuildwheel")
    repo_root = SCRIPT.parents[2]
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("CIBW_")}
    declared = set()
    selected = set()
    for platform in ("linux", "macos", "windows"):
        result = subprocess.run(
            [sys.executable, "-m", "cibuildwheel", "python", "--platform", platform,
             "--print-build-identifiers"],
            cwd=repo_root, env=environment, check=True, capture_output=True, text=True,
        )
        declared.update(result.stdout.split())
    for lane in shared_wheel_job()["strategy"]["matrix"]["include"]:
        result = subprocess.run(
            [sys.executable, "-m", "cibuildwheel", "python", "--platform", lane["platform"],
             "--archs", lane["arch"], "--print-build-identifiers"],
            cwd=repo_root, env=environment, check=True, capture_output=True, text=True,
        )
        identifiers = set(result.stdout.split())
        assert identifiers
        assert not selected & identifiers, lane["id"]
        selected.update(identifiers)
    assert selected == declared
    assert len(selected) == 45


@pytest.mark.parametrize("lane_id,version,expected", [
    ("windows-arm64", "3.9", False),
    ("windows-arm64", "3.10", False),
    ("windows-arm64", "3.11", True),
    ("windows-arm64", "3.13", True),
    ("windows-amd64", "3.9", True),
    ("linux-x86_64", "3.9", True),
    ("linux-aarch64", "3.9", True),
    ("macos-arm64", "3.9", True),
    ("macos-universal2", "3.9", True),
])
def test_numpy_dependency_matches_available_wheel_targets(lane_id, version, expected):
    from packaging.requirements import Requirement

    build = shared_wheel_job()
    step = next(step for step in build["steps"] if step["name"] == "Build and test standalone wheels")
    assert step["env"]["CIBW_BEFORE_TEST"] == "${{ matrix.before_test || env.CIBW_BEFORE_TEST }}"
    lane = next(lane for lane in build["strategy"]["matrix"]["include"] if lane["id"] == lane_id)
    command = lane.get("before_test", build["env"]["CIBW_BEFORE_TEST"])
    arguments = shlex.split(command)
    numpy = Requirement(next(arg for arg in arguments if arg.startswith("numpy>=")))
    # The matrix controls architecture, even if an emulated shell reports AMD64.
    selected = numpy.marker is None or numpy.marker.evaluate({
        "platform_system": "Windows",
        "platform_machine": "AMD64",
        "python_version": version,
    })
    assert selected is expected
