#!/bin/bash
###############################################################################
# iccDEV multiLocalizedUnicodeType ISO code validation regression tests
###############################################################################
#
# Exercises warning-level validation for mluc languageCode/countryCode fields.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-mluc-iso-code-regressions}"
mkdir -p "$OUTDIR"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
SOURCE_XML="$TESTING_DIR/Display/sRGB_D65_MAT.xml"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

check_log() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- AddressSanitizer finding"
    FAIL=$((FAIL + 1))
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- undefined behavior"
    FAIL=$((FAIL + 1))
    return 1
  fi

  return 0
}

make_profile() {
  local name="$1"
  local lang_country="$2"
  local case_xml="$OUTDIR/${name}.xml"
  local case_icc="$OUTDIR/${name}.icc"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local exit_code=0

  sed "s|LanguageCountry=\"enUS\"|LanguageCountry=\"$lang_country\"|g" \
    "$SOURCE_XML" > "$case_xml"

  timeout 30 "$FROMXML" "$case_xml" "$case_icc" > "$fromxml_log" 2>&1 || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return 1
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$case_icc" ]; then
    echo "  [FAIL] $name -- iccFromXml failed with exit=$exit_code"
    sed -n '1,10p' "$fromxml_log"
    FAIL=$((FAIL + 1))
    return 1
  fi

  return 0
}

validate_profile() {
  local name="$1"
  local dump_log="$OUTDIR/${name}-dump.log"
  local exit_code=0

  timeout 30 "$DUMP" -v "$OUTDIR/${name}.icc" > "$dump_log" 2>&1 || exit_code=$?
  if ! check_log "$name/dump" "$dump_log"; then
    return 1
  fi
  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- iccDumpProfile timed out"
    FAIL=$((FAIL + 1))
    return 1
  fi
  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- iccDumpProfile signal exit=$exit_code"
    FAIL=$((FAIL + 1))
    return 1
  fi

  return 0
}

run_clean_test() {
  local name="$1"
  local lang_country="$2"
  local dump_log="$OUTDIR/${name}-dump.log"

  TOTAL=$((TOTAL + 1))
  rm -f "$OUTDIR/${name}.xml" "$OUTDIR/${name}.icc" "$dump_log"

  if ! make_profile "$name" "$lang_country"; then
    return
  fi
  if ! validate_profile "$name"; then
    return
  fi

  if grep -Eq "ISO-639|ISO-3166" "$dump_log"; then
    echo "  [FAIL] $name -- unexpected ISO warning"
    grep -E "ISO-639|ISO-3166" "$dump_log" | sed -n '1,10p'
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name"
  PASS=$((PASS + 1))
}

run_warning_test() {
  local name="$1"
  local lang_country="$2"
  local expected_one="$3"
  local expected_two="$4"
  local dump_log="$OUTDIR/${name}-dump.log"

  TOTAL=$((TOTAL + 1))
  rm -f "$OUTDIR/${name}.xml" "$OUTDIR/${name}.icc" "$dump_log"

  if ! make_profile "$name" "$lang_country"; then
    return
  fi
  if ! validate_profile "$name"; then
    return
  fi

  if ! grep -Fq "$expected_one" "$dump_log"; then
    echo "  [FAIL] $name -- missing expected warning: $expected_one"
    sed -n '1,80p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi
  if ! grep -Fq "$expected_two" "$dump_log"; then
    echo "  [FAIL] $name -- missing expected warning: $expected_two"
    sed -n '1,80p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name"
  PASS=$((PASS + 1))
}

echo "iccDEV mluc ISO code validation regressions"
echo "Tools: $TOOLS_DIR"
echo "Testing: $TESTING_DIR"
echo "Output: $OUTDIR"

if [ ! -x "$FROMXML" ] || [ ! -x "$DUMP" ]; then
  echo "  [FAIL] Missing required tools: $FROMXML or $DUMP"
  exit 1
fi
if [ ! -f "$SOURCE_XML" ]; then
  echo "  [FAIL] Missing source XML: $SOURCE_XML"
  exit 1
fi

run_clean_test "valid-en-us" "enUS"
run_clean_test "valid-uncommon-aa-et" "aaET"
run_warning_test "unknown-zz-zz" "zzZZ" \
  "Unknown ISO-639 language code 'zz' (0x7A7A)" \
  "Unknown ISO-3166 country code 'ZZ' (0x5A5A)"
run_warning_test "malformed-e1-u2" "e1U2" \
  "Malformed ISO-639 language code 'e1' (0x6531)" \
  "Malformed ISO-3166 country code 'U2' (0x5532)"

echo ""
echo "mluc ISO code validation: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
if [ "$TOTAL" -ne 4 ] || [ "$PASS" -ne 4 ]; then
  echo "  [FAIL] Expected 4 passing tests"
  exit 1
fi

exit 0
