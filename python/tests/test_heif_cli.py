"""Native HEIF CLI helper contracts."""

import hashlib
from pathlib import Path

import pytest

import iccdev


CORPUS = Path(__file__).resolve().parents[2] / ".github/ci/tooling/heif/img"
VALID_SHA256 = "de9b36ccc513fdc79860d5dff523aed9cf67411789e4c20ae866146cbaf75a4e"


def test_heif_dump_rejects_missing_input_and_output(tmp_path):
    with pytest.raises(FileNotFoundError, match="HEIF image not found"):
        iccdev.heif_dump(tmp_path / "missing.heic")
    with pytest.raises(NotADirectoryError, match="HEIF output directory not found"):
        iccdev.heif_dump(CORPUS / "valid-prof.heic", tmp_path / "missing")


@pytest.mark.cli
def test_heif_dump_reports_and_extracts_raw_profile(tmp_path):
    try:
        tool = iccdev.find_tool("iccHeifDump")
    except iccdev.IccToolError:
        pytest.skip("iccHeifDump is not built")
    assert tool.is_file()
    assert iccdev.available_tools()["iccHeifDump"] == tool

    report = iccdev.heif_dump(CORPUS / "valid-prof.heic", tmp_path)
    assert "item=1 property=1" in report
    assert "profile_bytes=540" in report
    assert "proflib_parse=ok" in report
    assert "icc_profiles=1" in report

    extracted = tmp_path / "item-1-property-1-prof.icc"
    assert extracted.stat().st_size == 540
    assert hashlib.sha256(extracted.read_bytes()).hexdigest() == VALID_SHA256

    empty_report = iccdev.heif_dump(CORPUS / "nclx-only.heif")
    assert "icc_profiles=0" in empty_report

    with pytest.raises(iccdev.IccToolError) as failure:
        iccdev.heif_dump(CORPUS / "invalid-ipma-index.heic")
    assert failure.value.returncode == 2
    assert "unable to read HEIF colour properties" in failure.value.stderr
