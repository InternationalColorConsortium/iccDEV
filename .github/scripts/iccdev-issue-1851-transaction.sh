#!/bin/bash
###############################################################################
# Maintainer transaction for issue #1851.
#
# Builds a vulnerable iccDEV ref and the patched checkout, replays the original
# iccFromXml PoC against both builds, and emits a Markdown transaction report
# suitable for GitHub Actions job summaries.
###############################################################################

set -euo pipefail

SOURCE_ROOT="${ICCDEV_SOURCE_ROOT:-$PWD}"
REPO_URL="${ICCDEV_REPO_URL:-https://github.com/InternationalColorConsortium/iccDEV.git}"
VULNERABLE_REF="${ICCDEV_VULNERABLE_REF:-809411cc4c8aa7675dec93067056477f4a2b25af}"
WORKDIR="${ICCDEV_TRANSACTION_WORKDIR:-${TMPDIR:-/tmp}/iccdev-issue-1851-transaction}"
REPORT="${ICCDEV_TRANSACTION_REPORT:-$WORKDIR/issue-1851-transaction.md}"
POC_REL=".github/ci/test-data/ub-parametriccurve-functiontype-overflow-1851.xml"
POC="$SOURCE_ROOT/$POC_REL"

if [ ! -r "$POC" ]; then
  echo "[FAIL] PoC fixture missing: $POC" >&2
  exit 2
fi

mkdir -p "$WORKDIR"
rm -rf "$WORKDIR/vulnerable" "$WORKDIR/build-vulnerable" "$WORKDIR/build-patched"
mkdir -p "$(dirname "$REPORT")"

log_section() {
  printf '\n========== %s ==========\n' "$1" >&2
}

append_report_block() {
  local title="$1"
  local file="$2"
  {
    echo ""
    echo "### $title"
    echo ""
    echo '```text'
    sed -n '1,120p' "$file"
    echo '```'
  } >> "$REPORT"
}

configure_and_build_fromxml() {
  local source_dir="$1"
  local build_dir="$2"
  local label="$3"

  log_section "Configure $label iccFromXml"
  cmake -S "$source_dir/Build/Cmake" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TOOLS=ON \
    -DCMAKE_C_COMPILER="${CC:-clang}" \
    -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined,integer -g3 -O0" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined,integer -g3 -O0" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined,integer" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined,integer" >&2

  log_section "Build $label iccFromXml"
  cmake --build "$build_dir" --target iccFromXml --parallel "${ICCDEV_BUILD_JOBS:-2}" >&2

  find "$build_dir/Tools" -maxdepth 3 -type f -name iccFromXml -print -quit
}

replay_fromxml() {
  local exe="$1"
  local output_icc="$2"
  local log_file="$3"

  rm -f "$output_icc" "$log_file"
  set +e
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0:allocator_may_return_null=1" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$exe" "$POC" "$output_icc" > "$log_file" 2>&1
  local rc=$?
  set -e
  printf '%s\n' "$rc"
}

{
  echo "# Issue #1851 iccFromXml sanitizer transaction"
  echo ""
  echo "- Issue: https://github.com/InternationalColorConsortium/iccDEV/issues/1851"
  echo "- PoC: \`$POC_REL\`"
  echo "- Original command-line shape: \`iccFromXml id.000005...txt foo\`"
  echo "- Vulnerable ref: \`$VULNERABLE_REF\`"
  echo "- Patched ref: \`$(git -C "$SOURCE_ROOT" rev-parse --short=12 HEAD 2>/dev/null || echo current-checkout)\`"
  echo ""
} > "$REPORT"

log_section "Inputs and original command-line reproduction"
echo "[INFO] Issue: https://github.com/InternationalColorConsortium/iccDEV/issues/1851"
echo "[INFO] PoC fixture: $POC"
echo "[INFO] Original command-line shape: iccFromXml id.000005...txt foo"
echo "[INFO] Vulnerable ref: $VULNERABLE_REF"
echo "[INFO] Patched checkout: $SOURCE_ROOT"

