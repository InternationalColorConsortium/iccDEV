#!/bin/bash
###############################################################################
# iccDEV issue #1337 - CIccCmm::CheckPCSConnections CIccPcsXform ownership
###############################################################################
#
# CheckPCSConnections() builds CIccPcsXform objects to bridge PCS encodings.
# One of the three blocks returned on a hard Connect() failure without deleting
# the locally-owned CIccPcsXform, leaking it (#1337).
#
# This reproduces the leak in isolation (no -PCC, so it is independent of the
# #1336 iccApplyToLink fix): a CMYK overprint profile chained to a spectral
# profile produces an unsupported PCS link, so CheckPCSConnections' middle
# Connect() fails.  The two trigger profiles are generated from committed XML
# sources, then the chain is run under LeakSanitizer.  Pre-fix the CIccPcsXform
# leaks (168 B via CheckPCSConnections); the test fails on any leak report.  The
# tool is expected to exit non-zero (it reports "Unsupported PCS Link"), so the
# pass/fail signal is the absence of a leak, not the tool exit code.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated profiles / logs
#
# Exit codes:
#   0 - pass (or skipped cleanly when tools / XML sources are unavailable)
#   2 - LeakSanitizer finding (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1337}"
HYBRID_DIR="$REPO_ROOT/Testing/hybrid"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
APPLYTOLINK="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ -z "$APPLYTOLINK" ] || [ ! -x "$FROMXML" ] || [ ! -x "$APPLYTOLINK" ]; then
  echo "[SKIP] iccFromXml / iccApplyToLink not found under $TOOLS_DIR"
  exit 0
fi

CMYK_XML="$HYBRID_DIR/CMYK-W_Overprint_Profile.xml"
SPEC_XML="$HYBRID_DIR/Data/Spec380_10_730-D50_2deg.xml"
for f in "$CMYK_XML" "$SPEC_XML"; do
  if [ ! -f "$f" ]; then
    echo "[SKIP] required XML source missing: $f"
    exit 0
  fi
done

CMYK_ICC="$OUTDIR/CMYK-W_Overprint.icc"
SPEC_ICC="$OUTDIR/Spec380_10_730-D50_2deg.icc"

# Generate the trigger profiles.  Run from the hybrid directory so the XML
# include/reference paths resolve.
( cd "$HYBRID_DIR" && "$FROMXML" "CMYK-W_Overprint_Profile.xml" "$CMYK_ICC" ) >"$OUTDIR/fromxml.log" 2>&1
( cd "$HYBRID_DIR" && "$FROMXML" "Data/Spec380_10_730-D50_2deg.xml" "$SPEC_ICC" ) >>"$OUTDIR/fromxml.log" 2>&1
if [ ! -f "$CMYK_ICC" ] || [ ! -f "$SPEC_ICC" ]; then
  echo "[SKIP] could not generate trigger profiles (see $OUTDIR/fromxml.log)"
  exit 0
fi

LOG="$OUTDIR/checkpcsconnections.log"

# Chain a CMYK overprint profile to a spectral profile: the PCS link is
# unsupported, so CheckPCSConnections' middle Connect() fails.  Force leak
# detection on and do not abort on the (expected) chain error.
ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:exitcode=0" \
UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
  "$APPLYTOLINK" "$OUTDIR/link.cube" 1 9 10 Issue1337 0.001 0.999 1 1 \
    "$CMYK_ICC" 1000 "$SPEC_ICC" 12 > "$LOG" 2>&1

if grep -q "LeakSanitizer: detected memory leaks" "$LOG"; then
  echo "[FAIL] CheckPCSConnections leaked a CIccPcsXform on the Connect error path (#1337)"
  grep -E "SUMMARY: AddressSanitizer|CheckPCSConnections|CIccPcsXform" "$LOG" | head
  exit 2
fi

echo "[PASS] CheckPCSConnections released its CIccPcsXform on the error path (no leak)"
exit 0
