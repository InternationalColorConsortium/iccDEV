#!/bin/bash
###############################################################################
# RunJsonTests temporary-file isolation regression (#3070).
#
# Eight copies process the same tracked profile concurrently. Before #3070,
# every copy used /tmp/json-rt-test.{json,icc}; one copy could remove another
# copy's output between its existence check and size read.
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-3070-json-temp-isolation}"
RUNNER="$TESTING_DIR/RunJsonTests.sh"
FIXTURE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"

mkdir -p "$OUTDIR"
WORKDIR="$(mktemp -d "$OUTDIR/work.XXXXXX")"

cleanup() {
  rm -f -- "$WORKDIR/RunJsonTests.sh" \
    "$WORKDIR/sRGB_v4_ICC_preference.icc" \
    "$WORKDIR"/run-*.log
  rmdir -- "$WORKDIR" 2>/dev/null || true
}
trap cleanup EXIT

if [ ! -x "$TOJSON" ] || [ ! -x "$FROMJSON" ]; then
  echo "[SKIP] JSON tools are unavailable under $TOOLS_DIR"
  exit 77
fi
if [ ! -x "$RUNNER" ] || [ ! -s "$FIXTURE" ]; then
  echo "[FAIL] RunJsonTests.sh or its tracked profile fixture is unavailable" >&2
  exit 1
fi

cp -- "$RUNNER" "$FIXTURE" "$WORKDIR/"

pids=()
logs=()
for index in 1 2 3 4 5 6 7 8; do
  log="$WORKDIR/run-$index.log"
  "$WORKDIR/RunJsonTests.sh" "$TOOLS_DIR" >"$log" 2>&1 &
  pids+=("$!")
  logs+=("$log")
done

failed=0
for index in "${!pids[@]}"; do
  if wait "${pids[$index]}"; then
    log="${logs[$index]}"
    if ! grep -Eq '^Total:[[:space:]]+1$' "$log" ||
       ! grep -Eq '^Pass:[[:space:]]+1$' "$log" ||
       ! grep -Eq '^Fail:[[:space:]]+0 ' "$log" ||
       ! grep -Eq '^Skip:[[:space:]]+0$' "$log" ||
       ! grep -Eq '^XFail:[[:space:]]+0$' "$log"; then
      echo "[FAIL] concurrent run $((index + 1)) did not exercise exactly one profile" >&2
      sed -n '/^=== Summary ===$/,$p' "$log" >&2
      failed=$((failed + 1))
    fi
  else
    status=$?
    echo "[FAIL] concurrent run $((index + 1)) exited $status" >&2
    failed=$((failed + 1))
  fi
done

if [ "$failed" -ne 0 ]; then
  echo "[FAIL] $failed of 8 concurrent RunJsonTests.sh runs failed" >&2
  exit 1
fi

echo "[PASS] 8 concurrent RunJsonTests.sh runs kept temporary files isolated"
