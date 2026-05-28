#!/bin/bash
###############################################################################
# iccFromCube/iccFromXml issue-1148 writer device regression
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1148-writer-device}"
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

FROMCUBE="$TOOLS_DIR/IccFromCube/iccFromCube"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"

fail() {
  echo "  [FAIL] issue-1148-writer-device -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,120p' "$logfile"
    return 1
  fi

  return 0
}

write_cube_pocs() {
  python3 - "$OUTDIR" <<'PY'
import pathlib
import sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
fixtures = {
    "ub-settext-continuation-line7801.cube":
        "54 49 54 4c 45 20 22 54 72 75 6e 63 e1 cf 74 45"
        "64 20 44 61 74 61 22 0a 4c 55 54 5f 33 44 5f 53"
        "49 5a 45 20 32 0a 23 20 4c 55 54 5f 31 44 5f 53"
        "49 5a 45 20 34 0a 44 4f 4d 41 49 4e 5f 4d 49 4e"
        "20 2d cd 33 2d 31 fa 4f 6e 6c 79 20 31 30 20 6f"
        "66 20 32 37 20 6e 26 71 75 69 72 65 44 20 65 30"
        "20 30 2e 30 20 30 2e 30 0a 30 2e 35 20 30 2e d9"
        "df cf d1 cf f5 ce 40 d1 2a 20 32",
    "ub-tagmpe-size-line1158-shared-curves.cube":
        "54 49 54 4c 45 20 22 41 53 43 49 49 22 0a 4c 55"
        "54 5f 33 44 5f 53 49 5a 45 20 32 0a 44 4f 4d 41"
        "49 4e 5f 4d 49 4e 20 2d 31 20 2d 31 20 2d 31 0a"
        "44 4f 4d 41 49 4e 5f 4d 41 58 20 32 20 32 20 32"
        "0a 30 20 30 20 30 0a 31 20 30 20 30 0a 30 20 31"
        "20 30 0a 31 20 31 20 30 0a 30 20 30 20 31 0a 31"
        "20 30 20 31 0a 30 20 31 20 31 0a 31 20 31 20 31"
        "0a",
    "ub-curveset-line3456-distinct-curves.cube":
        "54 49 54 4c 45 20 22 41 53 43 49 49 22 0a 4c 55"
        "54 5f 33 44 5f 53 49 5a 45 20 32 0a 44 4f 4d 41"
        "49 4e 5f 4d 49 4e 20 2d 31 20 2d 32 20 2d 33 0a"
        "44 4f 4d 41 49 4e 5f 4d 41 58 20 32 20 33 20 34"
        "0a 30 20 30 20 30 0a 31 20 30 20 30 0a 30 20 31"
        "20 30 0a 31 20 31 20 30 0a 30 20 30 20 31 0a 31"
        "20 30 20 31 0a 30 20 31 20 31 0a 31 20 31 20 31"
        "0a",
}
for name, hex_data in fixtures.items():
    (out / name).write_bytes(bytes.fromhex(hex_data))
PY
}

run_clean() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 30 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "$name crashed with signal $((rc - 128))"
  fi
}

run_device_failure() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 30 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "$name crashed with signal $((rc - 128))"
  fi
  if [ "$rc" -eq 0 ]; then
    fail "$name unexpectedly succeeded"
  fi
}

if [ ! -x "$FROMCUBE" ]; then
  fail "missing executable: $FROMCUBE"
fi
if [ ! -x "$FROMXML" ]; then
  fail "missing executable: $FROMXML"
fi

write_cube_pocs

run_clean settext-regular "$FROMCUBE" "$OUTDIR/ub-settext-continuation-line7801.cube" "$OUTDIR/settext.icc"
run_device_failure settext-dev-null "$FROMCUBE" "$OUTDIR/ub-settext-continuation-line7801.cube" /dev/null
run_clean tagmpe-regular "$FROMCUBE" "$OUTDIR/ub-tagmpe-size-line1158-shared-curves.cube" "$OUTDIR/tagmpe.icc"
run_device_failure tagmpe-dev-null "$FROMCUBE" "$OUTDIR/ub-tagmpe-size-line1158-shared-curves.cube" /dev/null
run_clean curveset-regular "$FROMCUBE" "$OUTDIR/ub-curveset-line3456-distinct-curves.cube" "$OUTDIR/curveset.icc"
run_device_failure curveset-dev-null "$FROMCUBE" "$OUTDIR/ub-curveset-line3456-distinct-curves.cube" /dev/null

run_device_failure camera-dev-null "$FROMXML" "$TESTING_DIR/Calc/CameraModel.xml" /dev/null
run_device_failure elevenchan-dev-zero "$FROMXML" "$TESTING_DIR/Calc/ElevenChanKubelkaMunk.xml" /dev/zero
run_device_failure camera-dev-full "$FROMXML" "$TESTING_DIR/Calc/CameraModel.xml" /dev/full

echo "  [OK] issue-1148-writer-device -- PoCs fail gracefully without sanitizer diagnostics"
