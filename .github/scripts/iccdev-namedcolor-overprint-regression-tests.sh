#!/bin/bash
###############################################################################
# iccDEV iccApplyNamedCmm overprint-variant regression tests
###############################################################################
#
# Exercises the v5 NamedColor overprint variants surfaced by the
# CIccCreateNamedColorXformHint::nOverprintType plumbing, on both axes:
#
# Overprint axis (which spectral array member is read):
#   - over-white (default)                                  -> 'spec'
#   - over-black (transform=*OnBlack / intent +1000000)     -> 'spcb'
#   - over-gray  (transform=*OnGray  / intent +2000000)     -> 'spcg'
#
# PCS-side axis (JSON only -- legacy CLI is auto via useD2BxB2Dx):
#   - named*              -- heuristic: spectralPCS when useD2BxB2Dx and
#                            spectralPCS is declared, otherwise PCS
#   - namedSpectral*      -- pinned to the spectral array member chosen
#                            by the overprint suffix; fails clean with
#                            icCmmStatBadTintXform if that member is
#                            absent for the matched entry
#   - namedColorimetric*  -- pinned to nmclPcsDataMbr; fails clean with
#                            icCmmStatBadTintXform if that member is
#                            absent for the matched entry (no silent
#                            integration from spectral)
#   - namedDevice         -- pinned to nmclDeviceDataMbr (v5 array) or
#                            the deviceCoords struct field (v4 NamedColor2);
#                            fails clean with icCmmStatBadTintXform when
#                            no device member is available for the entry.
#                            No overprint suffix is accepted.
#
# Test profile: Testing/Named/NamedColor.icc, rebuilt from NamedColor.xml
# by Testing/CreateAllProfiles.* (the iccdev_profiles fixture).  Two
# named-color entries cover the cases the script exercises:
#   - Silver: has 'spec' (~0.83) and 'spcb' (~0.30) spectral members
#             but NO nmclPcsDataMbr and NO 'spcg'.
#   - Red:    has nmclPcsDataMbr (last row L=47.9 a=73.5 b=60.6 at tint=1)
#             plus 'spec'; no 'spcb' or 'spcg'.
# Together they let the script exercise both halves of the strict
# semantics: namedSpectral* fails clean on missing spectral members,
# namedColorimetric* succeeds on Red and fails clean on Silver.
#
# Legacy-CLI assertions (intent code, useD2BxB2Dx implicit):
#   1) over-white succeeds and produces high-reflectance spectrum
#   2) over-black succeeds and produces low-reflectance spectrum
#   3) the two spectra differ substantially (delta-mean > 0.3)
#   4) over-gray fails clean (apply exits non-zero; no silent fallback)
#
# JSON-cfg assertions (added after the legacy block):
#   5) namedSpectral on Silver           -- high-reflectance spectrum (matches #1)
#   6) namedSpectralOnBlack on Silver    -- low-reflectance spectrum  (matches #2)
#   7) namedSpectralOnGray on Silver     -- fails clean (no 'spcg' on Silver)
#   8) namedColorimetric on Silver       -- fails clean (no nmclPcsDataMbr)
#   9) namedColorimetricOnBlack on Silver-- fails clean (no nmclPcsDataMbr;
#                                            overprint suffix does not matter
#                                            for the colorimetric stem)
#  10) namedColorimetric on Red          -- produces Lab matching the
#                                            tint=1 row of Red's nmclPcsDataMbr
#                                            (L ~= 47.9, |L-47.9| < 0.5)
#  11) namedDevice on Silver             -- fails clean (no nmclDeviceDataMbr
#                                            on any entry of the v5
#                                            NamedColor.icc fixture; this
#                                            guards the v5 array path's
#                                            BadTintXform surfacing)
#  12) tint echo in dump (S5/S6/S7 regression) -- the test-5 log must
#                                            carry a numeric tint token
#                                            after `;{ "Silver" }`, not
#                                            silently drop it (used to
#                                            be suppressed when tint==1).
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
TOTAL=12

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