log_section "Clone vulnerable iccDEV ref"
git clone --no-tags --no-checkout "$REPO_URL" "$WORKDIR/vulnerable"
git -C "$WORKDIR/vulnerable" fetch --no-tags origin "$VULNERABLE_REF"
git -C "$WORKDIR/vulnerable" checkout --detach FETCH_HEAD
mkdir -p "$WORKDIR/vulnerable/.github/ci/test-data"
cp "$POC" "$WORKDIR/vulnerable/$POC_REL"

vuln_exe="$(configure_and_build_fromxml "$WORKDIR/vulnerable" "$WORKDIR/build-vulnerable" "vulnerable")"

log_section "Apply CFL fuzz patch stack to patched checkout"
"$SOURCE_ROOT/.github/scripts/iccdev-apply-fuzz-patches.sh" --mode cfl --strict >&2

patched_exe="$(configure_and_build_fromxml "$SOURCE_ROOT" "$WORKDIR/build-patched" "patched")"

if [ -z "$vuln_exe" ] || [ ! -x "$vuln_exe" ]; then
  echo "[FAIL] Vulnerable iccFromXml build did not produce an executable" >&2
  exit 2
fi
if [ -z "$patched_exe" ] || [ ! -x "$patched_exe" ]; then
  echo "[FAIL] Patched iccFromXml build did not produce an executable" >&2
  exit 2
fi

vuln_log="$WORKDIR/vulnerable-replay.log"
patched_log="$WORKDIR/patched-replay.log"
vuln_out="$WORKDIR/vulnerable-output.icc"
patched_out="$WORKDIR/patched-output.icc"

log_section "Replay vulnerable command"
echo "[INFO] Vulnerable replay command: $vuln_exe $POC $vuln_out"
vuln_rc="$(replay_fromxml "$vuln_exe" "$vuln_out" "$vuln_log")"
echo "[INFO] Vulnerable replay exit: $vuln_rc"
sed -n '1,80p' "$vuln_log"

log_section "Replay patched command"
echo "[INFO] Patched replay command: $patched_exe $POC $patched_out"
patched_rc="$(replay_fromxml "$patched_exe" "$patched_out" "$patched_log")"
echo "[INFO] Patched replay exit: $patched_rc"
sed -n '1,80p' "$patched_log"

append_report_block "Vulnerable replay log excerpt" "$vuln_log"
append_report_block "Patched replay log excerpt" "$patched_log"

log_section "Transaction conclusion"
vuln_has_ubsan=0
patched_has_ubsan=0
if grep -qE "runtime error: implicit conversion|UndefinedBehaviorSanitizer" "$vuln_log"; then
  vuln_has_ubsan=1
fi
if grep -qE "runtime error: implicit conversion|UndefinedBehaviorSanitizer" "$patched_log"; then
  patched_has_ubsan=1
fi

if [ "$vuln_has_ubsan" -ne 1 ]; then
  echo "[FAIL] Vulnerable replay did not reproduce the #1851 sanitizer breadcrumb" >&2
  exit 2
fi
echo "[OK] Vulnerable replay reproduces the #1851 sanitizer breadcrumb"

if [ "$patched_has_ubsan" -ne 0 ]; then
  echo "[FAIL] Patched replay still reports sanitizer findings" >&2
  exit 2
fi
if [ -f "$patched_out" ]; then
  echo "[FAIL] Patched replay accepted malformed XML and wrote $patched_out" >&2
  exit 2
fi
if ! grep -q "Unable to Parse" "$patched_log"; then
  echo "[FAIL] Patched replay did not fail closed with a parser error" >&2
  exit 2
fi

echo "[OK] Patched replay fails closed without sanitizer findings"

{
  echo ""
  echo "## Conclusion"
  echo ""
  echo "[OK] Vulnerable replay reproduces the #1851 sanitizer breadcrumb."
  echo "[OK] Patched replay fails closed without sanitizer findings."
} >> "$REPORT"

echo "[INFO] Transaction report: $REPORT"
