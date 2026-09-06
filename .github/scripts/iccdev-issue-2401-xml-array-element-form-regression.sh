#!/bin/bash
###############################################################################
# #2401 -- the two spellings of an XML array body must parse identically
###############################################################################
#
# An <Array>/<Data> body may be written as text ("256 255 300 -1") or as a run
# of <n> elements.  They were parsed by two different conversions:
# CIccXmlArrayType::ParseText() used clipTypeRange<T>(atof(...)) while
# ParseArray()'s element branch cast atol() straight to T.  So one array had two
# meanings depending only on how it was spelled, and each conversion was wrong
# somewhere the other was right.  Measured on f186948c:
#
#   uInt8Array      input  256   300   -1
#     element <n>          0     44    255     (wrapped)
#     text                 255   255   0       (clipped)
#
#   float32Array    input  0.5   -2.25
#     element <n>          0.0   -2.0          (atol() dropped the fraction)
#     text                 0.5   -2.25
#
#   float32Array    input  nan   1.5
#     element <n>          0.0   1.0
#     text                 nan   1.5
#
#   uInt64Array     input  2^53+1              UINT64_MAX
#     element <n>          exact               9223372036854775807 (atol saturates at LLONG_MAX)
#     text                 9007199254740992    exact
#
# Note the last row: the element form was NOT simply the worse of the two.  Each
# spelling was exact where the other lost data, which is why the fix is a single
# shared conversion (icParseArrayValue) rather than making one call the other's
# old code -- that would have fixed one column and regressed the other.
#
# Assertions are made on the RAW TAG BYTES in the .icc, not on a round-trip
# through iccToXml.  That matters: DumpArray() has
# "case icSigUInt64ArrayType: // unused" and zeroes non-finite floats, so a
# round-trip cannot observe the uInt64 or the NaN case at all -- a test written
# against the XML dump is structurally blind to exactly the two rows above that
# were hardest to get right.
#
# Each case asserts BOTH that the two spellings agree AND what they equal.
# Agreement alone would stay green if both paths regressed the same way; the
# absolute values alone would not express the invariant.
#
# Red/green: all four cases fail against f186948c.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-xml-array-element-form-regression}"
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"

PASS=0
FAIL=0
TOTAL=0

fail_case() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
pass_case() { echo "  [PASS] $1 -- $2"; PASS=$((PASS + 1)); }

if [ ! -x "$FROMXML" ]; then
  echo "iccFromXml not found at $FROMXML -- skipping (build Tools first)"
  exit 77
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not available -- skipping (needed to read tag bytes out of the .icc)"
  exit 77
fi

# Decode one tag's payload from a profile.  Walks the tag table rather than
# assuming an offset, skips the 8-byte type signature + reserved header, and
# prints the values space-separated so the caller can compare them as a string.
read_tag_values() {
  python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import struct, sys, math
path, want, fmt, count = sys.argv[1], sys.argv[2].encode(), sys.argv[3], int(sys.argv[4])
data = open(path, 'rb').read()
ntags = struct.unpack('>I', data[128:132])[0]
for i in range(ntags):
    sig, off, size = struct.unpack('>4sII', data[132 + i * 12:144 + i * 12])
    if sig == want:
        vals = struct.unpack('>%d%s' % (count, fmt), data[off + 8:off + 8 + count * struct.calcsize('>' + fmt)])
        print(' '.join('nan' if isinstance(v, float) and math.isnan(v) else
                       ('%g' % v if isinstance(v, float) else str(v)) for v in vals))
        sys.exit(0)
sys.exit('tag %s not found' % want.decode())
PYEOF
}

# Raw payload of one tag, as hex, for cases whose values are buried in a
# structure (the MPE matrix below) rather than a flat array.
read_tag_hex() {
  python3 - "$1" "$2" <<'PYEOF2'
import struct, sys
data = open(sys.argv[1], 'rb').read()
want = sys.argv[2].encode()
ntags = struct.unpack('>I', data[128:132])[0]
for i in range(ntags):
    sig, off, size = struct.unpack('>4sII', data[132 + i * 12:144 + i * 12])
    if sig == want:
        print(data[off:off + size].hex())
        sys.exit(0)
sys.exit('tag %s not found' % want.decode())
PYEOF2
}

