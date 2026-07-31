#!/bin/bash
###############################################################################
# iccDEV issue #1781 - iccApplyToLink QA matrix
###############################################################################
#
# Covers the 20-case QA matrix handed over in #1781, recalibrated so it can run
# in CI, plus the defects that working through it exposed.
#
# The matrix as filed drives APTEC_CMYKOGV_Coated_LinearCTV_2025.icc, a 7.66 MB
# seven-channel profile fetched from registry.color.org, at lut_size 17 for 8 of
# the 12 feature cases (33 for three, 9 for one) -- 17^7 is 410,338,673 CMM
# evaluations, and the profile is one CI cannot download.  Here the same feature
# axes (link type, v4/v5 option, interpolation, every rendering intent used in
# the matrix, -ENV, -PCC, restricted range) run against a CMYK source built from
# the tracked CMYK-3DLUTs.xml at lut_size 5, which is 625 nodes and finishes
# immediately.  Only tracked inputs are used, so the test does not depend on
# Testing/ profile generation having run first.
#
# The three regressions it guards, all reproduced against origin/master before
# the fix:
#
#   1. CIccCLUT stores a grid as one flat array of NumPoints() * output-channel
#      floats, so CIccCLUT::Init enforces a limit on the node count AND on the
#      node count times the output channel count.  iccApplyToLink's own cap only
#      bounded the node count, omitting exactly the output-channel factor, so a
#      CMYK source at the documented maximum lut_size of 255 (255^4 =
#      4,228,250,625 nodes; 12,684,751,875 samples for a 3-channel destination)
#      passed the tool's check and was then refused by Init.  Init nulls m_pData
#      before returning false but has already committed m_nNumPoints, and
#      CIccMBB::NewCLUT discards Init's result, so the writer got a CLUT
#      advertising 4.2 billion nodes with no storage: GetData(0) yielded NULL,
#      the non-zero NumPoints() satisfied setNextNode()'s countdown guard, and
#      the first node memcpy'd to address zero.  SIGSEGV on both the v4
#      (option 0) and v5 (option 1) paths.
#
#   2. range_min/range_max bound the input domain the grid samples.  Only
#      finiteness was checked, so a zero-width range (every node sampling the
#      same input) and a reversed range both wrote a file and exited 0 --
#      QA cases 19 and 20, which the #1781 QA table records as
#      "Fail - no early range rejection".
#
#   3. Only the .cube writer (LUT_3D_INPUT_RANGE) and the v5 device-link branch
#      (CIccSingleSampledCurve) can record the input domain; the v4 branch
#      never reads it.  A v4 link over a restricted range therefore sampled
#      that range while declaring [0,1] -- QA case 04, which the QA table
#      records as a success because writing it does not fail.
#
# Crash detection: a run killed by a signal exits 128+N for N in 1..64, so
# 129..192 is treated as a crash.  The range matters -- iccApplyToLink's own
# error paths `return -1`, which the shell reports as 255, so a bare
# "rc >= 128" test would flag every graceful rejection as a crash and would
# have passed against the pre-fix build for the wrong reason.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for logs and generated fixtures
#
# Exit codes:
#   0 - pass (or skipped cleanly when tools / tracked inputs are unavailable)
#   1 - a case behaved differently than expected (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1781}"
mkdir -p "$OUTDIR"

TAG="issue-1781-applytolink-qa-matrix"
FAILURES=0

fail() {
  echo "  [FAIL] $TAG -- $1"
  FAILURES=$((FAILURES + 1))
}

BIN="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
  echo "[SKIP] iccApplyToLink binary not found under $TOOLS_DIR"
  exit 0
fi

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml binary not found under $TOOLS_DIR (needed to build the CMYK source)"
  exit 0
fi

# All three inputs are tracked in git, unlike the Testing/*.icc profiles that
# CreateAllProfiles.sh generates -- so this test needs no profile fixture.
CMYK_XML="$REPO_ROOT/Testing/CMYK-3DLUTs/CMYK-3DLUTs.xml"
SRGB="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"
PCC="$REPO_ROOT/Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
for f in "$CMYK_XML" "$SRGB" "$PCC"; do
  if [ ! -f "$f" ]; then
    echo "[SKIP] required tracked input missing: $f"
    exit 0
  fi
done

# Build the four-channel source.  A three-channel source cannot reach the
# capacity defect at all: 255^3 * 3 = 49,744,125 samples fits comfortably, so
# the node-count-only cap and the real limit agree and nothing is missed.
CMYK="$OUTDIR/cmyk-source.icc"
if ! ( cd "$(dirname "$CMYK_XML")" && "$FROMXML" "$(basename "$CMYK_XML")" "$CMYK" ) \
     > "$OUTDIR/fromxml.log" 2>&1 || [ ! -f "$CMYK" ]; then
  echo "[SKIP] could not build the CMYK source from $CMYK_XML"
  sed 's/^/    /' "$OUTDIR/fromxml.log"
  exit 0
fi

