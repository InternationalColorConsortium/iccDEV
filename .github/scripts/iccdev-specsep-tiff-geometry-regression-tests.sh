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
PALETTE_DIR="$OUTDIR/specsep-palette"
FLOAT_DIR="$OUTDIR/specsep-float-miniswhite"
OVERSIZE_DIR="$OUTDIR/specsep-oversized-row"
TRUNC_DIR="$OUTDIR/specsep-truncated"
BPS24_DIR="$OUTDIR/specsep-bps24"
LOGFILE="$OUTDIR/specsep-tiff-geometry.log"
OUTPUT_TIFF="$OUTDIR/specsep-geometry-output.tif"

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
  local dump_log="$OUTDIR/specsep-descending-tiffdump.log"

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF" "$dump_log"

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

  if command -v tiffdump >/dev/null 2>&1; then
    tiffdump "$OUTPUT_TIFF" > "$dump_log" 2>&1 || {
      fail_case "$name" "tiffdump could not inspect generated output"
      sed -n '1,40p' "$dump_log"
      return
    }
    if ! grep -Fq "ExtraSamples (338) SHORT (3) 2<0 0>" "$dump_log"; then
      fail_case "$name" "generated multi-sample TIFF lacks unspecified ExtraSamples"
      sed -n '1,80p' "$dump_log"
      return
    fi
  fi

  pass_case "$name" "descending range accepted with explicit ExtraSamples metadata"
}

generate_palette_tiff() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  local path="$1"
  mkdir -p "$(dirname "$path")"
  # Minimal 2x2 8-bit PHOTOMETRIC_PALETTE (=3) TIFF, including the required
  # ColorMap (tag 320); libtiff treats a palette photometric without a colormap
  # as min-is-black, so the colormap is what makes this a genuine palette file.
  python3 - "$path" <<'PY'
import struct, sys, pathlib
path = pathlib.Path(sys.argv[1])
w = h = 2; bps = 8; ncol = 1 << bps
pix = bytes(w * h); doff = 8; ifd = doff + len(pix)
if ifd % 2:
    pix += b"\0"; ifd += 1
cmap = b"".join(struct.pack("<H", (i * 65535) // (ncol - 1))
                for _ in range(3) for i in range(ncol))
entries = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, 1, bps), (259, 3, 1, 1),
           (262, 3, 1, 3), (273, 4, 1, doff), (277, 3, 1, 1), (278, 4, 1, h),
           (279, 4, 1, len(pix)), (284, 3, 1, 1), (296, 3, 1, 2),
           (320, 3, 3 * ncol, 0)]
entries.sort()
cmap_off = ifd + 2 + len(entries) * 12 + 4
entries = [(t, ty, c, (cmap_off if t == 320 else v)) for (t, ty, c, v) in entries]
d = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd)); d += pix
d += struct.pack("<H", len(entries))
for t, ty, c, v in entries:
    d += struct.pack("<HHI", t, ty, c)
    d += (struct.pack("<H", v) + b"\0\0") if (ty == 3 and c == 1) else struct.pack("<I", v)
d += struct.pack("<I", 0); d += cmap
path.write_bytes(d)
PY
}

run_specsep_palette_reject() {
  local name="specsep-palette-reject"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  if ! generate_palette_tiff "$PALETTE_DIR/spec_0"; then
    fail_case "$name" "python3 unavailable or palette TIFF generation failed"
    return
  fi

  # iccSpecSepToTiff must refuse palette input: GetPhoto() returns the internal
  # PHOTO_PALETTE enum, which the tool now compares correctly (#1381).
  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$PALETTE_DIR/spec_" 0 0 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "is a palette based file" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing palette rejection text (#1381)"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ -e "$OUTPUT_TIFF" ]; then
    fail_case "$name" "output TIFF created despite palette rejection"
    return
  fi

  pass_case "$name" "palette input rejected (#1381) without sanitizer findings"
}

