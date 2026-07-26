#!/bin/bash
###############################################################################
# iccDEV issues #1846 / #1847 - unvalidated CountOfDeviceCoords in the XML
#                               namedColor2 reader
###############################################################################
#
# CIccTagXmlNamedColor2::ParseXml read the attribute with atoi():
#
#   icUInt32Number newDeviceCoords = atoi(szDeviceCoords);   // IccTagXml.cpp
#   icUInt32Number n = icXmlNodeCount3(...);
#   SetSize(n, newDeviceCoords);                             // result discarded
#
# That is wrong in two independent ways, and this test covers both.
#
# CASE A -- the reported one.  atoi() is undefined on a value that does not fit
# an int.  CountOfDeviceCoords="333333333333" truncated to -1674115755, which
# then changed value again converting into the icUInt32Number:
#
#   IccTagXml.cpp:1014:42: runtime error: implicit conversion from type 'int' of
#   value -1674115755 (32-bit, signed) to type 'icUInt32Number' changed the
#   value to 2620851541 (32-bit, unsigned)
#
# Arriving back at SetSize()'s signed nDeviceCoords parameter, the negative
# value is that function's "keep the existing count" sentinel, so the attribute
# was silently ignored.  Detectable only under an integer/implicit-conversion
# sanitizer build; the script skips this case cleanly elsewhere.
#
# CASE B -- the one the issues do not mention, and the reason this test matters
# in the ordinary sanitizer jobs.  A large POSITIVE count is not sentinelled
# away.  SetSize(n, 2000000000) computes a per-entry size of roughly 8 GB, the
# calloc() fails, and SetSize() returns false -- but the result was discarded,
# leaving m_NamedColor pointing at the single-entry buffer the constructor
# allocated while the loop below still wrote one entry per XML node:
#
#   ERROR: AddressSanitizer: heap-buffer-overflow
#   WRITE of size 4 ... CIccTagXmlNamedColor2::ParseXml IccTagXml.cpp
#   located 32 bytes after 48-byte region
#   allocated by CIccTagNamedColor2::CIccTagNamedColor2(int, int)
#
# That is CWE-787, reachable from iccFromXml on attacker-supplied XML, and it
# needs only a plain ASan build.  Case B also carries a functional assertion
# that holds without any sanitizer: the fixed reader must REJECT the profile,
# because the count exceeds the kMaxNamedColorDeviceCoords bound that
# CIccTagNamedColor2::Read and both writers already enforce.  Pre-fix the tag
# "parsed" (the loop returns i==n) and the profile was written.
#
# Both PoCs are Testing/Named/NamedColorV4.xml with only CountOfDeviceCoords
# changed, so anything else they exercise is a known-good path.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (or skipped cleanly)
#   2 - regression detected
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1846}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi

# Locate the build's CMakeCache.txt (walk up from the tool) to decide whether the
# sign-change check of case A is actually compiled in.
#
# The gate deliberately does NOT accept ENABLE_UBSAN / a bare "undefined" in the
# flags.  Build/Cmake/CMakeLists.txt maps ENABLE_UBSAN to -fsanitize=undefined
# and only ENABLE_INTEGER_SANITIZER to -fsanitize=integer, and
# implicit-integer-sign-change lives in the integer group, not in undefined.  A
# looser test reports a confident PASS on an -fsanitize=address,undefined build
# that cannot observe the defect at all.
CACHE=""
probe="$(dirname "$FROMXML")"
for _ in 1 2 3 4 5; do
  if [ -f "$probe/CMakeCache.txt" ]; then CACHE="$probe/CMakeCache.txt"; break; fi
  probe="$(dirname "$probe")"
done
sanitized=0
if [ -n "$CACHE" ]; then
  if grep -qiE "ENABLE_INTEGER_SANITIZER:BOOL=ON" "$CACHE"; then
    sanitized=1
  elif grep -iE "CMAKE_(CXX|C)_FLAGS" "$CACHE" | grep -qiE "fsanitize=[^ ]*(integer|implicit)"; then
    sanitized=1
  fi
fi

status=0

