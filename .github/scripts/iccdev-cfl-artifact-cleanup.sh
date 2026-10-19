#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Retain only the newest valid same-run ClusterFuzzLite corpus for each target.
###############################################################################

set -euo pipefail

usage()
{
  echo "Usage: $0 --plan TSV | --delete-current-run OWNER/REPO RUN_ID"
}

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

archive_has_testcase()
{
  local archive_path="$1"

  if [ ! -f "$archive_path" ]; then
    echo "[FAIL] Missing downloaded corpus archive: $archive_path" >&2
    return 1
  fi

  unzip -Z1 "$archive_path" | awk '
    /\/$/ {
      next
    }
    {
      found = 1
      exit
    }
    END {
      exit !found
    }
  '
}

plan_artifacts()
{
  local artifact_file="$1"
  local artifact_id artifact_name artifact_size archive_path created_at row target
  local keep_artifact_id
  local -a rows

  for target in "${targets[@]}"; do
    artifact_name="cifuzz-corpus-$target"
    mapfile -t rows < <(
      awk -F '\t' -v name="$artifact_name" '$1 == name' "$artifact_file" |
        sort -t $'\t' -k3,3r -k2,2nr
    )
    if [ "${#rows[@]}" -eq 0 ]; then
      echo "[FAIL] Missing current-run corpus artifact: $artifact_name" >&2
      return 1
    fi

    keep_artifact_id=""
    for row in "${rows[@]}"; do
      IFS=$'\t' read -r _ artifact_id created_at artifact_size archive_path <<< "$row"
      case "$artifact_size" in
        ''|*[!0-9]*)
          echo "[FAIL] Invalid artifact size for $artifact_name: $artifact_size" >&2
          return 1
          ;;
      esac
      if [ -z "$keep_artifact_id" ] && archive_has_testcase "$archive_path"; then
        keep_artifact_id="$artifact_id"
      fi
    done
    if [ -z "$keep_artifact_id" ]; then
      echo "[FAIL] No current-run corpus artifact contains testcase files: $artifact_name" >&2
      return 1
    fi

    for row in "${rows[@]}"; do
      IFS=$'\t' read -r _ artifact_id created_at artifact_size archive_path <<< "$row"
      if [ "$artifact_id" = "$keep_artifact_id" ]; then
        printf 'KEEP\t%s\t%s\t%s\n' \
          "$artifact_name" "$artifact_id" "$created_at"
      else
        printf 'DELETE\t%s\t%s\t%s\n' \
          "$artifact_name" "$artifact_id" "$created_at"
      fi
    done
  done
}

case "${1:-}" in
  --plan)
    [ "$#" -eq 2 ] || { usage >&2; exit 2; }
    plan_artifacts "$2"
    ;;
  --delete-current-run)
    [ "$#" -eq 3 ] || { usage >&2; exit 2; }
    repository="$2"
    run_id="$3"
    case "$repository" in
      */*) ;;
      *) echo "[FAIL] Repository must use OWNER/REPO form" >&2; exit 2 ;;
    esac
    case "$run_id" in
      ''|*[!0-9]*) echo "[FAIL] Run ID must be numeric" >&2; exit 2 ;;
    esac
    if [ -z "${GH_TOKEN:-}" ]; then
      echo "[FAIL] GH_TOKEN is required for artifact cleanup" >&2
      exit 2
    fi

    work_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/iccdev-cfl-artifacts.XXXXXX")"
    trap 'rm -rf -- "$work_dir"' EXIT
    artifact_file="$work_dir/artifacts.tsv"
    plan_file="$work_dir/plan.tsv"
    gh api --paginate \
      "repos/$repository/actions/runs/$run_id/artifacts?per_page=100" \
      --jq '.artifacts[] | select(.expired == false) | [.name, (.id | tostring), .created_at, (.size_in_bytes | tostring)] | @tsv' \
      > "$artifact_file"
    while IFS=$'\t' read -r artifact_name artifact_id created_at artifact_size; do
      case "$artifact_name" in
        cifuzz-corpus-*)
          archive_path="$work_dir/$artifact_id.zip"
          gh api \
            "repos/$repository/actions/artifacts/$artifact_id/zip" > "$archive_path"
          printf '%s\t%s\t%s\t%s\t%s\n' \
            "$artifact_name" "$artifact_id" "$created_at" "$artifact_size" \
            "$archive_path"
          ;;
      esac
    done < "$artifact_file" > "$work_dir/inspected-artifacts.tsv"
    plan_artifacts "$work_dir/inspected-artifacts.tsv" | tee "$plan_file"

    while IFS=$'\t' read -r action artifact_name artifact_id _; do
      if [ "$action" != "DELETE" ]; then
        continue
      fi
      gh api --method DELETE \
        "repos/$repository/actions/artifacts/$artifact_id"
      printf '[PASS] Deleted superseded artifact name=%s id=%s\n' \
        "$artifact_name" "$artifact_id"
    done < "$plan_file"
    ;;
  -h|--help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
