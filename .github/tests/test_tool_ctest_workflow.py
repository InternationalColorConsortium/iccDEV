#!/usr/bin/env python3
###############################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# https://color.org
#
# Intent: Guard CTest spectral-preview setup and fast smoke selection.
###############################################################

import os
from pathlib import Path
import re
import shutil
import subprocess

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/ci-iccdev-tool-tests.yml"


def tool_step(name):
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="ascii"))
    return next(
        step for step in workflow["jobs"]["tool-tests"]["steps"]
        if step.get("name") == name
    )


def test_windows_sanitizers_use_trusted_checkout():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-pr-win.yml").read_text(encoding="ascii")
    )
    for job in workflow["jobs"].values():
        steps = job.get("steps", [])
        sanitizer_steps = [
            step for step in steps
            if "sanitize.ps1" in step.get("run", "")
        ]
        if not sanitizer_steps:
            continue
        trusted_checkout = next(
            step for step in steps
            if step.get("with", {}).get("path") == ".trusted-workflow"
        )
        assert trusted_checkout["with"]["ref"] == (
            "${{ github.event.pull_request.base.sha || 'master' }}"
        )
        for step in sanitizer_steps:
            assert ". .trusted-workflow\\.github\\scripts\\sanitize.ps1" in step["run"]
            assert steps.index(trusted_checkout) < steps.index(step)


def test_fast_mode_omits_slow_spectral_setup_and_adds_smoke_tests():
    setup = tool_step("Install spectral preview Python dependencies")
    ctest = tool_step("Run CTest tool suites")
    cmake = (ROOT / "Build/Cmake/Testing/CMakeLists.txt").read_text(
        encoding="utf-8"
    )

    assert setup["if"] == "${{ inputs.ctest_mode == 'full' }}"
    assert re.search(
        r"iccdev\.spectral-tiff-preview\s+120\s+\"[^\"]*;slow\"",
        cmake,
    )
    assert re.search(
        r"fast\)\s+ctest_args\+=\(--smoke --label-exclude slow "
        r"--label-exclude calculator\)",
        ctest["run"],
    )
    assert "ICCDEV_SPECTRAL_PREVIEW_PYTHON" not in ctest.get("env", {})


def test_image_reuse_keeps_pinned_requirements_and_brotli_checks():
    run = tool_step("Install spectral preview Python dependencies")["run"]
    dockerfile = (ROOT / "Dockerfile").read_text(encoding="ascii")

    assert "/opt/iccdev-spectral-preview/bin/python" in dockerfile
    assert "/workspace/iccDEV/.github/ci/requirements/docker-spectral-preview.txt" in dockerfile
    assert 'cmp -s "$checkout_requirements" "$image_requirements"' in run
    assert 'git -c safe.directory=/workspace/iccDEV -C /workspace/iccDEV rev-parse HEAD' in run
    assert '[[ "$image_revision" =~ ^[0-9a-f]{40}$ ]]' in run
    assert "importlib.metadata.version(package)" in run
    assert 'importlib.util.find_spec("imagecodecs._brotli") is not None' in run
    assert "-name 'libbrotlidec*'" in run
    assert "--no-binary=pillow,imagecodecs" in run
    assert 'verify_preview_runtime "$selected_python" "$checkout_requirements"' in run
    assert 'ICCDEV_SPECTRAL_PREVIEW_PYTHON=%s' in run


@pytest.mark.skipif(os.name == "nt" or shutil.which("bash") is None,
                    reason="Bash summary probe requires Unix execution")
