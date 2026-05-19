#!/bin/bash
###############################################################################
# iccDEV basic_string sanitizer regression tests
###############################################################################
#
# Issue:
#   https://github.com/InternationalColorConsortium/iccDEV/issues/1055
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-basic-string-regressions}"
mkdir -p "$OUTDIR"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

check_sanitizers() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- AddressSanitizer finding"
    sed -n '1,80p' "$logfile"
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- undefined behavior"
    sed -n '1,80p' "$logfile"
    return 1
  fi

  return 0
}

run_fromxml_success_test() {
  local name="$1"
  local xml_file="$2"
  local output_file="$OUTDIR/${name}.icc"
  local logfile="$OUTDIR/${name}.log"
  local xml_dir
  local xml_base
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  if [ ! -f "$xml_file" ]; then
    echo "  [FAIL] $name -- missing input $xml_file"
    FAIL=$((FAIL + 1))
    return
  fi

  xml_dir="$(cd "$(dirname "$xml_file")" && pwd)"
  xml_base="$(basename "$xml_file")"

  (
    cd "$xml_dir" &&
    timeout 30 "$FROMXML" "$xml_base" "$output_file"
  ) > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- timed out"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- crashed with signal $((exit_code - 128))"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ] || [ ! -s "$output_file" ]; then
    echo "  [FAIL] $name -- iccFromXml failed with exit=$exit_code"
    sed -n '1,20p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (iccFromXml converted without sanitizer findings)"
  PASS=$((PASS + 1))
}

if [ ! -x "$FROMXML" ]; then
  echo "ERROR: IccFromXml is required at $FROMXML"
  exit 1
fi

run_fromxml_success_test \
  "srgb-calc-plus-plus" \
  "$TESTING_DIR/Calc/srgbCalc++Test.xml"

run_fromxml_success_test \
  "rec2100-hlg-full" \
  "$TESTING_DIR/Display/Rec2100HlgFull.xml"

echo ""
echo "basic_string sanitizer regression summary:"
echo "  Total:  $TOTAL"
echo "  Passed: $PASS"
echo "  Failed: $FAIL"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
