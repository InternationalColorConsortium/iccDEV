#!/bin/bash
###############################################################################
# iccDEV hybrid iccApplyProfiles timing matrix
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR       -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR     -- path to Testing
#   ICCDEV_TEST_OUTDIR     -- output directory for logs and generated files
#   HYBRID_TIMING_THREADS  -- comma-separated variants, default baseline,2,4,8
#   HYBRID_TIMING_TIMEOUT  -- timeout in seconds per variant, default 60
#   HYBRID_TIMING_KEEP_CASES -- set to 1 to keep copied per-case workspaces
#   HYBRID_TIMING_HEARTBEAT -- progress interval in seconds, default 15
#   HYBRID_TIMING_AFFINITY  -- optional taskset CPU list, e.g. 0-15
#   HYBRID_TIMING_CASES     -- comma-separated cmykw,kw-mcs, default both
#   HYBRID_TIMING_BAND_ROWS -- optional ICC_APPLY_PROFILES_BAND_ROWS value
#   HYBRID_TIMING_INSTRUMENT -- set to 1 for apply/thread timing lines
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

OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-hybrid-applyprofiles-timing}"
THREADS_CSV="${HYBRID_TIMING_THREADS:-baseline,2,4,8}"
TIMEOUT_SEC="${HYBRID_TIMING_TIMEOUT:-60}"
KEEP_CASES="${HYBRID_TIMING_KEEP_CASES:-0}"
HEARTBEAT_SEC="${HYBRID_TIMING_HEARTBEAT:-15}"
AFFINITY_CPUS="${HYBRID_TIMING_AFFINITY:-}"
CASES_CSV="${HYBRID_TIMING_CASES:-cmykw,kw-mcs}"
BAND_ROWS="${HYBRID_TIMING_BAND_ROWS:-}"
INSTRUMENT="${HYBRID_TIMING_INSTRUMENT:-0}"

case "$TIMEOUT_SEC" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid HYBRID_TIMING_TIMEOUT: $TIMEOUT_SEC" >&2
    exit 1
    ;;
esac
if [ "$TIMEOUT_SEC" -gt 60 ]; then
  echo "[FAIL] HYBRID_TIMING_TIMEOUT must be <= 60 seconds for local timing windows" >&2
  exit 1
fi

case "$HEARTBEAT_SEC" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid HYBRID_TIMING_HEARTBEAT: $HEARTBEAT_SEC" >&2
    exit 1
    ;;
esac

case "$KEEP_CASES" in
  0|1)
    ;;
  *)
    echo "[FAIL] invalid HYBRID_TIMING_KEEP_CASES: $KEEP_CASES" >&2
    exit 1
    ;;
esac

case "$INSTRUMENT" in
  0|1)
    ;;
  *)
    echo "[FAIL] invalid HYBRID_TIMING_INSTRUMENT: $INSTRUMENT" >&2
    exit 1
    ;;
esac

case "$BAND_ROWS" in
  ""|*[!0-9]*)
    if [ -n "$BAND_ROWS" ]; then
      echo "[FAIL] invalid HYBRID_TIMING_BAND_ROWS: $BAND_ROWS" >&2
      exit 1
    fi
    ;;
esac

BUILD_ROOT="$(cd "$TOOLS/.." 2>/dev/null && pwd -P)"
export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${PATH:+:$PATH}"

tool_path()
{
  local tool_dir="$1"
  local tool_exe="$2"
  local path="$TOOLS/$tool_dir/$tool_exe"
  if [ ! -x "$path" ] && [ -x "$path.exe" ]; then
    path="$path.exe"
  fi
  if [ ! -x "$path" ]; then
    echo "[FAIL] missing tool: $path" >&2
    exit 1
  fi
  printf '%s\n' "$path"
}

ICC_FROM_XML="$(tool_path IccFromXml iccFromXml)"
ICC_APPLY_PROFILES="$(tool_path IccApplyProfiles iccApplyProfiles)"

RUNNER=()
if [ -z "$AFFINITY_CPUS" ] && command -v taskset >/dev/null 2>&1 && command -v nproc >/dev/null 2>&1; then
  AFFINITY_CPUS="0-$(($(nproc) - 1))"
fi

