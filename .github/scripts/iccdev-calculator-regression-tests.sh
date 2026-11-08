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
# Read the issue-1103 fixture from the regression corpus rather than from
# Testing/CalcTest. The two paths hold the same blob, and the sha256 below is
# checked before use either way, but Testing/ is the generated-profile corpus
# that CreateAllProfiles.sh rewrites, whereas .github/ci/regression/ holds
# fixtures that exist only to pin a defect. Pointing at the latter keeps this
# test from depending on corpus contents it does not own.
ISSUE_1103_CALC_UNDERFLOW_PROFILE="$REPO_ROOT/.github/ci/regression/issue-1103-calculator-window-underflow.icc"
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
    if grep "runtime error:" "$logfile" | grep -Ev '(/IccProfLib/IccMD5.cpp:|/include/c\+\+/[^/]+/(bits/(basic_string\.h|basic_string\.tcc|stl_bvector\.h|stl_uninitialized\.h|vector\.tcc)|ext/string_conversions\.h):)' >/dev/null 2>&1; then
      fail_case "$name" "undefined behavior"
      return 1
    fi
    echo "  [WARN] $name -- ignored known MD5/libstdc++ UBSAN instrumentation noise"
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

  # srgbCalcTest.xml is a self-reporting harness, not just a profile that has to
  # parse. Its MainFunction stores 86 constant-valued assertion verdicts in temp
  # registers -- including the only fJab/tJab numeric checks in the tree, at
  # tput(13) and tput(14) -- and gates its own output on their total:
  #
  #     tget(0,46) tget(46,46) sum(92) 92 eq
  #     if { ...real sRGB transform... out(0,3) } else { 0 0 0 out(0,3) }
  #
  # The window summed is 92 registers, not 92 assertions: the other six are
  # seeded to a literal 1 by tput(0,3) and tsav(4,3), so they pad the target
  # without testing anything. Either way one false verdict drops the sum below
  # 92 and takes the else branch.
  #
  # So a failed assertion is reported as an all-zero conversion, NOT as a
  # non-zero exit status. Everything above this point checks parse success, exit
  # status and sanitizer output, all of which stay clean when the calculator
  # computes the wrong colour, so those verdicts were being discarded. Reading
  # the output back is what turns them into a CI signal.
  #
  # The two inputs in srgbCalcTest.txt are 255,255,255 and 128,128,128 -- white
  # and mid grey. Neither converts to an all-zero XYZ through this pipeline, so
  # the sentinel is unambiguous and cannot fire on a legitimate result.
  local data_rows=0
  local zero_rows=0
  while IFS= read -r line; do
    data_rows=$((data_rows + 1))
    # iccApplyNamedCmm separates the converted triplet from the echoed source
    # sample with a tab, so the captured field keeps a trailing tab; the final
    # [[:space:]]* is what lets the anchor match it.
    if printf '%s\n' "$line" | grep -qE '^[[:space:]]*0\.0+[[:space:]]+0\.0+[[:space:]]+0\.0+[[:space:]]*$'; then
      zero_rows=$((zero_rows + 1))
    fi
  done < <(sed -n 's/^\(.*\);.*$/\1/p' "$apply_log" | grep -E '[0-9]')

  if [ "$data_rows" -eq 0 ]; then
    fail_case "$name" "no converted data rows in apply output"
    sed -n '1,20p' "$apply_log"
    return
  fi
  if [ "$zero_rows" -ne 0 ]; then
    fail_case "$name" \
      "calculator self-test failed: $zero_rows/$data_rows rows are the all-zero sentinel (one of the 86 tput assertion verdicts did not hold)"
    sed -n '1,30p' "$apply_log"
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

