#!/bin/bash
###############################################################################
# iccDEV issue #1421 - invalid-enum load in CIccCfgProfile::toJson()
###############################################################################
#
# iccApplyProfiles' "-exportcfg" path parses the per-profile rendering-intent
# code from the command line in CIccCfgProfileSequence::fromArgs().  The intent
# code is a packed decimal whose tens field selects the xform LUT type:
#
#     nType  = abs(nIntent) / 10;     // e.g. 65535 decodes to nType == 53
#     pProf->m_transform = (icXformLutType)nType;
#
# icXformLutType only defines 0x0..0xD.  CIccCfgProfile::toJson() then reads
# m_transform back out via icGetTransformName(m_transform, ...).  At the bisected
# commit (b90ac39, May 2024) the decoded nType was stored unclamped, so a value
# like 53 was loaded as an icXformLutType in toJson() -- undefined behaviour that
# UBSan's -fsanitize=enum (part of -fsanitize=undefined) traps:
#
#   IccCmmConfig.cpp: runtime error: load of value 53,
#       which is not a valid value for type 'icXformLutType'
#     in CIccCfgProfile::toJson(...)
#     #1 CIccCfgProfileSequence::toJson(...)
#
# Commit b087101 (#1400, "range check intents and xform lut types from command
# line") pins the decoded nType to [icXformLutMinimum, icXformLutMaximum] before
# it is stored, so toJson() can never load an out-of-range icXformLutType.  This
# test locks that fix in: it replays the exact #1421 repro (intent 65535) through
# -exportcfg and fails if the invalid-enum load reappears.
#
# -exportcfg writes the config (and so runs the toJson load) immediately after
# argument parsing, before any image or profile is opened, so this test is
# self-contained and drives the vulnerable path with placeholder file names.
#
# The enum check only exists when the tool was built with the UBSan "enum" check,
# i.e. -fsanitize=undefined (ENABLE_UBSAN / ENABLE_SANITIZERS, or an explicit
# -fsanitize=undefined/enum flag).  On a build without it the test SKIPS rather
# than report a misleading pass.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (no invalid-enum load) or skipped cleanly
#   2 - UBSan invalid-enum-load finding (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1421-cfg-transform-enum-load}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then TOOLS_DIR="$candidate"; break; fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyProfiles -type f 2>/dev/null | head -1)"
LOGFILE="$OUTDIR/issue-1421-cfg-transform-enum-load.log"

echo "=== iccApplyProfiles issue-1421 CIccCfgProfile::toJson invalid-enum-load regression ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyProfiles not built under $TOOLS_DIR"
  exit 0
fi

# Only meaningful when the tool was built with the UBSan enum check, which lives
# in -fsanitize=undefined.  Walk up from the tool to its CMakeCache.txt and
# require ENABLE_UBSAN/ENABLE_SANITIZERS or an explicit -fsanitize=undefined/enum
# flag.  Plain ASan (without UBSan) does not have the enum check.
CACHE=""
probe="$(dirname "$APPLY")"
for _ in 1 2 3 4 5; do
  if [ -f "$probe/CMakeCache.txt" ]; then CACHE="$probe/CMakeCache.txt"; break; fi
  probe="$(dirname "$probe")"
done
ubsan=0
if [ -n "$CACHE" ]; then
  if grep -qiE "ENABLE_UBSAN:BOOL=ON|ENABLE_SANITIZERS:BOOL=ON" "$CACHE"; then
    ubsan=1
  elif grep -iE "CMAKE_(CXX|C)_FLAGS" "$CACHE" | grep -qiE "fsanitize=.*(undefined|enum)"; then
    ubsan=1
  fi
fi
if [ "$ubsan" -ne 1 ]; then
  echo "  [SKIP] tool not built with UBSan enum check (CMakeCache: ${CACHE:-none})"
  exit 0
fi

# Export the config for a profile whose rendering-intent code 65535 decodes to an
# out-of-range xform LUT type (nType == 53).  -exportcfg serialises the parsed
# config (running CIccCfgProfile::toJson, which loads m_transform) before any
# image or profile file is opened, so placeholder file names are sufficient; the
# tool exits non-zero regardless.  Keep UBSan from halting so the diagnostic is
# captured in the log; the enum-load message is the signal we test for.
rm -f "$LOGFILE"
ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
  "$APPLY" -exportcfg "$OUTDIR/issue-1421-cfg.json" \
    "$OUTDIR/issue-1421-nosrc.tif" "$OUTDIR/issue-1421-nodst.tif" 2 1 0 1 1 \
    -embedded 3 "$OUTDIR/issue-1421-noprof.icc" 65535 > "$LOGFILE" 2>&1

if grep -qE "not a valid value for type 'icXformLutType'" "$LOGFILE"; then
  echo "  [FAIL] issue-1421 -- CIccCfgProfile::toJson invalid-enum load reappeared"
  grep -E "not a valid value for type 'icXformLutType'|toJson|IccCmmConfig.cpp" "$LOGFILE" | head
  exit 2
fi

echo "  [PASS] issue-1421-cfg-transform-enum-load -- out-of-range xform type pinned before toJson load"
exit 0
