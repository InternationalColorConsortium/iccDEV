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
msan_builder="$repo_root/.github/scripts/iccdev-build-msan-libcxx.sh"
issue_2687_fixture="$repo_root/.github/ci/regression/issue-2687-profile-list-node.icc.base64"
patch_mode_file="$repo_root/.clusterfuzzlite/known-bug-patch-mode"
target_group_file="$repo_root/.clusterfuzzlite/target-group"
patch_dir="$repo_root/.github/ci/fuzz-patches/cfl"
patch_readme="$patch_dir/README.md"
patch_test="$repo_root/.github/scripts/iccdev-fuzz-patch-check-tests.sh"
target_test="$repo_root/.github/scripts/iccdev-clusterfuzzlite-target-tests.sh"
artifact_cleanup="$repo_root/.github/scripts/iccdev-cfl-artifact-cleanup.sh"
artifact_cleanup_test="$repo_root/.github/scripts/iccdev-cfl-artifact-cleanup-tests.sh"

for required in "$workflow" "$adapter" "$project" "$dockerfile" \
  "$msan_builder" "$issue_2687_fixture" "$patch_mode_file" \
  "$target_group_file" "$patch_readme" "$patch_test" "$target_test" \
  "$artifact_cleanup" "$artifact_cleanup_test"; do
  if [ ! -s "$required" ]; then
    echo "[FAIL] Missing ClusterFuzzLite file: $required" >&2
    exit 1
  fi
done

bash -n "$adapter"
bash -n "$repo_root/.github/ci/cfl/build.sh"
bash -n "$repo_root/.github/scripts/iccdev-afl-smoke.sh"
bash -n "$msan_builder"
bash -n "$patch_test"
bash -n "$target_test"
bash -n "$artifact_cleanup"
bash -n "$artifact_cleanup_test"

for issue in 2686 2688 2699 2703 2704 2705 2707; do
  if ! find "$patch_dir" -maxdepth 1 -type f -name "*-issue-$issue-*.patch" \
      -print -quit | grep -q .; then
    echo "[FAIL] Missing individual temporary patch for issue #$issue" >&2
    exit 1
  fi
done
test "$(find "$patch_dir" -maxdepth 1 -type f -name '*.patch' | wc -l)" -eq 9

grep -qx 'language: c++' "$project"
grep -q '^FROM gcr.io/oss-fuzz-base/base-builder@sha256:[0-9a-f]\{64\}$' "$dockerfile"
grep -q '^      libxml2-dev=[^ ]* \\$' "$dockerfile"
grep -q '^      nlohmann-json3-dev=[^ ]* && \\$' "$dockerfile"
grep -q '^  actions: read  # ' "$workflow"
grep -q '^      actions: write  # Remove superseded corpus artifacts from this run\.$' "$workflow"
grep -q '^concurrency:$' "$workflow"
grep -q '^  workflow_dispatch:$' "$workflow"
grep -q '^      fuzz_minutes:$' "$workflow"
grep -q '^        default: 2$' "$workflow"
grep -q '^        type: number$' "$workflow"
grep -q '^      known_bug_patch_mode:$' "$workflow"
grep -q '^        default: patched$' "$workflow"
grep -q '^          - patched$' "$workflow"
grep -q '^          - unpatched$' "$workflow"
grep -q '^      generate_coverage:$' "$workflow"
grep -q '^        type: boolean$' "$workflow"
grep -q '^      - ci-qa-clusterfuzz$' "$workflow"
grep -q '^  configure:$' "$workflow"
grep -q '^    timeout-minutes: 10$' "$workflow"
# shellcheck disable=SC2016 # Match the literal Actions expression.
grep -q '^      fuzz_seconds: \${{ steps.duration.outputs.fuzz_seconds }}$' "$workflow"
test "$(grep -c '^        shell: bash --noprofile --norc {0}$' "$workflow")" -eq 8
grep -q '^          BASH_ENV: /dev/null$' "$workflow"
grep -q '^          git config --global credential.helper ""$' "$workflow"
grep -q '^          unset GITHUB_TOKEN || true$' "$workflow"
# shellcheck disable=SC2016 # Match literal workflow shell variables.
grep -q '^          if \[ "$minutes" -lt 2 \] || \[ "$minutes" -gt 45 \]; then$' "$workflow"
# shellcheck disable=SC2016 # Match the literal validated output write.
grep -q '^          echo "fuzz_seconds=$((minutes \* 60))" >> "$GITHUB_OUTPUT"  # elements-sanitized$' "$workflow"
grep -q '^    needs: configure$' "$workflow"
grep -q '^      max-parallel: 3$' "$workflow"
grep -q '^  prune:$' "$workflow"
grep -q '^  cleanup-artifacts:$' "$workflow"
# shellcheck disable=SC2016 # Match the literal Actions status expression.
test "$(grep -Fc '    if: ${{ !cancelled() &&' "$workflow")" -eq 3
if grep -q '^    if: .*always()' "$workflow"; then
  echo "[FAIL] ClusterFuzzLite cleanup jobs must stop on cancellation" >&2
  exit 1
