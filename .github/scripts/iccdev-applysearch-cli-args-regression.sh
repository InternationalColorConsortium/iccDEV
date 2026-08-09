#!/bin/bash
###############################################################################
# iccApplySearch command-line argument regression tests
###############################################################################
#
# Sibling of iccdev-applynamedcmm-cli-args-regression.sh. Both tools publish a
# "-cfg config_file_path" usage, and #1674 established for iccApplyNamedCmm that
# an argument the tool silently drops is worse than one it rejects: the run still
# reports success, so the caller cannot tell an honoured argument list from an
# ignored one. iccApplySearch shares that usage but never got the guard, so
# "cfg-extra" below is the case that fails against an unfixed tool. The remaining
# rejects already hold on master and are pinned here so the argument surface has
# a home of its own rather than living only inside the tool-coverage baseline.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for generated logs/configs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-applysearch-cli-args}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplySearch -type f 2>/dev/null | head -1)"
DATA="$TESTING_DIR/ApplyDataFiles/rgb8bit.txt"
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
CFG="$OUTDIR/applysearch-cfg.json"

fail() {
  echo "  [FAIL] iccApplySearch-cli-args -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,120p' "$logfile"
    return 1
  fi

  return 0
}

require_file() {
  local path="$1"

  if [ ! -f "$path" ]; then
    fail "missing fixture: $path"
  fi
}

require_tool() {
  local path="$1"

  if [ ! -x "$path" ]; then
    fail "missing executable: $path"
  fi
}

run_expect_success() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ne 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name failed with exit=$rc"
  fi
}

run_expect_reject() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  # A signal is never an acceptable refusal. Checking it separately from the
  # non-zero test below keeps a crash from reading as a rejection, which is the
  # failure mode a "did it refuse?" assertion is otherwise blind to.
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "$name crashed with signal $((rc - 128))"
  fi
  if [ "$rc" -eq 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name accepted malformed arguments"
  fi
}

require_tool "$APPLY"
require_file "$DATA"
require_file "$SRGB"

echo "=== iccApplySearch CLI argument regression ==="

# Let the tool write its own configuration rather than hand-authoring JSON here:
# the schema then cannot drift out from under this test, and the export doubles as
# a success case for the argument list that "cfg-extra" only extends by one token.
run_expect_success export-cfg \
  "$APPLY" -exportcfganddata "$CFG" "$DATA" 3 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
require_file "$CFG"

run_expect_success valid-cfg "$APPLY" -cfg "$CFG"
run_expect_success valid-legacy "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1

# The #1674 guard this script exists for: everything past argv[2] used to be read
# and dropped, leaving a zero exit behind.
run_expect_reject cfg-extra "$APPLY" -cfg "$CFG" ignored-extra

# Already rejected on master; pinned so the surface cannot regress quietly.
run_expect_reject init-missing-value "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT
run_expect_reject legacy-extra "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1 ignored-extra
run_expect_reject env-short-name "$APPLY" "$DATA" 0 0 -ENV:abc 1 "$SRGB" 1 "$SRGB" 1 -INIT 1

echo "  [PASS] iccApplySearch-cli-args"
exit 0
