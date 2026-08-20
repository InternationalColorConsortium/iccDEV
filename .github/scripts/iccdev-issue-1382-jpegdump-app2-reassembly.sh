#!/bin/bash
###############################################################################
# iccJpegDump issue-1382 APP2 ICC chunk reassembly regression
###############################################################################
#
# iccJpegDump located the embedded ICC profile by raw-scanning the JPEG bytes for
# the 'acsp' signature and stopped at the first APP2 segment, so it could not
# handle ICC profiles split across multiple APP2 segments (the standard way
# profiles larger than ~64 KB are embedded).  The tool now parses APP2
# "ICC_PROFILE" segments, reassembles their chunks in order, and chunks the
# profile on injection.  This test round-trips a single-segment profile and a
# multi-segment (>64 KB) payload, asserting byte-for-byte fidelity.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1382-jpegdump-app2-reassembly}"
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

JPEGDUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccJpegDump -type f 2>/dev/null | head -1)"
INPUT_ICC="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
BASE_JPEG="$OUTDIR/base.jpg"
LOGFILE="$OUTDIR/issue-1382-jpegdump-app2-reassembly.log"

fail() {
  echo "  [FAIL] issue-1382-jpegdump-app2-reassembly -- $1"
  exit 1
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null; then
    sed -n '1,80p' "$1"
    return 1
  fi
  return 0
}

# inject $1(icc) into the base JPEG -> $2(out.jpg), extract -> $3(out.icc),
# and require the extracted bytes to equal the injected bytes.
roundtrip() {
  local icc="$1" jpg="$2" icc_out="$3"
  rm -f "$jpg" "$icc_out" "$LOGFILE"
  "$JPEGDUMP" "$BASE_JPEG" --write-icc "$icc" --output "$jpg" > "$LOGFILE" 2>&1 || return 1
  "$JPEGDUMP" "$jpg" "$icc_out" >> "$LOGFILE" 2>&1 || return 1
  check_sanitizers "$LOGFILE" || return 1
  cmp -s "$icc" "$icc_out"
}

echo "=== iccJpegDump issue-1382 APP2 ICC chunk reassembly regression ==="

if [ -z "$JPEGDUMP" ] || [ ! -x "$JPEGDUMP" ]; then
  echo "  [SKIP] iccJpegDump not built under $TOOLS_DIR"
  exit 0
fi
if [ ! -f "$INPUT_ICC" ]; then
  echo "  [SKIP] missing input ICC: $INPUT_ICC"
  exit 0
fi

# Minimal valid JPEG container: SOI + EOI.  iccJpegDump prepends the ICC APP2
# segment(s) after SOI and copies the rest, so this is sufficient to round-trip.
printf '\xff\xd8\xff\xd9' > "$BASE_JPEG"

# 1) Single-segment profile (sRGB, ~60 KB) must round-trip byte-for-byte.
if ! roundtrip "$INPUT_ICC" "$OUTDIR/single.jpg" "$OUTDIR/single_out.icc"; then
  sed -n '1,40p' "$LOGFILE"
  fail "single-segment ICC profile did not round-trip"
fi

# 2) Multi-segment payload (>64 KB) must be split across APP2 segments on inject
#    and reassembled on extract.  Use a deterministic 140 KB blob (3 chunks).
BIG="$OUTDIR/big.bin"
head -c 140000 /dev/zero | tr '\0' 'A' > "$BIG"
if ! roundtrip "$BIG" "$OUTDIR/multi.jpg" "$OUTDIR/multi_out.icc"; then
  sed -n '1,40p' "$LOGFILE"
  fail "multi-segment payload did not round-trip (APP2 reassembly)"
fi

# The injected JPEG must actually contain more than one ICC_PROFILE APP2 segment.
chunks="$(grep -a -o "ICC_PROFILE" "$OUTDIR/multi.jpg" 2>/dev/null | wc -l | tr -d ' ')"
if [ "${chunks:-0}" -lt 2 ]; then
  fail "expected multiple ICC_PROFILE APP2 segments, found ${chunks:-0}"
fi

# 3) Marker-walk edge case: a segment declaring the minimum length of 2 -- an empty
#    payload -- as the last thing in the file leaves the walk's read cursor exactly
#    at end-of-buffer.  The segment pointer is taken there before the payload length
#    is examined, so it must be formed as data() + pos, not &data[pos]: subscripting
#    one past the end is undefined and a hardened libstdc++ build aborts on it.
#    This case therefore goes red only under _GLIBCXX_ASSERTIONS / _GLIBCXX_DEBUG;
#    on an ordinary build it pins the orderly refusal that must follow.
EDGE_JPEG="$OUTDIR/zero-length-app2-at-eof.jpg"
EDGE_LOG="$OUTDIR/zero-length-app2-at-eof.log"
printf '\xff\xd8\xff\xe2\x00\x02' > "$EDGE_JPEG"
rm -f "$EDGE_LOG" "$OUTDIR/edge_out.icc"
edge_rc=0
"$JPEGDUMP" "$EDGE_JPEG" "$OUTDIR/edge_out.icc" > "$EDGE_LOG" 2>&1 || edge_rc=$?
if ! check_sanitizers "$EDGE_LOG"; then
  fail "zero-length APP2 at end-of-file produced a sanitizer finding"
fi
# A signal lands in 128..192; the tool's own refusal path is EXIT_FAILURE, so an
# exit inside that band is a crash and anything else is a decision the tool made.
if [ "$edge_rc" -ge 128 ] && [ "$edge_rc" -le 192 ]; then
  sed -n '1,40p' "$EDGE_LOG"
  fail "zero-length APP2 at end-of-file crashed with signal $((edge_rc - 128))"
fi
if [ "$edge_rc" -ne 1 ]; then
  sed -n '1,40p' "$EDGE_LOG"
  fail "zero-length APP2 at end-of-file expected EXIT_FAILURE, got $edge_rc"
fi
if ! grep -F -q "No ICC_PROFILE APP2 marker found." "$EDGE_LOG"; then
  sed -n '1,40p' "$EDGE_LOG"
  fail "zero-length APP2 at end-of-file did not report the expected diagnostic"
fi

echo "  [PASS] issue-1382-jpegdump-app2-reassembly -- single + multi-segment ICC round-trip (${chunks} chunks), zero-length APP2 at EOF refused cleanly"
exit 0
