#!/usr/bin/env bash
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Markdown backticks in report printf format strings are intentionally literal.
# shellcheck disable=SC2016
###############################################################################
# Compare portable, AVX2, and AVX-512 CLUT regression builds locally.
###############################################################################

set -euo pipefail

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo "usage: $0 REPORT_DIR PORTABLE_BUILD AVX2_BUILD [AVX512_BUILD]" >&2
    exit 2
fi

report_dir="$1"
portable_build="$2"
avx2_build="$3"
avx512_build="${4:-}"
runs="${ICCDEV_CLUT_ISA_RUNS:-5}"
affinity="${ICCDEV_CLUT_ISA_AFFINITY:-}"
test_name="iccdev.clut-eight-output-regression"
samples_file="$report_dir/samples.tsv"
report_file="$report_dir/report.md"

case "$runs" in
    ''|*[!0-9]*|0)
        echo "ICCDEV_CLUT_ISA_RUNS must be a positive integer" >&2
        exit 2
        ;;
esac

if [ "$runs" -gt 20 ]; then
    echo "ICCDEV_CLUT_ISA_RUNS must not exceed 20" >&2
    exit 2
fi

if ! command -v ctest >/dev/null 2>&1; then
    echo "ctest is required" >&2
    exit 1
fi

if ! command -v /usr/bin/time >/dev/null 2>&1; then
    echo "/usr/bin/time is required" >&2
    exit 1
fi

runner=()
if [ -n "$affinity" ]; then
    if ! command -v taskset >/dev/null 2>&1; then
        echo "ICCDEV_CLUT_ISA_AFFINITY requires taskset" >&2
        exit 1
    fi
    runner=(taskset -c "$affinity")
fi

mkdir -p "$report_dir"
if [ -e "$samples_file" ] || [ -e "$report_file" ]; then
    echo "report directory already contains report output: $report_dir" >&2
    exit 1
fi

cache_value()
{
    local build_dir="$1"
    local name="$2"

    awk -F= -v name="$name" 'index($1, name ":") == 1 { print $2; exit }' \
        "$build_dir/CMakeCache.txt"
}

require_variant()
{
    local label="$1"
    local build_dir="$2"
    local expected_avx2="$3"
    local expected_avx512="$4"
    local avx2
    local avx512
    local avx2_effective
    local avx512_effective
    local listing

    if [ ! -f "$build_dir/CMakeCache.txt" ]; then
        echo "$label build lacks CMakeCache.txt: $build_dir" >&2
        exit 1
    fi

    avx2="$(cache_value "$build_dir" ICCDEV_ENABLE_AVX2)"
    avx512="$(cache_value "$build_dir" ICCDEV_ENABLE_AVX512)"
    avx2_effective="$(cache_value "$build_dir" ICCDEV_AVX2_EFFECTIVE)"
    avx512_effective="$(cache_value "$build_dir" ICCDEV_AVX512_EFFECTIVE)"
    if [ "$avx2" != "$expected_avx2" ] || [ "$avx512" != "$expected_avx512" ]; then
        echo "$label has ICCDEV_ENABLE_AVX2=$avx2 and ICCDEV_ENABLE_AVX512=$avx512; expected $expected_avx2/$expected_avx512" >&2
        exit 1
    fi
    if [ "$avx2_effective" != "$expected_avx2" ] ||
       [ "$avx512_effective" != "$expected_avx512" ]; then
        echo "$label requested AVX2/AVX-512=$avx2/$avx512 but compiled $avx2_effective/$avx512_effective" >&2
        exit 1
    fi

    listing="$(ctest --test-dir "$build_dir" -N -R "^${test_name}$" --no-tests=error)"
    if ! grep -q "$test_name" <<< "$listing"; then
        echo "$label does not register $test_name" >&2
        exit 1
    fi
}

median_seconds()
{
    local label="$1"

    awk -F '\t' -v label="$label" '$1 == label && $3 == 0 { print $2 }' \
        "$samples_file" | sort -n | awk '
            { values[NR] = $1 }
            END {
                if (NR == 0) {
                    print "n/a"
                } else if (NR % 2) {
                    printf "%.6f", values[(NR + 1) / 2]
                } else {
                    printf "%.6f", (values[NR / 2] + values[NR / 2 + 1]) / 2
                }
            }'
}

