#!/bin/bash
###############################################################################
# iccDEV issues #1342 / #1343 - signed->unsigned conversion in image ParseXml
###############################################################################
#
# CIccTagXmlEmbeddedHeightImage::ParseXml() and
# CIccTagXmlEmbeddedNormalImage::ParseXml() read the "SeamlessIndicator"
# attribute with atoi() (a signed int) and assigned it to the unsigned 32-bit
# member m_nSeamlesIndicator.  A negative attribute value wrapped to a huge
# unsigned value through the implicit int -> icUInt32Number conversion, which
# UBSan's implicit-integer-sign-change check (part of the "integer" group)
# flags as a runtime error:
#
#   IccTagXml.cpp:5548:25: runtime error: implicit conversion from type 'int'
#   of value -98 ... to type 'icUInt32Number' ... changed the value to ...
#
# The fix parses into a signed temporary and floors negatives to 0, so no
# signed->unsigned wrap occurs.  This test replays the two fuzz PoCs through
# iccFromXml and fails if the implicit-conversion runtime error reappears.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1342-1343}"
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

HEIGHT_XML="$DATA_DIR/ub-heightimage-parsexml-1342.xml"
NORMAL_XML="$DATA_DIR/ub-normalimage-parsexml-1343.xml"

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
  # The PoC profile is intentionally invalid, so iccFromXml exits non-zero
  # regardless of the fix; the pass/fail signal is the UBSan runtime error, not
  # the tool exit code.  Keep ASan quiet about unrelated leaks on this invalid
  # input so UBSan's conversion diagnostic is the only thing we key on.
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$FROMXML" "$xml" "$OUTDIR/$(basename "$xml").out" > "$log" 2>&1

  if grep -qE "runtime error: implicit conversion" "$log"; then
    echo "[FAIL] #$issue: signed->unsigned conversion of SeamlessIndicator reappeared"
    grep -E "runtime error: implicit conversion|SeamlessIndicator|ParseXml" "$log" | head
    status=2
  else
    echo "[PASS] #$issue: SeamlessIndicator parsed without signed->unsigned conversion"
  fi
}

run_case 1342 "$HEIGHT_XML"
run_case 1343 "$NORMAL_XML"

exit "$status"
