#!/bin/bash
###############################################################################
# iccApplyNamedCmm command-line argument regression tests
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for generated logs/configs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-applynamedcmm-cli-args}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyNamedCmm -type f 2>/dev/null | head -1)"
DATA="$TESTING_DIR/ApplyDataFiles/rgb8bit.txt"
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
# #2124: sRGB_v4 has a Lab PCS and sRGB_D65_MAT an XYZ PCS, so the two give the
# encoding cases below a Lab and an XYZ destination. Both are tracked in git,
# which keeps this test free of the generated-profile fixture.
XYZP="$TESTING_DIR/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
CFG="$OUTDIR/minimal-cfg.json"
LABSRC="$OUTDIR/lab-percent-src.txt"

fail() {
  echo "  [FAIL] iccApplyNamedCmm-cli-args -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,120p' "$logfile"
    return 1
  fi

  return 0
}

require_file() {
  local path="$1"

  if [ ! -f "$path" ]; then
    fail "missing fixture: $path"
  fi
}

require_tool() {
  local path="$1"

  if [ ! -x "$path" ]; then
    fail "missing executable: $path"
  fi
}

run_expect_success() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ne 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name failed with exit=$rc"
  fi
}

run_expect_reject() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "$name crashed with signal $((rc - 128))"
  fi
  if [ "$rc" -eq 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name accepted malformed arguments"
  fi
}

require_tool "$APPLY"
require_file "$DATA"
require_file "$SRGB"
require_file "$XYZP"

cat > "$CFG" <<EOF_CFG
{
  "dataFiles": {
    "srcType": "legacy",
    "srcFile": "$DATA",
    "dstType": "legacy",
    "dstEncoding": "value"
  },
  "profileSequence": [
    {
      "iccFile": "$SRGB",
      "intent": "relative",
      "useD2BxB2Dx": true
    }
  ]
}
EOF_CFG

echo "=== iccApplyNamedCmm CLI argument regression ==="

run_expect_success valid-legacy "$APPLY" "$DATA" 0 0 "$SRGB" 1
run_expect_success valid-cfg "$APPLY" -cfg "$CFG"

run_expect_reject cfg-extra "$APPLY" -cfg "$CFG" ignored-extra
run_expect_reject normal-extra "$APPLY" "$DATA" 0 0 "$SRGB" 1 ignored-extra
run_expect_reject precision-junk "$APPLY" "$DATA" 0:abc:def 0 "$SRGB" 1
run_expect_reject precision-huge "$APPLY" "$DATA" 0:999999:999999 0 "$SRGB" 1
run_expect_reject interp-junk "$APPLY" "$DATA" 0 junk "$SRGB" 1
run_expect_reject interp-out-of-range "$APPLY" "$DATA" 0 2 "$SRGB" 1
run_expect_reject intent-junk "$APPLY" "$DATA" 0 0 "$SRGB" 1junk
run_expect_reject env-short-name "$APPLY" "$DATA" 0 0 -ENV:x 1 "$SRGB" 1
run_expect_reject env-long-name "$APPLY" "$DATA" 0 0 -ENV:abcdefgh 1 "$SRGB" 1
run_expect_reject env-junk "$APPLY" "$DATA" 0 0 -ENV:wtpt 1junk "$SRGB" 1
run_expect_reject env-nan "$APPLY" "$DATA" 0 0 -ENV:wtpt nan "$SRGB" 1
run_expect_reject env-inf "$APPLY" "$DATA" 0 0 -ENV:wtpt inf "$SRGB" 1
run_expect_reject dangling-pcc "$APPLY" "$DATA" 0 0 "$SRGB" 1 -PCC

###############################################################################
# #2124: final_data_encoding is validated per colour space, not globally
#
# A Lab-destination run refusing icEncodePercent was read as icEncodePercent
# being broken. It is not. IccCmm.h's icFloatColorEncoding table lists the
# valid encodings per space and the 'Lab ' and 'XYZ ' entries exclude
# *different* ones, which CIccCmm::ToInternalEncoding()/FromInternalEncoding()
# implement as the per-space switches.
#
# The four cases below are deliberately a crossing pair: each destination
# rejects an encoding the other accepts. That is what distinguishes a
# per-space contract from a broken encoding -- a Lab-only case reads as a
# universal defect whichever way it lands, so testing one space cannot tell
# the two apart. If a future change collapses the per-space switches into a
# single shared list, exactly one half of each pair flips and this fails.
###############################################################################

