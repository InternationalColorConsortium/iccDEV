#!/bin/bash
###############################################################################
# iccDEV PCC zero-illuminant regression tests
###############################################################################
#
# Issue:
#   https://github.com/InternationalColorConsortium/iccDEV/issues/958
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_BUILD_DIR   -- path to CMake build directory
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
BUILD_DIR="${ICCDEV_BUILD_DIR:-}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-pcc-zero-illuminant-regressions}"
mkdir -p "$OUTDIR"

if [ -z "$BUILD_DIR" ] && [ -d "$TOOLS_DIR" ]; then
  BUILD_DIR="$(cd "$TOOLS_DIR/.." && pwd)"
fi

if [ -z "$BUILD_DIR" ] || [ ! -d "$BUILD_DIR/IccProfLib" ]; then
  for candidate in "$REPO_ROOT/build" "$REPO_ROOT/Build" "$REPO_ROOT/build-sani"; do
    if [ -d "$candidate/IccProfLib" ]; then
      BUILD_DIR="$candidate"
      break
    fi
  done
fi

if [ -z "${CXX:-}" ]; then
  if command -v clang++-18 >/dev/null 2>&1; then
    CXX=clang++-18
  elif command -v clang++ >/dev/null 2>&1; then
    CXX=clang++
  else
    CXX=c++
  fi
fi

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

pass_case() {
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

run_zero_illuminant_helper() {
  local name="pcc-zero-illuminant"
  local helper_cpp="$REPO_ROOT/.github/ci/regression/pcc-zero-illuminant.cpp"
  local helper_bin="$OUTDIR/$name"
  local compile_log="$OUTDIR/$name.compile.log"
  local run_log="$OUTDIR/$name.run.log"
  local lib_dir="$BUILD_DIR/IccProfLib"
  local lib_arg=""
  local san_flags=()
  local run_ec=0

  TOTAL=$((TOTAL + 1))

  if [ -z "$BUILD_DIR" ] || [ ! -d "$lib_dir" ]; then
    fail_case "$name" "missing build directory with IccProfLib"
    return
  fi

  for lib_name in IccProfLib2d IccProfLib2; do
    if [ -f "$lib_dir/lib${lib_name}.so" ]; then
      lib_arg="-l${lib_name}"
      break
    fi
  done

  if [ -z "$lib_arg" ]; then
    fail_case "$name" "missing shared IccProfLib library in $lib_dir"
    return
  fi

  if [ ! -f "$helper_cpp" ]; then
    fail_case "$name" "missing helper source $helper_cpp"
    return
  fi

  if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    if grep -q '^ENABLE_SANITIZERS:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
      if "$CXX" --version 2>/dev/null | grep -qi clang; then
        san_flags+=("-fsanitize=address,undefined,integer,float-divide-by-zero,float-cast-overflow")
      else
        san_flags+=("-fsanitize=address,undefined")
      fi
    else
      if grep -q '^ENABLE_ASAN:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
        san_flags+=("-fsanitize=address")
      fi
      if grep -q '^ENABLE_UBSAN:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
        san_flags+=("-fsanitize=undefined")
      fi
      if grep -q '^ENABLE_INTEGER_SANITIZER:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt" &&
         "$CXX" --version 2>/dev/null | grep -qi clang; then
        san_flags+=("-fsanitize=integer")
      fi
    fi
  fi

  if ! "$CXX" -std=c++17 "${san_flags[@]}" \
      -I"$REPO_ROOT/IccProfLib" \
      -I"$BUILD_DIR/IccProfLib" \
      "$helper_cpp" \
      -L"$lib_dir" "$lib_arg" -Wl,-rpath,"$lib_dir" \
      -o "$helper_bin" > "$compile_log" 2>&1; then
    fail_case "$name" "failed to compile helper"
    sed -n '1,80p' "$compile_log"
    return
  fi

  LD_LIBRARY_PATH="$lib_dir:${LD_LIBRARY_PATH:-}" "$helper_bin" > "$run_log" 2>&1 || run_ec=$?

  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$run_log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding while checking zero-Y PCC illuminant"
    sed -n '1,80p' "$run_log"
    return
  fi

  if [ "$run_ec" -ne 0 ]; then
    fail_case "$name" "helper exited $run_ec"
    sed -n '1,80p' "$run_log"
    return
  fi

  pass_case "$name" "zero-Y custom PCC illuminant is rejected without D50 fallback"
}

echo "=== PCC zero-illuminant regression ==="

run_zero_illuminant_helper

echo "PCC zero-illuminant regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
