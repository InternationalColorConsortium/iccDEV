#!/usr/bin/env bash
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
###############################################################################
# Profile the MCS overprint iccApplyProfiles path with bounded diagnostics.
###############################################################################

set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 DEBUG_BUILD RELEASE_BUILD OUTPUT_DIR" >&2
    exit 2
fi

debug_build="$(cd "$1" && pwd)"
release_build="$(cd "$2" && pwd)"
output_dir="$3"
target_seconds="${ICCDEV_MCS_PROFILE_SECONDS:-300}"
maximum_seconds="${ICCDEV_MCS_PROFILE_MAX_SECONDS:-600}"
run_timeout="${ICCDEV_MCS_PROFILE_RUN_TIMEOUT:-120}"

for time_limit in "$target_seconds" "$maximum_seconds" "$run_timeout"; do
    case "$time_limit" in
        ''|*[!0-9]*|0|0[0-9]*|????*)
            echo "profiling time limits must be positive integers" >&2
            exit 2
            ;;
    esac
done
if [ "$target_seconds" -gt "$maximum_seconds" ]; then
    echo "ICCDEV_MCS_PROFILE_SECONDS must not exceed the maximum" >&2
    exit 2
fi
if [ "$maximum_seconds" -gt 600 ]; then
    echo "ICCDEV_MCS_PROFILE_MAX_SECONDS must not exceed 600" >&2
    exit 2
fi
if [ "$run_timeout" -gt 600 ]; then
    echo "ICCDEV_MCS_PROFILE_RUN_TIMEOUT must not exceed 600" >&2
    exit 2
fi
if [ -e "$output_dir" ]; then
    echo "OUTPUT_DIR already exists: $output_dir" >&2
    exit 2
fi
if ! command -v tiffcrop >/dev/null 2>&1; then
    echo "tiffcrop is required to construct the diagnostic images" >&2
    exit 2
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
debug_apply="$debug_build/Tools/IccApplyProfiles/iccApplyProfiles"
debug_from_xml="$debug_build/Tools/IccFromXml/iccFromXml"
release_apply="$release_build/Tools/IccApplyProfiles/iccApplyProfiles"
release_bench="$release_build/Tools/IccBenchApply/iccBenchApply"
source_tiff="$repo_root/Testing/mcs/CMYKSS-Numbered-Overprint.tif"
source_srgb="$repo_root/Testing/sRGB_v4_ICC_preference.icc"

for path in "$debug_apply" "$debug_from_xml" "$release_apply" "$release_bench" \
            "$source_tiff" "$source_srgb"; do
    if [ ! -e "$path" ]; then
        echo "required path is missing: $path" >&2
        exit 2
    fi
done

cache_value()
{
    local build_dir="$1"
    local key="$2"
    awk -F= -v key="$key" '$1 == key { print $2; exit }' \
        "$build_dir/CMakeCache.txt"
}

debug_sanitizers="$(cache_value "$debug_build" ENABLE_SANITIZERS:BOOL)"
debug_asan="$(cache_value "$debug_build" ENABLE_ASAN:BOOL)"
debug_ubsan="$(cache_value "$debug_build" ENABLE_UBSAN:BOOL)"
if [ "$(cache_value "$debug_build" CMAKE_BUILD_TYPE:STRING)" != "Debug" ] ||
   { [ "$debug_sanitizers" != "ON" ] &&
     { [ "$debug_asan" != "ON" ] || [ "$debug_ubsan" != "ON" ]; }; } ||
   [ "$(cache_value "$debug_build" ICCDEV_ENABLE_PERF_MONITORING:BOOL)" != "ON" ] ||
   [ "$(cache_value "$debug_build" ICC_VERBOSE_CALC_APPLY:BOOL)" != "OFF" ]; then
    echo "DEBUG_BUILD must be instrumented Debug ASAN+UBSAN with calculator logging OFF" >&2
    exit 2
