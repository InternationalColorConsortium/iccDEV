#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Validate the checked-in ClusterFuzzLite build and workflow contract.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
workflow="$repo_root/.github/workflows/ci-clusterfuzzlite.yml"
adapter="$repo_root/.clusterfuzzlite/build.sh"
project="$repo_root/.clusterfuzzlite/project.yaml"
dockerfile="$repo_root/.clusterfuzzlite/Dockerfile"

for required in "$workflow" "$adapter" "$project" "$dockerfile"; do
  if [ ! -s "$required" ]; then
    echo "[FAIL] Missing ClusterFuzzLite file: $required" >&2
    exit 1
  fi
done

bash -n "$adapter"
bash -n "$repo_root/.github/ci/cfl/build.sh"
bash -n "$repo_root/.github/scripts/iccdev-afl-smoke.sh"

grep -qx 'language: c++' "$project"
grep -q '^FROM gcr.io/oss-fuzz-base/base-builder@sha256:[0-9a-f]\{64\}$' "$dockerfile"
grep -q '^  workflow_dispatch:$' "$workflow"
grep -q '^      - ci-qa-clusterfuzz$' "$workflow"

for sanitizer in address undefined memory; do
  grep -q "^          - $sanitizer$" "$workflow"
done

grep -q '^targets=( profilevisualize writerserialize )$' "$adapter"
grep -q -- '--targets profilevisualize,writerserialize' "$adapter"
# shellcheck disable=SC2016 # The adapter must retain this literal template.
fuzzer_template='  fuzzer="icc_${target}_fuzzer"'
grep -Fqx "$fuzzer_template" "$adapter"
grep -Fq '21:21|22:22)' "$repo_root/.github/ci/cfl/build.sh"
grep -Fq '21:21|22:22)' "$repo_root/.github/scripts/iccdev-afl-smoke.sh"

if grep -qE 'uses: [^ ]+@(main|master|v[0-9]+)$' "$workflow"; then
  echo "[FAIL] ClusterFuzzLite workflow contains a mutable action reference" >&2
  exit 1
fi

echo "[PASS] ClusterFuzzLite configuration contract"
