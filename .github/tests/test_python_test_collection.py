"""Exercise reusable workflow handling of pytest success, failure, and emptiness.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

import base64
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest
import yaml


WORKFLOWS = Path(__file__).resolve().parents[1] / "workflows"
EXIT_CASES = [(0, 0), (1, 0), (5, 0), (0, 1), (0, 5)]


def workflow_test_block(workflow, step_name, marker):
    config = yaml.safe_load((WORKFLOWS / workflow).read_text(encoding="ascii"))
    steps = config["jobs"]["build-test"]["steps"]
    block = next(step["run"] for step in steps if step.get("name") == step_name)
    return block, block[block.index(marker):]


def bash_executable():
    if os.name == "nt":
        # System32/bash.exe starts WSL and cannot execute this Windows test cwd.
        for root in (os.environ.get("ProgramFiles"), os.environ.get("ProgramW6432")):
            if root:
                candidate = Path(root) / "Git/bin/bash.exe"
                if candidate.is_file():
                    return str(candidate)
        pytest.skip("Git Bash is unavailable")
    executable = shutil.which("bash")
    if not executable:
        pytest.skip("Bash is unavailable")
    return executable


@pytest.mark.parametrize("unit_exit,parity_exit", EXIT_CASES)
def test_unix_pytest_exit_codes(tmp_path, unit_exit, parity_exit):
    block, snippet = workflow_test_block(
        "_build-test-unix.yml", "Build and test Python bindings",
        'echo "=== Running Python/Cython unit tests ==="',
    )
    assert block.startswith("set -euo pipefail\n")
    script = f"""set -euo pipefail
WORKSPACE_DIR=.
LOGDIR=.
python() {{
  if [[ "${{@: -1}}" == parity ]]; then
    echo PYTEST_PARITY
    return {parity_exit}
  fi
  echo PYTEST_UNIT
  return {unit_exit}
}}
{snippet}
"""
    result = subprocess.run(
        [bash_executable(), "--noprofile", "--norc", "-c", script],
        cwd=tmp_path, capture_output=True, text=True, timeout=30,
        env={**os.environ, "BASH_ENV": "/dev/null"},
    )
    assert result.returncode == (unit_exit or parity_exit), result.stdout + result.stderr
    assert "PYTEST_UNIT" in result.stdout
    assert ("PYTEST_PARITY" in result.stdout) == (unit_exit == 0)
    assert (tmp_path / "python-tests.log").read_text(encoding="ascii") == "PYTEST_UNIT\n"
    assert (tmp_path / "python-parity-tests.log").exists() == (unit_exit == 0)


@pytest.mark.parametrize("unit_exit,parity_exit", EXIT_CASES)
def test_windows_pytest_exit_codes(tmp_path, unit_exit, parity_exit):
    executable = shutil.which("pwsh")
    if not executable:
        pytest.skip("PowerShell 7 is unavailable")
    _, snippet = workflow_test_block(
        "_build-test-windows.yml", "Install and test Python bindings",
        "$result = python -m pytest",
    )
    native_python = str(Path(sys.executable)).replace("'", "''")
    script = f"""Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
function python {{
  if ($args[-1] -eq 'parity') {{
    & '{native_python}' -c "import sys; print('PYTEST_PARITY'); sys.exit({parity_exit})"
  }} else {{
    & '{native_python}' -c "import sys; print('PYTEST_UNIT'); sys.exit({unit_exit})"
  }}
  $global:LASTEXITCODE = $LASTEXITCODE
}}
{snippet}
"""
    encoded = base64.b64encode(script.encode("utf-16le")).decode("ascii")
    result = subprocess.run(
        [executable, "-NoProfile", "-NoLogo", "-NonInteractive", "-EncodedCommand", encoded],
        cwd=tmp_path, capture_output=True, text=True, timeout=30,
    )
    assert (result.returncode == 0) == (unit_exit == parity_exit == 0), result.stdout + result.stderr
    assert "PYTEST_UNIT" in result.stdout
    assert ("PYTEST_PARITY" in result.stdout) == (unit_exit == 0)
    if unit_exit:
        assert f"pytest failed with exit code {unit_exit}" in result.stderr
    elif parity_exit:
        assert f"Python parity tests failed with exit code {parity_exit}" in result.stderr


@pytest.mark.parametrize("workflow,step_name", [
    ("_build-test-unix.yml", "Validate native Python wheel with cibuildwheel"),
    ("_build-test-windows.yml", "Validate Python wheel matrix"),
])
def test_legacy_wheel_smoke_remains_enabled_by_default(workflow, step_name):
    config = yaml.safe_load((WORKFLOWS / workflow).read_text(encoding="ascii"))
    trigger = config.get("on", config.get(True))
    smoke_input = trigger["workflow_call"]["inputs"]["run-wheel-smoke"]
    assert smoke_input["type"] == "boolean"
    assert smoke_input["default"] is True
    steps = config["jobs"]["build-test"]["steps"]
    smoke_step = next(step for step in steps if step.get("name") == step_name)
    assert smoke_step["if"] == "inputs.run-wheel-smoke"
    assert "python -m cibuildwheel --output-dir " in smoke_step["run"]
    assert all("inputs.run-wheel-smoke" not in step.get("if", "")
               for step in steps if step is not smoke_step)