emit_profile() {
  local out="$1" type="$2" container="$3" elem_body="$4" text_body="$5"
  cat > "$out" <<XMLEOF
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <PreferredCMMType>ICCD</PreferredCMMType>
    <ProfileVersion>4.30</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
    <CreationDateTime>2026-08-20T00:00:00</CreationDateTime>
    <ProfileFlags EmbeddedInFile="false" UseWithEmbeddedDataOnly="false"/>
    <DeviceAttributes ReflectiveOrTransparency="reflective" GlossyOrMatte="glossy" MediaPolarity="positive" MediaColour="colour"/>
    <RenderingIntent>Perceptual</RenderingIntent>
    <PCSIlluminant>
      <XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/>
    </PCSIlluminant>
    <ProfileCreator>ICCD</ProfileCreator>
  </Header>
  <Tags>
    <PrivateTag TagSignature="ELEM"> <$type>
      <$container>$elem_body</$container>
    </$type> </PrivateTag>
    <PrivateTag TagSignature="TEXT"> <$type>
      <$container>$text_body</$container>
    </$type> </PrivateTag>
    <grayTRCTag> <curveType>
      <Curve>563</Curve>
    </curveType> </grayTRCTag>
    <profileDescriptionTag> <multiLocalizedUnicodeType>
      <LocalizedText LanguageCountry="enUS">#2401 element-form array fixture</LocalizedText>
    </multiLocalizedUnicodeType> </profileDescriptionTag>
    <copyrightTag> <multiLocalizedUnicodeType>
      <LocalizedText LanguageCountry="enUS">ICC regression fixture</LocalizedText>
    </multiLocalizedUnicodeType> </copyrightTag>
    <mediaWhitePointTag> <XYZArrayType>
      <XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/>
    </XYZArrayType> </mediaWhitePointTag>
  </Tags>
</IccProfile>
XMLEOF
}

# name | xml type | container | struct fmt | value count | expected | element body | text body
run_case() {
  local name="$1" type="$2" container="$3" fmt="$4" count="$5" expected="$6" elem_body="$7" text_body="$8"
  TOTAL=$((TOTAL + 1))

  local xml="$OUTDIR/$name.xml" icc="$OUTDIR/$name.icc" log="$OUTDIR/$name.log"
  local rc=0

  emit_profile "$xml" "$type" "$container" "$elem_body" "$text_body"

  rm -f "$icc"
  # rc captured explicitly: inside "if ! cmd" the value of $? is the status of
  # the negation, which is always 0 when the command failed, so a diagnostic
  # built from it would report a nonsense status.
  timeout 25 "$FROMXML" "$xml" "$icc" > "$log" 2>&1 || rc=$?
  if [ "$rc" -ne 0 ]; then
    fail_case "$name" "iccFromXml refused the fixture (exit=$rc)"
    sed -n '1,10p' "$log"
    return
  fi

  local elem text
  elem="$(read_tag_values "$icc" ELEM "$fmt" "$count")" || {
    fail_case "$name" "could not read the element-form tag: $elem"; return; }
  text="$(read_tag_values "$icc" TEXT "$fmt" "$count")" || {
    fail_case "$name" "could not read the text-form tag: $text"; return; }

  if [ "$elem" != "$text" ]; then
    fail_case "$name" "element form '$elem' disagrees with text form '$text'"
    return
  fi
  # Agreement alone would survive both paths regressing identically, so pin the
  # value too.
  if [ "$elem" != "$expected" ]; then
    fail_case "$name" "both spellings agree on '$elem' but expected '$expected'"
    return
  fi

  pass_case "$name" "both spellings gave '$elem'"
}

