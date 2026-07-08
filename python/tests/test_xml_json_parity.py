"""Native tool parity tests for XML and JSON profile operations.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

from __future__ import annotations

import json

import pytest

from .parity_helpers import first_rgb_profile, require_tool, run_tool, selected_profiles


pytestmark = [pytest.mark.parity, pytest.mark.cli]


@pytest.mark.xml
def test_xml_roundtrip_selected_profiles(tmp_path):
    to_xml = require_tool("IccToXml", "iccToXml")
    from_xml = require_tool("IccFromXml", "iccFromXml")

    for profile_path in selected_profiles():
        xml_path = tmp_path / f"{profile_path.stem}.xml"
        roundtrip_path = tmp_path / f"{profile_path.stem}-xml.icc"

        result = run_tool(to_xml, profile_path, xml_path)
        result.assert_success(f"iccToXml {profile_path.name}")
        assert xml_path.stat().st_size > 0

        result = run_tool(from_xml, xml_path, roundtrip_path)
        result.assert_success(f"iccFromXml {profile_path.name}")
        assert roundtrip_path.stat().st_size > 0


@pytest.mark.json
def test_json_roundtrip_selected_profiles(tmp_path):
    to_json = require_tool("IccToJson", "iccToJson")
    from_json = require_tool("IccFromJson", "iccFromJson")

    for profile_path in selected_profiles():
        json_path = tmp_path / f"{profile_path.stem}.json"
        roundtrip_path = tmp_path / f"{profile_path.stem}-json.icc"

        result = run_tool(to_json, profile_path, json_path)
        result.assert_success(f"iccToJson {profile_path.name}")
        assert json_path.stat().st_size > 0

        result = run_tool(from_json, json_path, roundtrip_path)
        result.assert_success(f"iccFromJson {profile_path.name}")
        assert roundtrip_path.stat().st_size > 0


@pytest.mark.json
def test_json_config_apply_named_cmm_smoke(tmp_path):
    apply_named = require_tool("IccApplyNamedCmm", "iccApplyNamedCmm")
    profile_path = first_rgb_profile()
    output_path = tmp_path / "json-config-output.txt"
    config_path = tmp_path / "apply-named-cmm.json"
    config = {
        "dataFiles": {
            "srcType": "colorData",
            "srcFile": "",
            "dstType": "colorData",
            "dstFile": str(output_path),
            "dstEncoding": "float",
            "dstPrecision": 4,
            "dstDigits": 9,
        },
        "profileSequence": [
            {
                "iccFile": str(profile_path),
                "intent": 1,
                "interpolation": "tetrahedral",
                "useBPC": False,
                "useD2BxB2Dx": True,
                "adjustPcsLuminance": False,
            }
        ],
        "colorData": {
            "space": "RGB ",
            "encoding": "float",
            "data": [
                {"values": [1.0, 1.0, 1.0]},
                {"values": [0.0, 0.0, 0.0]},
                {"values": [0.5, 0.5, 0.5]},
            ],
        },
    }
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")

    result = run_tool(apply_named, "-cfg", config_path)
    result.assert_success("iccApplyNamedCmm -cfg")
    assert output_path.stat().st_size > 0


def test_python_cli_blob_helpers_align_with_native_tools(tmp_path):
    iccdev = pytest.importorskip("iccdev")
    dump = require_tool("IccDumpProfile", "iccDumpProfile")
    roundtrip = require_tool("IccRoundTrip", "iccRoundTrip")
    profile_path = first_rgb_profile()

    xml_text = iccdev.icc_to_xml(profile_path)
    assert "<IccProfile" in xml_text
    xml_blob = iccdev.icc_from_xml(xml_text)
    xml_profile = tmp_path / "from-xml.icc"
    xml_profile.write_bytes(xml_blob)
    with iccdev.IccProfile(xml_profile) as profile:
        assert profile.header.magic == 0x61637370

    json_text = iccdev.icc_to_json(xml_blob)
    json_data = json.loads(json_text)
    json_blob = iccdev.icc_from_json(json_data)
    json_profile = tmp_path / "from-json.icc"
    json_profile.write_bytes(json_blob)
    with iccdev.IccProfile(json_profile) as profile:
        assert profile.header.magic == 0x61637370

    helper_dump = iccdev.dump_profile(json_blob)
    native_dump = run_tool(dump, json_profile, "ALL")
    native_dump.assert_success("iccDumpProfile ALL from Python JSON blob")
    assert "Profile" in helper_dump or "Header" in helper_dump
    assert "Profile" in native_dump.output or "Header" in native_dump.output

    helper_roundtrip = iccdev.round_trip(profile_path)
    native_roundtrip = run_tool(roundtrip, profile_path, timeout=60)
    native_roundtrip.assert_success("iccRoundTrip from Python helper input")
    assert "Round Trip" in helper_roundtrip
    assert "DeltaE" in helper_roundtrip
    assert "Round Trip" in native_roundtrip.output
    assert "DeltaE" in native_roundtrip.output
