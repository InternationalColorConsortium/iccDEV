#!/bin/bash
###############################################################################
# "sameAsSource" compression and planar configuration regression (#2379 item 3)
###############################################################################
#
# iccApplyProfiles read a JSON config's "dstCompression": "sameAsSource" and
# "dstPlanar": "sameAsSource" by converting the source image's TIFF tag straight
# to bool:
#
#   bCompress = ... ? SrcImg.GetCompress() : ...
#   bSeparation = ... ? SrcImg.GetPlanar() : ...
#
# COMPRESSION_NONE is 1 and PLANARCONFIG_CONTIG is 1, so both accessors are
# nonzero for every TIFF libtiff can open and both tests were constant true.
# Create() then maps true onto COMPRESSION_LZW and PLANARCONFIG_SEPARATE
# (TiffImg.cpp:443-444), so a config asking to copy the source wrote LZW over an
# uncompressed image and separated planes over a contiguous one.  The request
# was not ignored, it was inverted, and the source's own value was never read.
#
# "sameAsSource" is reachable only through -cfg: the positional CLI path parses
# these two flags as 0/1 and can never produce icDstBoolFromSrc
# (IccCmmConfig.cpp:520-524), which is why nothing caught this.
#
# Case 1 is the red one -- it fails before the fix and passes after.  Case 2 is
# its control: it passes on both trees, and exists so an inverted fix (one that
# always reports "not compressed") cannot go unnoticed.  Cases 3 and 4 pin the
# explicit true/false branches the fix does not touch.
#
# Assertions read TIFFTAG_COMPRESSION (259) and TIFFTAG_PLANARCONFIG (284) out
# of the output file's IFD directly rather than through iccTiffDump, which reads
# them back through the same CTiffImg the writer used: a tool that misreports a
# tag could otherwise make a case pass for the wrong reason.  An absent tag is
# resolved to its TIFF 6.0 default, so the assertion is about the value a reader
# actually sees.
#
# Fixtures are tracked, not generated, so this needs no profile fixture:
#   Testing/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif  -- uncompressed, contiguous
#   .github/ci/regression/para-gamma-2.4.icc           -- RGB source profile
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2379-tiff-sameassource}"
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

APPLYPROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
SEED_TIFF="$REPO_ROOT/Testing/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
RGB_PROFILE="$REPO_ROOT/.github/ci/regression/para-gamma-2.4.icc"

WORKDIR="$OUTDIR/tiff-sameassource"
LOGFILE="$OUTDIR/tiff-sameassource.log"

# TIFF 6.0 values for the two tags under test.
COMPRESSION_NONE=1
COMPRESSION_LZW=5
PLANARCONFIG_CONTIG=1
PLANARCONFIG_SEPARATE=2

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  echo "  [FAIL] $1 -- $2"
  FAIL=$((FAIL + 1))
}

pass_case() {
  echo "  [PASS] $1 -- $2"
  PASS=$((PASS + 1))
}

# Print "<compression> <planarconfig>" for a TIFF's first IFD, resolving an
# absent tag to its TIFF 6.0 default (no compression, contiguous planes) so the
# result is the value a reader resolves rather than the value that happens to be
# stored.
read_tiff_flags() {
  python3 - "$1" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if len(data) < 8 or data[:2] not in (b"II", b"MM"):
    print("unreadable unreadable")
    sys.exit(0)

endian = "<" if data[:2] == b"II" else ">"
offset = struct.unpack_from(endian + "I", data, 4)[0]
if offset + 2 > len(data):
    print("unreadable unreadable")
    sys.exit(0)

# TIFF 6.0 defaults for the two tags, used when the tag is not stored at all.
values = {259: 1, 284: 1}
count = struct.unpack_from(endian + "H", data, offset)[0]
for index in range(count):
    entry = offset + 2 + 12 * index
    if entry + 12 > len(data):
        break
    tag, kind = struct.unpack_from(endian + "HH", data, entry)
    if tag in values and kind == 3:
        values[tag] = struct.unpack_from(endian + "H", data, entry + 8)[0]

print(f"{values[259]} {values[284]}")
PY
}

# Write a -cfg config that applies one profile to one image.  The two flags are
# injected verbatim so a case can pass a JSON boolean or the "sameAsSource"
# string.
#   write_config <config> <src> <dst> <dstCompression> <dstPlanar>
write_config() {
  cat > "$1" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$2",
    "dstImageFile": "$3",
    "dstEncoding": "8Bit",
    "dstCompression": $4,
    "dstPlanar": $5,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "$RGB_PROFILE",
      "intent": 1,
      "interpolation": "tetrahedral"
    }
  ]
}
EOF
}

check_sanitizers() {
  if grep -q "ERROR: AddressSanitizer" "$2" 2>/dev/null; then
    fail_case "$1" "AddressSanitizer finding"
    return 1
  fi

  if grep -q "runtime error:" "$2" 2>/dev/null; then
    fail_case "$1" "undefined behavior"
    return 1
  fi

  return 0
}

