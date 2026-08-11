#!/bin/bash
###############################################################################
# Trailing-NUL signature XML/JSON round-trip regression (issue #2097)
###############################################################################
#
# icGetSigStr() renders a signature as four characters when it can, and falls
# back to an unambiguous "%08Xh" escape when it cannot.  The fallback used to be
# selected only by a zero byte *followed* by a non-zero one, so a signature whose
# zeros were all trailing took the four-character path -- where the zero simply
# terminated the C string.  'DUP\0' (0x44555000) came out as "DUP", and the
# inverse icGetSigVal() right-pads a three-character signature with 0x20, so the
# value that came back was 'DUP ' (0x44555020).
#
# That matters because both machine-format writers serialize tag-table
# identifiers through this display helper:
#
#   IccXML/IccLibXML/IccProfileXml.cpp   <PrivateTag TagSignature="...">
#   IccJSON/IccLibJSON/IccProfileJson.cpp  "sig": "..."
#
# The live tag registry (registry.color.org/tag-signatures, checked 2026-08-11)
# opens six of its assigned ranges on such a signature -- 44555000 and 64757000
# (Dupont), 544B0000, 546B0000, 744B0000 and 746B0000 (Tektronix) -- so an
# ICC -> XML -> ICC or ICC -> JSON -> ICC trip silently rewrote the tag-table
# identifier of a conformant profile.  The damage is invisible
# to the validator -- 0x44555020 is merely an *unregistered* signature, not a
# malformed one -- so every assertion here is a byte comparison against the
# profile the round trip started from, not a validation report.
#
# NOT covered here, deliberately: a signature that is zero in *all four* bytes.
# icGetSigStr(0) returns the literal text "NULL", which is a display convention
# the call sites work around by writing an empty value for zero; that is the
# #1356 / #1361 / #1843 family and has its own three regressions.  This test is
# about the partially-zero signatures, which no call-site guard can reach.
#
# Section 4 is the anti-overfit assertion: an ordinary printable signature must
# still serialize as readable text.  Forcing every signature to hex would make
# sections 1-3 pass while making every profile on disk unreadable.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2097-nul-signature-roundtrip}"
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

TOXML="$(find_tool iccToXml)"
FROMXML="$(find_tool iccFromXml)"
TOJSON="$(find_tool iccToJson)"
FROMJSON="$(find_tool iccFromJson)"

SRC_XML="$REPO_ROOT/Testing/Encoding/ISO22028-Encoded-sRGB.xml"

fail() {
  echo "  [FAIL] issue-2097-nul-signature-roundtrip -- $1"
  exit 1
}

echo "=== Trailing-NUL signature XML/JSON round-trip regression (issue #2097) ==="

for tool in "$TOXML" "$FROMXML" "$TOJSON" "$FROMJSON"; do
  if [ -z "$tool" ] || [ ! -x "$tool" ]; then
    echo "  [SKIP] XML/JSON round-trip tools not all built under $TOOLS_DIR"
    exit 0
  fi
done

if [ ! -f "$SRC_XML" ]; then
  echo "  [SKIP] missing fixture $SRC_XML"
  exit 0
fi

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
# make_profile <hex-signature> <output.icc>
#
# Adds one extra signatureType tag to the tracked ISO 22028 fixture, carrying
# the requested tag-table identifier.  The reader takes the "%08Xh" form through
# icGetSigVal()'s hexadecimal path, so the profile on disk holds those exact four
# bytes whether or not the writer can render them -- which is what makes the
# comparisons below a test of the writer alone.
# ---------------------------------------------------------------------------
make_profile() {
  local sig="$1" out="$2"
  local xml="$OUTDIR/in-$sig.xml"

  awk -v sig="$sig" '
    /<\/Tags>/ && !done {
      printf "\t\t<signatureType>\n"
      printf "\t\t\t<TagSignature>%sh</TagSignature>\n", sig
      printf "\t\t\t<Signature>dorc</Signature>\n"
      printf "\t\t</signatureType>\n"
      done = 1
    }
    { print }
  ' "$SRC_XML" > "$xml"

  grep -q "<TagSignature>${sig}h</TagSignature>" "$xml" \
    || fail "could not inject signature $sig into the fixture"

  run "build-$sig" "$FROMXML" "$xml" "$out"
}

# ---------------------------------------------------------------------------
# assert_roundtrip <label> <base.icc> <roundtripped.icc>
#
# The only assertion that catches this defect.  One trailing zero moves a single
# byte (0x44555000 -> 0x44555020, measured at offset 172 of the fixture below);
# two trailing zeros move two.  Either way both profiles report "Profile is valid
# for version 5.00", because the rewritten signature is unregistered rather than
# malformed -- so a validation report is not an assertion, and the bytes are.
# ---------------------------------------------------------------------------
assert_roundtrip() {
  local what="$1" base="$2" rt="$3"

  if ! cmp -s "$base" "$rt"; then
    cmp -l "$base" "$rt" 2>/dev/null | sed -n '1,6p' | sed 's/^/    /'
    fail "$what changed the bytes of a profile with a trailing-NUL signature (#2097)"
  fi
}

