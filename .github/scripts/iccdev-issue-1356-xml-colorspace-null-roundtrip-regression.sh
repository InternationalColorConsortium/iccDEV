#!/bin/bash
###############################################################################
# iccDEV issue #1356 - XML serializer corrupts a zero (NoData) header
#                      colour-space signature on round-trip
###############################################################################
#
# CIccProfileXml::ToXml() serialized the data colour space and PCS header
# signatures with icGetColorSigStr() unconditionally.  For a zero signature
# (0x00000000, i.e. NoData) that helper returns the literal text "NULL"; the
# inverse icGetSigVal("NULL") on reparse packs the ASCII bytes 'N','U','L','L'
# into 0x4E554C4C.  A NoData data colour space therefore silently became
# "Unknown 'NULL' = 4E554C4C" after an iccToXml -> iccFromXml round-trip:
#
#   Data Color Space:   Unknown 'NULL' = 4E554C4C        (corrupted)
#
# iccFromJson kept the value as NoData because the JSON writer
# (IccProfileJson.cpp) guards the zero case and emits "" instead of "NULL".
# That asymmetry was the sole reason iccFromXml and iccFromJson reported
# different data colour spaces for the same profile.  The fix mirrors the JSON
# writer: a zero DataColourSpace / PCS signature is serialized as an empty
# element, which icXmlGetChildSigVal() round-trips back to 0 (NoData).
#
# This test round-trips a profile whose data colour space is 0x00000000
# (.github/ci/test-data/nodata-colourspace-1356.icc, the LCDDisplayCat8Obs.icc
# profile from the issue) through iccToXml -> iccFromXml and FAILS if:
#   * the serialized <DataColourSpace> element contains the literal "NULL", or
#   * the round-tripped profile reports the corrupted 0x4E554C4C value rather
#     than preserving NoData.
#
# Plain functional round-trip test -- no sanitizer build required.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass, or skipped cleanly (tools / fixture unavailable)
#   2 - regression: zero data colour space was corrupted on XML round-trip
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1356}"
FIXTURE="$REPO_ROOT/.github/ci/test-data/nodata-colourspace-1356.icc"
mkdir -p "$OUTDIR"

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

TOXML="$(find_tool iccToXml)"
FROMXML="$(find_tool iccFromXml)"
DUMP="$(find_tool iccDumpProfile)"

for t in "$TOXML" "$FROMXML" "$DUMP"; do
  if [ -z "$t" ] || [ ! -x "$t" ]; then
    echo "[SKIP] required tool missing under $TOOLS_DIR (need iccToXml, iccFromXml, iccDumpProfile)"
    exit 0
  fi
done

if [ ! -f "$FIXTURE" ]; then
  echo "[SKIP] NoData fixture missing: $FIXTURE"
  exit 0
fi

# Sanity: the fixture must genuinely carry a NoData (zero) data colour space,
# otherwise the test would pass vacuously without exercising the guard.
if ! "$DUMP" "$FIXTURE" 2>/dev/null | grep -qiE "Data Colou?r Space:[[:space:]]*NoData"; then
  echo "[SKIP] fixture is not a NoData-data-colour-space profile; nothing to exercise"
  exit 0
fi

XML="$OUTDIR/nodata-1356.xml"
RT="$OUTDIR/nodata-1356.roundtrip.icc"

# 1) Serialize to XML.  iccToXml exits 0 on success regardless of profile
#    validity (the fixture is intentionally invalid but parses fine).
"$TOXML" "$FIXTURE" "$XML" > "$OUTDIR/toxml.log" 2>&1

status=0

# 2) The emitted DataColourSpace element must NOT carry the literal "NULL".
if grep -qiE "<DataColourSpace>[[:space:]]*NULL[[:space:]]*</DataColourSpace>" "$XML"; then
  echo "[FAIL] #1356: iccToXml emitted <DataColourSpace>NULL</DataColourSpace> for a zero signature"
  grep -iE "DataColourSpace" "$XML" | head
  status=2
fi

# 3) Round-trip back from XML and confirm the data colour space survived as
#    NoData rather than the corrupted 0x4E554C4C ("NULL" packed as ASCII).
"$FROMXML" "$XML" "$RT" > "$OUTDIR/fromxml.log" 2>&1
dcs_line="$("$DUMP" "$RT" 2>/dev/null | grep -iE "Data Colou?r Space:")"

if echo "$dcs_line" | grep -qiE "4E554C4C|Unknown '?NULL"; then
  echo "[FAIL] #1356: zero data colour space corrupted to 0x4E554C4C on XML round-trip"
  echo "       $dcs_line"
  status=2
elif echo "$dcs_line" | grep -qiE "NoData"; then
  echo "[PASS] #1356: zero (NoData) data colour space survived the XML round-trip"
else
  echo "[FAIL] #1356: unexpected data colour space after round-trip: ${dcs_line:-<none>}"
  status=2
fi

exit "$status"
