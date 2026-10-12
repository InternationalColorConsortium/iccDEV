# Copyright (c) International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     1. Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#
#     2. Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#
#     3. Neither the name of International Color Consortium (ICC) nor the names of
#        its contributors may be used to endorse or promote products derived from
#        this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Release wheel intake regression tests."""

from pathlib import Path
import os
import runpy
import shlex
import shutil
import stat
import subprocess
import sys
import zipfile

import pytest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/validate-python-release-wheels.py"
VALIDATOR = runpy.run_path(str(SCRIPT))
VALIDATE = VALIDATOR["validate_wheels"]
LANE_IDENTIFIERS = VALIDATOR["lane_identifiers"]
WORKFLOWS = SCRIPT.parents[1] / "workflows"
LICENSE = (SCRIPT.parents[2] / "LICENSE.md").read_text(encoding="ascii")


def load_workflow(name):
    yaml = pytest.importorskip("yaml")
    return yaml.safe_load((WORKFLOWS / name).read_text(encoding="ascii"))


def shared_wheel_job():
    return load_workflow("_build-python-wheels.yml")["jobs"]["wheels"]


def make_wheel(directory, platform="manylinux_2_17_x86_64.manylinux2014_x86_64",
               version="0.1.0", native=True, metadata_version=None,
               python_tag="cp313", overrides=None):
    path = directory / f"iccdev-{version}-{python_tag}-{python_tag}-{platform}.whl"
    dist_info = f"iccdev-{version}.dist-info/"
    files = {
        dist_info + "METADATA": (
            f"Metadata-Version: 2.4\nName: iccdev\nVersion: {metadata_version or version}\n"
            "License-Expression: BSD-3-Clause\nLicense-File: LICENSE.md\n"
        ),
        dist_info + "WHEEL": "Wheel-Version: 1.0\nRoot-Is-Purelib: false\n" + "".join(
            f"Tag: {python_tag}-{python_tag}-{tag}\n" for tag in platform.split(".")
        ),
        dist_info + "RECORD": "",
        dist_info + "licenses/LICENSE.md": LICENSE,
        "iccdev/__init__.py": "",
        "iccdev/py.typed": "",
        "iccdev/_iccdev.pyi": "",
    }
    if native:
        files[f"iccdev/_iccdev.{python_tag}.so"] = "native fixture"
    files.update(overrides or {})
    with zipfile.ZipFile(path, "w") as archive:
        for name, content in files.items():
            if content is not None:
                member = zipfile.ZipInfo()
                # Preserve malformed paths even on Windows, where ZipInfo's
                # constructor otherwise normalizes backslashes automatically.
                member.filename = name
                archive.writestr(member, content)
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


@pytest.mark.parametrize("lane", VALIDATOR["LANE_PLATFORMS"])
def test_accepts_complete_lane_inventory(tmp_path, lane):
    identifiers = LANE_IDENTIFIERS(lane)
    for identifier in identifiers:
        python_tag, platform = identifier.split("-", 1)
        if platform.startswith("manylinux_"):
            platform = platform.replace("manylinux_", "manylinux_2_17_", 1)
        elif platform.startswith("musllinux_"):
            platform = platform.replace("musllinux_", "musllinux_1_2_", 1)
        elif platform.startswith("macosx_"):
            platform = platform.replace("macosx_", "macosx_11_0_", 1)
        make_wheel(tmp_path, platform, python_tag=python_tag)
    assert len(VALIDATE(tmp_path, sorted(identifiers), "0.1.0", lane=lane)) == len(identifiers)


def test_rejects_truncated_or_foreign_lane_inventory(tmp_path):
    make_wheel(tmp_path, "win_arm64")
    with pytest.raises(ValueError, match="inventory disagrees"):
        VALIDATE(tmp_path, ["cp313-win_arm64"], "0.1.0", lane="windows-arm64")
    with pytest.raises(ValueError, match="inventory disagrees"):
        VALIDATE(tmp_path, LANE_IDENTIFIERS("windows-amd64"), "0.1.0", lane="windows-arm64")


