#!/usr/bin/env bash
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
###############################################################################
# Collect repeatable CLUT timing, hardware-counter, syscall, and stack data.
###############################################################################

set -euo pipefail
export LC_ALL=C

if [ "$#" -ne 2 ]; then
    echo "usage: $0 BUILD_DIR OUTPUT_DIR" >&2
    exit 2
fi

build_dir="$1"
output_dir="$2"
runs="${ICCDEV_CLUT_PROFILE_RUNS:-21}"
affinity="${ICCDEV_CLUT_PROFILE_AFFINITY:-}"
enable_flamegraph="${ICCDEV_CLUT_PROFILE_FLAMEGRAPH:-0}"

case "$runs" in
    ''|*[!0-9]*|0)
        echo "ICCDEV_CLUT_PROFILE_RUNS must be a positive integer" >&2
        exit 2
        ;;
esac
if [ "$runs" -gt 100 ]; then
    echo "ICCDEV_CLUT_PROFILE_RUNS must not exceed 100" >&2
    exit 2
fi
if [ ! -f "$build_dir/CTestTestfile.cmake" ]; then
    echo "BUILD_DIR does not contain a configured CTest tree: $build_dir" >&2
    exit 2
fi
build_type="$(awk -F= '$1 == "CMAKE_BUILD_TYPE:STRING" { print $2; exit }' \
    "$build_dir/CMakeCache.txt")"
avx2_debug="$(awk -F= '$1 == "ICC_AVX2_CLUT_DEBUG:BOOL" { print $2; exit }' \
    "$build_dir/CMakeCache.txt")"
if [ "$build_type" != "Release" ] || [ "$avx2_debug" != "OFF" ]; then
    echo "BUILD_DIR must be Release with ICC_AVX2_CLUT_DEBUG=OFF: $build_dir" >&2
    exit 2
fi
perf_monitoring="$(awk -F= '
    $1 == "ICCDEV_ENABLE_PERF_MONITORING:BOOL" { print $2; exit }
' "$build_dir/CMakeCache.txt")"
if [ "$perf_monitoring" != "ON" ]; then
    echo "BUILD_DIR must enable ICCDEV_ENABLE_PERF_MONITORING: $build_dir" >&2
    exit 2
fi
if [ -e "$output_dir" ]; then
    echo "OUTPUT_DIR already exists: $output_dir" >&2
    exit 2
fi

mkdir -p "$output_dir"
samples_file="$output_dir/samples.tsv"
derived_perf_file="$output_dir/perf-derived.tsv"
environment_file="$output_dir/environment.txt"

runner=()
if [ -n "$affinity" ]; then
    if ! command -v taskset >/dev/null 2>&1; then
        echo "ICCDEV_CLUT_PROFILE_AFFINITY requires taskset" >&2
        exit 2
    fi
    runner=(taskset -c "$affinity")
fi

ctest_command=(
    ctest
    --test-dir "$build_dir"
    -R '^iccdev\.clut-eight-output-regression$'
    --output-on-failure
    --no-tests=error
)

{
    uname -a
    printf 'cpu_affinity=%s\n' "${affinity:-not-pinned}"
    printf 'runs=%s\n' "$runs"
    printf 'compiler='
    awk -F= '/^CMAKE_CXX_COMPILER:FILEPATH=/ { print $2; exit }' \
        "$build_dir/CMakeCache.txt"
    printf 'build_type='
    awk -F= '/^CMAKE_BUILD_TYPE:STRING=/ { print $2; exit }' \
        "$build_dir/CMakeCache.txt"
    if command -v lscpu >/dev/null 2>&1; then
        lscpu
    fi
} > "$environment_file"

perf_available=0
if command -v perf >/dev/null 2>&1 && perf stat -e cycles true >/dev/null 2>&1; then
    perf_available=1
fi

printf 'run\telapsed_s\tuser_s\tsystem_s\tmax_rss_kb\tstatus\tlog\tperf_stat\n' \
    > "$samples_file"
printf 'run\tcycles\tinstructions\tbranches\tbranch_misses\tcache_references\tcache_misses\tinstructions_per_cycle\tinstructions_per_second\tbranch_miss_rate\tcache_miss_rate\n' \
    > "$derived_perf_file"
for run in $(seq 1 "$runs"); do
    log_file="$output_dir/run-${run}.log"
    time_file="$output_dir/run-${run}.time"
    perf_file="$output_dir/run-${run}.perf.csv"
    telemetry_file="$output_dir/run-${run}.telemetry"
    status=0

    if [ "$perf_available" -eq 1 ]; then
        perf stat -x, -o "$perf_file" \
            -e cycles,instructions,branches,branch-misses,cache-references,cache-misses,\
