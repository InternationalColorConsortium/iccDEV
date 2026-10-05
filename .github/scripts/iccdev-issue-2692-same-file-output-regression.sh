#!/bin/bash
###############################################################################
# Output that is the same file as an input (#2692)
###############################################################################
#
# Five tools read one path and wrote another without checking that the two were
# different files.  When they were the same file:
#
#   iccApplyProfiles, iccSpecSepToTiff
#       truncated a source TIFF that was still open and unread, then faulted in
#       libtiff on the next line read, leaving the source as an 8-byte header.
#       iccApplyProfiles has more inputs than the image -- each profile, the -cfg
#       document -- and a second output, -exportcfg; all are covered.
#   iccTiffDump, iccJpegDump, iccPngDump
#       replaced the image with the profile they had just extracted from it, and
#       exited 0.
#
# Each tool now refuses before it creates its output, using icIsSameFile(), which
# compares file identity rather than spelling.  So besides the literal same path,
# the dump tools are driven through "./name", a hard link and a symlink -- the
# spellings a string compare misses.
#
# Every refusal case asserts three things: a nonzero exit, the "same file"
# message, and that the input's SHA-256 is unchanged.  On an unfixed build most
# cases fail on the hash, because the input really was truncated or replaced.
# Two do not: iccTiffDump already refused a symlink to its input (its own lstat()
# check, exit 1, no message) and survived a hard link (it writes a temporary file
# and renames it, which leaves the input's inode alone).  Those two fail on the
# exit code or the missing message instead, so they pin the refusal, not damage.
#
# Two guards keep the cases honest:
#   - Preconditions.  Each image fixture must carry an ICC profile, proven by
#     extracting it to a separate path.  A dump tool given an image with no
#     profile writes nothing, which would leave the input intact on an unfixed
#     build and let the case pass for the wrong reason.
#   - Positive controls.  Each tool must still succeed when its output is a
#     different file.  A fix that refused every output would pass all the
#     refusal cases; the controls catch it.
#
# Fixtures are built from tracked files with the tools' own embed and inject
# modes, so no image library is needed:
#   .github/ci/regression/para-gamma-2.4.icc            RGB profile
#   Testing/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif   RGB TIFF
#   examples/.../AppIcon-20.png, Tools/.../CIC33.jpg    PNG and JPEG bases
# The single-channel separations iccSpecSepToTiff reads are written here with the
# python3 standard library; they need no profile, as truncating any open TIFF is
# enough to destroy it.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2692-same-file-output}"
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

find_tool() {
  local name="$1" p
  for p in "$TOOLS_DIR/$2/$name" "$TOOLS_DIR/$2/$name.exe"; do
    if [ -x "$p" ]; then
      printf '%s' "$p"
      return 0
    fi
  done
  return 1
}

APPLY="$(find_tool iccApplyProfiles IccApplyProfiles)" || { echo "[FAIL] iccApplyProfiles not found under $TOOLS_DIR"; exit 1; }
SPECSEP="$(find_tool iccSpecSepToTiff IccSpecSepToTiff)" || { echo "[FAIL] iccSpecSepToTiff not found under $TOOLS_DIR"; exit 1; }
TIFFDUMP="$(find_tool iccTiffDump IccTiffDump)" || { echo "[FAIL] iccTiffDump not found under $TOOLS_DIR"; exit 1; }
JPEGDUMP="$(find_tool iccJpegDump IccJpegDump)" || { echo "[FAIL] iccJpegDump not found under $TOOLS_DIR"; exit 1; }
PNGDUMP="$(find_tool iccPngDump IccPngDump)" || { echo "[FAIL] iccPngDump not found under $TOOLS_DIR"; exit 1; }
if ! command -v python3 >/dev/null 2>&1; then
  echo "[FAIL] python3 is required to write the separation fixtures"
  exit 1
fi

