#!/bin/bash
###############################################################################
# iccFromCube issue-1179 SetText and write-position regression
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1179-fromcube-regression}"
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

FROMCUBE="$TOOLS_DIR/IccFromCube/iccFromCube"
POC="$OUTDIR/issue-1179.cube"
OUTICC="$OUTDIR/issue-1179.icc"
REAL_LOG="$OUTDIR/regular-output.log"
NULL_LOG="$OUTDIR/dev-null-output.log"

fail() {
  echo "  [FAIL] issue-1179-fromcube -- $1"
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

if [ ! -x "$FROMCUBE" ]; then
  fail "missing executable: $FROMCUBE"
fi

{
  printf 'TITLE "T\357\273\277 Identity LUT"\n'
  printf 'LUT_3D_SIZE 2\n'
  printf 'DOMAIN_MIN 0.0 0.0 0.0\n'
  printf 'DOMAIN_MAX 1.061.0(1.0\n'
  printf '0.0 0.0 0.0\n'
  printf '1.0 0.0 0.0\n'
  printf '0.0 1.0 0.0\n'
  printf '1.0 1.0 0.0\n'
  printf '0.0 0.0 1.0\n'
  printf '1.0 0.0 1.0\n'
  printf '0.0 1.0 1.0\n'
  printf '1.0 1.0 1.0\n'
} > "$POC"

rm -f "$OUTICC" "$REAL_LOG" "$NULL_LOG"
timeout 30 "$FROMCUBE" "$POC" "$OUTICC" > "$REAL_LOG" 2>&1 || fail "regular output run failed"
check_sanitizers "$REAL_LOG" || fail "regular output emitted sanitizer diagnostics"

if [ ! -s "$OUTICC" ]; then
  fail "regular output profile was not created"
fi

set +e
timeout 30 "$FROMCUBE" "$POC" /dev/null > "$NULL_LOG" 2>&1
null_rc=$?
set -e
check_sanitizers "$NULL_LOG" || fail "/dev/null output emitted sanitizer diagnostics"

if [ "$null_rc" -eq 0 ]; then
  fail "/dev/null output unexpectedly reported success"
fi

echo "  [OK] issue-1179-fromcube -- regular output clean; /dev/null fails without sanitizer diagnostics"