@pytest.mark.parametrize("mode", ["fast", "full"])
@pytest.mark.parametrize("recent_limit", ["0", "10"])
def test_summary_commands_preserve_smoke_selection(mode, recent_limit):
    run = tool_step("Generate summary")["run"]
    selections = list(re.finditer(
        r'case "\$CTEST_MODE" in.*?^\s*esac', run, re.MULTILINE | re.DOTALL
    ))
    assert len(selections) == 2
    setup = run[selections[0].start():run.index('timing_report=')]
    script = setup + '\nprintf "%s\\n" "$test_cmd"\n' + selections[1].group()
    result = subprocess.run(
        ["bash", "--noprofile", "--norc"],
        input=script,
        env=dict(
            os.environ,
            BASH_ENV="/dev/null",
            CTEST_MODE=mode,
            CTEST_RECENT_LIMIT=recent_limit,
            EXCLUDE_EXTENDED_PR_TESTS="false",
            INCLUDE_INFRASTRUCTURE_TESTS="true",
        ),
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    commands = [
        line for line in result.stdout.splitlines()
        if "run-recent-ctests.py" in line
    ]
    assert len(commands) == 2
    for command in commands:
        assert ("--smoke" in command.split()) == (mode == "fast")
        assert ("--limit" in command.split()) == (recent_limit != "0")


@pytest.mark.skipif(os.name == "nt" or shutil.which("bash") is None,
                    reason="Bash fixture probe requires Unix execution")
@pytest.mark.parametrize(
    ("case", "same_requirements", "brotli_version", "image_head", "reuse"),
    [
        ("matching image", True, "1.2.0-3ubuntu0.1", "a" * 40, True),
        ("changed requirements", False, "1.2.0-3ubuntu0.1", "a" * 40, False),
        ("stale Brotli", True, "1.2.0-3ubuntu0.0", "a" * 40, False),
        ("wrong image revision", True, "1.2.0-3ubuntu0.1", "b" * 40, False),
    ],
)
def test_spectral_environment_selection_falls_back_on_mismatch(
    tmp_path, case, same_requirements, brotli_version, image_head, reuse
):
    run = tool_step("Install spectral preview Python dependencies")["run"]
    checkout_requirements = tmp_path / "checkout-requirements.txt"
    image_requirements = tmp_path / "image-requirements.txt"
    checkout_requirements.write_text("numpy==2.5.4\n", encoding="ascii")
    image_requirements.write_text(
        "numpy==2.5.4\n" if same_requirements else "numpy==2.5.3\n",
        encoding="ascii",
    )
    image_python = tmp_path / "image" / "bin" / "python"
    image_python.parent.mkdir(parents=True)
    (image_python.parent.parent / "lib").mkdir()
    image_python.write_text("#!/bin/sh\nexit 0\n", encoding="ascii")
    image_python.chmod(0o755)
    fallback_venv = tmp_path / "fallback"
    github_env = tmp_path / "github-env"
    calls = tmp_path / "calls"

    run = run.replace(
        "checkout_requirements=.github/ci/requirements/docker-spectral-preview.txt",
        'checkout_requirements="$TEST_CHECKOUT_REQUIREMENTS"',
    ).replace(
        "image_requirements=/workspace/iccDEV/.github/ci/requirements/docker-spectral-preview.txt",
        'image_requirements="$TEST_IMAGE_REQUIREMENTS"',
    ).replace(
        "image_python=/opt/iccdev-spectral-preview/bin/python",
        'image_python="$TEST_IMAGE_PYTHON"',
    )
    mock_commands = """
git() {
  if [[ "$*" == *"rev-parse HEAD" ]]; then
    printf '%s\\n' "$TEST_IMAGE_HEAD"
  fi
}
dpkg-query() {
  case "$3" in
    libbrotli1|libbrotli-dev) printf '%s' "$TEST_BROTLI_VERSION" ;;
    libfreetype-dev) printf '%s' '2.14.2+dfsg-1ubuntu0.2' ;;
    *) return 2 ;;
  esac
}
apt-get() {
  printf 'apt-get %s\\n' "$*" >> "$TEST_CALLS"
  if [ "$1" = 'install' ]; then
    TEST_BROTLI_VERSION='1.2.0-3ubuntu0.1'
  fi
}
python3() {
  test "$1" = '-m' && test "$2" = 'venv' || return 2
  mkdir -p "$3/bin" "$3/lib"
  cp "$TEST_IMAGE_PYTHON" "$3/bin/python"
}
"""
    env = dict(
        os.environ,
        BASH_ENV="/dev/null",
        ICCDEV_SOURCE_REVISION="a" * 40,
        TEST_IMAGE_HEAD=image_head,
        TEST_BROTLI_VERSION=brotli_version,
        TEST_CHECKOUT_REQUIREMENTS=str(checkout_requirements),
        TEST_IMAGE_REQUIREMENTS=str(image_requirements),
        TEST_IMAGE_PYTHON=str(image_python),
        TEST_CALLS=str(calls),
        SPECTRAL_PREVIEW_VENV=str(fallback_venv),
        GITHUB_ENV=str(github_env),
    )
    result = subprocess.run(
        ["bash", "--noprofile", "--norc"],
        input=mock_commands + run,
        env=env,
        cwd=tmp_path,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, f"{case}: {result.stdout}\n{result.stderr}"
    selected = image_python if reuse else fallback_venv / "bin" / "python"
    assert github_env.read_text(encoding="ascii") == (
        f"ICCDEV_SPECTRAL_PREVIEW_PYTHON={selected}\n"
    )
    assert calls.exists() != reuse
