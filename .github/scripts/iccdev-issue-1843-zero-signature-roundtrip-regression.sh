#!/bin/bash
###############################################################################
# Zero-signature XML/JSON round-trip regression (issue #1843)
###############################################################################
#
# icGetSigStr() renders a zero signature as the literal text "NULL".  That is a
# *display* convention -- it is what iccDumpProfile prints for an absent
# signature -- but several XML and JSON writers used it as a *serialisation*
# primitive.  The inverse, icGetSigVal("NULL"), packs the ASCII bytes
# 'N','U','L','L' into 0x4E554C4C, so a legitimately zero signature does not
# survive a round trip through either text format.
#
# Two ways that bites:
#
#   1. profileSequenceDescType.  A technology of zero is the named value
#      icSigUndefined, and iccFromCube emits exactly that.  The reparsed profile
#      picked up a 0x4E554C4C technology and failed validation with
#      "NonCompliant! - profileSequenceDescTag: - Unknown 'NULL' = 4E554C4C",
#      which is what made #1843 read as an iccFromCube defect.  iccFromCube's
#      own output is clean; the corruption is introduced by the serializer.
#
#   2. signatureType tags and BAcs/EAcs elements.  Here the damage is silent:
#      0x4E554C4C is merely an *unrecognised* signature, not a malformed one, so
#      the validator still reports the round-tripped profile as valid while four
#      payload bytes have changed underneath it.
#
# Commit 751a0c6b (PR #1365) fixed this same defect class for the profile header
# fields by writing an empty element for zero.  This test covers the writers that
# were missed.  For the zero-signature defect the readers on both sides already
# map empty text back to zero, so that fix is writer-only and the assertions are
# all "what comes back out".
#
# Section 3 additionally covers a *reader* defect found in the same two acs
# element handlers: CIccMpeJsonBAcs/EAcs::ParseJson dropped the input/output
# channel counts that the JSON writer had stamped on them, so an acs bookend
# reloaded from JSON reported zero channels and failed element-chain validation.
# Its assertions are the byte comparison and the validation report, not the
# serialized text, because the text was never wrong.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1843-zero-signature-roundtrip}"
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

FROMCUBE="$(find_tool iccFromCube)"
TOXML="$(find_tool iccToXml)"
FROMXML="$(find_tool iccFromXml)"
TOJSON="$(find_tool iccToJson)"
FROMJSON="$(find_tool iccFromJson)"
DUMP="$(find_tool iccDumpProfile)"

fail() {
  echo "  [FAIL] issue-1843-zero-signature-roundtrip -- $1"
  exit 1
}

echo "=== Zero-signature XML/JSON round-trip regression (issue #1843) ==="

for tool in "$FROMCUBE" "$TOXML" "$FROMXML" "$TOJSON" "$FROMJSON" "$DUMP"; do
  if [ -z "$tool" ] || [ ! -x "$tool" ]; then
    echo "  [SKIP] XML/JSON round-trip tools not all built under $TOOLS_DIR"
    exit 0
  fi
done

# ---------------------------------------------------------------------------
# run <label> <tool> <args...> -- run a tool, showing its log if it fails.
# ---------------------------------------------------------------------------
run() {
  local label="$1"; shift
  local log="$OUTDIR/$label.log"

  if ! "$@" > "$log" 2>&1; then
    sed -n '1,20p' "$log"
    fail "$label failed"
  fi
}

# ---------------------------------------------------------------------------
# The corruption always shows up as the four characters NULL where an empty
# value belongs, so each assertion is "this field did not come back as NULL".
# grep -F keeps the pattern literal -- the JSON forms contain quotes and colons.
# ---------------------------------------------------------------------------
assert_absent() {
  local file="$1" pattern="$2" what="$3"
  if grep -qF "$pattern" "$file"; then
    grep -nF "$pattern" "$file" | sed -n '1,4p' | sed 's/^/    /'
    fail "$what round-tripped through the \"NULL\" text encoding (#1843)"
  fi
}

assert_present() {
  local file="$1" pattern="$2" what="$3"
  grep -qF "$pattern" "$file" || fail "$what was not written as an empty value (#1843)"
}

# ===========================================================================
# 1. profileSequenceDescType -- the failure reported in #1843.
#
# iccFromCube records no manufacturer, model or technology, so all three
# signatures in the profileSequenceDesc struct are zero.  Both text formats must
# preserve that, and the reparsed profile must still validate.
# ===========================================================================
CUBE="$REPO_ROOT/.github/ci/test-data/test-identity.cube"
[ -f "$CUBE" ] || fail "missing fixture $CUBE"

