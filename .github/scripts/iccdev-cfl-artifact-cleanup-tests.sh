#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Test deterministic non-empty current-run ClusterFuzzLite artifact selection.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
cleanup_script="$script_dir/iccdev-cfl-artifact-cleanup.sh"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-cfl-artifact-test.XXXXXX")"
trap 'rm -rf -- "$work_dir"' EXIT
artifact_file="$work_dir/artifacts.tsv"
plan_file="$work_dir/plan.tsv"
valid_seed="$work_dir/json-parametric-complete-params.json"
valid_archive="$work_dir/valid-corpus.zip"
empty_dir="$work_dir/empty-corpus"
empty_archive="$work_dir/empty-corpus.zip"

printf '{"parameters":[0.5]}\n' > "$valid_seed"
(
  cd "$work_dir"
  zip -q "$valid_archive" "$(basename "$valid_seed")"
)
mkdir "$empty_dir"
zip -q "$empty_archive" "$empty_dir/"

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
  printf 'cifuzz-corpus-%s\t%s\t2026-09-26T16:00:00Z\t16384\t%s\n' \
    "$target" "$artifact_id" "$valid_archive" >> "$artifact_file"
  artifact_id=$((artifact_id + 1))
  printf 'cifuzz-corpus-%s\t%s\t2026-09-26T17:00:00Z\t550\t%s\n' \
    "$target" "$artifact_id" "$valid_archive" >> "$artifact_file"
  artifact_id=$((artifact_id + 1))
  printf 'cifuzz-corpus-%s\t%s\t2026-09-26T18:00:00Z\t32768\t%s\n' \
    "$target" "$artifact_id" "$empty_archive" >> "$artifact_file"
  artifact_id=$((artifact_id + 1))
done
printf 'unrelated-artifact\t999\t2026-09-26T19:00:00Z\t32768\n' >> "$artifact_file"

"$cleanup_script" --plan "$artifact_file" > "$plan_file"
test "$(grep -c '^KEEP' "$plan_file")" -eq 8
test "$(grep -c '^DELETE' "$plan_file")" -eq 16
if grep -Fq 'unrelated-artifact' "$plan_file"; then
  echo "[FAIL] Artifact cleanup selected an unrelated artifact" >&2
  exit 1
fi
test "$(awk -F '\t' '$1 == "KEEP" && $4 == "2026-09-26T17:00:00Z" { n++ } END { print n+0 }' "$plan_file")" -eq 8

head -n -4 "$artifact_file" > "$work_dir/incomplete.tsv"
if "$cleanup_script" --plan "$work_dir/incomplete.tsv" >/dev/null 2>&1; then
  echo "[FAIL] Artifact cleanup accepted a missing canonical target" >&2
  exit 1
fi

awk -F '\t' -v OFS='\t' '{$4 = 349; print}' \
  "$artifact_file" > "$work_dir/undersized.tsv"
awk -F '\t' -v OFS='\t' -v archive="$empty_archive" '{$5 = archive; print}' \
  "$work_dir/undersized.tsv" > "$work_dir/empty.tsv"
if "$cleanup_script" --plan "$work_dir/empty.tsv" >/dev/null 2>&1; then
  echo "[FAIL] Artifact cleanup accepted artifacts without testcase files" >&2
  exit 1
fi

echo "[PASS] ClusterFuzzLite artifact cleanup contract"
