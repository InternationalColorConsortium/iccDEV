#!/usr/bin/env bash
###############################################################################
# Run the complete repository-owned iccSpecSepToTiff QA surface.
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd -P)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-specsep-qa}"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  echo "Usage: ICCDEV_TOOLS_DIR=/path/to/Build/Tools $0"
  exit 0
elif [ "$#" -ne 0 ]; then
  echo "Unknown argument: $1" >&2
  exit 1
fi

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

require_file() {
  if [ ! -s "$1" ]; then
    echo "Missing required SpecSep QA file: $1" >&2
    return 1
  fi
}

preflight_failed=0
for channel in {1..10}; do
  require_file "$REPO_ROOT/.github/ci/test-data/spectral/spec_$channel" ||
    preflight_failed=1
done
for channel in {1..8}; do
  require_file "$REPO_ROOT/.github/ci/test-data/specsep-harvest/gray300/spec_$channel" ||
    preflight_failed=1
done
require_file "$REPO_ROOT/.github/ci/test-data/specsep-truncated/spec_1" ||
  preflight_failed=1
require_file "$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc" ||
  preflight_failed=1
require_file "$REPO_ROOT/Testing/ICS/Spec400_10_700-D50_2deg-Part1.xml" ||
  preflight_failed=1
require_file "$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff" ||
  preflight_failed=1
require_file "$TOOLS_DIR/IccTiffDump/iccTiffDump" || preflight_failed=1
require_file "$TOOLS_DIR/IccFromXml/iccFromXml" || preflight_failed=1

if [ "$preflight_failed" -ne 0 ]; then
  echo "Build the iccDEV tools and restore the checked-in fixtures before running QA." >&2
  exit 1
fi

mkdir -p "$OUTDIR"

PASS=0
FAIL=0

run_suite() {
  local name="$1"
  local script="$2"
  local suite_outdir="$OUTDIR/$3"
  shift 3

  echo
  echo "=== $name ==="
  if ICCDEV_TOOLS_DIR="$TOOLS_DIR" ICCDEV_TEST_OUTDIR="$suite_outdir" \
      bash "$SCRIPT_DIR/$script" "$@"; then
    PASS=$((PASS + 1))
  else
    FAIL=$((FAIL + 1))
  fi
}

echo "iccSpecSepToTiff repository QA"
echo "repo: $REPO_ROOT"
echo "tools: $TOOLS_DIR"
echo "output: $OUTDIR"

run_suite "CLI arguments" \
  "iccdev-specsep-cli-args-regression-tests.sh" "cli-args"
run_suite "TIFF geometry" \
  "iccdev-specsep-tiff-geometry-regression-tests.sh" "tiff-geometry"
run_suite "Usage and metadata" \
  "iccdev-issue-1514-specsep-usage-exit-code-regression-tests.sh" "usage"
run_suite "Numbered fixture option matrix" \
  "iccdev-specsep-corpus-matrix.sh" "corpus-matrix"
run_suite "Optional ICC profile sweep (rejection corpus)" \
  "iccdev-specsep-profile-sweep.sh" "profile-sweep" \
  --profile-dir "$REPO_ROOT/Testing/CalcTest"
run_suite "Optional ICC profile sweep (acceptance)" \
  "iccdev-specsep-profile-sweep.sh" "profile-sweep-accept" \
  --profile-dir "$REPO_ROOT/Testing" --channels 3

echo
echo "iccSpecSepToTiff QA suites: $PASS passed, $FAIL failed, $((PASS + FAIL)) total"
echo "Generated TIFFs and logs: $OUTDIR"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
