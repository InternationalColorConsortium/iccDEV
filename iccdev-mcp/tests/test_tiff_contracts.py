# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""Shared MCP/REST TIFF contracts, including optional real-tool coverage."""

import base64
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest
from starlette.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from iccdev_mcp import cli_tools, profiles, server
from iccdev_mcp.rest_api import create_app


@pytest.mark.parametrize("kwargs", [
    {"start": True}, {"start": 2**31}, {"increment": 0},
    {"increment": -1}, {"end": 65536}, {"compress": "1"},
    {"planar": 2}, {"tile_width": 16}, {"tile_width": 17, "tile_height": 32},
    {"tile_width": 16.0, "tile_height": 32}, {"bigtiff": "true"}, {"profile": 0},
])
def test_invalid_specsep_options_fail_before_execution(monkeypatch, kwargs):
    def unexpected(*args, **options):
        pytest.fail("Invalid options must not launch a native tool")
    monkeypatch.setattr(cli_tools, "_run_tool", unexpected)
    options = {"start": 1, "end": 3, "increment": 1}
    options.update(kwargs)
    with pytest.raises(ValueError):
        cli_tools.run_spec_sep_to_tiff("missing_", "out.tif", **options)


def test_specsep_native_argv_and_channel_resolution(monkeypatch, tmp_path):
    prefix = str(tmp_path / "channel with spaces_")
    for number in (3, 2, 1):
        Path(prefix + str(number)).write_bytes(b"TIFF")
    profile = tmp_path / "match.icc"
    profile.write_bytes(b"ICC")
    captured = {}
    def run(tool, args):
        captured.update(tool=tool, args=args)
        return {"returncode": 0}
    monkeypatch.setattr(cli_tools, "_run_tool", run)
    cli_tools.run_spec_sep_to_tiff(
        prefix, "out.tif", 3, 1, -1, 1, 1, str(profile), 16, 32, True,
    )
    assert captured == {
        "tool": "iccSpecSepToTiff",
        "args": ["--tiles", "16", "32", "--bigtiff", "out.tif", "1", "1",
                 str(tmp_path.resolve() / "channel with spaces_"),
                 "3", "1", "-1", str(profile.resolve())],
    }
    Path(prefix + "2").unlink()
    with pytest.raises(FileNotFoundError):
        cli_tools.run_spec_sep_to_tiff(prefix, "out.tif", 3, 1, -1)


def test_specsep_checks_every_channel_access_policy(monkeypatch, tmp_path):
    prefix = str(tmp_path / "channel_")
    for number in (1, 2):
        Path(prefix + str(number)).write_bytes(b"TIFF")
    monkeypatch.setattr(profiles, "_is_under_allowed_base",
                        lambda path: path.name != "channel_2")
    with pytest.raises(ValueError, match="Access denied"):
        cli_tools.run_spec_sep_to_tiff(prefix, "out.tif", 1, 2, 1)


@pytest.mark.parametrize("kwargs", [
    {"ifd": -1}, {"ifd": 4096}, {"ifd": True},
    {"ifd": 0, "list_directories": True},
    {"list_directories": "true"}, {"evidence_json": 1},
])
def test_invalid_dump_options(kwargs):
    with pytest.raises(ValueError):
        cli_tools.run_tiff_dump("input.tif", **kwargs)


def test_tiff_dump_rest_and_mcp_options(monkeypatch, tmp_path):
    image = tmp_path / "input.tif"
    image.write_bytes(b"TIFF")
    captured = []
    def run(tool, args):
        captured.append((tool, args))
        return {"returncode": 0, "stdout": "", "stderr": ""}
    monkeypatch.setattr(cli_tools, "_run_tool", run)
    server.tiff_dump(str(image), ifd=1, evidence_json=True)
    client = TestClient(create_app())
    response = client.get("/api/tiff-dump", params={
        "path": str(image), "ifd": "1", "evidence_json": "true",
    })
    assert response.status_code == 200
    assert captured[0] == captured[1]
    assert captured[0][1][:3] == ["--ifd", "1", "--evidence-json"]
    for options in ({"ifd": "-1"}, {"ifd": "4096"},
                    {"ifd": "0", "list_directories": "true"},
                    {"evidence_json": "yes"}):
        assert client.get("/api/tiff-dump", params={
            "path": str(image), **options,
        }).status_code == 400


