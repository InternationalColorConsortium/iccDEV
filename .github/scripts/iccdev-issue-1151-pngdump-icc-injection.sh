#!/bin/bash
###############################################################################
# iccPngDump issue-1151 ICC injection signed-byte regression
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1151-pngdump-icc-injection}"
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

PNGDUMP="$TOOLS_DIR/IccPngDump/iccPngDump"
INPUT_PNG="$OUTDIR/2x2-rgba--Rec2020rgbColorimetric.png"
INPUT_ICC="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
OUTPUT_PNG="$OUTDIR/foo.bar"
LOGFILE="$OUTDIR/issue-1151-pngdump-icc-injection.log"
EXPECTED_SHA256="98d2010c4d8e71bd4905441d55f86b366f57300d8cd44de2b72a5991c22da51c"

fail() {
  echo "  [FAIL] issue-1151-pngdump-icc-injection -- $1"
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

generate_png() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$INPUT_PNG" "$EXPECTED_SHA256" <<'PY'
import base64
import hashlib
import pathlib
import sys

png_path = pathlib.Path(sys.argv[1])
expected = sys.argv[2]
data = base64.b64decode(
    "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"
    "AQoAJBUCS/JFi5wAAAAASUVORK5CYII=")
digest = hashlib.sha256(data).hexdigest()
if digest != expected:
    raise SystemExit(f"unexpected generated PNG SHA256: {digest}")
png_path.write_bytes(data)
PY
}

run_reproducer() {
  local exit_code=0

  if [ ! -x "$PNGDUMP" ]; then
    fail "missing executable: $PNGDUMP"
  fi
  if [ ! -f "$INPUT_ICC" ]; then
    fail "missing source ICC: $INPUT_ICC"
  fi
  if ! generate_png; then
    fail "failed to generate issue-1151 PNG"
  fi

  rm -f "$LOGFILE" "$OUTPUT_PNG"
  timeout 30 "$PNGDUMP" "$INPUT_PNG" --write-icc "$INPUT_ICC" --output "$OUTPUT_PNG" > "$LOGFILE" 2>&1 || exit_code=$?
  if ! check_sanitizers "$LOGFILE"; then
    fail "sanitizer finding"
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail "iccPngDump timed out"
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail "iccPngDump crashed with signal $((exit_code - 128))"
  fi
  if [ ! -s "$OUTPUT_PNG" ]; then
    fail "iccPngDump did not write output PNG"
  fi

  echo "  [PASS] issue-1151-pngdump-icc-injection -- no sanitizer finding"
}

echo "=== iccPngDump issue-1151 ICC injection regression ==="
run_reproducer
exit 0
