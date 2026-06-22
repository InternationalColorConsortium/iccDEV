#!/bin/bash
###############################################################################
# iccDEV encoding-profile converter matrix-leak regression tests (#1505)
###############################################################################
#
# Compiles .github/ci/regression/encprofile-matrix-leak.cpp against the build's
# IccProfLib and verifies that CIccDefaultEncProfileConverter::ConvertFromParams
# does not leak the CIccMpeMatrix it builds when a 'cept' parameter struct omits
# (or truncates) a primary chromaticity. Before the fix the unattached matrix
# leaked on each primary early-out (LeakSanitizer: "Direct leak of 56 byte(s)"
# from CIccBasicMpeFactory::CreateElement IccMpeFactory.cpp:92, retained via
# IccEncoding.cpp:278). After the fix every error exit frees it.
#
# LeakSanitizer must be active for this to bite, so the helper is forced to run
# with ASAN_OPTIONS=detect_leaks=1 regardless of the CI default (CI ctest
# otherwise runs detect_leaks=0, which would let the leak pass silently).
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-encprofile-matrix-leak-regressions}"
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

# detect_leaks=1 is mandatory: the regression IS a leak, and the CI default of
# detect_leaks=0 would let it pass.
export ASAN_OPTIONS="detect_leaks=1,halt_on_error=1,${ASAN_OPTIONS:-}"
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

run_encprofile_helper() {
  local name="encprofile-matrix-leak"
  local helper_cpp="$REPO_ROOT/.github/ci/regression/encprofile-matrix-leak.cpp"
  local helper_bin="${TMPDIR:-/tmp}/iccdev-${name}-helper-$$"
  local compile_log="$OUTDIR/$name.compile.log"
  local run_log="$OUTDIR/$name.run.log"
  local lib_dir="$BUILD_DIR/IccProfLib"
  local lib_arg=""
  local san_flags=()
  local has_asan=0
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
      has_asan=1
    else
      if grep -q '^ENABLE_ASAN:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
        san_flags+=("-fsanitize=address")
        has_asan=1
      fi
      if grep -q '^ENABLE_UBSAN:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
        san_flags+=("-fsanitize=undefined")
      fi
    fi
  fi

  # LeakSanitizer ships with AddressSanitizer; without an ASan build there is
  # nothing to detect the leak, so skip rather than report a false pass.
  if [ "$has_asan" -ne 1 ]; then
    pass_case "$name" "skipped (no AddressSanitizer build; LeakSanitizer unavailable)"
    return
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

  if grep -q "LeakSanitizer: detected memory leaks\\|ERROR: AddressSanitizer\\|runtime error:" "$run_log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding while converting a malformed 'cept' parameter struct"
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
  pass_case "$name" "ConvertFromParams freed the matrix on every malformed-primary exit"
}

echo "=== Encoding-profile converter matrix-leak regression (#1505) ==="

run_encprofile_helper

echo "Encoding-profile converter matrix-leak regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
