#!/bin/bash
###############################################################################
# iccSpecSepToTiff CLI argument regression tests
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-specsep-cli-args-regression}"
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
SPECTRAL_PREFIX="$REPO_ROOT/.github/ci/test-data/spectral/spec_"
HARVEST_PREFIX="$REPO_ROOT/.github/ci/test-data/specsep-harvest/gray300/spec_"
PROFILE="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"
TEXT_PROFILE="$REPO_ROOT/Tools/CmdLine/IccSpecSepToTiff/Readme.md"
EMPTY_PROFILE="$OUTDIR/empty-profile.icc"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
SPECTRAL_XML="$REPO_ROOT/Testing/ICS/Spec400_10_700-D50_2deg-Part1.xml"
SPECTRAL_PROFILE="$OUTDIR/Spec400_10_700-D50_2deg-Part1.icc"
SPECTRAL31_DIR="$OUTDIR/spectral31"

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
  local log="$2"
  if grep -Eq "ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in tool output"
    sed -n '1,40p' "$log"
    return 1
  fi
  return 0
}

run_expect_success() {
  local name="$1"
  local out="$2"
  shift 2
  local log="$OUTDIR/$name.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$out" "$log"

  timeout 60 "$SPECSEP" "$out" "$@" > "$log" 2>&1 || exit_code=$?

  check_sanitizers "$name" "$log" || return

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected success, got exit=$exit_code"
    sed -n '1,40p' "$log"
    return
  fi

  if [ ! -s "$out" ]; then
    fail_case "$name" "expected output TIFF was not created"
    sed -n '1,40p' "$log"
    return
  fi

  for expected in "Output:" "Size:" "BitsPerSample:" "SamplesPerPixel:" \
                  "Planar:" "Compression:" "Profile:" "Image successfully written!"; do
    if ! grep -Fq "$expected" "$log" 2>/dev/null; then
      fail_case "$name" "success output missing summary field: $expected"
      sed -n '1,80p' "$log"
      return
    fi
  done

  pass_case "$name" "created output TIFF: $out"
}

run_expect_reject() {
  local name="$1"
  local expected="$2"
  local out="$3"
  shift 3
  local log="$OUTDIR/$name.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$out" "$log"

  timeout 60 "$SPECSEP" "$out" "$@" > "$log" 2>&1 || exit_code=$?

  check_sanitizers "$name" "$log" || return

  if [ "$exit_code" -ne 255 ]; then
    fail_case "$name" "expected graceful reject exit=255, got exit=$exit_code"
    sed -n '1,40p' "$log"
    return
  fi

  if ! grep -Fq "$expected" "$log" 2>/dev/null; then
    fail_case "$name" "missing expected diagnostic: $expected"
    sed -n '1,40p' "$log"
    return
  fi

  if [ -e "$out" ]; then
    fail_case "$name" "output TIFF created despite rejected arguments"
    return
  fi

  pass_case "$name" "rejected malformed arguments"
}

check_tiffinfo_contains() {
  local name="$1"
  local file="$2"
  local expected="$3"
  local log="$OUTDIR/$name.tiffinfo"

  if ! command -v tiffinfo >/dev/null 2>&1; then
    return 0
  fi

  tiffinfo "$file" > "$log" 2>&1 || {
    fail_case "$name" "tiffinfo could not inspect output"
    sed -n '1,40p' "$log"
    return 1
  }

  if ! grep -Fq "$expected" "$log" 2>/dev/null; then
    fail_case "$name" "tiffinfo output missing: $expected"
    sed -n '1,80p' "$log"
    return 1
  fi

  return 0
}

check_tiff_data_contains() {
  local name="$1"
  local file="$2"
  local expected="$3"
  local log="$OUTDIR/$name.tiffdata"

  if ! command -v tiffinfo >/dev/null 2>&1; then
    return 0
  fi

  tiffinfo -d "$file" > "$log" 2>&1 || {
    fail_case "$name" "tiffinfo could not decode output pixels"
    sed -n '1,80p' "$log"
    return 1
  }

  if ! grep -Fq "$expected" "$log" 2>/dev/null; then
    fail_case "$name" "decoded TIFF data missing channel order: $expected"
    sed -n '1,120p' "$log"
    return 1
  fi

  return 0
}

