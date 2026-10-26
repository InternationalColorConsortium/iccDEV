# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""MCP release artifact and workflow regression tests."""

import base64
import csv
import hashlib
import io
from pathlib import Path
import runpy
import tarfile
import zipfile

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = runpy.run_path(str(ROOT / ".github/scripts/validate-mcp-release-package.py"))
VALIDATE = VALIDATOR["validate_packages"]
VERSION = VALIDATOR["package_version"]()


def make_packages(directory, wheel_changes=None, source_changes=None):
    dist_info = f"iccdev_mcp-{VERSION}.dist-info/"
    metadata = (f"Metadata-Version: 2.4\nName: iccdev-mcp\nVersion: {VERSION}\n"
                "Requires-Python: >=3.10\nLicense-Expression: BSD-3-Clause\nLicense-File: LICENSE.md\n")
    license_text = (ROOT / "LICENSE.md").read_text(encoding="ascii")
    modules = {"iccdev_mcp/" + name.name: "" for name in (ROOT / "iccdev-mcp/iccdev_mcp").glob("*.py")}
    files = {**modules, dist_info + "METADATA": metadata,
             dist_info + "WHEEL": "Wheel-Version: 1.0\nRoot-Is-Purelib: true\nTag: py3-none-any\n",
             dist_info + "licenses/LICENSE.md": license_text,
             dist_info + "entry_points.txt": "[console_scripts]\niccdev-mcp = iccdev_mcp.server:main\niccdev-mcp-rest = iccdev_mcp.rest_api:main\n"}
    files.update(wheel_changes or {})
    record = io.StringIO(newline="")
    writer = csv.writer(record)
    for name, content in files.items():
        if content is not None:
            data = content.encode("utf-8")
            checksum = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode("ascii")
            writer.writerow([name, "sha256=" + checksum, len(data)])
    writer.writerow([dist_info + "RECORD", "", ""])
    files[dist_info + "RECORD"] = record.getvalue()
    wheel = directory / f"iccdev_mcp-{VERSION}-py3-none-any.whl"
    with zipfile.ZipFile(wheel, "w") as archive:
        for name, content in files.items():
            if content is not None:
                archive.writestr(name, content)
    source = {**modules, "pyproject.toml": "", "README.md": "", "LICENSE.md": license_text, "PKG-INFO": metadata}
    source.update(source_changes or {})
    sdist = directory / f"iccdev_mcp-{VERSION}.tar.gz"
    with tarfile.open(sdist, "w:gz") as archive:
        for name, content in source.items():
            if content is not None:
                data = content.encode("utf-8")
                member = tarfile.TarInfo(f"iccdev_mcp-{VERSION}/" + name)
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
    return wheel, sdist


def test_accepts_universal_wheel_and_source_distribution(tmp_path):
    wheel, sdist = make_packages(tmp_path)
    assert VALIDATE(tmp_path, VERSION) == [wheel, sdist]


@pytest.mark.parametrize("changes", [
    {"iccdev/__init__.py": "conflict"}, {"../outside": "unsafe"},
    {"iccdev_mcp/backend.pyd": "native"}, {"iccdev_mcp/server.py": None},
    {f"iccdev_mcp-{VERSION}.dist-info/licenses/LICENSE.md": "short license"},
    {f"iccdev_mcp-{VERSION}.dist-info/METADATA": "Name: wrong\n"},
    {f"iccdev_mcp-{VERSION}.dist-info/WHEEL": "Root-Is-Purelib: false\n"},
    {f"iccdev_mcp-{VERSION}.dist-info/entry_points.txt": "[console_scripts]\niccdev-mcp = wrong:main\n"},
])
def test_rejects_unsafe_or_incomplete_wheel(tmp_path, changes):
    make_packages(tmp_path, wheel_changes=changes)
    with pytest.raises((ValueError, KeyError)):
        VALIDATE(tmp_path, VERSION)


@pytest.mark.parametrize("changes", [
    {"../outside": "unsafe"}, {"iccdev/__init__.py": "conflict"},
    {"pyproject.toml": None}, {"iccdev_mcp/server.py": None},
    {"LICENSE.md": "short license"}, {"PKG-INFO": "Name: wrong\n"},
])
def test_rejects_unsafe_or_incomplete_source_distribution(tmp_path, changes):
    make_packages(tmp_path, source_changes=changes)
    with pytest.raises(ValueError):
        VALIDATE(tmp_path, VERSION)


