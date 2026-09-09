# iccdev-mcp -- Docker entrypoint tests
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

from __future__ import annotations

import os
import importlib.util
import io
import json
from types import SimpleNamespace
import subprocess
from pathlib import Path

import pytest


ENTRYPOINT = Path(__file__).parent.parent / "docker" / "docker-entrypoint.sh"
SMOKE = Path(__file__).resolve().parents[2] / ".github/scripts/iccdev-container-smoke.py"
spec = importlib.util.spec_from_file_location("container_smoke", SMOKE)
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


def test_rpc_keeps_stdin_open_through_response():
    output = {"jsonrpc": "2.0", "id": 1, "result": {"tools": []}}
    process = SimpleNamespace(stdin=io.StringIO(), stdout=io.StringIO(json.dumps(output) + "\n"))
    rpc = smoke.Rpc(process, 1)
    assert rpc.request("tools/list", {}) == {"tools": []}
    assert not process.stdin.closed
    assert json.loads(process.stdin.getvalue())["method"] == "tools/list"


@pytest.mark.parametrize("response", ["", "garbage\n",
    json.dumps({"jsonrpc": "2.0", "id": 1, "error": {"code": -1}}) + "\n",
    json.dumps({"jsonrpc": "2.0", "id": 2, "result": {}}) + "\n"])
def test_rpc_rejects_eof_invalid_error_and_wrong_id(response):
    rpc = smoke.Rpc(SimpleNamespace(stdin=io.StringIO(), stdout=io.StringIO(response)), 1)
    with pytest.raises((RuntimeError, ValueError, AssertionError)):
        rpc.request("tools/list", {})


def test_rpc_timeout_has_deadline():
    rpc = smoke.Rpc(SimpleNamespace(stdin=io.StringIO(), stdout=io.StringIO()), 0.01)
    rpc.reader.join()
    rpc.lines.get_nowait()  # Remove EOF to simulate an open but silent server.
    with pytest.raises(TimeoutError, match="MCP response deadline"):
        rpc.request("tools/list", {})


def test_rpc_skips_notification_without_closing_stdin():
    messages = [{"jsonrpc": "2.0", "method": "notifications/message"},
                {"jsonrpc": "2.0", "id": 1, "result": {"ok": True}}]
    process = SimpleNamespace(stdin=io.StringIO(), stdout=io.StringIO(
        "\n".join(json.dumps(message) for message in messages) + "\n"))
    assert smoke.Rpc(process, 1).request("tools/list", {}) == {"ok": True}


def test_container_cleans_up_and_logs_on_failure(monkeypatch, tmp_path):
    calls = []
    monkeypatch.setattr(smoke, "docker", lambda *args: calls.append(args))
    def logs(args, **kwargs):
        calls.append(tuple(args[1:]))
        return SimpleNamespace(stdout="startup failure\n", stderr="diagnostic\n")
    monkeypatch.setattr(smoke.subprocess, "run", logs)
    with pytest.raises(ValueError):
        with smoke.container("local-image", "rest", "-p", "127.0.0.1::8080", report_dir=tmp_path):
            raise ValueError("probe failed")
    name = calls[0][2]
    assert calls[-2] == ("logs", name)
    assert calls[-1] == ("rm", "--force", name)
    assert (tmp_path / "rest-container.log").read_text() == "startup failure\ndiagnostic\n"


def _write_stub(bin_dir: Path, name: str) -> None:
    stub = bin_dir / name
    stub.write_text(
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n"
        "printf '%s\\n' \"$@\"\n",
        encoding="ascii",
    )
    stub.chmod(0o755)


def test_leading_option_runs_default_mcp_command(tmp_path):
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    _write_stub(bin_dir, "iccdev-mcp")
    env = os.environ.copy()
    env["PATH"] = f"{bin_dir}:{env['PATH']}"

    result = subprocess.run(
        ["bash", str(ENTRYPOINT), "--help"],
        check=False,
        capture_output=True,
        text=True,
        env=env,
    )

    assert result.returncode == 0
    assert result.stdout == "--help\n"
    assert result.stderr == ""


def test_unknown_command_is_passed_through():
    result = subprocess.run(
        ["bash", str(ENTRYPOINT), "printf", "%s", "passthrough"],
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0
    assert result.stdout == "passthrough"
    assert result.stderr == ""