fi
if [ "$(cache_value "$release_build" CMAKE_BUILD_TYPE:STRING)" != "Release" ] ||
   [ "$(cache_value "$release_build" ICCDEV_ENABLE_PERF_MONITORING:BOOL)" != "ON" ] ||
   [ "$(cache_value "$release_build" ICC_VERBOSE_CALC_APPLY:BOOL)" != "OFF" ]; then
    echo "RELEASE_BUILD must be instrumented Release with calculator logging OFF" >&2
    exit 2
fi

start_epoch="$(date +%s)"
mkdir -p "$output_dir/images" "$output_dir/profiles" "$output_dir/runs"
summary="$output_dir/summary.tsv"
printf 'case\tbuild\tpixels\tstatus\telapsed_s\tuser_s\tsystem_s\tmax_rss_kb\tapply_ms\tapply_pct\tsha256\tlog\n' \
    > "$summary"

output_dir="$(cd "$output_dir" && pwd)"

remaining_timeout()
{
    local label="$1"
    local now
    local remaining
    local timeout_seconds

    now="$(date +%s)"
    remaining=$((maximum_seconds - (now - start_epoch)))
    if [ "$remaining" -le 0 ]; then
        echo "[TIMEOUT] profiling window reached before $label" >&2
        return 124
    fi
    timeout_seconds="$run_timeout"
    if [ "$remaining" -lt "$timeout_seconds" ]; then
        timeout_seconds="$remaining"
    fi
    printf '%s\n' "$timeout_seconds"
}

run_with_remaining_budget()
{
    local label="$1"
    local timeout_seconds
    shift

    timeout_seconds="$(remaining_timeout "$label")" || return $?
    timeout --signal=KILL "$timeout_seconds" "$@"
}

(
    cd "$repo_root/Testing/mcs"
    run_with_remaining_budget profile-6ChanSelect-MID \
        "$debug_from_xml" 6ChanSelect-MID.xml \
        "$output_dir/profiles/6ChanSelect-MID.icc" >/dev/null
    run_with_remaining_budget profile-18ChanWithSpots-MVIS \
        "$debug_from_xml" 18ChanWithSpots-MVIS.xml \
        "$output_dir/profiles/18ChanWithSpots-MVIS.icc" >/dev/null
)

run_benchmark()
{
    local label="$1"
    shift
    local log="$output_dir/runs/$label.log"
    local time_file="$output_dir/runs/$label.time"
    local status=0
    local timeout_seconds

    timeout_seconds="$(remaining_timeout "$label")" || return $?

    /usr/bin/time -f 'elapsed_s=%e' -o "$time_file" \
        timeout --signal=KILL "$timeout_seconds" \
        "$release_bench" -csv -pixels 1048576 -repeats 5 1 \
        "$output_dir/profiles/6ChanSelect-MID.icc" 0 \
        "$@" \
        "$output_dir/profiles/18ChanWithSpots-MVIS.icc" 0 \
        "$source_srgb" 1 > "$log" 2>&1 || status=$?

    if [ "$status" -ne 0 ] ||
       ! awk -F, '$1 == "chain" && $7 == "ok" { found = 1 }
                    END { exit !found }' "$log"; then
        echo "[FAIL] $label benchmark failed; see $log" >&2
        return 1
    fi
}

run_benchmark release-bench-no-env
run_benchmark release-bench-env \
    -ENV:bkgX 0.4014 -ENV:bkgY 0.2391 -ENV:bkgZ 0.0272

bench_no_env_line="$(awk -F, '$1 == "chain" { print; exit }' \
    "$output_dir/runs/release-bench-no-env.log")"
bench_env_line="$(awk -F, '$1 == "chain" { print; exit }' \
    "$output_dir/runs/release-bench-env.log")"
bench_no_env_rate="$(printf '%s\n' "$bench_no_env_line" | cut -d, -f3)"
bench_env_rate="$(printf '%s\n' "$bench_env_line" | cut -d, -f3)"
bench_no_env_checksum="$(printf '%s\n' "$bench_no_env_line" | cut -d, -f6)"
bench_env_checksum="$(printf '%s\n' "$bench_env_line" | cut -d, -f6)"
if [ "$bench_no_env_checksum" = "$bench_env_checksum" ]; then
    echo "[FAIL] MCS benchmark environment values did not affect output" >&2
    exit 1