echo "=== XML array element-form parity regression (#2401) ==="

# uInt8: above, at and below the 0..255 range.  Clipped, not wrapped.
run_case uint8-range uInt8NumberType Array B 4 "255 255 255 0" \
  "<n>256</n><n>255</n><n>300</n><n>-1</n>" "256 255 300 -1"

# float32: fractions atol() discarded.  The float types accept only <Data>:
# CIccTagXmlFloatNum::ParseXml looks for "Data" and does not fall back to
# "Array" the way CIccTagXmlNum::ParseXml does.
run_case float32-fraction float32NumberType Data f 3 "0.5 -2.25 3.75" \
  "<n>0.5</n><n>-2.25</n><n>3.75</n>" "0.5 -2.25 3.75"

# float32 NaN: the text form has always stored a real NaN for floating-point
# types; the element form must too.
run_case float32-nan float32NumberType Data f 2 "nan 1.5" \
  "<n>nan</n><n>1.5</n>" "nan 1.5"

# uInt64: the row where each spelling was exact where the other was not, so it
# pins both directions at once.
run_case uint64-exact uInt64NumberType Array Q 3 \
  "9007199254740993 1234567890123456789 18446744073709551615" \
  "<n>9007199254740993</n><n>1234567890123456789</n><n>18446744073709551615</n>" \
  "9007199254740993 1234567890123456789 18446744073709551615"

# Indented NaN.  Pretty-printed XML puts whitespace inside the element, and the
# NaN literal test must see past it -- when the whitespace skip lived inside the
# integer branch, "<n>\n  nan\n</n>" in a float array flushed to 0 while the
# unindented spelling gave a real NaN.
run_case float32-nan-indented float32NumberType Data f 2 "nan 1.5" \
  "<n>
      nan
    </n><n>1.5</n>" "nan 1.5"

# A float-shaped token in an integer array takes the atof() fallback.  It must
# saturate rather than convert out of range -- (double)UINT64_MAX rounds up to
# 2^64, so the bound has to be compared inclusively.
run_case uint64-float-token uInt64NumberType Array Q 2 \
  "18446744073709551615 1000" "<n>1.8446744073709552e19</n><n>1e3</n>" \
  "1.8446744073709552e19 1e3"

# The <f> spelling is the third way to write a float array body, and it had its
# own unchecked conversion: no clamp, so <f>1e50</f> stored +inf (0x7f800000)
# into an MPE matrix coefficient where the identical text array clipped to
# FLT_MAX (0x7f7fffff), and a non-finite coefficient propagates into transform
# output.  Only icSigFloatArrayType scans for "f" elements, and that type is not
# a tag type -- it backs MPE matrices, curves and CLUTs -- so this case needs a
# multiProcessElementType rather than the PrivateTag shape above.  The two tags
# are identical apart from the spelling, so their payloads must be byte-equal.
TOTAL=$((TOTAL + 1))
MPE="$OUTDIR/mpe-f-element.xml"
cat > "$MPE" <<'XMLEOF'
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <PreferredCMMType>ICCD</PreferredCMMType>
    <ProfileVersion>5.00</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>RGB </DataColourSpace>
    <PCS>XYZ </PCS>
    <CreationDateTime>2026-08-20T00:00:00</CreationDateTime>
    <ProfileFlags EmbeddedInFile="false" UseWithEmbeddedDataOnly="false"/>
    <DeviceAttributes ReflectiveOrTransparency="reflective" GlossyOrMatte="glossy" MediaPolarity="positive" MediaColour="colour"/>
    <RenderingIntent>Perceptual</RenderingIntent>
    <PCSIlluminant>
      <XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/>
    </PCSIlluminant>
    <ProfileCreator>ICCD</ProfileCreator>
  </Header>
  <Tags>
    <AToB0Tag> <multiProcessElementType>
      <MultiProcessElements InputChannels="3" OutputChannels="3">
        <MatrixElement InputChannels="3" OutputChannels="3">
          <MatrixData><f>1e50</f><f>0</f><f>0</f><f>0</f><f>0.25</f><f>0</f><f>0</f><f>0</f><f>1</f></MatrixData>
        </MatrixElement>
      </MultiProcessElements>
    </multiProcessElementType> </AToB0Tag>
    <AToB1Tag> <multiProcessElementType>
      <MultiProcessElements InputChannels="3" OutputChannels="3">
        <MatrixElement InputChannels="3" OutputChannels="3">
          <MatrixData>1e50 0 0 0 0.25 0 0 0 1</MatrixData>
        </MatrixElement>
      </MultiProcessElements>
    </multiProcessElementType> </AToB1Tag>
    <profileDescriptionTag> <multiLocalizedUnicodeType>
      <LocalizedText LanguageCountry="enUS">#2401 f-element fixture</LocalizedText>
    </multiLocalizedUnicodeType> </profileDescriptionTag>
    <copyrightTag> <multiLocalizedUnicodeType>
      <LocalizedText LanguageCountry="enUS">ICC regression fixture</LocalizedText>
    </multiLocalizedUnicodeType> </copyrightTag>
    <mediaWhitePointTag> <XYZArrayType>
      <XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/>
    </XYZArrayType> </mediaWhitePointTag>
  </Tags>
