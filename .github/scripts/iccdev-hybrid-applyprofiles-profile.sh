#!/bin/bash
###############################################################################
# iccDEV hybrid iccApplyProfiles local profiling helper
###############################################################################
#
# Environment variables:
#   ICCDEV_PROFILE_OUTDIR       -- output directory
#   ICCDEV_PROFILE_MODE         -- timing-only, stat, record, or both
#   ICCDEV_PROFILE_FREQ         -- perf record frequency
#   ICCDEV_PROFILE_MIN_SAMPLES  -- minimum perf samples for record mode
#   ICCDEV_PROFILE_CALL_GRAPH   -- fp, dwarf, or lbr
#   ICCDEV_PROFILE_USER_ONLY    -- set to 1 to sample user space only
#   ICCDEV_FLAMEGRAPH_DIR       -- FlameGraph checkout with stackcollapse/flamegraph
#
# The script forwards ICCDEV_TOOLS_DIR, ICCDEV_TESTING_DIR, HYBRID_TIMING_*,
# ASAN_OPTIONS, UBSAN_OPTIONS, and LLVM_PROFILE_FILE to
# iccdev-hybrid-applyprofiles-timing.sh.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

OUTDIR="${ICCDEV_PROFILE_OUTDIR:-/tmp/iccdev-hybrid-applyprofiles-profile}"
MODE="${ICCDEV_PROFILE_MODE:-stat}"
FREQ="${ICCDEV_PROFILE_FREQ:-199}"
MIN_SAMPLES="${ICCDEV_PROFILE_MIN_SAMPLES:-1000}"
CALL_GRAPH="${ICCDEV_PROFILE_CALL_GRAPH:-dwarf}"
USER_ONLY="${ICCDEV_PROFILE_USER_ONLY:-1}"
FLAMEGRAPH_DIR="${ICCDEV_FLAMEGRAPH_DIR:-}"
TIMING_OUTDIR="$OUTDIR/timing"

usage()
{
  cat <<'EOF'
Usage:
  ICCDEV_TOOLS_DIR=$PWD/build/Tools \
  ICCDEV_TESTING_DIR=$PWD/Testing \
  ICCDEV_PROFILE_MODE=record \
  ICCDEV_PROFILE_OUTDIR=/tmp/iccdev-profile \
    .github/scripts/iccdev-hybrid-applyprofiles-profile.sh

ICCDEV_PROFILE_MODE values:
  timing-only  Run the timing matrix without perf.
  stat         Run perf stat around the timing matrix.
  record       Run perf record around the timing matrix and emit perf script/SVG.
  both         Run perf stat, then perf record. This executes the timing matrix twice.

Defaults favor readable FlameGraphs:
  ICCDEV_PROFILE_FREQ=199
  ICCDEV_PROFILE_MIN_SAMPLES=1000
  ICCDEV_PROFILE_CALL_GRAPH=dwarf
  ICCDEV_PROFILE_USER_ONLY=1

Set ICCDEV_FLAMEGRAPH_DIR to a FlameGraph checkout containing
stackcollapse-perf.pl and flamegraph.pl to generate flamegraph.svg.
EOF
}

case "${1:-}" in
  -h|--help)
    usage
    exit 0
    ;;
esac

case "$MODE" in
  timing-only|stat|record|both)
    ;;
  *)
    echo "[FAIL] invalid ICCDEV_PROFILE_MODE: $MODE" >&2
    usage >&2
    exit 1
    ;;
esac

case "$FREQ" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid ICCDEV_PROFILE_FREQ: $FREQ" >&2
    exit 1
    ;;
esac

case "$MIN_SAMPLES" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid ICCDEV_PROFILE_MIN_SAMPLES: $MIN_SAMPLES" >&2
    exit 1
    ;;
esac

case "$CALL_GRAPH" in
  fp|dwarf|lbr)
    ;;
  *)
    echo "[FAIL] invalid ICCDEV_PROFILE_CALL_GRAPH: $CALL_GRAPH" >&2
    exit 1
    ;;
esac

case "$USER_ONLY" in
  0|1)
    ;;
  *)
    echo "[FAIL] invalid ICCDEV_PROFILE_USER_ONLY: $USER_ONLY" >&2
    exit 1
    ;;
esac

mkdir -p "$OUTDIR" "$TIMING_OUTDIR"

TIMING_SCRIPT="$REPO_ROOT/.github/scripts/iccdev-hybrid-applyprofiles-timing.sh"
if [ ! -x "$TIMING_SCRIPT" ]; then
  echo "[FAIL] missing timing script: $TIMING_SCRIPT" >&2
  exit 1
fi

