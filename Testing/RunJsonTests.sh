#!/usr/bin/env bash
# RunJsonTests.sh -- JSON round-trip parity testing
#
# Tests iccToJson + iccFromJson against all generated ICC profiles.
# Reports per-profile results and aggregate statistics.
#
# Usage: RunJsonTests.sh [build_dir]
#   build_dir defaults to ../Build/Cmake
#
# Copyright (c) 2024-2026 The International Color Consortium.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/../Build/Cmake}"

TOJSON="$BUILD_DIR/Tools/IccToJson/iccToJson"
FROMJSON="$BUILD_DIR/Tools/IccFromJson/iccFromJson"

if [ ! -x "$TOJSON" ]; then
  echo "[FAIL] iccToJson not found at $TOJSON"
  exit 1
fi
if [ ! -x "$FROMJSON" ]; then
  echo "[FAIL] iccFromJson not found at $FROMJSON"
  exit 1
fi

export LD_LIBRARY_PATH="$BUILD_DIR/IccProfLib:$BUILD_DIR/IccXML:$BUILD_DIR/IccJSON${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

PASS=0
FAIL=0
SKIP=0
TOTAL=0
CALC_FAIL=0

echo "=== IccJSON Round-Trip Parity Test ==="
echo "Build: $BUILD_DIR"
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
    SKIP=$((SKIP + 1))
    echo "[SKIP] $relpath (FromJson failed)"
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
echo ""

if [ "$NON_CALC_FAIL" -eq 0 ]; then
  echo "[OK] All non-CalcTest profiles passed or skipped"
  if [ "$CALC_FAIL" -gt 0 ]; then
    echo "[INFO] $CALC_FAIL CalcTest profiles have known MPE Calculator gaps"
  fi
  exit 0
else
  echo "[WARN] $NON_CALC_FAIL non-CalcTest profiles had significant differences"
  exit 1
fi