fi

run_with_remaining_budget setup-1x1 \
    tiffcrop -U px -z 0,0,0,0 -c lzw -p contig "$source_tiff" \
    "$output_dir/images/setup-1x1.tif"
run_with_remaining_budget setup-smoke-64x64 \
    tiffcrop -U px -z 480,480,543,543 -c lzw -p contig "$source_tiff" \
    "$output_dir/images/smoke-center-64x64.tif"
run_with_remaining_budget setup-representative-512x512 \
    tiffcrop -U px -z 256,256,767,767 -c lzw -p contig "$source_tiff" \
    "$output_dir/images/representative-center-512x512.tif"

run_case()
{
    local label="$1"
    local build_name="$2"
    local tool="$3"
    local input="$4"
    local pixels="$5"
    local collect_telemetry="${6:-0}"
    local log="$output_dir/runs/$label.log"
    local time_file="$output_dir/runs/$label.time"
    local telemetry="$output_dir/runs/$label.telemetry"
    local telemetry_path=0
    local destination="$output_dir/runs/$label.tif"
    local status=0
    local timeout_seconds

    if [ "$collect_telemetry" = "1" ]; then
        telemetry_path="$telemetry"
    fi

    timeout_seconds="$(remaining_timeout "$label")" || return $?

    /usr/bin/time \
        -f 'elapsed_s=%e\nuser_s=%U\nsystem_s=%S\nmax_rss_kb=%M' \
        -o "$time_file" \
        timeout --signal=KILL "$timeout_seconds" \
        env ICC_APPLY_TRACE=1 ICC_APPLY_PROFILES_TIMING=1 \
            "ICC_PERF_STATS_FILE=$telemetry_path" \
            ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$tool" "$input" "$destination" 1 0 1 0 1 \
        "$output_dir/profiles/6ChanSelect-MID.icc" 0 \
        -ENV:bkgX 0.4014 -ENV:bkgY 0.2391 -ENV:bkgZ 0.0272 \
        "$output_dir/profiles/18ChanWithSpots-MVIS.icc" 0 \
        "$source_srgb" 1 > "$log" 2>&1 || status=$?

    elapsed="$(awk -F= '$1 == "elapsed_s" { print $2; exit }' "$time_file")"
    user_time="$(awk -F= '$1 == "user_s" { print $2; exit }' "$time_file")"
    system_time="$(awk -F= '$1 == "system_s" { print $2; exit }' "$time_file")"
    max_rss="$(awk -F= '$1 == "max_rss_kb" { print $2; exit }' "$time_file")"
    apply_ms="$(awk -F': ' '/^\[TIMING\] Apply ms:/ { print $2; exit }' "$log")"
    apply_pct="$(awk -F': ' '/^\[TIMING\] Apply pct:/ { print $2; exit }' "$log")"
    checksum="n/a"
    if [ "$status" -eq 0 ]; then
        checksum="$(sha256sum "$destination" | awk '{ print $1 }')"
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$label" "$build_name" "$pixels" "$status" "${elapsed:-n/a}" \
        "${user_time:-n/a}" "${system_time:-n/a}" "${max_rss:-n/a}" \
        "${apply_ms:-n/a}" "${apply_pct:-n/a}" "$checksum" "$log" >> "$summary"

    if [ "$status" -ne 0 ]; then
        echo "[FAIL] $label exited with status $status; see $log" >&2
        return "$status"
    fi
}

run_case debug-setup-1x1 debug "$debug_apply" \
    "$output_dir/images/setup-1x1.tif" 1
run_case debug-smoke-64x64 debug "$debug_apply" \
    "$output_dir/images/smoke-center-64x64.tif" 4096
run_case debug-representative-512x512 debug "$debug_apply" \
    "$output_dir/images/representative-center-512x512.tif" 262144
