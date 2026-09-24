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
grep -q '^  actions: read$' "$workflow"
grep -q '^  workflow_dispatch:$' "$workflow"
grep -q '^      - ci-qa-clusterfuzz$' "$workflow"
grep -Eq '^        uses: docker://gcr.io/oss-fuzz-base/clusterfuzzlite-build-fuzzers@sha256:[0-9a-f]{64}$' "$workflow"
grep -Eq '^        uses: docker://gcr.io/oss-fuzz-base/clusterfuzzlite-run-fuzzers@sha256:[0-9a-f]{64}$' "$workflow"

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

validate_action_reference() {
  local action_ref="$1"

  if [[ "$action_ref" == ./* ]]; then
    return 0
  fi
  if [[ "$action_ref" =~ ^docker://[^@[:space:]]+@sha256:[0-9a-f]{64}$ ]]; then
    return 0
  fi
  [[ "$action_ref" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(/[^@[:space:]]+)?@[0-9a-f]{40}$ ]]
}

while IFS= read -r action_ref; do
  if ! validate_action_reference "$action_ref"; then
    echo "[FAIL] Mutable or invalid action reference: $action_ref" >&2
    exit 1
  fi
done < <(sed -nE 's/^[[:space:]]*uses:[[:space:]]*([^[:space:]#]+).*$/\1/p' "$workflow")

for mutable_ref in \
  'actions/checkout@v5.0.0' \
  'actions/checkout@main' \
  'actions/checkout@08c6903' \
  'docker://gcr.io/example/action:v1'; do
  if validate_action_reference "$mutable_ref"; then
    echo "[FAIL] Pin validator accepted mutable reference: $mutable_ref" >&2
    exit 1
  fi
done

echo "[PASS] ClusterFuzzLite configuration contract"