# Run one apply and assert the two tags on its output.
#   run_case <name> <src> <dstCompression> <dstPlanar> <want_compression> <want_planar> <why>
run_case() {
  local name="$1" src="$2" want_compress="$5" want_planar="$6" why="$7"
  local config="$WORKDIR/$name.json"
  local output="$WORKDIR/$name.tif"
  local exit_code=0
  local flags="" actual_compress="" actual_planar=""

  TOTAL=$((TOTAL + 1))
  rm -f "$output" "$LOGFILE"

  if [ ! -s "$src" ]; then
    fail_case "$name" "missing source image: $src"
    return 1
  fi

  write_config "$config" "$src" "$output" "$3" "$4"

  # Well under the CTest TIMEOUT this is registered with, so a hung case is reported
  # here -- with its name and its log tail -- instead of being swallowed by CTest
  # killing the whole script.
  timeout 30 "$APPLYPROFILES" -cfg "$config" > "$LOGFILE" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$LOGFILE"; then
    sed -n '1,40p' "$LOGFILE"
    return 1
  fi

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected a successful conversion, got exit=$exit_code"
    sed -n '1,40p' "$LOGFILE"
    return 1
  fi

  flags="$(read_tiff_flags "$output")"
  actual_compress="${flags% *}"
  actual_planar="${flags#* }"

  if [ "$actual_compress" != "$want_compress" ] || [ "$actual_planar" != "$want_planar" ]; then
    fail_case "$name" \
      "output Compression=$actual_compress PlanarConfig=$actual_planar, expected Compression=$want_compress PlanarConfig=$want_planar"
    return 1
  fi

  pass_case "$name" "$why"
  return 0
}

echo "=== iccApplyProfiles sameAsSource compression/planar regression (#2379) ==="

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

if [ ! -x "$APPLYPROFILES" ]; then
  echo "  [SKIP] missing executable: $APPLYPROFILES"
  exit 77
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "  [SKIP] python3 unavailable, cannot read TIFF directory entries"
  exit 77
fi

if [ ! -s "$SEED_TIFF" ] || [ ! -s "$RGB_PROFILE" ]; then
  echo "[FAIL] tracked fixtures missing: $SEED_TIFF / $RGB_PROFILE"
  exit 1
fi

# The tracked seed is the uncompressed, contiguous end of the matrix.  Assert
# that before relying on it, so a replaced fixture cannot quietly turn case 1
# into a test of nothing.
seed_flags="$(read_tiff_flags "$SEED_TIFF")"
if [ "$seed_flags" != "$COMPRESSION_NONE $PLANARCONFIG_CONTIG" ]; then
  echo "[FAIL] seed fixture is not uncompressed+contiguous: $SEED_TIFF ($seed_flags)"
  exit 1
fi

# Case 3 first, because its output is the compressed+separate source the other
# two cases need.  Asserting its tags here is what makes that fixture trustworthy.
LZW_SEP_TIFF="$WORKDIR/explicit-true-compresses-and-separates.tif"
run_case "explicit-true-compresses-and-separates" "$SEED_TIFF" true true \
  "$COMPRESSION_LZW" "$PLANARCONFIG_SEPARATE" \
  "an explicit true still selects LZW and separate planes"
if [ ! -s "$LZW_SEP_TIFF" ]; then
  echo "[FAIL] could not build the compressed+separate source fixture"
  echo "iccApplyProfiles sameAsSource regression: $PASS passed, $FAIL failed, $TOTAL total"
  exit 1
fi

# Case 1 -- the defect.  Before the fix this produced Compression=5 and
# PlanarConfig=2 from a source carrying 1 and 1.
run_case "sameassource-keeps-uncompressed-contiguous" "$SEED_TIFF" '"sameAsSource"' '"sameAsSource"' \
  "$COMPRESSION_NONE" "$PLANARCONFIG_CONTIG" \
  "an uncompressed contiguous source is copied, not silently rewritten as LZW+separate"

# Case 2 -- the control.  Passes on both trees; catches a fix that answers
# "false" unconditionally instead of reading the source.
run_case "sameassource-keeps-compressed-separate" "$LZW_SEP_TIFF" '"sameAsSource"' '"sameAsSource"' \
  "$COMPRESSION_LZW" "$PLANARCONFIG_SEPARATE" \
  "a compressed separated source is still copied as compressed and separated"

# Case 4 -- the other untouched branch.
run_case "explicit-false-overrides-compressed-source" "$LZW_SEP_TIFF" false false \
  "$COMPRESSION_NONE" "$PLANARCONFIG_CONTIG" \
  "an explicit false still overrides a compressed separated source"

echo "iccApplyProfiles sameAsSource regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
