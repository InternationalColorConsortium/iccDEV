#!/bin/bash
###############################################################################
# CAM ImpactSurround range regression for JabToXYZElement and XYZToJabElement
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-cam-impact-surround-regression}"
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
DUMPPROFILE="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
BASE_XML="$TESTING_DIR/Calc/srgbCalcTest.xml"
BAD_XML="$OUTDIR/srgbCalcTest-bad-surround.xml"
GOOD_ICC="$OUTDIR/srgbCalcTest-good.icc"
BAD_ICC="$OUTDIR/srgbCalcTest-bad-surround.icc"
FROMXML_BAD_LOG="$OUTDIR/fromxml-bad-surround.log"
FROMXML_GOOD_LOG="$OUTDIR/fromxml-good.log"
DUMP_LOG="$OUTDIR/dump-bad-surround.log"

fail() {
  echo "  [FAIL] cam-impact-surround-regression -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,100p' "$logfile"
    return 1
  fi

  return 0
}

require_tool() {
  local tool="$1"

  if [ ! -x "$tool" ]; then
    fail "missing executable: $tool"
  fi
}

make_bad_xml() {
  if [ ! -f "$BASE_XML" ]; then
    fail "missing base XML: $BASE_XML"
  fi

  python3 - "$BASE_XML" "$BAD_XML" <<'PY'
import pathlib
import sys

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
text = src.read_text()
old = "<ImpactSurround>0.69</ImpactSurround>"
new = "<ImpactSurround>1.0e20</ImpactSurround>"
if old not in text:
    raise SystemExit("base XML does not contain expected ImpactSurround value")
dst.write_text(text.replace(old, new))
PY
}

make_bad_binary_profile() {
  timeout 60 "$FROMXML" "$BASE_XML" "$GOOD_ICC" > "$FROMXML_GOOD_LOG" 2>&1 || return 1
  check_sanitizers "$FROMXML_GOOD_LOG" || return 1

  cp "$GOOD_ICC" "$BAD_ICC"
  python3 - "$BAD_ICC" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
data = bytearray(path.read_bytes())
patched = 0
for sig in (b"JtoX", b"XtoJ"):
    start = 0
    while True:
        pos = data.find(sig, start)
        if pos < 0:
            break
        if pos + 36 <= len(data):
            data[pos + 32:pos + 36] = struct.pack(">f", 1.0e20)
            patched += 1
        start = pos + 4
if patched == 0:
    raise SystemExit("no CAM elements found to patch")
path.write_bytes(data)
PY
}

run_reproducer() {
  local exit_code=0

  require_tool "$FROMXML"
  require_tool "$DUMPPROFILE"
  make_bad_xml

  rm -f "$FROMXML_BAD_LOG" "$BAD_ICC"
  timeout 60 "$FROMXML" "$BAD_XML" "$BAD_ICC" > "$FROMXML_BAD_LOG" 2>&1 || exit_code=$?
  check_sanitizers "$FROMXML_BAD_LOG" || fail "sanitizer finding while parsing bad XML"
  if [ "$exit_code" -eq 0 ]; then
    fail "iccFromXml accepted out-of-range CAM ImpactSurround"
  fi
  if ! grep -q "ImpactSurround" "$FROMXML_BAD_LOG"; then
    sed -n '1,100p' "$FROMXML_BAD_LOG"
    fail "iccFromXml did not identify ImpactSurround"
  fi

  make_bad_binary_profile || fail "failed to create binary CAM reproducer"
  rm -f "$DUMP_LOG"
  timeout 60 "$DUMPPROFILE" -v 100 "$BAD_ICC" ALL > "$DUMP_LOG" 2>&1 || true
  check_sanitizers "$DUMP_LOG" || fail "sanitizer finding while validating bad ICC"
  if ! grep -q "Impact Surround must be in \\[0.0, 1.0\\]" "$DUMP_LOG"; then
    sed -n '1,160p' "$DUMP_LOG"
    fail "iccDumpProfile did not report out-of-range Impact Surround"
  fi

  echo "  [PASS] cam-impact-surround-regression -- out-of-range ImpactSurround rejected"
}

echo "=== CAM ImpactSurround range regression ==="
run_reproducer
exit 0
