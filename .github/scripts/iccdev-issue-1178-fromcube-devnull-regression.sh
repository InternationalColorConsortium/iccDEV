#!/bin/bash
###############################################################################
# iccFromCube issue-1178 /dev/null writer regression
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1178-fromcube-devnull}"
mkdir -p "$OUTDIR"

if [ ! -c /dev/null ]; then
  echo "  [SKIP] issue-1178-fromcube-devnull -- /dev/null is not available"
  exit 77
fi

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT=""
if [ -d "$TOOLS_DIR/.." ]; then
  BUILD_ROOT="$(cd "$TOOLS_DIR/.." && pwd -P)"
fi
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

FROMCUBE="$TOOLS_DIR/IccFromCube/iccFromCube"
POC="$OUTDIR/iridas_3d.cube"

fail() {
  echo "  [FAIL] issue-1178-fromcube-devnull -- $1"
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

write_poc() {
  {
    printf '%s\n' 'TITLE "A test 3D-LUT."'
    printf '%s\n' 'LUT_3D_SIZE 2'
    printf '%s\n' 'DOMAIN_MIN 0.0 1.0 0.0'
    printf '%s\n' 'DOMAIN_MAX 2.0 2.0 1.0'
    printf '\n'
    printf '%s\n' '0.0 0.0 0.0'
    printf '%s\n' '2.0 0.0 0.0'
    printf '%s\n' '0.0 2.0 0.0'
    printf '%s\n' '2.0 2.0 0.0'
    printf '%s\n' '0.0 0.0 2.0'
    printf '%s\n' '2.0 0.0 2.0'
    printf '%s\n' '0.0 2.0 2.0'
    printf '%s\n' '2.0 2.0 2.0'
  } > "$POC"
}

run_expect_success() {
  local logfile="$OUTDIR/regular-output.log"
  rm -f "$logfile" "$OUTDIR/iridas_3d.icc"

  timeout 60 "$FROMCUBE" "$POC" "$OUTDIR/iridas_3d.icc" > "$logfile" 2>&1 || {
    sed -n '1,80p' "$logfile"
    fail "regular file output failed"
  }
  check_sanitizers "$logfile" || fail "regular file output emitted sanitizer diagnostics"
}

run_expect_devnull_failure() {
  local logfile="$OUTDIR/devnull-output.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$FROMCUBE" "$POC" /dev/null > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "/dev/null output emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "/dev/null output timed out"
  fi
  if [ "$rc" -ge 128 ]; then
    fail "/dev/null output crashed with signal $((rc - 128))"
  fi
  if [ "$rc" -eq 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "/dev/null output unexpectedly succeeded"
  fi
  if grep -qi "successfully created" "$logfile"; then
    sed -n '1,80p' "$logfile"
    fail "/dev/null output printed success text"
  fi
}

if [ ! -x "$FROMCUBE" ]; then
  fail "missing executable: $FROMCUBE"
fi

write_poc
run_expect_success
run_expect_devnull_failure

echo "  [OK] issue-1178-fromcube-devnull -- no sanitizer diagnostics or false success"
