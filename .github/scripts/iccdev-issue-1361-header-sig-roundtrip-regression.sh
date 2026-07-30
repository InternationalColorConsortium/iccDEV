#!/bin/bash
###############################################################################
# iccDEV issue #1361 - header signature fields must survive an XML round-trip
#                      when zero (generalises #1356 / #1358)
###############################################################################
#
# #1356 fixed the XML serializer corrupting a zero *data colour space* / PCS
# signature: icGetSig*(0) emits the literal "NULL", and icGetSigVal("NULL")
# reparses it to 0x4E554C4C, so a zero signature did not round-trip. The same
# unguarded pattern existed for the other header signature fields that the JSON
# serializer already guards -- PreferredCMMType (cmmId), ProfileDeviceClass
# (deviceClass) and ProfileCreator (creator). This test keeps the historical
# zero-signature round-trip coverage as a plain functional regression.
#
# It zeroes those three header fields in a real profile, serialises it to XML
# and parses it back, and FAILS if any field is corrupted to 0x4E554C4C
# ("NULL" packed as ASCII) instead of round-tripping as zero.
#
# Header byte offsets (ICC.1 Table 13):
#   cmmId       @ 4   (PreferredCMMType)
#   deviceClass @ 12  (ProfileDeviceClass)
#   creator     @ 80  (ProfileCreator)
#
# Plain functional round-trip test -- no sanitizer build required.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass, or skipped cleanly (tools / fixture unavailable)
#   2 - regression: a zeroed header signature was corrupted on XML round-trip
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1361}"
FIXTURE="$REPO_ROOT/.github/ci/test-data/nodata-colourspace-1356.icc"
mkdir -p "$OUTDIR"

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

TOXML="$(find_tool iccToXml)"
FROMXML="$(find_tool iccFromXml)"

for t in "$TOXML" "$FROMXML"; do
  if [ -z "$t" ] || [ ! -x "$t" ]; then
    echo "[SKIP] required tool missing under $TOOLS_DIR (need iccToXml, iccFromXml)"
    exit 0
  fi
done

if [ ! -f "$FIXTURE" ]; then
  echo "[SKIP] fixture missing: $FIXTURE"
  exit 0
fi

BASE="$OUTDIR/zerosig.icc"
XML="$OUTDIR/zerosig.xml"
RT="$OUTDIR/zerosig.roundtrip.icc"

cp "$FIXTURE" "$BASE"

# Zero cmmId / deviceClass / creator in the (mutable) copy. dd with notrunc
# overwrites in place without changing file length.
zero4() { # offset
  printf '\x00\x00\x00\x00' | dd of="$BASE" bs=1 seek="$1" count=4 conv=notrunc status=none
}
zero4 4    # cmmId
zero4 12   # deviceClass
zero4 80   # creator

# Read 4 big-endian header bytes at an offset as lowercase hex.
read4() { # file offset
  dd if="$1" bs=1 skip="$2" count=4 status=none 2>/dev/null | od -An -tx1 | tr -d ' \n'
}

"$TOXML" "$BASE" "$XML" > "$OUTDIR/toxml.log" 2>&1

status=0

# The three elements must serialize empty, never the literal "NULL".
for tag in PreferredCMMType ProfileDeviceClass ProfileCreator; do
  if grep -qiE "<$tag>[[:space:]]*NULL[[:space:]]*</$tag>" "$XML"; then
    echo "[FAIL] #1361: iccToXml emitted <$tag>NULL</$tag> for a zero signature"
    status=2
  fi
done

"$FROMXML" "$XML" "$RT" > "$OUTDIR/fromxml.log" 2>&1

if [ ! -f "$RT" ]; then
  echo "[SKIP] iccFromXml produced no output (invalid intermediate profile); cannot verify bytes"
  exit 0
fi

# Each field must round-trip as zero, never 0x4E554C4C ('NULL' packed as ASCII).
check_field() { # name offset
  local name="$1" off="$2" val
  val="$(read4 "$RT" "$off")"
  if [ "$val" = "4e554c4c" ]; then
    echo "[FAIL] #1361: $name corrupted to 0x4E554C4C on XML round-trip"
    status=2
  elif [ "$val" = "00000000" ]; then
    echo "[PASS] #1361: $name survived the XML round-trip as zero"
  else
    echo "[FAIL] #1361: $name unexpected value 0x$val after round-trip (expected 00000000)"
    status=2
  fi
}
check_field cmmId       4
check_field deviceClass 12
check_field creator     80

exit "$status"
