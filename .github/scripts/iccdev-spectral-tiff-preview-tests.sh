#!/bin/bash
###############################################################################
# Spectral TIFF sidecar preview regression.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-spectral-tiff-preview}"
PYTHON="${ICCDEV_SPECTRAL_PREVIEW_PYTHON:-python3}"
PREVIEW="$SCRIPT_DIR/spectral-tiff-preview.py"
SOURCE_TIFF="$ICCDEV_TESTING/hybrid/Data/smCows380_5_780.tif"
OUTPUT_PNG="$OUTDIR/smCows380_5_780-preview.png"
OUTPUT_TIFFCP_PNG="$OUTDIR/smCows380_5_780-preview-tiffcp.png"
OUTPUT_BANDS="$OUTDIR/bands"
OUTPUT_CONTACT="$OUTDIR/smCows380_5_780-all-bands.png"
OUTPUT_METADATA="$OUTDIR/smCows380_5_780-metadata.json"
OUTPUT_CHANNELS_PNG="$OUTDIR/smCows400_10_700-channels-preview.png"
OUTPUT_CHANNELS_METADATA="$OUTDIR/smCows400_10_700-channels-metadata.json"
OUTPUT_REDUCED_PNG="$OUTDIR/smCows380_8_780_51ch-preview.png"
OUTPUT_REDUCED_TIFF="$OUTDIR/smCows380_8_780_51ch.tif"
OUTPUT_REDUCED_METADATA="$OUTDIR/smCows380_8_780_51ch-metadata.json"
OUTPUT_REDUCED_PROFILE="$OUTDIR/Spec380_8_780-D50_2deg.icc"
OUTPUT_REDUCED_PROFILE_XML="$OUTDIR/Spec380_8_780-D50_2deg.xml"

mkdir -p "$OUTDIR"

if [ ! -x "$PREVIEW" ]; then
  echo "[FAIL] missing executable: $PREVIEW"
  exit 1
fi

if [ ! -f "$SOURCE_TIFF" ]; then
  echo "[FAIL] missing source TIFF: $SOURCE_TIFF"
  exit 1
fi

if ! command -v "$PYTHON" >/dev/null 2>&1; then
  echo "[FAIL] missing python interpreter: $PYTHON"
  exit 1
fi

echo "=== spectral TIFF preview regression ==="

if command -v identify >/dev/null 2>&1; then
  if identify "$SOURCE_TIFF" >/dev/null 2>&1; then
    echo "[INFO] ImageMagick can identify source spectral TIFF directly"
  else
    echo "[INFO] ImageMagick cannot identify source spectral TIFF directly"
  fi
fi

"$PYTHON" - "$SOURCE_TIFF" <<'PY'
import sys

import imagecodecs
import tifffile

path = sys.argv[1]
with tifffile.TiffFile(path) as tiff:
    page = tiff.pages[0]
    if page.shape != (420, 600, 81):
        raise SystemExit(f"unexpected shape metadata: {page.shape}")
    if page.axes != "YXS":
        raise SystemExit(f"unexpected axes: {page.axes}")
    if int(page.samplesperpixel) != 81:
        raise SystemExit(f"unexpected SamplesPerPixel: {page.samplesperpixel}")
    if page.compression.name != "LZW":
        raise SystemExit(f"unexpected compression: {page.compression.name}")
    array = page.asarray()

if array.shape != (420, 600, 81):
    raise SystemExit(f"unexpected decoded shape: {array.shape}")
if str(array.dtype) != "uint16":
    raise SystemExit(f"unexpected dtype: {array.dtype}")
if int(array.min()) != 0 or int(array.max()) != 65519:
    raise SystemExit(f"unexpected range: {int(array.min())}..{int(array.max())}")

print(f"[PASS] direct LZW decode via {imagecodecs.__version__}")
PY

"$PYTHON" "$PREVIEW" "$SOURCE_TIFF" "$OUTPUT_PNG"
TIFFCP_PREVIEW=""
if command -v tiffcp >/dev/null 2>&1; then
  "$PYTHON" "$PREVIEW" --force-tiffcp "$SOURCE_TIFF" "$OUTPUT_TIFFCP_PNG"
  TIFFCP_PREVIEW="$OUTPUT_TIFFCP_PNG"
  echo "[PASS] forced libtiff tiffcp preview verified"
