#!/bin/bash
###############################################################################
# iccDEV issue #1853 - plain <TextData> silently dropped by the compressed-text
#                      XML readers (zut8 / zxml)
###############################################################################
#
# CIccTagXmlZipUtf8Text::ParseXml and CIccTagXmlZipXml::ParseXml both scanned
# the sibling list for <HexCompressedData> and then fell through to the
# plain-text reader:
#
#   while (pNode) {                          // IccXML/IccLibXML/IccTagXml.cpp
#     ... if (HexCompressedData) { ...; return true; }
#     pNode = pNode->next;
#   }
#   std::string outStr;
#   if( !icXmlParseTextString(pNode, parseStr, outStr, false) )   // pNode NULL
#     return false;
#   return SetText(outStr.c_str());
#
# The loop returns from inside itself on a match, so the only way to reach the
# call below it is with pNode already walked to NULL.  icXmlParseTextString()
# opens with its own `while (pNode)`, so it never dereferenced the NULL -- it
# appended nothing and returned true.  The tag was then SetText("").
#
# That is fail-open data loss rather than a crash: iccFromXml printed "Profile
# parsed and saved correctly" and wrote a profile whose compressed text tag is
# an empty string.  Nothing in the corpus exercised it -- no committed fixture
# authors a zut8 or zxml tag at all -- so the three XML files this script uses
# were written for it.
#
# CASE A -- zut8.  charTargetTag as utf8ZipType with <TextData>.  v5, because
# CIccProfile::CheckTagTypes only accepts zut8 under charTargetTag at v5.
#
# CASE B -- zxml.  CxfTag as zipXmlType with <TextData>.  v4, because that tag
# accepts zxml only below v5.  Same defect, separate copy of the code, so a fix
# applied to one call site and not the other still fails here.
#
# CASE C -- round-trip closure.  iccToXml re-emits these tags in the
# <HexCompressedData> form, so case C converts case A's profile back to XML and
# forward again, proving the two authoring forms agree rather than only that
# one of them survived a single pass.
#
# All three assert on the decompressed text, using iccDumpProfile's
# "ZLib Compressed String=" line as the oracle.  Comparing written bytes would
# not work: every fixture stamps <CreationDateTime>now</CreationDateTime>, which
# feeds the profile ID.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (or skipped cleanly)
#   2 - regression detected
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1853}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

MARKER="iccDEV-1853-ZIPTEXT-ROUNDTRIP-MARKER"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ] || [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  echo "[SKIP] iccFromXml / iccDumpProfile not found under $TOOLS_DIR"
  exit 0
fi

# Convert $1.xml (from the test-data directory) into $OUTDIR/$1.icc.
#
# Sets the globals rather than echoing a path: a caller using $(convert ...)
# would run this in a command-substitution subshell and CONV_RC would not
# escape it.
CONV_RC=0
CONV_LOG=""
CONV_ICC=""
convert_fixture() {
  local name="$1"
  CONV_LOG="$OUTDIR/$name.log"
  CONV_ICC="$OUTDIR/$name.icc"
  rm -f "$CONV_ICC"
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0:allocator_may_return_null=1" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$FROMXML" "$DATA_DIR/$name.xml" "$CONV_ICC" > "$CONV_LOG" 2>&1
  CONV_RC=$?
}

# Decompressed text of the first compressed-text tag in $1, or the empty string.
#
# iccDumpProfile prints it as:  ZLib Compressed String="<text>
# The trailing quote sits on the following line because SetText() compresses the
# NUL terminator along with the text, so the sed below takes everything after
# the opening quote on the matching line and nothing else.
zip_text_of() {
  "$DUMP" "$1" ALL 2>/dev/null |
    sed -n 's/^ZLib Compressed String="//p' |
    head -1
}

# A build without zlib cannot represent these tags at all: SetText() returns
# false unconditionally and Describe() prints BEGIN_COMPRESSED_DATA instead of
# the decompressed string, so every case below would report a false failure.
# Detect it from the control's dump rather than from CMakeCache, which is not
# reachable from an installed-tools run.
uses_zlib() {
  "$DUMP" "$1" ALL 2>/dev/null | grep -q "ZLib Compressed String="
}

# --- CONTROL: the <HexCompressedData> branch, which the fix does not touch ----
# Run first.  If the fix ever over-rejects, or the environment cannot handle
# these tags, this fails/skips here instead of the two PoCs passing or failing
# for the wrong reason.
CONTROL="zip-text-hexdata-control-1853"
if [ ! -f "$DATA_DIR/$CONTROL.xml" ]; then
  echo "[SKIP] control XML missing: $DATA_DIR/$CONTROL.xml"
  exit 0
fi
convert_fixture "$CONTROL"
CONTROL_ICC="$CONV_ICC"
if [ ! -f "$CONTROL_ICC" ]; then
  echo "[SKIP] control profile did not convert (rc=$CONV_RC); environment issue, not #1853"
  sed -n '1,10p' "$CONV_LOG"
  exit 0
fi
if ! uses_zlib "$CONTROL_ICC"; then
  echo "[SKIP] tools built without ICC_USE_ZLIB; compressed text tags cannot be decoded"
  exit 0
fi
CONTROL_TEXT="$(zip_text_of "$CONTROL_ICC")"
case "$CONTROL_TEXT" in
  *"$MARKER"*)
    echo "[PASS] #1853 control: HexCompressedData charTargetTag still decodes to the marker"
    ;;
  *)
    echo "[SKIP] control HexCompressedData did not decode to the marker; environment issue, not #1853"
    echo "       got: '$CONTROL_TEXT'"
    exit 0
    ;;
