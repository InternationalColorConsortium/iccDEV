#!/bin/bash
###############################################################################
# iccDEV XML parser regression tests
###############################################################################
#
# Issue #1856. Two things are pinned here, and they pull in opposite
# directions, which is why they share a script:
#
#   1. iccDEV must be able to read back the XML it just wrote. A profile whose
#      CLUT serialises past libxml2's XML_MAX_TEXT_LENGTH (10 MB) must be split
#      into bounded text nodes, so iccToXml never emits a document iccFromXml
#      rejects.
#
#   2. Relaxing that must not widen what the parser accepts: the nesting-depth
#      and name-length caps have to stay armed. The hardening cases below fail
#      if a later change lets either of them go.
#
# The fix for 1 reads the document into one contiguous buffer and parses that,
# and bounds the whole document instead (icXmlMaxTextFileBytes).
#
# Issue #2108, part 1 of 2 -- the harness only; the reader is not changed here.
#
# Two separate things were wrong with the hardening half above.
#
#   a. It asserted only that iccFromXml exited non-zero. None of its fixtures is
#      a valid ICC profile, so every one of them exits non-zero whatever the
#      parser did -- a well-formed <IccProfile></IccProfile> exits non-zero too.
#      Measured: with XML_PARSE_HUGE applied to the reader and the depth and
#      name caps therefore off, this script still reported 5/5 passed and exit
#      0, which is precisely the regression case 2 was written to catch.
#
#      So a refusal now has to be attributable to the parser. That puts one
#      requirement on the reader: when it refuses a document itself rather than
#      letting libxml2 refuse it, it has to say so, the way the document-size
#      bound below already does. A silent NULL is indistinguishable here from
#      "parsed fine, but is not a profile".
#
#   b. libxml2 v2.13.0 moved the XML_MAX_TEXT_LENGTH check into xmlSAX2Text
#      ("SAX2: Enforce size limit in xmlSAX2Text with XML_PARSE_HUGE"), so the
#      writer splits CLUT rows into text nodes below that cap rather than
#      weakening the reader with XML_PARSE_HUGE.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:-Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-xml-parser-regressions}"
mkdir -p "$OUTDIR"

# Absolute paths before any tool path is derived from them: the round-trip case
# runs tools from Testing/ so that a profile's sibling data files resolve.
if [ -d "$TOOLS_DIR" ]; then
  TOOLS_DIR="$(cd "$TOOLS_DIR" && pwd)"
fi
if [ -d "$TESTING_DIR" ]; then
  TESTING_DIR="$(cd "$TESTING_DIR" && pwd)"
fi

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
TOXML="$TOOLS_DIR/IccToXml/iccToXml"
APPLYTOLINK="$TOOLS_DIR/IccApplyToLink/iccApplyToLink"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
SKIPPED=0
TOTAL=0

# The document bound the XML reader was built with, in MiB. Kept in step with
# icXmlMaxTextFileBytes in IccXML/IccLibXML/IccUtilXml.h so that a build which
# retunes the constant reports a skip rather than a spurious failure.
XML_MAX_FILE_MB="${ICC_XML_MAX_FILE_MB:-256}"

# libxml2's per-text-node limit on the streaming path. The round-trip fixture
# below is only meaningful if it produces a node larger than this.
XML_MAX_TEXT_LENGTH=10000000

pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1)); }
fail() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1)); }
skip() { echo "  [SKIP] $1 -- $2"; SKIPPED=$((SKIPPED + 1)); TOTAL=$((TOTAL + 1)); }

check_sanitizers() {
  local name="$1" logfile="$2"
  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    fail "$name" "AddressSanitizer finding"
    return 1
  fi
  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    fail "$name" "undefined behavior"
    return 1
  fi
  return 0
}

###############################################################################
# 1. Round-trip a profile whose XML exceeds the text-node cap
###############################################################################

echo "=== Large text node round-trip (issue #1856) ==="

