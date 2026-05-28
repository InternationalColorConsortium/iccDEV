#!/bin/bash
###############################################################################
# iccDEV iccApplyNamedCmm overprint-variant regression tests
###############################################################################
#
# Exercises the v5 NamedColor overprint variants surfaced by the
# CIccCreateNamedColorXformHint::nOverprintType plumbing:
#   - over-white (default)            -> icSigNmclSpectralDataMbr      ('spec')
#   - over-black (transform=namedOnBlack / intent +1000000) -> 'spcb'
#   - over-gray  (transform=namedOnGray  / intent +2000000) -> 'spcg'
#
# Test profile: Testing/Named/NamedColor.icc, rebuilt from NamedColor.xml
# by Testing/CreateAllProfiles.* (the iccdev_profiles fixture).  Its Silver
# entry carries both 'spec' (mean reflectance ~0.83) and 'spcb' (~0.30)
# array members but no 'spcg', giving us four distinct assertions:
#
#   1) over-white succeeds and produces high-reflectance spectrum
#   2) over-black succeeds and produces low-reflectance spectrum
#   3) the two spectra differ substantially (delta-mean > 0.3)
#   4) over-gray fails clean (apply exits non-zero; no silent fallback)
#
# Environment variables (set by CI workflow):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TESTING_DIR -- path to Testing/ (must contain Named/NamedColor.icc
#                         and Named/NamedColorSilverOnly.txt)
#   ICCDEV_TEST_OUTDIR -- output directory for temporary logs
#
# Exit codes:
#   0 - all pass (or skipped cleanly when fixtures are absent)
#   1 - test failure
#   2 - sanitizer finding
###############################################################################

set -uo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:?Set ICCDEV_TOOLS_DIR to iccDEV Build/Tools path}"
TESTING_DIR="${ICCDEV_TESTING_DIR:?Set ICCDEV_TESTING_DIR to iccDEV Testing path}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-namedcolor-overprint}"
mkdir -p "$OUTDIR"

APPLY="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
PROFILE="$TESTING_DIR/Named/NamedColor.icc"
DATA="$TESTING_DIR/Named/NamedColorSilverOnly.txt"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=4

echo "=== iccApplyNamedCmm overprint regression ==="

if [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyNamedCmm not found at $APPLY"
  exit 0
fi
if [ ! -f "$PROFILE" ]; then
  echo "  [SKIP] $PROFILE not present (iccdev_profiles fixture not built?)"
  exit 0
fi
if [ ! -f "$DATA" ]; then
  echo "  [SKIP] $DATA not present"
  exit 0
fi

LOG_W="$OUTDIR/silver-overwhite.log"
LOG_B="$OUTDIR/silver-overblack.log"
LOG_G="$OUTDIR/silver-overgray.log"

# Wraps a single iccApplyNamedCmm invocation; surfaces ASAN/UBSAN findings.
run_apply() {
  local intent="$1"; local logpath="$2"; local ec=0
  timeout 30 "$APPLY" "$DATA" 0 1 "$PROFILE" "$intent" >"$logpath" 2>&1 || ec=$?
  if grep -q "ERROR: AddressSanitizer" "$logpath" 2>/dev/null; then
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
  fi
  if grep -q "runtime error:" "$logpath" 2>/dev/null; then
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
  fi
  return $ec
}

# Extract the Silver row's reflectance mean.  Output line shape:
#   <36 floats>\t;{ "Silver" }
# Mean = sum of numeric fields / count.  Returns empty if no Silver row was
# emitted (e.g. because the apply failed before producing output).
silver_mean() {
  local logpath="$1"
  awk '
    /Silver/ {
      sum=0; n=0
      for (i = 1; i <= NF; i++) {
        if ($i ~ /^-?[0-9]+(\.[0-9]+)?$/) { sum += $i; n++ }
      }
      if (n > 0) printf "%.4f\n", sum / n
      exit
    }
  ' "$logpath"
}

run_apply 3       "$LOG_W"; EC_W=$?
run_apply 1000003 "$LOG_B"; EC_B=$?
run_apply 2000003 "$LOG_G"; EC_G=$?

MEAN_W="$(silver_mean "$LOG_W")"
MEAN_B="$(silver_mean "$LOG_B")"

# Test 1: over-white succeeded, produced a high-reflectance spectrum.
if [ "$EC_W" -eq 0 ] && [ -n "$MEAN_W" ] && \
   awk -v m="$MEAN_W" 'BEGIN{ exit (m > 0.7) ? 0 : 1 }'; then
  echo "  [PASS] over-white  intent=3        Silver mean=$MEAN_W (> 0.7)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] over-white  intent=3        ec=$EC_W mean=${MEAN_W:-<missing>} (expected ec=0, mean>0.7)"
  sed -n '1,10p' "$LOG_W"
  FAIL=$((FAIL + 1))
fi

# Test 2: over-black succeeded, produced a low-reflectance spectrum.
if [ "$EC_B" -eq 0 ] && [ -n "$MEAN_B" ] && \
   awk -v m="$MEAN_B" 'BEGIN{ exit (m < 0.5) ? 0 : 1 }'; then
  echo "  [PASS] over-black  intent=1000003  Silver mean=$MEAN_B (< 0.5)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] over-black  intent=1000003  ec=$EC_B mean=${MEAN_B:-<missing>} (expected ec=0, mean<0.5)"
  sed -n '1,10p' "$LOG_B"
  FAIL=$((FAIL + 1))
fi

# Test 3: over-white and over-black spectra differ substantially.
if [ -n "$MEAN_W" ] && [ -n "$MEAN_B" ] && \
   awk -v w="$MEAN_W" -v b="$MEAN_B" 'BEGIN{ d=w-b; if(d<0) d=-d; exit (d > 0.3) ? 0 : 1 }'; then
  DELTA="$(awk -v w="$MEAN_W" -v b="$MEAN_B" 'BEGIN{printf "%.4f", w-b}')"
  echo "  [PASS] white-black delta-mean      = $DELTA (> 0.3; spec vs spcb really differ)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] white-black delta-mean too small or missing"
  FAIL=$((FAIL + 1))
fi

# Test 4: over-gray fails clean (no 'spcg' member on Silver in this profile).
if [ "$EC_G" -ne 0 ]; then
  if grep -qi "Profile application failed\|BadTintXform\|iccConnect:" "$LOG_G" 2>/dev/null; then
    echo "  [PASS] over-gray   intent=2000003  failed cleanly (ec=$EC_G; reports failure)"
  else
    echo "  [PASS] over-gray   intent=2000003  failed (ec=$EC_G; no message but non-zero)"
  fi
  PASS=$((PASS + 1))
else
  echo "  [FAIL] over-gray   intent=2000003  unexpectedly succeeded (expected fail-clean on missing 'spcg')"
  sed -n '1,10p' "$LOG_G"
  FAIL=$((FAIL + 1))
fi

echo "iccApplyNamedCmm overprint regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$ASAN_FINDINGS" -ne 0 ] || [ "$UBSAN_FINDINGS" -ne 0 ]; then
  exit 2
fi
if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
exit 0