PROF="$REPO_ROOT/.github/ci/regression/para-gamma-2.4.icc"
SEED_TIF="$REPO_ROOT/Testing/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
BASE_PNG="$REPO_ROOT/examples/ios-apply-preview/Assets.xcassets/AppIcon.appiconset/AppIcon-20.png"
BASE_JPG="$REPO_ROOT/Tools/Winnt/IccIisIsapi/assets/CIC33.jpg"
for f in "$PROF" "$SEED_TIF" "$BASE_PNG" "$BASE_JPG"; do
  [ -f "$f" ] || { echo "[FAIL] missing tracked fixture: $f"; exit 1; }
done

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); echo "  [PASS] $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  [FAIL] $1"; }

sha() { python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$1"; }

FIX="$OUTDIR/fixtures"
rm -rf "$FIX"
mkdir -p "$FIX"

echo "=== #2692 output that is the same file as an input ==="

# ---- fixtures --------------------------------------------------------------
timeout 60 "$APPLY" "$SEED_TIF" "$FIX/icc.tif" 0 0 0 1 1 "$PROF" 0 > "$FIX/make-tif.log" 2>&1
timeout 60 "$PNGDUMP" "$BASE_PNG" --write-icc "$PROF" --output "$FIX/icc.png" > "$FIX/make-png.log" 2>&1
timeout 60 "$JPEGDUMP" "$BASE_JPG" --write-icc "$PROF" --output "$FIX/icc.jpg" > "$FIX/make-jpg.log" 2>&1
python3 - "$FIX" <<'PY'
import os, struct, sys
# Baseline uncompressed 8-bit MinIsBlack TIFF, one strip, 16x16.
def gray_tiff(path, level, w=16, h=16):
    pixels = bytes([level]) * (w * h)
    entries = [
        (256, 3, 1, w), (257, 3, 1, h), (258, 3, 1, 8), (259, 3, 1, 1),
        (262, 3, 1, 1), (273, 4, 1, 0), (277, 3, 1, 1), (278, 3, 1, h),
        (279, 4, 1, len(pixels)), (282, 5, 1, 0), (283, 5, 1, 0),
        (284, 3, 1, 1), (296, 3, 1, 2),
    ]
    ifd_off = 8
    ifd_len = 2 + 12 * len(entries) + 4
    res_off = ifd_off + ifd_len
    pix_off = res_off + 16
    out = bytearray(b'II*\x00' + struct.pack('<I', ifd_off))
    out += struct.pack('<H', len(entries))
    for tag, typ, cnt, val in entries:
        if tag == 273: val = pix_off
        if tag in (282, 283): val = res_off + (0 if tag == 282 else 8)
        if typ == 3:
            out += struct.pack('<HHIHH', tag, typ, cnt, val, 0)
        else:
            out += struct.pack('<HHII', tag, typ, cnt, val)
    out += struct.pack('<I', 0)
    out += struct.pack('<II', 72, 1) + struct.pack('<II', 72, 1)
    out += pixels
    open(path, 'wb').write(out)
for n in (1, 2, 3):
    gray_tiff(os.path.join(sys.argv[1], 'sep_%d' % n), 60 * n)
PY

# ---- preconditions: each image really carries a profile --------------------
precondition() {
  local label="$1" tool="$2" image="$3" out="$FIX/pre-$1.icc"
  rm -f "$out"
  timeout 60 "$tool" "$image" "$out" > "$FIX/pre-$1.log" 2>&1
  if [ -s "$out" ]; then
    pass "precondition: $label fixture carries an ICC profile"
  else
    fail "precondition: $label fixture carries an ICC profile (none extracted; refusal cases would be vacuous)"
  fi
}
precondition tiff "$TIFFDUMP" "$FIX/icc.tif"
precondition jpeg "$JPEGDUMP" "$FIX/icc.jpg"
precondition png  "$PNGDUMP"  "$FIX/icc.png"

# ---- refusal cases ---------------------------------------------------------
# refuse <label> <input-to-protect> <how-to-name-the-output> <tool> <args...>
# The args use @IN@ and @OUT@ for the protected input and the output spelling.
refuse() {
  local label="$1" src="$2" spelling="$3"
  shift 3
  local dir="$OUTDIR/case-$label"
  rm -rf "$dir"
  mkdir -p "$dir"
  local base
  base="$(basename "$src")"
  cp "$src" "$dir/$base"
  local out
  case "$spelling" in
    same) out="$base" ;;
    dot)  out="./$base" ;;
    hard) ln "$dir/$base" "$dir/link-$base"; out="link-$base" ;;
    sym)  ln -s "$base" "$dir/sym-$base"; out="sym-$base" ;;
  esac
  local before after rc
  before="$(sha "$dir/$base")"
  local args=() a
  for a in "$@"; do
    a="${a//@IN@/$base}"
    a="${a//@OUT@/$out}"
    args+=("$a")
  done
  ( cd "$dir" && timeout 60 "${args[@]}" ) > "$dir/run.log" 2>&1
  rc=$?
  after="$(sha "$dir/$base")"

  if [ "$rc" -eq 0 ]; then
    fail "$label: exit 0 (expected a refusal)"
  elif [ "$before" != "$after" ]; then
    fail "$label: exit $rc but the input was modified"
  elif ! grep -q "is the same file as input" "$dir/run.log"; then
    fail "$label: exit $rc, input intact, but no 'same file' message"
  else
    pass "$label: refused (exit $rc), input unchanged"
  fi
}