</IccProfile>
XMLEOF
mpe_rc=0
timeout 25 "$FROMXML" "$MPE" "$OUTDIR/mpe-f-element.icc" > "$OUTDIR/mpe-f-element.log" 2>&1 || mpe_rc=$?
if [ "$mpe_rc" -ne 0 ]; then
  fail_case "floatarray-f-element" "iccFromXml refused the fixture (exit=$mpe_rc)"
  sed -n '1,10p' "$OUTDIR/mpe-f-element.log"
else
  f_hex="$(read_tag_hex "$OUTDIR/mpe-f-element.icc" 'A2B0')" || f_hex=""
  t_hex="$(read_tag_hex "$OUTDIR/mpe-f-element.icc" 'A2B1')" || t_hex=""
  if [ -z "$f_hex" ] || [ -z "$t_hex" ]; then
    fail_case "floatarray-f-element" "could not read both MPE tags back"
  elif [ "$f_hex" != "$t_hex" ]; then
    # 7f800000 is +inf, 7f7fffff is FLT_MAX -- name which one turned up so the
    # failure says what happened rather than dumping two hex blobs.
    case "$f_hex" in
      *7f800000*) fail_case "floatarray-f-element" "<f> spelling stored +inf where the text spelling clipped" ;;
      *)          fail_case "floatarray-f-element" "<f> and text spellings produced different MPE payloads" ;;
    esac
  elif [ "${f_hex#*7f7fffff}" = "$f_hex" ]; then
    fail_case "floatarray-f-element" "neither spelling clipped 1e50 to FLT_MAX"
  else
    pass_case "floatarray-f-element" "<f> and text spellings both clipped 1e50 to FLT_MAX"
  fi
fi

###############################################################################
# Acceptance parity (#2401 follow-up)
#
# The conversion was unified above; what stayed different was ACCEPTANCE.  The
# text form decides a body has values in it with ParseTextCount(), and rejects
# the whole array when it counts none.  The element form applied no such test:
# any <n>/<f> body was taken as a value.  Both spellings now go through
# ParseTextCount(), so the pairs below must agree on accept-vs-reject.
#
# The empty-element case is the sharp one, and it is a defect rather than an
# inconsistency.  icXmlNodeCount() counted <n></n>, the write loop's
# `children && content` guard skipped it, and SetSize()'s buffer comes from
# malloc() with no zeroing -- so the slot it stood for was serialized into the
# .icc never having been written, and the parse still reported success.  Under
# MALLOC_PERTURB_=42 glibc fills fresh allocations with 0xd5, which is what the
# assertion looks for: on the unfixed build a 48-element array with 47 empty
# elements writes 47 bytes of 0xd5 to disk.
#
# Deliberately NOT tightened past parity: "abc" and "Silver" contain characters
# icIsNumChar() accepts ('a', 'e'), so the text form takes them as one token
# worth 0 and the element form must too.  Testing/hybrid's three Overprint
# profiles carry <n>Silver</n> and <n>ProcessBlack</n> in a colorantOrderTag,
# which reaches this same CIccUInt8Array::ParseArray -- rejecting non-numeric
# bodies outright would refuse three tracked corpus profiles.  Those two cases
# are here to hold that line.
###############################################################################

