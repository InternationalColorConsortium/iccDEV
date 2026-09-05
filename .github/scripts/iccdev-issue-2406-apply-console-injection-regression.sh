#!/bin/bash
###############################################################################
# #2406 -- ICC profile paths could inject terminal control sequences
###############################################################################
#
# CWE-150 / CWE-117.  The four iccApply* tools printed profile paths, data-file
# paths and raw argv straight into their diagnostics, so a path containing ESC
# reproduced CSI colour and OSC title payloads on the operator's terminal and in
# any log that captured them.  Measured on f186948c, all four emit raw 0x1b:
#
#   iccApplyProfiles / iccApplySearch / iccApplyNamedCmm print the shared
#   sConnectError, whose text is assembled with the raw path in
#   IccConnect.cpp; iccApplyToLink prints argv directly.
#
# The library already had the helper -- icSanitizeConsoleText() in
# IccProfLib/IccFileUtil.h, which escapes C0/C1 and anything >= 0x7f as \xHH.
# iccSpecSepToTiff routes its OWN prints through it, which is why these four
# tools looked like the whole story; measured, it still emitted 4 raw ESC bytes
# on f186948c, because the leak it shares with iccApplyProfiles is not in either
# tool's prints.  TiffImg.cpp passes the file PATH to TIFFError() as libtiff's
# "module" string, and libtiff raises its own "TIFFOpen: <path>: ..." the same
# way, so both went out through libtiff's default handler untouched.  Sanitizing
# the call-site argument would not have covered libtiff's internal message; the
# handler is the one place both meet, and it is installed in TiffImg.cpp so
# iccTiffDump and iccSpecSepToTiff get it too.
#
# That also means the payload is reachable through the SOURCE and DESTINATION
# image arguments, not only the profile argument -- the first version of this
# test passed a valid source TIFF and put the payload only in the profile, so it
# reported 4/4 PASS while the tool still injected on its most common error path.
#
# The predicate is the raw byte, not the rendered text: a test that grepped for
# "ESC[31m" would pass on output that still contained 0x1b written another way.
# Each case counts 0x1b in BOTH streams and requires zero.
#
# Anti-vacuity matters more than usual here, because "no ESC in the output" is
# trivially satisfied by no output at all.  Every case therefore also requires
# (a) the marker text that only appears inside the path, proving the diagnostic
# naming the path was actually produced, and (b) the escaped form \x1B, proving
# the sanitizer ran rather than the payload being dropped.
#
# Red/green: all four cases fail against f186948c, each with 2 raw ESC bytes.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-apply-console-injection-regression}"
mkdir -p "$OUTDIR"

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
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
SEARCH="$TOOLS_DIR/IccApplySearch/iccApplySearch"
NAMEDCMM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
TOLINK="$TOOLS_DIR/IccApplyToLink/iccApplyToLink"
SPECSEP="$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff"

SRC_TIFF="$REPO_ROOT/Testing/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
SRC_DATA="$REPO_ROOT/Testing/ApplyDataFiles/DarkRed-CMYK.txt"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

fail_case() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
pass_case() { echo "  [PASS] $1 -- $2"; PASS=$((PASS + 1)); }
skip_case() { echo "  [SKIP] $1 -- $2"; SKIP=$((SKIP + 1)); }

# A path that does not exist, carrying a CSI colour payload.  The tools fail to
# open it and name it in the diagnostic, which is the path under test.
ESC=$(printf '\033')
PWN_PATH="$OUTDIR/missing-${ESC}[31mICC-PWN${ESC}[0m.icc"

count_esc() {
  # Count raw 0x1b bytes.  od + grep on a byte column rather than grep -c on the
  # text, so a payload split across lines is still counted.
  od -An -tx1 -v "$1" | tr ' ' '\n' | grep -c '^1b$' || true
}

