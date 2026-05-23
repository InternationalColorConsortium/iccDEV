#!/bin/bash
###############################################################################
# iccDEV CIccCmmSearch search-cost weight regression
###############################################################################
#
# Exercises CIccCmmSearch PCC weight validation via a small C++ helper that
# links IccProfLib2.  This guards the weighted-average search-cost denominator
# introduced by CIccApplyCmmSearch::costFunc().
#
# What is asserted:
#   - positive PCC weights are accepted
#   - zero PCC weights are rejected before they can produce 0 / 0
#   - negative PCC weights are rejected before weights can cancel out
#   - non-finite PCC weights are rejected before NaN/Inf reaches costFunc
#
# Environment variables (set by CI workflow):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/ (sibling of IccProfLib)
#   ICCDEV_TESTING_DIR -- path to Testing/ (must contain a forward, init,
#                         and PCC profile suitable for a search chain)
#   ICCDEV_TEST_OUTDIR -- output directory for compile / runtime logs
#
# Exit codes:
#   0 - pass (or skipped cleanly when required profiles are unavailable)
#   1 - test failure
#   2 - sanitizer finding
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-iccconnect-search-cost}"
BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd)"
mkdir -p "$OUTDIR"

HELPER_SRC="$REPO_ROOT/.github/ci/regression/iccconnect-search-cost.cpp"
HELPER_BIN="$OUTDIR/iccconnect-search-cost"
HELPER_LOG="$OUTDIR/iccconnect-search-cost.log"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=4

find_library_file() {
  local dir="$1"
  local base="$2"
  find "$dir" -maxdepth 1 \( \
      -name "lib${base}*.so" -o \
      -name "lib${base}*.dylib" -o \
      -name "lib${base}*.a" \
    \) -print 2>/dev/null | sort | head -n 1
}

echo "=== CIccCmmSearch::GetApplyCost regression ==="

if [ ! -f "$HELPER_SRC" ]; then
  echo "  [FAIL] missing helper source: $HELPER_SRC"
  exit 1
fi

if [ -z "$BUILD_ROOT" ] || [ ! -d "$BUILD_ROOT/IccProfLib" ]; then
  echo "  [SKIP] unable to locate build root from ICCDEV_TOOLS_DIR=$TOOLS_DIR"
  exit 0
fi

# Pick profiles that are available in the iccDEV Testing tree.  Generate the
# small hybrid fixtures on demand so this regression is not only a skip unless
# the full hybrid pipeline has already run.
HYBRID_ICC="$TESTING_DIR/hybrid/ICC"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
if [ -x "$FROMXML" ] && [ -d "$TESTING_DIR/hybrid/Data" ]; then
  mkdir -p "$HYBRID_ICC"
  [ -f "$HYBRID_ICC/Lab_float-D50_2deg.icc" ] ||
    "$FROMXML" "$TESTING_DIR/hybrid/Data/Lab_float-D50_2deg.xml" \
               "$HYBRID_ICC/Lab_float-D50_2deg.icc" >/dev/null 2>&1 || true
fi

PCC=""
for candidate in \
    "$TESTING_DIR/hybrid/ICC/Lab_float-D50_2deg.icc" \
    "$TESTING_DIR/PCC/Lab_float-D50_2deg.icc" \
    "$TESTING_DIR/hybrid/ICC/Lab_float-IllumA_2deg-MAT.icc"; do
  if [ -f "$candidate" ]; then PCC="$candidate"; break; fi
done

if [ -z "$PCC" ]; then
  echo "  [SKIP] required PCC profile not present under $TESTING_DIR"
  echo "         (need Lab PCC profile)"
  exit 0
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
  echo "  [SKIP] missing linkable IccProfLib library under $BUILD_ROOT/IccProfLib"
  exit 0
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

run_helper_case() {
  local label="$1"
  shift
  local case_log="$OUTDIR/${label}.log"
  local run_ec=0

  "$HELPER_BIN" "$PCC" "$@" > "$case_log" 2>&1 || run_ec=$?
  cat "$case_log" >> "$HELPER_LOG"
  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$case_log" 2>/dev/null; then
    echo "  [FAIL] $label produced sanitizer findings"
    sed -n '1,60p' "$case_log"
    FAIL=$((FAIL + 1))
  elif [ "$run_ec" -ne 0 ]; then
    echo "  [FAIL] $label failed"
    sed -n '1,60p' "$case_log"
    FAIL=$((FAIL + 1))
  else
    echo "  [PASS] $label"
    sed -n '$p' "$case_log"
    PASS=$((PASS + 1))
  fi
}

if [ "$compile_ec" -ne 0 ]; then
  echo "  [FAIL] helper build failed"
  sed -n '1,30p' "$HELPER_LOG"
  FAIL=$((FAIL + 1))
else
  run_helper_case "AttachPCC accepts positive weight" 1 expect-accept
  run_helper_case "AttachPCC rejects zero weight" 0 expect-reject
  run_helper_case "AttachPCC rejects negative weight" -1 expect-reject
  run_helper_case "AttachPCC rejects non-finite weight" nan expect-reject
fi

rm -f "$HELPER_BIN"

echo "CIccCmmSearch::GetApplyCost regression: $PASS passed, $FAIL failed, $TOTAL total"

if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$HELPER_LOG" 2>/dev/null; then
  exit 2
fi
if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
exit 0