cache_summary()
{
    local build_dir="$1"

    printf 'compiler=%s; build_type=%s; avx2=%s; avx512=%s; avx2_debug=%s' \
        "$(cache_value "$build_dir" CMAKE_CXX_COMPILER)" \
        "$(cache_value "$build_dir" CMAKE_BUILD_TYPE)" \
        "$(cache_value "$build_dir" ICCDEV_AVX2_EFFECTIVE)" \
        "$(cache_value "$build_dir" ICCDEV_AVX512_EFFECTIVE)" \
        "$(cache_value "$build_dir" ICC_AVX2_CLUT_DEBUG)"
}

run_variant()
{
    local label="$1"
    local build_dir="$2"
    local run
    local log
    local time_log
    local status
    local elapsed

    for run in $(seq 1 "$runs"); do
        log="$report_dir/${label}-${run}.log"
        time_log="$report_dir/${label}-${run}.time"
        status=0
        /usr/bin/time \
            -f 'elapsed_s=%e\nuser_s=%U\nsystem_s=%S\nmax_rss_kb=%M' \
            -o "$time_log" \
            "${runner[@]}" \
            ctest --test-dir "$build_dir" -R "^${test_name}$" \
            --output-on-failure --no-tests=error > "$log" 2>&1 || status=$?
        elapsed="$(awk -F= '$1 == "elapsed_s" { print $2; exit }' "$time_log")"
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "$label" "${elapsed:-n/a}" "$status" "$run" "$log" >> "$samples_file"
    done
}

require_variant portable "$portable_build" OFF OFF
require_variant avx2 "$avx2_build" ON OFF
if [ -n "$avx512_build" ]; then
    require_variant avx512 "$avx512_build" ON ON
fi

printf 'variant\telapsed_s\tstatus\trun\tlog\n' > "$samples_file"
run_variant portable "$portable_build"
run_variant avx2 "$avx2_build"
if [ -n "$avx512_build" ]; then
    run_variant avx512 "$avx512_build"
fi

{
    printf '# Local CLUT ISA Coverage and Timing Report\n\n'
    printf '## Scope\n\n'
    printf 'This report executes `%s` against independently configured builds.\n' \
        "$test_name"
    printf 'The regression covers 8-, 9-, 11-, and 14-output AVX2 fallbacks plus the 15-output AVX2 full vector and masked tail; AVX-512 may select its own eligible paths.\n\n'
    printf '## Environment\n\n'
    printf '| Field | Value |\n|---|---|\n'
    printf '| Kernel | `%s` |\n' "$(uname -srmo)"
    printf '| CPU affinity | `%s` |\n' "${affinity:-not pinned}"
    printf '| Repetitions | %s |\n' "$runs"
    if command -v lscpu >/dev/null 2>&1; then
        printf '| CPU | `%s` |\n' "$(lscpu | awk -F: '/Model name/ { sub(/^[[:space:]]+/, "", $2); print $2; exit }')"
    fi
    printf '\n## Build and Functional Coverage\n\n'
    printf '| Variant | Build configuration | Focused regression | Median end-to-end time (s) |\n'
    printf '|---|---|---|---:|\n'
    printf '| Portable | `%s` | 8-, 9-, 11-, 14-, and 15-output scalar/SSE fallback | %s |\n' \
        "$(cache_summary "$portable_build")" "$(median_seconds portable)"
    printf '| AVX2 | `%s` | 8-, 9-, 11-, and 14-output fallback; 15-output vector plus masked tail | %s |\n' \
        "$(cache_summary "$avx2_build")" "$(median_seconds avx2)"
    if [ -n "$avx512_build" ]; then
        printf '| AVX-512 | `%s` | 8-, 9-, 11-, 14-, and 15-output eligible paths | %s |\n' \
            "$(cache_summary "$avx512_build")" "$(median_seconds avx512)"
    fi
    printf '\n## Timing Samples\n\n'
    printf '```text\n'
    cat "$samples_file"
    printf '```\n\n'
    printf '## Interpretation\n\n'
    printf 'These are end-to-end CTest timings: XML conversion, profile validation, CMM setup, one CLUT application, and process startup are included. They are repeatable regression-envelope measurements, not isolated CLUT-kernel throughput claims. Keep `ICC_AVX2_CLUT_DEBUG=OFF` while collecting them. Use the diagnostic build only to establish dispatch evidence, and collect a separate kernel-level benchmark before claiming an ISA speedup.\n'
} > "$report_file"

if awk -F '\t' 'NR > 1 && $3 != 0 { exit 1 }' "$samples_file"; then
    echo "[PASS] report: $report_file"
else
    echo "[FAIL] one or more focused regressions failed; report retained at $report_file" >&2
    exit 1
fi
