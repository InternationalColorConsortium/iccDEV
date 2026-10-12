#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Prove source or installed MCP imports and a real console stdio session."""

import argparse
import asyncio
import json
import os
from pathlib import Path
import subprocess
import sys
import sysconfig
import tempfile


def tool_value(result):
    value = result.model_dump(by_alias=True, exclude_none=True)
    assert not value.get("isError"), value
    if "structuredContent" in value:
        return value["structuredContent"]
    return json.loads("".join(item["text"] for item in value["content"] if item["type"] == "text"))


async def protocol_smoke(command, directory, environment, profile):
    import anyio
    from mcp.client import Client
    from mcp.client.stdio import StdioServerParameters

    parameters = StdioServerParameters(command=str(command), cwd=directory, env=environment)
    with anyio.fail_after(45):
        async with Client(parameters, read_timeout_seconds=15) as client:
            names = set()
            cursor = None
            for _ in range(10):
                listing = (await client.list_tools(cursor=cursor)).model_dump(by_alias=True)
                names.update(tool["name"] for tool in listing["tools"])
                cursor = listing.get("nextCursor")
                if not cursor:
                    break
            else:
                raise AssertionError("MCP inventory pagination did not finish")
            assert {"health_check", "profile_summary", "inspect_header"} <= names, names
            health = tool_value(await client.call_tool("health_check", {}))
            assert health["status"] == "ok", health
            assert set(health["python_api"]["available_tools"]) <= names, health
            summary = tool_value(await client.call_tool("profile_summary", {"path": str(profile)}))
            assert summary["profile_size"] > 128 and summary["filename"] == profile.name, summary
            print("[OK] Console stdio discovery, health, and profile inspection", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-tree", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    source = root / "iccdev-mcp/iccdev_mcp"
    import iccdev_mcp
    from iccdev_mcp import server, rest_api

    imported = Path(iccdev_mcp.__file__).resolve()
    assert imported.is_relative_to(source.resolve()) == args.source_tree, imported
    assert callable(server.main) and callable(rest_api.main)
    environment = os.environ.copy()
    environment.pop("PYTHONPATH", None)
    # Pip-only capability checks must not inherit an unrelated native build.
    for key in ("ICCDEV_BUILD_DIR", "ICCDEV_VALIDATION_LIBRARY", "ICCDEV_TOOLS_DIR"):
        environment.pop(key, None)
    profile = root / "Testing/sRGB_v4_ICC_preference.icc"
    assert profile.is_file(), profile
    environment["ICCDEV_TESTING_DIR"] = str(profile.parent)
    if not server._transform_available():
        for operation in (
            lambda: server.color_transform(str(profile), str(profile), [[0.2, 0.4, 0.6]]),
            lambda: server.roundtrip_delta(str(profile), [[0.2, 0.4, 0.6]]),
        ):
            try:
                operation()
            except server.NativeBindingsUnavailable:
                pass
            else:
                raise AssertionError("MCP fabricated a transform without native bindings")
    from starlette.testclient import TestClient
    with TestClient(rest_api.create_app()) as client:
        response = client.get("/api/health")
        assert response.status_code == 200 and response.json()["ok"] is True, response.text
    print("[OK] REST health and optional transform capability contract", flush=True)
    with tempfile.TemporaryDirectory(prefix="iccdev-mcp-package-") as directory:
        child = subprocess.run(
            [sys.executable, "-c", "import json,iccdev_mcp; from iccdev_mcp import server,rest_api; "
             "print(json.dumps(iccdev_mcp.__file__))"],
            cwd=directory, env=environment, capture_output=True, text=True, check=True, timeout=30,
        )
        assert Path(json.loads(child.stdout)).resolve() == imported, child.stdout
        scripts = Path(sysconfig.get_path("scripts"))
        suffix = ".exe" if os.name == "nt" else ""
        for name in ("iccdev-mcp", "iccdev-mcp-rest"):
            command = scripts / (name + suffix)
            assert command.is_file(), command
            subprocess.run([str(command), "--help"], cwd=directory, env=environment,
                           capture_output=True, text=True, check=True, timeout=30)
        print(f"[OK] Parent/child {'source' if args.source_tree else 'installed'} imports: {imported}", flush=True)
        asyncio.run(protocol_smoke(scripts / ("iccdev-mcp" + suffix), directory, environment, profile))


if __name__ == "__main__":
    main()
