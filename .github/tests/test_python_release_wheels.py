"""Release wheel intake regression tests.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

from pathlib import Path
import runpy
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
    assert '"numpy>=1.23,<3"' in build["env"]["CIBW_BEFORE_TEST"]