emit_single() {
  # Both tags carry the same body, so a rejection is attributable to that body
  # rather than to whichever of the two spellings happened to be paired with it.
  emit_profile "$1" uInt8NumberType Array "$2" "$2"
}

# name | body | expected: "accept" or "reject" -- asserted for BOTH spellings
run_acceptance_case() {
  local name="$1" elem_body="$2" text_body="$3" want="$4"
  TOTAL=$((TOTAL + 1))

  local e_rc=0 t_rc=0
  emit_single "$OUTDIR/$name-elem.xml" "$elem_body"
  emit_single "$OUTDIR/$name-text.xml" "$text_body"
  rm -f "$OUTDIR/$name-elem.icc" "$OUTDIR/$name-text.icc"
  timeout 25 "$FROMXML" "$OUTDIR/$name-elem.xml" "$OUTDIR/$name-elem.icc" \
    > "$OUTDIR/$name-elem.log" 2>&1 || e_rc=$?
  timeout 25 "$FROMXML" "$OUTDIR/$name-text.xml" "$OUTDIR/$name-text.icc" \
    > "$OUTDIR/$name-text.log" 2>&1 || t_rc=$?

  local e_got t_got
  [ "$e_rc" -eq 0 ] && e_got=accept || e_got=reject
  [ "$t_rc" -eq 0 ] && t_got=accept || t_got=reject

  if [ "$e_got" != "$t_got" ]; then
    fail_case "$name" "element form $e_got(s) while text form $t_got(s) -- the parity this fixes"
    return
  fi
  # Agreement alone would be satisfied by both spellings regressing together, so
  # pin which side of the line the body falls on.
  if [ "$e_got" != "$want" ]; then
    fail_case "$name" "both spellings $e_got, expected both to $want"
    return
  fi
  pass_case "$name" "both spellings $want -- agreed"
}

# Element-form-only assertion.  Used where the two spellings are NOT twins: a
# multi-token element body has no text equivalent with the same array length, so
# parity is the wrong question -- the contract is that the element form refuses
# rather than filling one slot and dropping the rest.
run_element_reject_case() {
  local name="$1" elem_body="$2"
  TOTAL=$((TOTAL + 1))

  local rc=0
  emit_single "$OUTDIR/$name-elem.xml" "$elem_body"
  rm -f "$OUTDIR/$name-elem.icc"
  timeout 25 "$FROMXML" "$OUTDIR/$name-elem.xml" "$OUTDIR/$name-elem.icc" \
    > "$OUTDIR/$name-elem.log" 2>&1 || rc=$?

  if [ "$rc" -eq 0 ]; then
    fail_case "$name" "element form accepted a body carrying more than one value"
    return
  fi
  pass_case "$name" "element form refused a multi-value body (exit=$rc)"
}

echo "--- acceptance parity ---"

# No character icIsNumChar() accepts anywhere in the body: the text form has
# always refused these, and the element form now does too.
#
# The bodies compared here are single-token on purpose.  A MIXED body such as
# "zz 2" is not a like-for-like twin: ParseTextCount() does not reject it, it
# simply produces no token for the "zz" run, so the text form yields a
# one-element array while <n>zz</n><n>2</n> is a two-element array with one bad
# element.  Those are different inputs, not the same input parsed two ways.
run_acceptance_case nonnumeric-zz  "<n>zz</n>"          "zz"     reject
run_acceptance_case nonnumeric-pun "<n>!!!</n>"         "!!!"    reject

