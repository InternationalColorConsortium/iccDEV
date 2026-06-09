#!/bin/bash
###############################################################################
# iccDEV issue #1230 JSON connect.threads regression
###############################################################################

set -euo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:?Set ICCDEV_TOOLS_DIR to iccDEV Build/Tools path}"
TESTING_DIR="${ICCDEV_TESTING_DIR:?Set ICCDEV_TESTING_DIR to iccDEV Testing path}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1230-output}"

APPLY_PROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
PROFILE_PREF="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
PROFILE_D65="$TESTING_DIR/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
SOURCE_IMAGE="$TESTING_DIR/hybrid/Data/TShirtDesignKW.tif"
SEED_TIFF="$TESTING_DIR/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"

mkdir -p "$OUTDIR"

if [ ! -x "$APPLY_PROFILES" ]; then
  echo "[FAIL] iccApplyProfiles not found: $APPLY_PROFILES"
  exit 1
fi

if [ ! -f "$PROFILE_PREF" ]; then
  echo "[FAIL] ICC profile fixture not found: $PROFILE_PREF"
  exit 1
fi

if [ ! -f "$PROFILE_D65" ]; then
  echo "[FAIL] ICC profile fixture not found: $PROFILE_D65"
  exit 1
fi

if [ ! -f "$SOURCE_IMAGE" ]; then
  echo "[FAIL] TIFF fixture not found: $SOURCE_IMAGE"
  exit 1
fi

if [ ! -f "$SEED_TIFF" ]; then
  echo "[FAIL] TIFF fixture not found: $SEED_TIFF"
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
INTENT_CONFIG="$TMPDIR/issue-1230-rendering-intent.json"
BOOL_CONFIG="$TMPDIR/issue-1230-image-bool.json"
OUTPUT_IMAGE="$TMPDIR/issue-1230-output.tif"
INTENT_OUTPUT_IMAGE="$TMPDIR/issue-1230-intent-output.tif"
BOOL_OUTPUT_IMAGE="$TMPDIR/issue-1230-bool-output.tif"

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
      "iccFile": "$PROFILE_PREF",
      "intent": 1,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1111111111111111111111
  }
}
EOF

cat > "$INTENT_CONFIG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$SOURCE_IMAGE",
    "dstImageFile": "$INTENT_OUTPUT_IMAGE",
    "dstEncoding": "8Bit",
    "/stCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE_PREF",
      "intent": 2147483648,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1
  }
}
EOF

cat > "$BOOL_CONFIG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$SOURCE_IMAGE",
    "dstImageFile": "$BOOL_OUTPUT_IMAGE",
    "dstEncoding": "8Bit",
    "/stCompression": false,
    "dstPlanar": 4294967296,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE_PREF",
      "intent": 1,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1
  }
}
EOF

CASE_DIR="$TMPDIR/local-research-shape"
mkdir -p "$CASE_DIR/test-profiles"
cp "$SEED_TIFF" "$CASE_DIR/seed-tiff-none-rgb-8x8.tif"
cp "$PROFILE_D65" "$CASE_DIR/test-profiles/sRGB_D65_MAT.icc"

LOCAL_CONFIG="$CASE_DIR/ub-runtime-error-outside-range-IccJsonUtil_cpp-Line274.json"
LOCAL_INTENT_CONFIG="$CASE_DIR/ub-rendering-intent-wrap-IccCmmConfig_cpp-Line732.json"
LOCAL_BOOL_CONFIG="$CASE_DIR/ub-image-bool-wrap-IccJsonUtil_cpp-Line356.json"
cat > "$LOCAL_CONFIG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "seed-tiff-none-rgb-8x8.tif",
    "dstImageFile": "foo.bar",
    "dstEncoding": "8Bit",
    "/stCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "test-profiles/sRGB_D65_MAT.icc",
      "intent": 1,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1111111111111111111111
  }
}
EOF

cat > "$LOCAL_INTENT_CONFIG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "seed-tiff-none-rgb-8x8.tif",
    "dstImageFile": "foo-intent.bar",
    "dstEncoding": "8Bit",
    "/stCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "test-profiles/sRGB_D65_MAT.icc",
      "intent": 4294967295,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1
  }
}
EOF

cat > "$LOCAL_BOOL_CONFIG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "seed-tiff-none-rgb-8x8.tif",
    "dstImageFile": "foo-bool.bar",
    "dstEncoding": "8Bit",
    "/stCompression": false,
    "dstPlanar": 4294967296,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "test-profiles/sRGB_D65_MAT.icc",
      "intent": 1,
      "interpolation": "tetrahedral"
    }
  ],
  "connect": {
    "threads": 1
  }
}
EOF

run_case() {
  local name="$1"
  local workdir="$2"
  local config="$3"
  local logfile="$OUTDIR/$name.log"
  local exit_code=0

  (cd "$workdir" && timeout 30 "$APPLY_PROFILES" -cfg "$config") > "$logfile" 2>&1 || exit_code=$?

  if grep -Eq 'ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|outside the range of representable values' "$logfile"; then
    echo "[FAIL] issue-1230 JSON numeric sanitizer finding reproduced in $name"
    sed -n '1,120p' "$logfile"
    exit 2
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "[FAIL] issue-1230 command timed out in $name"
    sed -n '1,120p' "$logfile"
    exit 1
  fi

  if [ "$exit_code" -eq 0 ]; then
    echo "[FAIL] issue-1230 malformed JSON numeric value was accepted in $name"
    sed -n '1,120p' "$logfile"
    exit 1
  fi

  echo "[PASS] issue-1230 JSON numeric value failed closed in $name without sanitizer findings (exit=$exit_code)"
}

run_case generated-absolute "$TMPDIR" "$CONFIG"
run_case generated-rendering-intent "$TMPDIR" "$INTENT_CONFIG"
run_case generated-image-bool "$TMPDIR" "$BOOL_CONFIG"
run_case local-research-shape "$CASE_DIR" "$(basename "$LOCAL_CONFIG")"
run_case local-rendering-intent "$CASE_DIR" "$(basename "$LOCAL_INTENT_CONFIG")"
run_case local-image-bool "$CASE_DIR" "$(basename "$LOCAL_BOOL_CONFIG")"