fi
test "$(grep -c '^      - configure$' "$workflow")" -eq 2
test "$(grep -c '^      - fuzz$' "$workflow")" -eq 1
test "$(grep -c '^      - prune$' "$workflow")" -eq 1
grep -q '^    timeout-minutes: 60$' "$workflow"
grep -q '^          MODE: prune$' "$workflow"
grep -q '^          MODE: coverage$' "$workflow"
test "$(grep -c '^          FUZZ_SECONDS: "960"$' "$workflow")" -eq 1
test "$(grep -c '^          FUZZ_SECONDS: "120"$' "$workflow")" -eq 1
# shellcheck disable=SC2016 # Match the literal Actions expression.
grep -q '^          FUZZ_SECONDS: \${{ needs.configure.outputs.fuzz_seconds }}$' "$workflow"
grep -Eq '^        uses: docker://gcr.io/oss-fuzz-base/clusterfuzzlite-build-fuzzers@sha256:[0-9a-f]{64}$' "$workflow"
grep -Eq '^        uses: docker://gcr.io/oss-fuzz-base/clusterfuzzlite-run-fuzzers@sha256:[0-9a-f]{64}$' "$workflow"

for sanitizer in address undefined memory; do
  grep -q "^          - $sanitizer$" "$workflow"
done
for group in core formats assessment; do
  grep -q "^          - $group$" "$workflow"
done

# The official GitHub action clones GITHUB_SHA before building, so mutations
# made to the checkout are not visible in its builder container. CFL_EXTRA_*
# is the supported forwarding boundary for matrix-specific build settings.
# shellcheck disable=SC2016 # Match literal Actions expressions.
grep -Fq '          CFL_EXTRA_ICCDEV_CFL_TARGET_GROUP: ${{ matrix.group }}' "$workflow"
# shellcheck disable=SC2016 # Match literal Actions expressions.
test "$(grep -Fc '          CFL_EXTRA_ICCDEV_CFL_KNOWN_BUG_PATCH_MODE: ${{ needs.configure.outputs.known_bug_patch_mode }}' "$workflow")" -eq 3
test "$(grep -c '^          CFL_EXTRA_ICCDEV_CFL_TARGET_GROUP: all$' "$workflow")" -eq 2
test "$(grep -c '^      - name: Verify built source and configuration$' "$workflow")" -eq 3
test "$(grep -c '^    name: "Remove superseded corpus artifacts"$' "$workflow")" -eq 1
grep -q '^    needs: prune$' "$workflow"
test "$(grep -c '^      - name: Remove superseded same-run corpus artifacts$' "$workflow")" -eq 1
# shellcheck disable=SC2016 # Match literal GitHub runner variables.
grep -Fq -- '--delete-current-run "$GITHUB_REPOSITORY" "$GITHUB_RUN_ID"' "$workflow"
test "$(grep -c '^          provenance=build-out/iccdev-cfl-build-provenance.txt$' "$workflow")" -eq 3
# shellcheck disable=SC2016 # Match literal workflow shell variables.
test "$(grep -Fc '          grep -Fqx "source_sha=$GITHUB_SHA" "$provenance"' "$workflow")" -eq 3
# shellcheck disable=SC2016 # Match literal workflow shell variables.
test "$(grep -Fc '          grep -Fqx "target_group=$EXPECTED_TARGET_GROUP" "$provenance"' "$workflow")" -eq 3
# shellcheck disable=SC2016 # Match literal workflow shell variables.
test "$(grep -Fc '          grep -Fqx "patch_mode=$EXPECTED_PATCH_MODE" "$provenance"' "$workflow")" -eq 3
test "$(grep -Fc "          sed 's/^/[EVIDENCE] cfl_/' \"\$provenance\"" "$workflow")" -eq 3

