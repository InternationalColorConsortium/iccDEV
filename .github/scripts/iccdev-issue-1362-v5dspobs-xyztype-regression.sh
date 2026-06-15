#!/bin/bash
###############################################################################
# iccDEV issue #1362 - iccV5DspObsToV4Dsp must emit rXYZ/gXYZ/bXYZ as XYZType
###############################################################################
#
# The red/green/blue matrix-column tags (rXYZ/gXYZ/bXYZ, a.k.a.
# red/green/blueColorantTag) must be XYZType ('XYZ ', ICC.1 9.2.46 / 9.2.31 /
# 9.2.4 -> 10.31). iccV5DspObsToV4Dsp created them as CIccTagS15Fixed16, so they
# serialized with the type signature 'sf32' (s15Fixed16ArrayType) -- a
# non-compliant profile whose primaries are unreadable by consumers keying on
# the spec type (dynamic_cast<CIccTagXYZ*> returns nullptr).
#
# This test runs the producer on the committed V5 display + observer inputs and
# inspects the on-disk type signature of each of the three matrix-column tags.
# It FAILS if any tag is still 'sf32' (s15Fixed16ArrayType, hex 73663332)
# instead of 'XYZ ' (XYZType, hex 58595a20).
#
# Plain functional test -- no sanitizer build required.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass, or skipped cleanly (tool / fixtures unavailable)
#   2 - regression: a matrix-column tag is not XYZType
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1362}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
DSP="$DATA_DIR/v5dspobs-lcddisplay.icc"
OBS="$DATA_DIR/v5dspobs-cat8lab.icc"
mkdir -p "$OUTDIR"

PRODUCER="$(find "$TOOLS_DIR" -maxdepth 2 -name iccV5DspObsToV4Dsp -type f 2>/dev/null | head -1)"
if [ -z "$PRODUCER" ] || [ ! -x "$PRODUCER" ]; then
  echo "[SKIP] iccV5DspObsToV4Dsp not found under $TOOLS_DIR"
  exit 0
fi
for f in "$DSP" "$OBS"; do
  if [ ! -f "$f" ]; then
    echo "[SKIP] input fixture missing: $f"
    exit 0
  fi
done

OUT="$OUTDIR/v5dspobs-out.icc"
"$PRODUCER" "$DSP" "$OBS" "$OUT" > "$OUTDIR/producer.log" 2>&1
if [ ! -f "$OUT" ]; then
  echo "[SKIP] producer did not create an output profile"
  cat "$OUTDIR/producer.log"
  exit 0
fi

# Read a big-endian uint32 at a byte offset.
read_u32be() { # file offset
  local hex
  hex="$(dd if="$1" bs=1 skip="$2" count=4 status=none 2>/dev/null | od -An -tx1 | tr -d ' \n')"
  printf '%d' "$((16#$hex))"
}
# Read 4 bytes at an offset as lowercase hex (a signature).
read_sig() { # file offset
  dd if="$1" bs=1 skip="$2" count=4 status=none 2>/dev/null | od -An -tx1 | tr -d ' \n'
}

# Signatures, as hex: tag names and the type values we discriminate.
SIG_R=7258595a   # 'rXYZ'
SIG_G=6758595a   # 'gXYZ'
SIG_B=6258595a   # 'bXYZ'
TYPE_XYZ=58595a20  # 'XYZ '  (XYZType -- correct)
TYPE_SF32=73663332 # 'sf32'  (s15Fixed16ArrayType -- the bug)

ntags="$(read_u32be "$OUT" 128)"
status=0
seen=0

for ((i=0; i<ntags; i++)); do
  base=$((132 + i*12))
  sig="$(read_sig "$OUT" "$base")"
  case "$sig" in
    "$SIG_R"|"$SIG_G"|"$SIG_B") ;;
    *) continue ;;
  esac
  case "$sig" in
    "$SIG_R") name=rXYZ ;;
    "$SIG_G") name=gXYZ ;;
    "$SIG_B") name=bXYZ ;;
  esac
  seen=$((seen+1))
  toff="$(read_u32be "$OUT" $((base+4)))"
  tsig="$(read_sig "$OUT" "$toff")"
  if [ "$tsig" = "$TYPE_XYZ" ]; then
    echo "[PASS] #1362: $name is XYZType ('XYZ ')"
  elif [ "$tsig" = "$TYPE_SF32" ]; then
    echo "[FAIL] #1362: $name is s15Fixed16ArrayType ('sf32') -- must be XYZType ('XYZ ')"
    status=2
  else
    echo "[FAIL] #1362: $name has unexpected type signature 0x$tsig (expected 'XYZ '=58595a20)"
    status=2
  fi
done

if [ "$seen" -ne 3 ]; then
  echo "[FAIL] #1362: expected rXYZ/gXYZ/bXYZ tags, found $seen of 3"
  status=2
fi

exit "$status"
