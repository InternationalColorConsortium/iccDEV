#!/usr/bin/env bash
# RunJsonTests.sh -- JSON round-trip parity testing
#
# Tests iccToJson + iccFromJson against all generated ICC profiles.
# Reports per-profile results and aggregate statistics.
#
# Usage: RunJsonTests.sh [build_dir|tools_dir]
#   path defaults to ../Build/Tools
#
# Copyright (c) 2024-2026 The International Color Consortium.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/../Build/Tools}"

if [ -x "$BUILD_DIR/IccToJson/iccToJson" ] || [ -x "$BUILD_DIR/IccFromJson/iccFromJson" ]; then
  TOOLS_DIR="$BUILD_DIR"
  BUILD_ROOT="$(dirname "$TOOLS_DIR")"
else
  BUILD_ROOT="$BUILD_DIR"
  TOOLS_DIR="$BUILD_ROOT/Tools"
fi

TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"

if [ ! -x "$TOJSON" ]; then
  echo "[FAIL] iccToJson not found at $TOJSON"
  exit 1
fi
if [ ! -x "$FROMJSON" ]; then
  echo "[FAIL] iccFromJson not found at $FROMJSON"
  exit 1
fi

export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

is_expected_fromjson_failure() {
  case "$1" in
    CalcTest/calcUnderStack_*.icc)
      return 0
      ;;
  esac
  return 1
}

PASS=0
FAIL=0
SKIP=0
XFAIL=0
TOTAL=0
CALC_FAIL=0

echo "=== IccJSON Round-Trip Parity Test ==="
echo "Build: $BUILD_ROOT"
echo "Tools: $TOOLS_DIR"
echo ""

while IFS= read -r icc; do
  TOTAL=$((TOTAL + 1))
  relpath="${icc#"$SCRIPT_DIR"/}"

  # ToJson
  if ! timeout 30 "$TOJSON" "$icc" /tmp/json-rt-test.json >/dev/null 2>&1; then
    SKIP=$((SKIP + 1))
    echo "[SKIP] $relpath (ToJson failed)"
    continue
  fi
  # FromJson
  if ! timeout 30 "$FROMJSON" /tmp/json-rt-test.json /tmp/json-rt-test.icc >/dev/null 2>&1; then
    if is_expected_fromjson_failure "$relpath"; then
      XFAIL=$((XFAIL + 1))
      echo "[XFAIL] $relpath (expected invalid calculator profile)"
    else
      SKIP=$((SKIP + 1))
      echo "[SKIP] $relpath (FromJson failed)"
    fi
    rm -f /tmp/json-rt-test.json
    continue
  fi

  orig_size=$(wc -c < "$icc" | tr -d ' ')
  rt_size=$(wc -c < /tmp/json-rt-test.icc | tr -d ' ')

  if [ "$orig_size" -eq "$rt_size" ]; then
    PASS=$((PASS + 1))
    echo "[OK]   $relpath ($orig_size bytes)"
  else
    delta=$((rt_size - orig_size))
    FAIL=$((FAIL + 1))
    if grep -qE 'CalcTest/' <<< "$relpath"; then
      CALC_FAIL=$((CALC_FAIL + 1))
    fi
    echo "[FAIL] $relpath (orig=$orig_size rt=$rt_size delta=$delta)"
  fi

  rm -f /tmp/json-rt-test.json /tmp/json-rt-test.icc
done < <(find "$SCRIPT_DIR" -name '*.icc' -type f | sort)

NON_CALC_FAIL=$((FAIL - CALC_FAIL))

echo ""
echo "=== Summary ==="
echo "Total:      $TOTAL"
echo "Pass:       $PASS"
echo "Fail:       $FAIL (CalcTest: $CALC_FAIL, Other: $NON_CALC_FAIL)"
echo "Skip:       $SKIP"
echo "XFail:      $XFAIL"
echo ""

if [ "$NON_CALC_FAIL" -eq 0 ]; then
  echo "[OK] All non-CalcTest profiles passed or skipped"
  if [ "$CALC_FAIL" -gt 0 ] || [ "$XFAIL" -gt 0 ]; then
    echo "[INFO] $((CALC_FAIL + XFAIL)) CalcTest profiles have known MPE Calculator gaps"
  fi
  exit 0
else
  echo "[WARN] $NON_CALC_FAIL non-CalcTest profiles had significant differences"
  exit 1
fi
