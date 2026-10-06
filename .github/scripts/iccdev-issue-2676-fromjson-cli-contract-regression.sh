#!/bin/bash
###############################################################################
# iccFromJson CLI-contract regression (#2676)
#
# The JSON half of the family iccdev-issue-2387-fromxml-cli-contract-regression.sh
# covers for iccFromXml: a malformed invocation must not report success, an
# unrecognised option must not be skipped in silence, and -noid must leave no
# profile ID behind.  Each case was measured against iccFromXml's behaviour.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2676-fromjson-cli-contract}"
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
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"
TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
WORKDIR="$OUTDIR/work"
STDOUT_LOG="$OUTDIR/fromjson-cli.stdout.log"
STDERR_LOG="$OUTDIR/fromjson-cli.stderr.log"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
PASS=0
FAIL=0
SKIP=0
TOTAL=0
pass_case() { PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1)); echo "  [PASS] $1 -- $2"; }
fail_case() { FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1)); echo "  [FAIL] $1 -- $2"; }
skip_case() { SKIP=$((SKIP + 1)); TOTAL=$((TOTAL + 1)); echo "  [SKIP] $1 -- $2"; }
check_sanitizers() {
  local name="$1" log="$2"
  if grep -Eq "ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in tool output"
    return 1
  fi
  return 0
}
echo "=== iccFromJson CLI-contract regression (#2676) ==="
if [ ! -x "$FROMJSON" ]; then
  skip_case "fromjson-cli-contract" "iccFromJson not built at $FROMJSON"
  echo ""
  echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
  exit 77
fi
# The fixture is the JSON iccToJson writes for a tracked v4 profile, so its tag
# shapes are the writer's own and it carries a ProfileID for the -noid case.
if [ ! -x "$TOJSON" ]; then
  skip_case "fromjson-cli-contract" "iccToJson not built at $TOJSON, no fixture"
  echo ""
  echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
  exit 77
fi
if ! "$TOJSON" "$TESTING_DIR/sRGB_v4_ICC_preference.icc" "$WORKDIR/base.json" >/dev/null 2>&1 || [ ! -s "$WORKDIR/base.json" ]; then
  skip_case "fromjson-cli-contract" "iccToJson could not write the fixture from Testing/sRGB_v4_ICC_preference.icc"
  echo ""
  echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
  exit 77
fi
run_case() {
  local name="$1" want_exit="$2" want_stream="$3" desc="$4"
  shift 5   # drop the "--" too
  local exit_code=0
  ( cd "$WORKDIR" && timeout 60 "$FROMJSON" "$@" ) > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?
  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "iccFromJson died on a signal (exit $exit_code)"
    return
  fi
  if [ "$exit_code" -ne "$want_exit" ]; then
    fail_case "$name" "expected exit=$want_exit, got exit=$exit_code ($desc)"
    sed -n '1,6p' "$STDERR_LOG"
    return
  fi
  if [ "$want_stream" = "err" ]; then
    if [ ! -s "$STDERR_LOG" ]; then
      fail_case "$name" "expected a diagnostic on stderr, got none"
      return
    fi
    if [ -s "$STDOUT_LOG" ]; then
      fail_case "$name" "a failed invocation wrote to stdout"
      sed -n '1,6p' "$STDOUT_LOG"
      return
    fi
  elif [ "$want_stream" = "out" ]; then
    if [ ! -s "$STDOUT_LOG" ]; then
      fail_case "$name" "expected output on stdout, got none"
      return
    fi
  fi
  pass_case "$name" "$desc"
}
echo "tools: $TOOLS_DIR"
# The fixture itself must convert, or every refusal below could be a parse failure.
run_case "fixture-converts" 0 out "the fixture converts" -- base.json out.icc
run_case "no-operands"   1 err "a bare invocation is a malformed conversion, not a help request" --
run_case "one-operand"   1 err "an input with no output path converts nothing" -- base.json
run_case "help-short"    0 out "-h is an explicit help request and succeeds" -- -h
run_case "help-long"     0 out "--help is an explicit help request and succeeds" -- --help
run_case "typo-no-id"    1 err "the '-no-id' typo is refused instead of silently ignored" -- base.json out-typo.icc -no-id
if [ -f "$WORKDIR/out-typo.icc" ]; then
  fail_case "typo-no-id-no-output" "a refused invocation still wrote a profile"
else
  pass_case "typo-no-id-no-output" "the refused invocation wrote no profile"
fi
run_case "help-flag-after-operands" 1 err "a help flag mixed into a conversion is refused, not silently obeyed" -- base.json out-h.icc -h
profile_id_bytes() {
  python3 - "$1" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1]).read_bytes()
print(d[84:100].hex() if len(d) >= 100 else "short")
PY
}
if ! grep -q '"ProfileID"' "$WORKDIR/base.json" 2>/dev/null; then
  skip_case "noid-clears-profile-id" "the fixture carries no ProfileID, nothing to suppress"
else
  {
    ( cd "$WORKDIR" && "$FROMJSON" base.json noid.icc -noid >/dev/null 2>&1 )
    ( cd "$WORKDIR" && "$FROMJSON" base.json withid.icc >/dev/null 2>&1 )
    noid_id="$(profile_id_bytes "$WORKDIR/noid.icc")"
    withid_id="$(profile_id_bytes "$WORKDIR/withid.icc")"
    if [ "$withid_id" = "00000000000000000000000000000000" ]; then
      fail_case "noid-clears-profile-id" "the control without -noid also has a zero ID -- fixture proves nothing"
    elif [ "$noid_id" != "00000000000000000000000000000000" ]; then
      fail_case "noid-clears-profile-id" "-noid left profile ID bytes 84-99 as $noid_id"
    else
      pass_case "noid-clears-profile-id" "-noid zeroes the ID while the same document without it keeps $withid_id"
    fi
  }
fi
echo ""
echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
if [ "$PASS" -eq 0 ] && [ "$FAIL" -eq 0 ]; then
  exit 77
fi
[ "$FAIL" -eq 0 ]