else
  echo "[SKIP] forced libtiff tiffcp preview; tiffcp not available"
fi
"$PYTHON" "$PREVIEW" \
  --export-bands "$OUTPUT_BANDS" \
  --contact-sheet "$OUTPUT_CONTACT" \
  --metadata "$OUTPUT_METADATA" \
  "$SOURCE_TIFF" "$OUTPUT_PNG"

"$PYTHON" "$PREVIEW" \
  --channels 4:65:2 \
  --metadata "$OUTPUT_CHANNELS_METADATA" \
  "$SOURCE_TIFF" "$OUTPUT_CHANNELS_PNG"

profile_args=()
if command -v iccFromXml >/dev/null 2>&1; then
  profile_args=(
    --converted-profile-xml-template "$ICCDEV_TESTING/hybrid/Data/Spec380_10_730-D50_2deg.xml"
    --generated-icc-profile "$OUTPUT_REDUCED_PROFILE"
    --generated-profile-xml "$OUTPUT_REDUCED_PROFILE_XML"
  )
else
  echo "[SKIP] reduced spectral TIFF ICC embedding; iccFromXml not available"
fi

"$PYTHON" "$PREVIEW" \
  --channel-count 51 \
  --converted-tiff "$OUTPUT_REDUCED_TIFF" \
  "${profile_args[@]}" \
  --metadata "$OUTPUT_REDUCED_METADATA" \
  "$SOURCE_TIFF" "$OUTPUT_REDUCED_PNG"

"$PYTHON" - \
  "$OUTPUT_PNG" \
  "$TIFFCP_PREVIEW" \
  "$OUTPUT_BANDS" \
  "$OUTPUT_CONTACT" \
  "$OUTPUT_METADATA" \
  "$OUTPUT_CHANNELS_PNG" \
  "$OUTPUT_CHANNELS_METADATA" \
  "$OUTPUT_REDUCED_PNG" \
  "$OUTPUT_REDUCED_TIFF" \
  "$OUTPUT_REDUCED_METADATA" \
  "$OUTPUT_REDUCED_PROFILE" <<'PY'
import json
import sys
from pathlib import Path

import tifffile
from PIL import Image

preview_paths = [Path(sys.argv[1])]
if sys.argv[2]:
    preview_paths.append(Path(sys.argv[2]))
band_dir = Path(sys.argv[3])
contact_path = Path(sys.argv[4])
metadata_path = Path(sys.argv[5])
channels_preview_path = Path(sys.argv[6])
channels_metadata_path = Path(sys.argv[7])
reduced_preview_path = Path(sys.argv[8])
reduced_tiff_path = Path(sys.argv[9])
reduced_metadata_path = Path(sys.argv[10])
reduced_profile_path = Path(sys.argv[11])

for path in preview_paths:
    with Image.open(path) as image:
        if image.mode != "RGB":
            raise SystemExit(f"unexpected mode for {path}: {image.mode}")
        if image.size != (600, 420):
            raise SystemExit(f"unexpected size for {path}: {image.size}")
        if image.getextrema() != ((0, 255), (0, 255), (0, 255)):
            raise SystemExit(f"unexpected extrema for {path}: {image.getextrema()}")

bands = sorted(band_dir.glob("band_*.png"))
if len(bands) != 81:
    raise SystemExit(f"expected 81 band previews, found {len(bands)}")
if bands[0].name != "band_000_0380nm.png" or bands[-1].name != "band_080_0780nm.png":
    raise SystemExit(f"unexpected band name range: {bands[0].name}..{bands[-1].name}")
for path in (bands[0], bands[40], bands[-1]):
    with Image.open(path) as image:
        if image.mode != "L":
            raise SystemExit(f"unexpected band mode for {path}: {image.mode}")
        if image.size != (600, 420):
            raise SystemExit(f"unexpected band size for {path}: {image.size}")

