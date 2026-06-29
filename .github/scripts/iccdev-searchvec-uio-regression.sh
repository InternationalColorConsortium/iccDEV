#!/bin/bash
###############################################################################
# iccDEV CIccSearchVec unsigned-overflow sanitizer regression
###############################################################################
#
# Replays the hybrid iccApplySearch command that instantiates zero-length
# CIccSearchVec arithmetic under the Nelder-Mead search path.  Clang 21 with
# libstdc++ 15 and -fsanitize=integer reported an unsigned overflow inside the
# std::vector resize(fill) implementation used by CIccSearchVec(unsigned int).
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts and logs
#
# Exit codes:
#   0 - pass or clean skip when integer sanitizer is not enabled
#   1 - setup or tool failure
#   2 - sanitizer finding
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-${HOME:-/tmp}/work/copilot/iccdev-searchvec-uio}"
WORKDIR="$OUTDIR/hybrid"
LOGDIR="$OUTDIR/logs"

mkdir -p "$LOGDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1:abort_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"

find_tool() {
  local name="$1"
  find "$TOOLS_DIR" -maxdepth 2 -name "$name" -type f 2>/dev/null | head -1
}

find_cache() {
  local probe="$1"
  for _ in 1 2 3 4 5; do
    if [ -f "$probe/CMakeCache.txt" ]; then
      printf '%s\n' "$probe/CMakeCache.txt"
      return
    fi
    probe="$(dirname "$probe")"
  done
}

has_sanitizer_finding() {
  grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null
}

fail() {
  echo "  [FAIL] searchvec-uio -- $1"
  exit 1
}

fail_sanitizer() {
  local label="$1"
  local log="$2"

  echo "  [FAIL] searchvec-uio -- sanitizer finding during $label"
  sed -n '1,120p' "$log"
  exit 2
}

run_to_file() {
  local label="$1"
  local output="$2"
  local log="$3"
  shift 3

  set +e
  "$@" > "$output" 2> "$log"
  local rc=$?
  set -e

  if has_sanitizer_finding "$log"; then
    fail_sanitizer "$label" "$log"
  fi
  if [ "$rc" -ne 0 ]; then
    echo "  [FAIL] searchvec-uio -- $label exited $rc"
    sed -n '1,80p' "$log"
    exit 1
  fi
}

run_logged() {
  local label="$1"
  local log="$2"
  shift 2

  set +e
  "$@" > "$log" 2>&1
  local rc=$?
  set -e

  if has_sanitizer_finding "$log"; then
    fail_sanitizer "$label" "$log"
  fi
  if [ "$rc" -ne 0 ]; then
    echo "  [FAIL] searchvec-uio -- $label exited $rc"
    sed -n '1,80p' "$log"
    exit 1
  fi
}

echo "=== CIccSearchVec unsigned-overflow sanitizer regression ==="

CACHE="$(find_cache "$BUILD_ROOT")"
if [ -z "$CACHE" ]; then
  echo "  [SKIP] unable to find CMakeCache.txt for $TOOLS_DIR"
  exit 0
fi
if ! grep -qiE "ENABLE_INTEGER_SANITIZER:BOOL=ON|ENABLE_SANITIZERS:BOOL=ON|fsanitize=[^ ]*integer" "$CACHE"; then
  echo "  [SKIP] build is not instrumented with the integer sanitizer"
  echo "         CMakeCache: $CACHE"
  exit 0
fi

FROMXML="$(find_tool iccFromXml)"
APPLYNAMED="$(find_tool iccApplyNamedCmm)"
APPLYSEARCH="$(find_tool iccApplySearch)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "  [SKIP] iccFromXml not built under $TOOLS_DIR"
  exit 0
fi
if [ -z "$APPLYNAMED" ] || [ ! -x "$APPLYNAMED" ]; then
  echo "  [SKIP] iccApplyNamedCmm not built under $TOOLS_DIR"
  exit 0
fi
if [ -z "$APPLYSEARCH" ] || [ ! -x "$APPLYSEARCH" ]; then
  echo "  [SKIP] iccApplySearch not built under $TOOLS_DIR"
  exit 0
fi