esac

status=0

# assert_marker <label> <icc> <failure text>
assert_marker() {
  local label="$1" icc="$2" what="$3" text
  text="$(zip_text_of "$icc")"
  case "$text" in
    *"$MARKER"*)
      echo "[PASS] #1853 $label: $what survived the conversion"
      return 0
      ;;
    "")
      echo "[FAIL] #1853 $label: $what was stored as an EMPTY string"
      echo "       the plain-text fallback was handed a NULL node list again"
      status=2
      return 1
      ;;
    *)
      echo "[FAIL] #1853 $label: $what decoded to unexpected content"
      echo "       got: '$text'"
      status=2
      return 1
      ;;
  esac
}

# --- CASE A: zut8 authored as plain <TextData> -------------------------------
POC_A="zut8-plaintext-1853"
CASE_A_ICC=""
if [ ! -f "$DATA_DIR/$POC_A.xml" ]; then
  echo "[SKIP] PoC XML missing: $DATA_DIR/$POC_A.xml"
else
  convert_fixture "$POC_A"
  if [ ! -f "$CONV_ICC" ]; then
    echo "[FAIL] #1853 case A: plain <TextData> zut8 tag no longer converts (rc=$CONV_RC)"
    sed -n '1,10p' "$CONV_LOG"
    status=2
  else
    assert_marker "case A (zut8)" "$CONV_ICC" "charTargetTag plain <TextData>" &&
      CASE_A_ICC="$CONV_ICC"
  fi
fi

# --- CASE B: zxml authored as plain <TextData> -------------------------------
POC_B="zxml-plaintext-1853"
if [ ! -f "$DATA_DIR/$POC_B.xml" ]; then
  echo "[SKIP] PoC XML missing: $DATA_DIR/$POC_B.xml"
else
  convert_fixture "$POC_B"
  if [ ! -f "$CONV_ICC" ]; then
    echo "[FAIL] #1853 case B: plain <TextData> zxml tag no longer converts (rc=$CONV_RC)"
    sed -n '1,10p' "$CONV_LOG"
    status=2
  else
    assert_marker "case B (zxml)" "$CONV_ICC" "CxfTag plain <TextData>" || true
  fi
fi

# --- CASE C: XML -> ICC -> XML -> ICC closure --------------------------------
# Only meaningful if case A produced a profile carrying the marker; otherwise it
# would re-report the same failure a second time.
if [ -z "$TOXML" ] || [ ! -x "$TOXML" ]; then
  echo "[SKIP] case C: iccToXml not found under $TOOLS_DIR"
elif [ -z "$CASE_A_ICC" ]; then
  echo "[SKIP] case C: case A did not produce a usable profile"
else
  RT_XML="$OUTDIR/$POC_A-roundtrip.xml"
  RT_ICC="$OUTDIR/$POC_A-roundtrip.icc"
  rm -f "$RT_XML" "$RT_ICC"
  if ! "$TOXML" "$CASE_A_ICC" "$RT_XML" > "$OUTDIR/$POC_A-roundtrip.log" 2>&1 || [ ! -f "$RT_XML" ]; then
    echo "[FAIL] #1853 case C: iccToXml could not re-emit the zut8 tag"
    sed -n '1,10p' "$OUTDIR/$POC_A-roundtrip.log"
    status=2
  elif ! grep -q "HexCompressedData" "$RT_XML"; then
    echo "[FAIL] #1853 case C: re-emitted XML carries no <HexCompressedData> for the zut8 tag"
    status=2
  elif ! "$FROMXML" "$RT_XML" "$RT_ICC" >> "$OUTDIR/$POC_A-roundtrip.log" 2>&1 || [ ! -f "$RT_ICC" ]; then
    echo "[FAIL] #1853 case C: re-emitted XML no longer converts back to a profile"
    sed -n '1,10p' "$OUTDIR/$POC_A-roundtrip.log"
    status=2
  else
    assert_marker "case C (round-trip)" "$RT_ICC" "the marker after XML -> ICC -> XML -> ICC" || true
  fi
fi

exit "$status"
