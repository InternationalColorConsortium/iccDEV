#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Verify that core PR, sanitizer, risk, and infrastructure lanes stay separate.
###############################################################################
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. In the absence of prior written permission, the names "ICC" and "The
#    International Color Consortium" must not be used to imply that the ICC
#    organization endorses or promotes products derived from this software.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# This software consists of voluntary contributions made by many individuals on
# behalf of the International Color Consortium. Membership in the ICC is
# encouraged when this software is used for commercial purposes. For more
# information, see <http://www.color.org/>.

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
pr_workflow="$repo_root/.github/workflows/ci-pr-action.yml"
mcp_workflow="$repo_root/.github/workflows/ci-mcp-server.yml"
tool_workflow="$repo_root/.github/workflows/ci-iccdev-tool-tests.yml"
pr_unix_workflow="$repo_root/.github/workflows/ci-pr-unix.yml"
apple_workflow="$repo_root/.github/workflows/ci-apple-platform-smoke.yml"
unix_workflow="$repo_root/.github/workflows/_build-test-unix.yml"
windows_workflow="$repo_root/.github/workflows/_build-test-windows.yml"
pr_windows_workflow="$repo_root/.github/workflows/ci-pr-win.yml"
sanitizer_workflow="$repo_root/.github/workflows/ci-sanitizer-regression.yml"
risk_workflow="$repo_root/.github/workflows/ci-risk-analysis-gate.yml"
cfl_workflow="$repo_root/.github/workflows/ci-clusterfuzzlite.yml"
ctest_file="$repo_root/Build/Cmake/Testing/CMakeLists.txt"

# Fast-lane tool tests use sanitized Release; the full PR route uses core Debug.
tool_tests_job="$(awk '
  /^  tool-tests:$/ { in_job=1; next }
  in_job && /^  [[:alnum:]_-]+:$/ { exit }
  in_job { print }
' "$pr_workflow")"
grep -Fq 'uses: ./.github/workflows/ci-iccdev-tool-tests.yml' <<< "$tool_tests_job"
grep -Fq "tool_build_type: \${{ needs.detect-src.outputs.fast_lane == 'true' && 'Release' || 'Debug' }}" <<< "$tool_tests_job"
grep -Fq "instrumentation: \${{ needs.detect-src.outputs.fast_lane == 'true' && 'sanitizers' || 'core' }}" <<< "$tool_tests_job"

# Trusted-master dispatches share github.ref, and separate PRs may share a
# commit. Reusable lanes need the resolved PR number, exact head, and caller
# run ID so concurrent requests for the same PR/head cannot cancel each other.
require_head_concurrency() {
  local workflow="$1"
  local input_name="$2"
  local group
  group="$(grep -A6 -E '^[[:space:]]+group:' "$workflow")"
  if [[ "$group" != *'inputs.pr_number'* || "$group" != *"inputs.$input_name"* ||
        "$group" != *'github.run_id'* ]]; then
    echo "[FAIL] Missing PR/head/request concurrency identity: $workflow" >&2
    exit 1
  fi
  grep -q '^      pr_number:$' "$workflow"
}
for workflow in "$tool_workflow" "$mcp_workflow" "$pr_windows_workflow"; do
  require_head_concurrency "$workflow" target_sha
done
for workflow in "$pr_unix_workflow" "$apple_workflow"; do
  require_head_concurrency "$workflow" target-sha
done
# A PR event and trusted-master dispatch for the same PR/head may run together.
# shellcheck disable=SC2016 # GitHub expression is literal workflow syntax.
grep -Fq 'group: pr-unix-${{ github.event_name }}-' "$pr_unix_workflow"
# All six ci-pr-action calls to these reusable lanes must pass its resolved PR.
# shellcheck disable=SC2016 # GitHub expression is literal workflow syntax.
test "$(grep -Fc 'pr_number: ${{ needs.detect-src.outputs.pr_number }}' "$pr_workflow")" -eq 6

grep -Fq 'uses: ./.github/workflows/ci-mcp-server.yml' "$pr_workflow"
grep -Fq 'needs.detect-src.outputs.mcp_changed' "$pr_workflow"
grep -Fq 'MCP_RESULT:' "$pr_workflow"
grep -Fq "needs['mcp-server'].result" "$pr_workflow"
grep -q '^  workflow_call:$' "$mcp_workflow"
grep -q '^  workflow_dispatch:$' "$mcp_workflow"
if grep -q '^  pull_request:$' "$mcp_workflow"; then
  echo "[FAIL] MCP PR checks must be routed only by ci-pr-action" >&2
  exit 1
fi
# shellcheck disable=SC2016 # GitHub expression is literal workflow syntax.
test "$(grep -Fc 'ref: ${{ inputs.target_sha || github.event.pull_request.head.sha || github.sha }}' "$mcp_workflow")" -eq 2
if grep -Fq 'ci-pr-risk-security-analysis.yml' "$pr_workflow"; then
  echo "[FAIL] ci-pr-action must not call risk analysis" >&2
  exit 1
fi

grep -Fq 'default: sanitizers' "$tool_workflow"
grep -Fq 'ctest_args+=(--label-exclude ci-infrastructure --label-exclude clusterfuzzlite)' "$tool_workflow"
if grep -Fq 'include_infrastructure_tests:' "$tool_workflow"; then
  echo "[FAIL] The tool lane must not expose infrastructure CTests" >&2
  exit 1
fi
test "$(grep -Fc -- "--label-exclude 'slow|clusterfuzzlite'" "$unix_workflow")" -eq 2
grep -Fq -- "--label-exclude 'known-red|clusterfuzzlite'" "$windows_workflow"
test "$(grep -Fc -- '--label-exclude clusterfuzzlite' "$pr_windows_workflow")" -eq 1
test "$(grep -Fc -- "'--label-exclude', 'clusterfuzzlite'" "$pr_windows_workflow")" -eq 2
grep -q '^  pull_request:$' "$sanitizer_workflow"
grep -Fq "      - '.github/ci/test-data/**'" "$sanitizer_workflow"
grep -Fq "      - '.github/scripts/**'" "$sanitizer_workflow"
grep -Fq 'uses: ./.github/workflows/ci-iccdev-tool-tests.yml' "$sanitizer_workflow"
grep -Fq 'instrumentation: sanitizers' "$sanitizer_workflow"
grep -Fq 'uses: ./.github/workflows/ci-pr-risk-security-analysis.yml' "$risk_workflow"
if grep -q '^    paths:$' "$risk_workflow"; then
  echo "[FAIL] required risk contexts must run for every PR" >&2
  exit 1
fi

grep -q '^  schedule:$' "$cfl_workflow"
grep -Fq 'default: smoke' "$cfl_workflow"
if grep -q '^  push:$' "$cfl_workflow"; then
  echo "[FAIL] ClusterFuzzLite must not run in a PR or push lane" >&2
  exit 1
fi

test "$(grep -c 'clusterfuzzlite.*ci-infrastructure' "$ctest_file")" -eq 2

echo "[PASS] CI lane boundaries"