def test_rejects_duplicate_inventory_entries(tmp_path):
    make_wheel(tmp_path)
    with pytest.raises(ValueError, match="Duplicate cibuildwheel"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"] * 2, "0.1.0")


@pytest.mark.parametrize("platform", [
    "manylinux_bad_x86_64", "manylinux_2_17_x86_64.win_arm64",
    "manylinux_2_17_x86_64.any", "manylinux_2_17_x86_64.",
])
def test_rejects_malformed_or_mixed_platform_tags(tmp_path, platform):
    make_wheel(tmp_path, platform)
    with pytest.raises(ValueError, match="Unexpected or duplicate wheel target"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


@pytest.mark.parametrize("path,contents,error", [
    ("METADATA", "Name: iccdev\nName: another\nVersion: 0.1.0\n", "metadata Name"),
    ("METADATA", "Name: iccdev\nVersion: 0.1.0\nVersion: 2.0\n", "metadata Version"),
    ("METADATA", "Name: iccdev\nVersion: 0.1.0\n", "metadata License-Expression"),
    ("METADATA", "malformed metadata", "Malformed wheel metadata"),
    ("WHEEL", None, "Missing wheel metadata"),
    ("RECORD", None, "Missing wheel metadata"),
    ("WHEEL", "Wheel-Version: 1.0\nRoot-Is-Purelib: true\n", "metadata Root-Is-Purelib"),
    ("WHEEL", "Wheel-Version: 1.0\nRoot-Is-Purelib: false\nTag: cp39-cp39-win32\n", "tags disagree"),
    ("licenses/LICENSE.md", "", "canonical LICENSE.md"),
    ("licenses/LICENSE.md", "BSD-3-Clause", "canonical LICENSE.md"),
])
def test_rejects_inconsistent_metadata_and_license(tmp_path, path, contents, error):
    make_wheel(tmp_path, overrides={"iccdev-0.1.0.dist-info/" + path: contents})
    with pytest.raises(ValueError, match=error):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


def test_accepts_canonical_license_with_checkout_line_endings(tmp_path):
    make_wheel(tmp_path, overrides={
        "iccdev-0.1.0.dist-info/licenses/LICENSE.md": LICENSE.replace("\n", "\r\n"),
    })
    assert len(VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")) == 1


@pytest.mark.parametrize("path", [
    "../outside", "/outside", "iccdev/../outside", "iccdev\\outside",
    "C:/outside", "iccdev/./outside", "iccdev//outside", "iccdev/control\nfile",
])
def test_rejects_unsafe_archive_paths(tmp_path, path):
    make_wheel(tmp_path, overrides={path: "payload"})
    with pytest.raises(ValueError, match="Unsafe wheel archive member"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


def test_rejects_duplicate_archive_members(tmp_path):
    wheel = make_wheel(tmp_path)
    with zipfile.ZipFile(wheel, "a") as archive, pytest.warns(UserWarning, match="Duplicate name"):
        archive.writestr("iccdev/__init__.py", "different contents")
    with pytest.raises(ValueError, match="Duplicate wheel archive member"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


def test_rejects_archive_symlinks(tmp_path):
    wheel = make_wheel(tmp_path)
    with zipfile.ZipFile(wheel, "a") as archive:
        link = zipfile.ZipInfo("iccdev/link")
        link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        archive.writestr(link, "../../outside")
    with pytest.raises(ValueError, match="Unsafe wheel archive member"):
        VALIDATE(tmp_path, ["cp313-manylinux_x86_64"], "0.1.0")


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
    assert '--lane "$id"' in steps["Package release archives"]["run"]
    assert steps["Attest release archive provenance"]["with"]["subject-checksums"] == "release-assets/SHA256SUMS"
    assert '"pytest>=7,<9"' in build["env"]["CIBW_BEFORE_TEST"]
    assert "--only-binary=numpy" in build["env"]["CIBW_BEFORE_TEST"]
    wheel_step = next(step for step in build["steps"]
                      if step.get("name") == "Build and test standalone wheels")
    assert wheel_step["env"]["WHEEL_LANE"] == "${{ matrix.id }}"
    assert '--lane "$WHEEL_LANE"' in wheel_step["run"]


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
        assert identifiers == LANE_IDENTIFIERS(lane["id"])
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


@pytest.mark.parametrize("release_outcome,attestation_outcome,with_assets,with_checkout", [
    ("success", "success", True, True),
    ("failure", "success", True, True),
    ("skipped", "failure", True, True),
    ("skipped", "skipped", False, True),
    ("skipped", "skipped", False, False),
])
def test_release_summary_reports_outcomes_and_sanitizes_assets(
    tmp_path, release_outcome, attestation_outcome, with_assets, with_checkout,
):
    if os.name == "nt":
        bash = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/bin/bash.exe"
        executable = str(bash) if bash.is_file() else None
    else:
        executable = shutil.which("bash")
    if not executable:
        pytest.skip("Bash is unavailable")
    steps = load_workflow("ci-latest-release.yml")["jobs"]["release"]["steps"]
    summary = next(step for step in steps if step.get("name") == "Summary Report")
    assert summary["env"]["RELEASE_OUTCOME"] == "${{ steps.publish.outcome }}"
    assert summary["env"]["ATTESTATION_OUTCOME"] == "${{ steps.attest.outcome }}"
    if with_checkout:
        scripts = tmp_path / ".github/scripts"
        scripts.mkdir(parents=True)
        shutil.copyfile(SCRIPT.parent / "sanitize-sed.sh", scripts / "sanitize-sed.sh")
    if with_assets:
        assets = tmp_path / "release-assets"
        assets.mkdir()
        name = "untrusted[link](payload).whl"
        if os.name != "nt":
            name = "untrusted[link](payload)<script>\x1b\n### forged.whl"
        (assets / name).write_text("fixture", encoding="ascii")
        (assets / "normal.zip").write_text("fixture", encoding="ascii")
        (assets / "not-a-package.txt").write_text("fixture", encoding="ascii")
        (assets / "directory.whl").mkdir()
    output = tmp_path / "summary.md"
    # macOS ships BSD find; asset counting must not require GNU find.
    prologue = "git() { :; }\nfind() { echo 'find is unavailable in this portability test' >&2; return 1; }\n"
    result = subprocess.run(
        [executable, "--noprofile", "--norc", "-c", prologue + summary["run"]],
        cwd=tmp_path, capture_output=True, text=True, timeout=30,
        env={**os.environ, "BASH_ENV": "/dev/null", "GITHUB_RUN_ID": "1234",
             "GITHUB_RUN_ATTEMPT": "2", "GITHUB_SHA": "abcdef1234567890",
             "GITHUB_STEP_SUMMARY": output.as_posix(), "RELEASE_OUTCOME": release_outcome,
             "ATTESTATION_OUTCOME": attestation_outcome},
    )
    assert result.returncode == 0, result.stdout + result.stderr
    content = output.read_text(encoding="ascii")
    assert f"Release publication: {release_outcome}" in content
    assert f"Artifact attestation: {attestation_outcome}" in content
    assert f"Assets staged: {2 if with_assets else 0} package files" in content
    assert "[link]" not in content
    assert "<script>" not in content
    assert "\x1b" not in content
    assert "\n### forged" not in content
    assert "Assets published" not in content