roundtrip_large_text_node() {
  local name="xml-large-text-node-roundtrip"
  local src="$TESTING_DIR/CMYK-3DLUTs/CMYK-3DLUTs.icc"
  local dst="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
  local link="$OUTDIR/link33.icc"
  local xml="$OUTDIR/link33.xml"
  local back="$OUTDIR/link33-roundtrip.icc"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$APPLYTOLINK" ] || [ ! -x "$TOXML" ] || [ ! -x "$FROMXML" ]; then
    skip "$name" "iccApplyToLink, iccToXml or iccFromXml not built"
    return
  fi
  if [ ! -f "$src" ] || [ ! -f "$dst" ]; then
    skip "$name" "source profiles missing (run CreateAllProfiles.sh)"
    return
  fi

  # A grid-33 CMYK->RGB device link is the cheapest way to get a CLUT whose
  # numeric text exceeds 10 MB: 33^4 grid points x 3 channels is ~21 MB.
  if ! "$APPLYTOLINK" "$link" 0 33 0 "issue 1856 regression" 0 1 1 1 \
       "$src" 1 "$dst" 1 >"$logfile" 2>&1; then
    fail "$name" "iccApplyToLink failed to build the fixture link"
    return
  fi

  if ! "$TOXML" "$link" "$xml" >>"$logfile" 2>&1; then
    fail "$name" "iccToXml failed on the fixture link"
    return
  fi

  # Confirm the fixture exercises the limit while the writer keeps every text
  # node under it. If the document becomes small, the fixture no longer covers
  # the boundary; if a node exceeds the cap, current libxml2 rejects it.
  # Assign in a separate statement from the command substitution so the exit
  # status checked is python's and not the local builtin's -- otherwise a failed
  # measurement yields empty values and reports as a skip with a blank size,
  # silently retiring the one case that pins the boundary.
  local measured total longest splits
  if ! measured="$(python3 - "$xml" <<'PY'
import re, sys
data = open(sys.argv[1], "rb").read()
nodes = [len(m.group(1)) for m in re.finditer(rb">([^<]*)<", data)]
print(sum(nodes), max(nodes, default=0), data.count(b"<!-- TableData continuation -->"))
PY
)"; then
    fail "$name" "could not measure the text nodes in $xml"
    return
  fi
  read -r total longest splits <<<"$measured"

  if [ -z "$total" ] || [ "$total" -le "$XML_MAX_TEXT_LENGTH" ]; then
    skip "$name" "total CLUT text ${total:-unknown} bytes does not exceed $XML_MAX_TEXT_LENGTH"
    return
  fi
  if [ -z "$longest" ] || [ "$longest" -gt "$XML_MAX_TEXT_LENGTH" ]; then
    fail "$name" "longest text node ${longest:-unknown} bytes exceeds $XML_MAX_TEXT_LENGTH"
    return
  fi
  # The size bounds above are necessary but not sufficient: "total" counts every
  # indent and every unrelated element's text, so on its own it does not prove
  # the CLUT approached the cap. Requiring the writer to have actually split
  # ties the case to the mechanism under test, so removing the split turns this
  # red here rather than only at the iccFromXml call below.
  if [ -z "$splits" ] || [ "$splits" -lt 1 ]; then
    fail "$name" "writer emitted no text-node split for ${total} bytes of CLUT text"
    return
  fi

  if ! "$FROMXML" "$xml" "$back" >>"$logfile" 2>&1; then
    fail "$name" "iccFromXml refused ${total} bytes of CLUT text split into ${longest}-byte nodes"
    return
  fi
  check_sanitizers "$name" "$logfile" || return

  # Byte-compare, ignoring the two header fields that are expected to differ
  # between two writes: the creation dateTime (offsets 24-35) and the profile
  # ID / MD5 (offsets 84-99). Without masking these, a correct round trip
  # still compares unequal.
  if python3 - "$link" "$back" <<'PY'
import sys
def norm(path):
    b = bytearray(open(path, "rb").read())
    b[24:36] = b"\0" * 12
    b[84:100] = b"\0" * 16
    return bytes(b)
sys.exit(0 if norm(sys.argv[1]) == norm(sys.argv[2]) else 1)
PY
  then
    pass "$name (${total} bytes across ${splits} split(s), longest node ${longest} bytes, round-trip byte-identical)"
  else
    fail "$name" "round-trip profile differs from the original"
  fi
}

roundtrip_large_text_node

###############################################################################
# 2. Parser hardening must survive the fix
###############################################################################
#
# Each of these is refused by libxml2 only because XML_PARSE_HUGE is NOT set.
# If a later change sets it to solve a size problem, these turn red.

echo "=== Parser hardening still armed ==="

# A parser-level refusal, as opposed to "this parsed but is not a profile".
# libxml2 prefixes its own diagnostics with "parser error", which covers the
# cases it refuses on its own. The second alternative is for a reader that stops
# the parse itself: xmlStopParser sets no message, so a reader that enforces a
# limit in its own SAX callbacks has to append one, and this is the text it is
# expected to use.
PARSER_DIAG='parser error|exceeds the parser'"'"'s nesting-depth or name-length limit'

# What the responseCurveSet16Type parser says when it refuses the count itself, as
# opposed to iccFromXml refusing the profile for some later reason.
#
# Anchored on the diagnostics the fix actually emits.  The first version of this
# pattern carried a bare "responseCurveSet16Type" alternative, which subsumed the
# specific one and matched CIccProfileXml::ParseTag's generic
#   Unable to Parse "responseCurveSet16Type" (responseCurveSet16Type) Tag on line N
# -- a line printed for ANY failure inside the tag.  Measured against a document whose
# only defect is a channel-count mismatch: the generic line matched, so the attribution
# asserted nothing beyond "the tag failed" and a change that refused every
# responseCurveSet16Type would have passed all of these cases while the guards under
# test were gone.
RESP_TAG_DIAG='(Empty|Invalid) CountOfChannels in responseCurveSet16Type|Invalid Measurement (DeviceCode|Reserved) in responseCurveSet16Type'