run_case debug-full-calibration debug "$debug_apply" "$source_tiff" 1048576
run_case release-full release "$release_apply" "$source_tiff" 1048576
run_case debug-full-telemetry debug "$debug_apply" "$source_tiff" 1048576 1
run_case release-full-telemetry release "$release_apply" "$source_tiff" 1048576 1

for telemetry in \
    "$output_dir/runs/debug-full-telemetry.telemetry" \
    "$output_dir/runs/release-full-telemetry.telemetry"; do
    if [ ! -s "$telemetry" ] ||
       ! grep -q '^clut_calls_dimensions_4=' "$telemetry"; then
        echo "[FAIL] missing 4D CLUT telemetry: $telemetry" >&2
        exit 1
    fi
done

calibration_seconds="$(awk -F '\t' '
    $1 == "debug-full-calibration" { print $5; exit }
' "$summary")"
measured_seconds="$(awk -F '\t' '
    $1 == "debug-full-calibration" || $1 ~ /^debug-full-repeat-/ {
        elapsed += $5
    }
    END { printf "%.6f\n", elapsed }
' "$summary")"
bench_elapsed_seconds="$(awk -F= '$1 == "elapsed_s" { elapsed += $2 }
    END { printf "%.6f\n", elapsed }' \
    "$output_dir/runs/release-bench-no-env.time" \
    "$output_dir/runs/release-bench-env.time")"
additional_runs="$(awk -v target="$target_seconds" \
    -v measured="$measured_seconds" -v calibration="$calibration_seconds" '
    BEGIN {
        if (calibration <= 0)
            exit 1
        remaining = target - measured
        if (remaining > 0)
            print int((remaining / calibration) + 0.5)
        else
            print 0
    }
')"
repeat_budget="$(awk -v calibration="$calibration_seconds" '
    BEGIN { print int((calibration * 1.25) + 5.999999) }
')"

for run in $(seq 1 "$additional_runs"); do
    now="$(date +%s)"
    remaining=$((maximum_seconds - (now - start_epoch)))
    if [ "$remaining" -lt "$repeat_budget" ]; then
        echo "[LIMIT] stopping before repeat $run with ${remaining}s remaining"
        break
    fi
    run_case "debug-full-repeat-$run" debug "$debug_apply" "$source_tiff" 1048576
done

debug_checksum="$(awk -F '\t' '
    $1 == "debug-full-calibration" { print $11; exit }
' "$summary")"
release_checksum="$(awk -F '\t' '
    $1 == "release-full" { print $11; exit }
' "$summary")"
if [ "$debug_checksum" != "$release_checksum" ]; then
    echo "[FAIL] Debug and Release output checksums differ" >&2
    exit 1
fi
if ! awk -F '\t' -v expected="$debug_checksum" '
    NR > 1 && ($1 ~ /^debug-full/ || $1 ~ /^release-full/) &&
        $11 != expected {
        print "[FAIL] checksum mismatch: " $1 " " $11 > "/dev/stderr"
        failed = 1
    }
    END { exit failed }
' "$summary"; then
    exit 1
fi