find_perf()
{
  local candidate
  if command -v perf >/dev/null 2>&1 && perf --version >/dev/null 2>&1; then
    command -v perf
    return 0
  fi

  for candidate in /usr/lib/linux-tools/*/perf; do
    if [ -x "$candidate" ] && "$candidate" --version >/dev/null 2>&1; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

PERF_BIN=""
if [ "$MODE" != "timing-only" ]; then
  if ! PERF_BIN="$(find_perf)"; then
    echo "[FAIL] perf is required for ICCDEV_PROFILE_MODE=$MODE" >&2
    echo "[INFO] install linux-tools or set PATH to a usable perf binary" >&2
    exit 1
  fi
  echo "[INFO] using perf: $PERF_BIN"
fi

run_timing()
{
  ICCDEV_TEST_OUTDIR="$TIMING_OUTDIR" "$TIMING_SCRIPT"
}

write_repro()
{
  {
    echo "cd $(printf '%q' "$REPO_ROOT")"
    printf 'ICCDEV_PROFILE_MODE=%q ICCDEV_PROFILE_OUTDIR=%q ICCDEV_PROFILE_FREQ=%q ' \
      "$MODE" "$OUTDIR" "$FREQ"
    printf 'ICCDEV_PROFILE_MIN_SAMPLES=%q ICCDEV_PROFILE_CALL_GRAPH=%q ' \
      "$MIN_SAMPLES" "$CALL_GRAPH"
    printf 'ICCDEV_PROFILE_USER_ONLY=%q ' "$USER_ONLY"
    printf '.github/scripts/iccdev-hybrid-applyprofiles-profile.sh\n'
  } > "$OUTDIR/reproduce.sh"
  chmod 0755 "$OUTDIR/reproduce.sh"
}

find_flamegraph_tool()
{
  local name="$1"
  local path=""
  if [ -n "$FLAMEGRAPH_DIR" ]; then
    path="$FLAMEGRAPH_DIR/$name"
    if [ -x "$path" ]; then
      printf '%s\n' "$path"
      return 0
    fi
  fi
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  return 1
}

sample_count()
{
  local report="$1"
  awk '
    /^# Samples:/ {
      value=$3
      gsub(/,/, "", value)
      if (value ~ /K$/) {
        sub(/K$/, "", value)
        printf "%.0f\n", value * 1000
      } else if (value ~ /M$/) {
        sub(/M$/, "", value)
        printf "%.0f\n", value * 1000000
      } else {
        print value
      }
      found=1
    }
    END { if (!found) print 0 }
  ' "$report"
}

unknown_folded_lines()
{
  local folded="$1"
  if [ ! -f "$folded" ]; then
    printf '0\n'
    return 0
  fi
  grep -Ec '(^|;)(\[unknown\]|unknown)(;| )' "$folded" || true
}

run_perf_stat()
{
  echo "[START] perf stat hybrid timing"
  "$PERF_BIN" stat \
    -d \
    -o "$OUTDIR/perf-stat.txt" \
    -- env ICCDEV_TEST_OUTDIR="$TIMING_OUTDIR" "$TIMING_SCRIPT"
  echo "[PASS] perf stat hybrid timing"
}

run_perf_record()
{
  local event="task-clock"
  if [ "$USER_ONLY" = "1" ]; then
    event="task-clock:u"
  fi

  echo "[START] perf record hybrid timing event=$event freq=$FREQ call-graph=$CALL_GRAPH"
  "$PERF_BIN" record \
    -F "$FREQ" \
    -e "$event" \
    --call-graph "$CALL_GRAPH" \
    --user-callchains \
    -o "$OUTDIR/perf.data" \
    -- env ICCDEV_TEST_OUTDIR="$TIMING_OUTDIR" "$TIMING_SCRIPT"
  echo "[PASS] perf record hybrid timing"

  "$PERF_BIN" script --demangle -i "$OUTDIR/perf.data" > "$OUTDIR/perf.script"
  echo "[PASS] wrote $OUTDIR/perf.script"

  "$PERF_BIN" report --stdio --demangle -i "$OUTDIR/perf.data" > "$OUTDIR/perf.report.txt" 2> "$OUTDIR/perf.report.err" || true
  echo "[PASS] wrote $OUTDIR/perf.report.txt"

  local samples
  samples="$(sample_count "$OUTDIR/perf.report.txt")"
  if [ "$samples" -lt "$MIN_SAMPLES" ]; then
    echo "[FAIL] perf sample count $samples is below ICCDEV_PROFILE_MIN_SAMPLES=$MIN_SAMPLES" >&2
    echo "[INFO] increase HYBRID_TIMING_TIMEOUT, broaden HYBRID_TIMING_CASES, or lower ICCDEV_PROFILE_MIN_SAMPLES" >&2
    exit 1
  fi
  echo "[PASS] perf samples: $samples"

  local stackcollapse
  local flamegraph
  stackcollapse="$(find_flamegraph_tool stackcollapse-perf.pl || true)"
  flamegraph="$(find_flamegraph_tool flamegraph.pl || true)"

  if [ -x "$stackcollapse" ] && [ -x "$flamegraph" ]; then
    "$stackcollapse" "$OUTDIR/perf.script" > "$OUTDIR/perf.folded"
    "$flamegraph" "$OUTDIR/perf.folded" > "$OUTDIR/flamegraph.svg"
    echo "[PASS] wrote $OUTDIR/flamegraph.svg"
    echo "[INFO] folded unknown lines: $(unknown_folded_lines "$OUTDIR/perf.folded")"
  else
    echo "[SKIP] FlameGraph scripts not found; set ICCDEV_FLAMEGRAPH_DIR to enable SVG output"
  fi
}

write_repro

case "$MODE" in
  timing-only)
    echo "[START] timing-only hybrid timing"
    run_timing
    echo "[PASS] timing-only hybrid timing"
    ;;
  stat)
    run_perf_stat
    ;;
  record)
    run_perf_record
    ;;
  both)
    run_perf_stat
    run_perf_record
    ;;
esac

echo "[INFO] profile output: $OUTDIR"
find "$OUTDIR" -maxdepth 2 -type f | sort
echo "[PASS] hybrid applyprofiles profiling helper completed"
