#!/bin/bash
###############################################################################
# iccDEV iccApplyProfiles Lab-roundtrip regression tests
###############################################################################
#
# Exercises the TIFF-boundary encoding rule for chains that read from and write
# to Lab-destination TIFFs via a v5 MPE-based PCC (Profile Connection Conditions)
# profile that maps standard CIELAB to/from a normalized device-Lab integer
# encoding. The chain pattern (sRGB -> encoded Lab TIFF -> sRGB) was previously
# broken because the TIFF boundary applied PCS-encoding conversions intended
# for v4 chains to values that were already in device-encoded [0,1] form,
# producing saturated 16-bit pixels and 100x-scaled float pixels.
#
# This test guards against future regression of that fix by:
#   1) Synthesizing a deterministic RGB TIFF with sRGB v4 (Lab PCS) embedded.
#   2) Building a v5 MPE Lab integer-encoding PCC profile from inline XML.
#   3) Running a forward apply (RGB -> encoded Lab 16-bit) and a reverse apply
#      (encoded Lab 16-bit -> RGB) using the same v5 profile.
#   4) Asserting the RGB round-trip RMS error stays within a generous bound
#      that any chromaticity-saturation regression would blow through.
#
# Environment variables (set by CI workflow):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools (contains tool subdirs)
#   ICCDEV_TESTING_DIR -- path to Testing/ (contains sRGB_v4_ICC_preference.icc)
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# The test SKIPs cleanly when python3, Pillow, tifffile + imagecodecs, or
# iccFromXml are unavailable, so it does not false-fail in minimal images.
#
# Exit code: 0 = all pass (or skipped), 1 = test failure, 2 = ASAN/UBSAN.
###############################################################################

set -uo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:?Set ICCDEV_TOOLS_DIR to iccDEV Build/Tools path}"
TESTING_DIR="${ICCDEV_TESTING_DIR:?Set ICCDEV_TESTING_DIR to iccDEV Testing path}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-iccapplyprofiles-lab-roundtrip}"
mkdir -p "$OUTDIR"

APPLY="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=0

echo "=== iccApplyProfiles Lab-roundtrip ==="

