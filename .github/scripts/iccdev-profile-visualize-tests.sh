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

VISUALIZE="$TOOLS_DIR/IccProfileVisualize/iccProfileVisualize"
PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
JSON_PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.json"
WORK_PROFILE="$OUTDIR/sRGB_v4_ICC_preference.icc"
BAD_TAG_PROFILE="$OUTDIR/sRGB_v4_ICC_preference-bad-A2B0-offset.icc"
BAD_CLUT_PROFILE="$OUTDIR/sRGB_v4_ICC_preference-missing-A2B0-clut.icc"
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
  # shellcheck disable=SC2317
  local name="$1"
  # shellcheck disable=SC2317
  local path="$2"

  # shellcheck disable=SC2317
  require_file "$name" "$path" || return 1

  # shellcheck disable=SC2317
  if ! grep -Fq "<svg" "$path" 2>/dev/null; then
    # shellcheck disable=SC2317
    fail_case "$name" "missing svg root: $path"
    # shellcheck disable=SC2317
    return 1
  fi

  # shellcheck disable=SC2317
  return 0
}

require_pdf() {
  local name="$1"
  local path="$2"

  require_file "$name" "$path" || return 1

  if ! python3 -c 'import sys; data = open(sys.argv[1], "rb").read(4); assert data == b"%PDF"' "$path"; then
    fail_case "$name" "invalid PDF magic: $path"
    return 1
  fi

  return 0
}

require_text() {
  local name="$1"
  local path="$2"
  local expected="$3"

  if ! grep -Fq "$expected" "$path" 2>/dev/null; then
    fail_case "$name" "missing expected text: $expected"
    sed -n '1,40p' "$path"
    return 1
  fi

  return 0
}

reject_text() {
  local name="$1"
  local path="$2"
  local unexpected="$3"

  if grep -Fq "$unexpected" "$path" 2>/dev/null; then
    fail_case "$name" "unexpected text: $unexpected"
    sed -n '1,40p' "$path"
    return 1
  fi

  return 0
}

make_bad_tag_offset_profile() {
  local input="$1"
  local output="$2"

  python3 -c '
import struct
import sys

src, dst = sys.argv[1:3]
data = bytearray(open(src, "rb").read())
count = struct.unpack(">I", data[128:132])[0]
for i in range(count):
    entry = 132 + i * 12
    if data[entry:entry + 4] == b"A2B0":
        data[entry + 4:entry + 8] = struct.pack(">I", len(data) + 4096)
        open(dst, "wb").write(data)
        sys.exit(0)
raise SystemExit("A2B0 tag not found")
' "$input" "$output"
}

make_missing_clut_profile() {
  local input="$1"
  local output="$2"

  python3 -c '
import struct
import sys

src, dst = sys.argv[1:3]
data = bytearray(open(src, "rb").read())
count = struct.unpack(">I", data[128:132])[0]
for i in range(count):
    entry = 132 + i * 12
    if data[entry:entry + 4] == b"A2B0":
        offset = struct.unpack(">I", data[entry + 4:entry + 8])[0]
        if data[offset:offset + 4] != b"mAB ":
            raise SystemExit("A2B0 is not mAB")
        data[offset + 24:offset + 28] = b"\x00\x00\x00\x00"
        open(dst, "wb").write(data)
        sys.exit(0)
raise SystemExit("A2B0 tag not found")
' "$input" "$output"
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
        "$OUTDIR"/sRGB_v4_ICC_preference_luts.pdf \
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
  require_pdf "$name/pdf" "$OUTDIR/sRGB_v4_ICC_preference_luts.pdf" || return

  echo "  [INFO] generated artifacts:"
  for artifact in "$OUTDIR"/sRGB_v4_ICC_preference_A2B0.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_A2B1.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_B2A0.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_B2A1.tif \
                  "$OUTDIR"/sRGB_v4_ICC_preference_luts.pdf; do
    printf "    %s %s bytes\n" "$(basename "$artifact")" "$(wc -c < "$artifact")"
  done

  pass_case "$name" "generated A2B/B2A TIFFs and LUT PDF"
}

run_malformed_visualize() {
  local name="$1"
  local profile="$2"
  local expected="$3"
  local unexpected="${4:-}"
  local unexpected2="${5:-}"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile"

  if [ ! -x "$VISUALIZE" ]; then
    fail_case "$name" "missing executable: $VISUALIZE"
    return
  fi

  timeout 60 "$VISUALIZE" "$profile" > "$logfile" 2>&1 || exit_code=$?
  check_sanitizers "$name" "$logfile" || return

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "iccProfileVisualize timed out"
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "iccProfileVisualize crashed with signal $((exit_code - 128))"
    return
  fi

  if [ -n "$expected" ]; then
    require_text "$name" "$logfile" "$expected" || return
  fi

  if [ -n "$unexpected" ]; then
    reject_text "$name" "$logfile" "$unexpected" || return
  fi

  if [ -n "$unexpected2" ]; then
    reject_text "$name" "$logfile" "$unexpected2" || return
  fi

  pass_case "$name" "malformed LUT skipped without sanitizer findings"
}

echo "=== iccProfileVisualize regression ==="

run_visualize

if [ "$FAIL" -eq 0 ]; then
  make_bad_tag_offset_profile "$PROFILE" "$BAD_TAG_PROFILE" || fail_case "bad-tag-fixture" "failed to create bad tag offset profile"
  make_missing_clut_profile "$PROFILE" "$BAD_CLUT_PROFILE" || fail_case "missing-clut-fixture" "failed to create missing CLUT profile"
fi

if [ "$FAIL" -eq 0 ]; then
  run_malformed_visualize "bad-A2B0-offset" "$BAD_TAG_PROFILE" "Skipping A2B0: unable to load tag"
  run_malformed_visualize "missing-A2B0-clut" "$BAD_CLUT_PROFILE" "" "missing CLUT" "clut data could not be read"
fi

echo "iccProfileVisualize regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