# Every grep below is anchored with -x. The tool prints each diagnostic on a
# line of its own, and Usage() mentions the same restrictions in prose -- an
# unanchored match would be satisfied by usage output, so a regression that
# routed one of these invocations to usage-and-exit would still look green.
assert_rejected_encoding() {
  local name="$1" which="$2"

  grep -Fqx "Invalid $which data encoding" "$OUTDIR/$name.log" ||
    fail "$name did not report the $which encoding rejection"
}

# Lab destination: percent is not in its table, 8-bit is.
run_expect_reject enc-percent-lab-dst "$APPLY" "$DATA" 1 0 "$SRGB" 1
assert_rejected_encoding enc-percent-lab-dst final
run_expect_success enc-8bit-lab-dst "$APPLY" "$DATA" 4 0 "$SRGB" 1

# XYZ destination: the exact inverse -- percent is in its table, 8-bit is not.
run_expect_reject enc-8bit-xyz-dst "$APPLY" "$DATA" 4 0 "$XYZP" 1
assert_rejected_encoding enc-8bit-xyz-dst final
run_expect_success enc-percent-xyz-dst "$APPLY" "$DATA" 1 0 "$XYZP" 1
grep -Eqx "'XYZ '[[:space:]]*;[[:space:]]*Data Format" "$OUTDIR/enc-percent-xyz-dst.log" ||
  fail "enc-percent-xyz-dst did not reach an XYZ destination"
grep -Eqx "icEncodePercent[[:space:]]*;[[:space:]]*Encoding" "$OUTDIR/enc-percent-xyz-dst.log" ||
  fail "enc-percent-xyz-dst did not declare percent encoding"

# Accepting the selector is not the same as honouring it: assert the output is
# actually on the 0-100 scale, so the case cannot pass if percent silently
# degraded to the 0-1 float encoding.
#
# Scanned only over real data rows -- a row is a line carrying a standalone ';'
# field with numeric fields before it, which is the converted-value column;
# the ';' also stops the scan before the echoed 8-bit source column. Requiring
# at least one such row is what stops usage output satisfying this: Usage()
# prints selector lines like "   10 + Intent" that begin with a number but
# carry no ';' field at all.
#
# The two scales are exactly 100x apart -- against this fixture the largest
# converted value is 1.0889 under every non-percent encoding and 108.89 under
# percent -- so the threshold sits at 10.0, an order of magnitude clear of
# each. The obvious 1.1 would sit ~1% above the float maximum and go vacuous
# on any fixture or intent whose white point pushed a channel slightly higher.
awk '
  $0 ~ /;/ {
    numeric = 0
    for (i = 1; i <= NF && $i != ";"; i++) {
      if ($i ~ /^[+-]?[0-9]+\.?[0-9]*$/) {
        numeric++
        if (($i + 0) > 10.0) scaled = 1
      }
    }
    if (numeric > 0) rows++
  }
  END { exit (rows > 0 && scaled) ? 0 : 1 }
' "$OUTDIR/enc-percent-xyz-dst.log" ||
  fail "enc-percent-xyz-dst did not emit data rows on the 0-100 scale"

# The source side refuses percent for Lab too. Pinning both directions is what
# makes this a contract rather than a one-sided omission, and it is the check
# that would have caught the read as a bug: a genuinely broken encoding would
# not be refused symmetrically with a distinct diagnostic.
printf "'Lab ' ; Data Format\nicEncodePercent ; Encoding\n50.0 10.0 -10.0\n" > "$LABSRC"
run_expect_reject enc-percent-lab-src "$APPLY" "$LABSRC" 3 0 "$SRGB" 1
grep -Fq "Invalid source data encoding" "$OUTDIR/enc-percent-lab-src.log" ||
  fail "enc-percent-lab-src did not report the source encoding rejection"

echo "  [PASS] iccApplyNamedCmm-cli-args"
exit 0