generate_tagged_tiff() {
  # Writes a single-channel TIFF with explicit photometric/sample-format/geometry
  # control, laying the IFD ahead of the pixel data so a caller can truncate the
  # data without destroying the directory.
  local path="$1"
  local width="$2"
  local height="$3"
  local bps="$4"
  local photometric="$5"
  local sampleformat="$6"
  local payload="$7"
  local declared_bytes="$8"

  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$path" "$width" "$height" "$bps" "$photometric" "$sampleformat" "$payload" "$declared_bytes" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
width, height, bps = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
photometric, sampleformat = int(sys.argv[5]), int(sys.argv[6])
payload_kind = sys.argv[7]
declared_bytes = int(sys.argv[8])
path.parent.mkdir(parents=True, exist_ok=True)

if payload_kind == "float-ramp":
    pixel_data = struct.pack("<4f", 0.0, 0.25, 0.5, 1.0)
elif payload_kind == "short":
    pixel_data = b"\x11" * 4
else:
    pixel_data = b"\x00" * 64

entries = []
def add(tag, tag_type, count, value):
    entries.append((tag, tag_type, count, value))

n_entries = 13
ifd_offset = 8
ifd_size = 2 + n_entries * 12 + 4
xres_offset = ifd_offset + ifd_size
yres_offset = xres_offset + 8
data_offset = yres_offset + 8

add(256, 4, 1, width)                                    # ImageWidth
add(257, 4, 1, height)                                   # ImageLength
add(258, 3, 1, bps)                                      # BitsPerSample
add(259, 3, 1, 1)                                        # Compression: none
add(262, 3, 1, photometric)                              # PhotometricInterpretation
add(273, 4, 1, data_offset)                              # StripOffsets
add(277, 3, 1, 1)                                        # SamplesPerPixel
add(278, 4, 1, height)                                   # RowsPerStrip
add(279, 4, 1, declared_bytes if declared_bytes else len(pixel_data))
add(282, 5, 1, xres_offset)                              # XResolution
add(283, 5, 1, yres_offset)                              # YResolution
add(296, 3, 1, 2)                                        # ResolutionUnit: inch
add(339, 3, 1, sampleformat)                             # SampleFormat
assert len(entries) == n_entries
entries.sort()

data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset))
data.extend(struct.pack("<H", len(entries)))
for tag, tag_type, count, value in entries:
    data.extend(struct.pack("<HHI", tag, tag_type, count))
    if tag_type == 3 and count == 1:
        data.extend(struct.pack("<H", value) + b"\0\0")
    else:
        data.extend(struct.pack("<I", value))
data.extend(struct.pack("<I", 0))
data.extend(struct.pack("<II", 72, 1))
data.extend(struct.pack("<II", 72, 1))
data.extend(pixel_data)
path.write_bytes(data)
PY
}

tool_is_sanitized() {
  # ASan/TSan/MSan-instrumented binaries reserve terabytes of shadow memory, so any
  # `ulimit -v` cap makes it die at startup regardless of the defect under test.
  # Detect instrumentation from the binary rather than from a CTest label.
  # Fail closed: if neither probe is installed we cannot tell, and guessing
  # "not sanitized" would run the ulimit case against an ASan build and produce
  # a red leg caused by missing binutils rather than by the code.
  if ! command -v nm >/dev/null 2>&1 && ! command -v strings >/dev/null 2>&1; then
    return 0
  fi

  # Capture the output rather than piping it into `grep -q`: grep exits at the
  # first match and kills nm with SIGPIPE, which `set -o pipefail` (line 10)
  # reports as 141.  That made the guard read a genuinely instrumented binary as
  # "not sanitized" and run it under the cap anyway, which is the red leg this
  # function exists to prevent.
  local symbols=""
  symbols="$(nm -D "$SPECSEP" 2>/dev/null || true)"
  case "$symbols" in
    *__asan_init*|*__tsan_init*|*__msan_init*) return 0 ;;
  esac

  local text=""
  text="$(strings "$SPECSEP" 2>/dev/null || true)"
  case "$text" in
    *libclang_rt.asan*|*libclang_rt.tsan*|*libclang_rt.msan*) return 0 ;;
  esac

  return 1
}

run_specsep_float_miniswhite_reject() {
  local name="specsep-float-miniswhite-reject"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  # MinIsWhite is applied as a per-byte XOR with 0xff.  On IEEE float samples
  # that rewrites sign and exponent instead of the value, so 0.25 became
  # -15.999999 and 0.0 became a NaN while the tool still reported success.
  if ! generate_tagged_tiff "$FLOAT_DIR/spec_1" 2 2 32 0 3 "float-ramp" 0 ||
     ! generate_tagged_tiff "$FLOAT_DIR/spec_2" 2 2 32 0 3 "float-ramp" 0; then
    fail_case "$name" "python3 unavailable or float TIFF generation failed"
    return
  fi

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$FLOAT_DIR/spec_" 1 2 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code (float samples silently corrupted)"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if ! grep -Fq "Floating point MinIsWhite input cannot be inverted" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "missing float MinIsWhite rejection text"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ -e "$OUTPUT_TIFF" ]; then
    fail_case "$name" "output TIFF created despite float MinIsWhite rejection"
    return
  fi

  pass_case "$name" "float MinIsWhite input rejected instead of silently inverted"
}

