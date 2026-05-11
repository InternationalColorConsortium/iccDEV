#!/bin/bash
###############################################################################
# iccDEV iccApplyProfiles threading equivalence regression tests
###############################################################################
#
# Verifies that iccApplyProfiles produces bit-identical TIFF output under
# every non-negative -threads mode supported by CIccConnectCmm::CreateStandard():
#   -threads  1   single-threaded (no CIccThreadedCmm wrapper, per-pixel apply)
#   -threads  0   wrapper with std::thread::hardware_concurrency() workers
#   -threads  N   wrapper with N workers (default N=4 here)
#
# Catches partitioning regressions in CIccApplyThreadedCmm::Apply()'s strip
# split / async-launch / last-strip-on-caller logic, and any future ownership
# regression in CIccConnectCmm threaded initialization.
#
# Input: a deterministic 512x512 sRGB TIFF generated on the fly so this test
# does not depend on a checked-in image with a specific colour space. Python 3
# with Pillow is required for synthesis; the test SKIPs cleanly if either is
# unavailable.
#
# Environment variables (set by CI workflow):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/ (contains tool subdirs)
#   ICCDEV_TESTING_DIR -- path to Testing/ (contains sRGB_v4_ICC_preference.icc)
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit code: 0 = all pass, 1 = test failure, 2 = ASAN/UBSAN finding
###############################################################################

set -uo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:?Set ICCDEV_TOOLS_DIR to iccDEV Build/Tools path}"
TESTING_DIR="${ICCDEV_TESTING_DIR:?Set ICCDEV_TESTING_DIR to iccDEV Testing path}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-iccapplyprofiles-threading}"
mkdir -p "$OUTDIR"

APPLY_PROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
SKIP=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=0

echo "=== iccApplyProfiles -threads equivalence ==="

if [ ! -x "$APPLY_PROFILES" ]; then
  echo "  [SKIP] iccApplyProfiles not found at $APPLY_PROFILES"
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

# Synthesize a deterministic 512x512 sRGB TIFF with the sRGB profile embedded.
# 512x512 = 262144 pixels: comfortably larger than std::thread::hardware_concurrency()
# so the threaded apply path actually engages every worker on typical hosts.
SRC_TIFF="$OUTDIR/threading-equiv-src.tif"
PY_LOG="$OUTDIR/threading-equiv-pyinput.log"
if ! command -v python3 >/dev/null 2>&1; then
  echo "  [SKIP] python3 not available -- cannot synthesize deterministic source TIFF"
  exit 0
fi

python3 - "$SRC_TIFF" "$SRGB_PROFILE" >"$PY_LOG" 2>&1 <<'PY'
import sys
from PIL import Image

out_path, icc_path = sys.argv[1], sys.argv[2]
with open(icc_path, "rb") as f:
    icc_bytes = f.read()

w, h = 512, 512
# Deterministic gradient: R = x, G = y, B = (x+y)/2 -- full 8-bit range, no compression.
data = bytearray(w * h * 3)
for y in range(h):
    base = y * w * 3
    for x in range(w):
        i = base + x * 3
        data[i]     = x & 0xFF
        data[i + 1] = y & 0xFF
        data[i + 2] = ((x + y) >> 1) & 0xFF

img = Image.frombytes("RGB", (w, h), bytes(data))
img.save(out_path, format="TIFF", compression="raw", icc_profile=icc_bytes)
print(f"wrote {out_path} ({w}x{h})")
PY
PY_EC=$?
if [ "$PY_EC" -ne 0 ] || [ ! -s "$SRC_TIFF" ]; then
  echo "  [SKIP] Pillow/PIL unavailable or input generation failed -- see $PY_LOG"
  sed -n '1,10p' "$PY_LOG"
  exit 0
fi

# Build a -cfg JSON pointing at the synthetic input. The profile sequence uses
# the embedded source profile (empty iccFile + iccFile entries) followed by an
# explicit sRGB destination; this exercises CIccConnectCmm::CreateStandard's
# embedded-profile branch alongside a normal AddXform call.
CFG_JSON="$OUTDIR/threading-equiv.cfg.json"
cat > "$CFG_JSON" <<EOF
{
  "imageFiles": {
    "srcImageFile": "$SRC_TIFF",
    "dstImageFile": "__DST__",
    "dstEncoding": "16Bit",
    "dstCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    { "iccFile": "",              "intent": 1, "interpolation": "tetrahedral" },
    { "iccFile": "$SRGB_PROFILE", "intent": 1, "interpolation": "tetrahedral" }
  ]
}
EOF

# run_threads N out_label -> writes $OUTDIR/threading-equiv-<label>.tif
run_threads() {
  local nthreads="$1"
  local label="$2"
  local cfg="$OUTDIR/threading-equiv-${label}.cfg.json"
  local dst="$OUTDIR/threading-equiv-${label}.tif"
  local log="$OUTDIR/threading-equiv-${label}.log"

  sed "s|__DST__|$dst|" "$CFG_JSON" > "$cfg"

  rm -f "$dst"
  local ec=0
  timeout 120 "$APPLY_PROFILES" -threads "$nthreads" -cfg "$cfg" > "$log" 2>&1 || ec=$?

  if grep -q "ERROR: AddressSanitizer" "$log" 2>/dev/null; then
    echo "  [ASAN] -threads $nthreads -- AddressSanitizer finding"
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    return 1
  fi
  if grep -q "runtime error:" "$log" 2>/dev/null; then
    echo "  [UBSAN] -threads $nthreads -- undefined behavior"
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    return 1
  fi
  if [ "$ec" -ne 0 ] || [ ! -s "$dst" ]; then
    echo "  [FAIL] -threads $nthreads exit=$ec (no output)"
    sed -n '1,20p' "$log"
    return 1
  fi
  echo "$dst"
  return 0
}

REF=""
for entry in "1:t1" "0:auto" "4:t4"; do
  TOTAL=$((TOTAL + 1))
  nthreads="${entry%%:*}"
  label="${entry##*:}"

  out="$(run_threads "$nthreads" "$label")" || { FAIL=$((FAIL + 1)); continue; }

  if [ -z "$REF" ]; then
    REF="$out"
    echo "  [PASS] -threads $nthreads (reference output: $(basename "$REF"))"
    PASS=$((PASS + 1))
    continue
  fi

  if cmp -s "$REF" "$out"; then
    echo "  [PASS] -threads $nthreads matches $(basename "$REF")"
    PASS=$((PASS + 1))
  else
    echo "  [FAIL] -threads $nthreads diverges from $(basename "$REF")"
    # Help diagnose: report first differing byte offset.
    cmp "$REF" "$out" 2>&1 | head -1 || true
    FAIL=$((FAIL + 1))
  fi
done

echo "iccApplyProfiles threading equivalence: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total"

if [ "$ASAN_FINDINGS" -ne 0 ] || [ "$UBSAN_FINDINGS" -ne 0 ]; then
  exit 2
fi
if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
exit 0
