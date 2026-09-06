#!/usr/bin/env python3
# Copyright (c) 2026 International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""Real bounded MCP/REST smoke for the unified image; host needs only Python."""

import argparse
import contextlib
import json
from pathlib import Path
import queue
import subprocess
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

PROFILE = "/workspace/iccDEV/Testing/sRGB_v4_ICC_preference.icc"


def docker(*args):
    return subprocess.check_output(["docker", *args], text=True, timeout=60).strip()


def record(report_dir, name, value):
    text = json.dumps(value, sort_keys=True)
    print(text, flush=True)
    if report_dir is not None:
        report_dir.mkdir(parents=True, exist_ok=True)
        (report_dir / (name + ".json")).write_text(text + "\n", encoding="ascii")


@contextlib.contextmanager
def container(image, mode, *options, report_dir=None):
    name = "iccdev-smoke-" + uuid.uuid4().hex
    docker("create", "--name", name, *options, image, "iccdev-mcp-entrypoint", mode)
    try:
        yield name
    except BaseException:
        # CI retains these diagnostics in its job log, including startup failures.
        result = subprocess.run(["docker", "logs", name], check=False, timeout=15,
                                capture_output=True, text=True)
        log = result.stdout + result.stderr
        print(log, flush=True)
        if report_dir is not None:
            report_dir.mkdir(parents=True, exist_ok=True)
            (report_dir / (mode + "-container.log")).write_text(log, encoding="utf-8")
        raise
    finally:
        docker("rm", "--force", name)


class Rpc:
    """Keep stdin alive; read responses and stderr concurrently without sleeps."""

    def __init__(self, process, timeout):
        self.process = process
        self.timeout = timeout
        self.lines = queue.Queue()
        self.sequence = 0
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        for line in self.process.stdout:
            self.lines.put(line)
        self.lines.put(None)

    def send(self, method, params, request_id=None):
        message = {"jsonrpc": "2.0", "method": method, "params": params}
        if request_id is not None:
            message["id"] = request_id
        self.process.stdin.write(json.dumps(message) + "\n")
        self.process.stdin.flush()

    def request(self, method, params):
        self.sequence += 1
        self.send(method, params, self.sequence)
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"MCP response deadline: {method}")
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty as exc:
                raise TimeoutError(f"MCP response deadline: {method}") from exc
            if line is None:
                raise RuntimeError(f"MCP closed stdout before response: {method}")
            response = json.loads(line)
            assert response["jsonrpc"] == "2.0", response
            if "id" not in response:
                continue  # Server notification, not our response.
            assert response["id"] == self.sequence, response
            assert "error" not in response, response
            return response["result"]

    def call(self, name, arguments):
        result = self.request("tools/call", {"name": name, "arguments": arguments})
        assert not result.get("isError"), result
        if "structuredContent" in result:
            return result["structuredContent"]
        return json.loads("".join(item["text"] for item in result["content"]
                                  if item["type"] == "text"))


def check_validation(result):
    assert result["status"] == 0 and result["status_name"] == "OK", result
    assert isinstance(result["report"], str), result


def smoke_mcp(image, timeout, require_validation, report_dir=None):
    expected_cli = set(json.loads(docker(
        "run", "--rm", image, "python", "-c",
        "import json; from iccdev_mcp.cli_tools import TOOL_BINARIES; "
        "print(json.dumps(list(TOOL_BINARIES)))")))
    with container(image, "mcp", "-i", report_dir=report_dir) as name:
        process = subprocess.Popen(["docker", "start", "-a", "-i", name],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   text=True)  # stderr stays in the job log
        try:
            rpc = Rpc(process, timeout)
            init = rpc.request("initialize", {
                "protocolVersion": "2025-06-18", "capabilities": {},
                "clientInfo": {"name": "iccdev-container-smoke", "version": "1"},
            })
            assert init["serverInfo"]["name"] == "iccdev-mcp", init
            assert "tools" in init["capabilities"], init
            rpc.send("notifications/initialized", {})
            tools = []
            params = {}
            listing_deadline = time.monotonic() + timeout
            while True:
                rpc.timeout = max(0, listing_deadline - time.monotonic())
                listing = rpc.request("tools/list", params)
                tools.extend(listing["tools"])
                if not listing.get("nextCursor"):
                    break
                params = {"cursor": listing["nextCursor"]}
            rpc.timeout = timeout
            names = {tool["name"] for tool in tools}
            assert len(names) == len(tools) and names, tools
            assert all(isinstance(tool["inputSchema"], dict) for tool in tools)
            health = rpc.call("health_check", {})
            assert health["status"] == "ok" and health["python_api"]["available"], health
            assert set(health["cli_tools"]["available"]) == expected_cli, health
            assert not health["cli_tools"]["missing"], health
            native = health["python_api"]["available_tools"]
            services = health["service_tools"]
            assert set(native + services) <= names, health
            assert health["python_api"]["tools"] == len(native), health
            assert health["total_tools"] == len(native) + len(expected_cli) + len(services), health
            validation = health["validation_api"]["available"]
            record(report_dir, "mcp", {"mcp_tools": sorted(names), "health": health})
            if require_validation:
                assert validation, health
                assert health["total_tools"] == len(names), health
            header = rpc.call("inspect_header", {"path": PROFILE})
            assert header["color_space"] == "RGB ", header
            if validation:
                check_validation(rpc.call("validate_profile", {"path": PROFILE}))
            return health
        finally:
            try:
                process.stdin.close()  # Only after the final requested response.
            except BrokenPipeError:
                pass
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            process.stdout.close()


