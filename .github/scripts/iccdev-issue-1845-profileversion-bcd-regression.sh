#!/bin/bash
###############################################################################
# iccDEV issue #1845 - negative <ProfileVersion> component in the XML importer
###############################################################################
#
# parseVersion() in IccXML/IccLibXML/IccProfileXml.cpp read a version component
# with atoi(), which accepts a leading '-'.  For <ProfileVersion>-12.</...> that
# gave val == -12; ((val/10)%10)*16 + (val%10) then evaluated to -18, and storing
# that in the function's unsigned char return produced 238 (0xEE):
#
#   IccProfileXml.cpp:343:8: runtime error: implicit conversion from type 'int'
#   of value -18 (32-bit, signed) to type 'unsigned char' changed the value to
#   238 (8-bit, unsigned)
#
# 0xE is not a valid BCD digit, so the resulting header version is one
# CIccInfo::GetVersionName can only render back as "Invalid BCD version".  The
# tool nevertheless reported "Profile parsed and saved correctly" and wrote the
# byte to disk, which is what makes this worth a functional check and not just a
# sanitizer check.
#
# The fix routes the component through icXmlParseU32(s, out, 99), the strict
# attribute parser in IccUtilXml.h, and rejects the whole version if any
# component is malformed -- leaving the memset-zeroed 0, which renders as a
# plain "0.00".  This mirrors icJsonParseBCDByte in IccProfileJson.cpp, the same
# defect fixed for #1830 on the JSON side.
#
# This test therefore keys on TWO signals:
#
#   1. Functional (every build): if a profile is written, its major-version byte
#      must be a valid BCD pair.  Pre-fix that byte is 0xEE; post-fix it is 0x00.
#   2. Sanitizer (integer/implicit-conversion builds only): the UBSan
#      implicit-conversion diagnostic must not reappear.
#
# Signal 1 is the reason this test is useful in the ordinary CI jobs; signal 2
# only fires where the sanitizer is enabled, exactly as for #1346.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1845}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi

POC="$DATA_DIR/ub-profileversion-negative-1845.xml"
if [ ! -f "$POC" ]; then
  echo "[SKIP] PoC XML missing: $POC"
  exit 0
fi

LOG="$OUTDIR/profileversion-negative-1845.log"
OUT_ICC="$OUTDIR/profileversion-negative-1845.icc"
rm -f "$OUT_ICC"

# Keep ASan quiet about unrelated leaks, and let UBSan report without aborting so
# both signals can be inspected from one run.
ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
  "$FROMXML" "$POC" "$OUT_ICC" > "$LOG" 2>&1

status=0

# --- Signal 1: the saved header must not carry a non-BCD version byte --------
# The ICC header version is at offset 8; its first byte packs the major version
# as two BCD digits, so each nibble must be 0-9.
if [ -f "$OUT_ICC" ]; then
  ver_byte="$(od -An -tx1 -j 8 -N 1 "$OUT_ICC" 2>/dev/null | tr -d ' \n')"
  case "$ver_byte" in
    [0-9][0-9])
      echo "[PASS] #1845: saved major-version byte 0x$ver_byte is valid BCD"
      ;;
    "")
      echo "[WARN] #1845: could not read the version byte from $OUT_ICC"
      ;;
    *)
      echo "[FAIL] #1845: saved major-version byte 0x$ver_byte is not valid BCD"
      echo "       a negative <ProfileVersion> component wrapped into the header again"
      status=2
      ;;
  esac
else
  # Not a failure on its own: a build may reject the profile before saving.
  echo "[INFO] #1845: no profile written; relying on the sanitizer signal alone"
fi

# --- Signal 2: no implicit-conversion diagnostic in the version parse --------
# Only claim this signal on a build that can actually produce it.
# Build/Cmake/CMakeLists.txt maps ENABLE_UBSAN to -fsanitize=undefined and only
# ENABLE_INTEGER_SANITIZER to -fsanitize=integer, and implicit-integer-sign-change
# lives in the integer group -- so accepting a bare "undefined" here would report
# a confident PASS on a build that cannot observe the defect.
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

if [ "$sanitized" -ne 1 ]; then
  echo "[SKIP] sanitizer signal: not an integer/implicit-conversion build (CMakeCache: ${CACHE:-none})"
elif grep -qE "runtime error: implicit conversion" "$LOG"; then
  echo "[FAIL] #1845: implicit-conversion diagnostic reappeared in the version parse"
  grep -E "runtime error: implicit conversion|IccProfileXml|parseVersion|ParseBasic" "$LOG" | head
  status=2
else
  echo "[PASS] #1845: negative version component parsed without a signed->unsigned conversion"
fi

exit "$status"
