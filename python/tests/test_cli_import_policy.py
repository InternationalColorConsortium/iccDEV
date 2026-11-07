"""Security contract for Python CLI-backed XML and JSON conversion.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

from __future__ import annotations

import importlib.util
import importlib
import os
from pathlib import Path

import pytest


def test_conversion_helpers_require_explicit_sidecar_opt_in(monkeypatch):
    try:
        cli = importlib.import_module("iccdev.cli")
    except ModuleNotFoundError as error:
        if (os.environ.get("ICCDEV_REQUIRE_INSTALLED_PACKAGE") == "1" or
                error.name not in {"iccdev", "iccdev._iccdev"}):
            raise
        source = Path(__file__).resolve().parents[1] / "iccdev" / "cli.py"
        spec = importlib.util.spec_from_file_location("iccdev_cli_policy_test", source)
        assert spec is not None and spec.loader is not None
        cli = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cli)
    calls = []

    def fake_run_tool(name, *args, timeout):
        calls.append((name, args, timeout))
        Path(args[1]).write_bytes(b"ICC")
        return b""

    monkeypatch.setattr(cli, "_run_tool", fake_run_tool)
    assert cli.icc_from_xml("<IccProfile/>") == b"ICC"
    assert cli.icc_from_xml("<IccProfile/>", allow_file_includes=True) == b"ICC"
    assert cli.icc_from_json({"IccProfile": {}}) == b"ICC"
    assert cli.icc_from_json(
        {"IccProfile": {}}, allow_file_imports=True
    ) == b"ICC"
    assert [name for name, _, _ in calls] == [
        "iccFromXml", "iccFromXml", "iccFromJson", "iccFromJson"
    ]
    assert [args[2:] for _, args, _ in calls] == [
        ("-no-file-includes",), (), ("-no-file-imports",),
        ("-allow-file-imports",),
    ]

    with pytest.raises(TypeError, match="boolean"):
        cli.icc_from_xml("<IccProfile/>", allow_file_includes="false")
    with pytest.raises(TypeError, match="boolean"):
        cli.icc_from_json({}, allow_file_imports="false")
