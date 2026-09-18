#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Configure and build a non-sanitized Debug tree for Valgrind tools.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
VG_SOURCE_DIR="${ICCDEV_VALGRIND_SOURCE_DIR:-$repo_root}"
VG_BUILD_DIR="${ICCDEV_VALGRIND_BUILD_DIR:-$repo_root/out/valgrind}"
jobs="${ICCDEV_VALGRIND_JOBS:-$(nproc)}"
clean=0
requested_targets=()

# shellcheck source=.github/ci/valgrind/targets.sh
source "$script_dir/targets.sh"
# shellcheck source=.github/ci/valgrind/common.sh
source "$script_dir/common.sh"

usage()
{
  echo "Usage: $0 [--clean] [--source-dir DIR] [--build-dir DIR] [--jobs N] [--target NAME]"
  echo "Builds every registered target unless --target selects one or more lanes."
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --clean) clean=1; shift ;;
    --source-dir) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; VG_SOURCE_DIR="$2"; shift 2 ;;
    --build-dir) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; VG_BUILD_DIR="$2"; shift 2 ;;
    --jobs) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; jobs="$2"; shift 2 ;;
    --target) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; requested_targets+=("$2"); shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

case "$jobs" in
  ''|0*|*[!0-9]*) echo "ERROR: --jobs must be a positive integer" >&2; exit 2 ;;
esac
[ -f "$VG_SOURCE_DIR/Build/Cmake/CMakeLists.txt" ] || {
  echo "ERROR: not an iccDEV source tree: $VG_SOURCE_DIR" >&2
  exit 2
}

resolved_build="$(realpath -m "$VG_BUILD_DIR")"
resolved_source="$(realpath -m "$VG_SOURCE_DIR")"
case "$resolved_build" in
  /|"$resolved_source"|"$resolved_source/Build")
    echo "ERROR: refusing unsafe Valgrind build directory: $resolved_build" >&2
    exit 2
    ;;
esac
if [ -n "${ICCDEV_BUILD_DIR:-}" ] &&
    [ "$resolved_build" = "$(realpath -m "$ICCDEV_BUILD_DIR")" ]; then
  echo "ERROR: refusing configured main build directory: $resolved_build" >&2
  exit 2
fi

if [ "$clean" -eq 1 ] && [ -e "$VG_BUILD_DIR" ]; then
  case "$resolved_build" in
    *valgrind*) ;;
    *) echo "ERROR: Valgrind cleanup path must contain 'valgrind': $resolved_build" >&2; exit 2 ;;
  esac
  rm -rf -- "$resolved_build"
fi

cmake_args=(
  -S "$VG_SOURCE_DIR/Build/Cmake"
  -B "$VG_BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Debug
  -DENABLE_TESTS=ON
  -DENABLE_TOOLS=ON
  -DENABLE_WXWIDGETS=OFF
  -DENABLE_SANITIZERS=OFF
  -DENABLE_ASAN=OFF
  -DENABLE_UBSAN=OFF
  -DENABLE_INTEGER_SANITIZER=OFF
  -DENABLE_FLOAT_SANITIZER=OFF
  -DENABLE_TSAN=OFF
  -DENABLE_MSAN=OFF
  -DENABLE_LSAN=OFF
  -DENABLE_FUZZING=OFF
  -DENABLE_COVERAGE=OFF
  -DENABLE_PROFILING=OFF
  -DENABLE_LTO=OFF
)
if [ ! -f "$VG_BUILD_DIR/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
  cmake_args+=(-G Ninja)
fi
cmake "${cmake_args[@]}"
vg_assert_unsanitized_cache "$VG_BUILD_DIR/CMakeCache.txt"

build_targets=()
if [ "${#requested_targets[@]}" -eq 0 ]; then
  build_targets=("${VG_BUILD_TARGETS[@]}")
else
  for requested in "${requested_targets[@]}"; do
    VG_RUN_WORK="$VG_BUILD_DIR/validation-work"
    if ! vg_configure_target "$requested"; then
      echo "ERROR: unknown target: $requested" >&2
      vg_print_targets >&2
      exit 2
    fi
    build_targets+=("$VG_CMAKE_TARGET" "${VG_EXTRA_CMAKE_TARGETS[@]}")
  done
fi

cmake --build "$VG_BUILD_DIR" --parallel "$jobs" --target "${build_targets[@]}"

for registered in "${VG_TARGETS[@]}"; do
  VG_RUN_WORK="$VG_BUILD_DIR/validation-work"
  vg_configure_target "$registered"
  if [[ " ${build_targets[*]} " == *" $VG_CMAKE_TARGET "* ]] && [ ! -x "$VG_BINARY" ]; then
    echo "ERROR: expected executable is missing: $VG_BINARY" >&2
    exit 1
  fi
  if [ -x "$VG_BINARY" ]; then
    vg_assert_unsanitized_binary "$VG_BINARY"
  fi
done

echo "[PASS] Non-sanitized Debug build ready: $VG_BUILD_DIR"