run fromcube "$FROMCUBE" "$CUBE" "$OUTDIR/pseq.icc"

run pseq-toxml "$TOXML" "$OUTDIR/pseq.icc" "$OUTDIR/pseq.xml"
assert_absent  "$OUTDIR/pseq.xml" "<Technology>NULL</Technology>" "profileSequenceDesc Technology"
assert_absent  "$OUTDIR/pseq.xml" "<DeviceManufacturerSignature>NULL<" "profileSequenceDesc DeviceManufacturerSignature"
assert_absent  "$OUTDIR/pseq.xml" "<DeviceModelSignature>NULL<" "profileSequenceDesc DeviceModelSignature"
assert_present "$OUTDIR/pseq.xml" "<Technology></Technology>" "profileSequenceDesc Technology"

run pseq-tojson "$TOJSON" "$OUTDIR/pseq.icc" "$OUTDIR/pseq.json"
assert_absent  "$OUTDIR/pseq.json" '"technology": "NULL"' "profileSequenceDesc technology"
assert_absent  "$OUTDIR/pseq.json" '"deviceManufacturerSignature": "NULL"' "profileSequenceDesc deviceManufacturerSignature"
assert_absent  "$OUTDIR/pseq.json" '"deviceModelSignature": "NULL"' "profileSequenceDesc deviceModelSignature"
assert_present "$OUTDIR/pseq.json" '"technology": ""' "profileSequenceDesc technology"

# The profile that comes back must validate.  Before the fix this reported
# "NonCompliant! - profileSequenceDescTag: - Unknown 'NULL' = 4E554C4C".
for fmt in xml json; do
  case "$fmt" in
    xml)  run "pseq-fromxml"  "$FROMXML"  "$OUTDIR/pseq.xml"  "$OUTDIR/pseq-rt-xml.icc" ;;
    json) run "pseq-fromjson" "$FROMJSON" "$OUTDIR/pseq.json" "$OUTDIR/pseq-rt-json.icc" ;;
  esac

  "$DUMP" -v "$OUTDIR/pseq-rt-$fmt.icc" > "$OUTDIR/pseq-validate-$fmt.log" 2>&1
  if grep -q "4E554C4C" "$OUTDIR/pseq-validate-$fmt.log"; then
    grep -n "4E554C4C" "$OUTDIR/pseq-validate-$fmt.log" | sed -n '1,4p' | sed 's/^/    /'
    fail "the $fmt round trip left a 0x4E554C4C technology in profileSequenceDescTag (#1843)"
  fi
  grep -q "NonCompliant" "$OUTDIR/pseq-validate-$fmt.log" \
    && fail "the $fmt round trip made a valid profile non-compliant (#1843)"
done

# A clean validation report is necessary but not sufficient -- 0x4E554C4C was
# only ever flagged because the *technology* enumeration is checked by name, and
# a wrong value in a field with no such check would pass silently.  The XML round
# trip reproduces the profile byte for byte, so assert that directly.
if ! cmp -s "$OUTDIR/pseq.icc" "$OUTDIR/pseq-rt-xml.icc"; then
  cmp -l "$OUTDIR/pseq.icc" "$OUTDIR/pseq-rt-xml.icc" 2>/dev/null | sed -n '1,6p' | sed 's/^/    /'
  fail "an ICC -> XML -> ICC round trip changed the bytes of a zero-signature profileSequenceDesc (#1843)"
fi
echo "    profileSequenceDesc: zero signatures survive both round trips, XML byte-exact"

