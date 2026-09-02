#!/bin/bash
###############################################################################
# TIFFTAG_RESOLUTIONUNIT propagation regression (#2220)
###############################################################################
#
# CTiffImg::Create() never wrote TIFFTAG_RESOLUTIONUNIT, so every tool that
# copies a source image's XResolution/YResolution emitted them without the unit
# that gives them meaning.  Readers then fall back to the TIFF 6.0 default of
# inches: a 254-pixel row at 100 pixels/cm is 1.00 inch wide, and the converted
# output read as 2.54 inches.
#
# The defect lives in shared code, so it is measured per CALLER: iccSpecSepToTiff
# and iccApplyProfiles both reach it through the same Create(), and iccTiffDump
# reads the same member back.
#
# Assertions read the ResolutionUnit tag out of the output file's IFD directly
# rather than through a tool, so a tool reporting the unit wrongly cannot make a
# case pass for the wrong reason.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tiff-resolution-unit-regression}"
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
APPLYPROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
TIFFDUMP="$TOOLS_DIR/IccTiffDump/iccTiffDump"
SRGB_PROFILE="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"

WORKDIR="$OUTDIR/tiff-resolution-unit"
LOGFILE="$OUTDIR/tiff-resolution-unit.log"
OUTPUT_TIFF="$WORKDIR/output.tif"

# TIFF 6.0 ResolutionUnit codes.
RESUNIT_NONE=1
RESUNIT_INCH=2
RESUNIT_CENTIMETER=3

PASS=0
FAIL=0
SKIP=0
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

skip_case() {
  local name="$1"
  local reason="$2"
  echo "  [SKIP] $name -- $reason"
  SKIP=$((SKIP + 1))
}

# Emit a minimal single-strip TIFF carrying an explicit ResolutionUnit plus
# XResolution/YResolution.  Written by hand rather than with a library so the
# unit under test is exactly the byte the test intends, with no encoder
# normalizing it on the way in.
#   generate_tiff <path> <width> <height> <samples> <fill> <resunit> <res>
generate_tiff() {
  python3 - "$@" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
width = int(sys.argv[2])
height = int(sys.argv[3])
samples = int(sys.argv[4])
fill = int(sys.argv[5]) & 0xff
resunit = int(sys.argv[6])
res = int(sys.argv[7])
path.parent.mkdir(parents=True, exist_ok=True)

# RGB needs three BitsPerSample values, which do not fit inline in an IFD entry.
photometric = 2 if samples == 3 else 1
pixel_data = bytes([fill]) * (width * height * samples)

data_offset = 8
strip_bytes = len(pixel_data)
cursor = data_offset + strip_bytes
if cursor % 2:
    # Pad only to keep the following IFD word-aligned.  StripByteCounts is computed
    # from the unpadded length: counting the pad would declare a strip one byte longer
    # than the row, and libtiff would read into whatever follows the pixel data.
    pixel_data += b"\0"
    cursor += 1

bps_offset = cursor
if samples > 1:
    cursor += 2 * samples

ifd_offset = cursor

entries = [
    (256, 4, 1, width),
    (257, 4, 1, height),
    (258, 3, samples, bps_offset if samples > 1 else 8),
    (259, 3, 1, 1),
    (262, 3, 1, photometric),
    (273, 4, 1, data_offset),
    (277, 3, 1, samples),
    (278, 4, 1, height),
    (279, 4, 1, strip_bytes),
    (282, 5, 1, 0),
    (283, 5, 1, 0),
    (284, 3, 1, 1),
    (296, 3, 1, resunit),
]
entries.sort()

ifd_size = 2 + 12 * len(entries) + 4
xres_offset = ifd_offset + ifd_size
yres_offset = xres_offset + 8
entries = [
    (tag, kind, count, xres_offset if tag == 282 else yres_offset if tag == 283 else value)
    for (tag, kind, count, value) in entries
]

data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset))
data.extend(pixel_data)
if samples > 1:
    data.extend(struct.pack("<H", 8) * samples)
data.extend(struct.pack("<H", len(entries)))
for tag, kind, count, value in entries:
    data.extend(struct.pack("<HHI", tag, kind, count))
    if kind == 3 and count == 1:
        data.extend(struct.pack("<H", value))
        data.extend(b"\0\0")
    else:
        data.extend(struct.pack("<I", value))
