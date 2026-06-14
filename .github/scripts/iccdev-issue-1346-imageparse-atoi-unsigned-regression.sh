#!/bin/bash
###############################################################################
# iccDEV issue #1346 - signed atoi() -> unsigned wrap in IccTagXml ParseXml
###############################################################################
#
# #1346 tracks the pervasive pattern (reported as #1342/#1343) where ParseXml
# handlers in IccXML/IccLibXML/IccTagXml.cpp read an XML attribute with atoi()
# (a signed int) and store it into an unsigned icUIntNN target.  A negative
# attribute wrapped to a large unsigned value; where the conversion is implicit
# UBSan's implicit-integer-sign-change check (part of the "integer" group)
# flags it as a runtime error, e.g.:
#
#   IccTagXml.cpp:1172:27: runtime error: implicit conversion from type 'int'
#   of value -1 (32-bit, signed) to type 'icUInt8Number' (aka 'unsigned char')
#   changed the value to 255 (8-bit, unsigned)
#
# The fix routes the affected sites through icXmlAttrToUInt(), which floors
# negatives to 0 so no signed->unsigned wrap occurs.  This test replays a PoC
# profile whose cicpFields ColorPrimaries attribute is negative through
# iccFromXml and fails if the implicit-conversion runtime error reappears.
#
# The PoC is a VALID profile (a copy of Testing/HDR/BT2100PQFullScene.xml with
# ColorPrimaries set to -1), so iccFromXml parses and saves it successfully:
# the pass/fail signal is purely the presence of the UBSan diagnostic, not the
# tool exit code.
#
# Note: only the implicit "Class A" sites can be exercised this way.  The
# "Class B" sites carry an explicit (icUIntNN) cast that suppresses the UBSan
# check, so they are invisible to any sanitizer-based test and are guarded by
# code review / the shared helper instead.
#
# This check is only meaningful when iccFromXml was built with the UBSan
# integer / implicit-conversion sanitizer (the ci-iccdev-tool-tests workflow
# builds with ENABLE_UBSAN=ON ENABLE_INTEGER_SANITIZER=ON).  On a plain build
# the conversion check is absent, so the test SKIPS rather than report a
# misleading pass.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (no sign-change error) or skipped cleanly
#   2 - UBSan implicit-conversion finding (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1346}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi

# Only meaningful under a build instrumented with the integer / implicit
# conversion sanitizer.  Inspect the build's CMakeCache.txt (walk up from the
# tool location) for either the ENABLE_* options or an explicit -fsanitize flag
# covering integer/undefined/implicit conversions.
CACHE=""
probe="$(dirname "$FROMXML")"
for _ in 1 2 3 4 5; do
  if [ -f "$probe/CMakeCache.txt" ]; then CACHE="$probe/CMakeCache.txt"; break; fi
  probe="$(dirname "$probe")"
done
sanitized=0
if [ -n "$CACHE" ]; then
  if grep -qiE "ENABLE_INTEGER_SANITIZER:BOOL=ON|ENABLE_UBSAN:BOOL=ON" "$CACHE"; then
    sanitized=1
  elif grep -iE "CMAKE_(CXX|C)_FLAGS" "$CACHE" | grep -qiE "fsanitize=[^ ]*(integer|undefined|implicit)"; then
    sanitized=1
  fi
fi
if [ "$sanitized" -ne 1 ]; then
  echo "[SKIP] iccFromXml not built with the integer/implicit-conversion sanitizer (CMakeCache: ${CACHE:-none})"
  exit 0
fi

CICP_XML="$DATA_DIR/ub-cicp-colorprimaries-1346.xml"

status=0
run_case() {
  local issue="$1" xml="$2"
  if [ ! -f "$xml" ]; then
    echo "[SKIP] PoC XML missing: $xml"
    return
  fi
  # Declare then assign separately so the $(basename ...) exit status is not
  # masked by `local` (shellcheck SC2155).
  local log
  log="$OUTDIR/$(basename "$xml").log"
  # Keep ASan quiet about any unrelated leaks so UBSan's conversion diagnostic
  # is the only thing we key on; the tool itself exits 0 on this valid profile.
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$FROMXML" "$xml" "$OUTDIR/$(basename "$xml").out" > "$log" 2>&1

  if grep -qE "runtime error: implicit conversion" "$log"; then
    echo "[FAIL] #$issue: signed->unsigned conversion of an XML attribute reappeared"
    grep -E "runtime error: implicit conversion|cicpFields|ParseXml|IccTagXml" "$log" | head
    status=2
  else
    echo "[PASS] #$issue: negative XML attribute parsed without signed->unsigned conversion"
  fi
}

run_case 1346 "$CICP_XML"

exit "$status"