wall_elapsed_seconds=$(( $(date +%s) - start_epoch ))
awk -F '\t' -v target="$target_seconds" -v maximum="$maximum_seconds" \
    -v wall="$wall_elapsed_seconds" -v bench_elapsed="$bench_elapsed_seconds" '
    NR > 1 {
        elapsed += $5
        apply += $9
        pixels += $3
        runs++
        if ($1 ~ /^debug-full/) {
            if ($1 == "debug-full-telemetry") {
                debug_telemetry_elapsed = $5
                debug_telemetry_apply = $9
                next
            }
            debug_elapsed += $5
            debug_apply += $9
            debug_pixels += $3
            debug_runs++
        }
        if ($1 == "release-full-telemetry") {
            release_telemetry_elapsed = $5
            release_telemetry_apply = $9
            next
        }
        if ($1 == "release-full") {
            release_elapsed = $5
            release_apply = $9
            release_pixels = $3
        }
    }
    END {
        printf "target_seconds=%s\nmaximum_seconds=%s\n", target, maximum
        printf "wall_elapsed_s=%s\nruns=%d\npixels=%d\n", wall, runs, pixels
        printf "bench_runs=2\ntotal_measured_runs=%d\n", runs + 2
        printf "measured_elapsed_s=%.3f\napply_ms=%.3f\n",
            elapsed + bench_elapsed, apply
        printf "bench_elapsed_s=%.3f\n", bench_elapsed
        if (debug_runs > 0) {
            printf "debug_full_runs=%d\n", debug_runs
            printf "debug_full_mean_elapsed_s=%.6f\n", debug_elapsed / debug_runs
            printf "debug_full_mean_apply_ms=%.3f\n", debug_apply / debug_runs
            printf "debug_full_mpix_per_s=%.6f\n",
                debug_pixels / debug_elapsed / 1000000
        }
        if (release_elapsed > 0) {
            printf "release_full_elapsed_s=%.6f\n", release_elapsed
            printf "release_full_apply_ms=%.3f\n", release_apply
            printf "release_full_mpix_per_s=%.6f\n",
                release_pixels / release_elapsed / 1000000
            printf "release_full_apply_mpix_per_s=%.6f\n",
                release_pixels / (release_apply / 1000) / 1000000
        }
        if (debug_runs > 0 && release_elapsed > 0)
            printf "debug_release_elapsed_ratio=%.6f\n",
                (debug_elapsed / debug_runs) / release_elapsed
        if (debug_telemetry_elapsed > 0 && debug_runs > 0)
            printf "debug_telemetry_elapsed_overhead_pct=%.6f\n",
                ((debug_telemetry_elapsed / (debug_elapsed / debug_runs)) - 1) * 100
        if (release_telemetry_elapsed > 0 && release_elapsed > 0)
            printf "release_telemetry_elapsed_overhead_pct=%.6f\n",
                ((release_telemetry_elapsed / release_elapsed) - 1) * 100
    }
' "$summary" > "$output_dir/report.txt"
{
    sed 's/^/debug_telemetry_/' \
        "$output_dir/runs/debug-full-telemetry.telemetry"
    sed 's/^/release_telemetry_/' \
        "$output_dir/runs/release-full-telemetry.telemetry"
    printf 'output_sha256=%s\n' "$debug_checksum"
} >> "$output_dir/report.txt"
release_rate="$(awk -F= '$1 == "release_full_mpix_per_s" {
    print $2; exit }' "$output_dir/report.txt")"
release_apply_rate="$(awk -F= '$1 == "release_full_apply_mpix_per_s" {
    print $2; exit }' "$output_dir/report.txt")"
{
    printf 'bench_input=deterministic_lcg_not_tiff\n'
    printf 'bench_apply_granularity=multi_pixel_not_tiff_scalar\n'
    printf 'bench_comparison=diagnostic_not_directly_comparable\n'
    printf 'bench_pixels=1048576\n'
    printf 'bench_repeats=5\n'
    printf 'bench_no_env_mpix_per_s=%s\n' "$bench_no_env_rate"
    printf 'bench_no_env_checksum=%s\n' "$bench_no_env_checksum"
    printf 'bench_env_mpix_per_s=%s\n' "$bench_env_rate"
    printf 'bench_env_checksum=%s\n' "$bench_env_checksum"
    awk -v env="$bench_env_rate" -v noenv="$bench_no_env_rate" \
        -v release="$release_rate" -v apply="$release_apply_rate" 'BEGIN {
        printf "bench_env_rate_delta_pct=%.6f\n", ((env / noenv) - 1) * 100
        printf "bench_vs_tiff_processing_diagnostic_pct=%.6f\n",
            ((env / release) - 1) * 100
        printf "bench_vs_tiff_scalar_apply_diagnostic_pct=%.6f\n",
            ((env / apply) - 1) * 100
    }'
} >> "$output_dir/report.txt"

echo "[PASS] MCS profiling report: $output_dir"
cat "$output_dir/report.txt"