context-switches,cpu-migrations \
            env "ICC_PERF_STATS_FILE=$telemetry_file" \
            /usr/bin/time -f 'elapsed_s=%e\nuser_s=%U\nsystem_s=%S\nmax_rss_kb=%M' \
            -o "$time_file" "${runner[@]}" "${ctest_command[@]}" > "$log_file" 2>&1 || status=$?
    else
        printf 'unavailable\n' > "$perf_file"
        env "ICC_PERF_STATS_FILE=$telemetry_file" \
            /usr/bin/time -f 'elapsed_s=%e\nuser_s=%U\nsystem_s=%S\nmax_rss_kb=%M' \
            -o "$time_file" "${runner[@]}" "${ctest_command[@]}" > "$log_file" 2>&1 || status=$?
    fi

    elapsed="$(awk -F= '$1 == "elapsed_s" { print $2; exit }' "$time_file")"
    user="$(awk -F= '$1 == "user_s" { print $2; exit }' "$time_file")"
    system="$(awk -F= '$1 == "system_s" { print $2; exit }' "$time_file")"
    rss="$(awk -F= '$1 == "max_rss_kb" { print $2; exit }' "$time_file")"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$run" "${elapsed:-n/a}" "${user:-n/a}" "${system:-n/a}" "${rss:-n/a}" \
        "$status" "$log_file" "$perf_file" >> "$samples_file"
    if [ "$status" -ne 0 ]; then
        echo "[FAIL] focused CLUT regression failed in run $run; log retained at $log_file" >&2
        exit "$status"
    fi
    if [ ! -s "$telemetry_file" ]; then
        echo "[FAIL] telemetry was not produced in run $run: $telemetry_file" >&2
        exit 1
    fi

    awk -F, -v run="$run" -v elapsed="${elapsed:-}" '
        function clean(value) {
            gsub(/[[:space:]]/, "", value)
            gsub(/,/, "", value)
            return value
        }
        function number(value) {
            return value ~ /^[0-9]+([.][0-9]+)?$/
        }
        function ratio(numerator, denominator) {
            if (number(numerator) && number(denominator) && denominator > 0)
                return numerator / denominator
            return "n/a"
        }
        {
            event = clean($3)
            sub(/:.*/, "", event)
        }
        event == "cycles" { cycles = clean($1) }
        event == "instructions" { instructions = clean($1) }
        event == "branches" { branches = clean($1) }
        event == "branch-misses" { branch_misses = clean($1) }
        event == "cache-references" { cache_references = clean($1) }
        event == "cache-misses" { cache_misses = clean($1) }
        END {
            values[1] = cycles
            values[2] = instructions
            values[3] = branches
            values[4] = branch_misses
            values[5] = cache_references
            values[6] = cache_misses
            for (i = 1; i <= 6; i++)
                if (!number(values[i]))
                    values[i] = "n/a"
            printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                run, values[1], values[2], values[3], values[4], values[5],
                values[6], ratio(values[2], values[1]),
                ratio(values[2], elapsed), ratio(values[4], values[3]),
                ratio(values[6], values[5])
        }' "$perf_file" >> "$derived_perf_file"
done

if command -v strace >/dev/null 2>&1; then
    if ! strace -f -c -o "$output_dir/strace-summary.txt" \
        "${runner[@]}" "${ctest_command[@]}" > "$output_dir/strace.log" 2>&1; then
        printf '[WARN] strace installed but unusable; see strace.log\n' \
            >> "$output_dir/strace-summary.txt"
    fi
else
    printf 'strace unavailable\n' > "$output_dir/strace-summary.txt"
fi

if [ "$enable_flamegraph" = "1" ] && [ "$perf_available" -eq 1 ]; then
    perf record -o "$output_dir/perf.data" -g --call-graph dwarf -- \
        "${runner[@]}" "${ctest_command[@]}" > "$output_dir/perf-record.log" 2>&1
    perf script -i "$output_dir/perf.data" > "$output_dir/perf.script"
    if [ -n "${FLAMEGRAPH_DIR:-}" ] &&
       [ -x "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ] &&
       [ -x "$FLAMEGRAPH_DIR/flamegraph.pl" ]; then
        "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" "$output_dir/perf.script" |
            "$FLAMEGRAPH_DIR/flamegraph.pl" > "$output_dir/flamegraph.svg"
    fi
fi

awk -F '\t' '
    NR > 1 { values[++count] = $2 }
    END {
        if (!count)
            exit 1
        for (i = 1; i <= count; i++)
            for (j = i + 1; j <= count; j++)
                if (values[j] < values[i]) {
                    value = values[i]; values[i] = values[j]; values[j] = value
                }
        if (count % 2)
            median = values[int((count + 1) / 2)]
        else
            median = (values[count / 2] + values[count / 2 + 1]) / 2
        p95 = values[int((count * 95 + 99) / 100)]
        printf "samples=%d\nmedian_elapsed_s=%s\np95_elapsed_s=%s\n", count, median, p95
    }' "$samples_file" > "$output_dir/summary.txt"

printf 'perf_stat=%s\n' "$([ "$perf_available" -eq 1 ] && echo available || echo unavailable)" \
    >> "$output_dir/summary.txt"
printf 'perf_derived=%s\n' "$derived_perf_file" >> "$output_dir/summary.txt"
printf '[PASS] profile report: %s\n' "$output_dir"
