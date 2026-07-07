#!/bin/bash
###############################################################################
# iccDEV issue #1336 - iccApplyToLink -PCC profile ownership regression
###############################################################################
#
# iccApplyToLink opens each -PCC profile and tracks it in pccList, releasing
# the list only after cmm.Begin() consumes the connection conditions.  The
# error-exit paths before that point (a -PCC open failure, an AddXform failure,
# or a Begin failure) must release pccList too, otherwise the accumulated -PCC
# profiles leak (#1336).
#
# This drives the AddXform-failure path with tracked profiles: a monitor
# profile is added (input) with a -PCC profile attached, then an abstract
# profile that does not connect to the chain triggers icCmmStatBadSpaceLink and
# the early return.  Pre-fix that return strands the -PCC profile; the run is
# executed under LeakSanitizer and the test fails if a leak is reported.  The
# tool itself is expected to exit non-zero (it is reporting the chain error),
# so the tool's exit code is not the pass/fail signal - the absence of a
# LeakSanitizer report is.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for logs
#
# Exit codes:
#   0 - pass (or skipped cleanly when the tool / profiles are unavailable)
#   2 - LeakSanitizer finding (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1336}"
mkdir -p "$OUTDIR"

BIN="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
  echo "[SKIP] iccApplyToLink binary not found under $TOOLS_DIR"
  exit 0
fi

INPUT="$REPO_ROOT/.github/ci/test-data/fuzz-mntr-ef352586.icc"
PCC="$REPO_ROOT/Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
NOCONNECT="$REPO_ROOT/.github/ci/test-data/fuzz-abst-a044ff69.icc"
for f in "$INPUT" "$PCC" "$NOCONNECT"; do
  if [ ! -f "$f" ]; then
    echo "[SKIP] required profile missing: $f"
    exit 0
  fi
done

LOG="$OUTDIR/iccapplytolink-pcc.log"

# Force leak detection on for this run regardless of the harness default, and
# do not abort on the (expected) chain error so LeakSanitizer runs at exit.
ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
  "$BIN" "$OUTDIR/link.icc" 0 17 1 Issue1336 0.0 1.0 0 1 \
    "$INPUT" 1 -PCC "$PCC" "$NOCONNECT" 1 > "$LOG" 2>&1
status=$?

if grep -q "LeakSanitizer: detected memory leaks" "$LOG"; then
  echo "[FAIL] iccApplyToLink leaked a -PCC profile on the AddXform error path (#1336)"
  grep -E "SUMMARY: AddressSanitizer|CIccProfile::ReadBasic|iccApplyToLink.cpp" "$LOG" | head
  exit 2
fi

echo "[PASS] iccApplyToLink released -PCC profiles on the error path (tool exit ${status}, no leak)"
exit 0