if [ -n "$AFFINITY_CPUS" ]; then
  if command -v taskset >/dev/null 2>&1; then
    RUNNER=(taskset -c "$AFFINITY_CPUS")
    echo "[INFO] using CPU affinity: $AFFINITY_CPUS"
  else
    echo "[WARN] HYBRID_TIMING_AFFINITY ignored because taskset is unavailable"
  fi
fi

if [ ! -d "$ICCDEV_TESTING/hybrid" ]; then
  echo "[FAIL] missing hybrid test directory: $ICCDEV_TESTING/hybrid" >&2
  exit 1
fi
if [ ! -f "$ICCDEV_TESTING/sRGB_v4_ICC_preference.icc" ]; then
  echo "[FAIL] missing sRGB profile: $ICCDEV_TESTING/sRGB_v4_ICC_preference.icc" >&2
  exit 1
fi

mkdir -p "$OUTDIR"
SUMMARY="$OUTDIR/hybrid-applyprofiles-timing.tsv"
printf 'case\tthreads\tcommand\tstatus\telapsed_s\tuser_s\tsystem_s\tmax_rss_kb\tminor_faults\tvoluntary_ctx\tinvoluntary_ctx\tlog\n' > "$SUMMARY"

IFS=, read -r -a TIMING_VARIANTS <<< "$THREADS_CSV"
IFS=, read -r -a TIMING_CASES <<< "$CASES_CSV"

if [ "${#TIMING_VARIANTS[@]}" -eq 0 ]; then
  echo "[FAIL] no timing variants requested" >&2
  exit 1
fi

if [ "${#TIMING_CASES[@]}" -eq 0 ]; then
  echo "[FAIL] no timing cases requested" >&2
  exit 1
fi

prepare_case_dir()
{
  local case_dir="$1"
  rm -rf "$case_dir"
  mkdir -p "$case_dir/testing"
  cp -a "$ICCDEV_TESTING/hybrid" "$case_dir/testing/hybrid"
  cp "$ICCDEV_TESTING/sRGB_v4_ICC_preference.icc" "$case_dir/testing/"
  mkdir -p "$case_dir/testing/hybrid/ICC" "$case_dir/testing/hybrid/Results" "$case_dir/testing/hybrid/config"
  (
    cd "$case_dir/testing/hybrid"
    "$ICC_FROM_XML" CMYK-S_Overprint_Profile.xml ICC/CMYK-S_Overprint_Profile.icc >/dev/null
    "$ICC_FROM_XML" MS-Mid_Overprint.xml ICC/MS-Mid_Overprint.icc >/dev/null
  )
}

time_value()
{
  local pattern="$1"
  local file="$2"
  awk -v pat="$pattern" 'index($0, pat) == 1 { print substr($0, length(pat) + 1) }' "$file" | tail -n 1
}

