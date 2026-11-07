# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""WASM and shared workflow contract regression tests."""

import os
import re
import subprocess
from pathlib import Path

import pytest
import yaml


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    ("workflow_name", "job_name"),
    [
        ("ci-latest-release.yml", "wasm"),
        ("ci-pr-wasm.yml", "wasm-build-and-validate"),
    ],
)
def test_wasm_archives_use_bounded_retries_and_checksums(workflow_name, job_name):
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows" / workflow_name).read_text(encoding="ascii")
    )
    steps = workflow["jobs"][job_name]["steps"]
    build = next(
        step["run"] for step in steps
        if step["name"].startswith("Build third-party dependencies")
    )
    assert "curl -sLO" not in build
    for archive, url, checksum in (
        (
            "libpng-1.6.57.tar.gz",
            "https://download.sourceforge.net/libpng/libpng-1.6.57.tar.gz",
            "01fdab044cdc575c74a9ef62a4042fbe24fea8bedfe8aa5c89f4467b781bbb60",
        ),
        (
            "jpegsrc.v9e.tar.gz",
            "https://ijg.org/files/jpegsrc.v9e.tar.gz",
            "4077d6a6a75aeb01884f708919d25934c93305e49f7e3f36db9129320e6f4f3d",
        ),
    ):
        command_end = build.index(url) + len(url)
        command_start = build.rfind(
            "curl --fail --location --silent --show-error", 0, command_end
        )
        assert command_start >= 0
        command = build[command_start:command_end]
        assert "--retry 5 --retry-all-errors --retry-delay 5 --retry-max-time 180" in command
        assert "--connect-timeout 30 --max-time 60" in command
        assert f"--output {archive}" in command
        assert f"{checksum}  {archive}" in build


def test_release_publication_is_not_cancelled():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )

    assert workflow["concurrency"]["cancel-in-progress"] is False


def test_release_windows_build_supports_manifest_and_pinned_dependencies():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )
    triggers = workflow.get("on", workflow.get(True))
    source_input = triggers["workflow_dispatch"]["inputs"]["windows_vcpkg_source"]
    assert source_input["default"] == "manifest"
    assert source_input["options"] == ["manifest", "pinned-release-bundle"]
    windows_steps = {
        step["name"]: step for step in workflow["jobs"]["windows"]["steps"]
    }
    build = windows_steps["Install manifest dependencies and build"]["run"]
    pinned, manifest = build.split(
        "} elseif ($env:WINDOWS_VCPKG_SOURCE -eq 'manifest') {", 1
    )
    manifest = manifest.split("} else {", 1)[0]
    assert "if ($env:WINDOWS_VCPKG_SOURCE -eq 'pinned-release-bundle') {" in pinned
    assert "vcpkg-win-23241-libheif-deps" in pinned
    assert "$actualSha256 -ne $assetSha256" in pinned
    assert 'throw "Pinned vcpkg release asset SHA256 mismatch:' in pinned
    assert "'-DVCPKG_MANIFEST_MODE=OFF'" in pinned
    assert "vcpkg integrate install" in manifest
    assert "vcpkg install" in manifest
    assert "(Get-Command vcpkg -CommandType Application -ErrorAction Stop).Source" in manifest
    assert "$env:VCPKG_ROOT = Split-Path -Parent $vcpkgExecutable" in manifest
    assert "Join-Path $env:VCPKG_ROOT 'scripts\\buildsystems\\vcpkg.cmake'" in build
    assert "vcpkg toolchain file not found: $vcpkgToolchain" in build
    assert "cmake --preset vs2022-x64 -B msvc -S Build/Cmake" in build
    assert "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" in build
    assert "cmake --build msvc --config Release --parallel" in build
    assert "vcpkg-win-" not in manifest
    assert "iccdev-windows-vcpkg-classic" not in manifest
    assert "VCPKG_MANIFEST_MODE=OFF" not in manifest
    assert 'throw "Unsupported Windows vcpkg source:' in build
    assert 'fullfile(pwd, \'msvc\')' in windows_steps["Build MATLAB MEX gateway"]["with"]["command"]
    assert '"msvc/bin/Release"' in windows_steps["Stage bundle into Testing directory"]["run"]