# Proves the assertion in reject_case() is worth something: a well-formed
# document that violates no parser limit must be refused WITHOUT any parser
# diagnostic, because it fails later as an invalid profile. If this ever starts
# emitting one, every reject_case below has stopped discriminating.
control_no_parser_diagnostic() {
  local name="xml-control-wellformed-no-parser-error"
  local file="$OUTDIR/control.xml"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  printf '<IccProfile></IccProfile>' >"$file"

  if "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "an empty IccProfile document was accepted as a profile"
    return
  fi
  if grep -qE "$PARSER_DIAG" "$logfile"; then
    fail "$name" "a well-formed document produced a parser diagnostic"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (refused as a profile, not by the parser)"
}

# $4 selects how strictly the refusal is checked:
#   parser -- a parser-level diagnostic is required (the caps XML_PARSE_HUGE
#             raises, which is what this script exists to pin)
#   exit   -- exit status only, for a property the parser owns across every
#             supported libxml2 and which no iccDEV parser flag can change
reject_case() {
  local name="$1" file="$2" why="$3" strictness="${4:-parser}"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  if "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "$why was accepted"
    return
  fi
  # Exit status alone cannot carry this: none of these fixtures is a valid
  # profile, so iccFromXml fails on them either way. The parser has to say so.
  if [ "$strictness" = "parser" ] && ! grep -qE "$PARSER_DIAG" "$logfile"; then
    fail "$name" "$why was refused as an invalid profile, not by the parser"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name ($why refused)"
}

control_no_parser_diagnostic

# Nesting past libxml2's 256-level default depth cap.
python3 - "$OUTDIR/deep.xml" <<'PY'
import sys
open(sys.argv[1], "w").write("<IccProfile>" + "<n>" * 3000 + "</n>" * 3000 + "</IccProfile>")
PY
reject_case "xml-reject-deep-nesting" "$OUTDIR/deep.xml" "3000 levels of nesting"

# Element name past XML_MAX_NAME_LENGTH (50000).
python3 - "$OUTDIR/longname.xml" <<'PY'
import sys
open(sys.argv[1], "w").write("<IccProfile><" + "z" * 60000 + "/></IccProfile>")
PY
reject_case "xml-reject-long-name" "$OUTDIR/longname.xml" "a 60000-byte element name"

# Entity amplification. Bounded at 10^6 characters so a regression cannot
# exhaust memory on the machine running the suite.
python3 - "$OUTDIR/laughs.xml" <<'PY'
import sys
open(sys.argv[1], "w").write(
    '<?xml version="1.0"?>\n<!DOCTYPE IccProfile [\n'
    '<!ENTITY a "AAAAAAAAAA">\n'
    '<!ENTITY b "&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;">\n'
    '<!ENTITY c "&b;&b;&b;&b;&b;&b;&b;&b;&b;&b;">\n'
    '<!ENTITY d "&c;&c;&c;&c;&c;&c;&c;&c;&c;&c;">\n'
    '<!ENTITY e "&d;&d;&d;&d;&d;&d;&d;&d;&d;&d;">\n'
    '<!ENTITY f "&e;&e;&e;&e;&e;&e;&e;&e;&e;&e;">\n'
    ']>\n<IccProfile>&f;</IccProfile>\n')
PY
#
# Checked on exit status only, deliberately. Measured against libxml2 2.9.14,
# 2.13.8 and 2.15.2, this document's fate does not depend on XML_PARSE_HUGE on
# any of them: 2.13 and 2.15 refuse it either way, and 2.9 accepts it either
# way, because without XML_PARSE_NOENT -- which iccDEV does not set -- entity
# content is not expanded and older libxml2 does not account for it. So this is
# a property libxml2 owns and no parser flag of ours moves, and requiring a
# parser diagnostic here would only encode the linked libxml2's version. Kept as
# a coverage case; tightening it needs a decision on the minimum supported
# libxml2 rather than a change to this script.
reject_case "xml-reject-entity-amplification" "$OUTDIR/laughs.xml" "a billion-laughs document" exit

###############################################################################
# 3. The whole-document bound replaces the per-node one
###############################################################################

echo "=== Document size bound ==="

