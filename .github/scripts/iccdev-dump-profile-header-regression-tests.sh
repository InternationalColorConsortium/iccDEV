#!/bin/bash
###############################################################################
# iccDumpProfile malformed header-size regression tests
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-dump-profile-header-regressions}"
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

DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
BASE_PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
MUTATED="$OUTDIR/header-size-ffffffff.icc"
LOGFILE="$OUTDIR/header-size-ffffffff.log"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

pass_case() {
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

check_sanitizers() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    fail_case "$name" "AddressSanitizer finding"
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    fail_case "$name" "undefined behavior"
    return 1
  fi

  return 0
}

generate_mutated_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$BASE_PROFILE" "$MUTATED" <<'PY'
import pathlib
import sys

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
data = bytearray(src.read_bytes())
if len(data) < 128:
    raise SystemExit("profile header is too short")

data[0:4] = b"\xff\xff\xff\xff"
dst.write_bytes(data)
PY
}

run_header_size_reject() {
  local name="dump-profile-header-size-ffffffff"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$MUTATED" "$LOGFILE"

  if [ ! -x "$DUMP" ]; then
    fail_case "$name" "missing executable: $DUMP"
    return
  fi

  if [ ! -f "$BASE_PROFILE" ]; then
    fail_case "$name" "missing base profile: $BASE_PROFILE"
    return
  fi

  if ! generate_mutated_profile; then
    fail_case "$name" "failed to generate mutated profile"
    return
  fi

  timeout 60 "$DUMP" -v 100 "$MUTATED" > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi

  if [ "$exit_code" -eq 134 ] || [ "$exit_code" -eq 136 ] || [ "$exit_code" -eq 139 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  pass_case "$name" "malformed header size handled without sanitizer findings"
}

echo "=== iccDumpProfile malformed header-size regression ==="
run_header_size_reject
echo "iccDumpProfile malformed header-size regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
