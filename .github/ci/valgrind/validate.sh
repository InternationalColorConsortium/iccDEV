#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Validate the Valgrind target registry without requiring a build.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
VG_SOURCE_DIR="$repo_root"
VG_BUILD_DIR="$repo_root/out/valgrind"
VG_RUN_WORK="$repo_root/out/valgrind-validation"

# shellcheck source=.github/ci/valgrind/targets.sh
source "$script_dir/targets.sh"

errors=0
declare -A seen=()
for target in "${VG_TARGETS[@]}"; do
  if [ -n "${seen[$target]:-}" ]; then
    echo "ERROR: duplicate target: $target" >&2
    errors=$((errors + 1))
    continue
  fi
  seen[$target]=1
  if ! vg_configure_target "$target"; then
    echo "ERROR: target does not configure: $target" >&2
    errors=$((errors + 1))
    continue
  fi
  if [ -z "$VG_BINARY" ] || [ -z "$VG_CMAKE_TARGET" ] || [ "${#VG_ARGS[@]}" -eq 0 ]; then
    echo "ERROR: incomplete target: $target" >&2
    errors=$((errors + 1))
  fi
  for extra_target in "${VG_EXTRA_CMAKE_TARGETS[@]}"; do
    if [ -z "$extra_target" ]; then
      echo "ERROR: empty extra CMake target for $target" >&2
      errors=$((errors + 1))
    fi
  done
  case "$VG_RECOMMENDED_TOOL" in
    memcheck|helgrind|drd) ;;
    *) echo "ERROR: bad recommended tool for $target: $VG_RECOMMENDED_TOOL" >&2; errors=$((errors + 1)) ;;
  esac
done

[ "$errors" -eq 0 ] || exit 1
echo "[PASS] Valgrind target registry: ${#VG_TARGETS[@]} targets"
