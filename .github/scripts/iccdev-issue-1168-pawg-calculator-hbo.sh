#!/bin/bash
###############################################################################
# iccPawgReport issue-1168 calculator bounds regression
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1168-pawg-calculator-hbo}"
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
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
PAWG="$TOOLS_DIR/IccPawgReport/iccPawgReport"
SOURCE_XML="$TESTING_DIR/CalcTest/calcExercizeOps.xml"
BASE_PROFILE="$OUTDIR/calcExercizeOps.icc"
POC_PROFILE="$OUTDIR/hbo-CIccCalculatorFunc-CIccCalculatorFunc-IccMpeCalc_cpp-Line3789.icc"
FROMXML_LOG="$OUTDIR/fromxml.log"
PAWG_LOG="$OUTDIR/pawg.log"
EXPECTED_SHA256="dc5d5e6e9d93c806ca4586700d3afd74e9b8bb66d7278c0dfa245d93924e2b9b"

fail() {
  echo "  [FAIL] issue-1168-pawg-calculator-hbo -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,80p' "$logfile"
    return 1
  fi

  return 0
}

generate_issue_1168_profile() {
  if [ ! -x "$FROMXML" ]; then
    fail "missing executable: $FROMXML"
  fi
  if [ ! -f "$SOURCE_XML" ]; then
    fail "missing source XML: $SOURCE_XML"
  fi

  rm -f "$BASE_PROFILE" "$POC_PROFILE" "$FROMXML_LOG"
  timeout 60 "$FROMXML" "$SOURCE_XML" "$BASE_PROFILE" > "$FROMXML_LOG" 2>&1 || return 1
  check_sanitizers "$FROMXML_LOG" || return 1

  python3 - "$BASE_PROFILE" "$POC_PROFILE" "$EXPECTED_SHA256" <<'PY'
import hashlib
import pathlib
import sys

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
expected = sys.argv[3]
data = bytearray(src.read_bytes())
mutations = {
    27: 0x03,
    29: 0x04,
    31: 0x10,
    33: 0x34,
    35: 0x05,
    84: 0x58,
    85: 0xfe,
    86: 0xd8,
    87: 0x11,
    88: 0x83,
    89: 0xe6,
    90: 0x73,
    91: 0xcf,
    92: 0x80,
    93: 0x57,
    94: 0xf7,
    95: 0x83,
    96: 0xdb,
    97: 0xb9,
    98: 0x44,
    99: 0xdb,
    143: 0x3c,
    227: 0x78,
    251: 0x20,
    386: 0x0f,
    5106: 0x0b,
    5114: 0x0a,
    5184: 0x10,
    325674: 0x35,
    449343: 0x01,
    449346: 0x60,
    651995: 0x00,
    653383: 0x5c,
}
for offset, value in mutations.items():
    data[offset] = value
digest = hashlib.sha256(data).hexdigest()
if digest != expected:
    raise SystemExit(f"unexpected generated profile SHA256: {digest}")
dst.write_bytes(data)
PY
}

run_reproducer() {
  local exit_code=0

  if [ ! -x "$PAWG" ]; then
    fail "missing executable: $PAWG"
  fi
  if ! generate_issue_1168_profile; then
    fail "failed to generate issue-1168 reproducer"
  fi

  rm -f "$PAWG_LOG"
  timeout 60 "$PAWG" "$POC_PROFILE" > "$PAWG_LOG" 2>&1 || exit_code=$?
  if ! check_sanitizers "$PAWG_LOG"; then
    fail "sanitizer finding"
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail "iccPawgReport timed out"
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail "iccPawgReport crashed with signal $((exit_code - 128))"
  fi

  echo "  [PASS] issue-1168-pawg-calculator-hbo -- no sanitizer finding"
}

echo "=== iccPawgReport issue-1168 calculator bounds regression ==="
run_reproducer
exit 0
