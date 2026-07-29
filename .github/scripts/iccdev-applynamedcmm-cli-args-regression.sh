#!/bin/bash
###############################################################################
# iccApplyNamedCmm command-line argument regression tests
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for generated logs/configs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-applynamedcmm-cli-args}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyNamedCmm -type f 2>/dev/null | head -1)"
DATA="$TESTING_DIR/ApplyDataFiles/rgb8bit.txt"
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
CFG="$OUTDIR/minimal-cfg.json"

fail() {
  echo "  [FAIL] iccApplyNamedCmm-cli-args -- $1"
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

require_file() {
  local path="$1"

  if [ ! -f "$path" ]; then
    fail "missing fixture: $path"
  fi
}

require_tool() {
  local path="$1"

  if [ ! -x "$path" ]; then
    fail "missing executable: $path"
  fi
}

run_expect_success() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ne 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name failed with exit=$rc"
  fi
}

run_expect_reject() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
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
    sed -n '1,80p' "$logfile"
    fail "$name accepted malformed arguments"
  fi
}

require_tool "$APPLY"
require_file "$DATA"
require_file "$SRGB"

cat > "$CFG" <<EOF_CFG
{
  "dataFiles": {
    "srcType": "legacy",
    "srcFile": "$DATA",
    "dstType": "legacy",
    "dstEncoding": "value"
  },
  "profileSequence": [
    {
      "iccFile": "$SRGB",
      "intent": "relative",
      "useD2BxB2Dx": true
    }
  ]
}
EOF_CFG

echo "=== iccApplyNamedCmm CLI argument regression ==="

run_expect_success valid-legacy "$APPLY" "$DATA" 0 0 "$SRGB" 1
run_expect_success valid-cfg "$APPLY" -cfg "$CFG"

run_expect_reject cfg-extra "$APPLY" -cfg "$CFG" ignored-extra
run_expect_reject normal-extra "$APPLY" "$DATA" 0 0 "$SRGB" 1 ignored-extra
run_expect_reject precision-junk "$APPLY" "$DATA" 0:abc:def 0 "$SRGB" 1
run_expect_reject precision-huge "$APPLY" "$DATA" 0:999999:999999 0 "$SRGB" 1
run_expect_reject interp-junk "$APPLY" "$DATA" 0 junk "$SRGB" 1
run_expect_reject interp-out-of-range "$APPLY" "$DATA" 0 2 "$SRGB" 1
run_expect_reject intent-junk "$APPLY" "$DATA" 0 0 "$SRGB" 1junk
run_expect_reject env-short-name "$APPLY" "$DATA" 0 0 -ENV:x 1 "$SRGB" 1
run_expect_reject env-long-name "$APPLY" "$DATA" 0 0 -ENV:abcdefgh 1 "$SRGB" 1
run_expect_reject env-junk "$APPLY" "$DATA" 0 0 -ENV:wtpt 1junk "$SRGB" 1
run_expect_reject env-nan "$APPLY" "$DATA" 0 0 -ENV:wtpt nan "$SRGB" 1
run_expect_reject env-inf "$APPLY" "$DATA" 0 0 -ENV:wtpt inf "$SRGB" 1
run_expect_reject dangling-pcc "$APPLY" "$DATA" 0 0 "$SRGB" 1 -PCC

echo "  [PASS] iccApplyNamedCmm-cli-args"
exit 0
