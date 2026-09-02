#!/usr/bin/env bash
###############################################################################
# Run the complete SpecSep QA gate and a bounded option/output matrix.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd -P)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-specsep-campaign}"
DURATION=300

usage() {
  echo "Usage: $0 [--seconds N] [--output-dir DIR] [--tools-dir DIR]"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --seconds)
      [ "$#" -ge 2 ] || { echo "Missing value for --seconds" >&2; exit 2; }
      DURATION="$2"
      shift 2
      ;;
    --output-dir)
      [ "$#" -ge 2 ] || { echo "Missing value for --output-dir" >&2; exit 2; }
      OUTDIR="$2"
      shift 2
      ;;
    --tools-dir)
      [ "$#" -ge 2 ] || { echo "Missing value for --tools-dir" >&2; exit 2; }
      TOOLS_DIR="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if ! [[ "$DURATION" =~ ^[0-9]+$ ]] || [ "$DURATION" -gt 3600 ]; then
  echo "--seconds must be an integer between 0 and 3600" >&2
  exit 1
fi

if [ ! -d "$TOOLS_DIR" ]; then
  echo "Missing tools directory: $TOOLS_DIR" >&2
  exit 1
fi

# imagecodecs belongs in this guard: half the matrix writes LZW output, and
# tifffile defers LZW decoding to imagecodecs, so without it the run dies part
# way through with "requires the 'imagecodecs' package" instead of here.
if ! python3 -c "import numpy, tifffile, imagecodecs" >/dev/null 2>&1; then
  echo "The extended campaign requires Python numpy, tifffile, and imagecodecs." >&2
  exit 1
fi

mkdir -p "$OUTDIR"

echo "=== SpecSep canonical QA preflight ==="
ICCDEV_TOOLS_DIR="$TOOLS_DIR" \
ICCDEV_TEST_OUTDIR="$OUTDIR/canonical-qa" \
  bash "$SCRIPT_DIR/iccdev-specsep-qa.sh" || exit 1

echo
echo "=== SpecSep ${DURATION}-second option/output matrix ==="
set -o pipefail
python3 "$SCRIPT_DIR/iccdev_specsep_matrix.py" \
  --tools-dir "$TOOLS_DIR" \
  --output-dir "$OUTDIR/matrix" \
  --seconds "$DURATION" 2>&1 | tee "$OUTDIR/campaign.log"
exit_code=$?

echo "Campaign log: $OUTDIR/campaign.log"
echo "Matrix summary: $OUTDIR/matrix/matrix-summary.txt"
echo "Representative inspections: $OUTDIR/matrix/inspection"

exit "$exit_code"
