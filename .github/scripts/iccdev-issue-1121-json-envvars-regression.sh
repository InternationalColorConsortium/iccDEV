#!/bin/bash
###############################################################################
# iccApplyNamedCmm issue-1121 malformed JSON env-var regression
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1121-json-envvars-regression}"
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
NAMEDCMM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
SOURCE_XML="$TESTING_DIR/Display/sRGB_D65_MAT.xml"
PROFILE="$OUTDIR/sRGB_D65_MAT.icc"
CFG="$OUTDIR/cfg.json"
FROMXML_LOG="$OUTDIR/fromxml.log"
NAMEDCMM_LOG="$OUTDIR/issue-1121-json-envvars.log"

fail() {
  echo "  [FAIL] issue-1121-json-envvars-regression -- $1"
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

generate_inputs() {
  if [ ! -x "$FROMXML" ]; then
    fail "missing executable: $FROMXML"
  fi
  if [ ! -f "$SOURCE_XML" ]; then
    fail "missing source XML: $SOURCE_XML"
  fi

  rm -f "$PROFILE" "$CFG" "$FROMXML_LOG"
  timeout 60 "$FROMXML" "$SOURCE_XML" "$PROFILE" > "$FROMXML_LOG" 2>&1 || return 1
  check_sanitizers "$FROMXML_LOG" || return 1

  printf '%s\n' '{"dataFiles":{"srcType":"colorData","srcFile":"","dstType":"colorData","dstFile":"","dstEncoding":"value"},"profileSequence":[{"iccFile":"sRGB_D65_MAT.icc","intent":1,"iccEnvVars":[{}]}],"colorData":{"space":"RGB ","encoding":"float","data":[{"values":[1,1,1]}]}}' > "$CFG"
}

run_reproducer() {
  local exit_code=0

  if [ ! -x "$NAMEDCMM" ]; then
    fail "missing executable: $NAMEDCMM"
  fi
  if ! generate_inputs; then
    fail "failed to generate issue-1121 inputs"
  fi

  rm -f "$NAMEDCMM_LOG"
  (cd "$OUTDIR" && timeout 30 "$NAMEDCMM" -cfg "$CFG" > "$NAMEDCMM_LOG" 2>&1) || exit_code=$?
  if ! check_sanitizers "$NAMEDCMM_LOG"; then
    fail "sanitizer finding"
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail "iccApplyNamedCmm timed out"
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail "iccApplyNamedCmm crashed with signal $((exit_code - 128))"
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail "iccApplyNamedCmm accepted malformed JSON env-vars"
  fi
  if ! grep -q "Unable to parse profileSequence" "$NAMEDCMM_LOG"; then
    sed -n '1,80p' "$NAMEDCMM_LOG"
    fail "iccApplyNamedCmm did not report profileSequence parse failure"
  fi

  echo "  [PASS] issue-1121-json-envvars-regression -- malformed config failed gracefully"
}

echo "=== iccApplyNamedCmm issue-1121 JSON env-vars regression ==="
run_reproducer
exit 0