oversize_document() {
  local name="xml-reject-oversize-document"
  local file="$OUTDIR/oversize.xml"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  # Needs a little over the configured bound on disk; skip rather than fail on
  # a constrained runner.
  local need_mb=$((XML_MAX_FILE_MB + 8))
  local avail_mb
  avail_mb="$(df -Pm "$OUTDIR" 2>/dev/null | awk 'NR==2 {print $4}')"
  if [ -z "$avail_mb" ] || [ "$avail_mb" -lt $((need_mb + 256)) ]; then
    skip "$name" "needs ${need_mb} MiB free in $OUTDIR, has ${avail_mb:-unknown}"
    return
  fi

  python3 - "$file" "$need_mb" <<'PY'
import sys
path, mb = sys.argv[1], int(sys.argv[2])
with open(path, "wb") as f:
    f.write(b"<IccProfile><Data>")
    chunk = b"7" * (1 << 20)
    for _ in range(mb):
        f.write(chunk)
    f.write(b"</Data></IccProfile>")
PY

  if "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "a ${need_mb} MiB document was accepted above the ${XML_MAX_FILE_MB} MiB bound"
    rm -f "$file"
    return
  fi

  # The rejection must say why. A bare parse failure here is what made the
  # original report hard to diagnose.
  if grep -q "exceeds ${XML_MAX_FILE_MB} MiB limit" "$logfile"; then
    pass "$name (refused with the compiled-in bound reported)"
  else
    fail "$name" "refused without naming the size limit"
  fi
  rm -f "$file"
}

oversize_document

###############################################################################
# 4. A refused array allocation must not be written through (issue #2106)
###############################################################################
#
# CIccTagXYZ::SetSize refuses more than 65536 entries, and on that path it frees
# the array, sets the pointer to NULL and returns false. CIccTagXmlXYZ::ParseXml
# ignored the result and wrote m_XYZ[0..n-1] anyway, so a document holding one
# entry more than the cap segfaulted iccFromXml. Nothing about it is exotic: the
# fixture below is ordinary well-formed XML, under 3 MB, that violates no parser
# limit -- which is why it belongs beside the hardening cases above rather than
# among them. Neither libxml2 nor the document-size bound stops it.
#
# The assertion is not "exit non-zero". Every fixture in this script exits
# non-zero, and a process killed inside ParseXml exits non-zero too. It is that
# iccFromXml reaches its OWN error path and names the tag it could not parse:
# that line is printed by the caller after ParseXml returns, so a crash cannot
# produce it. Measured before the fix, the log was empty and the shell reported
# signal 11.

echo "=== Array sizing refusals are not written through ==="

# Kept in step with CIccTagXYZ::SetSize in IccProfLib/IccTagBasic.cpp. A build
# that retunes the cap makes the over-cap fixture parse successfully, which is
# reported as a skip below rather than as a failure.
XYZ_MAX_ENTRIES="${ICC_XYZ_MAX_ENTRIES:-65536}"

# The diagnostic CIccProfileXml prints when a tag's ParseXml returns false.
XYZ_TAG_DIAG='Unable to Parse .*XYZType.* Tag'

write_xyz_document() {
  python3 - "$1" "$2" <<'PY'
import sys
path, n = sys.argv[1], int(sys.argv[2])
with open(path, "w") as f:
    f.write('<IccProfile>\n  <Header>\n'
            '    <ProfileVersion>4.30</ProfileVersion>\n'
            '    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n'
            '    <DataColourSpace>RGB </DataColourSpace>\n'
            '    <PCS>XYZ </PCS>\n'
            '    <RenderingIntent>Perceptual</RenderingIntent>\n'
            '    <PCSIlluminant>\n'
            '      <XYZNumber X="0.9642" Y="1.0" Z="0.8249"/>\n'
            '    </PCSIlluminant>\n'
            '  </Header>\n  <Tags>\n    <XYZType>\n'
            '      <TagSignature>wtpt</TagSignature>\n')
    f.write('      <XYZNumber X="0.5" Y="0.5" Z="0.5"/>\n' * n)
    f.write('    </XYZType>\n  </Tags>\n</IccProfile>\n')
PY
}

xyz_array_over_cap() {
  local name="xml-xyz-array-over-cap-not-written-through"
  local file="$OUTDIR/xyz-over-cap.xml"
  local logfile="$OUTDIR/${name}.log"
  local status

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  write_xyz_document "$file" $((XYZ_MAX_ENTRIES + 1))

  "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1
  status=$?

  # An accepted document means the compiled cap is not the one this case was
  # written against, so it is testing nothing. Report that rather than a
  # failure -- a reverted fix crashes here, it does not succeed.
  if [ "$status" -eq 0 ]; then
    skip "$name" "$((XYZ_MAX_ENTRIES + 1)) entries were accepted; the ${XYZ_MAX_ENTRIES}-entry cap has moved"
    rm -f "$file"
    return
  fi

  if ! grep -qE "$XYZ_TAG_DIAG" "$logfile"; then
    if [ "$status" -ge 128 ]; then
      fail "$name" "iccFromXml was killed by signal $((status - 128)) writing through a refused allocation"
    else
      fail "$name" "iccFromXml exited $status without reporting the tag it could not parse"
    fi
    rm -f "$file"
    return
  fi
  check_sanitizers "$name" "$logfile" || { rm -f "$file"; return; }
  pass "$name ($((XYZ_MAX_ENTRIES + 1)) entries refused at the tag, process intact)"
  rm -f "$file"
}

