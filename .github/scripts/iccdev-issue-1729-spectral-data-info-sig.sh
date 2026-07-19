#!/bin/bash
###############################################################################
# iccFromXml/iccDumpProfile issue-1729 spectral data info tag signature
###############################################################################
#
# The MCS display fixtures Testing/Display/LCDDisplay.xml and
# Testing/Display/LaserProjector.xml carried their spectralDataInfoType tag under
# the signature 'smwi' (0x736D7769), which is not a registered ICC signature. On
# a fromXml -> dump round-trip iccDumpProfile reported it as
# "Unknown 'smwi' = 736D7769" and the Version 5 summary listed
# "Spectral Data Info (sdin)  ---" (absent), because the tag never landed in the
# registered 'sdin' (icSigSpectralDataInfoTag, 0x7364696E) slot.
#
# #1729 corrects the two fixtures to use 'sdin'. There is no committed .icc for
# these profiles (only the XML is tracked), so this guards the fix by rebuilding
# each profile with iccFromXml and asserting iccDumpProfile now recognizes the tag
# as spectralDataInfoTag ('sdin') and no longer emits the "Unknown 'smwi'" line.
# The header declares no spectralPCS, so the sdin<->spectralPCS conformance check
# stays gated off and validation is otherwise unchanged by the relabel.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to the Testing/ tree
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 2 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1729-spectral-data-info-sig}"
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

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"

regress() {
  echo "  [FAIL] issue-1729-spectral-data-info-sig -- $1"
  exit 2
}

# A sanitizer diagnostic under ASAN/UBSAN_OPTIONS=halt_on_error=0 is printed but
# does NOT change the tool's exit code, so a memory/UB error would otherwise slip
# past the gate. Scan the captured log for the sanitizer report markers and fail
# on any hit. $1 = log file, $2 = context describing what the tool was doing.
SAN_MARKERS='runtime error:|ERROR: (Address|Leak|Memory|Thread)Sanitizer|SUMMARY: (Address|Leak|Memory|Thread|Undefined)'
scan_sanitizer() {
  if grep -Eq "$SAN_MARKERS" "$1"; then
    grep -En "$SAN_MARKERS" "$1" | sed -n '1,6p'
    regress "sanitizer diagnostic while $2"
  fi
}

echo "=== iccFromXml/iccDumpProfile issue-1729 spectral data info tag signature regression ==="

if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ] || [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  echo "  [SKIP] iccFromXml or iccDumpProfile not built under $TOOLS_DIR"
  exit 0
fi

# The two MCS display fixtures whose spectralDataInfoType tag was mislabeled.
FIXTURES=(
  "Display/LCDDisplay.xml"
  "Display/LaserProjector.xml"
)

checked=0
for rel in "${FIXTURES[@]}"; do
  xml="$TESTING_DIR/$rel"
  [ -f "$xml" ] || xml="$REPO_ROOT/Testing/$rel"
  # Both fixtures are tracked in Testing/Display, so a missing one is not a
  # benign skip -- it means the fixture was deleted or renamed, which would
  # otherwise silently reduce this gate to partial coverage. Treat it as a
  # regression. (Absent tools are still a clean skip; that is handled above.)
  if [ ! -f "$xml" ]; then
    regress "tracked fixture missing (deleted or renamed?): $rel"
  fi

  base="$(basename "${rel%.xml}")"
  icc="$OUTDIR/$base.icc"
  fromxml_log="$OUTDIR/$base.fromxml.log"
  dump_log="$OUTDIR/$base.dump.log"

  # Build the profile. A nonzero iccFromXml exit is a hard failure; even on a
  # zero exit, scan its own (separate) log so a sanitizer finding during the
  # build is not lost when the dump log is written next.
  if ! "$FROMXML" "$xml" "$icc" > "$fromxml_log" 2>&1; then
    sed -n '1,8p' "$fromxml_log"
    regress "iccFromXml failed to build $rel"
  fi
  scan_sanitizer "$fromxml_log" "iccFromXml built $rel"

  # Dump the profile with no -v, so iccDumpProfile does not run validation and a
  # successful dump returns 0. (With -v it returns 0 for OK/warning/noncompliant
  # and -1 only for a critical error, but validation is off here.) Fatal errors
  # return negative codes -- -1 for bad args, -3 for a failed tag dump or stdout
  # write (-> shell 255/253) -- and a crash raises a signal (>=128). So only
  # rc>=126 (fatal returns + signals) fails the gate; a 0 success passes.
  "$DUMP" "$icc" ALL > "$dump_log" 2>&1
  dump_rc=$?
  if [ "$dump_rc" -ge 126 ]; then
    sed -n '1,12p' "$dump_log"
    regress "iccDumpProfile crashed or failed fatally on $rel (rc=$dump_rc)"
  fi
  scan_sanitizer "$dump_log" "iccDumpProfile dumped $rel"

  # Regression 1: the mislabeled signature must be gone -- no "Unknown 'smwi'"
  # line and no raw 0x736D7769 signature in the dump.
  if grep -qi "smwi\|736D7769" "$dump_log"; then
    grep -ni "smwi\|736D7769" "$dump_log" | sed -n '1,4p'
    regress "$rel still emits the unregistered 'smwi' signature (0x736D7769)"
  fi

  # Regression 2: the tag must now be recognized as spectralDataInfoTag ('sdin').
  if ! grep -qi "spectralDataInfoTag\|Spectral Data Info (sdin) *PRESENT" "$dump_log"; then
    sed -n '1,12p' "$dump_log"
    regress "$rel does not report a recognized spectralDataInfoTag ('sdin')"
  fi

  checked=$((checked + 1))
done

# Every tracked fixture must have been exercised. A missing fixture already
# regressed above; this guards against the loop body being skipped for any
# other reason, so partial coverage can never report success.
if [ "$checked" -ne "${#FIXTURES[@]}" ]; then
  regress "only $checked of ${#FIXTURES[@]} fixtures were checked"
fi

echo "  [PASS] issue-1729-spectral-data-info-sig -- $checked fixture(s) report spectralDataInfoTag ('sdin'), no 'smwi'"
exit 0