generate_spectral31_inputs() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$SPECTRAL31_DIR" <<'PY'
import pathlib
import struct
import sys

root = pathlib.Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)


def ifd_entry(tag, tag_type, count, value):
    entry = struct.pack("<HHI", tag, tag_type, count)
    if tag_type == 3 and count == 1:
        return entry + struct.pack("<H", value) + b"\0\0"
    return entry + struct.pack("<I", value)


def write_tiff(path, fill):
    width = height = 2
    pixels = bytes([fill & 0xff]) * (width * height)
    ifd_offset = 8 + len(pixels)
    entries = [
        (256, 4, 1, width),
        (257, 4, 1, height),
        (258, 3, 1, 8),
        (259, 3, 1, 1),
        (262, 3, 1, 1),
        (273, 4, 1, 8),
        (277, 3, 1, 1),
        (278, 4, 1, height),
        (279, 4, 1, len(pixels)),
        (284, 3, 1, 1),
        (296, 3, 1, 2),
        (339, 3, 1, 1),
    ]
    data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset))
    data.extend(pixels)
    data.extend(struct.pack("<H", len(entries)))
    for entry in sorted(entries):
        data.extend(ifd_entry(*entry))
    data.extend(struct.pack("<I", 0))
    path.write_bytes(data)


for i in range(1, 32):
    write_tiff(root / f"spec_{i}", i * 7)
PY
}

prepare_spectral_profile() {
  if [ ! -x "$FROMXML" ] || [ ! -f "$SPECTRAL_XML" ]; then
    return 1
  fi

  "$FROMXML" "$SPECTRAL_XML" "$SPECTRAL_PROFILE" > "$OUTDIR/fromxml-Spec400_10_700-D50_2deg-Part1.log" 2>&1 &&
    [ -s "$SPECTRAL_PROFILE" ] &&
    generate_spectral31_inputs
}

echo "=== iccSpecSepToTiff CLI argument regression ==="

if [ ! -x "$SPECSEP" ]; then
  echo "iccSpecSepToTiff not found at $SPECSEP -- skipping (build Tools first)"
  exit 0
fi

run_expect_success \
  "specsep-harvest-gray300-no-profile" \
  "$OUTDIR/harvest-gray300-no-profile.tif" \
  0 0 "$HARVEST_PREFIX" 1 8 1
check_tiffinfo_contains "specsep-harvest-gray300-no-profile" "$OUTDIR/harvest-gray300-no-profile.tif" "Samples/Pixel: 8" &&
  check_tiffinfo_contains "specsep-harvest-gray300-no-profile" "$OUTDIR/harvest-gray300-no-profile.tif" "Image Width: 300 Image Length: 300"

run_expect_success \
  "specsep-srgb-profile-matching-3ch" \
  "$OUTDIR/srgb-profile-matching-3ch.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 3 1 "$PROFILE"
check_tiffinfo_contains "specsep-srgb-profile-matching-3ch" "$OUTDIR/srgb-profile-matching-3ch.tif" "Samples/Pixel: 3" &&
  check_tiffinfo_contains "specsep-srgb-profile-matching-3ch" "$OUTDIR/srgb-profile-matching-3ch.tif" "ICC Profile: <present>"

run_expect_success \
  "specsep-fast-separate-planes" \
  "$OUTDIR/fast-separate.tif" \
  0 1 "$SPECTRAL_PREFIX" 1 10 1
check_tiffinfo_contains "specsep-fast-separate-planes" "$OUTDIR/fast-separate.tif" "Planar Configuration: separate image planes"

run_expect_success \
  "specsep-channel-order-ascending" \
  "$OUTDIR/channel-order-ascending.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 3 1
check_tiff_data_contains "specsep-channel-order-ascending" "$OUTDIR/channel-order-ascending.tif" "99 19 32 33 cb 4c"

run_expect_success \
  "specsep-channel-order-descending" \
  "$OUTDIR/channel-order-descending.tif" \
  0 0 "$SPECTRAL_PREFIX" 3 1 -1
check_tiff_data_contains "specsep-channel-order-descending" "$OUTDIR/channel-order-descending.tif" "cb 4c 32 33 99 19"

