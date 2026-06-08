#!/bin/bash
###############################################################################
# iccDEV issue #1230 JSON connect.threads regression
###############################################################################

set -euo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:?Set ICCDEV_TOOLS_DIR to iccDEV Build/Tools path}"
TESTING_DIR="${ICCDEV_TESTING_DIR:?Set ICCDEV_TESTING_DIR to iccDEV Testing path}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1230-output}"

APPLY_PROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
SOURCE_IMAGE="$TESTING_DIR/hybrid/Data/TShirtDesignKW.tif"

mkdir -p "$OUTDIR"

if [ ! -x "$APPLY_PROFILES" ]; then
  echo "[FAIL] iccApplyProfiles not found: $APPLY_PROFILES"
  exit 1
fi

if [ ! -f "$PROFILE" ]; then
  echo "[FAIL] ICC profile fixture not found: $PROFILE"
  exit 1
fi

if [ ! -f "$SOURCE_IMAGE" ]; then
  echo "[FAIL] TIFF fixture not found: $SOURCE_IMAGE"
  exit 1
fi

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

TMPDIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMPDIR"
}
trap cleanup EXIT

CONFIG="$TMPDIR/issue-1230-connect-threads.json"
LOGFILE="$OUTDIR/issue-1230-connect-threads.log"
OUTPUT_IMAGE="$TMPDIR/issue-1230-output.tif"

cat > "$CONFIG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$SOURCE_IMAGE",
    "dstImageFile": "$OUTPUT_IMAGE",
    "dstEncoding": "8Bit",
    "/stCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1111111111111111111111
  }
}
EOF

exit_code=0
timeout 30 "$APPLY_PROFILES" -cfg "$CONFIG" > "$LOGFILE" 2>&1 || exit_code=$?

if grep -Eq 'ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|outside the range of representable values' "$LOGFILE"; then
  echo "[FAIL] issue-1230 sanitizer finding reproduced"
  sed -n '1,120p' "$LOGFILE"
  exit 2
fi

if [ "$exit_code" -eq 124 ]; then
  echo "[FAIL] issue-1230 command timed out"
  sed -n '1,120p' "$LOGFILE"
  exit 1
fi

if [ "$exit_code" -eq 0 ]; then
  echo "[FAIL] issue-1230 malformed connect.threads value was accepted"
  sed -n '1,120p' "$LOGFILE"
  exit 1
fi

echo "[PASS] issue-1230 connect.threads failed closed without sanitizer findings (exit=$exit_code)"
