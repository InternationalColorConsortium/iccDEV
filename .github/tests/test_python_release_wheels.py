"""Release wheel intake regression tests.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

from pathlib import Path
import runpy
import shlex
import zipfile

import pytest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/validate-python-release-wheels.py"
VALIDATE = runpy.run_path(str(SCRIPT))["validate_wheels"]


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
    yaml = pytest.importorskip("yaml")
    workflow_path = SCRIPT.parents[1] / "workflows/ci-latest-release.yml"
    workflow = yaml.safe_load(workflow_path.read_text(encoding="ascii"))
    jobs = workflow["jobs"]
    build = jobs["python-wheels"]
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
    yaml = pytest.importorskip("yaml")
    from packaging.requirements import Requirement

    workflow_path = SCRIPT.parents[1] / "workflows/ci-latest-release.yml"
    workflow = yaml.safe_load(workflow_path.read_text(encoding="ascii"))
    build = workflow["jobs"]["python-wheels"]
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
