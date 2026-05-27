#!/bin/bash
###############################################################################
# iccFromJson issue-1079 malformed input soft-failure regression
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1079-fromjson-soft-failure}"
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
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccJSON${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"
README_INPUT="$OUTDIR/README.txt"
EMPTY_INPUT="$OUTDIR/id-empty"
LOG="$OUTDIR/fromjson-soft-failure.log"

fail() {
  echo "  [FAIL] issue-1079-fromjson-soft-failure -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,80p' "$logfile"
    return 1
  fi

  return 0
}

run_case() {
  local name="$1"
  local input="$2"
  local exit_code=0

  rm -f "$LOG"
  timeout 30 "$FROMJSON" "$input" /dev/null > "$LOG" 2>&1 || exit_code=$?
  check_sanitizers "$LOG" || fail "$name sanitizer finding"
  if [ "$exit_code" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$exit_code" -ge 128 ]; then
    sed -n '1,80p' "$LOG"
    fail "$name returned crash-like exit $exit_code"
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail "$name unexpectedly parsed malformed input"
  fi
  if ! grep -q "Unable to Parse" "$LOG"; then
    sed -n '1,80p' "$LOG"
    fail "$name did not report parse failure"
  fi
}

run_reproducer() {
  if [ ! -x "$FROMJSON" ]; then
    fail "missing executable: $FROMJSON"
  fi

  printf '%s\n' "Command line used to find this crash:" > "$README_INPUT"
  : > "$EMPTY_INPUT"

  run_case "afl-readme" "$README_INPUT"
  run_case "empty-crash-id" "$EMPTY_INPUT"

  echo "  [PASS] issue-1079-fromjson-soft-failure -- malformed JSON exits below signal range"
}

echo "=== iccFromJson issue-1079 soft-failure regression ==="
run_reproducer
exit 0
