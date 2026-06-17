#!/bin/bash
###############################################################################
# iccRoundTrip issue-1405 unbounded device-space round-trip sampling (DoS)
###############################################################################
#
# CIccEvalCompare::EvaluateProfile() walks an nGran^ndim grid of device samples,
# running two CMM Apply() calls at every node.  The count explodes geometrically
# with the channel count: a 6-channel profile at the default granularity of 33
# is 33^6 ~= 1.3 billion samples, and the evaluation ran for over an hour before
# returning -- a CWE-400 uncontrolled-resource-consumption hazard reachable from
# the iccRoundTrip tool with any high-channel (or fuzzer-crafted) profile.  The
# evaluator now rejects an over-budget sample count up front and returns
# icCmmStatTooManySamples ("Too many samples used"), which iccRoundTrip surfaces.
#
# This test regenerates the 6-channel SixChanCameraRef profile from its tracked
# XML (the .icc artifacts are .gitignored) and runs iccRoundTrip under a short
# timeout: pre-fix the tool never terminates (the timeout fires); post-fix it
# returns immediately reporting "Too many samples used".  A normal 3-channel
# profile (33^3 = 35937 samples, far under the cap) must still round-trip, so the
# cap is proven not over-aggressive.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 2 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1405-roundtrip-sample-cap}"
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
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

ROUNDTRIP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccRoundTrip -type f 2>/dev/null | head -1)"
FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
SIXCHAN_XML="$REPO_ROOT/Testing/SpecRef/SixChanCameraRef.xml"
RGB_ICC="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"
LOGFILE="$OUTDIR/issue-1405-roundtrip-sample-cap.log"

# Post-fix the over-budget profile is rejected in well under a second; pre-fix it
# runs for over an hour.  Any modest ceiling cleanly separates the two.
DOS_TIMEOUT=45

regress() {
  echo "  [FAIL] issue-1405-roundtrip-sample-cap -- $1"
  exit 2
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null; then
    sed -n '1,80p' "$1"
    return 1
  fi
  return 0
}

echo "=== iccRoundTrip issue-1405 unbounded round-trip sampling regression ==="

if [ -z "$ROUNDTRIP" ] || [ ! -x "$ROUNDTRIP" ]; then
  echo "  [SKIP] iccRoundTrip not built under $TOOLS_DIR"
  exit 0
fi
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "  [SKIP] iccFromXml not built under $TOOLS_DIR (needed to build the 6-channel fixture)"
  exit 0
fi
if [ ! -f "$SIXCHAN_XML" ]; then
  echo "  [SKIP] $SIXCHAN_XML missing"
  exit 0
fi

# 1) Build the 6-channel (6CLR) fixture from its tracked XML, then round-trip it.
#    Pre-fix: 33^6 samples never terminate -> timeout fires (rc 124).
#    Post-fix: rejected up front with "Too many samples used".
SIXCHAN_ICC="$OUTDIR/SixChanCameraRef.icc"
rm -f "$SIXCHAN_ICC" "$LOGFILE"
if ! "$FROMXML" "$SIXCHAN_XML" "$SIXCHAN_ICC" > "$LOGFILE" 2>&1 || [ ! -s "$SIXCHAN_ICC" ]; then
  sed -n '1,20p' "$LOGFILE"
  echo "  [SKIP] could not generate 6-channel fixture from XML"
  exit 0
fi

timeout "$DOS_TIMEOUT" "$ROUNDTRIP" "$SIXCHAN_ICC" > "$LOGFILE" 2>&1
rc=$?
check_sanitizers "$LOGFILE" || regress "sanitizer error round-tripping the 6-channel profile"

if [ "$rc" -eq 124 ]; then
  regress "6-channel round trip did not terminate within ${DOS_TIMEOUT}s -- sample count (33^6) uncapped"
fi
if ! grep -q "Too many samples used" "$LOGFILE"; then
  sed -n '1,20p' "$LOGFILE"
  regress "6-channel round trip was not rejected with 'Too many samples used' (rc=$rc)"
fi

# 2) Positive control: a normal 3-channel profile (33^3 << cap) must still
#    round-trip cleanly, proving the cap rejects nothing legitimate.
if [ -f "$RGB_ICC" ]; then
  timeout 120 "$ROUNDTRIP" "$RGB_ICC" > "$LOGFILE" 2>&1
  rc=$?
  check_sanitizers "$LOGFILE" || regress "sanitizer error round-tripping the 3-channel control profile"
  if [ "$rc" -ne 0 ] || ! grep -q "Round Trip 1" "$LOGFILE"; then
    sed -n '1,20p' "$LOGFILE"
    regress "3-channel control profile failed to round-trip (rc=$rc) -- cap too aggressive"
  fi
else
  echo "  [INFO] $RGB_ICC missing -- skipping positive control"
fi

echo "  [PASS] issue-1405-roundtrip-sample-cap -- 6-channel over-budget refused, 3-channel still round-trips"
exit 0