def test_windows_workflows_pin_one_vcpkg_export():
    pin = re.compile(
        r"(https://github\.com/InternationalColorConsortium/iccDEV/releases/download/"
        r"vcpkg-win-[\w.-]+/iccdev-windows-vcpkg-[\w.-]+\.zip)"
        r"[\s\S]{0,400}?\b([0-9a-fA-F]{64})\b"
    )
    pins = {}
    for workflow in sorted((ROOT / ".github/workflows").glob("*.yml")):
        text = workflow.read_text(encoding="utf-8")
        found = {(url, digest.lower()) for url, digest in pin.findall(text)}
        if workflow.name == "ci-latest-release.yml":
            asset_name = re.search(r"\$assetName = '([^']+)'", text)
            asset_uri = re.search(r'\$assetUri = "([^"]+)"', text)
            asset_sha256 = re.search(r"\$assetSha256 = '([0-9a-fA-F]{64})'", text)
            assert asset_name and asset_uri and asset_sha256
            found.add(
                (
                    asset_uri.group(1).replace("$assetName", asset_name.group(1)),
                    asset_sha256.group(1).lower(),
                )
            )
        if found:
            pins[workflow.name] = found

    assert {
        "_build-test-windows.yml",
        "ci-latest-release.yml",
        "ci-matlab.yml",
        "ci-pr-win.yml",
    } <= set(pins)
    assert len(set().union(*pins.values())) == 1, pins


def test_preflight_cache_policy_keeps_trusted_base_boundary():
    script = (ROOT / ".github/scripts/preflight-safety-checks.sh").read_text(
        encoding="ascii"
    )
    assert 'CACHE_POLICY_CHECKER="$SCRIPT_DIR/check-workflow-cache-policy.sh"' in script
    assert '"$CACHE_POLICY_CHECKER" "${workflow_files[@]}"' in script
    assert (
        '.github/scripts/check-workflow-cache-policy.sh "${workflow_files[@]}"'
        not in script
    )


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    ("workflow", "allowed", "diagnostic"),
    [
        (
            "steps:\n  - uses: actions/cache@v4\n",
            False,
            "actions/cache must use restore or save pinned to a lowercase commit SHA",
        ),
        (
            "steps:\n  - uses: msys2/setup-msys2@v2\n",
            False,
            "msys2/setup-msys2 cache not disabled",
        ),
        (
            "steps:\n  - uses: msys2/setup-msys2@v2\n    with:\n      cache: false\n",
            True,
            "[OK] Cache policy",
        ),
        (
            "steps:\n  - uses: actions/setup-python@v5\n    with:\n      cache: pip\n",
            False,
            "cache: pip",
        ),
        (
            "steps:\n  - run: |\n      echo 'uses: actions/cache@v4'\n",
            True,
            "[OK] Cache policy",
        ),
    ],
)
def test_cache_policy_scanner(workflow, allowed, diagnostic, tmp_path):
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert (result.returncode == 0) is allowed, output
    assert diagnostic in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    ("operation", "condition", "allowed", "diagnostic"),
    [
        ("restore", "", True, "[OK] Cache policy"),
        (
            "save",
            "        if: github.event_name == 'workflow_dispatch' "
            "&& steps.dependency-archive.outputs.cache-hit != 'true'\n",
            True,
            "[OK] Cache policy",
        ),
        (
            "save",
            "        if: github.event_name == 'pull_request'\n",
            False,
            "cache save condition includes an untrusted event",
        ),
        (
            "save",
            "        if: github.event_name == 'workflow_dispatch' || true\n",
            False,
            "cache save condition must contain only trusted-event equality checks "
            "and a restore miss check",
        ),
        (
            "save",
            "        if: github.event_name == 'workflow_dispatch'\n",
            False,
            "cache save condition must contain only trusted-event equality checks "
            "and a restore miss check",
        ),
        (
            "save",
            "        if: github.event_name == 'push' || "
            "github.event_name == 'workflow_dispatch' "
            "&& steps.dependency-archive.outputs.cache-hit != 'true'\n",
            False,
            "cache save condition must contain only trusted-event equality checks "
            "and a restore miss check",
        ),
        (
            "save",
            "        if: (github.event_name == 'push' || "
            "github.event_name == 'workflow_dispatch') "
            "&& steps.dependency-archive.outputs.cache-hit != 'true'\n",
            True,
            "[OK] Cache policy",
        ),
    ],
)
def test_cache_policy_scanner_allows_only_verified_archives(
    operation, condition, allowed, diagnostic, tmp_path
):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        "permissions:\n"
        "  contents: read\n"
        "on:\n"
        "  workflow_dispatch:\n"
        "jobs:\n"
        "  test:\n"
        "    steps:\n"
        "      - name: Restore archive\n"
        "        id: dependency-archive\n"
        f"        uses: actions/cache/restore@{action_pin}\n"
        "        with:\n"
        "          path: ${{ runner.temp }}/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        "      - name: Verify archive\n"
        "        run: |\n"
        "          set -euo pipefail\n"
        "          ARCHIVE=dependencies.zip\n"
        f"          ARCHIVE_SHA256={archive_digest}\n"
        "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
    )
    if operation == "save":
        workflow += (
            "      - name: Save archive\n"
            f"{condition}"
            f"        uses: actions/cache/save@{action_pin}\n"
            "        with:\n"
            "          path: ${{ runner.temp }}/dependencies.zip\n"
            f"          key: iccdev-dependencies-{archive_digest}\n"
        )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert (result.returncode == 0) is allowed, output
    assert diagnostic in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
