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

  pass_case "$name" "created output TIFF"
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

echo "=== iccSpecSepToTiff CLI argument regression ==="

if [ ! -x "$SPECSEP" ]; then
  echo "iccSpecSepToTiff not found at $SPECSEP -- skipping (build Tools first)"
  exit 0
fi

run_expect_success \
  "specsep-harvest-gray300-profile" \
  "$OUTDIR/harvest-gray300-profile.tif" \
  0 0 "$HARVEST_PREFIX" 1 8 1 "$PROFILE"
check_tiffinfo_contains "specsep-harvest-gray300-profile" "$OUTDIR/harvest-gray300-profile.tif" "Samples/Pixel: 8" &&
  check_tiffinfo_contains "specsep-harvest-gray300-profile" "$OUTDIR/harvest-gray300-profile.tif" "ICC Profile: <present>" &&
  check_tiffinfo_contains "specsep-harvest-gray300-profile" "$OUTDIR/harvest-gray300-profile.tif" "Image Width: 300 Image Length: 300"

run_expect_success \
  "specsep-fast-separate-planes" \
  "$OUTDIR/fast-separate.tif" \
  0 1 "$SPECTRAL_PREFIX" 1 10 1
check_tiffinfo_contains "specsep-fast-separate-planes" "$OUTDIR/fast-separate.tif" "Planar Configuration: separate image planes"

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
  "specsep-non-divisible-range" \
  "increment does not land on end" \
  "$OUTDIR/non-divisible-range.tif" \
  0 0 "$SPECTRAL_PREFIX" 1 10 4

echo "iccSpecSepToTiff CLI argument regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
