# iccdev-mcp -- Docker entrypoint tests
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

from __future__ import annotations

import os
import subprocess
from pathlib import Path


ENTRYPOINT = Path(__file__).parent.parent / "docker" / "docker-entrypoint.sh"


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