def test_specsep_rest_and_mcp_output_and_cleanup(monkeypatch, tmp_path):
    prefix = str(tmp_path / "channel_")
    Path(prefix + "1").write_bytes(b"TIFF")
    outputs = []
    def run(tool, args):
        assert tool == "iccSpecSepToTiff"
        output = Path(args[0])
        outputs.append(output)
        output.write_bytes(b"TIFF output")
        return {"returncode": 0, "stdout": "", "stderr": ""}
    monkeypatch.setattr(cli_tools, "_run_tool", run)
    arguments = {"input_prefix": prefix, "start": 1, "end": 1, "increment": 1}
    expected = base64.b64encode(b"TIFF output").decode("ascii")
    assert server.spec_sep_to_tiff(**arguments)["output_base64"] == expected
    client = TestClient(create_app())
    response = client.post("/api/spec-sep", json=arguments)
    assert response.status_code == 200
    assert response.json()["output_base64"] == expected
    assert all(not output.exists() for output in outputs)
    for invalid in ({"increment": 0}, {"start": True}, {"bigtiff": "true"}, {"profile": 0},
                    {"reflectance_profile": "old.icc"}):
        assert client.post("/api/spec-sep", json={
            **arguments, **invalid,
        }).status_code == 400
    def fail(tool, args):
        Path(args[0]).write_bytes(b"partial")
        outputs.append(Path(args[0]))
        return {"returncode": 1, "stdout": "", "stderr": "native failure"}
    monkeypatch.setattr(cli_tools, "_run_tool", fail)
    failed = server.spec_sep_to_tiff(**arguments)
    assert failed["returncode"] == 1 and "output_base64" not in failed
    assert all(not output.exists() for output in outputs)


def test_native_tiff_binding_alignment(tmp_path):
    """Run against explicitly selected current-branch tools, not mocks."""
    fixture = os.environ.get("ICCDEV_TIFF_FIXTURE")
    if not fixture:
        pytest.skip("Set ICCDEV_TIFF_FIXTURE to enable native TIFF binding QA")
    subprocess.run([fixture, "--fixtures", str(tmp_path)], check=True, timeout=60)
    client = TestClient(create_app())
    multi = tmp_path / "multi.tif"
    response = client.get("/api/tiff-dump", params={
        "path": str(multi), "list_directories": "true", "evidence_json": "true",
    })
    assert response.status_code == 200 and response.json()["returncode"] == 0
    inventory = json.loads(response.json()["stdout"])["directories"]
    assert len(inventory) == 2 and inventory[0]["tiled"] and inventory[1]["bigTiff"]
    assert server.tiff_dump(str(multi), ifd=1)["returncode"] != 0  # malformed ICC
    for bits in (8, 16, 32):
        for planar in (0, 1):
            options = {"input_prefix": str(tmp_path / f"gray{bits}_"),
                       "start": 1, "end": 3, "increment": 1,
                       "compress": 1, "planar": planar}
            stripped = server.spec_sep_to_tiff(**options)
            tiled_response = client.post("/api/spec-sep", json={
                **options, "tile_width": 16, "tile_height": 32, "bigtiff": True,
            })
            assert tiled_response.status_code == 200
            tiled = tiled_response.json()
            assert stripped["returncode"] == tiled["returncode"] == 0
            strip_path, tile_path = tmp_path / "strip.tif", tmp_path / "tile.tif"
            strip_path.write_bytes(base64.b64decode(stripped["output_base64"]))
            tile_path.write_bytes(base64.b64decode(tiled["output_base64"]))
            subprocess.run([fixture, "--compare", str(strip_path), str(tile_path)],
                           check=True, timeout=60)
            evidence = server.tiff_dump(str(tile_path), evidence_json=True)
            metadata = json.loads(evidence["stdout"])
            assert metadata["tiled"] and metadata["bigTiff"]

    profile = tmp_path / "display.icc"
    source = Path(__file__).resolve().parents[2] / "Testing" / "Display" / "LCDDisplay.xml"
    assert cli_tools.run_from_xml(str(source), str(profile))["returncode"] == 0
    strip_path, tile_path = tmp_path / "apply-strip.tif", tmp_path / "apply-tile.tif"
    config = {
        "imageFiles": {
            "srcImageFile": str(tmp_path / "tiles.tif"),
            "dstImageFile": str(strip_path), "dstEncoding": "8Bit",
            "dstCompression": True, "dstPlanar": False, "dstEmbedIcc": True,
        },
        "connect": {"threads": 2},
        "profileSequence": [
            {"iccFile": str(profile), "intent": "relative"},
            {"iccFile": str(profile), "intent": "relative"},
        ],
    }
    config_path = tmp_path / "apply.json"
    config_path.write_text(json.dumps(config), encoding="ascii")
    assert server.apply_profiles(config_args=["-cfg", str(config_path)])["returncode"] == 0
    config["imageFiles"].update(dstImageFile=str(tile_path), dstTileWidth=16,
                               dstTileHeight=32, dstBigTiff=True)
    config_path.write_text(json.dumps(config), encoding="ascii")
    response = client.post("/api/apply-profiles", json={
        "config_args": ["-cfg", str(config_path)],
    })
    assert response.status_code == 200 and response.json()["returncode"] == 0
    subprocess.run([fixture, "--compare", str(strip_path), str(tile_path)],
                   check=True, timeout=60)
    metadata = json.loads(server.tiff_dump(str(tile_path), evidence_json=True)["stdout"])
    assert metadata["tiled"] and metadata["bigTiff"]
    config["imageFiles"]["dstTileWidth"] = -16
    config_path.write_text(json.dumps(config), encoding="ascii")
    assert server.apply_profiles(config_args=["-cfg", str(config_path)])["returncode"] != 0