run_case() {
  local name="$1" tool="$2"; shift 2
  if [ ! -x "$tool" ]; then
    skip_case "$name" "not built at $tool"
    return
  fi
  TOTAL=$((TOTAL + 1))

  local log="$OUTDIR/$name.log"
  local rc=0
  : > "$log"
  timeout 60 "$tool" "$@" > "$log" 2>&1 || rc=$?

  if [ "$rc" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi

  local esc
  esc="$(count_esc "$log")"

  # (a) the diagnostic that names the path was produced at all
  if ! grep -q -- '-PWN' "$log" 2>/dev/null; then
    fail_case "$name" "no diagnostic naming the profile path -- nothing was measured"
    sed -n '1,10p' "$log"
    return
  fi
  # the defect itself, reported before the backstop below so a failure says what
  # actually went wrong
  if [ "$esc" -ne 0 ]; then
    fail_case "$name" "$esc raw ESC byte(s) reached the output"
    return
  fi
  # (b) the sanitizer ran, rather than the payload simply being dropped
  if ! grep -q 'x1B\|x1b' "$log" 2>/dev/null; then
    fail_case "$name" "path is named and carries no ESC, but the escape was not rendered as \\xHH"
    sed -n '1,10p' "$log"
    return
  fi

  pass_case "$name" "path reported with the escape neutralised, no raw ESC"
}

if [ ! -x "$PROFILES" ] && [ ! -x "$SEARCH" ] && [ ! -x "$NAMEDCMM" ] && [ ! -x "$TOLINK" ]; then
  echo "no iccApply* tool found under $TOOLS_DIR -- skipping"
  exit 77
fi
if [ ! -f "$SRC_TIFF" ] || [ ! -f "$SRC_DATA" ]; then
  echo "apply data files missing -- skipping"
  exit 77
fi

echo "=== iccApply* console injection regression (#2406) ==="

run_case iccapplyprofiles-connect-error "$PROFILES" \
  "$SRC_TIFF" "$OUTDIR/out.tif" 0 0 0 0 1 "$PWN_PATH" 1
run_case iccapplynamedcmm-connect-error "$NAMEDCMM" \
  "$SRC_DATA" 0 0 "$PWN_PATH" 1
run_case iccapplysearch-connect-error "$SEARCH" \
  "$SRC_DATA" 0 0 "$PWN_PATH" 1 "$PWN_PATH" 1 -INIT 1
run_case iccapplytolink-argv "$TOLINK" \
  "$OUTDIR/link.icc" 0 33 0 title 0 1 0 0 "$PWN_PATH" 1

# The image arguments, not the profile argument.  A bad source or destination
# path is iccApplyProfiles' most common failure and it goes out through
# libtiff's handler, so these two are what cover the TiffImg.cpp half.
PWN_TIFF="$OUTDIR/missing-${ESC}[31mTIFF-PWN${ESC}[0m.tif"
PWN_DST="$OUTDIR/no-such-dir/${ESC}[31mDST-PWN${ESC}[0m.tif"
SRGB="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"

if [ -f "$SRGB" ]; then
  run_case iccapplyprofiles-source-tiff "$PROFILES" \
    "$PWN_TIFF" "$OUTDIR/out2.tif" 0 0 0 0 1 "$SRGB" 1
  run_case iccapplyprofiles-dest-tiff "$PROFILES" \
    "$SRC_TIFF" "$PWN_DST" 0 0 0 0 1 "$SRGB" 1
else
  skip_case iccapplyprofiles-image-paths "sRGB reference profile missing"
fi

# iccSpecSepToTiff shares TiffImg.cpp, so it inherits both the defect and the
# fix.  Included because the header used to claim this tool was already clean
# and measurement said otherwise.
if [ -x "$SPECSEP" ]; then
  run_case iccspecseptotiff-prefix "$SPECSEP" \
    "$OUTDIR/specsep-out.tif" 0 0 "$OUTDIR/missing-${ESC}[31mSEP-PWN${ESC}[0m-" 400 700 10
else
  skip_case iccspecseptotiff-prefix "not built"
fi

echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
if [ "$PASS" -eq 0 ]; then
  echo "no case was measured -- treating as skipped rather than passed"
  exit 77
fi
exit 0