def test_rejects_missing_or_foreign_package_inventory(tmp_path):
    with pytest.raises(ValueError, match="inventory"):
        VALIDATE(tmp_path, VERSION)
    wheel, sdist = make_packages(tmp_path)
    (tmp_path / "iccdev_mcp-wrong.whl").write_bytes(wheel.read_bytes())
    with pytest.raises(ValueError, match="inventory"):
        VALIDATE(tmp_path, VERSION)
    sdist.unlink()
    with pytest.raises(ValueError, match="inventory"):
        VALIDATE(tmp_path, VERSION)


def test_release_proves_both_installation_modes_and_collects_mcp_artifacts():
    jobs = yaml.safe_load((ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii"))["jobs"]
    producer = jobs["mcp-package"]
    assert producer["permissions"] == {"contents": "read"}
    assert set(producer["strategy"]["matrix"]["os"]) == {"ubuntu-24.04", "windows-2022"}
    build = next(step["run"] for step in producer["steps"] if step["name"] == "Build and test MCP distributions")
    assert "--source-tree" in build and "python -m venv" in build
    assert "mcp-dist/*.whl mcp-dist/*.tar.gz" in build
    assert '"$interpreter" .github/scripts/iccdev-mcp-package-smoke.py' in build
    upload = next(step for step in producer["steps"] if step.get("uses", "").startswith("actions/upload-artifact@"))
    assert upload["if"] == "runner.os == 'Linux'"
    assert upload["with"]["if-no-files-found"] == "error"
    release = jobs["release"]
    assert "mcp-package" in release["needs"]
    downloads = [step["with"]["name"] for step in release["steps"]
                 if step.get("uses", "").startswith("actions/download-artifact@")]
    assert downloads.count("iccdev-mcp-python") == 1
    steps = {step["name"]: step for step in release["steps"]}
    assert "validate-mcp-release-package.py artifacts/iccdev-mcp-python" in steps["Package release archives"]["run"]
    assert '-name \'*.tar.gz\'' in steps["Generate release checksums"]["run"]
    assert "release-assets/*.tar.gz" in steps["Create latest release"]["run"]
    assert steps["Attest release archive provenance"]["with"]["subject-checksums"] == "release-assets/SHA256SUMS"


def test_release_uses_opt_in_trusted_pypi_publication_and_public_smoke():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )
    dispatch = workflow["on"]["workflow_dispatch"]["inputs"]["pypi_packages"]
    assert dispatch["type"] == "choice" and dispatch["default"] == "none"
    assert dispatch["options"] == ["none", "iccdev", "iccdev-mcp", "both"]

    release = workflow["jobs"]["release"]
    assert release["permissions"]["id-token"] == "write"
    assert "'release'" in release["environment"]["name"]
    steps = {step["name"]: step for step in release["steps"]}
    stage = steps["Stage validated PyPI distributions"]
    assert "inputs.pypi_packages != 'none'" in stage["if"]
    assert "validate-python-release-wheels.py release-assets --release" in stage["run"]
    assert "validate-mcp-release-package.py release-assets" in stage["run"]
    assert '"$distribution_count" -ne 45' in stage["run"]
    assert '"$distribution_count" -ne 2' in stage["run"]

    publish_iccdev = steps["Publish iccdev to PyPI"]
    assert publish_iccdev["uses"] == \
        "pypa/gh-action-pypi-publish@7245906fb61fa33a13554f9d5fb856fe61758086"
    assert publish_iccdev["with"]["packages-dir"] == "pypi-iccdev/"
    assert publish_iccdev["with"]["attestations"] is True
    assert "password" not in publish_iccdev["with"]

    publish_mcp = steps["Publish iccdev-mcp to PyPI"]
    assert publish_mcp["uses"] == publish_iccdev["uses"]
    assert publish_mcp["with"]["packages-dir"] == "pypi-iccdev-mcp/"
    assert publish_mcp["with"]["attestations"] is True

    iccdev_smoke = steps["Verify public iccdev installation"]["run"]
    assert "https://pypi.org/simple" in iccdev_smoke
    assert "ICCDEV_REQUIRE_INSTALLED_PACKAGE=1" in iccdev_smoke
    mcp_smoke = steps["Verify public iccdev-mcp installation"]["run"]
    assert "https://pypi.org/simple" in mcp_smoke
    assert "iccdev-mcp-package-smoke.py" in mcp_smoke

    release_notes = steps["Create latest release"]["run"]
    assert 'python -m pip install \\"iccdev-mcp[rest]\\"' in release_notes


def test_mcp_readme_pins_source_install_to_a_release_tag():
    readme = (ROOT / "iccdev-mcp/README.md").read_text(encoding="ascii")
    assert (
        "git+https://github.com/InternationalColorConsortium/iccDEV.git"
        "@<release-tag>#subdirectory=iccdev-mcp"
    ) in readme