refuse applyprofiles-source "$SEED_TIF" same \
  "$APPLY" @IN@ @OUT@ 0 0 0 0 1 "$PROF" 0
refuse applyprofiles-profile "$PROF" same \
  "$APPLY" "$SEED_TIF" @OUT@ 0 0 0 1 1 @IN@ 0
# A -PCC file is opened for loading tags on demand; an output naming it replaced
# the connection conditions with the image.  The copy under test is a different
# file from the profile applied, so only the PCC can match.
refuse applyprofiles-pcc "$PROF" same \
  "$APPLY" "$SEED_TIF" @OUT@ 0 0 0 0 1 "$PROF" 0 -PCC @IN@
# -exportcfg is a second output of iccApplyProfiles, written before the source is
# even opened, so it is checked against the inputs on its own.
refuse_exportcfg() {
  local dir="$OUTDIR/case-applyprofiles-exportcfg"
  rm -rf "$dir"
  mkdir -p "$dir"
  cp "$SEED_TIF" "$dir/src.tif"
  local before after rc
  before="$(sha "$dir/src.tif")"
  ( cd "$dir" && timeout 60 "$APPLY" -exportcfg src.tif src.tif dst.tif 0 0 0 0 1 "$PROF" 0 ) > "$dir/run.log" 2>&1
  rc=$?
  after="$(sha "$dir/src.tif")"
  if [ "$rc" -eq 0 ]; then
    fail "applyprofiles-exportcfg: exit 0 (expected a refusal)"
  elif [ "$before" != "$after" ]; then
    fail "applyprofiles-exportcfg: exit $rc but the source was modified"
  elif ! grep -q "is the same file as input" "$dir/run.log"; then
    fail "applyprofiles-exportcfg: exit $rc, source intact, but no 'same file' message"
  else
    pass "applyprofiles-exportcfg: refused (exit $rc), source unchanged"
  fi
}

# The -cfg document is an input too: an output naming it overwrote the config.
refuse_cfg_document() {
  local dir="$OUTDIR/case-applyprofiles-cfg"
  rm -rf "$dir"
  mkdir -p "$dir"
  cp "$SEED_TIF" "$dir/src.tif"
  cat > "$dir/run.json" <<EOF
{
  "imageFiles": {
    "srcImageFile": "src.tif",
    "dstImageFile": "run.json",
    "dstEncoding": "8Bit",
    "dstCompression": false,
    "dstPlanar": false,
    "dstEmbedIcc": false
  },
  "profileSequence": [
    {
      "iccFile": "$PROF",
      "intent": 0,
      "interpolation": "tetrahedral"
    }
  ]
}
EOF
  local before after rc
  before="$(sha "$dir/run.json")"
  ( cd "$dir" && timeout 60 "$APPLY" -cfg run.json ) > "$dir/run.log" 2>&1
  rc=$?
  after="$(sha "$dir/run.json")"
  if [ "$rc" -eq 0 ]; then
    fail "applyprofiles-cfg-document: exit 0 (expected a refusal)"
  elif [ "$before" != "$after" ]; then
    fail "applyprofiles-cfg-document: exit $rc but the config was modified"
  elif ! grep -q "is the same file as input" "$dir/run.log"; then
    fail "applyprofiles-cfg-document: exit $rc, config intact, but no 'same file' message"
  else
    pass "applyprofiles-cfg-document: refused (exit $rc), config unchanged"
  fi
}