def test_cache_policy_scanner_keeps_inline_save_condition_in_its_step(tmp_path):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        "permissions:\n"
        "  contents: read\n"
        "on:\n"
        "  workflow_dispatch:\n"
        "jobs:\n"
        "  test:\n"
        "    steps:\n"
        "      - uses: actions/cache/restore@"
        f"{action_pin}\n"
        "        id: dependency-archive\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        "      - name: Verify archive\n"
        "        run: |\n"
        "          set -euo pipefail\n"
        "          ARCHIVE=/tmp/dependencies.zip\n"
        f"          ARCHIVE_SHA256={archive_digest}\n"
        "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
        "      - name: Trusted preceding condition\n"
        "        if: github.event_name == 'workflow_dispatch' "
        "&& steps.dependency-archive.outputs.cache-hit != 'true'\n"
        "        run: echo ready\n"
        "      - uses: actions/cache/save@"
        f"{action_pin}\n"
        "        if: github.event_name == 'pull_request'\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert "cache save condition includes an untrusted event" in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    "verification",
    [
        (
            "      - name: Verify unrelated archive\n"
            "        run: |\n"
            "          set -euo pipefail\n"
            "          ARCHIVE=unrelated.zip\n"
            "          ARCHIVE_SHA256=" + "a" * 64 + "\n"
            "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
        ),
        (
            "      - name: Use before verification\n"
            "        run: |\n"
            "          set -euo pipefail\n"
            "          ARCHIVE=dependencies.zip\n"
            "          ARCHIVE_SHA256=" + "a" * 64 + "\n"
            "          unzip \"$ARCHIVE\"\n"
            "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
        ),
    ],
)
def test_cache_policy_scanner_rejects_unbound_or_late_verification(
    verification, tmp_path
):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        "permissions:\n"
        "  contents: read\n"
        "on:\n"
        "  workflow_dispatch:\n"
        "jobs:\n"
        "  test:\n"
        "    steps:\n"
        "      - id: dependency-archive\n"
        f"        uses: actions/cache/restore@{action_pin}\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        f"{verification}"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert "cached archive SHA-256 must be verified before extraction or saving" in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    (
        "verification_header",
        "verification_control",
        "extraction_header",
        "extraction_control",
    ),
    [
        (
            "      - name: Verify archive\n",
            "        if: steps.dependency-archive.outputs.cache-hit != 'true'\n",
            "      - name: Extract archive\n",
            "",
        ),
        (
            "      - name: Verify archive\n",
            "        continue-on-error: true\n",
            "      - name: Extract archive\n",
            "",
        ),
        (
            "      - name: Verify archive\n",
            "",
            "      - name: Extract archive\n",
            "        if: always()\n",
        ),
        (
            "      - if: steps.dependency-archive.outputs.cache-hit != 'true'\n",
            "",
            "      - name: Extract archive\n",
            "",
        ),
        (
            "      - continue-on-error: true\n",
            "",
            "      - name: Extract archive\n",
            "",
        ),
        (
            "      - if: >-\n"
            "          steps.dependency-archive.outputs.cache-hit != 'true'\n",
            "",
            "      - name: Extract archive\n",
            "",
        ),
        (
            "      - name: Verify archive\n",
            "",
            "      - if: always()\n",
            "",
        ),
    ],
)
def test_cache_policy_scanner_requires_mandatory_verification(
    verification_header,
    verification_control,
    extraction_header,
    extraction_control,
    tmp_path,
):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        "permissions:\n"
        "  contents: read\n"
        "on:\n"
        "  workflow_dispatch:\n"
        "jobs:\n"
        "  test:\n"
        "    steps:\n"
        "      - id: dependency-archive\n"
        f"        uses: actions/cache/restore@{action_pin}\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        f"{verification_header}"
        f"{verification_control}"
        "        run: |\n"
        "          set -euo pipefail\n"
        "          ARCHIVE=/tmp/dependencies.zip\n"
        f"          ARCHIVE_SHA256={archive_digest}\n"
        "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
        f"{extraction_header}"
        f"{extraction_control}"
        "        run: unzip /tmp/dependencies.zip\n"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert "cached archive SHA-256 must be verified before extraction or saving" in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
