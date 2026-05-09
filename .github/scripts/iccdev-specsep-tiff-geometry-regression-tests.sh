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

echo "=== iccSpecSepToTiff malformed TIFF geometry regression ==="
run_specsep_geometry_reject
echo "iccSpecSepToTiff malformed TIFF geometry regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
