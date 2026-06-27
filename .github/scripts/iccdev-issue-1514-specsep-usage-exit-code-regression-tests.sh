#!/bin/bash
###############################################################################
# iccSpecSepToTiff usage-path exit-code regression (#1514)
###############################################################################
#
# #1514: invoking iccSpecSepToTiff with too few arguments printed Usage() and
# returned 0 -- a success status for what is actually a malformed invocation,
# so wrapper scripts / CI could not distinguish a no-op from a real conversion.
# The fix makes the argc<minargs path return -1 (process status 255), matching
# every other error exit in main().
#
# This test asserts:
#   1. no-args            -> non-zero exit (the #1514 fix) + Usage printed
#   2. too-few-args       -> non-zero exit (the #1514 fix) + Usage printed
#   3. valid argc, missing input prefix -> exit 255 + "Cannot open input"
#      (regression guard: the real-error path must keep failing non-zero)
#
# Before the fix, cases 1 and 2 exit 0 and this script FAILs (red); after the
# fix they exit 255 and it PASSes (green).
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-specsep-usage-exit-code-regression}"
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

SPECSEP="$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff"
LOGFILE="$OUTDIR/specsep-usage-exit-code.log"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  echo "  [FAIL] $1 -- $2"
  FAIL=$((FAIL + 1))
}

pass_case() {
  echo "  [PASS] $1 -- $2"
  PASS=$((PASS + 1))
}

check_sanitizers() {
  local name="$1"
  local log="$2"
  if grep -Eq "ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in tool output"
    return 1
  fi
  return 0
}

# Cases 1 & 2: malformed (too few args) must fail non-zero AND still print Usage.
run_usage_nonzero() {
  local name="$1"; shift
  TOTAL=$((TOTAL + 1))
  local exit_code=0
  timeout 60 "$SPECSEP" "$@" > "$LOGFILE" 2>&1 || exit_code=$?

  check_sanitizers "$name" "$LOGFILE" || { sed -n '1,20p' "$LOGFILE"; return; }

  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "expected non-zero exit for malformed invocation, got exit=0 (#1514)"
    sed -n '1,20p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "Usage:" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing Usage text on malformed invocation"
    sed -n '1,20p' "$LOGFILE"
    return
  fi

  pass_case "$name" "malformed invocation rejected with exit=$exit_code and Usage printed"
}

# Case 3: well-formed argc but the input prefix resolves to no files -- the
# pre-existing error path that already returned -1; guard it stays non-zero.
run_missing_input_reject() {
  local name="iccSpecSepToTiff missing-input-prefix still exits non-zero"
  TOTAL=$((TOTAL + 1))
  local exit_code=0
  local out="$OUTDIR/should-not-exist.tif"
  rm -f "$out"
  # 8 args (no profile): output compress sep prefix start end incr
  timeout 60 "$SPECSEP" "$out" 0 0 "$OUTDIR/no-such-prefix_" 0 0 1 > "$LOGFILE" 2>&1 || exit_code=$?

  check_sanitizers "$name" "$LOGFILE" || { sed -n '1,20p' "$LOGFILE"; return; }

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected exit=255 for missing input, got exit=$exit_code"
    sed -n '1,20p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "Cannot open input" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing 'Cannot open input' diagnostic"
    sed -n '1,20p' "$LOGFILE"
    return
  fi

  if [ -e "$out" ]; then
    fail_case "$name" "output TIFF created despite missing input"
    return
  fi

  pass_case "$name" "missing input rejected with exit=255"
}

if [ ! -x "$SPECSEP" ]; then
  echo "iccSpecSepToTiff not found at $SPECSEP -- skipping (build Tools first)"
  exit 0
fi

echo "=== iccSpecSepToTiff usage-path exit-code regression (#1514) ==="
run_usage_nonzero "iccSpecSepToTiff no-args exits non-zero"
run_usage_nonzero "iccSpecSepToTiff too-few-args exits non-zero" foo.bar 0 0
run_missing_input_reject
echo "iccSpecSepToTiff usage-path exit-code regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
