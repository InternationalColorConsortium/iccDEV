#!/bin/bash
###############################################################################
# iccFromCube issue-1379 DeviceLink conformance regression
###############################################################################
#
# iccFromCube built an ICC.2 DeviceLink but only attached copyrightTag when the
# .cube file contained comments, and never attached profileSequenceDescTag -- so
# a comment-less cube produced a profile its own validator rejects with a
# critical error.  This test converts a comment-less cube and asserts the output
# carries both required tags and validates cleanly.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1379-fromcube-conformance}"
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

FROMCUBE="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromCube -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"
CUBE="$OUTDIR/nocomment.cube"
OUT_ICC="$OUTDIR/nocomment.icc"
LOGFILE="$OUTDIR/issue-1379-fromcube-conformance.log"

fail() {
  echo "  [FAIL] issue-1379-fromcube-conformance -- $1"
  exit 1
}

echo "=== iccFromCube issue-1379 DeviceLink conformance regression ==="

if [ -z "$FROMCUBE" ] || [ ! -x "$FROMCUBE" ]; then
  echo "  [SKIP] iccFromCube not built under $TOOLS_DIR"
  exit 0
fi
if [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  echo "  [SKIP] iccDumpProfile not built under $TOOLS_DIR"
  exit 0
fi

# A minimal, comment-less .cube (no '#' lines -> no copyright source).
printf 'LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n' > "$CUBE"

rm -f "$OUT_ICC" "$LOGFILE"
if ! "$FROMCUBE" "$CUBE" "$OUT_ICC" > "$LOGFILE" 2>&1; then
  sed -n '1,40p' "$LOGFILE"
  fail "iccFromCube failed to create a profile from a comment-less cube"
fi
[ -s "$OUT_ICC" ] || fail "iccFromCube produced no output profile"

# Required DeviceLink tags must both be present, even with no cube comments.
"$DUMP" "$OUT_ICC" > "$OUTDIR/dump.log" 2>&1 || true
grep -Fq "'cprt'" "$OUTDIR/dump.log" || fail "copyrightTag (cprt) missing from output (#1379)"
grep -Fq "'pseq'" "$OUTDIR/dump.log" || fail "profileSequenceDescTag (pseq) missing from output (#1379)"

# The profile must validate without a conformance violation.
"$DUMP" -v "$OUT_ICC" > "$OUTDIR/validate.log" 2>&1 || true
if grep -qE "Critical Error|violate the ICC specification|NonCompliant" "$OUTDIR/validate.log"; then
  grep -iE "Validation|Critical|violate|NonCompliant|Required" "$OUTDIR/validate.log" | head -10
  fail "output DeviceLink does not validate (#1379)"
fi
grep -Fq "Profile is valid" "$OUTDIR/validate.log" || fail "validator did not report the profile as valid"

echo "  [PASS] issue-1379-fromcube-conformance -- comment-less cube yields a conformant DeviceLink"
exit 0
