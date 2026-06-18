#!/bin/bash
###############################################################################
# iccDEV issue #1410 - divide-by-zero in icYxy2XYZVector (IccEncoding.cpp)
###############################################################################
#
# Loading a colorEncodingParams ("cenc") profile runs it through
# icConvertEncodingProfile() -> CIccDefaultEncProfileConverter::ConvertFromParams,
# which converts the white/media/surround chromaticities to tristimulus XYZ via
#
#     XYZ[0]            = Y*x / y;
#     XYZ[idxOffset<<1] = Y*(1-x-y) / y;
#
# where y is the CIE chromaticity y coordinate (xy[1]).  A malformed profile can
# carry a chromaticity member whose y is zero -- the committed PoC
# (ub-encoding-yxy-xyz-dbz-1410.icc) does exactly this -- so Y*x/y is a divide by
# zero that UBSan's float-divide-by-zero check flags, yielding inf/NaN that then
# poisons the header illuminant and the XYZ tags built from the vector:
#
#   IccEncoding.cpp:138:20: runtime error: division by zero
#   IccEncoding.cpp:140:44: runtime error: division by zero
#     in icYxy2XYZVector(float*, float, float*, unsigned char)
#     #1 CIccDefaultEncProfileConverter::ConvertFromParams
#     #2 icConvertEncodingProfile
#
# The fix treats a non-positive, non-finite y as a degenerate point and emits
# (0, Y, 0) instead of dividing.  This test replays the fuzz PoC by adding it as a
# transform profile through iccApplyToLink (the profile is rejected as invalid
# regardless -- the divide-by-zero diagnostic is the signal) and fails if the
# divide-by-zero reappears.
#
# Like the other UBSan fuzz regressions, the divide-by-zero check only exists when
# the tool was built with the float-divide-by-zero sanitizer (the
# ci-regression-container build adds -fsanitize=float-divide-by-zero; plain
# ENABLE_UBSAN does NOT).  On a build without it the test SKIPS rather than report
# a misleading pass.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1410-encoding-yxy-xyz-dbz}"
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

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
POC_ICC="$DATA_DIR/ub-encoding-yxy-xyz-dbz-1410.icc"
LOGFILE="$OUTDIR/issue-1410-encoding-yxy-xyz-dbz.log"

echo "=== iccApplyToLink issue-1410 icYxy2XYZVector divide-by-zero regression ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyToLink not built under $TOOLS_DIR"
  exit 0
fi
if [ ! -f "$POC_ICC" ]; then
  echo "  [SKIP] PoC profile missing: $POC_ICC"
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

# Add the PoC profile to a transform link.  The dst/title file names are
# placeholders -- the cenc profile is converted (and the divide-by-zero is hit)
# while it is being added to the transform chain, before any LUT is written, so
# the tool exits non-zero on the invalid profile regardless.  Keep UBSan from
# halting so the diagnostic is captured in the log; the divide-by-zero message is
# the signal we test for.
rm -f "$LOGFILE"
ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
  "$APPLY" "$OUTDIR/issue-1410-link.bar" 0 2 1 issue-1410 0 1 1 0 "$POC_ICC" 9 > "$LOGFILE" 2>&1

if grep -qE "runtime error: division by zero" "$LOGFILE"; then
  echo "  [FAIL] issue-1410 -- icYxy2XYZVector divide-by-zero reappeared"
  grep -E "runtime error: division by zero|icYxy2XYZVector|IccEncoding.cpp" "$LOGFILE" | head
  exit 2
fi

echo "  [PASS] issue-1410-encoding-yxy-xyz-dbz -- zero chromaticity y handled without divide-by-zero"
exit 0