refuse_exportcfg
refuse_cfg_document

# iccSpecSepToTiff reads every separation from one prefix, so all three have to
# sit beside the output in the case directory; the output then names one of them.
# Checking each position shows the per-channel check covers every generated name.
refuse_specsep() {
  local n="$1" dir="$OUTDIR/case-specsep-channel-$1"
  rm -rf "$dir"
  mkdir -p "$dir"
  cp "$FIX/sep_1" "$FIX/sep_2" "$FIX/sep_3" "$dir/"
  local before after rc
  before="$(sha "$dir/sep_$n")"
  ( cd "$dir" && timeout 60 "$SPECSEP" "sep_$n" 0 0 sep_ 1 3 1 ) > "$dir/run.log" 2>&1
  rc=$?
  after="$(sha "$dir/sep_$n")"
  if [ "$rc" -eq 0 ]; then
    fail "specsep-channel-$n: exit 0 (expected a refusal)"
  elif [ "$before" != "$after" ]; then
    fail "specsep-channel-$n: exit $rc but the separation was modified"
  elif ! grep -q "is the same file as input" "$dir/run.log"; then
    fail "specsep-channel-$n: exit $rc, input intact, but no 'same file' message"
  else
    pass "specsep-channel-$n: refused (exit $rc), separation unchanged"
  fi
}
for n in 1 2 3; do
  refuse_specsep "$n"
done
for spelling in same dot hard sym; do
  refuse "tiffdump-$spelling" "$FIX/icc.tif" "$spelling" "$TIFFDUMP" @IN@ @OUT@
  refuse "jpegdump-$spelling" "$FIX/icc.jpg" "$spelling" "$JPEGDUMP" @IN@ @OUT@
  refuse "pngdump-$spelling"  "$FIX/icc.png" "$spelling" "$PNGDUMP"  @IN@ @OUT@
done
# Injection mode: --write-icc is an INPUT (the profile to embed), so an --output
# naming it must be refused as well as one naming the image.
refuse jpegdump-inject-profile "$PROF" same \
  "$JPEGDUMP" "$FIX/icc.jpg" --write-icc @IN@ --output @OUT@
refuse pngdump-inject-profile "$PROF" same \
  "$PNGDUMP" "$FIX/icc.png" --write-icc @IN@ --output @OUT@

# ---- positive controls: a different output still works ---------------------
control() {
  local label="$1" out="$2"
  shift 2
  rm -f "$out"
  timeout 60 "$@" > "$OUTDIR/control-$label.log" 2>&1
  local rc=$?
  if [ "$rc" -eq 0 ] && [ -s "$out" ]; then
    pass "control: $label writes a different output"
  else
    fail "control: $label writes a different output (exit $rc)"
  fi
}
control applyprofiles "$OUTDIR/ctl-apply.tif" "$APPLY" "$SEED_TIF" "$OUTDIR/ctl-apply.tif" 0 0 0 0 1 "$PROF" 0
control specsep       "$OUTDIR/ctl-specsep.tif" "$SPECSEP" "$OUTDIR/ctl-specsep.tif" 0 0 "$FIX/sep_" 1 3 1
control tiffdump      "$OUTDIR/ctl-tiff.icc" "$TIFFDUMP" "$FIX/icc.tif" "$OUTDIR/ctl-tiff.icc"
control jpegdump      "$OUTDIR/ctl-jpeg.icc" "$JPEGDUMP" "$FIX/icc.jpg" "$OUTDIR/ctl-jpeg.icc"
control pngdump       "$OUTDIR/ctl-png.icc"  "$PNGDUMP"  "$FIX/icc.png" "$OUTDIR/ctl-png.icc"

echo "=== summary: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