# Parity is the rule, not "reject anything that is not a number": both of these
# carry a character icIsNumChar() accepts, so both spellings take them as a
# token worth 0.  Testing/hybrid's colorantOrderTag depends on this.
run_acceptance_case numchar-abc    "<n>abc</n>"         "abc"    accept
run_acceptance_case numchar-silver "<n>Silver</n>"      "Silver" accept

# One element is one slot.  icParseArrayValue() converts the first token in a
# body and ignores the rest, so <n>10 20 30</n> filled one slot and dropped two
# values -- the array came out SHORTER than the identical text spelling, which is
# the same count-vs-values mismatch the empty element produces at the other end.
# Measured before the fix: <n>10 20 30</n><n>40</n> gave [10, 40] where the text
# body "10 20 30 40" gave [10, 20, 30, 40], and iccFromXml exited 0.
#
# The test for this is whitespace tokens, NOT ParseTextCount()==1: that counter
# treats 'a', 'e' and 'n' as numeric so "nan" survives, which makes
# "ProcessBlack" two tokens and "ProcessMagenta" four -- and Testing/hybrid's
# three Overprint profiles put exactly those in a colorantOrderTag.  Counting
# with it here would refuse tracked corpus profiles; the last case below pins
# that.
run_element_reject_case multi-token     "<n>10 20 30</n><n>40</n>"
run_element_reject_case multi-token-one "<n>10 20</n>"
run_acceptance_case     corpus-colorant "<n>ProcessBlack</n>" "ProcessBlack" accept

# An empty element is counted but yields nothing, so it must reject rather than
# leave the slot it reserved unwritten.
run_acceptance_case empty-element  "<n></n><n>2</n>"    ""       reject
run_acceptance_case selfclosing    "<n/><n>2</n>"       ""       reject

# The uninitialised read itself, observed rather than inferred.  47 empty
# elements and one real value: on the unfixed build this is accepted and the 47
# unwritten bytes reach the file as MALLOC_PERTURB_'s fill pattern.
TOTAL=$((TOTAL + 1))
perturb_body=""
i=0
while [ "$i" -lt 47 ]; do perturb_body="$perturb_body<n></n>"; i=$((i + 1)); done
perturb_body="$perturb_body<n>170</n>"
emit_single "$OUTDIR/perturb.xml" "$perturb_body"
rm -f "$OUTDIR/perturb.icc"
perturb_rc=0
MALLOC_PERTURB_=42 timeout 25 "$FROMXML" "$OUTDIR/perturb.xml" "$OUTDIR/perturb.icc" \
  > "$OUTDIR/perturb.log" 2>&1 || perturb_rc=$?
if [ "$perturb_rc" -ne 0 ]; then
  pass_case "uninitialised-slots" "array with unwritten slots refused (exit=$perturb_rc)"
elif [ ! -f "$OUTDIR/perturb.icc" ]; then
  fail_case "uninitialised-slots" "iccFromXml reported success but wrote no profile"
else
  perturb_hex="$(read_tag_values "$OUTDIR/perturb.icc" ELEM B 48)" || perturb_hex=""
  # 0xd5 == 42 ^ 0xff, glibc's fill for a freshly malloc()ed block.
  if printf '%s\n' "$perturb_hex" | grep -qw 213; then
    fail_case "uninitialised-slots" \
      "accepted, and unwritten slots reached the .icc as MALLOC_PERTURB_ fill: $perturb_hex"
  else
    fail_case "uninitialised-slots" \
      "accepted an array whose slots were never all written: $perturb_hex"
  fi
fi

echo "=== summary: $PASS passed, $FAIL failed, $TOTAL total ==="

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
if [ "$PASS" -eq 0 ]; then
  echo "no case was measured -- treating as skipped rather than passed"
  exit 77
fi
exit 0
