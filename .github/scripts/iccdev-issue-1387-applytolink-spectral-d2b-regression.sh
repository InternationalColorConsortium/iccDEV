#!/bin/bash
###############################################################################
# iccDEV issue #1387 - iccApplyToLink spectral DToB channel mismatch regression
###############################################################################
#
# SpecRef/SixChanInputRef.icc advertises colorimetric PCS XYZ and spectral PCS
# reflectance. Its AToB3 tag is 6->3, while its DToB3 tag is 6->36. When the
# caller requests "without D2Bx/B2Dx" via intent 11 or 13, CIccCmm::AddXform()
# exposes XYZ as the CMM destination space. CIccXform::Create() must therefore
# honor that same D2B opt-out and not select the 6->36 DToB3 MPE; otherwise
# CIccMpeMatrix::Apply() writes 36 floats into an iccApplyToLink buffer sized
# for the 3-sample XYZ destination.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated profile / logs
#
# Exit codes:
#   0 - pass
#   2 - missing fixture/tooling, fixture drift, sanitizer finding, unexpected
#       tool failure, or crash
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1387}"
mkdir -p "$OUTDIR"

fail()
{
  echo "[FAIL] $1"
  exit 2
}

require_pattern()
{
  pattern="$1"
  description="$2"
  file="$3"
  if ! grep -Eq "$pattern" "$file"; then
    echo "[FAIL] fixture drift: missing $description"
    echo "       pattern: $pattern"
    echo "       log: $file"
    exit 2
  fi
}

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
APPLYTOLINK="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  fail "iccFromXml not found under $TOOLS_DIR"
fi
if [ -z "$APPLYTOLINK" ] || [ ! -x "$APPLYTOLINK" ]; then
  fail "iccApplyToLink not found under $TOOLS_DIR"
fi
if [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  fail "iccDumpProfile not found under $TOOLS_DIR"
fi

XML="$REPO_ROOT/Testing/SpecRef/SixChanInputRef.xml"
ICC="$OUTDIR/SixChanInputRef.icc"
if [ ! -f "$XML" ]; then
  fail "required XML source missing: $XML"
fi

"$FROMXML" "$XML" "$ICC" > "$OUTDIR/fromxml.log" 2>&1
fromxml_status=$?
if [ "$fromxml_status" -ne 0 ] || [ ! -s "$ICC" ]; then
  fail "could not generate trigger profile (exit $fromxml_status, see $OUTDIR/fromxml.log)"
fi

"$DUMP" -v 100 "$ICC" ALL > "$OUTDIR/dump.log" 2>&1
dump_status=$?
if grep -Eq "AddressSanitizer:|runtime error:|UndefinedBehaviorSanitizer" "$OUTDIR/dump.log"; then
  fail "iccDumpProfile triggered sanitizer output while checking fixture shape"
fi
if [ "$dump_status" -ne 0 ]; then
  fail "iccDumpProfile failed while checking fixture shape (exit $dump_status, see $OUTDIR/dump.log)"
fi

require_pattern "Data Color Space:[[:space:]]+MCH6Data/6ColorData" "6-channel data color space" "$OUTDIR/dump.log"
require_pattern "PCS Color Space:[[:space:]]+XYZData" "XYZ PCS" "$OUTDIR/dump.log"
require_pattern "Spectral PCS:[[:space:]]+0x0024ChannelReflectanceData" "36-channel spectral PCS" "$OUTDIR/dump.log"
require_pattern "AToB3Tag[[:space:]]+'A2B3'" "AToB3 tag" "$OUTDIR/dump.log"
require_pattern "DToB3Tag[[:space:]]+'D2B3'" "DToB3 tag" "$OUTDIR/dump.log"
require_pattern "Matrix Element \\('matf' = 6D617466\\) 6->3" "AToB3 6-to-3 matrix" "$OUTDIR/dump.log"
require_pattern "Matrix Element \\('matf' = 6D617466\\) 6->36" "DToB3 6-to-36 matrix" "$OUTDIR/dump.log"
echo "[PASS] SixChanInputRef fixture retains dual PCS 6->3 / 6->36 trigger shape"

status=0
# Use valid input ranges so this regression reaches the #1387 D2B opt-out path;
# malformed ranges are covered separately by the NaN/SixChanInputRef test.
for case_args in "11 0" "13 0"; do
  intent="${case_args%% *}"
  min_range="${case_args#* }"
  LOG="$OUTDIR/applytolink-intent-${intent}.log"
  LINK="$OUTDIR/link-${intent}.icc"
  ASAN_OPTIONS="print_scariness=1:halt_on_error=1:detect_leaks=0" \
  UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
    "$APPLYTOLINK" "$LINK" 0 2 1 Issue1387 "$min_range" 1 1 0 \
      "$ICC" "$intent" > "$LOG" 2>&1
  tool_status=$?

  if grep -Eq "AddressSanitizer:|runtime error:|UndefinedBehaviorSanitizer" "$LOG"; then
    echo "[FAIL] iccApplyToLink triggered sanitizer output for intent $intent (#1387)"
    grep -E "AddressSanitizer:|runtime error:|SUMMARY:|CIccMpeMatrix::Apply" "$LOG" | head
    status=2
  elif [ "$tool_status" -gt 127 ]; then
    echo "[FAIL] iccApplyToLink crashed for intent $intent with exit $tool_status (#1387)"
    status=2
  elif [ "$tool_status" -ne 0 ]; then
    echo "[FAIL] iccApplyToLink failed for intent $intent with exit $tool_status (#1387)"
    sed -n '1,20p' "$LOG"
    status=2
  elif [ ! -s "$LINK" ]; then
    echo "[FAIL] iccApplyToLink did not create output profile for intent $intent (#1387)"
    status=2
  else
    LINK_DUMP="$OUTDIR/link-${intent}-dump.log"
    "$DUMP" -v 100 "$LINK" ALL > "$LINK_DUMP" 2>&1
    if grep -Eq "AddressSanitizer:|runtime error:|UndefinedBehaviorSanitizer" "$LINK_DUMP"; then
      echo "[FAIL] iccDumpProfile triggered sanitizer output for generated link intent $intent (#1387)"
      status=2
    elif ! grep -Eq "Data Color Space:[[:space:]]+MCH6Data/6ColorData" "$LINK_DUMP" ||
         ! grep -Eq "PCS Color Space:[[:space:]]+XYZData" "$LINK_DUMP" ||
         ! grep -Eq "AToB0Tag[[:space:]]+'A2B0'" "$LINK_DUMP" ||
         ! grep -Eq "MPE Element Chain: 1 elements, 6->3 channels" "$LINK_DUMP"; then
      echo "[FAIL] generated link for intent $intent did not retain 6-to-3 colorimetric output (#1387)"
      sed -n '1,80p' "$LINK_DUMP"
      status=2
    else
      echo "[PASS] iccApplyToLink intent $intent completed and wrote 6-to-3 XYZ output"
    fi
  fi
done

exit "$status"