# The control for the case above. Without it, a change that made every XYZType
# tag fail to parse would leave the over-cap case passing while the tag stopped
# working entirely. This asserts the fixture sits exactly on the boundary: one
# entry fewer and the same tag must still parse.
xyz_array_at_cap() {
  local name="xml-xyz-array-at-cap-still-parses"
  local file="$OUTDIR/xyz-at-cap.xml"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  write_xyz_document "$file" "$XYZ_MAX_ENTRIES"

  # Exit status is deliberately not asserted: this fixture is a structurally
  # incomplete profile, so what iccFromXml returns for it is a question about its
  # validation policy, not about the tag. The tag is what is under test.
  "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1

  if grep -qE "$XYZ_TAG_DIAG" "$logfile"; then
    fail "$name" "the tag was refused at exactly $XYZ_MAX_ENTRIES entries, so the over-cap case proves nothing"
    rm -f "$file"
    return
  fi

  # The absence of the diagnostic cannot carry this on its own. If some later
  # change made iccFromXml give up earlier -- refusing the document while reading
  # the header, say -- the diagnostic would never be printed, this control would
  # stay green, and the over-cap case would silently lose the pairing that is the
  # only reason this control exists. So require positive evidence that the tool
  # got far enough to emit a profile. A validation policy that stops writing one
  # for this fixture turns this red, which is the correct outcome: the control is
  # no longer controlling anything and needs re-examining, not passing quietly.
  if [ ! -s "$OUTDIR/${name}.icc" ]; then
    fail "$name" "iccFromXml wrote no profile, so nothing proves it reached the tag at all"
    rm -f "$file"
    return
  fi
  check_sanitizers "$name" "$logfile" || { rm -f "$file"; return; }
  pass "$name ($XYZ_MAX_ENTRIES entries parsed, profile written)"
  rm -f "$file"
}

xyz_array_over_cap
xyz_array_at_cap

###############################################################################
# 5. responseCurveSet16Type CountOfChannels (#2397 null deref, #2398 wrap)
###############################################################################
#
# CIccTagXmlResponseCurveSet16::ParseXml checked that icXmlFindNode located the
# <CountOfChannels> element and then read pNode->children->content without
# checking the child.  An empty <CountOfChannels/> is well-formed XML with no
# text child, so libxml2 leaves children NULL: UBSan reported "member access
# within null pointer of type 'struct _xmlNode'" and a release build took a
# SIGSEGV (#2397).
#
# The same line used atoi() and stored into an icUInt32Number, while
# SetNumChannels takes an icUInt16Number.  "4294967297" is 0x100000001 --
# undefined for atoi() to begin with -- and truncated to 1, so the tool wrote a
# profile byte-identical to the one a count of 1 produces and reported success
# (#2398).  That equality is the assertion below, because exit status alone
# cannot distinguish "refused the count" from "refused the profile".
#
# No tracked document uses responseCurveSet16Type, which is why neither survived
# into a fixture; these three are written here.

write_countofchannels_document() {
  # write_countofchannels_document <path> <count-element>
  cat > "$1" <<XMLEOF
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <ProfileVersion>2.10</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
  </Header>
  <Tags>
    <responseCurveSet16Type>
      <TagSignature>resp</TagSignature>
      $2
      <ResponseCurve MeasUnitSignature="Status A">
        <ChannelResponses X="0" Y="0" Z="0">
          <Measurement DeviceCode="0" MeasValue="0"/>
        </ChannelResponses>
      </ResponseCurve>
    </responseCurveSet16Type>
  </Tags>
</IccProfile>
XMLEOF
}

countofchannels_empty_element() {
  local name="xml-responsecurve-empty-count"
  local file="$OUTDIR/resp-empty.xml" logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  write_countofchannels_document "$file" "<CountOfChannels/>"

  "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1
  local status=$?

  # The defect was a crash, so the assertion is on HOW it exits, not merely that
  # it does. A signal death surfaces as 128+signum through the shell (139 for
  # SIGSEGV), which is what this fixture produced before the child guard.
  if [ "$status" -ge 128 ]; then
    fail "$name" "iccFromXml died on a signal (exit $status) -- the null deref is back"
    return
  fi
  if [ "$status" -eq 0 ]; then
    fail "$name" "an empty CountOfChannels was accepted"
    return
  fi
  # Section 4's rule applies here: exit status alone is not attribution, because
  # every fixture in this script exits non-zero. Require the parser to name the tag
  # it refused, so a change that rejects this document earlier -- during header
  # parsing, or by refusing every responseCurveSet16Type -- goes red instead of
  # passing while the guard under test is gone.
  if ! grep -qE "$RESP_TAG_DIAG" "$logfile"; then
    fail "$name" "refused, but not by the responseCurveSet16Type parser"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (empty element refused by the tag parser, exit $status, no signal)"
}

