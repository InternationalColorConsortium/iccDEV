#!/bin/bash
###############################################################################
# iccDEV iccProfileVisualize regression tests
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-profile-visualize}"
mkdir -p "$OUTDIR"

VISUALIZE="$TOOLS_DIR/iccProfileVisualize/iccProfileVisualize"
PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
JSON_PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.json"
WORK_PROFILE="$OUTDIR/sRGB_v4_ICC_preference.icc"
LOGFILE="$OUTDIR/iccProfileVisualize.log"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

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

require_file() {
  local name="$1"
  local path="$2"

  if [ ! -s "$path" ]; then
    fail_case "$name" "missing or empty file: $path"
    return 1
  fi

  return 0
}

require_tiff() {
  local name="$1"
  local path="$2"

  require_file "$name" "$path" || return 1

  if ! python3 -c 'import sys; data = open(sys.argv[1], "rb").read(4); assert data in (b"II*\x00", b"MM\x00*")' "$path"; then
    fail_case "$name" "invalid TIFF magic: $path"
    return 1
  fi

  return 0
}

require_svg() {
  local name="$1"
  local path="$2"

  require_file "$name" "$path" || return 1

  if ! grep -Fq "<svg" "$path" 2>/dev/null; then
    fail_case "$name" "missing svg root: $path"
    return 1
  fi

  return 0
}

run_visualize() {
  local name="srgb-v4-icc-preference-lut-exports"
  local exit_code=0
  local artifact

  TOTAL=$((TOTAL + 1))
  rm -f "$OUTDIR"/sRGB_v4_ICC_preference_A2B0.tif \
        "$OUTDIR"/sRGB_v4_ICC_preference_A2B1.tif \
        "$OUTDIR"/sRGB_v4_ICC_preference_B2A0.tif \
        "$OUTDIR"/sRGB_v4_ICC_preference_B2A1.tif \
        "$OUTDIR"/sRGB_v4_ICC_preference_luts.svg \
        "$WORK_PROFILE" "$LOGFILE"

  if [ ! -x "$VISUALIZE" ]; then
    fail_case "$name" "missing executable: $VISUALIZE"
    return
  fi

  if [ ! -f "$PROFILE" ]; then
    fail_case "$name" "missing profile fixture: $PROFILE"
    return
  fi

  if [ ! -f "$JSON_PROFILE" ]; then
    fail_case "$name" "missing JSON fixture: $JSON_PROFILE"
    return
  fi

  cp "$PROFILE" "$WORK_PROFILE"

  timeout 60 "$VISUALIZE" "$WORK_PROFILE" > "$LOGFILE" 2>&1 || exit_code=$?
  check_sanitizers "$name" "$LOGFILE" || return

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "iccProfileVisualize exited $exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  require_tiff "$name/A2B0" "$OUTDIR/sRGB_v4_ICC_preference_A2B0.tif" || return
  require_tiff "$name/A2B1" "$OUTDIR/sRGB_v4_ICC_preference_A2B1.tif" || return
  require_tiff "$name/B2A0" "$OUTDIR/sRGB_v4_ICC_preference_B2A0.tif" || return
  require_tiff "$name/B2A1" "$OUTDIR/sRGB_v4_ICC_preference_B2A1.tif" || return
  require_svg "$name/svg" "$OUTDIR/sRGB_v4_ICC_preference_luts.svg" || return

  echo "  [INFO] generated artifacts:"
  for artifact in "$OUTDIR"/sRGB_v4_ICC_preference_A2B0.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_A2B1.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_B2A0.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_B2A1.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_luts.svg; do
    printf "    %s %s bytes\n" "$(basename "$artifact")" "$(wc -c < "$artifact")"
  done

  pass_case "$name" "generated A2B/B2A TIFFs and LUT SVG"
}

echo "=== iccProfileVisualize regression ==="

run_visualize

echo "iccProfileVisualize regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
