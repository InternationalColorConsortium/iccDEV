#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Test deterministic current-run ClusterFuzzLite artifact selection.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
cleanup_script="$script_dir/iccdev-cfl-artifact-cleanup.sh"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-cfl-artifact-test.XXXXXX")"
trap 'rm -rf -- "$work_dir"' EXIT
artifact_file="$work_dir/artifacts.tsv"
plan_file="$work_dir/plan.tsv"

targets=(
  icc_profileparse_fuzzer
  icc_cmmapply_fuzzer
  icc_profilevisualize_fuzzer
  icc_writerserialize_fuzzer
  icc_xmlparse_fuzzer
  icc_jsonparse_fuzzer
  icc_connectconfig_fuzzer
  icc_pawgreport_fuzzer
)

artifact_id=100
for target in "${targets[@]}"; do
  printf 'cifuzz-corpus-%s\t%s\t2026-09-26T16:00:00Z\n' \
    "$target" "$artifact_id" >> "$artifact_file"
  artifact_id=$((artifact_id + 1))
  printf 'cifuzz-corpus-%s\t%s\t2026-09-26T17:00:00Z\n' \
    "$target" "$artifact_id" >> "$artifact_file"
  artifact_id=$((artifact_id + 1))
done
printf 'unrelated-artifact\t999\t2026-09-26T18:00:00Z\n' >> "$artifact_file"

"$cleanup_script" --plan "$artifact_file" > "$plan_file"
test "$(grep -c '^KEEP' "$plan_file")" -eq 8
test "$(grep -c '^DELETE' "$plan_file")" -eq 8
if grep -Fq 'unrelated-artifact' "$plan_file"; then
  echo "[FAIL] Artifact cleanup selected an unrelated artifact" >&2
  exit 1
fi
test "$(awk -F '\t' '$1 == "KEEP" && $4 == "2026-09-26T17:00:00Z" { n++ } END { print n+0 }' "$plan_file")" -eq 8

head -n -3 "$artifact_file" > "$work_dir/incomplete.tsv"
if "$cleanup_script" --plan "$work_dir/incomplete.tsv" >/dev/null 2>&1; then
  echo "[FAIL] Artifact cleanup accepted a missing canonical target" >&2
  exit 1
fi

echo "[PASS] ClusterFuzzLite artifact cleanup contract"
