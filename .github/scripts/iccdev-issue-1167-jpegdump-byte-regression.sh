#!/bin/bash
###############################################################################
# iccJpegDump issue-1167 signed-byte raw fallback regression
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1167-jpegdump-byte-regression}"
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

JPEGDUMP="$TOOLS_DIR/IccJpegDump/iccJpegDump"
POC="$OUTDIR/non-ascii-raw-fallback.jpg"
LOGFILE="$OUTDIR/issue-1167-jpegdump.log"

fail() {
  echo "  [FAIL] issue-1167-jpegdump-byte-regression -- $1"
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

generate_poc() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$POC" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
path.write_bytes(bytes([0x89, 0xeb, 0xff, 0x00, 0x97, 0x97, 0x20, 0x01]))
PY
}

run_reproducer() {
  local exit_code=0

  if [ ! -x "$JPEGDUMP" ]; then
    fail "missing executable: $JPEGDUMP"
  fi
  if ! generate_poc; then
    fail "failed to generate non-ASCII raw fallback input"
  fi

  rm -f "$LOGFILE"
  timeout 30 "$JPEGDUMP" "$POC" /dev/null > "$LOGFILE" 2>&1 || exit_code=$?
  if ! check_sanitizers "$LOGFILE"; then
    fail "sanitizer finding"
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail "iccJpegDump timed out"
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail "iccJpegDump crashed with signal $((exit_code - 128))"
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail "malformed non-ICC input unexpectedly extracted a profile"
  fi

  echo "  [PASS] issue-1167-jpegdump-byte-regression -- no sanitizer finding"
}

echo "=== iccJpegDump issue-1167 signed-byte raw fallback regression ==="
run_reproducer
exit 0
