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
LOGFILE="$OUTDIR/issue-1729-spectral-data-info-sig.log"

regress() {
  echo "  [FAIL] issue-1729-spectral-data-info-sig -- $1"
  exit 2
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
  if [ ! -f "$xml" ]; then
    echo "  [info] fixture missing, skipping: $rel"
    continue
  fi

  icc="$OUTDIR/$(basename "${rel%.xml}").icc"
  if ! "$FROMXML" "$xml" "$icc" > "$LOGFILE" 2>&1; then
    sed -n '1,8p' "$LOGFILE"
    regress "iccFromXml failed to build $rel"
  fi

  "$DUMP" "$icc" ALL > "$LOGFILE" 2>&1

  # Regression 1: the mislabeled signature must be gone -- no "Unknown 'smwi'"
  # line and no raw 0x736D7769 signature in the dump.
  if grep -qi "smwi\|736D7769" "$LOGFILE"; then
    grep -ni "smwi\|736D7769" "$LOGFILE" | sed -n '1,4p'
    regress "$rel still emits the unregistered 'smwi' signature (0x736D7769)"
  fi

  # Regression 2: the tag must now be recognized as spectralDataInfoTag ('sdin').
  if ! grep -qi "spectralDataInfoTag\|Spectral Data Info (sdin) *PRESENT" "$LOGFILE"; then
    sed -n '1,12p' "$LOGFILE"
    regress "$rel does not report a recognized spectralDataInfoTag ('sdin')"
  fi

  checked=$((checked + 1))
done

if [ "$checked" -eq 0 ]; then
  echo "  [SKIP] no display fixtures available to exercise the spectral data info signature"
  exit 0
fi

echo "  [PASS] issue-1729-spectral-data-info-sig -- $checked fixture(s) report spectralDataInfoTag ('sdin'), no 'smwi'"
exit 0