if [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyProfiles not found at $APPLY"
  exit 0
fi
if [ ! -x "$FROMXML" ]; then
  echo "  [SKIP] iccFromXml not found at $FROMXML -- cannot build test profile"
  exit 0
fi

SRGB_PROFILE=""
for candidate in \
  "$TESTING_DIR/sRGB_v4_ICC_preference.icc" \
  "$TESTING_DIR/../Testing/sRGB_v4_ICC_preference.icc"; do
  if [ -f "$candidate" ]; then
    SRGB_PROFILE="$candidate"
    break
  fi
done
if [ -z "$SRGB_PROFILE" ]; then
  echo "  [SKIP] sRGB_v4_ICC_preference.icc not found under $TESTING_DIR"
  exit 0
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "  [SKIP] python3 not available -- cannot synthesize/verify TIFFs"
  exit 0
fi

# Synthesize a deterministic 64x64 RGB TIFF with sRGB v4 (Lab PCS) embedded.
SRC_TIFF="$OUTDIR/lab-rt-src.tif"
PY_LOG="$OUTDIR/lab-rt-synth.log"
python3 - "$SRC_TIFF" "$SRGB_PROFILE" >"$PY_LOG" 2>&1 <<'PY'
import sys
try:
    from PIL import Image
except Exception as e:
    print(f"Pillow unavailable: {e}")
    sys.exit(2)

out_path, icc_path = sys.argv[1], sys.argv[2]
with open(icc_path, "rb") as f:
    icc_bytes = f.read()

w, h = 64, 64
data = bytearray(w * h * 3)
for y in range(h):
    base = y * w * 3
    for x in range(w):
        i = base + x * 3
        data[i]     = (x * 4) & 0xFF
        data[i + 1] = (y * 4) & 0xFF
        data[i + 2] = ((x + y) * 2) & 0xFF

img = Image.frombytes("RGB", (w, h), bytes(data))
img.save(out_path, format="TIFF", compression="raw", icc_profile=icc_bytes)
print(f"wrote {out_path} ({w}x{h})")
PY
PY_EC=$?
if [ "$PY_EC" -ne 0 ] || [ ! -s "$SRC_TIFF" ]; then
  echo "  [SKIP] Pillow unavailable or source TIFF synthesis failed -- see $PY_LOG"
  sed -n '1,5p' "$PY_LOG"
  exit 0
fi

# Build the v5 MPE PCC Lab integer-encoding profile from inline XML.  The
# A2B3 matrix decodes device-encoded [0,1] Lab to standard CIELAB
# (L = L_dev*100, a/b = ab_dev*256-128), and B2A3 encodes the inverse.
LAB_XML="$OUTDIR/lab-rt-Lab_int_D50.xml"
LAB_ICC="$OUTDIR/lab-rt-Lab_int_D50.icc"
cat > "$LAB_XML" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <PreferredCMMType></PreferredCMMType>
    <ProfileVersion>5.00</ProfileVersion>
    <ProfileDeviceClass>spac</ProfileDeviceClass>
    <DataColourSpace>Lab </DataColourSpace>
    <PCS>Lab </PCS>
    <CreationDateTime>now</CreationDateTime>
    <ProfileFlags EmbeddedInFile="true" UseWithEmbeddedDataOnly="false"/>
    <DeviceAttributes ReflectiveOrTransparency="reflective" GlossyOrMatte="glossy" MediaPolarity="positive" MediaColour="colour"/>
    <RenderingIntent>Absolute Colorimetric</RenderingIntent>
    <PCSIlluminant>
      <XYZNumber X="0.964245565128246" Y="1.00000000" Z="0.824679094091967"/>
    </PCSIlluminant>
    <ProfileCreator></ProfileCreator>
    <ProfileID>1</ProfileID>
  </Header>
  <Tags>
    <multiLocalizedUnicodeType>
      <TagSignature>desc</TagSignature>
      <LocalizedText LanguageCountry="enUS"><![CDATA[Test Lab integer encoding D50 2deg observer]]></LocalizedText>
    </multiLocalizedUnicodeType>
    <multiProcessElementType>
      <TagSignature>A2B3</TagSignature>
      <MultiProcessElements InputChannels="3" OutputChannels="3">
        <MatrixElement InputChannels="3" OutputChannels="3">
          <MatrixData>
            100.0 0 0
            0 256.0 0
            0 0 256.0
          </MatrixData>
          <OffsetData>0 -128.0 -128.0</OffsetData>
        </MatrixElement>
      </MultiProcessElements>
    </multiProcessElementType>
    <multiProcessElementType>
      <TagSignature>B2A3</TagSignature>
      <MultiProcessElements InputChannels="3" OutputChannels="3">
        <MatrixElement InputChannels="3" OutputChannels="3">
          <MatrixData>
            0.01 0 0
            0 0.0039062 0
            0 0 0.0039062
          </MatrixData>
          <OffsetData>0 0.5 0.5</OffsetData>
        </MatrixElement>
      </MultiProcessElements>
    </multiProcessElementType>
  </Tags>
</IccProfile>
XML

XML_LOG="$OUTDIR/lab-rt-fromxml.log"
if ! "$FROMXML" "$LAB_XML" "$LAB_ICC" >"$XML_LOG" 2>&1; then
  echo "  [SKIP] iccFromXml failed to build Lab encoding profile"
  sed -n '1,10p' "$XML_LOG"
  exit 0
fi
if [ ! -s "$LAB_ICC" ]; then
  echo "  [SKIP] iccFromXml produced no output"
  exit 0
fi

# Forward: embedded sRGB v4 -> encoded Lab 16-bit TIFF.
FWD_CFG="$OUTDIR/lab-rt-fwd.cfg.json"
ENC_TIFF="$OUTDIR/lab-rt-encoded.tif"
cat > "$FWD_CFG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$SRC_TIFF",
    "dstImageFile": "$ENC_TIFF",
    "dstEncoding": "16bit",
    "dstCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": true
  },
  "profileSequence": [
    { "iccFile": null,       "intent": "relative", "transform": "default" },
    { "iccFile": "$LAB_ICC", "intent": "relative", "transform": "default" }
  ]
}
EOF

