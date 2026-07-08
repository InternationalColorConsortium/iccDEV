"""Node-based regression tests for the embedded REST dashboard JavaScript."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

import pytest


def test_rest_dashboard_ui_cjs_regressions():
    """Run browserless UI regressions for profile selectors and request building."""
    node = shutil.which("node")
    if node is None:
        pytest.skip("node is not installed")

    script = Path(__file__).with_name("rest_dashboard_ui_test.cjs")
    result = subprocess.run(
        [node, str(script)],
        check=False,
        text=True,
        capture_output=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    assert "REST dashboard UI regression tests passed" in result.stdout
