#!/bin/bash
###############################################################################
# iccDEV issue #1406 - divide-by-zero in CIccPcsXform::pushXyzLumToXyz
###############################################################################
#
# When luminance-based PCS matching is requested (+1000 rendering intent), the
# CMM inserts an inverse-luminance PCS scale step.  pushXyzLumToXyz() built that
# step as
#
#     icFloatNumber scale = 1.0f / XYZLum[1];
#
# where XYZLum[1] is the illuminant luminance (Y) from the destination profile's
# connection conditions.  A malformed profile can drive that Y to zero -- the
# committed PoC carries a spectralViewingConditions (svcn) tag whose
# IlluminantXYZ is (0,0,0) -- so 1.0/0 is a divide by zero that UBSan's
# float-divide-by-zero check flags, producing an infinite scale that then
# corrupts every PCS sample:
#
#   IccCmm.cpp:3048:30: runtime error: division by zero
#     in CIccPcsXform::pushXyzLumToXyz(IIccProfileConnectionConditions*)
#
# The fix falls back to an identity (1.0) scale when the luminance is not a
# positive, finite value.  This test replays the fuzz PoC through iccApplyProfiles
# with a +1000 luminance intent and fails if the divide-by-zero reappears.
#
# Like the other UBSan fuzz regressions, the divide-by-zero check only exists
# when the tool was built with the float-divide-by-zero sanitizer (the
# ci-docker regression image adds -fsanitize=float-divide-by-zero; plain
# ENABLE_UBSAN does NOT).  On a build without it the test SKIPS rather than
# report a misleading pass.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TESTING_DIR -- path to Testing/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (no divide-by-zero) or skipped cleanly
#   2 - UBSan float-divide-by-zero finding (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1406-roundtrip-xyzlum-dbz}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then TOOLS_DIR="$candidate"; break; fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyProfiles -type f 2>/dev/null | head -1)"
POC_ICC="$DATA_DIR/ub-roundtrip-xyzlum-dbz-1406.icc"
# The PoC device space is RGB; reuse the always-present spectral test image whose
# embedded profile provides the upstream xform the PoC luminance-matches against.
SRC_TIFF="$TESTING_DIR/hybrid/Data/smCows380_5_780.tif"
LOGFILE="$OUTDIR/issue-1406-roundtrip-xyzlum-dbz.log"

echo "=== iccApplyProfiles issue-1406 pushXyzLumToXyz divide-by-zero regression ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyProfiles not built under $TOOLS_DIR"
  exit 0
fi
if [ ! -f "$POC_ICC" ]; then
  echo "  [SKIP] PoC profile missing: $POC_ICC"
  exit 0
fi
if [ ! -f "$SRC_TIFF" ]; then
  echo "  [SKIP] source image missing: $SRC_TIFF"
  exit 0
fi

# Only meaningful when the tool was built with the float-divide-by-zero
# sanitizer.  Walk up from the tool to its CMakeCache.txt and require either the
# float/bundled sanitizer option or an explicit -fsanitize=float-divide-by-zero
# flag.  Plain ENABLE_UBSAN does not enable the float check, so it is not enough.
CACHE=""
probe="$(dirname "$APPLY")"
for _ in 1 2 3 4 5; do
  if [ -f "$probe/CMakeCache.txt" ]; then CACHE="$probe/CMakeCache.txt"; break; fi
  probe="$(dirname "$probe")"
done
fdz=0
if [ -n "$CACHE" ]; then
  if grep -qiE "ENABLE_FLOAT_SANITIZER:BOOL=ON|ENABLE_SANITIZERS:BOOL=ON" "$CACHE"; then
    fdz=1
  elif grep -iE "CMAKE_(CXX|C)_FLAGS" "$CACHE" | grep -qi "float-divide-by-zero"; then
    fdz=1
  fi
fi
if [ "$fdz" -ne 1 ]; then
  echo "  [SKIP] tool not built with float-divide-by-zero sanitizer (CMakeCache: ${CACHE:-none})"
  exit 0
fi

# Apply the PoC profile (rendering intent 1003 = absolute + the +1000 luminance
# PCS adjustment) on top of the source image's embedded profile, so the CMM
# inserts the inverse-luminance step and reaches pushXyzLumToXyz.  Keep UBSan
# from halting so the diagnostic is captured in the log; the invalid PoC may make
# the tool exit non-zero regardless -- the divide-by-zero message is the signal.
OUT_TIFF="$OUTDIR/issue-1406-out.tif"
rm -f "$LOGFILE" "$OUT_TIFF"
ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
  "$APPLY" "$SRC_TIFF" "$OUT_TIFF" 2 1 0 1 1 -embedded 3 "$POC_ICC" 1003 > "$LOGFILE" 2>&1

if grep -qE "runtime error: division by zero" "$LOGFILE"; then
  echo "  [FAIL] issue-1406 -- pushXyzLumToXyz divide-by-zero reappeared"
  grep -E "runtime error: division by zero|pushXyzLumToXyz|IccCmm.cpp" "$LOGFILE" | head
  exit 2
fi

echo "  [PASS] issue-1406-roundtrip-xyzlum-dbz -- zero illuminant luminance handled without divide-by-zero"
exit 0
