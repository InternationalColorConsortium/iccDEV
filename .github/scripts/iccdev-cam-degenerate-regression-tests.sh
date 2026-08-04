#!/bin/bash
###############################################################################
# iccDEV CAM degenerate-state regression tests
###############################################################################
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-cam-degenerate-regressions}"
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

run_cam_degenerate_helper() {
  local name="cam-degenerate"
  local helper_cpp="$REPO_ROOT/.github/ci/regression/cam-degenerate.cpp"
  local helper_bin="${TMPDIR:-/tmp}/iccdev-${name}-helper-$$"
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
    if [ -f "$lib_dir/lib${lib_name}.so" ] || [ -f "$lib_dir/lib${lib_name}.dylib" ]; then
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
    rm -f "$helper_bin"
    return
  fi

  LD_LIBRARY_PATH="$lib_dir:${LD_LIBRARY_PATH:-}" "$helper_bin" > "$run_log" 2>&1 || run_ec=$?

  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$run_log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding while checking degenerate CAM state"
    sed -n '1,80p' "$run_log"
    rm -f "$helper_bin"
    return
  fi

  if [ "$run_ec" -ne 0 ]; then
    fail_case "$name" "helper exited $run_ec"
    sed -n '1,80p' "$run_log"
    rm -f "$helper_bin"
    return
  fi

  rm -f "$helper_bin"
  pass_case "$name" "degenerate CAM state produces finite zero output"
}

# #1950: CalcCoefficients divided by 5*m_La + 1 with no domain check on m_La,
# and that denominator is exactly zero at m_La == -0.2f.
#
# This needs its own helper rather than another case in cam-degenerate.cpp. The
# division happens inside IccProfLib, so it is only instrumented when the library
# itself was compiled with -fsanitize=float-divide-by-zero -- and no CI lane both
# enables that and runs ctest (the float-sanitized build in ci-docker-pr.yml
# builds iccDumpProfile only). Sanitizer flags on the helper alone would not see
# it, so the helper compiles IccCAM.cpp into its own binary instead. Its copy of
# the converter then wins for the calls the helper makes, and the check works
# against an ordinary non-sanitized library build.
#
# The value contract for these luminances is asserted separately, as case 7 of
# cam-degenerate.cpp; it cannot catch the division, because a negative m_La
# already produced finite zeros through the public API. See the comment there.
run_cam_divzero_helper() {
  local name="cam-la-divide-by-zero"
  local helper_cpp="$REPO_ROOT/.github/ci/regression/cam-la-divzero.cpp"
  local cam_cpp="$REPO_ROOT/IccProfLib/IccCAM.cpp"
  local helper_bin="${TMPDIR:-/tmp}/iccdev-${name}-helper-$$"
  local compile_log="$OUTDIR/$name.compile.log"
  local run_log="$OUTDIR/$name.run.log"
  local lib_dir="$BUILD_DIR/IccProfLib"
  local lib_arg=""
  local run_ec=0

  TOTAL=$((TOTAL + 1))

  if [ -z "$BUILD_DIR" ] || [ ! -d "$lib_dir" ]; then
    fail_case "$name" "missing build directory with IccProfLib"
    return
  fi

  for lib_name in IccProfLib2d IccProfLib2; do
    if [ -f "$lib_dir/lib${lib_name}.so" ] || [ -f "$lib_dir/lib${lib_name}.dylib" ]; then
      lib_arg="-l${lib_name}"
      break
    fi
  done

  if [ -z "$lib_arg" ]; then
    fail_case "$name" "missing shared IccProfLib library in $lib_dir"
    return
  fi

  if [ ! -f "$helper_cpp" ] || [ ! -f "$cam_cpp" ]; then
    fail_case "$name" "missing helper source $helper_cpp or $cam_cpp"
    return
  fi

  # Not every supported compiler carries this check; skip rather than fail so a
  # toolchain without it does not turn into a false regression.
  if ! echo 'int main(){return 0;}' |
       "$CXX" -x c++ -std=c++17 -fsanitize=float-divide-by-zero - \
         -o "$helper_bin" >/dev/null 2>&1; then
    rm -f "$helper_bin"
    pass_case "$name" "skipped -- $CXX does not support -fsanitize=float-divide-by-zero"
    return
  fi
  rm -f "$helper_bin"

  if ! "$CXX" -std=c++17 -fsanitize=float-divide-by-zero -g \
      -DICCPROFLIB_EXPORTS \
      -I"$REPO_ROOT/IccProfLib" \
      -I"$BUILD_DIR/IccProfLib" \
      "$helper_cpp" "$cam_cpp" \
      -L"$lib_dir" "$lib_arg" -Wl,-rpath,"$lib_dir" \
      -o "$helper_bin" > "$compile_log" 2>&1; then
    fail_case "$name" "failed to compile helper"
    sed -n '1,80p' "$compile_log"
    rm -f "$helper_bin"
    return
  fi

  UBSAN_OPTIONS="print_stacktrace=0" \
    LD_LIBRARY_PATH="$lib_dir:${LD_LIBRARY_PATH:-}" "$helper_bin" \
    > "$run_log" 2>&1 || run_ec=$?

  if grep -q "runtime error:" "$run_log" 2>/dev/null; then
    fail_case "$name" "division by zero in CalcCoefficients for a negative adapting luminance"
    sed -n '1,80p' "$run_log"
    rm -f "$helper_bin"
    return
  fi

  if [ "$run_ec" -ne 0 ]; then
    fail_case "$name" "helper exited $run_ec"
    sed -n '1,80p' "$run_log"
    rm -f "$helper_bin"
    return
  fi

  rm -f "$helper_bin"
  pass_case "$name" "negative adapting luminance does not divide by zero"
}

echo "=== CAM degenerate-state regression ==="

run_cam_degenerate_helper
run_cam_divzero_helper

echo "CAM degenerate-state regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
