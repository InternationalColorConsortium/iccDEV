#!/bin/bash
###############################################################################
# iccSpecSepToTiff malformed TIFF geometry regression
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-specsep-tiff-geometry-regression}"
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

SPECSEP="$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff"
MALFORMED_DIR="$OUTDIR/specsep-malformed"
MALFORMED_FILE="$MALFORMED_DIR/spec_3"
PACKED_DIR="$OUTDIR/specsep-packed"
PACKED_FILE="$PACKED_DIR/spec_1"
VALID_DIR="$OUTDIR/specsep-valid"
LOGFILE="$OUTDIR/specsep-tiff-geometry.log"
OUTPUT_TIFF="$OUTDIR/specsep-geometry-output.tif"

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

generate_malformed_tiff() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$MALFORMED_FILE" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
path.parent.mkdir(parents=True, exist_ok=True)


def ifd_entry(tag, tag_type, count, value):
    entry = struct.pack("<HHI", tag, tag_type, count)
    if tag_type == 3 and count == 1:
        return entry + struct.pack("<H", value) + b"\0\0"
    return entry + struct.pack("<I", value)


entry_count = 12
xres_offset = 8 + 2 + 12 * entry_count + 4
yres_offset = xres_offset + 8
entries = [
    (256, 4, 1, 0xDFFF0004),
    (257, 4, 1, 4),
    (258, 3, 1, 37136),
    (259, 3, 1, 1),
    (262, 3, 1, 1),
    (273, 4, 1, 256),
    (277, 3, 1, 1),
    (278, 4, 1, 3),
    (279, 4, 1, 32),
    (282, 5, 1, xres_offset),
    (283, 5, 1, yres_offset),
    (296, 3, 1, 2),
]

data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", 8))
data.extend(struct.pack("<H", len(entries)))
for entry in entries:
    data.extend(ifd_entry(*entry))
data.extend(struct.pack("<I", 0))
data.extend(struct.pack("<II", 72, 1))
data.extend(struct.pack("<II", 72, 1))
path.write_bytes(data)
PY
}

generate_single_channel_tiff() {
  local path="$1"
  local width="$2"
  local height="$3"
  local bps="$4"
  local fill="${5:-170}"

  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$path" "$width" "$height" "$bps" "$fill" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
width = int(sys.argv[2])
height = int(sys.argv[3])
bps = int(sys.argv[4])
fill = int(sys.argv[5]) & 0xff
path.parent.mkdir(parents=True, exist_ok=True)

bits = width * height * bps
data_len = max(1, (bits + 7) // 8)
pixel_data = bytes([fill]) * data_len
data_offset = 8
ifd_offset = data_offset + len(pixel_data)
if ifd_offset % 2:
    pixel_data += b"\0"
    ifd_offset += 1

entries = []


def add_entry(tag, tag_type, count, value):
    entries.append((tag, tag_type, count, value))


add_entry(256, 4, 1, width)
add_entry(257, 4, 1, height)
add_entry(258, 3, 1, bps)
add_entry(259, 3, 1, 1)
add_entry(262, 3, 1, 1)
add_entry(273, 4, 1, data_offset)
add_entry(277, 3, 1, 1)
add_entry(278, 4, 1, 1)
add_entry(279, 4, 1, len(pixel_data))
add_entry(284, 3, 1, 1)
add_entry(296, 3, 1, 2)
entries.sort()

data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset))
data.extend(pixel_data)
data.extend(struct.pack("<H", len(entries)))
for tag, tag_type, count, value in entries:
    data.extend(struct.pack("<HHI", tag, tag_type, count))
    if tag_type == 3 and count == 1:
        data.extend(struct.pack("<H", value))
        data.extend(b"\0\0")
    else:
        data.extend(struct.pack("<I", value))
data.extend(struct.pack("<I", 0))
path.write_bytes(data)
PY
}

generate_valid_descending_inputs() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  generate_single_channel_tiff "$VALID_DIR/spec_1" 2 1 8 17 &&
    generate_single_channel_tiff "$VALID_DIR/spec_2" 2 1 8 34 &&
    generate_single_channel_tiff "$VALID_DIR/spec_3" 2 1 8 51
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

run_specsep_geometry_reject() {
  local name="specsep-tiff-geometry-reject"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  if ! generate_malformed_tiff; then
    fail_case "$name" "python3 unavailable or malformed TIFF generation failed"
    return
  fi

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$MALFORMED_DIR/spec_" 3 3 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "Cannot open input $MALFORMED_FILE" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing expected rejection text"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  pass_case "$name" "malformed TIFF geometry rejected without sanitizer findings"
}

run_specsep_packed_bps_reject() {
  local name="specsep-packed-bps-reject"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  if ! generate_single_channel_tiff "$PACKED_FILE" 9 1 1; then
    fail_case "$name" "packed TIFF generation failed"
    return
  fi

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$PACKED_DIR/spec_" 1 1 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "Input bits per sample must be byte aligned: 1" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing packed BPS rejection text"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ -e "$OUTPUT_TIFF" ]; then
    fail_case "$name" "output TIFF created before packed BPS rejection"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  pass_case "$name" "packed BitsPerSample rejected before output creation"
}

run_specsep_descending_range_accept() {
  local name="specsep-descending-range-accept"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  if ! generate_valid_descending_inputs; then
    fail_case "$name" "valid TIFF generation failed"
    return
  fi

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$VALID_DIR/spec_" 3 1 -1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 0 ] || ! grep -Fq "Image successfully written!" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "expected descending range success, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  pass_case "$name" "descending range accepted without sanitizer findings"
}

echo "=== iccSpecSepToTiff malformed TIFF geometry regression ==="
run_specsep_geometry_reject
run_specsep_packed_bps_reject
run_specsep_descending_range_accept
echo "iccSpecSepToTiff malformed TIFF geometry regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