data.extend(struct.pack("<I", 0))
data.extend(struct.pack("<II", res, 1))
data.extend(struct.pack("<II", res, 1))
path.write_bytes(data)
PY
}

# Print the ResolutionUnit stored in a TIFF's first IFD, or "absent" when the tag
# is not written at all -- which is what the defect produced, and what a reader
# silently resolves to inches.  Parsed from the bytes so the assertion does not
# depend on the same code the tools use.
read_resolution_unit() {
  python3 - "$1" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if len(data) < 8 or data[:2] not in (b"II", b"MM"):
    print("unreadable")
    sys.exit(0)

endian = "<" if data[:2] == b"II" else ">"
offset = struct.unpack_from(endian + "I", data, 4)[0]
if offset + 2 > len(data):
    print("unreadable")
    sys.exit(0)

count = struct.unpack_from(endian + "H", data, offset)[0]
for index in range(count):
    entry = offset + 2 + 12 * index
    if entry + 12 > len(data):
        break
    tag, kind = struct.unpack_from(endian + "HH", data, entry)
    if tag == 296:
        print(struct.unpack_from(endian + "H", data, entry + 8)[0])
        sys.exit(0)

print("absent")
PY
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

require_tool() {
  local name="$1"
  local tool="$2"

  if [ ! -x "$tool" ]; then
    fail_case "$name" "missing executable: $tool"
    return 1
  fi

  if ! command -v python3 >/dev/null 2>&1; then
    skip_case "$name" "python3 unavailable, cannot build or inspect TIFF fixtures"
    return 1
  fi

  return 0
}

# iccSpecSepToTiff must hand the unit of its input channels to its output.  Run
# for each of the three units TIFF defines, including RESUNIT_INCH: before the
# fix no unit tag was written at all, so the inch case is a real assertion rather
# than one that passes on both trees.
run_specsep_unit_preserved() {
  local unit="$1"
  local label="$2"
  local name="specsep-resolution-unit-$label-preserved"
  local exit_code=0
  local actual=""
  local channel=""

  TOTAL=$((TOTAL + 1))
  rm -rf "$WORKDIR"
  rm -f "$LOGFILE"
  mkdir -p "$WORKDIR"

  if ! require_tool "$name" "$SPECSEP"; then
    return
  fi

  for channel in 1 2; do
    if ! generate_tiff "$WORKDIR/spec_$channel" 254 1 1 $((channel * 17)) "$unit" 100; then
      fail_case "$name" "TIFF fixture generation failed"
      return
    fi
  done

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$WORKDIR/spec_" 1 2 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected a successful conversion, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  actual="$(read_resolution_unit "$OUTPUT_TIFF")"
  if [ "$actual" != "$unit" ]; then
    fail_case "$name" "output ResolutionUnit is $actual, expected $unit"
    return
  fi

  pass_case "$name" "output carries the input ResolutionUnit ($unit)"
}

# Channel images are required to share a format, and the resolution unit is part
# of that format: 100 pixels/inch and 100 pixels/cm are the same two numbers
# describing different images.  Comparing only the resolution values accepted the
# set and silently used the first channel's unit for all of them.
run_specsep_unit_mismatch_rejected() {
  local name="specsep-resolution-unit-mismatch-rejected"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -rf "$WORKDIR"
  rm -f "$LOGFILE"
  mkdir -p "$WORKDIR"

  if ! require_tool "$name" "$SPECSEP"; then
    return
  fi

  if ! generate_tiff "$WORKDIR/spec_1" 254 1 1 17 "$RESUNIT_INCH" 100 ||
     ! generate_tiff "$WORKDIR/spec_2" 254 1 1 34 "$RESUNIT_CENTIMETER" 100; then
    fail_case "$name" "TIFF fixture generation failed"
    return
  fi

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$WORKDIR/spec_" 1 2 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "does not have same format as other files" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing the format-mismatch rejection text"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ -e "$OUTPUT_TIFF" ]; then
    fail_case "$name" "output TIFF created despite mismatched resolution units"
    return
  fi

  pass_case "$name" "mixed inch/centimeter channels rejected before output creation"
}