def test_cache_policy_scanner_requires_verification_before_save(tmp_path):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        "permissions:\n"
        "  contents: read\n"
        "on:\n"
        "  workflow_dispatch:\n"
        "jobs:\n"
        "  test:\n"
        "    steps:\n"
        "      - id: dependency-archive\n"
        f"        uses: actions/cache/restore@{action_pin}\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        "      - if: github.event_name == 'workflow_dispatch' "
        "&& steps.dependency-archive.outputs.cache-hit != 'true'\n"
        f"        uses: actions/cache/save@{action_pin}\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        "      - name: Verify too late\n"
        "        run: |\n"
        "          set -euo pipefail\n"
        "          ARCHIVE=/tmp/dependencies.zip\n"
        f"          ARCHIVE_SHA256={archive_digest}\n"
        "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert "cached archive SHA-256 must be verified before extraction or saving" in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    ("trigger", "cache_from", "cache_to", "allowed"),
    [
        (
            "  workflow_dispatch:\n",
            "${{ inputs.use_build_cache && steps.cache.outputs.source || '' }}",
            "${{ inputs.use_build_cache && env.PUBLISH_IMAGE == 'true' "
            "&& 'type=inline' || '' }}",
            True,
        ),
        ("  workflow_dispatch:\n", "type=gha", "type=inline", False),
        (
            "  pull_request_target:\n  workflow_dispatch:\n",
            "${{ inputs.use_build_cache && steps.cache.outputs.source || '' }}",
            "${{ inputs.use_build_cache && env.PUBLISH_IMAGE == 'true' "
            "&& 'type=inline' || '' }}",
            False,
        ),
    ],
)
def test_cache_policy_scanner_allows_only_validated_image_cache(
    trigger, cache_from, cache_to, allowed, tmp_path
):
    workflow = (
        "'on':\n"
        f"{trigger}"
        "jobs:\n"
        "  image:\n"
        "    steps:\n"
        "      - name: Resolve validated image cache\n"
        "        id: cache\n"
        "        run: |\n"
        "          cache_digest=sha256:"
        + "a" * 64
        + "\n"
        "          docker buildx imagetools inspect image:latest\n"
        "          if [[ ! \"$cache_digest\" =~ ^sha256:[0-9a-f]{64}$ ]]; then exit 1; fi\n"
        "          printf 'source=type=registry,ref=%s@%s\\n' image \"$cache_digest\"\n"
        "      - uses: docker/build-push-action@"
        + "0" * 40
        + "\n"
        "        with:\n"
        "          no-cache: ${{ !inputs.use_build_cache || "
        "steps.cache.outputs.source == '' }}\n"
        f"          cache-from: {cache_from}\n"
        f"          cache-to: {cache_to}\n"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert (result.returncode == 0) is allowed, output
    if allowed:
        assert "[OK] Cache policy" in output
    else:
        assert "digest-pinned validated-image inline-cache contract" in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    ("header", "diagnostic"),
    [
        (
            "on:\n  pull_request_target:\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "permissions:\n  contents: write\non:\n  workflow_dispatch:\n",
            "actions/cache is forbidden in a write-capable workflow",
        ),
        (
            "permissions: {contents: write}\non:\n  workflow_dispatch:\n",
            "actions/cache is forbidden in a write-capable workflow",
        ),
        (
            "permissions: write-all\non:\n  workflow_dispatch:\n",
            "actions/cache is forbidden in a write-capable workflow",
        ),
        (
            "on: [pull_request_target]\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "on:\n  pull_request_target: {}\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "on:\n  'workflow_run': {workflows: [CI], types: [completed]}\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "on: {workflow_run: {workflows: [CI], types: [completed]}}\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "on: pull_request_target\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "on:\n  - issues\n",
            "actions/cache is forbidden on low-trust triggers",
        ),
        (
            "on:\n  workflow_dispatch:\n",
            "cached archive SHA-256 must be verified before extraction or saving",
        ),
    ],
)
def test_cache_policy_scanner_rejects_unsafe_cache(
    header, diagnostic, tmp_path
):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        f"{header}"
        "jobs:\n"
        "  test:\n"
        "    steps:\n"
        "      - uses: actions/cache/restore@"
        f"{action_pin}\n"
        "        with:\n"
        "          path: /tmp/dependencies.tar.gz\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert diagnostic in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
