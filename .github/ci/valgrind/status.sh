#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Summarize one Valgrind evidence directory, or the newest local run.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
evidence_dir="${1:-}"
if [ -z "$evidence_dir" ]; then
  output_base="${ICCDEV_VALGRIND_OUTPUT_DIR:-$repo_root/out/valgrind-evidence}"
  evidence_dir="$(find "$output_base" -mindepth 1 -maxdepth 1 -type d \
    -printf '%T@ %p\n' 2>/dev/null | sort -nr | sed -n '1s/^[^ ]* //p')"
fi
[ -n "$evidence_dir" ] && [ -f "$evidence_dir/summary.tsv" ] || {
  echo "No Valgrind summary found." >&2
  exit 1
}

echo "Evidence: $evidence_dir"
column -t -s $'\t' "$evidence_dir/summary.tsv" 2>/dev/null ||
  sed -n '1,200p' "$evidence_dir/summary.tsv"