# run <logname> <args...> -- records the exit code in RC, output in $OUTDIR
LOG=""
RC=0
NAME=""
run() {
  NAME="$1"
  LOG="$OUTDIR/$1.log"
  shift
  timeout 120s "$BIN" "$@" > "$LOG" 2>&1
  RC=$?
  # Strip the \r-delimited progress counter so message matching sees clean text.
  tr -d '\r' < "$LOG" | grep -v '^[0-9]*%$' > "$LOG.clean" 2>/dev/null
  check_sanitizer
}

# Under a sanitizer build the CTest harness sets halt_on_error=0, so a finding
# is reported without changing the exit status -- every assertion below would
# still pass while the tool was writing out of bounds. Scan explicitly. Inert on
# a non-sanitizer build, where none of these markers can appear. The whole
# matrix is currently clean under ASAN+UBSan, so any hit is new.
check_sanitizer() {
  if grep -qE "runtime error:|ERROR: (Address|Leak|Memory)Sanitizer|SUMMARY: UndefinedBehaviorSanitizer" "$LOG" 2>/dev/null; then
    fail "$NAME: sanitizer finding"
    grep -hE "runtime error:|ERROR:|SUMMARY:" "$LOG" | head -4 | sed 's/^/    /'
  fi
}

# True when the shell reports the run as killed by a signal (128+N, N in 1..64).
# 255 is deliberately outside this range: it is the tool's own `return -1`.
crashed() {
  [ "$RC" -ge 129 ] && [ "$RC" -le 192 ]
}

# A case that must be refused: non-zero exit, and specifically NOT a signal.
# Pre-fix, the capacity cases exited 139 (SIGSEGV) -- which is non-zero too, so
# checking only "non-zero" would have passed against the broken build.
expect_reject() {
  local name="$1" expect_msg="$2"
  shift 2
  run "$name" "$@"
  if crashed; then
    fail "$name: terminated by signal (exit $RC) -- expected a graceful rejection"
    sed 's/^/    /' "$LOG.clean" | tail -5
    return
  fi
  if [ "$RC" -eq 0 ]; then
    fail "$name: exited 0 -- expected a rejection"
    return
  fi
  if ! grep -qF "$expect_msg" "$LOG.clean"; then
    fail "$name: rejected (exit $RC) but without the expected diagnostic '$expect_msg'"
    sed 's/^/    /' "$LOG.clean" | tail -5
  fi
}

expect_ok() {
  local name="$1"
  shift
  run "$name" "$@"
  if crashed; then
    fail "$name: terminated by signal (exit $RC)"
    sed 's/^/    /' "$LOG.clean" | tail -5
    return
  fi
  if [ "$RC" -ne 0 ]; then
    fail "$name: exit $RC -- expected success"
    sed 's/^/    /' "$LOG.clean" | tail -5
  fi
}

O="$OUTDIR/link.icc"
C="$OUTDIR/link.cube"

###############################################################################
# 1. Capacity regressions -- the NULL write.  Both were SIGSEGV pre-fix.
###############################################################################
expect_reject "cap-v4-grid255" "LUT size too large" \
  "$O" 0 255 0 "QA cap v4" 0 1 1 0 "$CMYK" 1 "$SRGB" 1
expect_reject "cap-v5-grid255" "LUT size too large" \
  "$O" 0 255 1 "QA cap v5" 0 1 1 0 "$CMYK" 1 "$SRGB" 1

# A three-channel source at the same lut_size stays viable: the rejection above
# must come from the capacity arithmetic, not from lut_size 255 being refused
# outright -- without this control, capping lut_size at 254 would "fix" the
# crash and still pass.  255^3 = 16,581,375 nodes: measured at 8.9s and 195 MB
# resident un-sanitized, and it dominates this script's runtime (8.8s of the
# plain run, 35s under ASAN, against the 300s CTest budget).
expect_ok "cap-rgb-grid255-still-ok" \
  "$O" 0 255 0 "QA cap rgb" 0 1 1 0 "$SRGB" 1 "$SRGB" 1

###############################################################################
# 2. Range ordering -- QA cases 19 and 20.  Both exited 0 pre-fix.
###############################################################################
expect_reject "case19-zero-width-range" "must be greater than" \
  "$O" 0 5 0 "QA zero width range" 1 1 1 0 "$CMYK" 1 "$SRGB" 1
expect_reject "case20-reversed-range" "must be greater than" \
  "$O" 0 5 0 "QA reversed range" 1 0 1 1 "$CMYK" 1 "$SRGB" 1

###############################################################################
# 3. v4 cannot record a restricted domain -- QA case 04.  Exited 0 pre-fix,
#    writing a link that sampled [0.001,0.999] while declaring [0,1].
###############################################################################
expect_reject "case04-v4-restricted-range" "V4 device links cannot record" \
  "$O" 0 5 0 "QA v4 restricted" 0.001 0.999 1 0 "$CMYK" 1 "$SRGB" 1

# The refusal must be specific to v4: the two writers that can record the domain
# keep accepting the identical range.
expect_ok "case04-v5-restricted-range-ok" \
  "$O" 0 5 1 "QA v5 restricted" 0.001 0.999 1 0 "$CMYK" 1 "$SRGB" 1
