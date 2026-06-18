#!/bin/bash
###############################################################################
# iccDEV issue #1422 - invalid-enum load in CIccCfgDataApply::fromArgs()
###############################################################################
#
# iccApplyNamedCmm's "-exportcfganddata" path parses the destination encoding
# selector straight from the command line.  CIccCfgDataApply::fromArgs() did:
#
#     int nDstEncoding;
#     icParseIntArg(args[1], nDstEncoding);
#     icFloatColorEncoding dstEncoding = (icFloatColorEncoding)nDstEncoding;
#     if (!icIsJsonColorEncoding(dstEncoding))   // <-- loads the enum
#       return 0;
#
# icFloatColorEncoding only defines 0..icEncodeUnknown.  When the caller passes
# an out-of-range value (the fuzz repro passes 9), narrowing it to the enum and
# then *reading* it is undefined behaviour -- UBSan's -fsanitize=enum (part of
# -fsanitize=undefined) traps the load:
#
#   IccCmmConfig.cpp:345: runtime error: load of value 9,
#       which is not a valid value for type 'icFloatColorEncoding'
#     in CIccCfgDataApply::fromArgs(char const**, int, bool)
#
# The fix range-checks the raw int (icIsJsonColorEncoding now takes an int) and
# only narrows to the enum once the value is known valid, so the out-of-range
# value is never loaded as an enum.
#
# The selector is parsed before any data file or profile is opened, so this test
# is self-contained: it drives the exact "9" encoding argument with placeholder
# file names and fails if the invalid-enum load reappears.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1422-cfg-encoding-enum-load}"
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

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyNamedCmm -type f 2>/dev/null | head -1)"
LOGFILE="$OUTDIR/issue-1422-cfg-encoding-enum-load.log"

echo "=== iccApplyNamedCmm issue-1422 CIccCfgDataApply::fromArgs invalid-enum-load regression ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyNamedCmm not built under $TOOLS_DIR"
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

# A minimal placeholder data file; the encoding selector "9" is parsed (and the
# invalid-enum load is hit) before this file or the profiles are ever opened, so
# the tool exits non-zero on the unparsable configuration regardless.  Keep UBSan
# from halting so the diagnostic is captured in the log; the enum-load message is
# the signal we test for.
DATAFILE="$OUTDIR/issue-1422-placeholder-data.txt"
printf '0.0 0.0 0.0 0.0\n' > "$DATAFILE"
rm -f "$LOGFILE"
ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
  "$APPLY" -exportcfganddata "$OUTDIR/issue-1422-cfg.json" "$DATAFILE" 9 1 \
    "$OUTDIR/issue-1422-nope1.icc" 10003 "$OUTDIR/issue-1422-nope2.icc" 3 > "$LOGFILE" 2>&1

if grep -qE "not a valid value for type 'icFloatColorEncoding'" "$LOGFILE"; then
  echo "  [FAIL] issue-1422 -- CIccCfgDataApply::fromArgs invalid-enum load reappeared"
  grep -E "not a valid value for type 'icFloatColorEncoding'|icIsJsonColorEncoding|IccCmmConfig.cpp" "$LOGFILE" | head
  exit 2
fi

echo "  [PASS] issue-1422-cfg-encoding-enum-load -- out-of-range encoding rejected without invalid-enum load"
exit 0