@pytest.mark.parametrize(
    ("permissions", "trigger", "job_name"),
    [
        (
            "permissions:\n  issues: read\n",
            "on:\n  workflow_dispatch:\n    inputs:\n      issues:\n"
            "        required: false\n        type: string\n",
            "test",
        ),
        (
            "permissions:\n  'issues': read\n",
            "on:\n  workflow_dispatch:\n",
            "test",
        ),
        (
            "permissions: {issues: read}\n",
            "on: {workflow_dispatch: {inputs: {issues: {required: false}}}}\n",
            "test",
        ),
        (
            "permissions: {contents: read}\n",
            "on: workflow_dispatch\n",
            "issues",
        ),
        (
            "permissions: {contents: read}\n",
            "on: {\n  workflow_dispatch: {\n    inputs: "
            "{issues: {required: false}}\n  }\n}\n",
            "test",
        ),
    ],
)
def test_cache_policy_scanner_scopes_trigger_names_to_on_mapping(
    permissions, trigger, job_name, tmp_path
):
    action_pin = "0" * 40
    archive_digest = "a" * 64
    workflow = (
        f"{permissions}"
        f"{trigger}"
        "jobs:\n"
        f"  {job_name}:\n"
        "    steps:\n"
        "      - id: dependency-archive\n"
        f"        uses: actions/cache/restore@{action_pin}\n"
        "        with:\n"
        "          path: /tmp/dependencies.zip\n"
        f"          key: iccdev-dependencies-{archive_digest}\n"
        "      - name: Verify archive\n"
        "        run: |\n"
        "          set -euo pipefail\n"
        "          ARCHIVE=/tmp/dependencies.zip\n"
        f"          ARCHIVE_SHA256={archive_digest}\n"
        "          printf '%s  %s\\n' \"$ARCHIVE_SHA256\" \"$ARCHIVE\" | sha256sum -c -\n"
    )
    fixture = tmp_path / "cache-policy.yml"
    fixture.write_text(workflow, encoding="utf-8")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode == 0, output
    assert "[OK] Cache policy" in output


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
def test_cache_policy_scanner_fails_closed_on_decode_error(tmp_path):
    fixture = tmp_path / "invalid-utf8.yml"
    fixture.write_bytes(b"\xff")
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(fixture)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode != 0
    assert "[OK] Cache policy" not in result.stdout


