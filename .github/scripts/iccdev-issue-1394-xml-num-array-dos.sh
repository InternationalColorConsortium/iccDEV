#!/bin/bash
###############################################################################
# iccToXml issue-1394 unbounded ui08/ui16/ui32/ui64 array serialization (DoS)
###############################################################################
#
# CIccTagXmlNum<T,A,Tsig>::ToXml() walked m_nSize without an upper-bound check,
# so a malformed profile carrying an oversized UInt{8,16,32,64}Array tag drove
# an unbounded serialization loop: a ~16 MB ui08 array expanded to ~80 MB of XML
# (the original report observed a 165 MB file).  The serializer now asserts the
# same 0xffffff cap already enforced on the sibling CIccTagXmlFixedNum::ToXml and
# refuses to emit an over-cap array (CWE-400/CWE-834).
#
# This test builds a minimal ICC profile whose ui08 tag count exceeds the cap and
# asserts iccToXml refuses it without ballooning the output; it also confirms an
# in-bounds ui08 array still round-trips to <Array> XML.  Pure-python fixture
# generation (no PIL), python3-gated.
#
# Note on the over-cap assertions: they check the output, not the exit code.  See
# the comment at that check -- #1779 made the writer skip an unserializable tag
# and keep the document, so iccToXml now exits 0 on this fixture while still
# refusing the array.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 2 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1394-xml-num-array-dos}"
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

TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"
LOGFILE="$OUTDIR/issue-1394-xml-num-array-dos.log"

regress() {
  echo "  [FAIL] issue-1394-xml-num-array-dos -- $1"
  exit 2
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null; then
    sed -n '1,80p' "$1"
    return 1
  fi
  return 0
}

echo "=== iccToXml issue-1394 unbounded ui08 array serialization regression ==="

if [ -z "$TOXML" ] || [ ! -x "$TOXML" ]; then
  echo "  [SKIP] iccToXml not built under $TOOLS_DIR"
  exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "  [SKIP] python3 not available for fixture generation"
  exit 0
fi

# Emit a minimal valid ICC profile carrying a single ui08 (UInt8Array) tag with N
# data bytes; m_nSize on load == N.  The 0xffffff serialization cap is exclusive,
# so N=0x1000000 is one element over the limit and N=0xffff is comfortably under.
gen_ui08() {  # $1=outpath  $2=N  $3=fillbyte
  python3 - "$1" "$2" "$3" <<'PY'
import struct, sys
outpath, N, fill = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0)
be32 = lambda v: struct.pack('>I', v)
tag_data = b'ui08' + be32(0) + bytes([fill]) * N      # type sig + reserved + data
tag_off  = 128 + 4 + 12                                # header + (count + 1 entry)
tagtbl   = be32(1) + b'tu08' + be32(tag_off) + be32(len(tag_data))
total    = tag_off + len(tag_data)
hdr = bytearray(128)
hdr[0:4]   = be32(total)
hdr[8:12]  = bytes([0x04, 0x30, 0x00, 0x00])          # v4.3
hdr[12:16] = b'mntr'
hdr[16:20] = b'RGB '
hdr[20:24] = b'XYZ '
hdr[36:40] = b'acsp'
open(outpath, 'wb').write(bytes(hdr) + tagtbl + tag_data)
PY
}

# 1) Over-cap ui08 array (m_nSize = 0x1000000 > 0xffffff) must be refused without
#    ballooning the XML.  Pre-fix: iccToXml returned 0 and wrote ~80 MB.
DOS_ICC="$OUTDIR/ui08-array-dos.icc"
DOS_XML="$OUTDIR/ui08-array-dos.xml"
rm -f "$DOS_ICC" "$DOS_XML" "$LOGFILE"
gen_ui08 "$DOS_ICC" 0x1000000 0xFF
"$TOXML" "$DOS_ICC" "$DOS_XML" > "$LOGFILE" 2>&1
rc=$?
check_sanitizers "$LOGFILE" || regress "sanitizer error converting over-cap ui08 array"

xml_bytes=0
[ -f "$DOS_XML" ] && xml_bytes="$(stat -c %s "$DOS_XML" 2>/dev/null || stat -f %z "$DOS_XML" 2>/dev/null || echo 0)"

# The property under test is that the over-cap array is never walked: the output
# must not balloon and must not contain the serialized <Array>.
#
# This deliberately no longer keys off a non-zero exit code.  Until #1779 the XML
# writer abandoned the whole document as soon as any tag's ToXml() returned false,
# so "iccToXml failed" was a usable stand-in for "the cap fired".  The writer now
# skips just the offending tag and still emits the rest of the profile, so a zero
# exit code is the correct result here and is no longer evidence either way.  The
# assertions below therefore check the thing that actually matters, and are
# strictly stronger than the old exit-code proxy: the array must be absent from
# the output, and the refusal must be recorded rather than the tag silently
# vanishing.
if [ "${xml_bytes:-0}" -gt 1048576 ]; then
  sed -n '1,20p' "$LOGFILE"
  regress "over-cap ui08 array ballooned the output (rc=$rc, xml=${xml_bytes} bytes) -- unbounded loop not capped"
fi
if [ -f "$DOS_XML" ] && grep -q "<Array>" "$DOS_XML"; then
  sed -n '1,20p' "$LOGFILE"
  regress "over-cap ui08 array was serialized into the document -- unbounded loop not capped"
fi
if ! grep -q "Unable to output tag with type tu08" "$LOGFILE" \
   && ! { [ -f "$DOS_XML" ] && grep -q "unable to serialize, skipped" "$DOS_XML"; }; then
  sed -n '1,20p' "$LOGFILE"
  regress "over-cap ui08 array was neither refused nor recorded as skipped -- cap did not fire"
fi

# 2) In-bounds ui08 array must still serialize to an <Array> element.
OK_ICC="$OUTDIR/ui08-array-ok.icc"
OK_XML="$OUTDIR/ui08-array-ok.xml"
rm -f "$OK_ICC" "$OK_XML"
gen_ui08 "$OK_ICC" 0xffff 0x2A
"$TOXML" "$OK_ICC" "$OK_XML" > "$LOGFILE" 2>&1
rc=$?
check_sanitizers "$LOGFILE" || regress "sanitizer error converting in-bounds ui08 array"
if [ "$rc" -ne 0 ] || [ ! -s "$OK_XML" ] || ! grep -q "<Array>" "$OK_XML"; then
  sed -n '1,20p' "$LOGFILE"
  regress "in-bounds ui08 array failed to serialize (rc=$rc) -- cap too aggressive"
fi

echo "  [PASS] issue-1394-xml-num-array-dos -- over-cap ui08 refused, in-bounds ui08 serialized"
exit 0