TOOL_RC=0
TOOL_LOG=""
TOOL_ICC=""
run_tool() {
  # $1 = PoC basename (without .xml).
  #
  # Sets the globals TOOL_LOG / TOOL_ICC / TOOL_RC rather than echoing the log
  # path: a caller writing LOG=$(run_tool ...) would run this in a command
  # substitution subshell, and TOOL_RC would never reach the caller -- it would
  # read as 0 no matter how the tool died, quietly disabling the exit-status half
  # of the crash check below.
  local poc="$DATA_DIR/$1.xml"
  TOOL_LOG="$OUTDIR/$1.log"
  TOOL_ICC="$OUTDIR/$1.icc"
  rm -f "$TOOL_ICC"
  # allocator_may_return_null keeps ASan from turning the deliberately huge
  # allocation into an allocator abort, so the heap-overflow check is what fires.
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0:allocator_may_return_null=1" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$FROMXML" "$poc" "$TOOL_ICC" > "$TOOL_LOG" 2>&1
  TOOL_RC=$?
}

# --- CASE A: sign-change on an out-of-int-range count ------------------------
POC_A="ub-namedcolor-devicecoords-overflow-1846"
if [ ! -f "$DATA_DIR/$POC_A.xml" ]; then
  echo "[SKIP] PoC XML missing: $DATA_DIR/$POC_A.xml"
elif [ "$sanitized" -ne 1 ]; then
  echo "[SKIP] case A: not an integer/implicit-conversion build (CMakeCache: ${CACHE:-none})"
else
  run_tool "$POC_A"
  LOG_A="$TOOL_LOG"
  if grep -qE "runtime error: implicit conversion" "$LOG_A"; then
    echo "[FAIL] #1846: implicit conversion of CountOfDeviceCoords reappeared"
    grep -E "runtime error: implicit conversion|IccTagXml" "$LOG_A" | head
    status=2
  else
    echo "[PASS] #1846 case A: out-of-range CountOfDeviceCoords parsed without a sign change"
  fi
fi

# --- CASE B: large positive count -> heap-buffer-overflow write --------------
POC_B="ub-namedcolor-devicecoords-huge-1846"
if [ ! -f "$DATA_DIR/$POC_B.xml" ]; then
  echo "[SKIP] PoC XML missing: $DATA_DIR/$POC_B.xml"
else
  run_tool "$POC_B"
  LOG_B="$TOOL_LOG"
  rc_b="$TOOL_RC"
  ICC_B="$TOOL_ICC"

  # Any abnormal termination is a failure, however it is reported.  On an ASan
  # build the overflow surfaces as heap-buffer-overflow; without ASan the same
  # write corrupts glibc's heap metadata and the process dies later inside
  # malloc ("SEGV ... in unlink_chunk"), so keying only on the ASan string would
  # miss the unsanitized build entirely.  A signal death gives rc >= 128.
  crashed=0
  if grep -qE "AddressSanitizer|LeakSanitizer|runtime error: |ABORTING|SEGV|Segmentation fault" "$LOG_B"; then
    crashed=1
  elif [ "$rc_b" -ge 128 ]; then
    crashed=1
  fi

  if [ "$crashed" -eq 1 ]; then
    echo "[FAIL] #1847: huge CountOfDeviceCoords crashed the parser (rc=$rc_b)"
    grep -E "AddressSanitizer|WRITE of size|IccTagXml.cpp|located .* bytes|allocated by|SEGV|ABORTING" "$LOG_B" | head
    status=2
  # Functional assertion, valid on every build: the count is far above the
  # kMaxNamedColorDeviceCoords bound the binary reader and both writers enforce,
  # so the reader must reject the tag rather than write a profile.  Checked only
  # once the run is known to have terminated normally, otherwise "no profile
  # written" would read as success when the process merely died first.
  elif [ -f "$ICC_B" ]; then
    echo "[FAIL] #1847: profile written despite CountOfDeviceCoords=2000000000"
    echo "       the XML reader accepted a count the binary reader rejects"
    status=2
  else
    echo "[PASS] #1847 case B: out-of-bounds CountOfDeviceCoords rejected cleanly (rc=$rc_b), no profile written"
  fi
fi

exit "$status"