run_case()
{
  local label="$1"
  local threads="$2"
  local command_name="$3"
  shift 3

  local case_dir="$OUTDIR/$label-$command_name"
  local log="$OUTDIR/$label-$command_name.log"
  local time_log="$OUTDIR/$label-$command_name.time"
  local status=0
  local -a thread_args=()

  prepare_case_dir "$case_dir"

  if [ "$threads" != "baseline" ]; then
    thread_args=(-threads "$threads")
  fi

  echo "[START] $(date -u '+%Y-%m-%dT%H:%M:%SZ') $label $command_name timeout=${TIMEOUT_SEC}s threads=$threads"

  (
    cd "$case_dir/testing/hybrid"
    if [ "$INSTRUMENT" -eq 1 ]; then
      export ICC_APPLY_PROFILES_TIMING=1
      export ICC_CMM_THREAD_TIMING=1
    fi
    if [ -n "$BAND_ROWS" ]; then
      export ICC_APPLY_PROFILES_BAND_ROWS="$BAND_ROWS"
    fi
    /usr/bin/time \
      -f 'elapsed_s=%e\nuser_s=%U\nsystem_s=%S\nmax_rss_kb=%M\nminor_faults=%R\nvoluntary_ctx=%w\ninvoluntary_ctx=%c' \
      -o "$time_log" \
      timeout "$TIMEOUT_SEC" "${RUNNER[@]}" "$ICC_APPLY_PROFILES" "${thread_args[@]}" "$@"
  ) > "$log" 2>&1 &
  local run_pid=$!
  local start_epoch
  start_epoch="$(date +%s)"
  while kill -0 "$run_pid" 2>/dev/null; do
    sleep "$HEARTBEAT_SEC"
    if kill -0 "$run_pid" 2>/dev/null; then
      local now_epoch
      now_epoch="$(date +%s)"
      echo "[HEARTBEAT] $(date -u '+%Y-%m-%dT%H:%M:%SZ') $label $command_name elapsed=$((now_epoch - start_epoch))s"
    fi
  done
  wait "$run_pid" || status=$?

  local elapsed
  local user_time
  local system_time
  local max_rss
  local minor_faults
  local voluntary_ctx
  local involuntary_ctx

  elapsed="$(time_value elapsed_s= "$time_log")"
  user_time="$(time_value user_s= "$time_log")"
  system_time="$(time_value system_s= "$time_log")"
  max_rss="$(time_value max_rss_kb= "$time_log")"
  minor_faults="$(time_value minor_faults= "$time_log")"
  voluntary_ctx="$(time_value voluntary_ctx= "$time_log")"
  involuntary_ctx="$(time_value involuntary_ctx= "$time_log")"

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$label" "$threads" "$command_name" "$status" "${elapsed:-n/a}" \
    "${user_time:-n/a}" "${system_time:-n/a}" "$max_rss" "$minor_faults" \
    "$voluntary_ctx" "$involuntary_ctx" "$log" >> "$SUMMARY"

  if [ "$status" -ne 0 ]; then
    if [ "$status" -eq 124 ]; then
      echo "[TIMEOUT] $(date -u '+%Y-%m-%dT%H:%M:%SZ') $label $command_name exceeded ${TIMEOUT_SEC}s"
    else
      echo "[FAIL] $(date -u '+%Y-%m-%dT%H:%M:%SZ') $label $command_name exit=$status"
    fi
    echo "[INFO] retained failing case workspace: $case_dir"
    sed -n '1,120p' "$log"
    return 1
  fi

  if [ "$KEEP_CASES" -eq 0 ]; then
    rm -rf "$case_dir"
  else
    echo "[INFO] retained case workspace: $case_dir"
  fi

  echo "[PASS] $(date -u '+%Y-%m-%dT%H:%M:%SZ') $label $command_name elapsed=${elapsed:-n/a}s"
}

failures=0
for requested in "${TIMING_VARIANTS[@]}"; do
  case "$requested" in
    baseline|default|none)
      label="baseline"
      threads="baseline"
      ;;
    ""|*[!0-9]*)
      echo "[FAIL] invalid thread variant: $requested" >&2
      failures=$((failures + 1))
      continue
      ;;
    *)
      label="threads-$requested"
      threads="$requested"
      ;;
  esac

  for timing_case in "${TIMING_CASES[@]}"; do
    case "$timing_case" in
      cmykw)
        run_case "$label" "$threads" cmykw \
          -exportcfg "config/timing-${label}-cmykw.json" \
          Data/TShirtDesignCMYKW.tif "Results/timing-${label}-cmykw.tif" \
          1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 \
          ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1 || failures=$((failures + 1))
        ;;
      kw-mcs)
        run_case "$label" "$threads" kw-mcs \
          -exportcfg "config/timing-${label}-kw-mcs.json" \
          Data/TShirtDesignKW.tif "Results/timing-${label}-kw-mcs.tif" \
          1 1 0 0 0 ICC/MS-Mid_Overprint.icc 80 \
          -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 '-ENV:0ni?' 1 \
          ICC/CMYK-S_Overprint_Profile.icc 10080 ../sRGB_v4_ICC_preference.icc 1 || failures=$((failures + 1))
        ;;
      *)
        echo "[FAIL] invalid HYBRID_TIMING_CASES entry: $timing_case" >&2
        failures=$((failures + 1))
        ;;
    esac
  done
done

echo "[INFO] timing summary: $SUMMARY"
sed -n '1,80p' "$SUMMARY"

if [ "$failures" -ne 0 ]; then
  echo "[FAIL] hybrid applyprofiles timing matrix failures: $failures"
  exit 1
fi

echo "[PASS] hybrid applyprofiles timing matrix completed"
