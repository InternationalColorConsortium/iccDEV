#!/bin/bash
###############################################################################
# iccDEV XML parser regression tests
###############################################################################
#
# Issue #1856. Two things are pinned here, and they pull in opposite
# directions, which is why they share a script:
#
#   1. iccDEV must be able to read back the XML it just wrote. A profile whose
#      CLUT serialises to a single text node larger than libxml2's
#      XML_MAX_TEXT_LENGTH (10 MB) used to be refused on read, so iccToXml
#      could emit a document iccFromXml then rejected.
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
#   b. Case 1's mechanism no longer holds on current libxml2. v2.13.0 moved the
#      XML_MAX_TEXT_LENGTH check into xmlSAX2Text ("SAX2: Enforce size limit in
#      xmlSAX2Text with XML_PARSE_HUGE"), so it applies however the input is
#      delivered and parsing from a buffer no longer avoids it. Case 1 therefore
#      fails against libxml2 >= 2.13 and is expected to keep failing until the
#      reader is fixed, which is the other half of #2108.
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
# 1. Round-trip a profile whose XML carries a text node over the streaming cap
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

  # A grid-33 CMYK->RGB device link is the cheapest way to get a CLUT that
  # serialises past the 10 MB text-node limit: 33^4 grid points x 3 channels
  # is ~21 MB of whitespace-separated values in one element.
  if ! "$APPLYTOLINK" "$link" 0 33 0 "issue 1856 regression" 0 1 1 1 \
       "$src" 1 "$dst" 1 >"$logfile" 2>&1; then
    fail "$name" "iccApplyToLink failed to build the fixture link"
    return
  fi

  if ! "$TOXML" "$link" "$xml" >>"$logfile" 2>&1; then
    fail "$name" "iccToXml failed on the fixture link"
    return
  fi

  # Confirm the fixture actually exercises the limit. If a future change to the
  # writer shortens the node below the cap this test would silently stop
  # testing anything, so treat that as a skip with the measured size.
  local longest
  longest="$(python3 - "$xml" <<'PY'
import re, sys
data = open(sys.argv[1], "rb").read()
print(max((len(m.group(1)) for m in re.finditer(rb">([^<]*)<", data)), default=0))
PY
)"
  if [ -z "$longest" ] || [ "$longest" -le "$XML_MAX_TEXT_LENGTH" ]; then
    skip "$name" "longest text node ${longest:-unknown} bytes does not exceed $XML_MAX_TEXT_LENGTH"
    return
  fi

  if ! "$FROMXML" "$xml" "$back" >>"$logfile" 2>&1; then
    fail "$name" "iccFromXml refused a ${longest}-byte text node it had just written"
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
    pass "$name (${longest} byte text node, round-trip byte-identical)"
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
