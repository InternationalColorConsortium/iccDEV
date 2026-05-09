#!/bin/bash
###############################################################################
# iccDEV hybrid pipeline CTest wrapper
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for logs
#   HYBRID_TIMEOUT     -- timeout in seconds for BuildAndTest.sh, default 600
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [ -d "$REPO_ROOT/IccProfLib" ]; then
  TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/build/Tools}"
  ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
else
  TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/iccDEV/Build/Tools}"
  ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/iccDEV/Testing}"
fi

BUILD_ROOT="$(cd "$TOOLS/.." 2>/dev/null && pwd -P)"
export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-hybrid-pipeline}"
LOGFILE="$OUTDIR/hybrid-01.log"
mkdir -p "$OUTDIR"

HYBRID_DIR="$ICCDEV_TESTING/hybrid"
if [ ! -f "$HYBRID_DIR/BuildAndTest.sh" ]; then
  echo "[FAIL] missing hybrid pipeline script: $HYBRID_DIR/BuildAndTest.sh"
  exit 1
fi

TOOL_DIRS=""
for tooldir in "$TOOLS"/*/; do
  [ -d "$tooldir" ] || continue
  TOOL_DIRS="${TOOL_DIRS:+$TOOL_DIRS:}$tooldir"
done

if [ -z "$TOOL_DIRS" ]; then
  echo "[FAIL] no tool directories found under $TOOLS"
  exit 1
fi

timeout_sec="${HYBRID_TIMEOUT:-600}"
exit_code=0
(
  cd "$HYBRID_DIR"
  PATH="$TOOL_DIRS:$PATH" timeout "$timeout_sec" bash BuildAndTest.sh
) > "$LOGFILE" 2>&1 || exit_code=$?

has_asan=0
has_ubsan=0
if grep -q "ERROR: AddressSanitizer" "$LOGFILE" 2>/dev/null; then
  has_asan=1
fi
if grep -q "runtime error:" "$LOGFILE" 2>/dev/null; then
  has_ubsan=1
fi

if [ "$exit_code" -eq 0 ] && [ "$has_asan" -eq 0 ] && [ "$has_ubsan" -eq 0 ]; then
  echo "[PASS] hybrid pipeline completed without sanitizer findings"
  echo "[INFO] log: $LOGFILE"
  exit 0
fi

if [ "$exit_code" -eq 124 ]; then
  echo "[FAIL] hybrid pipeline timed out after ${timeout_sec}s"
else
  echo "[FAIL] hybrid pipeline exit=$exit_code"
fi
if [ "$has_asan" -eq 1 ]; then
  echo "[FAIL] AddressSanitizer finding detected"
fi
if [ "$has_ubsan" -eq 1 ]; then
  echo "[FAIL] UndefinedBehaviorSanitizer finding detected"
fi
echo "[INFO] log: $LOGFILE"
sed -n '1,120p' "$LOGFILE"
exit 1