# ===========================================================================
# 2. signatureType tags -- the silent case.
#
# Zeroing the imst signature of a tracked fixture gives a profile whose
# signatureType payload is legitimately 0.  The round trip must be byte-exact:
# before the fix exactly four bytes changed from 00 00 00 00 to 'N','U','L','L'
# and the validator still called the result valid, so a byte comparison is the
# only assertion that catches it.
# ===========================================================================
SRC_XML="$REPO_ROOT/Testing/Encoding/ISO22028-Encoded-sRGB.xml"
if [ -f "$SRC_XML" ]; then
  sed 's|<Signature>dorc</Signature>|<Signature></Signature>|' "$SRC_XML" > "$OUTDIR/sig-zero-in.xml"
  assert_present "$OUTDIR/sig-zero-in.xml" "<Signature></Signature>" "the zeroed input signature"

  run sig-build "$FROMXML" "$OUTDIR/sig-zero-in.xml" "$OUTDIR/sig-zero.icc"

  run sig-toxml "$TOXML" "$OUTDIR/sig-zero.icc" "$OUTDIR/sig-zero.xml"
  assert_absent  "$OUTDIR/sig-zero.xml" "<Signature>NULL</Signature>" "signatureType Signature"
  assert_present "$OUTDIR/sig-zero.xml" "<Signature></Signature>" "signatureType Signature"

  run sig-tojson "$TOJSON" "$OUTDIR/sig-zero.icc" "$OUTDIR/sig-zero.json"
  assert_absent  "$OUTDIR/sig-zero.json" '"signature": "NULL"' "signatureType signature"

  # Assert on the bytes that come back, not just on the text that went out.
  # Checking only the serialized text is too weak here: it passes as soon as the
  # writer stops emitting "NULL", even if the reader then restores some *other*
  # wrong value.  That is a live hazard on the JSON side, where the
  # CIccTagSignature constructor seeds m_nSig with 0x3F3F3F3F ("????") -- a
  # reader that treats an empty string as "field absent" leaves that sentinel in
  # place and corrupts the tag just as thoroughly as "NULL" did.
  run sig-fromxml  "$FROMXML"  "$OUTDIR/sig-zero.xml"  "$OUTDIR/sig-zero-rt-xml.icc"
  run sig-fromjson "$FROMJSON" "$OUTDIR/sig-zero.json" "$OUTDIR/sig-zero-rt-json.icc"
  for fmt in xml json; do
    if ! cmp -s "$OUTDIR/sig-zero.icc" "$OUTDIR/sig-zero-rt-$fmt.icc"; then
      cmp -l "$OUTDIR/sig-zero.icc" "$OUTDIR/sig-zero-rt-$fmt.icc" 2>/dev/null | sed -n '1,6p' | sed 's/^/    /'
      fail "an ICC -> ${fmt} -> ICC round trip changed the bytes of a zero signatureType (#1843)"
    fi
  done
  echo "    signatureType: zero signature is byte-exact across both round trips"
else
  echo "    [note] $SRC_XML absent -- signatureType section skipped"
fi

