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
JPEGDUMP="$TOOLS_DIR/IccJpegDump/iccJpegDump"
PNGDUMP="$TOOLS_DIR/IccPngDump/iccPngDump"
TOXML="$TOOLS_DIR/IccToXml/iccToXml"
TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
APPLYPROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
APPLYNAMED="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
APPLYSEARCH="$TOOLS_DIR/IccApplySearch/iccApplySearch"
DISPLAY="$TESTING_DIR/Display/LCDDisplay.icc"
OBSERVER="$TESTING_DIR/ICS/XYZ_float-D65_2deg-Part1.icc"
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
TIFF="$TESTING_DIR/hybrid/Data/smCows380_5_780.tif"
PNG="$TESTING_DIR/hybrid/Data/MS-Sensors.png"
DATA="$TESTING_DIR/ApplyDataFiles/rgb-cmykhues.txt"

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
  if grep -qiE "successfully created|successfully written|saved successfully|extracted to:|saved to:" "$logfile"; then
    sed -n '1,80p' "$logfile"
    fail "$name printed a success message for a failed output path"
  fi
}

require_tool "$V5DSP"
require_tool "$TIFFDUMP"
require_tool "$APPLYLINK"
require_tool "$JPEGDUMP"
require_tool "$PNGDUMP"
require_tool "$TOXML"
require_tool "$TOJSON"
require_tool "$APPLYPROFILES"
require_tool "$APPLYNAMED"
require_tool "$APPLYSEARCH"
require_file "$SRGB"
require_file "$TIFF"
require_file "$PNG"
require_file "$DATA"

HAVE_V5_FIXTURES=1
for v5_fixture in "$DISPLAY" "$OBSERVER"; do
  if [ ! -f "$v5_fixture" ]; then
    echo "  [SKIP] issue-1150-output-failure -- missing V5 fixture: $v5_fixture"
    HAVE_V5_FIXTURES=0
  fi
done

if [ "$HAVE_V5_FIXTURES" -eq 1 ]; then
  run_expect_output_failure v5-missing-dir \
    "$V5DSP" "$DISPLAY" "$OBSERVER" "$OUTDIR/no-such-dir/out.icc"

  run_expect_output_failure v5-dev-full \
    "$V5DSP" "$DISPLAY" "$OBSERVER" /dev/full

  run_expect_output_failure v5-dev-null \
    "$V5DSP" "$DISPLAY" "$OBSERVER" /dev/null
fi

run_expect_output_failure tiffdump-dev-full \
  "$TIFFDUMP" "$TIFF" /dev/full

run_expect_output_failure tiffdump-dev-null \
  "$TIFFDUMP" "$TIFF" /dev/null

run_expect_output_failure jpegdump-dev-full \
  "$JPEGDUMP" "$SRGB" /dev/full

run_expect_output_failure jpegdump-dev-null \
  "$JPEGDUMP" "$SRGB" /dev/null

run_expect_output_failure pngdump-inject-dev-full \
  "$PNGDUMP" "$PNG" --write-icc "$SRGB" --output /dev/full

run_expect_output_failure pngdump-inject-dev-null \
  "$PNGDUMP" "$PNG" --write-icc "$SRGB" --output /dev/null

run_expect_output_failure toxml-dev-full \
  "$TOXML" "$SRGB" /dev/full

run_expect_output_failure toxml-dev-null \
  "$TOXML" "$SRGB" /dev/null

run_expect_output_failure tojson-dev-full \
  "$TOJSON" "$SRGB" /dev/full

run_expect_output_failure tojson-dev-null \
  "$TOJSON" "$SRGB" /dev/null

run_expect_output_failure applylink-devlink-dev-full \
  "$APPLYLINK" /dev/full 0 5 0 "issue-1150-devlink" 0.0 1.0 0 0 "$SRGB" 1

run_expect_output_failure applylink-devlink-dev-null \
  "$APPLYLINK" /dev/null 0 5 0 "issue-1150-devlink" 0.0 1.0 0 0 "$SRGB" 1

run_expect_output_failure applylink-cube-dev-full \
  "$APPLYLINK" /dev/full 1 5 4 "issue-1150-cube" 0.0 1.0 0 0 "$SRGB" 1

run_expect_output_failure applyprofiles-exportcfg-dev-full \
  "$APPLYPROFILES" -exportcfg /dev/full "$TIFF" "$OUTDIR/applyprofiles.tif" 0 0 0 0 0 "$SRGB" 0

run_expect_output_failure applyprofiles-exportcfg-dev-null \
  "$APPLYPROFILES" -exportcfg /dev/null "$TIFF" "$OUTDIR/applyprofiles.tif" 0 0 0 0 0 "$SRGB" 0

run_expect_output_failure applynamed-exportcfg-dev-full \
  "$APPLYNAMED" -exportcfg /dev/full "$DATA" 0 0 "$SRGB" 0

run_expect_output_failure applynamed-exportcfg-dev-null \
  "$APPLYNAMED" -exportcfg /dev/null "$DATA" 0 0 "$SRGB" 0

run_expect_output_failure applysearch-exportcfg-dev-full \
  "$APPLYSEARCH" -exportcfg /dev/full "$DATA" 0 0 "$SRGB" 0 "$SRGB" 0 -INIT 0 "$SRGB" 1

run_expect_output_failure applysearch-exportcfg-dev-null \
  "$APPLYSEARCH" -exportcfg /dev/null "$DATA" 0 0 "$SRGB" 0 "$SRGB" 0 -INIT 0 "$SRGB" 1

echo "  [OK] issue-1150-output-failure -- failed outputs return non-zero without success text"
