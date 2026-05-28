#!/bin/bash
###############################################################################
# issue-1150 output failure regression
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1150-output-failure}"
mkdir -p "$OUTDIR"

if [ ! -c /dev/full ]; then
  echo "  [SKIP] issue-1150-output-failure -- /dev/full is not available"
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

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

V5DSP="$TOOLS_DIR/IccV5DspObsToV4Dsp/iccV5DspObsToV4Dsp"
TIFFDUMP="$TOOLS_DIR/IccTiffDump/iccTiffDump"
APPLYLINK="$TOOLS_DIR/IccApplyToLink/iccApplyToLink"
DISPLAY="$TESTING_DIR/Display/LCDDisplay.icc"
OBSERVER="$TESTING_DIR/ICS/XYZ_float-D65_2deg-Part1.icc"
TIFF="$TESTING_DIR/hybrid/Data/smCows380_5_780.tif"

fail() {
  echo "  [FAIL] issue-1150-output-failure -- $1"
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

run_expect_output_failure() {
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
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "$name crashed with signal $((rc - 128))"
  fi
  if [ "$rc" -eq 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name reported success for a failed output path"
  fi
  if grep -qiE "successfully created|successfully written|extracted to:|saved to:" "$logfile"; then
    sed -n '1,80p' "$logfile"
    fail "$name printed a success message for a failed output path"
  fi
}

require_tool "$V5DSP"
require_tool "$TIFFDUMP"
require_tool "$APPLYLINK"
require_file "$DISPLAY"
require_file "$OBSERVER"
require_file "$TIFF"

run_expect_output_failure v5-missing-dir \
  "$V5DSP" "$DISPLAY" "$OBSERVER" "$OUTDIR/no-such-dir/out.icc"

run_expect_output_failure v5-dev-full \
  "$V5DSP" "$DISPLAY" "$OBSERVER" /dev/full

run_expect_output_failure tiffdump-dev-full \
  "$TIFFDUMP" "$TIFF" /dev/full

run_expect_output_failure applylink-devlink-dev-full \
  "$APPLYLINK" /dev/full 0 5 0 "issue-1150-devlink" 0.0 1.0 0 0 "$DISPLAY" 1

run_expect_output_failure applylink-cube-dev-full \
  "$APPLYLINK" /dev/full 1 5 4 "issue-1150-cube" 0.0 1.0 0 0 "$DISPLAY" 1

echo "  [OK] issue-1150-output-failure -- failed outputs return non-zero without success text"