run_specsep_oversized_row_buffer_reject() {
  local name="specsep-oversized-row-buffer-reject"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  if tool_is_sanitized; then
    skip_case "$name" "sanitizer build cannot run under a ulimit -v cap"
    return
  fi

  # A 250-byte file may declare a 500,000,000-pixel wide float row, asking for a
  # 2GB row buffer.  Throwing new turned the refusal into an unhandled
  # std::bad_alloc and the tool died with SIGABRT (exit 134) instead of
  # reporting the problem.  The cap makes the allocation fail deterministically
  # rather than relying on the host's overcommit behaviour.
  if ! generate_tagged_tiff "$OVERSIZE_DIR/spec_1" 500000000 1 32 1 3 "zero" 64; then
    fail_case "$name" "python3 unavailable or oversized TIFF generation failed"
    return
  fi

  ( ulimit -v 2000000; timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$OVERSIZE_DIR/spec_" 1 1 1 ) > "$LOGFILE" 2>&1 || exit_code=$?

  # main() returns -1 on every error path, which the shell reports as 255; a
  # real signal death lands in 129..192, so 255 must not be read as a signal.
  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -lt 255 ]; then
    fail_case "$name" "tool died from signal $((exit_code - 128)) instead of reporting the allocation failure"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if grep -q "std::bad_alloc" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "unhandled std::bad_alloc reached the terminate handler"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  # Naming the diagnostic stops the case passing for the wrong reason: the
  # pre-fix binary also exits 255 when the allocation is never reached, because
  # the short strip makes ReadLine fail instead.
  if grep -Fq "Cannot allocate image row buffers" "$LOGFILE" 2>/dev/null; then
    pass_case "$name" "oversized row buffer refused without aborting"
    return
  fi

  # Getting past the allocation at all means the address-space cap was not
  # enforced -- `ulimit -v` is accepted but ignored under Git Bash on Windows
  # and on Darwin.  The tool then reaches ReadLine and exits 255 on the short
  # strip.  Neither build can be told apart there, so skip rather than report a
  # red leg caused by the platform instead of by the code.
  if grep -qE "Error reading line|Cannot open input" "$LOGFILE" 2>/dev/null; then
    skip_case "$name" "address-space cap not enforced on this platform; allocation never refused"
    return
  fi

  fail_case "$name" "did not refuse at the row-buffer allocation; the geometry no longer discriminates"
  sed -n '1,40p' "$LOGFILE"
}

run_specsep_partial_output_discarded() {
  local name="specsep-partial-output-discarded"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE" "$OUTPUT_TIFF"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  # Create() truncates any existing output, so a run that failed part way
  # through left an unreadable stub in place of a previously good result while
  # still exiting non-zero.  A failed conversion must leave no output at all.
  if ! generate_tagged_tiff "$TRUNC_DIR/spec_1" 4 4 8 1 1 "short" 16 ||
     ! generate_tagged_tiff "$TRUNC_DIR/spec_2" 4 4 8 1 1 "short" 16; then
    fail_case "$name" "python3 unavailable or truncated TIFF generation failed"
    return
  fi

  printf 'pre-existing output that must not survive a failed run' > "$OUTPUT_TIFF"

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 0 0 "$TRUNC_DIR/spec_" 1 2 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ -e "$OUTPUT_TIFF" ]; then
    fail_case "$name" "incomplete output TIFF left behind after a failed conversion"
    return
  fi

  pass_case "$name" "incomplete output discarded after a late read failure"
}

run_specsep_create_failure_preserves_existing_output() {
  local name="specsep-create-failure-preserves-existing-output"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$LOGFILE"

  if [ ! -x "$SPECSEP" ]; then
    fail_case "$name" "missing executable: $SPECSEP"
    return
  fi

  # Create() refuses compression for a 24 bit sample size *before* its
  # TIFFOpen(...,"w"), so the destination is never touched.  Discarding the
  # output unconditionally on a Create() failure would delete a pre-existing
  # file this run never opened, which is worse than the stub it was meant to
  # clean up.  24 bps clears both main()'s byte-alignment check and
  # CTiffImg::Open, so it reaches Create() and fails there.
  if ! generate_tagged_tiff "$BPS24_DIR/spec_1" 2 2 24 1 1 "short" 0 ||
     ! generate_tagged_tiff "$BPS24_DIR/spec_2" 2 2 24 1 1 "short" 0; then
    fail_case "$name" "python3 unavailable or 24 bps TIFF generation failed"
    return
  fi

  printf 'pre-existing output that Create() never opened' > "$OUTPUT_TIFF"
  local before
  before="$(cksum < "$OUTPUT_TIFF")"

  timeout 60 "$SPECSEP" "$OUTPUT_TIFF" 1 0 "$BPS24_DIR/spec_" 1 2 1 > "$LOGFILE" 2>&1 || exit_code=$?

  if ! grep -Fq "Unable to create" "$LOGFILE" 2>/dev/null; then
    fail_case "$name" "expected a Create() refusal for a compressed 24 bps output"
    sed -n '1,40p' "$LOGFILE"
    return
  fi

  if [ ! -e "$OUTPUT_TIFF" ]; then
    fail_case "$name" "pre-existing output deleted after a Create() failure that never opened it"
    return
  fi

  if [ "$(cksum < "$OUTPUT_TIFF")" != "$before" ]; then
    fail_case "$name" "pre-existing output modified after a Create() failure that never opened it"
    return
  fi

  pass_case "$name" "pre-existing output left intact when Create() refused before opening it"
}

echo "=== iccSpecSepToTiff malformed TIFF geometry regression ==="
run_specsep_geometry_reject
run_specsep_packed_bps_reject
run_specsep_descending_range_accept
run_specsep_palette_reject
run_specsep_float_miniswhite_reject
run_specsep_oversized_row_buffer_reject
run_specsep_partial_output_discarded
run_specsep_create_failure_preserves_existing_output
if [ "$SKIP" -ne 0 ]; then
  echo "iccSpecSepToTiff malformed TIFF geometry regression: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total"
else
  echo "iccSpecSepToTiff malformed TIFF geometry regression: $PASS passed, $FAIL failed, $TOTAL total"
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
