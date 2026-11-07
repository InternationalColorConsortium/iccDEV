"""Regression checks for service file and upload trust boundaries."""

from __future__ import annotations

import base64
import json
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest

from iccdev_mcp import cli_tools, rest_api, server


ROOT = Path(__file__).resolve().parents[2]


def _native_tool(name: str) -> str:
    tool = cli_tools._find_tool(name)
    tools_dir = os.environ.get("ICCDEV_TOOLS_DIR")
    if tools_dir and (
        not tool or not Path(tool).resolve().is_relative_to(Path(tools_dir).resolve())
    ):
        pytest.fail(f"{name} is missing from ICCDEV_TOOLS_DIR={tools_dir}")
    if not tool:
        pytest.skip(f"{name} is not installed")
    return tool


def test_conversion_wrappers_pass_file_import_opt_out(monkeypatch, tmp_path):
    calls = []

    def fake_run_tool(name, args, **kwargs):
        calls.append((name, args))
        return {"returncode": 1, "stdout": "", "stderr": ""}

    monkeypatch.setattr(cli_tools, "_run_tool", fake_run_tool)
    for converter, name, flag in (
        (cli_tools.run_from_xml, "iccFromXml", "-no-file-includes"),
        (cli_tools.run_from_json, "iccFromJson", "-no-file-imports"),
    ):
        converter("input", str(tmp_path / "output.icc"))
        converter("input")
        assert calls[-2][0] == calls[-1][0] == name
        assert calls[-2][1][-1] == calls[-1][1][-1] == flag


def test_raw_cli_args_require_service_owner_opt_in(monkeypatch, tmp_path):
    monkeypatch.delenv("ICCDEV_MCP_ALLOW_RAW_CLI_ARGS", raising=False)
    canary = tmp_path / "canary.txt"
    canary.write_text("CANARY", encoding="ascii")
    raw = ["--exportcfg", str(canary), "missing.tif"]

    for invoke in (
        lambda: cli_tools.run_apply_profiles_args(raw),
        lambda: cli_tools.run_apply_named_cmm(raw),
        lambda: cli_tools.run_apply_search(raw),
        lambda: server.apply_profiles(config_args=raw),
        lambda: server.apply_named_cmm(" ".join(raw)),
        lambda: server.apply_search(" ".join(raw)),
    ):
        with pytest.raises(ValueError, match="Raw CLI arguments are disabled"):
            invoke()
    assert canary.read_text(encoding="ascii") == "CANARY"

    pytest.importorskip("starlette")
    from starlette.testclient import TestClient

    with TestClient(rest_api.create_app()) as client:
        for endpoint in ("apply-profiles", "apply-named-cmm", "apply-search"):
            response = client.post(
                f"/api/{endpoint}", json={"config_args": raw}
            )
            assert response.status_code == 400
            assert "Raw CLI arguments are disabled" in response.json()["error"]
    assert canary.read_text(encoding="ascii") == "CANARY"

    calls = []
    monkeypatch.setattr(
        cli_tools, "_run_tool",
        lambda name, args: calls.append((name, args)) or {"returncode": 0},
    )
    monkeypatch.setenv("ICCDEV_MCP_ALLOW_RAW_CLI_ARGS", "1")
    for wrapper in (
        cli_tools.run_apply_profiles_args,
        cli_tools.run_apply_named_cmm,
        cli_tools.run_apply_search,
    ):
        assert wrapper(raw)["returncode"] == 0
    assert [name for name, _ in calls] == [
        "iccApplyProfiles", "iccApplyNamedCmm", "iccApplySearch"
    ]