countofchannels_overflow_is_not_one() {
  local name="xml-responsecurve-count-overflow"
  local over="$OUTDIR/resp-over.xml" ctrl="$OUTDIR/resp-ctrl.xml"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  write_countofchannels_document "$over" "<CountOfChannels>4294967297</CountOfChannels>"
  write_countofchannels_document "$ctrl" "<CountOfChannels>1</CountOfChannels>"

  rm -f "$OUTDIR/${name}-over.icc" "$OUTDIR/${name}-ctrl.icc"
  "$FROMXML" "$over" "$OUTDIR/${name}-over.icc" >"$logfile" 2>&1
  local over_status=$?
  "$FROMXML" "$ctrl" "$OUTDIR/${name}-ctrl.icc" >"$OUTDIR/${name}-ctrl.log" 2>&1
  local ctrl_status=$?

  # The control must still convert. Without it a fix that refuses every
  # responseCurveSet16Type document would pass this case while breaking the tag.
  if [ "$ctrl_status" -ne 0 ] || [ ! -s "$OUTDIR/${name}-ctrl.icc" ]; then
    fail "$name" "the count-of-1 control no longer converts (exit $ctrl_status)"
    return
  fi

  if [ "$over_status" -eq 0 ]; then
    # This is the shape the issue measured: both documents produced the same
    # bytes, so the tool silently agreed to a different document than the one
    # supplied. Name that explicitly rather than reporting a bare "accepted".
    if cmp -s "$OUTDIR/${name}-over.icc" "$OUTDIR/${name}-ctrl.icc"; then
      fail "$name" "4294967297 truncated to 1 -- output is byte-identical to the control"
    else
      fail "$name" "an out-of-range CountOfChannels was accepted"
    fi
    return
  fi

  if [ -s "$OUTDIR/${name}-over.icc" ]; then
    fail "$name" "a rejected count still left a profile behind"
    return
  fi

  if ! grep -qE "$RESP_TAG_DIAG" "$logfile"; then
    fail "$name" "refused, but not by the responseCurveSet16Type parser"
    return
  fi

  # Scan BOTH logs. The control is the run that actually reaches SetNumChannels and
  # allocates per-channel storage, so it is the interesting memory path; scanning
  # only the rejected run would let a finding on the accepted one sit unread while
  # this case reports PASS on the sanitizer lanes.
  check_sanitizers "$name" "$logfile" || return
  check_sanitizers "$name (control)" "$OUTDIR/${name}-ctrl.log" || return
  pass "$name (out-of-range count refused, control still converts)"
}

countofchannels_indented_is_accepted() {
  local name="xml-responsecurve-count-indented"
  local file="$OUTDIR/resp-indent.xml" logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  # Element text carries the document's own indentation. atoi() skipped it; a strict
  # helper that requires the whole string consumed does not, so tightening this parse
  # without trimming first would refuse an ordinary pretty-printed document -- the
  # same trap #2387 fixed for <ProfileVersion>. This case is the guard for that, and
  # it is the ONLY one here that asserts acceptance.
  write_countofchannels_document "$file" "$(printf '<CountOfChannels>\n        1\n      </CountOfChannels>')"

  if ! "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "a pretty-printed CountOfChannels was refused"
    return
  fi
  if [ ! -s "$OUTDIR/${name}.icc" ]; then
    fail "$name" "accepted but wrote no profile, so nothing proves it reached the tag"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (indented element text still parses)"
}

countofchannels_zero_refused() {
  local name="xml-responsecurve-count-zero"
  local file="$OUTDIR/resp-zero.xml" logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  # The JSON twin rejects CountOfChannels <= 0 and docs/icc-profile.schema.json
  # declares "minimum": 1, so the XML path accepting 0 was the odd one out.
  #
  # This fixture deliberately carries NO ResponseCurve element. With one present the
  # later "nChannels != icXmlNodeCount(...)" mismatch refuses the document anyway, so
  # a zero-count fixture that includes a curve passes even against the unfixed
  # library -- measured, and it made the first version of this case vacuous. With no
  # curve that check cannot fire, and the old parser accepted the document and wrote
  # a profile for a tag claiming zero channels.
  cat > "$file" <<XMLEOF
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <ProfileVersion>2.10</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
  </Header>
  <Tags>
    <responseCurveSet16Type>
      <TagSignature>resp</TagSignature>
      <CountOfChannels>0</CountOfChannels>
    </responseCurveSet16Type>
  </Tags>
</IccProfile>
XMLEOF

  if "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "a zero CountOfChannels was accepted and a profile written, unlike the JSON twin"
    return
  fi
  if ! grep -qE "$RESP_TAG_DIAG" "$logfile"; then
    fail "$name" "refused, but not by the responseCurveSet16Type parser"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (zero count refused, matching the JSON twin and the schema)"
}