@pytest.mark.asyncio
async def test_native_stdio_tiff_contract(tmp_path):
    fixture = os.environ.get("ICCDEV_TIFF_FIXTURE")
    if not fixture:
        pytest.skip("Set ICCDEV_TIFF_FIXTURE to enable native stdio TIFF QA")
    from mcp.client import Client
    from mcp.client.stdio import StdioServerParameters
    import anyio

    subprocess.run([fixture, "--fixtures", str(tmp_path)], check=True, timeout=60)
    package_root = Path(__file__).resolve().parents[1]
    environment = dict(os.environ, PYTHONPATH=str(package_root))
    parameters = StdioServerParameters(
        command=sys.executable, args=["-m", "iccdev_mcp.server"],
        cwd=str(package_root), env=environment,
    )
    def value(result):
        payload = result.model_dump(by_alias=True, exclude_none=True)
        assert not payload.get("isError"), payload
        if "structuredContent" in payload:
            return payload["structuredContent"]
        return json.loads("".join(item["text"] for item in payload["content"]
                                  if item["type"] == "text"))

    with anyio.fail_after(45):
        async with Client(parameters, read_timeout_seconds=15) as client:
            inventory = value(await client.call_tool("tiff_dump", {
                "path": str(tmp_path / "multi.tif"),
                "list_directories": True, "evidence_json": True,
            }))
            assert inventory["returncode"] == 0
            assert len(json.loads(inventory["stdout"])["directories"]) == 2
            combined = value(await client.call_tool("spec_sep_to_tiff", {
                "input_prefix": str(tmp_path / "gray8_"),
                "start": 1, "end": 3, "increment": 1,
                "tile_width": 16, "tile_height": 32, "bigtiff": True,
            }))
            assert combined["returncode"] == 0
            output = tmp_path / "stdio.tif"
            output.write_bytes(base64.b64decode(combined["output_base64"]))
            evidence = value(await client.call_tool("tiff_dump", {
                "path": str(output), "evidence_json": True,
            }))
            metadata = json.loads(evidence["stdout"])
            assert metadata["tiled"] and metadata["bigTiff"]
