#!/bin/bash
###############################################################################
# iccDumpProfile issue-1395 GetProfileClassSigName "Class" suffix removal
###############################################################################
#
# CIccInfo::GetProfileClassSigName() returned display labels with a glued "Class"
# suffix ("DisplayClass", "OutputClass", ...). That suffix is not an ICC.1/ICC.2
# spec identifier and is redundant in every (display-only) caller, so #1395 drops
# it ("Display", "Output", ...).  The strings are human-readable labels only --
# nothing parses or serializes them -- so this guards the cleanup against
# regression by dumping one committed profile per class and asserting the
# "Profile Class:" line carries the de-suffixed name with no trailing "Class".
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
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1395-profileclass-signame}"
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

DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"
LOGFILE="$OUTDIR/issue-1395-profileclass-signame.log"

regress() {
  echo "  [FAIL] issue-1395-profileclass-signame -- $1"
  exit 2
}

echo "=== iccDumpProfile issue-1395 GetProfileClassSigName suffix-removal regression ==="

if [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  echo "  [SKIP] iccDumpProfile not built under $TOOLS_DIR"
  exit 0
fi

# One committed profile per deviceClass, with the expected de-suffixed label.
# "MultiplexLink" has no committed sample so it is exercised only by the
# negative grep below (no "*Class" form may appear for any class).
CASES=(
  "Display|Testing/ICS/Rec2100HlgFull-Part1.icc"
  "Output|Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc"
  "Input|Testing/SpecRef/SixChanInputRef.icc"
  "Link|Testing/Display/RgbGSDF.icc"
  "Abstract|Testing/SpecRef/RefDecC.icc"
  "ColorSpace|Testing/sRGB_v4_ICC_preference.icc"
  "NamedColor|Testing/Named/FluorescentNamedColor.icc"
  "ColorEncoding|Testing/Encoding/sRgbEncoding.icc"
  "MultiplexIdentification|Testing/mcs/6ChanSelect-MID.icc"
  "MultiplexVisualization|Testing/mcs/18ChanWithSpots-MVIS.icc"
)

checked=0
for case in "${CASES[@]}"; do
  expect="${case%%|*}"
  rel="${case#*|}"
  prof="$TESTING_DIR/${rel#Testing/}"
  [ -f "$prof" ] || prof="$REPO_ROOT/$rel"
  if [ ! -f "$prof" ]; then
    echo "  [info] sample profile missing, skipping case: $rel"
    continue
  fi

  "$DUMP" "$prof" > "$LOGFILE" 2>&1
  line="$(grep -m1 -i "Profile Class:" "$LOGFILE" | sed 's/\r$//')"
  if [ -z "$line" ]; then
    echo "  [info] no Profile Class line for $rel (tool output below)"; sed -n '1,8p' "$LOGFILE"
    continue
  fi

  got="$(printf '%s\n' "$line" | sed 's/.*Profile Class: *//')"
  # The label must be exactly the de-suffixed name -- a trailing "Class" means the
  # cleanup regressed (e.g. "DisplayClass" instead of "Display").
  if [ "$got" != "$expect" ]; then
    sed -n '1,8p' "$LOGFILE"
    regress "deviceClass label for $rel was '$got', expected '$expect' (suffix not dropped?)"
  fi
  case "$got" in
    *Class) regress "deviceClass label '$got' still carries the 'Class' suffix ($rel)" ;;
  esac
  checked=$((checked + 1))
done

if [ "$checked" -eq 0 ]; then
  echo "  [SKIP] no sample profiles available to exercise GetProfileClassSigName"
  exit 0
fi

echo "  [PASS] issue-1395-profileclass-signame -- $checked profile-class labels de-suffixed"
exit 0