@pytest.mark.skipif(os.name == "nt", reason="POSIX Bash and path syntax required")
def test_cache_policy_scanner_checks_each_workflow_in_directory(tmp_path):
    (tmp_path / "allowed.yml").write_text("steps:\n  - run: echo ok\n", encoding="utf-8")
    (tmp_path / "forbidden.yaml").write_text(
        "steps:\n  - uses: actions/cache@v4\n", encoding="utf-8"
    )
    result = subprocess.run(
        ["bash", str(ROOT / ".github/scripts/check-workflow-cache-policy.sh"), str(tmp_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode != 0
    assert "[OK] Cache policy:" in result.stdout
    assert "actions/cache must use restore or save pinned to a lowercase commit SHA" in result.stderr


def test_docker_and_tool_tests_workflow_pin_fixed_brotli_version():
    dockerfile = (ROOT / "Dockerfile").read_text(encoding="ascii")
    docker_version = re.search(
        r"^ARG BROTLI_VERSION=([^\s]+)$", dockerfile, re.MULTILINE
    )
    assert docker_version is not None
    version = docker_version.group(1)
    assert version == "1.2.0-3ubuntu0.1"
    assert "libbrotli1=${BROTLI_VERSION}" in dockerfile
    assert "libbrotli-dev=${BROTLI_VERSION}" in dockerfile

    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-iccdev-tool-tests.yml").read_text(
            encoding="ascii"
        )
    )
    steps = [
        step
        for job in workflow["jobs"].values()
        for step in job.get("steps", [])
        if step.get("name") == "Install spectral preview Python dependencies"
    ]
    assert len(steps) == 1
    workflow_versions = dict(
        re.findall(
            r"^\s+(libbrotli1|libbrotli-dev)=([^\s\\]+)",
            steps[0]["run"],
            re.MULTILINE,
        )
    )
    assert workflow_versions == {
        "libbrotli1": version,
        "libbrotli-dev": version,
    }


def test_ubuntu_latest_build_matrix_supports_26_04_migration():
    matrix = yaml.safe_load(
        (ROOT / ".github/workflows/_build-matrix.yml").read_text(
            encoding="ascii"
        )
    )
    standard_builds = matrix["jobs"]["standard-builds"]
    assert "ubuntu-latest" in standard_builds["strategy"]["matrix"]["os"]
    assert (
        standard_builds["with"]["run-full-tests"]
        == "${{ matrix.os == 'ubuntu-latest' }}"
    )

    unix_workflow = yaml.safe_load(
        (ROOT / ".github/workflows/_build-test-unix.yml").read_text(
            encoding="ascii"
        )
    )
    dependency_step = next(
        step
        for step in unix_workflow["jobs"]["build-test"]["steps"]
        if step.get("name") == "Install system dependencies (Linux)"
    )
    assert "24.04) brotli_fixed=1.1.0-2ubuntu0.1 ;;" in dependency_step["run"]
    assert "26.04) brotli_fixed=1.2.0-3ubuntu0.1 ;;" in dependency_step["run"]
    assert (
        "Unsupported Ubuntu release for Brotli security check"
        in dependency_step["run"]
    )


def test_wasm_workflow_regression_runs_in_shared_wheel_gate():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/_build-python-wheels.yml").read_text(
            encoding="ascii"
        )
    )
    steps = workflow["jobs"]["wheels"]["steps"]
    test_step = next(
        step for step in steps if step["name"] == "Build and test standalone wheels"
    )

    assert ".github/tests/test_wasm_workflow.py" in test_step["run"]


