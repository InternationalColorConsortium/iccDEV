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
#   2. Relaxing that must not have been done with XML_PARSE_HUGE, which would
#      also have disabled the nesting-depth and name-length caps for every
#      input. The hardening cases below fail if someone later reaches for that
#      flag to solve a size problem.
#
# The fix reads the document into one contiguous buffer and parses that, which
# avoids the streaming reader's per-node accumulation limit, and bounds the
# whole document instead (icXmlMaxTextFileBytes).
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

reject_case() {
  local name="$1" file="$2" why="$3"
  local logfile="$OUTDIR/${name}.log"

  if [ ! -x "$FROMXML" ]; then
    skip "$name" "iccFromXml not built"
    return
  fi

  if "$FROMXML" "$file" "$OUTDIR/${name}.icc" >"$logfile" 2>&1; then
    fail "$name" "$why was accepted"
    return
  fi
  check_sanitizers "$name" "$logfile" || return
  pass "$name ($why refused)"
}

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
reject_case "xml-reject-entity-amplification" "$OUTDIR/laughs.xml" "a billion-laughs document"

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