# Extract the Silver row's reflectance mean.  Output line shape (with the
# tint-echo fix in place):
#   <36 floats>\t;{ "Silver" }    <tint>
# Mean = sum of numeric fields / count, stopping at the ';' comment
# marker so the trailing tint token does not pollute the spectral mean.
# Returns empty if no Silver row was emitted (e.g. because the apply
# failed before producing output).
silver_mean() {
  local logpath="$1"
  awk '
    /Silver/ {
      sum=0; n=0
      for (i = 1; i <= NF; i++) {
        if ($i ~ /^;/) break
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

###############################################################################
# JSON-cfg variants: exercise the new namedSpectral* / namedColorimetric*
# transform names through iccApplyNamedCmm -cfg.
###############################################################################

# Build a one-stage config that names $color_name at tint=1.0 and
# routes it through $PROFILE with the requested transform.
#   $1: dst encoding ("float" for spectral, "value" for Lab/XYZ)
#   $2: transform name
#   $3: named color name (e.g. "Silver", "Red")
#   $4: output cfg path
write_cfg() {
  local dst_encoding="$1"; local transform_name="$2"
  local color_name="$3"; local cfg_path="$4"
  cat > "$cfg_path" <<EOF
{
 "colorData": {
  "data": [ { "n": "$color_name", "v": [ 1.0 ] } ],
  "encoding": "percent",
  "srcEncoding": "value",
  "srcSpace": "nmcl"
 },
 "dataFiles": {
  "dstEncoding": "$dst_encoding",
  "dstType": "legacy",
  "srcFile": null,
  "srcType": "colorData"
 },
 "profileSequence": [
  {
   "iccFile": "$PROFILE",
   "intent": "absolute",
   "transform": "$transform_name"
  }
 ]
}
EOF
}

# Wraps a JSON-cfg iccApplyNamedCmm invocation; mirrors run_apply's sanitizer
# bookkeeping so ASAN/UBSAN findings still surface in the script's exit code.
run_apply_cfg() {
  local cfg_path="$1"; local logpath="$2"; local ec=0
  timeout 30 "$APPLY" -cfg "$cfg_path" >"$logpath" 2>&1 || ec=$?
  if grep -q "ERROR: AddressSanitizer" "$logpath" 2>/dev/null; then
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
  fi
  if grep -q "runtime error:" "$logpath" 2>/dev/null; then
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
  fi
  return $ec
}

# Extract just the L* of the matched-color row (first numeric token
# before the ';' comment marker).  Lab output from a one-stage
# NamedColor profile in icEncodeValue carries Lab directly:
# "<L>  <a>  <b>  ;{ \"$2\" }    <tint>".
#   $1: log path
#   $2: color name (matched as a substring on the row's trailing comment)
first_value() {
  local logpath="$1"; local color_name="$2"
  awk -v color="$color_name" '
    index($0, color) {
      for (i = 1; i <= NF; i++) {
        if ($i ~ /^;/) break
        if ($i ~ /^-?[0-9]+(\.[0-9]+)?$/) { printf "%.4f\n", $i; exit }
      }
      exit
    }
  ' "$logpath"
}

CFG_NS_W="$OUTDIR/cfg-namedSpectral-Silver.json"
CFG_NS_B="$OUTDIR/cfg-namedSpectralOnBlack-Silver.json"
CFG_NS_G="$OUTDIR/cfg-namedSpectralOnGray-Silver.json"
CFG_NC_W="$OUTDIR/cfg-namedColorimetric-Silver.json"
CFG_NC_B="$OUTDIR/cfg-namedColorimetricOnBlack-Silver.json"
CFG_NC_R="$OUTDIR/cfg-namedColorimetric-Red.json"
CFG_ND_S="$OUTDIR/cfg-namedDevice-Silver.json"

LOG_NS_W="$OUTDIR/cfg-namedSpectral-Silver.log"
LOG_NS_B="$OUTDIR/cfg-namedSpectralOnBlack-Silver.log"
LOG_NS_G="$OUTDIR/cfg-namedSpectralOnGray-Silver.log"
LOG_NC_W="$OUTDIR/cfg-namedColorimetric-Silver.log"
LOG_NC_B="$OUTDIR/cfg-namedColorimetricOnBlack-Silver.log"
LOG_NC_R="$OUTDIR/cfg-namedColorimetric-Red.log"
LOG_ND_S="$OUTDIR/cfg-namedDevice-Silver.log"

write_cfg "float" "namedSpectral"            "Silver" "$CFG_NS_W"
write_cfg "float" "namedSpectralOnBlack"     "Silver" "$CFG_NS_B"
write_cfg "float" "namedSpectralOnGray"      "Silver" "$CFG_NS_G"
write_cfg "value" "namedColorimetric"        "Silver" "$CFG_NC_W"
write_cfg "value" "namedColorimetricOnBlack" "Silver" "$CFG_NC_B"
write_cfg "value" "namedColorimetric"        "Red"    "$CFG_NC_R"
write_cfg "value" "namedDevice"              "Silver" "$CFG_ND_S"

run_apply_cfg "$CFG_NS_W" "$LOG_NS_W"; EC_NS_W=$?
run_apply_cfg "$CFG_NS_B" "$LOG_NS_B"; EC_NS_B=$?
run_apply_cfg "$CFG_NS_G" "$LOG_NS_G"; EC_NS_G=$?
run_apply_cfg "$CFG_NC_W" "$LOG_NC_W"; EC_NC_W=$?
run_apply_cfg "$CFG_NC_B" "$LOG_NC_B"; EC_NC_B=$?
run_apply_cfg "$CFG_NC_R" "$LOG_NC_R"; EC_NC_R=$?
run_apply_cfg "$CFG_ND_S" "$LOG_ND_S"; EC_ND_S=$?

MEAN_NS_W="$(silver_mean "$LOG_NS_W")"
MEAN_NS_B="$(silver_mean "$LOG_NS_B")"
L_NC_R="$(first_value "$LOG_NC_R" "Red")"

# Test 5: JSON namedSpectral on Silver -- spectral output, high reflectance.
# Equivalent to legacy intent=3 because both end up reading 'spec'.
if [ "$EC_NS_W" -eq 0 ] && [ -n "$MEAN_NS_W" ] && \
   awk -v m="$MEAN_NS_W" 'BEGIN{ exit (m > 0.7) ? 0 : 1 }'; then
  echo "  [PASS] cfg namedSpectral Silver           mean=$MEAN_NS_W (> 0.7)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedSpectral Silver           ec=$EC_NS_W mean=${MEAN_NS_W:-<missing>} (expected ec=0, mean>0.7)"
  sed -n '1,10p' "$LOG_NS_W"
  FAIL=$((FAIL + 1))
fi

# Test 6: JSON namedSpectralOnBlack on Silver -- spectral output, low reflectance.
if [ "$EC_NS_B" -eq 0 ] && [ -n "$MEAN_NS_B" ] && \
   awk -v m="$MEAN_NS_B" 'BEGIN{ exit (m < 0.5) ? 0 : 1 }'; then
  echo "  [PASS] cfg namedSpectralOnBlack Silver    mean=$MEAN_NS_B (< 0.5)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedSpectralOnBlack Silver    ec=$EC_NS_B mean=${MEAN_NS_B:-<missing>} (expected ec=0, mean<0.5)"
  sed -n '1,10p' "$LOG_NS_B"
  FAIL=$((FAIL + 1))
fi

# Test 7: JSON namedSpectralOnGray on Silver -- fail clean (no 'spcg' on Silver).
if [ "$EC_NS_G" -ne 0 ]; then
  echo "  [PASS] cfg namedSpectralOnGray Silver     failed cleanly (ec=$EC_NS_G)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedSpectralOnGray Silver     unexpectedly succeeded (expected fail-clean on missing 'spcg')"
  sed -n '1,10p' "$LOG_NS_G"
  FAIL=$((FAIL + 1))
fi

# Test 8: JSON namedColorimetric on Silver -- fail clean (no nmclPcsDataMbr;
# the colorimetric stem does not integrate from spectral members).
if [ "$EC_NC_W" -ne 0 ]; then
  echo "  [PASS] cfg namedColorimetric Silver       failed cleanly (ec=$EC_NC_W; no nmclPcsDataMbr)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedColorimetric Silver       unexpectedly succeeded (expected fail-clean on missing nmclPcsDataMbr)"
  sed -n '1,10p' "$LOG_NC_W"
  FAIL=$((FAIL + 1))
fi

# Test 9: JSON namedColorimetricOnBlack on Silver -- fail clean for the
# same reason as test 8.  The overprint suffix does not change which
# member the colorimetric stem reads (it still reads nmclPcsDataMbr).
if [ "$EC_NC_B" -ne 0 ]; then
  echo "  [PASS] cfg namedColorimetricOnBlack Silver  failed cleanly (ec=$EC_NC_B; no nmclPcsDataMbr)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedColorimetricOnBlack Silver  unexpectedly succeeded (expected fail-clean on missing nmclPcsDataMbr)"
  sed -n '1,10p' "$LOG_NC_B"
  FAIL=$((FAIL + 1))
fi

# Test 10: JSON namedColorimetric on Red -- read nmclPcsDataMbr and return
# the tint=1 row directly.  Red's nmclPcsDataMbr last row in NamedColor.xml
# is L=47.90215 a=73.46014 b=60.61409 -- expect L within +-0.5 of 47.90.
if [ "$EC_NC_R" -eq 0 ] && [ -n "$L_NC_R" ] && \
   awk -v l="$L_NC_R" 'BEGIN{ d=l-47.90; if(d<0) d=-d; exit (d < 0.5) ? 0 : 1 }'; then
  echo "  [PASS] cfg namedColorimetric Red          L*=$L_NC_R (within 0.5 of 47.90)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedColorimetric Red          ec=$EC_NC_R L*=${L_NC_R:-<missing>} (expected ec=0, |L-47.90|<0.5)"
  sed -n '1,10p' "$LOG_NC_R"
  FAIL=$((FAIL + 1))
fi

# Test 11: JSON namedDevice on Silver -- fail clean.  No entry in the v5
# NamedColor.icc fixture carries nmclDeviceDataMbr, and the profile
# header's DataColourSpace is empty, so the v5 array path's
# GetDeviceTint returns false and Apply surfaces icCmmStatBadTintXform.
# This is the v5 negative guard; a v4 success case would need a
# NamedColor2 profile with deviceCoords (e.g. NamedColorV4.icc) and is
# left for a separate fixture if/when one is part of the iccdev_profiles
# bundle.
if [ "$EC_ND_S" -ne 0 ]; then
  echo "  [PASS] cfg namedDevice Silver             failed cleanly (ec=$EC_ND_S; no nmclDeviceDataMbr)"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] cfg namedDevice Silver             unexpectedly succeeded (expected fail-clean on missing nmclDeviceDataMbr)"
  sed -n '1,10p' "$LOG_ND_S"
  FAIL=$((FAIL + 1))
fi

# Test 12: tint-echo guard (S5/S6/S7 dump regression).  Re-use the test-5
# log: that run drove namedSpectral on Silver tint=1.0, so the dump row
# must end with `;{ "Silver" }    <tint>`.  Earlier code suppressed the
# tint token whenever the value was exactly 1.0, which made S5/S6/S7
# look as though no tint had been supplied; this assertion fails if that
# regression returns or if the tool stops copying pData->m_values into
# the output entry's m_srcValues at all.
if grep -E 'Silver"[[:space:]]+\}[[:space:]]+[0-9]+\.[0-9]+' "$LOG_NS_W" > /dev/null 2>&1; then
  echo "  [PASS] tint echo                          { \"Silver\" } row in test-5 log carries a tint token"
  PASS=$((PASS + 1))
else
  echo "  [FAIL] tint echo                          { \"Silver\" } row in test-5 log has no tint token (S5/S6/S7 regression)"
  sed -n '/Silver/p' "$LOG_NS_W"
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