def test_release_uses_opt_in_npm_trusted_publishing():
    workflow = yaml.safe_load(
        (ROOT / ".github/workflows/ci-latest-release.yml").read_text(encoding="ascii")
    )
    dispatch = workflow["on"]["workflow_dispatch"]["inputs"]
    assert dispatch["npm_packages"] == {
        "description": "Validated npm distributions to publish with OIDC",
        "required": True,
        "default": "none",
        "type": "choice",
        "options": ["none", "iccdev"],
    }
    assert dispatch["npm_version"]["type"] == "string"
    assert dispatch["npm_dist_tag"] == {
        "description": "npm dist-tag for the selected distribution",
        "required": True,
        "default": "latest",
        "type": "string",
    }

    wasm_steps = {
        step["name"]: step for step in workflow["jobs"]["wasm"]["steps"]
    }
    stage_wasm = wasm_steps["Stage npm-shaped WASM bundle (issue #830)"]
    assert stage_wasm["env"]["NPM_PACKAGES"] == "${{ inputs.npm_packages }}"
    assert stage_wasm["env"]["NPM_VERSION"] == "${{ inputs.npm_version }}"
    assert stage_wasm["env"]["NPM_DIST_TAG"] == "${{ inputs.npm_dist_tag }}"
    assert 'VERSION="$NPM_VERSION"' in stage_wasm["run"]
    assert 'npm_version must be a SemVer version without build metadata' in stage_wasm["run"]
    assert (
        r"^[0-9]+(\.[0-9]+){2}(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$"
        in stage_wasm["run"]
    )
    assert 'Prerelease npm_version values require a non-latest npm_dist_tag' in stage_wasm["run"]

    release = workflow["jobs"]["release"]
    assert release["permissions"]["id-token"] == "write"
    release_step_names = [step["name"] for step in release["steps"]]
    steps = {step["name"]: step for step in release["steps"]}
    setup_node = steps["Set up Node.js for npm publication"]
    assert re.fullmatch(r"actions/setup-node@[0-9a-f]{40}", setup_node["uses"])
    assert setup_node["with"]["node-version"] == "24"
    assert setup_node["with"]["package-manager-cache"] is False

    stage = steps["Stage validated npm distribution"]
    assert "artifacts/iccdev-wasm-release/package.json" in stage["run"]
    assert 'npm pack --dry-run --json' in stage["run"]
    assert 'npm pack --pack-destination "$GITHUB_WORKSPACE/npm-iccdev"' in stage["run"]

    publish = steps["Publish iccdev to npm"]
    assert publish["id"] == "npm_iccdev"
    assert 'npm publish "./$tarball" --provenance --tag "$NPM_DIST_TAG"' in publish["run"]
    assert "unset NODE_AUTH_TOKEN || true" in publish["run"]
    assert "NODE_AUTH_TOKEN" not in publish["env"]

    smoke = steps["Verify public iccdev npm package"]
    assert smoke["continue-on-error"] is True
    assert "!cancelled()" in smoke["if"]
    assert "steps.npm_iccdev.outcome == 'success'" in smoke["if"]
    assert "for attempt in {1..30}" in smoke["run"]
    assert 'npm pack --pack-destination "$npm_environment"' in smoke["run"]
    assert "Published npm package tarball was not available after 450 seconds" in smoke["run"]
    assert 'tar -xzf "$tarball" --strip-components=1' in smoke["run"]
    assert '"/package.json"' in smoke["run"]

    summary = steps["Summary Report"]
    assert summary["env"]["NPM_ICCDEV_OUTCOME"] == "${{ steps.npm_iccdev.outcome }}"
    last_publication = max(
        release_step_names.index(name)
        for name in (
            "Publish iccdev to PyPI",
            "Publish iccdev-mcp to PyPI",
            "Publish iccdev to npm",
        )
    )
    first_verification = min(
        release_step_names.index(name)
        for name in (
            "Verify public iccdev installation",
            "Verify public iccdev-mcp installation",
            "Verify public iccdev npm package",
        )
    )
    assert last_publication < first_verification < release_step_names.index("Summary Report")
