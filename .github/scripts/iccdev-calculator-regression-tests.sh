#!/bin/bash
###############################################################################
# iccDEV calculator sanitizer regression tests
###############################################################################
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-calculator-regressions}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
APPLYNCM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
ISSUE_1103_CALC_UNDERFLOW_PROFILE="$TESTING_DIR/CalcTest/uio-CIccCalculatorFunc-CheckUnderflowOverflow-IccMpeCalc_cpp-Line4238.icc"
ISSUE_1103_CALC_UNDERFLOW_SHA256="e482d1defb841192735881d80b4b7ca0540f5b859164153f0be2080ce6167800"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0:detect_leaks=0}"
export LSAN_OPTIONS="${LSAN_OPTIONS:-detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  local name="$1"
  local detail="$2"

  echo "  [FAIL] $name -- $detail"
  FAIL=$((FAIL + 1))
}

pass_case() {
  local name="$1"

  echo "  [PASS] $name"
  PASS=$((PASS + 1))
}

check_log() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    fail_case "$name" "AddressSanitizer finding"
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    if grep "runtime error:" "$logfile" | grep -v "/IccProfLib/IccMD5.cpp:" >/dev/null 2>&1; then
      fail_case "$name" "undefined behavior"
      return 1
    fi
    echo "  [WARN] $name -- ignored known MD5 unsigned-shift instrumentation noise"
  fi

  return 0
}

require_tool() {
  local tool="$1"
  local label="$2"

  if [ ! -x "$tool" ]; then
    fail_case "$label" "missing executable: $tool"
    return 1
  fi

  return 0
}

run_valid_calc_apply() {
  local name="valid-srgb-calculator-apply"
  local calc_dir="$TESTING_DIR/Calc"
  local source_xml="$calc_dir/srgbCalcTest.xml"
  local source_txt="$calc_dir/srgbCalcTest.txt"
  local target_icc="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
  local built_icc="$OUTDIR/srgbCalcTest.icc"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local apply_log="$OUTDIR/${name}-apply.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$built_icc" "$fromxml_log" "$apply_log"

  if [ ! -f "$source_xml" ] || [ ! -f "$source_txt" ] || [ ! -f "$target_icc" ]; then
    fail_case "$name" "missing srgbCalcTest input fixture"
    return
  fi

  (cd "$calc_dir" && timeout 30 "$FROMXML" "srgbCalcTest.xml" "$built_icc" > "$fromxml_log" 2>&1) || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$built_icc" ]; then
    fail_case "$name" "iccFromXml failed with exit=$exit_code"
    sed -n '1,20p' "$fromxml_log"
    return
  fi

  exit_code=0
  timeout 30 "$APPLYNCM" "$source_txt" 2 0 "$built_icc" 3 "$target_icc" 3 > "$apply_log" 2>&1 || exit_code=$?
  if ! check_log "$name/apply" "$apply_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "iccApplyNamedCmm failed with exit=$exit_code"
    sed -n '1,20p' "$apply_log"
    return
  fi

  pass_case "$name"
}

run_debug_calc_apply() {
  local name="debug-calculator-operator-exercise"
  local calc_dir="$TESTING_DIR/CalcTest"
  local source_xml="$calc_dir/calcExercizeOps.xml"
  local source_txt="$calc_dir/rgbExercise8bit.txt"
  local built_icc="$OUTDIR/calcExercizeOps.icc"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local apply_log="$OUTDIR/${name}-apply.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$built_icc" "$fromxml_log" "$apply_log"

  if [ ! -f "$source_xml" ] || [ ! -f "$source_txt" ]; then
    fail_case "$name" "missing CalcTest operator fixture"
    return
  fi

  (cd "$calc_dir" && timeout 30 "$FROMXML" "calcExercizeOps.xml" "$built_icc" > "$fromxml_log" 2>&1) || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$built_icc" ]; then
    fail_case "$name" "iccFromXml failed with exit=$exit_code"
    sed -n '1,20p' "$fromxml_log"
    return
  fi

  exit_code=0
  timeout 30 "$APPLYNCM" -debugcalc "$source_txt" 0 1 "$built_icc" 1 > "$apply_log" 2>&1 || exit_code=$?
  if ! check_log "$name/apply" "$apply_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "iccApplyNamedCmm -debugcalc failed with exit=$exit_code"
    sed -n '1,30p' "$apply_log"
    return
  fi

  pass_case "$name"
}

run_reject_profile() {
  local fixture="$1"
  local name="reject-${fixture%.icc}"
  local profile="$TESTING_DIR/CalcTest/$fixture"
  local log="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$log"

  if [ ! -f "$profile" ]; then
    fail_case "$name" "missing fixture: $profile"
    return
  fi

  timeout 30 "$DUMP" -v "$profile" > "$log" 2>&1 || exit_code=$?
  if ! check_log "$name" "$log"; then
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "malformed calculator profile was accepted"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "iccDumpProfile timed out"
    return
  fi
  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "iccDumpProfile crashed with signal $((exit_code - 128))"
    return
  fi

  pass_case "$name"
}

run_issue_1103_dump_profile_regression() {
  local name="issue-1103-calculator-window-underflow"
  local profile="$ISSUE_1103_CALC_UNDERFLOW_PROFILE"
  local log="$OUTDIR/${name}.log"
  local digest=""
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$log"

  if [ ! -s "$profile" ]; then
    fail_case "$name" "missing regression profile: $profile"
    return
  fi

  digest="$(sha256sum "$profile" | awk '{print $1}')"
  if [ "$digest" != "$ISSUE_1103_CALC_UNDERFLOW_SHA256" ]; then
    fail_case "$name" "unexpected regression profile SHA256: $digest"
    return
  fi

  exit_code=0
  timeout 30 "$DUMP" -v 100 "$profile" ALL > "$log" 2>&1 || exit_code=$?
  if ! check_log "$name" "$log"; then
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "iccDumpProfile timed out"
    return
  fi
  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "iccDumpProfile crashed with signal $((exit_code - 128))"
    return
  fi

  pass_case "$name"
}

require_tool "$FROMXML" "iccFromXml" || true
require_tool "$DUMP" "iccDumpProfile" || true
require_tool "$APPLYNCM" "iccApplyNamedCmm" || true

if [ "$FAIL" -eq 0 ]; then
  run_valid_calc_apply
  run_debug_calc_apply
  run_reject_profile "calcUnderStack_rond.icc"
  run_reject_profile "calcUnderStack_trnc.icc"
  run_reject_profile "calcUnderStack_mod.icc"
  run_reject_profile "calcUnderStack_sel.icc"
  run_reject_profile "calcUnderStack_if.icc"
  run_reject_profile "calcOverMem_tget.icc"
  run_reject_profile "calcOverMem_tput.icc"
  run_reject_profile "calcOverMem_tsav.icc"
  run_issue_1103_dump_profile_regression
fi

echo "Calculator regression tests: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

exit 0
