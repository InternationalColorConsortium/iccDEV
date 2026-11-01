#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# https://color.org
#
# Intent: Prevent false green results in the NaN range CTest helper.
###############################################################

import os
from pathlib import Path
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github/scripts/iccdev-applytolink-nan-sixchan-debug.sh"


@pytest.mark.skipif(os.name == "nt" or shutil.which("bash") is None,
                    reason="NaN range helper requires Unix execution")
def test_nan_range_helper_distinguishes_skip_failure_and_pass(tmp_path):
    tools = tmp_path / "Tools"
    tools.mkdir()
    out = tmp_path / "output"
    profile = tmp_path / "SixChanInputRef.icc"
    env = dict(
        os.environ,
        ICCDEV_TOOLS_DIR=str(tools),
        ICCDEV_TESTING_DIR=str(tmp_path),
        ICCDEV_TEST_OUTDIR=str(out),
        ICCDEV_APPLYTOLINK_NAN_PROFILE=str(profile),
        ICCDEV_APPLYTOLINK_FLAMEGRAPH="0",
        ICCDEV_APPLYTOLINK_PROFILE_PROBE="0",
    )

    def run():
        return subprocess.run(["bash", str(SCRIPT)], env=env,
                              capture_output=True, text=True, check=False)

    assert run().returncode == 77  # Tool absent.

    apply = tools / "iccApplyToLink"
    apply.write_text("#!/bin/sh\necho 'Invalid input range' >&2\nexit 1\n",
                     encoding="ascii")
    apply.chmod(0o755)
    cache = tmp_path / "CMakeCache.txt"
    cache.write_text("ENABLE_SANITIZERS:BOOL=OFF\n", encoding="ascii")
    assert run().returncode == 77  # Tool lacks AddressSanitizer.

    cache.write_text("ENABLE_SANITIZERS:BOOL=ON\n", encoding="ascii")
    # The NaN guard must work without an ICCXML-generated profile.
    measured = run()
    assert measured.returncode == 0, measured.stdout + measured.stderr
    assert "[PASS]" in measured.stdout

    apply.write_text("#!/bin/sh\necho 'cannot open profile' >&2\nexit 1\n",
                     encoding="ascii")
    wrong_failure = run()
    assert wrong_failure.returncode == 2

    env["ICCDEV_APPLYTOLINK_PROFILE_PROBE"] = "1"
    requested_probe = run()
    assert requested_probe.returncode == 2
    assert "profile probe requested" in requested_probe.stdout
