#!/bin/bash
# test-json-tools.sh -- JSON config mode test suite for iccDEV tools
# Tests iccApplyNamedCmm -cfg, iccApplySearch -cfg, and malformed JSON handling
# Run from repo root: bash docs/Testing/test-json-tools.sh
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

BUILD="${BUILD:-Build/Cmake}"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-$BUILD/IccProfLib:$BUILD/IccXML}"
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0
UBSAN_SUPP="$REPO_ROOT/Testing/silence.txt"
if [ -f "$UBSAN_SUPP" ]; then
  export UBSAN_OPTIONS="print_stacktrace=1,halt_on_error=0,suppressions=$UBSAN_SUPP"
else
  export UBSAN_OPTIONS=print_stacktrace=1,halt_on_error=0
fi

NAMEDCMM="$BUILD/Tools/IccApplyNamedCmm/iccApplyNamedCmm"
SEARCH="$BUILD/Tools/IccApplySearch/iccApplySearch"

PASS=0; FAIL=0; ASAN=0; TOTAL=0

run_json_cfg() {
  local name="$1" tool="$2" cfg="$3" expect="$4"
  TOTAL=$((TOTAL+1))
  output=$("$tool" -cfg "$cfg" 2>&1)
  rc=$?
  asan_hit=0
  if grep -qE 'AddressSanitizer|runtime error:' <<< "$output"; then
    asan_hit=1
  fi
  if [ "$asan_hit" -gt 0 ]; then
    ASAN=$((ASAN+1))
    echo "[ASAN] #$TOTAL $name (exit=$rc)"
    grep -m2 'SUMMARY:' <<< "$output" || true
  elif [ "$expect" = "any" ] || [ "$rc" -eq "$expect" ]; then
    PASS=$((PASS+1))
    echo "[PASS] #$TOTAL $name (exit=$rc)"
  else
    FAIL=$((FAIL+1))
    echo "[FAIL] #$TOTAL $name (exit=$rc, expected=$expect)"
    tail -3 <<< "$output"
  fi
}

run_cli() {
  local name="$1"; shift
  local expect="$1"; shift
  TOTAL=$((TOTAL+1))
  output=$("$@" 2>&1)
  rc=$?
  asan_hit=0
  if grep -qE 'AddressSanitizer|runtime error:' <<< "$output"; then
    asan_hit=1
  fi
  if [ "$asan_hit" -gt 0 ]; then
    ASAN=$((ASAN+1))
    echo "[ASAN] #$TOTAL $name (exit=$rc)"
  elif [ "$expect" = "any" ] || [ "$rc" -eq "$expect" ]; then
    PASS=$((PASS+1))
    echo "[PASS] #$TOTAL $name (exit=$rc)"
  else
    FAIL=$((FAIL+1))
    echo "[FAIL] #$TOTAL $name (exit=$rc, expected=$expect)"
    tail -3 <<< "$output"
  fi
}

echo "=================================================================="
echo " JSON Tools Test Suite"
echo "=================================================================="

# --- JSON config mode tests ---
echo ""
echo "--- iccApplyNamedCmm -cfg JSON configs ---"
for cfg in docs/Testing/json-configs/applynamedcmm-*.json; do
  [ -f "$cfg" ] || continue
  name=$(basename "$cfg" .json)
  run_json_cfg "$name" "$NAMEDCMM" "$cfg" "any"
done

echo ""
echo "--- iccApplySearch -cfg JSON configs ---"
for cfg in docs/Testing/json-configs/applysearch-*.json; do
  [ -f "$cfg" ] || continue
  name=$(basename "$cfg" .json)
  run_json_cfg "$name" "$SEARCH" "$cfg" "any"
done

# --- CLI legacy mode tests ---
echo ""
echo "--- iccApplyNamedCmm CLI legacy mode ---"
run_cli "sRGB-float-tetrahedral" "any" "$NAMEDCMM" docs/Testing/test-data/rgb-float.txt 3 1 Testing/Display/sRGB_D65_MAT.icc 1
run_cli "sRGB-8bit" "any" "$NAMEDCMM" docs/Testing/test-data/rgb-8bit.txt 4 1 Testing/Display/sRGB_D65_MAT.icc 1
run_cli "sRGB-float-10digit" "any" "$NAMEDCMM" docs/Testing/test-data/rgb-float.txt 3:10 1 Testing/Display/sRGB_D65_MAT.icc 1
run_cli "sRGB-BPC-intent40" "any" "$NAMEDCMM" docs/Testing/test-data/rgb-float.txt 3 1 Testing/Display/sRGB_D65_MAT.icc 40

# --- Malformed JSON (negative tests) ---
echo ""
echo "--- Malformed JSON (negative tests) ---"
for cfg in docs/Testing/malformed-json/*.json; do
  [ -f "$cfg" ] || continue
  name="malformed-$(basename "$cfg" .json)"
  TOTAL=$((TOTAL+1))
  output=$("$NAMEDCMM" -cfg "$cfg" 2>&1)
  rc=$?
  asan_hit=0
  if grep -qE 'AddressSanitizer|runtime error:' <<< "$output"; then
    asan_hit=1
  fi
  if [ "$asan_hit" -gt 0 ]; then
    ASAN=$((ASAN+1))
    echo "[ASAN] #$TOTAL $name (exit=$rc)"
  elif [ "$rc" -ne 0 ]; then
    PASS=$((PASS+1))
    echo "[PASS] #$TOTAL $name (exit=$rc, expected non-zero)"
  else
    FAIL=$((FAIL+1))
    echo "[FAIL] #$TOTAL $name (exit=$rc, expected non-zero)"
  fi
done

# --- IccLibJSON native tool tests ---
echo ""
echo "--- IccLibJSON native tools ---"
TOJSON="$BUILD/Tools/IccToJson/iccToJson"
FROMJSON="$BUILD/Tools/IccFromJson/iccFromJson"
# RoundTrip test using iccToJson + iccFromJson
if [ -x "$TOJSON" ] && [ -x "$FROMJSON" ]; then
  run_cli "RoundTrip-sRGB" "any" "$TOJSON" Testing/Display/sRGB_D65_MAT.icc /tmp/test-json-rt.json
  if [ -f /tmp/test-json-rt.json ]; then
    run_cli "RoundTrip-FromJson-sRGB" "any" "$FROMJSON" /tmp/test-json-rt.json /tmp/test-json-rt.icc
    rm -f /tmp/test-json-rt.json /tmp/test-json-rt.icc
  fi
else
  echo "[SKIP] IccLibJSON tools not built"
fi

echo ""
echo "=================================================================="
echo " Results: $PASS pass, $FAIL fail, $ASAN ASAN findings ($TOTAL total)"
echo "=================================================================="

if [ "$ASAN" -gt 0 ]; then
  exit 2
elif [ "$FAIL" -gt 0 ]; then
  exit 1
else
  exit 0
fi