expect_ok "case04-cube-restricted-range-ok" \
  "$C" 1 5 4 "QA cube restricted" 0.001 0.999 1 0 "$SRGB" 1 "$SRGB" 1

# Proves the range actually reached the file rather than being silently dropped
# the way the v4 branch drops it -- which is the whole point of case 04.
if [ -f "$C" ] && ! grep -q "LUT_3D_INPUT_RANGE" "$C"; then
  fail "case04-cube-restricted-range-ok: .cube output does not carry LUT_3D_INPUT_RANGE"
fi

###############################################################################
# 4. Argument rejections -- QA cases 13-18.  These already behaved correctly;
#    they are pinned so the new range checks above cannot disturb them.
###############################################################################
# Case 13 is an argv-parity rejection, not an interpolation check: dropping
# interp leaves an odd number of trailing arguments, so the profile shifts into
# the interp position and the pairing check fires first.
expect_reject "case13-missing-interp" "Missing arguments!" \
  "$O" 0 5 0 "QA missing interp" 0 1 1 "$CMYK" 1 "$SRGB" 1
expect_reject "case14-lut-below-min" "expected an integer between 2 and 255" \
  "$O" 0 1 0 "QA LUT too small" 0 1 1 0 "$CMYK" 1 "$SRGB" 1
expect_reject "case15-lut-above-max" "expected an integer between 2 and 255" \
  "$O" 0 256 0 "QA LUT too large" 0 1 1 0 "$CMYK" 1 "$SRGB" 1
expect_reject "case16-bad-devicelink-option" "DeviceLink option must be 0 (v4) or 1 (v5)" \
  "$O" 0 5 2 "QA invalid DeviceLink option" 0 1 1 0 "$CMYK" 1 "$SRGB" 1
expect_reject "case17-bad-first-transform" "expected 0 or 1" \
  "$O" 0 5 0 "QA invalid first transform" 0 1 2 0 "$CMYK" 1 "$SRGB" 1
expect_reject "case18-bad-interp" "expected 0 (linear) or 1 (tetrahedral)" \
  "$O" 0 5 0 "QA invalid interpolation" 0 1 1 2 "$CMYK" 1 "$SRGB" 1

###############################################################################
# 5. Feature cases 01-12, recalibrated to lut_size 5.
#
#    The #1781 QA table marks 03, 06, 08, 09, 11 and 12 as
#    "Unable to write LUT" against the seven-channel APTEC source.  All six
#    succeed here, so those failures track that profile and grid size rather
#    than the feature being exercised -- though that is an inference, not a
#    reproduction: confirming it needs the registry profile, which CI cannot
#    fetch.
#
#    Case 04 is absent from this list because it is covered above: as filed it
#    is a v4 link with a restricted range, which is now refused, and its v5
#    and .cube equivalents are asserted there.
###############################################################################
expect_ok "case01-v4-linear-relative"  "$O" 0 5 0 "QA 01" 0 1 1 0 "$CMYK" 1    "$SRGB" 1
expect_ok "case02-v4-tetra-perceptual" "$O" 0 5 0 "QA 02" 0 1 1 1 "$CMYK" 0    "$SRGB" 0
expect_ok "case03-v5-relative"         "$O" 0 5 1 "QA 03" 0 1 1 1 "$CMYK" 1    "$SRGB" 1
expect_ok "case05-saturation"          "$O" 0 5 0 "QA 05" 0 1 1 1 "$CMYK" 2    "$SRGB" 2
expect_ok "case06-absolute"            "$O" 0 5 1 "QA 06" 0 1 1 0 "$CMYK" 3    "$SRGB" 3
expect_ok "case07-relative-bpc"        "$O" 0 5 0 "QA 07" 0 1 1 1 "$CMYK" 41   "$SRGB" 41
expect_ok "case08-luminance-pcs"       "$O" 0 5 1 "QA 08" 0 1 1 0 "$CMYK" 101  "$SRGB" 101
expect_ok "case09-v5-subprofile"       "$O" 0 5 1 "QA 09" 0 1 1 1 "$CMYK" 1001 "$SRGB" 1001
expect_ok "case10-v5-luminance"        "$O" 0 5 1 "QA 10" 0 1 1 1 "$CMYK" 1101 "$SRGB" 1101
expect_ok "case11-env-vars"            "$O" 0 5 0 "QA 11" 0 1 1 1 \
  -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 "$CMYK" 1 "$SRGB" 1
expect_ok "case12-pcc-after-first"     "$O" 0 5 1 "QA 12" 0 1 1 1 \
  "$CMYK" 1 -PCC "$PCC" "$SRGB" 1

###############################################################################

if [ "$FAILURES" -ne 0 ]; then
  echo "  [FAIL] $TAG -- $FAILURES case(s) regressed"
  exit 1
fi

echo "  [PASS] $TAG -- capacity, range-ordering and v4-domain rejections hold; QA cases 01-18 behave as expected"
exit 0