if prepare_spectral_profile; then
  run_expect_success \
    "specsep-spectral-profile-matching-31ch" \
    "$OUTDIR/spectral-profile-matching-31ch.tif" \
    0 0 "$SPECTRAL31_DIR/spec_" 1 31 1 "$SPECTRAL_PROFILE"

  run_expect_reject \
    "specsep-spectral-profile-sample-mismatch" \
    "do not match TIFF SamplesPerPixel" \
    "$OUTDIR/spectral-profile-sample-mismatch.tif" \
    0 0 "$SPECTRAL31_DIR/spec_" 1 4 1 "$SPECTRAL_PROFILE"
else
  echo "  [SKIP] specsep-spectral-profile-alignment -- missing iccFromXml or Spec400_10_700-D50_2deg-Part1.xml"
fi

run_expect_reject \
  "specsep-invalid-compress-bool" \
  "Invalid boolean value for compress or sep" \
  "$OUTDIR/invalid-compress.tif" \
  2 0 "$SPECTRAL_PREFIX" 1 3 1

run_expect_reject \
  "specsep-invalid-sep-bool" \
  "Invalid boolean value for compress or sep" \
  "$OUTDIR/invalid-sep.tif" \
  0 nope "$SPECTRAL_PREFIX" 1 3 1

run_expect_reject \
  "specsep-extra-arg" \
  "Usage:" \
  "$OUTDIR/extra-arg.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 3 1 "$PROFILE" extra-token

run_expect_reject \
  "specsep-missing-profile" \
  "Cannot open profile" \
  "$OUTDIR/missing-profile.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 3 1 "$OUTDIR/no-such-profile.icc"

run_expect_reject \
  "specsep-non-icc-profile" \
  "Cannot parse profile" \
  "$OUTDIR/non-icc-profile.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 3 1 "$TEXT_PROFILE"

: > "$EMPTY_PROFILE"
run_expect_reject \
  "specsep-empty-profile" \
  "refusing to embed zero-length ICC data" \
  "$OUTDIR/empty-profile.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 3 1 "$EMPTY_PROFILE"

run_expect_reject \
  "specsep-profile-sample-mismatch" \
  "do not match TIFF SamplesPerPixel" \
  "$OUTDIR/profile-sample-mismatch.tif" \
  0 0 "$HARVEST_PREFIX" 1 8 1 "$PROFILE"

run_expect_reject \
  "specsep-non-divisible-range" \
  "increment does not land on end" \
  "$OUTDIR/non-divisible-range.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 10 4

run_expect_reject \
  "specsep-nan-start" \
  "Invalid channel range" \
  "$OUTDIR/nan-start.tif" \
  0 0 "$SPECTRAL_PREFIX" nan 3 1

run_expect_reject \
  "specsep-float-start" \
  "Invalid channel range" \
  "$OUTDIR/float-start.tif" \
  0 0 "$SPECTRAL_PREFIX" 1.0 3 1

run_expect_reject \
  "specsep-hex-start" \
  "Invalid channel range" \
  "$OUTDIR/hex-start.tif" \
  0 0 "$SPECTRAL_PREFIX" 0x1 3 1

run_expect_reject \
  "specsep-plus-start" \
  "Invalid channel range" \
  "$OUTDIR/plus-start.tif" \
  0 0 "$SPECTRAL_PREFIX" +1 3 1

run_expect_reject \
  "specsep-leading-space-start" \
  "Invalid channel range" \
  "$OUTDIR/leading-space-start.tif" \
  0 0 "$SPECTRAL_PREFIX" " 1" 3 1

run_expect_reject \
  "specsep-int-overflow-start" \
  "Invalid channel range" \
  "$OUTDIR/int-overflow-start.tif" \
  0 0 "$SPECTRAL_PREFIX" 2147483648 3 1

run_expect_reject \
  "specsep-int-underflow-start" \
  "Invalid channel range" \
  "$OUTDIR/int-underflow-start.tif" \
  0 0 "$SPECTRAL_PREFIX" -2147483649 3 1

run_expect_reject \
  "specsep-printf-pattern-prefix" \
  "Cannot open input" \
  "$OUTDIR/printf-pattern-prefix.tif" \
  0 0 "$SPECTRAL_PREFIX%03d.tif" 1 3 1

echo "iccSpecSepToTiff CLI argument regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