def test_xml_include_is_local_cli_only(monkeypatch, tmp_path):
    pytest.importorskip("starlette")
    from starlette.testclient import TestClient

    source = ROOT / "Testing" / "Display" / "SpectralRangeTags.xml"
    original = source.read_text(encoding="utf-8")
    modified, count = re.subn(
        r"<Image>\s*[0-9A-F]+\s*</Image>",
        '<Image File="server_canary.txt"/>',
        original,
        count=1,
    )
    assert count == 1
    xml_path = tmp_path / "import.xml"
    xml_path.write_text(modified, encoding="utf-8")
    inline_path = tmp_path / "inline.xml"
    inline_path.write_text(original, encoding="utf-8")
    canary = b"ICCDEV_XML_FILE_INCLUDE_CANARY"
    (tmp_path / "server_canary.txt").write_bytes(canary)
    monkeypatch.chdir(tmp_path)

    direct = tmp_path / "direct.icc"
    subprocess.run(
        [_native_tool("iccFromXml"), str(xml_path), str(direct)],
        check=True, capture_output=True, timeout=30,
    )
    assert canary in direct.read_bytes()

    blocked = cli_tools.run_from_xml(str(xml_path))
    assert blocked["returncode"] != 0
    assert "icc_base64" not in blocked
    inline = cli_tools.run_from_xml(str(inline_path))
    assert inline["returncode"] == 0
    assert base64.b64decode(inline["icc_base64"])
    assert server.xml_to_profile(str(xml_path))["returncode"] != 0

    with TestClient(rest_api.create_app()) as client:
        upload = client.post(
            "/api/upload",
            files={"file": ("import.xml", xml_path.read_bytes(), "application/xml")},
        )
        assert upload.status_code == 200
        uploaded = Path(upload.json()["path"])
        try:
            response = client.post(
                "/api/from-xml", json={"xml_path": str(uploaded)}
            )
            assert response.status_code == 200
            assert response.json()["returncode"] != 0
            assert "icc_base64" not in response.json()
        finally:
            uploaded.unlink(missing_ok=True)


def test_json_tint_file_is_local_cli_only(monkeypatch, tmp_path):
    pytest.importorskip("starlette")
    from starlette.testclient import TestClient

    from_xml = _native_tool("iccFromXml")
    to_json = _native_tool("iccToJson")
    from_json = _native_tool("iccFromJson")
    profile_path = tmp_path / "source.icc"
    subprocess.run(
        [from_xml, str(ROOT / "Testing/Calc/srgbCalcTest.xml"), str(profile_path)],
        check=True, capture_output=True, timeout=30,
    )
    inline_path = tmp_path / "inline.json"
    subprocess.run(
        [to_json, str(profile_path), str(inline_path)],
        check=True, capture_output=True, timeout=30,
    )
    assert cli_tools.run_from_json(str(inline_path))["returncode"] == 0

    document = json.loads(inline_path.read_text(encoding="utf-8"))
    tint = document["IccProfile"]["Tags"][1]["AToB1Tag"]["data"][
        "elements"][0]["subElements"][8]
    assert tint["type"] == "TintArrayElement"
    tint.pop("values")
    tint.update(file="tint_values.txt", format="text")
    (tmp_path / "tint_values.txt").write_text(
        "0.125 0.25 0.375 0.5 0.625 0.75", encoding="ascii"
    )
    imported_path = tmp_path / "import.json"
    imported_path.write_text(json.dumps(document), encoding="utf-8")
    monkeypatch.chdir(tmp_path)

    default = subprocess.run(
        [from_json, str(imported_path), str(tmp_path / "default.icc")],
        capture_output=True, timeout=30,
    )
    assert default.returncode != 0
    assert not (tmp_path / "default.icc").exists()
    subprocess.run(
        [from_json, str(imported_path), str(tmp_path / "direct.icc"),
         "-allow-file-imports"],
        check=True, capture_output=True, timeout=30,
    )
    blocked = cli_tools.run_from_json(str(imported_path))
    assert blocked["returncode"] != 0
    assert "icc_base64" not in blocked
    assert server.json_to_profile(str(imported_path))["returncode"] != 0

    with TestClient(rest_api.create_app()) as client:
        response = client.post(
            "/api/from-json", json={"json_path": str(imported_path)}
        )
        assert response.status_code == 200
        assert response.json()["returncode"] != 0
        assert "icc_base64" not in response.json()


