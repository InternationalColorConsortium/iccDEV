#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Reproduce issue #2592 and require clean DRD and Helgrind results.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_dir="${ICCDEV_VALGRIND_BUILD_DIR:-$repo_root/out/issue-2592-valgrind}"
output_dir="${ICCDEV_VALGRIND_OUTPUT_DIR:-$repo_root/out/issue-2592-evidence}"
jobs="${ICCDEV_VALGRIND_JOBS:-$(nproc)}"

if [ -e "$output_dir" ]; then
  echo "ERROR: evidence directory already exists: $output_dir" >&2
  exit 2
fi

ICCDEV_VALGRIND_BUILD_DIR="$build_dir" ICCDEV_VALGRIND_JOBS="$jobs" \
  "$script_dir/build.sh" --target applyprofiles-row
ICCDEV_VALGRIND_BUILD_DIR="$build_dir" \
  "$script_dir/run.sh" --tool drd --timeout 180 \
    --output-dir "$output_dir/drd" applyprofiles-row
ICCDEV_VALGRIND_BUILD_DIR="$build_dir" \
  "$script_dir/run.sh" --tool helgrind --timeout 180 \
    --output-dir "$output_dir/helgrind" applyprofiles-row

drd_tiff="$output_dir/drd/applyprofiles-row/work/applied.tif"
helgrind_tiff="$output_dir/helgrind/applyprofiles-row/work/applied.tif"
cmp "$drd_tiff" "$helgrind_tiff"
sha256sum "$drd_tiff" "$helgrind_tiff"
echo "[PASS] Issue #2592 DRD and Helgrind regression"