HYBRID_SRC="$TESTING_DIR/hybrid"
for required in \
    "$HYBRID_SRC/CMYK_Hybrid_Profile.xml" \
    "$HYBRID_SRC/Data/cmykGrays.txt" \
    "$HYBRID_SRC/Data/Spec380_10_730-D50_2deg.xml" \
    "$HYBRID_SRC/Data/Lab_float-D50_2deg.xml" \
    "$HYBRID_SRC/Data/Lab_float-D93_2deg-MAT.xml" \
    "$HYBRID_SRC/Data/Lab_float-F11_2deg-MAT.xml" \
    "$HYBRID_SRC/Data/Lab_float-IllumA_2deg-MAT.xml"; do
  [ -f "$required" ] || fail "missing hybrid fixture: $required"
done

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR/Data" "$WORKDIR/ICC" "$WORKDIR/Results" "$WORKDIR/config"
cp "$HYBRID_SRC/CMYK_Hybrid_Profile.xml" "$WORKDIR/"
cp "$HYBRID_SRC/Data/cmykGrays.txt" "$WORKDIR/Data/"
cp "$HYBRID_SRC/Data/Spec380_10_730-D50_2deg.xml" "$WORKDIR/Data/"
cp "$HYBRID_SRC/Data/Lab_float-D50_2deg.xml" "$WORKDIR/Data/"
cp "$HYBRID_SRC/Data/Lab_float-D93_2deg-MAT.xml" "$WORKDIR/Data/"
cp "$HYBRID_SRC/Data/Lab_float-F11_2deg-MAT.xml" "$WORKDIR/Data/"
cp "$HYBRID_SRC/Data/Lab_float-IllumA_2deg-MAT.xml" "$WORKDIR/Data/"

(
  cd "$WORKDIR" || exit 1

  run_logged "fromxml CMYK hybrid" "$LOGDIR/fromxml-cmyk.log" \
    "$FROMXML" CMYK_Hybrid_Profile.xml ICC/CMYK_Hybrid_Profile.icc
  run_logged "fromxml spectral D50" "$LOGDIR/fromxml-spec380.log" \
    "$FROMXML" Data/Spec380_10_730-D50_2deg.xml ICC/Spec380_10_730-D50_2deg.icc
  run_logged "fromxml Lab D50" "$LOGDIR/fromxml-lab-d50.log" \
    "$FROMXML" Data/Lab_float-D50_2deg.xml ICC/Lab_float-D50_2deg.icc
  run_logged "fromxml Lab D93" "$LOGDIR/fromxml-lab-d93.log" \
    "$FROMXML" Data/Lab_float-D93_2deg-MAT.xml ICC/Lab_float-D93_2deg-MAT.icc
  run_logged "fromxml Lab F11" "$LOGDIR/fromxml-lab-f11.log" \
    "$FROMXML" Data/Lab_float-F11_2deg-MAT.xml ICC/Lab_float-F11_2deg-MAT.icc
  run_logged "fromxml Lab IllumA" "$LOGDIR/fromxml-lab-illuma.log" \
    "$FROMXML" Data/Lab_float-IllumA_2deg-MAT.xml ICC/Lab_float-IllumA_2deg-MAT.icc

  run_to_file "reference CMYK gray data" Results/cmykGraysRef.txt "$LOGDIR/cmykGraysRef.log" \
    "$APPLYNAMED" -exportcfganddata config/cmykGraysRef.json Data/cmykGrays.txt \
    3 1 ICC/CMYK_Hybrid_Profile.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3

  run_to_file "search CMYK gray estimate" Results/cmykGraysEst.txt "$LOGDIR/cmykGraysEst.log" \
    "$APPLYSEARCH" -exportcfganddata config/cmykGraysEst.json Results/cmykGraysRef.txt \
    0 1 ICC/Spec380_10_730-D50_2deg.icc 3 ICC/Lab_float-D50_2deg.icc 3 \
    ICC/CMYK_Hybrid_Profile.icc 10003 -INIT 3 ICC/Lab_float-D50_2deg.icc 1 \
    ICC/Lab_float-D93_2deg-MAT.icc 1 ICC/Lab_float-F11_2deg-MAT.icc 1 \
    ICC/Lab_float-IllumA_2deg-MAT.icc 1
)

[ -s "$WORKDIR/Results/cmykGraysEst.txt" ] || fail "iccApplySearch produced no estimate output"

echo "  [PASS] searchvec-uio -- hybrid iccApplySearch completed without integer sanitizer findings"
exit 0
