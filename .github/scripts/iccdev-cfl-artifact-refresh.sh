#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Prepare recent exact-name corpus artifacts before ClusterFuzzLite runs.
###############################################################################

set -euo pipefail

usage()
{
  echo "Usage: $0 --prepare OWNER/REPO OUTPUT_DIR TARGET [TARGET ...]"
}

is_supported_target()
{
  case "$1" in
    icc_profileparse_fuzzer|icc_cmmapply_fuzzer|\
      icc_profilevisualize_fuzzer|icc_writerserialize_fuzzer|\
      icc_xmlparse_fuzzer|icc_jsonparse_fuzzer|\
      icc_connectconfig_fuzzer|icc_pawgreport_fuzzer)
      return 0
      ;;
  esac
  return 1
}

archive_has_testcase()
{
  local archive_path="$1"

  tar -tf "$archive_path" | awk '
    /\/$/ {
      next
    }
    {
      found = 1
    }
    END {
      exit !found
    }
  '
}

case "${1:-}" in
  --prepare)
    if [ "$#" -lt 4 ]; then
      usage >&2
      exit 2
    fi
    repository="$2"
    output_dir="$3"
    shift 3
    ;;
  -h|--help)
    usage
    exit 0
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac

if [[ ! "$repository" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]]; then
  echo "[FAIL] Repository must use OWNER/REPO form" >&2
  exit 2
fi
case "$output_dir" in
  ''|.|/)
    echo "[FAIL] Refusing unsafe output directory: $output_dir" >&2
    exit 2
    ;;
esac
if [ -z "${GH_TOKEN:-}" ]; then
  echo "[FAIL] GH_TOKEN is required for corpus artifact refresh" >&2
  exit 2
fi
if ! command -v unzip >/dev/null 2>&1; then
  echo "[FAIL] unzip is required for corpus artifact refresh" >&2
  exit 2
fi

declare -A seen_targets=()
for target in "$@"; do
  if ! is_supported_target "$target"; then
    echo "[FAIL] Unsupported ClusterFuzzLite target: $target" >&2
    exit 2
  fi
  if [ -n "${seen_targets[$target]:-}" ]; then
    echo "[FAIL] Duplicate ClusterFuzzLite target: $target" >&2
    exit 2
  fi
  seen_targets[$target]=1
done
mkdir -p "$output_dir"
work_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/iccdev-cfl-refresh.XXXXXX")"
trap 'rm -rf -- "$work_dir"' EXIT

for target in "$@"; do
  artifact_name="cifuzz-corpus-$target"
  output_tar="$output_dir/$artifact_name.tar"
  candidate_file="$work_dir/$target-candidates.tsv"
  sorted_candidate_file="$work_dir/$target-candidates-sorted.tsv"
  refreshed=0

  gh api \
    "repos/$repository/actions/artifacts?name=$artifact_name&per_page=100" \
    --jq '.artifacts[] | select(.expired == false) | [.created_at, (.id | tostring)] | @tsv' \
    > "$candidate_file"
  LC_ALL=C sort -r "$candidate_file" > "$sorted_candidate_file"

  while IFS=$'\t' read -r created_at artifact_id; do
    if [ -z "$created_at" ] || [[ ! "$artifact_id" =~ ^[0-9]+$ ]]; then
      echo "[FAIL] Invalid artifact metadata for $artifact_name" >&2
      exit 1
    fi

    zip_path="$work_dir/$artifact_id.zip"
    candidate_tar="$work_dir/$artifact_id.tar"
    if ! gh api \
      "repos/$repository/actions/artifacts/$artifact_id/zip" > "$zip_path"; then
      echo "[WARN] Could not download $artifact_name id=$artifact_id" >&2
      continue
    fi
    if ! zip_entries="$(unzip -Z1 "$zip_path")"; then
      echo "[WARN] Invalid artifact ZIP for $artifact_name id=$artifact_id" >&2
      continue
    fi
    if [ "$zip_entries" != "$artifact_name.tar" ]; then
      echo "[WARN] Unexpected artifact layout for $artifact_name id=$artifact_id" >&2
      continue
    fi
    if ! unzip -p "$zip_path" "$artifact_name.tar" > "$candidate_tar"; then
      echo "[WARN] Could not extract corpus TAR for $artifact_name id=$artifact_id" >&2
      continue
    fi
    if ! archive_has_testcase "$candidate_tar"; then
      echo "[WARN] Corpus artifact has no testcase files: $artifact_name id=$artifact_id" >&2
      continue
    fi

    mv "$candidate_tar" "$output_tar"
    printf '[PASS] Refreshed corpus artifact name=%s id=%s created_at=%s\n' \
      "$artifact_name" "$artifact_id" "$created_at"
    refreshed=1
    break
  done < "$sorted_candidate_file"

  if [ "$refreshed" -eq 0 ]; then
    tar -cf "$output_tar" --files-from /dev/null
    printf '[PASS] Created empty cold-start corpus artifact name=%s\n' \
      "$artifact_name"
  fi
done
