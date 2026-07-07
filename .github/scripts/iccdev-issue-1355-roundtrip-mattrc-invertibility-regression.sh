#!/bin/bash
###############################################################################
# iccDEV issue #1355 - matrix/TRC (PCSXYZ) profiles must round-trip near-exactly
###############################################################################
#
# A matrix/TRC display profile is an analytic, exactly-invertible transform, so a
# device->PCS->device round trip should return ~0 dE. iccRoundTrip reported ~20 dE
# mean for every such profile, which traced to two independent defects (both fixed
# in the PR that adds this test):
#
#   1. CIccPcsXform::ConnectFirst (IccCmm.cpp) omitted the actual->internal XYZ
#      rescale (pushXyzToXyzIn) after a Lab->XYZ step, so Lab->device through an
#      XYZ-PCS profile was ~2x off (factor 65535/32768). Central CMM bug.
#   2. CIccEvalCompare::EvaluateProfile (IccEval.cpp) forced the round-trip
#      connection space to Lab even for XYZ-PCS profiles, injecting a numerically
#      lossy XYZ<->Lab (L* cube-root) conversion pair into every leg; it now
#      connects through the profile's native PCS.
#
# This test runs iccRoundTrip on committed matrix/TRC PCSXYZ fixtures and FAILS if
# RT1 mean dE exceeds THRESHOLD (was ~20 before the fix, ~0 after).
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#
# Exit codes:
#   0 - pass, or skipped cleanly (tool / fixtures unavailable)
#   2 - regression: a matrix/TRC PCSXYZ profile failed to round-trip
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
THRESHOLD=1.0

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

RT="$(find_tool iccRoundTrip)"
if [ -z "$RT" ] || [ ! -x "$RT" ]; then
  echo "[SKIP] iccRoundTrip not built under $TOOLS_DIR"
  exit 0
fi

# Committed matrix/TRC RGB profiles whose PCS is XYZ -- exactly invertible, so RT1
# mean dE must be ~0. All three exhibited ~20 dE before the fix.
FIXTURES=(
  ".github/ci/regression/gamma-2.20703125.icc"
  ".github/ci/regression/para-gamma-2.4.icc"
  ".github/ci/test-data/v5dspobs-lcddisplay.icc"
)

rc=0
ran=0
for f in "${FIXTURES[@]}"; do
  p="$REPO_ROOT/$f"
  if [ ! -f "$p" ]; then
    echo "[SKIP] missing fixture: $f"
    continue
  fi
  ran=$((ran + 1))
  mean="$("$RT" "$p" 1 | awk '/Round Trip 1/{x=1} x&&/Mean DeltaE/{print $3; exit}')"
  if [ -z "$mean" ]; then
    echo "[FAIL] $f: could not parse RT1 mean dE"
    rc=2
    continue
  fi
  if awk "BEGIN{exit !($mean > $THRESHOLD)}"; then
    echo "[FAIL] $f: RT1 mean dE=$mean > $THRESHOLD (Lab<->XYZ round-trip regression)"
    rc=2
  else
    echo "[PASS] $f: RT1 mean dE=$mean"
  fi
done

if [ "$ran" -eq 0 ]; then
  echo "[SKIP] no fixtures available"
  exit 0
fi

exit $rc