# ===========================================================================
# 1. The reported example, both lanes.  0x44555000 is a registered private tag
#    signature; "DUP" in the serialized text is the corruption in flight.
# ===========================================================================
make_profile 44555000 "$OUTDIR/dup.icc"

run dup-toxml "$TOXML" "$OUTDIR/dup.icc" "$OUTDIR/dup.xml"
if grep -qF 'TagSignature="DUP"' "$OUTDIR/dup.xml"; then
  grep -nF 'TagSignature="DUP"' "$OUTDIR/dup.xml" | sed -n '1,2p' | sed 's/^/    /'
  fail "the XML writer truncated 'DUP\\0' to the three characters DUP (#2097)"
fi
grep -qF 'TagSignature="44555000h"' "$OUTDIR/dup.xml" \
  || fail "the XML writer did not escape the trailing-NUL signature as 44555000h (#2097)"

run dup-tojson "$TOJSON" "$OUTDIR/dup.icc" "$OUTDIR/dup.json"
if grep -qF '"sig": "DUP"' "$OUTDIR/dup.json"; then
  grep -nF '"sig": "DUP"' "$OUTDIR/dup.json" | sed -n '1,2p' | sed 's/^/    /'
  fail "the JSON writer truncated 'DUP\\0' to the three characters DUP (#2097)"
fi
grep -qF '"sig": "44555000h"' "$OUTDIR/dup.json" \
  || fail "the JSON writer did not escape the trailing-NUL signature as 44555000h (#2097)"

run dup-fromxml  "$FROMXML"  "$OUTDIR/dup.xml"  "$OUTDIR/dup-rt-xml.icc"
run dup-fromjson "$FROMJSON" "$OUTDIR/dup.json" "$OUTDIR/dup-rt-json.icc"

assert_roundtrip "an ICC -> XML -> ICC round trip"  "$OUTDIR/dup.icc" "$OUTDIR/dup-rt-xml.icc"
assert_roundtrip "an ICC -> JSON -> ICC round trip" "$OUTDIR/dup.icc" "$OUTDIR/dup-rt-json.icc"
echo "    0x44555000 (DUP + NUL): both round trips byte-exact, both formats escaped"

# ===========================================================================
# 2. The rest of the family.  These are the registry signatures the sweep in
#    #2097 measured as changing; one lane each is enough once section 1 has
#    shown the two writers behave alike.
# ===========================================================================
for sig in 544B0000 546B0000 64757000 744B0000 746B0000; do
  make_profile "$sig" "$OUTDIR/$sig.icc"
  run "$sig-toxml"   "$TOXML"   "$OUTDIR/$sig.icc" "$OUTDIR/$sig.xml"
  run "$sig-fromxml" "$FROMXML" "$OUTDIR/$sig.xml" "$OUTDIR/$sig-rt.icc"
  assert_roundtrip "an ICC -> XML -> ICC round trip of $sig" "$OUTDIR/$sig.icc" "$OUTDIR/$sig-rt.icc"
done
echo "    the five remaining registry signatures ending in NUL: byte-exact through XML"

# ===========================================================================
# 3. The cross-format lane #2097 reported: a profile written by one text format
#    and reparsed by the other.  Each hop is a separate opportunity to truncate.
# ===========================================================================
run cross-toxml   "$TOXML"   "$OUTDIR/dup-rt-json.icc" "$OUTDIR/cross.xml"
run cross-fromxml "$FROMXML" "$OUTDIR/cross.xml"       "$OUTDIR/cross.icc"
assert_roundtrip "the JSON -> ICC -> XML -> ICC lane" "$OUTDIR/dup.icc" "$OUTDIR/cross.icc"
echo "    JSON -> ICC -> XML -> ICC: byte-exact"

# ===========================================================================
# 4. Anti-overfit.  Escaping *every* signature to hex would satisfy sections
#    1-3 and make every serialized profile unreadable, so assert that an
#    ordinary printable signature is still written as its four characters.
# ===========================================================================
make_profile 44555058 "$OUTDIR/plain.icc"
run plain-toxml "$TOXML" "$OUTDIR/plain.icc" "$OUTDIR/plain.xml"
grep -qF 'TagSignature="DUPX"' "$OUTDIR/plain.xml" \
  || fail "a printable tag-table identifier stopped serializing as readable text (#2097)"
grep -qF "<Signature>dorc</Signature>" "$OUTDIR/plain.xml" \
  || fail "a printable payload signature stopped serializing as readable text (#2097)"
run plain-fromxml "$FROMXML" "$OUTDIR/plain.xml" "$OUTDIR/plain-rt.icc"
assert_roundtrip "an ICC -> XML -> ICC round trip of a printable signature" \
  "$OUTDIR/plain.icc" "$OUTDIR/plain-rt.icc"
echo "    printable signatures still serialize as text, not hex, and still round-trip"

echo "  [PASS] issue-2097-nul-signature-roundtrip -- 6 trailing-NUL signatures survive XML and JSON"
exit 0