# ===========================================================================
# 3. BAcs / EAcs elements.
#
# Zero is a named, legal acs signature -- IccProfLib calls it icSigAcsZero -- so
# an element carrying it must not acquire one on the way through either format.
# The elements are grafted onto a tracked MPE fixture because the corpus has no
# profile with an acs bookend of its own; the begin bookend goes first in the
# element chain and the end bookend last, which is where they belong.
#
# This section also covers a second, independent defect in the same two readers.
# CIccTagJsonMultiProcessElement::ToJson stamps inputChannels/outputChannels onto
# every element generically, and each element's ParseJson reads them back --
# except CIccMpeJsonBAcs/EAcs::ParseJson, which did not.  Since
# CIccMpeCreator::CreateElement builds these through CIccMpeBAcs(nChannels = 0),
# an acs bookend came back from JSON reporting 0 in and 0 out however many the
# document declared, and the profile that had just been written out valid
# reloaded as
#
#   Error! - AToB1Tag:Mis-matching number of input channels in first process element!
#
# The XML reader was always correct here (CIccMpeXmlBAcs::ParseXml calls
# icXmlParseChannels), which is why only the JSON round trip was affected.  An
# earlier revision of this test excluded the JSON round trip from the byte
# comparison below for exactly that reason; both formats are now checked.
# ===========================================================================
MPE_XML="$REPO_ROOT/Testing/Display/sRGB_D65_MAT-300cdm2.xml"
if [ -f "$MPE_XML" ]; then
  awk '{
    if (!de && $0 ~ /<\/MultiProcessElements>/) {
      print "        <EAcsElement InputChannels=\"3\" OutputChannels=\"3\" Signature=\"\"/>"
      de = 1
    }
    print
    if (!db && $0 ~ /<MultiProcessElements InputChannels="3" OutputChannels="3">/) {
      print "        <BAcsElement InputChannels=\"3\" OutputChannels=\"3\" Signature=\"\"/>"
      db = 1
    }
  }' "$MPE_XML" > "$OUTDIR/acs-in.xml"
  assert_present "$OUTDIR/acs-in.xml" "<BAcsElement" "the grafted BAcsElement"
  assert_present "$OUTDIR/acs-in.xml" "<EAcsElement" "the grafted EAcsElement"

  run acs-build "$FROMXML" "$OUTDIR/acs-in.xml" "$OUTDIR/acs.icc"

  run acs-toxml "$TOXML" "$OUTDIR/acs.icc" "$OUTDIR/acs.xml"
  assert_absent  "$OUTDIR/acs.xml" 'Signature="NULL"' "BAcsElement Signature"
  assert_present "$OUTDIR/acs.xml" 'Signature=""' "BAcsElement Signature"

  run acs-tojson "$TOJSON" "$OUTDIR/acs.icc" "$OUTDIR/acs.json"
  assert_absent "$OUTDIR/acs.json" '"signature": "NULL"' "BAcsElement signature"

  # The writer half was never at fault -- assert that, so a future failure here
  # is attributed to the reader rather than to the counts going missing upstream.
  #
  # This has to be scoped to the acs element objects rather than grepped for
  # across the file: the fixture is 3-channel throughout, so a bare search for
  # '"inputChannels": 3' matches a dozen other elements and would still pass with
  # the acs counts gone.  Isolate each element object by brace and test inside
  # it, which also keeps the check independent of key order -- ICC_JSON_ORDERED
  # changes that, and a fixed-offset context window would silently stop matching.
  assert_acs_counts() {
    local file="$1" elem="$2" block
    block="$(awk -v e="$elem" '
      /\{/  { buf = "" }
              { buf = buf $0 "\n" }
      /\}/  { if (buf ~ "\"type\": \"" e "\"") printf "%s", buf; buf = "" }
    ' "$file")"
    [ -n "$block" ] || fail "$elem is missing from the JSON entirely (#1843)"
    printf '%s' "$block" | grep -qF '"inputChannels": 3' \
      || fail "$elem lost its inputChannels in the JSON writer (#1843)"
    printf '%s' "$block" | grep -qF '"outputChannels": 3' \
      || fail "$elem lost its outputChannels in the JSON writer (#1843)"
  }
  assert_acs_counts "$OUTDIR/acs.json" BAcsElement
  assert_acs_counts "$OUTDIR/acs.json" EAcsElement

  # Assert on the bytes that come back rather than only on the text that went
  # out.  Checking the serialized text alone is too weak: it passes as soon as
  # the writer stops emitting "NULL", even if the reader then restores some other
  # wrong value -- which is precisely what the dropped channel counts did.
  run acs-fromxml  "$FROMXML"  "$OUTDIR/acs.xml"  "$OUTDIR/acs-rt-xml.icc"
  run acs-fromjson "$FROMJSON" "$OUTDIR/acs.json" "$OUTDIR/acs-rt-json.icc"
  for fmt in xml json; do
    if ! cmp -s "$OUTDIR/acs.icc" "$OUTDIR/acs-rt-$fmt.icc"; then
      cmp -l "$OUTDIR/acs.icc" "$OUTDIR/acs-rt-$fmt.icc" 2>/dev/null | sed -n '1,6p' | sed 's/^/    /'
      fail "an ICC -> ${fmt} -> ICC round trip changed the bytes of an icSigAcsZero element (#1843)"
    fi
  done

  # The byte comparison above is the strict assertion, but it reports a diff
  # rather than the symptom a user would see, so check the symptom too: before
  # the reader fix the reloaded profile failed element-chain validation.
  "$DUMP" -v "$OUTDIR/acs-rt-json.icc" > "$OUTDIR/acs-validate-json.log" 2>&1
  if grep -q "Mis-matching number of input channels" "$OUTDIR/acs-validate-json.log"; then
    grep -n "Mis-matching number of input channels" "$OUTDIR/acs-validate-json.log" \
      | sed -n '1,3p' | sed 's/^/    /'
    fail "the JSON round trip dropped the acs element channel counts (#1843)"
  fi
  echo "    BAcs/EAcsElement: icSigAcsZero and the channel counts survive both round trips, byte-exact"
else
  echo "    [note] $MPE_XML absent -- BAcs section skipped"
fi

echo "  [PASS] issue-1843-zero-signature-roundtrip -- zero signatures are not written as \"NULL\""
exit 0
