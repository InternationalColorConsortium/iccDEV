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
tool_workflow="$repo_root/.github/workflows/ci-iccdev-tool-tests.yml"
sanitizer_workflow="$repo_root/.github/workflows/ci-sanitizer-regression.yml"
risk_workflow="$repo_root/.github/workflows/ci-risk-analysis-gate.yml"
cfl_workflow="$repo_root/.github/workflows/ci-clusterfuzzlite.yml"
ctest_file="$repo_root/Build/Cmake/Testing/CMakeLists.txt"

grep -Fq 'instrumentation: core' "$pr_workflow"
grep -Fq 'include_infrastructure_tests: false' "$pr_workflow"
if grep -Fq 'ci-pr-risk-security-analysis.yml' "$pr_workflow"; then
  echo "[FAIL] ci-pr-action must not call risk analysis" >&2
  exit 1
fi

grep -Fq 'default: sanitizers' "$tool_workflow"
grep -Fq 'ctest_args+=(--label-exclude ci-infrastructure)' "$tool_workflow"
grep -q '^  pull_request:$' "$sanitizer_workflow"
grep -Fq 'uses: ./.github/workflows/ci-iccdev-tool-tests.yml' "$sanitizer_workflow"
grep -Fq 'instrumentation: sanitizers' "$sanitizer_workflow"
grep -Fq 'include_infrastructure_tests: false' "$sanitizer_workflow"
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