measurement_devicecode_overflow() {
  local name="xml-responsecurve-devicecode-overflow"
  local over="$OUTDIR/resp-dev-over.xml" ctrl="$OUTDIR/resp-dev-ctrl.xml"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  # icResponse16Number::deviceCode is an icUInt16Number and was filled by atoi(), so
  # DeviceCode="65537" truncated to 1 exactly as CountOfChannels did -- same tag, same
  # function, 45 lines apart. Asserted the same way: the two documents must not
  # produce the same bytes.
  #
  # Both documents are written from the helper rather than sed-ed out of another case's
  # output.  The previous version read $OUTDIR/resp-ctrl.xml, which only exists once
  # countofchannels_overflow_is_not_one() has run; its fallback arm wrote "$ctrl" and the
  # unconditional redirect on the following line then truncated that same file to zero
  # bytes before sed failed, with "|| true" swallowing the failure -- so the case reported
  # a false "the DeviceCode=1 control no longer converts" instead of testing anything.
  write_countofchannels_document "$ctrl" "<CountOfChannels>1</CountOfChannels>"
  write_countofchannels_document "$over" "<CountOfChannels>1</CountOfChannels>"
  sed -i 's|DeviceCode="0"|DeviceCode="1"|' "$ctrl"
  sed -i 's|DeviceCode="0"|DeviceCode="65537"|' "$over"

  rm -f "$OUTDIR/${name}-over.icc" "$OUTDIR/${name}-ctrl.icc"
  "$FROMXML" "$over" "$OUTDIR/${name}-over.icc" >"$logfile" 2>&1
  local over_status=$?
  "$FROMXML" "$ctrl" "$OUTDIR/${name}-ctrl.icc" >"$OUTDIR/${name}-ctrl.log" 2>&1
  local ctrl_status=$?

  if [ "$ctrl_status" -ne 0 ] || [ ! -s "$OUTDIR/${name}-ctrl.icc" ]; then
    fail "$name" "the DeviceCode=1 control no longer converts (exit $ctrl_status)"
    return
  fi

  if [ "$over_status" -eq 0 ]; then
    if cmp -s "$OUTDIR/${name}-over.icc" "$OUTDIR/${name}-ctrl.icc"; then
      fail "$name" "DeviceCode 65537 truncated to 1 -- output is byte-identical to the control"
    else
      fail "$name" "an out-of-range DeviceCode was accepted"
    fi
    return
  fi

  check_sanitizers "$name" "$logfile" || return
  check_sanitizers "$name (control)" "$OUTDIR/${name}-ctrl.log" || return
  pass "$name (out-of-range DeviceCode refused, control still converts)"
}

responsecurve_profile_reads_back() {
  local name="xml-responsecurve-reads-back"
  local file="$OUTDIR/resp-readback.xml" logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ] || [ ! -x "$TOXML" ]; then
    skip "$name" "iccFromXml or iccToXml not built"
    return
  fi

  # CIccTagResponseCurveSet16::Read bounded the curve offset twice: once tag-relative
  # against the tag length (correct), then again as an ABSOLUTE file position against
  # that same tag length. A 44-byte resp tag at file offset 396 with curve offset 16
  # computed 412 > 44, so Read returned false and NO profile carrying this tag could be
  # read back -- iccToXml and iccToJson both said "Unable to read" and iccDumpProfile
  # reported the tag missing, for bytes that are correct (#2399).
  #
  # This is section 1's read-back-what-you-wrote invariant applied to this tag: without
  # it every other case here asserts only on documents the toolchain cannot consume, and
  # all four would stay green while the tag remained unreadable.
  write_countofchannels_document "$file" "<CountOfChannels>1</CountOfChannels>"

  if ! "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "the control document no longer converts"
    return
  fi
  if ! "$TOXML" "$OUTDIR/${name}.icc" "$OUTDIR/${name}-back.xml" >>"$logfile" 2>&1; then
    fail "$name" "iccToXml cannot read back the profile it just wrote -- the resp tag offset bound is back"
    return
  fi
  if ! grep -q "responseCurveSet16Type" "$OUTDIR/${name}-back.xml"; then
    fail "$name" "round-tripped, but the resp tag did not survive"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (profile with a resp tag reads back and keeps the tag)"
}

measurement_reserved_not_inherited() {
  local name="xml-responsecurve-reserved-not-inherited"
  local file="$OUTDIR/resp-reserved.xml" logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ] || [ ! -x "$TOXML" ]; then
    skip "$name" "iccFromXml or iccToXml not built"
    return
  fi

  # icResponse16Number was declared once per <ChannelResponses> and reused for every
  # <Measurement> sibling. deviceCode and measurementValue are assigned every iteration,
  # but reserved only when the attribute is present, so a bare <Measurement> following
  # Reserved="7" inherited the 7 -- a field ICC requires to be 0 (#2399).
  #
  # Asserted through a round trip rather than on the bytes: ToXml emits Reserved only
  # when it is nonzero, so the count of Reserved="7" in the regenerated document is
  # exactly the number of measurements that carry it. Two means the second inherited.
  cat > "$file" <<XMLEOF
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <ProfileVersion>2.10</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
  </Header>
  <Tags>
    <responseCurveSet16Type>
      <TagSignature>resp</TagSignature>
      <CountOfChannels>1</CountOfChannels>
      <ResponseCurve MeasUnitSignature="Status A">
        <ChannelResponses X="0" Y="0" Z="0">
          <Measurement DeviceCode="1" MeasValue="0" Reserved="7"/>
          <Measurement DeviceCode="2" MeasValue="0"/>
        </ChannelResponses>
      </ResponseCurve>
    </responseCurveSet16Type>
  </Tags>
