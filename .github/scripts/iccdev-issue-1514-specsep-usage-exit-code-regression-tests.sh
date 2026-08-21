#!/bin/bash
###############################################################################
# iccSpecSepToTiff usage-path exit-code regression (#1514)
###############################################################################
#
# #1514: invoking iccSpecSepToTiff with too few arguments printed Usage() and
# returned 0 -- a success status for what is actually a malformed invocation,
# so wrapper scripts / CI could not distinguish a no-op from a real conversion.
# The fix makes the argc<minargs path return -1 (process status 255), matching
# every other error exit in main().  The tool now also has real -h/--help and
# --version flags, so the "Usage() only ever fires on an error" assumption the
# original test was written under no longer holds: which stream Usage() lands on
# is what separates a help request from a malformed invocation.
#
# This test asserts:
#   1. no-args and too-few-args -> exit 255, Usage on stderr, nothing on stdout
#   2. -h/--help                -> exit 0, Usage on stdout, nothing on stderr
#   3. --version                -> exit 0, version on stdout, nothing on stderr
#   4. missing input prefix     -> exit 255, diagnostic on stderr, no output file
#
# Before the #1514 fix, cases 1 and 2 exit 0 and this script FAILs (red).  Before
# the stream split, every case above writes its diagnostic to stdout and the
# empty-stream assertions FAIL (red).
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-specsep-usage-exit-code-regression}"
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

SPECSEP="$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff"
STDOUT_LOG="$OUTDIR/specsep-usage.stdout.log"
STDERR_LOG="$OUTDIR/specsep-usage.stderr.log"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  echo "  [FAIL] $1 -- $2"
  FAIL=$((FAIL + 1))
}

pass_case() {
  echo "  [PASS] $1 -- $2"
  PASS=$((PASS + 1))
}

check_sanitizers() {
  local name="$1"
  local log="$2"
  if grep -Eq "ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in tool output"
    return 1
  fi
  return 0
}

# Malformed invocations fail with Usage on stderr and no stdout payload.
run_usage_error() {
  local name="$1"; shift
  TOTAL=$((TOTAL + 1))
  local exit_code=0
  timeout 60 "$SPECSEP" "$@" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || { sed -n '1,20p' "$STDOUT_LOG"; return; }
  check_sanitizers "$name" "$STDERR_LOG" || { sed -n '1,20p' "$STDERR_LOG"; return; }

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected exit=255 for malformed invocation, got exit=$exit_code"
    sed -n '1,20p' "$STDERR_LOG"
    return
  fi

  if ! grep -Fq "Usage:" "$STDERR_LOG" 2>/dev/null; then
    fail_case "$name" "missing Usage text on stderr"
    sed -n '1,20p' "$STDERR_LOG"
    return
  fi

  if [ -s "$STDOUT_LOG" ]; then
    fail_case "$name" "malformed invocation wrote to stdout"
    sed -n '1,20p' "$STDOUT_LOG"
    return
  fi

  pass_case "$name" "malformed invocation rejected with exit=255 and Usage on stderr"
}

run_meta_success() {
  local name="$1"
  local arg="$2"
  local expected="$3"
  local forbidden="${4:-}"
  TOTAL=$((TOTAL + 1))
  local exit_code=0

  timeout 60 "$SPECSEP" "$arg" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -ne 0 ] || ! grep -Eq "$expected" "$STDOUT_LOG" 2>/dev/null; then
    fail_case "$name" "expected successful stdout matching: $expected"
    sed -n '1,30p' "$STDOUT_LOG"
    sed -n '1,30p' "$STDERR_LOG"
    return
  fi

  # --version must not simply reprint the usage block: the synopsis lines carry
  # the tool name too, so a name-only match cannot discriminate the two paths.
  if [ -n "$forbidden" ] && grep -Eq "$forbidden" "$STDOUT_LOG" 2>/dev/null; then
    fail_case "$name" "stdout unexpectedly matched: $forbidden"
    sed -n '1,30p' "$STDOUT_LOG"
    return
  fi

  if [ -s "$STDERR_LOG" ]; then
    fail_case "$name" "successful metadata request wrote to stderr"
    sed -n '1,30p' "$STDERR_LOG"
    return
  fi

  pass_case "$name" "returned requested metadata on stdout with exit=0"
}

# A well-formed argc with a missing input remains a normal CLI failure.
run_missing_input_reject() {
  local name="iccSpecSepToTiff missing-input-prefix still exits non-zero"
  TOTAL=$((TOTAL + 1))
  local exit_code=0
  local out="$OUTDIR/should-not-exist.tif"
  rm -f "$out"
  # 8 args (no profile): output compress sep prefix start end incr
  timeout 60 "$SPECSEP" "$out" 0 0 "$OUTDIR/no-such-prefix_" 0 0 1 > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected exit=255 for missing input, got exit=$exit_code"
    sed -n '1,20p' "$STDERR_LOG"
    return
  fi

  if ! grep -Fq "Cannot open input" "$STDERR_LOG" 2>/dev/null; then
    fail_case "$name" "missing 'Cannot open input' diagnostic on stderr"
    sed -n '1,20p' "$STDERR_LOG"
    return
  fi

  if [ -s "$STDOUT_LOG" ]; then
    fail_case "$name" "missing-input failure wrote to stdout"
    sed -n '1,20p' "$STDOUT_LOG"
    return
  fi

  if [ -e "$out" ]; then
    fail_case "$name" "output TIFF created despite missing input"
    return
  fi

  pass_case "$name" "missing input rejected with exit=255 and stderr diagnostic"
}

if [ ! -x "$SPECSEP" ]; then
  echo "iccSpecSepToTiff not found at $SPECSEP -- skipping (build Tools first)"
  exit 0
fi

echo "=== iccSpecSepToTiff usage-path exit-code regression (#1514) ==="
run_usage_error "iccSpecSepToTiff no-args exits non-zero"
run_usage_error "iccSpecSepToTiff too-few-args exits non-zero" foo.bar 0 0
run_meta_success "iccSpecSepToTiff short help" -h "^Usage:"
run_meta_success "iccSpecSepToTiff long help" --help "^Usage:"
run_meta_success "iccSpecSepToTiff version" --version "^iccSpecSepToTiff [0-9]" "^Usage:"
run_missing_input_reject
echo "iccSpecSepToTiff usage-path exit-code regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