# ICC.2:2023 Table 102 'noop', 'scnt' and 'vxor' (#2086). Before the fix
# iccFromXml rejected these fixtures with Invalid Operator "..." and wrote no
# profile, so the conversion alone is the red/green check for acceptance.
#
# Acceptance is not enough on its own: an operator can be added to the enum and
# still compute the wrong thing, so each fixture's function takes no input and
# every row must come back as one fixed triple. The operand choices that make
# these discriminating are documented in the fixtures themselves -- in short,
# vxor is exercised at width 3 across the 0,5 threshold so that vand, vor and a
# threshold-free (Xi != Yi) all produce a different triple, and the noop/scnt
# ladder must read 0, 1, 2.
run_table102_operator_fixture() {
  local name="$1"
  local fixture="$2"
  local expected="$3"
  local source_xml="$REPO_ROOT/.github/ci/regression/${fixture}.xml"
  local source_txt="$REPO_ROOT/.github/ci/regression/issue-2086-table102-operands.txt"
  local built_icc="$OUTDIR/${fixture}.icc"
  local fromxml_log="$OUTDIR/${fixture}-fromxml.log"
  local apply_log="$OUTDIR/${fixture}-apply.log"
  local exit_code=0
  local rows=0
  local row

  TOTAL=$((TOTAL + 1))
  rm -f "$built_icc" "$fromxml_log" "$apply_log"

  if [ ! -f "$source_xml" ] || [ ! -f "$source_txt" ]; then
    fail_case "$name" "missing Table 102 fixture"
    return
  fi

  timeout 30 "$FROMXML" "$source_xml" "$built_icc" > "$fromxml_log" 2>&1 || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$built_icc" ]; then
    fail_case "$name" "iccFromXml rejected the Table 102 operators (exit=$exit_code)"
    sed -n '1,10p' "$fromxml_log"
    return
  fi

  exit_code=0
  timeout 30 "$APPLYNCM" "$source_txt" 3 0 "$built_icc" 1 > "$apply_log" 2>&1 || exit_code=$?
  if ! check_log "$name/apply" "$apply_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "iccApplyNamedCmm failed with exit=$exit_code"
    return
  fi

  # Data rows carry the source values after a ';'. Compare only the result
  # triple, and count the rows so that "no output at all" cannot pass.
  while IFS= read -r row; do
    rows=$((rows + 1))
    if [ "$row" != "$expected" ]; then
      fail_case "$name" "row $rows was '$row', expected '$expected'"
      return
    fi
  done < <(awk -F';' '/;/ && $1 ~ /[0-9]/ {print $1}' "$apply_log" |
             awk '{ if (NF==3) printf "%s %s %s\n", $1, $2, $3 }')

  if [ "$rows" -ne 3 ]; then
    fail_case "$name" "expected 3 result rows, saw $rows"
    return
  fi

  pass_case "$name"
}