def get_json(base, route, timeout):
    # Do not send localhost probes through ambient proxy configuration.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with opener.open(base + route, timeout=timeout) as response:
        assert response.status == 200
        return json.load(response)


def smoke_rest(image, timeout, mcp_health, require_validation, report_dir=None):
    with container(image, "rest", "-p", "127.0.0.1::8080", report_dir=report_dir) as name:
        docker("start", name)
        bindings = json.loads(docker("inspect", name))[0]["NetworkSettings"]["Ports"]["8080/tcp"]
        assert len(bindings) == 1 and bindings[0]["HostIp"] == "127.0.0.1", bindings
        base = "http://127.0.0.1:" + bindings[0]["HostPort"]
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("REST health startup deadline")
            try:
                health = get_json(base, "/api/health", min(remaining, 2))
                break
            except (urllib.error.URLError, TimeoutError, ConnectionError):
                time.sleep(min(0.2, max(0, deadline - time.monotonic())))
        assert health["ok"] and health["server"] == "iccdev-mcp", health
        assert health["python_api_available"], health
        assert health["validation_api_available"] == mcp_health["validation_api"]["available"], health
        if require_validation:
            assert health["validation_api_available"], health
        assert set(health["cli_tools"]["available"]) == set(mcp_health["cli_tools"]["available"]), health
        assert not health["cli_tools"]["missing"], health
        inventory = get_json(base, "/api/tools", timeout)
        tools, utilities = inventory["tools"], inventory["rest_utility_routes"]
        assert inventory["count"] == len(tools) == health["tools_count"], inventory
        assert inventory["rest_utility_routes_count"] == len(utilities) == health["rest_utility_routes_count"]
        assert inventory["total_rest_routes"] == len(tools) + len(utilities), inventory
        assert len({tool["name"] for tool in tools + utilities}) == len(tools) + len(utilities)
        assert all(tool["method"] in {"GET", "POST"} and tool["path"].startswith("/api/")
                   and tool["description"] for tool in tools + utilities), inventory
        record(report_dir, "rest", {"rest_health": health, "rest_inventory": inventory})
        query = "?" + urllib.parse.urlencode({"path": PROFILE})
        header = get_json(base, "/api/inspect-header" + query, timeout)
        assert header["color_space"] == "RGB ", header
        if health["validation_api_available"]:
            check_validation(get_json(base, "/api/validate-profile" + query, timeout))
        pawg = get_json(base, "/api/pawg-report" + query, timeout)
        assert pawg["returncode"] == 0, pawg


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--report-dir", type=Path, help="save identity, inventories and failure logs")
    parser.add_argument("--allow-missing-validation", action="store_true",
                        help="inventory older/minimal images; never use for the release gate")
    args = parser.parse_args()
    if not 1 <= args.timeout <= 300:
        parser.error("timeout must be between 1 and 300 seconds")
    identity = json.loads(docker("image", "inspect", args.image))[0]
    record(args.report_dir, "image", {"id": identity["Id"], "digests": identity["RepoDigests"],
                                      "labels": identity["Config"]["Labels"]})
    required = not args.allow_missing_validation
    health = smoke_mcp(args.image, args.timeout, required, args.report_dir)
    smoke_rest(args.image, args.timeout, health, required, args.report_dir)
    print("PASS: real MCP handshake, discovery, operations and REST smoke", flush=True)


if __name__ == "__main__":
    main()