grep -q '^  core)$' "$adapter"
grep -q '^  formats)$' "$adapter"
grep -q '^  assessment)$' "$adapter"
# shellcheck disable=SC2016 # Match the literal adapter variable.
grep -Fq -- '--targets "$targets_csv"' "$adapter"
grep -qx 'patched' "$patch_mode_file"
grep -qx 'all' "$target_group_file"
grep -Fq 'ICCDEV_CFL_KNOWN_BUG_PATCH_MODE' "$adapter"
grep -Fq 'CFL_EXTRA_ICCDEV_CFL_TARGET_GROUP' "$adapter"
grep -Fq 'CFL_EXTRA_ICCDEV_CFL_KNOWN_BUG_PATCH_MODE' "$adapter"
grep -Fq '.github/ci/fuzz-patches/cfl' "$adapter"
grep -Fq 'iccdev-cfl-build-provenance.txt' "$adapter"
grep -Fq "printf 'source_sha=%s\\n'" "$adapter"
grep -Fq "printf 'target_group=%s\\n'" "$adapter"
grep -Fq "printf 'patch_mode=%s\\n'" "$adapter"
"$repo_root/.github/scripts/iccdev-apply-fuzz-patches.sh" \
  --mode cfl --dry-run --strict
grep -Fq 'iccdev-build-msan-libcxx.sh' "$adapter"
grep -Fq -- '--skip-libxml2' "$adapter"
grep -Fq 'ICCDEV_CFL_LIBXML2_PREFIX' "$adapter"
grep -Fq 'libxml2_soname' "$adapter"
grep -Fq 'CXXFLAGS//-stdlib=libc++/' "$adapter"
grep -Fq 'ICCDEV_CFL_CXX_LINK_FLAGS' "$adapter"
grep -Fq 'libstdc++.so' "$adapter"
grep -Fq '[EVIDENCE] msan_runtime fuzzer=' "$adapter"
grep -Fq '[EVIDENCE] msan_xml_runtime fuzzer=' "$adapter"
grep -Fq '[PASS] MSan replay fuzzer=' "$adapter"
# shellcheck disable=SC2016 # Match the literal ELF loader token.
grep -Fq '$ORIGIN' "$adapter"
grep -Fq 'libc++.so.1.0' "$adapter"
grep -Fq 'libc++abi.so.1' "$adapter"
grep -Fq 'issue-2687-profile-list-node.icc.base64' "$adapter"
grep -Fq 'origin_history_size=7' "$adapter"
grep -Fq 'xmlSetGenericErrorFunc(nullptr, discardXmlDiagnostic)' \
  "$repo_root/.github/ci/cfl/icc_xmlparse_fuzzer.cpp"
grep -Fq 'xmlSetStructuredErrorFunc(nullptr, discardXmlStructuredDiagnostic)' \
  "$repo_root/.github/ci/cfl/icc_xmlparse_fuzzer.cpp"
grep -Fq 'ICCDEV_CFL_CXX_LINK_FLAGS' "$repo_root/.github/ci/cfl/build.sh"
grep -Fq 'ICCDEV_CFL_LIBXML2_PREFIX' "$repo_root/.github/ci/cfl/build.sh"
grep -Fq -- '--skip-libxml2' < <("$msan_builder" --help)
grep -Fq 'cmake_generator="Unix Makefiles"' "$msan_builder"
grep -Fq 'extensions.partialClone origin' "$msan_builder"
grep -Fq 'remote.origin.promisor true' "$msan_builder"
grep -Fq 'remote.origin.partialCloneFilter blob:none' "$msan_builder"
test "$(grep -c 'configure_partial_clone "\$.*source_dir"' "$msan_builder")" -eq 2
test "$(grep -c -- '--filter=blob:none' "$msan_builder")" -eq 2
issue_2687_sha="$(base64 --decode "$issue_2687_fixture" | sha256sum | cut -d' ' -f1)"
test "$issue_2687_sha" = \
  'bb9c4ad53f9269920947bac185a634da2971a94b17da6cff117dd7ad45bcfe85'
# shellcheck disable=SC2016 # The adapter must retain this literal template.
fuzzer_template='  fuzzer="icc_${target}_fuzzer"'
grep -Fqx "$fuzzer_template" "$adapter"
grep -Fq '21:21|22:22)' "$repo_root/.github/ci/cfl/build.sh"
grep -Fq '21:21|22:22)' "$repo_root/.github/scripts/iccdev-afl-smoke.sh"
# shellcheck disable=SC2016 # Match the literal skip-build condition.
skip_build_line="$(grep -n '^if \[ "$skip_build" -eq 0 \]; then$' "$repo_root/.github/scripts/iccdev-afl-smoke.sh" | cut -d: -f1)"
# shellcheck disable=SC2016 # Match the literal compiler-gate condition.
compiler_gate_line="$(grep -n '^    if \[ -z "${AFL_CC:-}" \]; then$' "$repo_root/.github/scripts/iccdev-afl-smoke.sh" | cut -d: -f1)"
test "$compiler_gate_line" -gt "$skip_build_line"

"$target_test"
"$artifact_cleanup_test"
"$patch_test"

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