run_table102_operators() {
  run_table102_operator_fixture \
    "issue-2086-vxor-operator" \
    "issue-2086-vxor-operator" \
    "1.0000 0.0000 0.0000"
  run_table102_operator_fixture \
    "issue-2086-noop-scnt-operators" \
    "issue-2086-noop-scnt-operators" \
    "0.0000 1.0000 2.0000"
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

# An explicit calculator variable occupies [Position, Position + Size). The
# upper edge of the 65536-register address space is valid, but crossing it or
# overflowing int while computing it must fail before flattening the function.
run_calc_position_boundary_case() {
  local name="$1"
  local position="$2"
  local size="$3"
  local expected="$4"
  local fixture="$REPO_ROOT/.github/ci/regression/calculator-position-boundary.xml"
  local source_xml="$OUTDIR/${name}.xml"
  local built_icc="$OUTDIR/${name}.icc"
  local log="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$source_xml" "$built_icc" "$log"

  if [ ! -f "$fixture" ]; then
    fail_case "$name" "missing calculator position fixture"
    return
  fi

  sed "s/Position=\"65535\" Size=\"1\"/Position=\"$position\" Size=\"$size\"/" \
    "$fixture" > "$source_xml"
  if ! grep -Fq "Position=\"$position\" Size=\"$size\"" "$source_xml"; then
    fail_case "$name" "failed to set calculator variable boundary"
    return
  fi

  timeout 30 "$FROMXML" "$source_xml" "$built_icc" > "$log" 2>&1 || exit_code=$?
  if ! check_log "$name" "$log"; then
    return
  fi

  if [ "$expected" = reject ]; then
    if [ "$exit_code" -lt 1 ] || [ "$exit_code" -gt 123 ] || [ -e "$built_icc" ]; then
      fail_case "$name" "expected graceful rejection (exit=$exit_code)"
      sed -n '1,20p' "$log"
      return
    fi
    if ! grep -Fq "Temporary variable addressing out of bounds" "$log"; then
      fail_case "$name" "missing temporary-variable bounds diagnostic"
      sed -n '1,20p' "$log"
      return
    fi
  elif [ "$exit_code" -ne 0 ] || [ ! -s "$built_icc" ]; then
    fail_case "$name" "expected valid boundary to parse (exit=$exit_code)"
    sed -n '1,20p' "$log"
    return
  fi

  pass_case "$name"
}

run_calc_decl_width_case() {
  local name="$1"
  local replacement="$2"
  local diagnostic="$3"
  local fixture="$REPO_ROOT/.github/ci/regression/calculator-position-boundary.xml"
  local source_xml="$OUTDIR/${name}.xml"
  local built_icc="$OUTDIR/${name}.icc"
  local log="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$source_xml" "$built_icc" "$log"

  if [ ! -f "$fixture" ]; then
    fail_case "$name" "missing calculator position fixture"
    return
  fi

  sed "s|<Variables><Declare Name=\"x\" Position=\"65535\" Size=\"1\"/></Variables>|$replacement|" \
    "$fixture" > "$source_xml"
  if ! grep -Fq "$replacement" "$source_xml"; then
    fail_case "$name" "failed to set calculator declaration"
    return
  fi

  timeout 30 "$FROMXML" "$source_xml" "$built_icc" > "$log" 2>&1 || exit_code=$?
  if ! check_log "$name" "$log"; then
    return
  fi
  if [ "$exit_code" -lt 1 ] || [ "$exit_code" -gt 123 ] || [ -e "$built_icc" ]; then
    fail_case "$name" "expected graceful rejection (exit=$exit_code)"
    sed -n '1,20p' "$log"
    return
  fi
  if ! grep -Fq "$diagnostic" "$log"; then
    fail_case "$name" "missing declaration-width diagnostic"
    sed -n '1,20p' "$log"
    return
  fi

  pass_case "$name"
}

# Calculator imports recurse through separate XML documents. A shallow chain
# must load, while a deeper chain and a failed nested import must stop parsing.
run_calc_import_case() {
  local name="$1" depth="$2" expected="$3"
  local fixture="$REPO_ROOT/.github/ci/test-data/xml-spec-qa/mcs-3-channel-mid.xml"
  local case_dir="$OUTDIR/$name"
  local main_xml="$case_dir/main.xml"
  local built_icc="$case_dir/profile.icc"
  local log="$case_dir/fromxml.log"
  local current next i exit_code=0

  TOTAL=$((TOTAL + 1))
  if [ ! -f "$fixture" ]; then
    fail_case "$name" "missing MCS calculator XML fixture"
    return
  fi
  if ! mkdir -p "$case_dir"; then
    fail_case "$name" "cannot create import-chain directory"
    return
  fi
  rm -f "$built_icc" "$log"

  sed 's|<SubElements/>|<Imports><Import Filename="i0000.xml"/></Imports><SubElements/>|' \
    "$fixture" > "$main_xml"
  if ! grep -Fq 'Filename="i0000.xml"' "$main_xml"; then
    fail_case "$name" "failed to add calculator import"
    return
  fi

  for ((i=0; i<depth; i++)); do
    printf -v current 'i%04d.xml' "$i"
    if ((i + 1 < depth)); then
      printf -v next 'i%04d.xml' "$((i + 1))"
      printf '<IccCalcImport><Imports><Import Filename="%s"/></Imports></IccCalcImport>\n' \
        "$next" > "$case_dir/$current"
    else
      printf '<IccCalcImport/>\n' > "$case_dir/$current"
    fi
  done
  if [ "$expected" = missing ]; then
    printf '<IccCalcImport><Imports><Import Filename="missing-child.xml"/></Imports></IccCalcImport>\n' \
      > "$case_dir/i0000.xml"
  fi

  (cd "$case_dir" && timeout 30 "$FROMXML" main.xml "$built_icc" > "$log" 2>&1) || exit_code=$?
  if ! check_log "$name" "$log"; then
    return
  fi

  if [ "$expected" = accept ]; then
    if [ "$exit_code" -ne 0 ] || [ ! -s "$built_icc" ]; then
      fail_case "$name" "expected shallow import chain to parse (exit=$exit_code)"
      sed -n '1,20p' "$log"
      return
    fi
  else
    if [ "$exit_code" -lt 1 ] || [ "$exit_code" -gt 123 ] || [ -e "$built_icc" ]; then
      fail_case "$name" "expected graceful import rejection (exit=$exit_code)"
      sed -n '1,20p' "$log"
      return
    fi
    if [ "$expected" = depth ]; then
      if ! grep -Fq 'Calculator import nesting exceeds 32 levels' "$log"; then
        fail_case "$name" "missing import-depth diagnostic"
        sed -n '1,20p' "$log"
        return
      fi
    elif ! grep -Fq "Unable to import 'missing-child.xml" "$log"; then
      fail_case "$name" "missing nested-import diagnostic"
      sed -n '1,20p' "$log"
      return
    fi
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
  run_table102_operators
  run_calc_position_boundary_case "calc-position-intmax-overflow" 2147483647 2 reject
  run_calc_position_boundary_case "calc-position-past-end" 65535 2 reject
  run_calc_position_boundary_case "calc-position-last-register" 65535 1 accept
  run_calc_decl_width_case "calc-variable-member-overflow" \
    '<Variables><Declare Name="x">a[65535] b</Declare></Variables>' \
    'Temporary variable declaration exceeds 65535 channels'
  run_calc_decl_width_case "calc-macro-local-overflow" \
    '<Macros><Macro Name="m" Local="a[65535] b">{ in(0,3) out(0,3) }</Macro></Macros>' \
    'Temporary macro local declaration exceeds 65535 channels'
  run_calc_import_case "calc-import-shallow" 8 accept
  run_calc_import_case "calc-import-depth" 40 depth
  run_calc_import_case "calc-import-missing-child" 1 missing
fi

echo "Calculator regression tests: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

exit 0
