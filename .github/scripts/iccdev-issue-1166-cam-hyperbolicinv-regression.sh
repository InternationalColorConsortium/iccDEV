#!/bin/bash
###############################################################################
# iccApplyNamedCmm issue-1166 CAM HyperbolicInv divide-by-zero regression
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1166-cam-hyperbolicinv-regression}"
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

APPLY_NAMED_CMM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
POC_JSON="$OUTDIR/dbz-CIccCamConverter-HyperbolicInv-IccCAM_cpp-Line214.json"
POC_ICC="$OUTDIR/dbz-CIccCamConverter-HyperbolicInv-IccCAM_cpp-Line214.icc"
LOGFILE="$OUTDIR/issue-1166-cam-hyperbolicinv.log"

fail() {
  echo "  [FAIL] issue-1166-cam-hyperbolicinv-regression -- $1"
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

generate_poc() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$POC_JSON" "$POC_ICC" <<'PY'
import base64
import pathlib
import sys

json_path = pathlib.Path(sys.argv[1])
icc_path = pathlib.Path(sys.argv[2])
json_path.write_bytes(base64.b64decode(
    "ewogICJkYXRhRmlsZXMiOiB7CiAgICAic3JjVHlwZSI6ICJjb2xvckRhdGEiLAogICAgInNy"
    "Y1NwYWNlIjogIlJHQiAiLAogICAgImRzdFR5cGUiOiAiY29sb3JEYXRhIiwKICAgICJkc3RF"
    "bmNvZGluZyI6ICJmbG9hdCIKICB9LAogICJwcm9maWxlU2VxdWVuY2UiOiBbCiAgICB7CiAg"
    "ICAgICJpY2NGaWxlIjogImRiei1DSWNjQ2FtQ29udmVydGVyLUh5cGVyYm9saWNJbnYtSWNj"
    "Q0FNX2NwcC1MaW5lMjE0LmljYyIsCiAgICAgICJpbnRlbnQiOiAicmVsYXRpdmUiLAogICAg"
    "ICAidHJhbnNmb3JtIjogImRlZmF1bHQiLAogICAgICAiaW50ZXJwb2xhdGlvbiI6ICJsaW5l"
    "YXIiCiAgICB9CiAgXSwKICAiY29sb3JEYXRhIjogewogICAgInNwYWNlIjogIlJHQiAiLAog"
    "ICAgImVuY29kaW5nIjogImZsb2F0IiwKICAgICJzcmNTcGFjZSI6ICJSR0IgIiwKICAgICJz"
    "cmNFbmNvZGluZyI6ICJmbG9hdCIsCiAgICAiZGF0YSI6IFsKICAgICAgewogICAgICAgICJu"
    "IjogIkphYl8xMDBfMF8wIiwKICAgICAgICAidiI6IFsxMDAuMCwgMC4wLCAwLjBdCiAgICAg"
    "IH0KICAgIF0KICB9Cn0K"))
icc_path.write_bytes(base64.b64decode(
    "AAACFAAAAAAFAAAAc3BhY1JHQiBYWVogB+oABQAbAAoAIgAZYWNzcAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAEAAPbWAAEAAAAA0y1JQ0MgnL2TVVoCRnWXuJlKIyAf2AAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFZGVzYwAAAMAAAABkQTJCMQAAASQAAABEQjJBMQAA"
    "AWgAAABUd3RwdAAAAbwAAAAUY3BydAAAAdAAAABCbWx1YwAAAAAAAAABAAAADGVuVVMAAABI"
    "AAAAHABEAEIAWgAgAEMAQQBNACAASgBhAGIAVABvAFgAWQBaACAAUABvAEMAIABBAEEAQQBB"
    "AEEAQQBBAEEAIAAwAHgANAAxADQAMW1wZXQAAAAAAAMAAwAAAAEAAAAYAAAALEp0b1gAAAAA"
    "AAMAAz921dA/gAAAP1MspQAAAAdBoAAAPzCj1z+AAAA/gAAAbXBldAAAAAAAAwADAAAAAQAA"
    "ABgAAAA8bWF0ZgAAAAAAAwADP4AAAAAAAAAAAAAAAAAAAD+AAAAAAAAAAAAAAAAAAAA/gAAA"
    "AAAAAAAAAAAAAAAAWFlaIAAAAAAAAPbWAAEAAAAA0y1tbHVjAAAAAAAAAAEAAAAMZW5VUwAA"
    "ACYAAAAcAEwAbwBjAGEAbAAgAHMAYQBuAGkAdABpAHoAZQByACAAUABvAEMAAA=="))
PY
}

run_reproducer() {
  local exit_code=0

  if [ ! -x "$APPLY_NAMED_CMM" ]; then
    fail "missing executable: $APPLY_NAMED_CMM"
  fi
  if ! generate_poc; then
    fail "failed to generate issue-1166 config and profile"
  fi

  rm -f "$LOGFILE"
  (cd "$OUTDIR" && timeout 30 "$APPLY_NAMED_CMM" -cfg "$(basename "$POC_JSON")" > "$LOGFILE" 2>&1) || exit_code=$?
  if ! check_sanitizers "$LOGFILE"; then
    fail "sanitizer finding"
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail "iccApplyNamedCmm timed out"
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail "iccApplyNamedCmm crashed with signal $((exit_code - 128))"
  fi

  echo "  [PASS] issue-1166-cam-hyperbolicinv-regression -- no sanitizer finding"
}

echo "=== iccApplyNamedCmm issue-1166 CAM HyperbolicInv regression ==="
run_reproducer
exit 0