# The second caller of the same shared Create().  iccApplyProfiles copies the
# source resolution onto its destination, so it dropped the unit the same way.
run_applyprofiles_unit_preserved() {
  local name="applyprofiles-resolution-unit-centimeter-preserved"
  local exit_code=0
  local actual=""

  TOTAL=$((TOTAL + 1))
  rm -rf "$WORKDIR"
  rm -f "$LOGFILE"
  mkdir -p "$WORKDIR"

  if ! require_tool "$name" "$APPLYPROFILES"; then
    return
  fi

  if [ ! -f "$SRGB_PROFILE" ]; then
    fail_case "$name" "missing profile fixture: $SRGB_PROFILE"
    return
  fi

  if ! generate_tiff "$WORKDIR/source.tif" 254 1 3 128 "$RESUNIT_CENTIMETER" 100; then
    fail_case "$name" "TIFF fixture generation failed"
    return
  fi

  timeout 120 "$APPLYPROFILES" "$WORKDIR/source.tif" "$OUTPUT_TIFF" 0 0 0 0 1 \
    "$SRGB_PROFILE" 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected a successful conversion, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  actual="$(read_resolution_unit "$OUTPUT_TIFF")"
  if [ "$actual" != "$RESUNIT_CENTIMETER" ]; then
    fail_case "$name" "output ResolutionUnit is $actual, expected $RESUNIT_CENTIMETER"
    return
  fi

  pass_case "$name" "destination carries the source ResolutionUnit ($RESUNIT_CENTIMETER)"
}

# The third consumer of the same member.  iccTiffDump printed "pixels per/inch"
# for every image and computed the physical size by dividing pixels by the
# resolution regardless of unit, so a centimeter-based image was reported at
# 2.54x its real size with a label that contradicted the file.
run_tiffdump_reports_unit() {
  local name="tiffdump-reports-resolution-unit"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -rf "$WORKDIR"
  rm -f "$LOGFILE"
  mkdir -p "$WORKDIR"

  if ! require_tool "$name" "$TIFFDUMP"; then
    return
  fi

  # 254 pixels at 100 pixels/cm is 2.54 cm, i.e. exactly 1.00 inch.
  if ! generate_tiff "$WORKDIR/source.tif" 254 1 1 17 "$RESUNIT_CENTIMETER" 100; then
    fail_case "$name" "TIFF fixture generation failed"
    return
  fi

  timeout 60 "$TIFFDUMP" "$WORKDIR/source.tif" > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected a successful dump, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "pixels per/centimeter" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "resolution unit not reported as centimeters"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq '(1.00" x ' "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "physical width not converted to inches (254 px at 100 px/cm is 1.00 inch)"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  pass_case "$name" "centimeter resolution reported by unit and converted to inches"
}

# RESUNIT_NONE is not hypothetical: the checked-in
# .github/ci/test-data/specsep-harvest/gray300/spec_* fixtures declare it, and specsep
# output derived from them now inherits it.  Under that unit the resolution values fix
# an aspect ratio and no absolute size exists, so the physical-size figure has to be
# omitted rather than invented.
run_tiffdump_relative_resolution() {
  local name="tiffdump-relative-resolution-omits-physical-size"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -rf "$WORKDIR"
  rm -f "$LOGFILE"
  mkdir -p "$WORKDIR"

  if ! require_tool "$name" "$TIFFDUMP"; then
    return
  fi

  if ! generate_tiff "$WORKDIR/source.tif" 254 1 1 17 "$RESUNIT_NONE" 100; then
    fail_case "$name" "TIFF fixture generation failed"
    return
  fi

  timeout 60 "$TIFFDUMP" "$WORKDIR/source.tif" > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected a successful dump, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "(relative, no absolute unit)" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "RESUNIT_NONE not reported as a relative resolution"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if grep -Eq '^Size:.*"' "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "physical size printed for an image that declares no absolute unit"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  pass_case "$name" "relative resolution reported without inventing a physical size"
}

echo "=== TIFF ResolutionUnit propagation regression ==="
run_specsep_unit_preserved "$RESUNIT_CENTIMETER" "centimeter"
run_specsep_unit_preserved "$RESUNIT_INCH" "inch"
run_specsep_unit_preserved "$RESUNIT_NONE" "none"
run_specsep_unit_mismatch_rejected
run_applyprofiles_unit_preserved
run_tiffdump_reports_unit
run_tiffdump_relative_resolution

if [ "$SKIP" -ne 0 ]; then
  echo "TIFF ResolutionUnit propagation regression: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total"
else
  echo "TIFF ResolutionUnit propagation regression: $PASS passed, $FAIL failed, $TOTAL total"
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
