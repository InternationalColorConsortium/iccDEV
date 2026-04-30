#!/bin/bash
###############################################################################
# iccDEV namedColor2 pixel-to-name apply regression tests
###############################################################################
#
# Exercises CIccXformNamedColor::Apply() for malformed namedColor2 device data
# that previously reached sanitizer findings through iccApplyNamedCmm.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-namedcolor-apply-regressions}"
BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd)"
mkdir -p "$OUTDIR"

HELPER_SRC="$REPO_ROOT/.github/ci/regression/namedcolor-apply-guard.cpp"
HELPER_BIN="$OUTDIR/namedcolor-apply-guard"
HELPER_LOG="$OUTDIR/namedcolor-apply-guard.log"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=1

find_library_file() {
  local dir="$1"
  local base="$2"

  find "$dir" -maxdepth 1 \( \
      -name "lib${base}*.so" -o \
      -name "lib${base}*.dylib" -o \
      -name "lib${base}*.a" \
    \) -print 2>/dev/null | sort | head -n 1
}

echo "=== namedColor2 apply regression ==="

if [ ! -f "$HELPER_SRC" ]; then
  echo "  [FAIL] missing helper source: $HELPER_SRC"
  exit 1
fi

if [ -z "$BUILD_ROOT" ] || [ ! -d "$BUILD_ROOT/IccProfLib" ]; then
  echo "  [FAIL] unable to locate build root from ICCDEV_TOOLS_DIR=$TOOLS_DIR"
  exit 1
fi

if [ -n "${CXX:-}" ]; then
  HELPER_CXX="$CXX"
elif command -v clang++-18 >/dev/null 2>&1; then
  HELPER_CXX="clang++-18"
elif command -v clang++ >/dev/null 2>&1; then
  HELPER_CXX="clang++"
else
  HELPER_CXX="c++"
fi

PROFLIB="$(find_library_file "$BUILD_ROOT/IccProfLib" "IccProfLib2" || true)"
if [ -z "$PROFLIB" ]; then
  echo "  [FAIL] missing linkable IccProfLib library under $BUILD_ROOT/IccProfLib"
  exit 1
fi

SAN_FLAGS="-fsanitize=address,undefined"
if "$HELPER_CXX" --version 2>/dev/null | grep -qi clang; then
  SAN_FLAGS="-fsanitize=address,undefined,integer -fno-sanitize-recover=undefined -fno-sanitize-recover=integer"
fi

compile_ec=0
# shellcheck disable=SC2086
"$HELPER_CXX" -std=c++17 $SAN_FLAGS \
  -I"$BUILD_ROOT/IccProfLib" \
  -I"$REPO_ROOT/IccProfLib" \
  -I"$REPO_ROOT" \
  "$HELPER_SRC" \
  -Wl,-rpath,"$BUILD_ROOT/IccProfLib" \
  "$PROFLIB" \
  -o "$HELPER_BIN" > "$HELPER_LOG" 2>&1 || compile_ec=$?

if [ "$compile_ec" -ne 0 ]; then
  echo "  [FAIL] helper build failed"
  sed -n '1,30p' "$HELPER_LOG"
  FAIL=$((FAIL + 1))
elif "$HELPER_BIN" >> "$HELPER_LOG" 2>&1; then
  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$HELPER_LOG" 2>/dev/null; then
    echo "  [FAIL] helper produced sanitizer findings"
    sed -n '1,60p' "$HELPER_LOG"
    FAIL=$((FAIL + 1))
  else
    echo "  [PASS] namedColor2 pixel-to-name guard"
    PASS=$((PASS + 1))
  fi
else
  echo "  [FAIL] helper runtime failed"
  sed -n '1,60p' "$HELPER_LOG"
  FAIL=$((FAIL + 1))
fi

rm -f "$HELPER_BIN"

echo "namedColor2 apply regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
