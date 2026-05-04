#!/bin/bash
###############################################################################
# iccDEV invalid BCD profile version regression tests
###############################################################################
#
# Exercises ICC header version bytes with non-BCD nibbles. Malformed BCD values
# must be reported as invalid instead of being decoded as decimal-looking ICC
# versions such as 141.91.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-version-bcd-regressions}"
mkdir -p "$OUTDIR"

DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
MUTATED="$OUTDIR/invalid-bcd-version.icc"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=0

check_sanitizers() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    echo "  [ASAN] $name -- AddressSanitizer finding"
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    echo "  [UBSAN] $name -- undefined behavior"
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    return 1
  fi

  return 0
}

fail_case() {
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

pass_case() {
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

require_text() {
  local name="$1"
  local logfile="$2"
  local expected="$3"

  if ! grep -Fq "$expected" "$logfile" 2>/dev/null; then
    fail_case "$name" "missing expected text: $expected"
    sed -n '1,40p' "$logfile"
    return 1
  fi

  return 0
}

run_invalid_bcd_dump() {
  local name="invalid-bcd-version-dump"
  local logfile="$OUTDIR/$name.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$MUTATED" "$logfile"

  if [ ! -x "$DUMP" ]; then
    fail_case "$name" "missing executable $DUMP"
    return
  fi

  if [ ! -f "$PROFILE" ]; then
    fail_case "$name" "missing profile $PROFILE"
    return
  fi

  if ! python3 -c 'import sys; src, dst = sys.argv[1:3]; data = bytearray(open(src, "rb").read()); assert len(data) >= 128; data[8:12] = bytes.fromhex("DB91BA7B"); data[120:124] = b"test"; open(dst, "wb").write(data)' "$PROFILE" "$MUTATED"; then
    fail_case "$name" "failed to create mutated profile"
    return
  fi

  timeout 30 "$DUMP" -v "$MUTATED" ALL > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    return
  fi

  require_text "$name" "$logfile" "Version:            Invalid BCD version 0xDB91BA7B" || return
  require_text "$name" "$logfile" "SubClass Version:   Invalid BCD subclass version 0xBA7B" || return
  require_text "$name" "$logfile" "Profile violates ICC specification for version Invalid BCD version 0xDB91BA7B" || return
  require_text "$name" "$logfile" "Warning! - Version number 0xDB91BA7B contains non-BCD digit(s)." || return

  if grep -Fq "Version:            141.91" "$logfile" 2>/dev/null; then
    fail_case "$name" "invalid BCD value was decoded as 141.91"
    return
  fi

  pass_case "$name" "invalid BCD version rejected with explicit diagnostics"
}

run_valid_bcd_smoke() {
  local name="valid-bcd-version-smoke"
  local logfile="$OUTDIR/$name.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile"

  if [ ! -x "$DUMP" ]; then
    fail_case "$name" "missing executable $DUMP"
    return
  fi

  if [ ! -f "$PROFILE" ]; then
    fail_case "$name" "missing profile $PROFILE"
    return
  fi

  timeout 30 "$DUMP" -v "$PROFILE" ALL > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "valid profile exited $exit_code"
    sed -n '1,40p' "$logfile"
    return
  fi

  require_text "$name" "$logfile" "Version:            4.20" || return
  require_text "$name" "$logfile" "Profile is valid for version 4.20" || return

  if grep -Fq "Invalid BCD" "$logfile" 2>/dev/null; then
    fail_case "$name" "valid BCD profile reported invalid BCD text"
    return
  fi

  pass_case "$name" "valid BCD version formatting preserved"
}

echo "=== invalid BCD profile version regression ==="

run_invalid_bcd_dump
run_valid_bcd_smoke

echo "invalid BCD profile version regression: $PASS passed, $FAIL failed, $TOTAL total"
echo "sanitizer findings: ASAN=$ASAN_FINDINGS UBSAN=$UBSAN_FINDINGS"

if [ "$FAIL" -ne 0 ] || [ "$ASAN_FINDINGS" -ne 0 ] || [ "$UBSAN_FINDINGS" -ne 0 ]; then
  exit 1
fi

exit 0