</IccProfile>
XMLEOF

  if ! "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "the two-measurement document no longer converts"
    return
  fi
  if ! "$TOXML" "$OUTDIR/${name}.icc" "$OUTDIR/${name}-back.xml" >>"$logfile" 2>&1; then
    fail "$name" "iccToXml cannot read the profile back, so the field cannot be checked"
    return
  fi

  local nReserved
  nReserved=$(grep -c 'Reserved="7"' "$OUTDIR/${name}-back.xml")
  if [ "$nReserved" -ne 1 ]; then
    fail "$name" "Reserved=\"7\" appears $nReserved time(s); the measurement that omitted it inherited the previous value"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (an omitted Reserved stays 0 instead of inheriting)"
}

countofchannels_65536_channel_write() {
  local name="xml-responsecurve-count-65536-oob"
  local file="$OUTDIR/resp-65536.xml" logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  # The narrowing in #2398 was not only a wrong value -- at exactly 65536 it was a heap
  # out-of-bounds WRITE, and a second crash distinct from #2397's null deref.
  #
  # CountOfChannels was parsed into an icUInt32Number, so 65536 survived intact in
  # nChannels.  CIccResponseCurveStruct's constructor takes an icUInt16Number, so the
  # struct was built with 65536 truncated to 0 and calloc'd nothing -- while the
  # "nChannels != icXmlNodeCount(...)" gate compared the UNtruncated 65536 and therefore
  # passed for a document carrying 65536 <ChannelResponses>.  The loop then wrote through
  # curves.GetXYZ(i) (IccTagBasic.h:1704, an unchecked &m_maxColorantXYZ[index]) for i up
  # to 65535: roughly 768 KiB written past a zero-sized allocation.  Measured on the
  # unfixed parser: SIGSEGV, core dumped, exit 139.
  #
  # The fixture MUST carry all 65536 elements.  With fewer, the count gate refuses the
  # document before the write and the unfixed parser exits non-zero all by itself, so the
  # case would pass against the very defect it exists to pin -- the same vacuity that the
  # zero-count fixture above had to be rebuilt to avoid.
  if ! python3 - "$file" <<'PY'
import sys
n = 65536
chan = ('        <ChannelResponses X="0" Y="0" Z="0">\n'
        '          <Measurement DeviceCode="0" MeasValue="0"/>\n'
        '        </ChannelResponses>\n')
with open(sys.argv[1], "w") as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n<IccProfile>\n  <Header>\n'
            '    <ProfileVersion>2.10</ProfileVersion>\n'
            '    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n'
            '    <DataColourSpace>GRAY</DataColourSpace>\n'
            '    <PCS>XYZ </PCS>\n  </Header>\n  <Tags>\n'
            '    <responseCurveSet16Type>\n      <TagSignature>resp</TagSignature>\n'
            f'      <CountOfChannels>{n}</CountOfChannels>\n'
            '      <ResponseCurve MeasUnitSignature="Status A">\n')
    f.write(chan * n)
    f.write('      </ResponseCurve>\n    </responseCurveSet16Type>\n  </Tags>\n</IccProfile>\n')
PY
  then
    skip "$name" "could not generate the 65536-channel document"
    return
  fi

  "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1
  local status=$?

  # Asserted on HOW it exits, like the empty-element case: this defect's signature is a
  # signal death (128+signum), which a bare "exit != 0" would have accepted.
  if [ "$status" -ge 128 ]; then
    fail "$name" "iccFromXml died on a signal (exit $status) -- the 65536-channel OOB write is back"
    return
  fi
  if [ "$status" -eq 0 ]; then
    fail "$name" "a CountOfChannels of 65536 was accepted"
    return
  fi
  if ! grep -qE "$RESP_TAG_DIAG" "$logfile"; then
    fail "$name" "refused, but not by the responseCurveSet16Type parser"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name (65536 channels refused at the count, no signal)"
}

echo
echo "=== responseCurveSet16 CountOfChannels (issues #2397, #2398, #2399) ==="

countofchannels_empty_element
countofchannels_overflow_is_not_one
countofchannels_indented_is_accepted
countofchannels_zero_refused
measurement_devicecode_overflow
responsecurve_profile_reads_back
measurement_reserved_not_inherited
countofchannels_65536_channel_write

###############################################################################

echo
echo "=== Summary ==="
echo "  Total:   $TOTAL"
echo "  Passed:  $PASS"
echo "  Failed:  $FAIL"
echo "  Skipped: $SKIPPED"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
exit 0