with Image.open(contact_path) as image:
    if image.mode != "RGB":
        raise SystemExit(f"unexpected contact sheet mode: {image.mode}")
    if image.size[0] <= 600 or image.size[1] <= 420:
        raise SystemExit(f"unexpected contact sheet size: {image.size}")

if not metadata_path.is_file():
    raise SystemExit(f"missing metadata: {metadata_path}")

for path in (channels_preview_path, reduced_preview_path):
    with Image.open(path) as image:
        if image.mode != "RGB":
            raise SystemExit(f"unexpected reduced preview mode for {path}: {image.mode}")
        if image.size != (600, 420):
            raise SystemExit(f"unexpected reduced preview size for {path}: {image.size}")

with channels_metadata_path.open("r", encoding="ascii") as handle:
    channels_metadata = json.load(handle)
if channels_metadata["array_shape"] != [420, 600, 31]:
    raise SystemExit(f"unexpected channel-selected shape: {channels_metadata['array_shape']}")
if channels_metadata["selected_source_indexes"][0] != 4:
    raise SystemExit("channel selection did not start at source channel 4")
if channels_metadata["selected_source_indexes"][-1] != 64:
    raise SystemExit("channel selection did not end at source channel 64")

with reduced_metadata_path.open("r", encoding="ascii") as handle:
    reduced_metadata = json.load(handle)
if reduced_metadata["array_shape"] != [420, 600, 51]:
    raise SystemExit(f"unexpected reduced shape: {reduced_metadata['array_shape']}")
if not reduced_metadata["resampled"]:
    raise SystemExit("reduced TIFF should be resampled")
if reduced_metadata["selected_wavelengths_nm"][0] != 380.0:
    raise SystemExit("reduced wavelength selection did not start at 380nm")
if reduced_metadata["selected_wavelengths_nm"][-1] != 780.0:
    raise SystemExit("reduced wavelength selection did not end at 780nm")
if reduced_metadata["converted_tiff"]["shape"] != [420, 600, 51]:
    raise SystemExit("converted TIFF metadata shape mismatch")

with tifffile.TiffFile(reduced_tiff_path) as tiff:
    page = tiff.pages[0]
    if page.shape != (420, 600, 51):
        raise SystemExit(f"unexpected reduced TIFF shape: {page.shape}")
    if int(page.samplesperpixel) != 51:
        raise SystemExit(f"unexpected reduced TIFF SamplesPerPixel: {page.samplesperpixel}")
    if page.dtype.name != "uint16":
        raise SystemExit(f"unexpected reduced TIFF dtype: {page.dtype}")
    if reduced_profile_path.is_file() and 34675 not in page.tags:
        raise SystemExit("missing embedded ICC profile in reduced TIFF")

print("[PASS] RGB previews, reduced TIFF, band sidecars, contact sheet, and metadata verified")
PY

if command -v iccTiffDump >/dev/null 2>&1; then
  REDUCED_DUMP="$OUTDIR/smCows380_8_780_51ch-dump.txt"
  iccTiffDump "$OUTPUT_REDUCED_TIFF" >"$REDUCED_DUMP"
  grep -q "SamplesPerPixel:   51" "$REDUCED_DUMP"
  if [ -f "$OUTPUT_REDUCED_PROFILE" ]; then
    grep -q "Spectral Range:   start=380.0nm, end=780.0nm, steps=51" \
      "$REDUCED_DUMP"
  fi
  echo "[PASS] reduced spectral TIFF validated with iccTiffDump"
else
  echo "[SKIP] reduced spectral TIFF iccTiffDump validation; tool not available"
fi

if command -v identify >/dev/null 2>&1; then
  identify "$OUTPUT_PNG"
  if [ -n "$TIFFCP_PREVIEW" ]; then
    identify "$TIFFCP_PREVIEW"
  fi
  identify "$OUTPUT_CONTACT"
  identify "$OUTPUT_BANDS/band_000_0380nm.png"
  identify "$OUTPUT_BANDS/band_040_0580nm.png"
  identify "$OUTPUT_BANDS/band_080_0780nm.png"
  identify "$OUTPUT_CHANNELS_PNG"
  identify "$OUTPUT_REDUCED_PNG"
fi

echo "[PASS] spectral TIFF preview regression complete"