# Reverse: encoded Lab 16-bit TIFF (embedded profile) -> sRGB 8-bit RGB.
REV_CFG="$OUTDIR/lab-rt-rev.cfg.json"
RT_TIFF="$OUTDIR/lab-rt-roundtrip.tif"
cat > "$REV_CFG" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$ENC_TIFF",
    "dstImageFile": "$RT_TIFF",
    "dstEncoding": "8bit",
    "dstCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    { "iccFile": null,           "intent": "relative", "transform": "default" },
    { "iccFile": "$SRGB_PROFILE", "intent": "relative", "transform": "default" }
  ]
}
EOF

run_step() {
  local label="$1"; local cfg="$2"
  local log="$OUTDIR/lab-rt-${label}.log"
  local ec=0
  timeout 60 "$APPLY" -cfg "$cfg" >"$log" 2>&1 || ec=$?
  if grep -q "ERROR: AddressSanitizer" "$log" 2>/dev/null; then
    echo "  [ASAN] $label -- AddressSanitizer finding"
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    return 1
  fi
  if grep -q "runtime error:" "$log" 2>/dev/null; then
    echo "  [UBSAN] $label -- undefined behavior"
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    return 1
  fi
  if [ "$ec" -ne 0 ]; then
    echo "  [FAIL] $label exit=$ec"
    sed -n '1,10p' "$log"
    return 1
  fi
  return 0
}

TOTAL=$((TOTAL + 1))
if ! run_step "forward" "$FWD_CFG"; then
  FAIL=$((FAIL + 1))
else
  TOTAL=$((TOTAL + 1))
  if ! run_step "reverse" "$REV_CFG"; then
    FAIL=$((FAIL + 1))
  else
    # Compare round-trip RGB to source RGB.
    TOTAL=$((TOTAL + 1))
    CMP_LOG="$OUTDIR/lab-rt-compare.log"
    python3 - "$SRC_TIFF" "$RT_TIFF" >"$CMP_LOG" 2>&1 <<'PY'
import sys
try:
    import tifffile
    import numpy as np
except Exception as e:
    print(f"SKIP: tifffile/numpy unavailable ({e})")
    sys.exit(77)  # treat as skip

src = np.asarray(tifffile.imread(sys.argv[1]), dtype=np.float32)[..., :3]
rt  = np.asarray(tifffile.imread(sys.argv[2]), dtype=np.float32)[..., :3]
if src.shape != rt.shape:
    print(f"FAIL: shape mismatch src={src.shape} rt={rt.shape}")
    sys.exit(1)
rms = float(np.sqrt(((src - rt) ** 2).mean()))
mx  = float(np.abs(src - rt).max())
print(f"rms={rms:.2f}  max_diff={mx:.0f}")

# Threshold: the fix gives RMS ~9 / max ~30 on this chain. The pre-fix bug
# saturated chromatic channels to 0 or 65535 producing RMS > 100 and max
# diff = 255. Set threshold to 35 RMS / 80 max to catch any regression that
# reintroduces gross encoding errors while tolerating colorimetric loss.
if rms > 35.0 or mx > 80.0:
    print("FAIL: RGB round-trip exceeded threshold (rms<=35, max<=80)")
    sys.exit(1)

print("PASS: RGB round-trip within threshold")
sys.exit(0)
PY
    cmp_ec=$?
    if [ "$cmp_ec" -eq 77 ]; then
      echo "  [SKIP] tifffile/numpy unavailable for comparison"
      sed -n '1,3p' "$CMP_LOG"
    elif [ "$cmp_ec" -eq 0 ]; then
      echo "  [PASS] sRGB -> encoded Lab 16-bit -> sRGB ($(sed -n '1p' "$CMP_LOG"))"
      PASS=$((PASS + 1))
    else
      echo "  [FAIL] sRGB -> encoded Lab 16-bit -> sRGB"
      sed -n '1,5p' "$CMP_LOG"
      FAIL=$((FAIL + 1))
    fi
  fi
fi

echo "iccApplyProfiles Lab-roundtrip: $PASS passed, $FAIL failed, $TOTAL steps total"

if [ "$ASAN_FINDINGS" -ne 0 ] || [ "$UBSAN_FINDINGS" -ne 0 ]; then
  exit 2
fi
if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
exit 0