def test_upload_quota_expiry_and_private_files(monkeypatch, tmp_path):
    pytest.importorskip("starlette")
    from starlette.testclient import TestClient

    directory = tmp_path / "iccdev-mcp-uploads"
    monkeypatch.setattr(rest_api, "UPLOAD_DIR", directory)
    monkeypatch.setattr(rest_api, "MAX_UPLOAD_TOTAL_BYTES", 10)
    monkeypatch.setattr(rest_api, "UPLOAD_RETENTION_SECONDS", 60)
    with TestClient(rest_api.create_app()) as client:
        first = client.post(
            "/api/upload", files={"file": ("a.icc", b"1234567")}
        )
        assert first.status_code == 200
        first_path = Path(first.json()["path"])
        assert re.fullmatch(r"[0-9a-f]{32}_a\.icc", first_path.name)
        if os.name != "nt":
            assert directory.stat().st_mode & 0o077 == 0
            assert first_path.stat().st_mode & 0o077 == 0

        refused = client.post(
            "/api/upload", files={"file": ("b.icc", b"1234567")}
        )
        assert refused.status_code == 507
        assert first_path.read_bytes() == b"1234567"
        assert len(list(directory.glob("*.icc"))) == 1

        os.utime(first_path, (1, 1))
        accepted = client.post(
            "/api/upload", files={"file": ("c.icc", b"1234567")}
        )
        assert accepted.status_code == 200
        assert not first_path.exists()
        assert Path(accepted.json()["path"]).read_bytes() == b"1234567"


def test_concurrent_uploads_share_quota(monkeypatch, tmp_path):
    pytest.importorskip("starlette")
    from starlette.testclient import TestClient

    directory = tmp_path / "iccdev-mcp-uploads"
    monkeypatch.setattr(rest_api, "UPLOAD_DIR", directory)
    monkeypatch.setattr(rest_api, "MAX_UPLOAD_TOTAL_BYTES", 10)

    def upload(index):
        with TestClient(rest_api.create_app()) as client:
            return client.post(
                "/api/upload",
                files={"file": (f"{index}.icc", b"1234567")},
            ).status_code

    with ThreadPoolExecutor(max_workers=4) as pool:
        statuses = list(pool.map(upload, range(4)))
    assert sorted(statuses) == [200, 507, 507, 507]
    assert sum(path.stat().st_size for path in directory.glob("*.icc")) == 7


def test_empty_uploads_cannot_exhaust_inodes(monkeypatch, tmp_path):
    pytest.importorskip("starlette")
    from starlette.testclient import TestClient

    directory = tmp_path / "iccdev-mcp-uploads"
    monkeypatch.setattr(rest_api, "UPLOAD_DIR", directory)
    monkeypatch.setattr(rest_api, "MAX_UPLOAD_FILES", 2)
    with TestClient(rest_api.create_app()) as client:
        statuses = [
            client.post(
                "/api/upload", files={"file": (f"{i}.icc", b"")}
            ).status_code
            for i in range(3)
        ]
    assert statuses == [200, 200, 507]
    assert len(list(directory.glob("*.icc"))) == 2


@pytest.mark.asyncio
async def test_upload_stream_limit_without_content_length(monkeypatch, tmp_path):
    pytest.importorskip("starlette")
    httpx = pytest.importorskip("httpx")
    import starlette.formparsers as formparsers

    directory = tmp_path / "iccdev-mcp-uploads"
    monkeypatch.setattr(rest_api, "UPLOAD_DIR", directory)
    monkeypatch.setattr(rest_api, "MAX_MULTIPART_BYTES", 150)
    if hasattr(formparsers.MultiPartParser, "spool_max_size"):
        monkeypatch.setattr(formparsers.MultiPartParser, "spool_max_size", 16)
    else:
        monkeypatch.setattr(formparsers.MultiPartParser, "max_file_size", 16)
    spooled = []
    original_spooled_file = formparsers.SpooledTemporaryFile

    def track_spooled_file(*args, **kwargs):
        file = original_spooled_file(*args, **kwargs)
        spooled.append(file)
        return file

    monkeypatch.setattr(formparsers, "SpooledTemporaryFile", track_spooled_file)
    payload = (
        b"--b\r\nContent-Disposition: form-data; name=\"file\"; "
        b"filename=\"a.icc\"\r\n\r\n"
    )
    async def body():
        yield payload + b"X" * 32
        yield b"X" * 151
        yield b"\r\n--b--\r\n"

    async with httpx.AsyncClient(
        transport=httpx.ASGITransport(app=rest_api.create_app()),
        base_url="http://testserver",
    ) as client:
        response = await client.post(
            "/api/upload",
            content=body(),
            headers={"content-type": "multipart/form-data; boundary=b"},
        )
    assert response.status_code == 413
    assert spooled and all(file.closed for file in spooled)
    assert not directory.exists()
