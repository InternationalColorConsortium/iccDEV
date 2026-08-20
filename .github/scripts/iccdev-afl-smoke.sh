#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Build iccDEV with AFL++ instrumentation and run bounded CLI smoke fuzzing.
###############################################################################

set -euo pipefail

usage() {
    sed -n '2,4p' "$0" | sed 's/^# \?//'
    echo ""
    echo "Usage: $0 [--seconds N] [--targets CSV] [--build-type TYPE] [--exec-timeout-ms N] [--build-dir DIR] [--work-dir DIR] [--patches [DIR]] [--skip-build]"
    echo ""
    echo "Targets: dump, toxml, fromxml, tojson, fromjson, roundtrip, fromcube"
}

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
seconds="${AFL_SECONDS:-300}"
targets_csv="${AFL_TARGETS:-dump}"
build_dir="${AFL_BUILD_DIR:-$repo_root/build-afl-smoke}"
work_dir="${AFL_WORK_DIR:-$repo_root/.afl-smoke}"
build_type="${AFL_BUILD_TYPE:-Debug}"
exec_timeout_ms="${AFL_EXEC_TIMEOUT_MS:-30000}"
# Raised from 49152 to admit the whole committed seed corpus, matching
# cfl/build.sh after #2120. Exactly one committed seed exceeds the old default --
# .github/ci/test-data/fuzz-clut-hbo-69197729.icc at 61,015 bytes -- so every AFL
# smoke run staged that file and then deleted it again before fuzzing.
#
# What the loss costs the seven fork/exec CLI targets here is NOT measured. #2120
# measured it for the in-process CFL writer targets -- 1 of 11 seeds reaches
# RenderRaster and it was the pruned one -- and deliberately did not extend the
# claim to the CLI wrappers, which consume a profile differently. The reason to
# raise the cap is narrower: the seed is a deliberate regression artifact -- added
# by #1517 alongside the tool it exercises, and named as a CLUT heap-buffer-overflow
# reproducer -- and a cap that quietly removes one turns a coverage decision into an
# accident. AFL_MAX_SEED_BYTES still lowers the cap per-run, but only as far as the
# largest committed seed: below that, prune_large_seeds refuses the run instead of
# dropping the seed, so a lane that genuinely wants smaller inputs has to remove the
# seed deliberately. That refusal is the explicit decision #2120 asked for, which is
# why this knob is not a free dial downwards.
max_seed_bytes="${AFL_MAX_SEED_BYTES:-262144}"
skip_build=0
apply_patches="${ICCDEV_AFL_APPLY_PATCHES:-0}"
patch_dir="${ICCDEV_AFL_PATCH_DIR:-$repo_root/.github/ci/fuzz-patches/afl}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --seconds)
            [ "$#" -ge 2 ] || { echo "ERROR: --seconds requires a value" >&2; exit 2; }
            seconds="$2"
            shift 2
            ;;
        --targets)
            [ "$#" -ge 2 ] || { echo "ERROR: --targets requires a value" >&2; exit 2; }
            targets_csv="$2"
            shift 2
            ;;
        --build-dir)
            [ "$#" -ge 2 ] || { echo "ERROR: --build-dir requires a value" >&2; exit 2; }
            build_dir="$2"
            shift 2
            ;;
        --build-type)
            [ "$#" -ge 2 ] || { echo "ERROR: --build-type requires a value" >&2; exit 2; }
            build_type="$2"
            shift 2
            ;;
        --exec-timeout-ms)
            [ "$#" -ge 2 ] || { echo "ERROR: --exec-timeout-ms requires a value" >&2; exit 2; }
            exec_timeout_ms="$2"
            shift 2
            ;;
        --work-dir)
            [ "$#" -ge 2 ] || { echo "ERROR: --work-dir requires a value" >&2; exit 2; }
            work_dir="$2"
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        --patches)
            apply_patches=1
            if [ "$#" -ge 2 ] && [ "${2#--}" = "$2" ]; then
                patch_dir="$2"
                shift 2
            else
                shift
            fi
            ;;
        --patch-dir)
            [ "$#" -ge 2 ] || { echo "ERROR: --patch-dir requires a value" >&2; exit 2; }
            patch_dir="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$seconds" in
    ''|*[!0-9]*)
        echo "ERROR: --seconds must be numeric" >&2
        exit 2
        ;;
esac

if [ "$seconds" -lt 1 ] || [ "$seconds" -gt 3600 ]; then
    echo "ERROR: --seconds must be between 1 and 3600" >&2
    exit 2
fi

case "$exec_timeout_ms" in
    ''|*[!0-9]*)
        echo "ERROR: --exec-timeout-ms must be numeric" >&2
        exit 2
        ;;
esac

if [ "$exec_timeout_ms" -lt 20 ] || [ "$exec_timeout_ms" -gt 30000 ]; then
    echo "ERROR: --exec-timeout-ms must be between 20 and 30000" >&2
    exit 2
fi

case "$max_seed_bytes" in
    ''|*[!0-9]*)
        echo "ERROR: AFL_MAX_SEED_BYTES must be numeric" >&2
        exit 2
        ;;
esac

if [ "$max_seed_bytes" -lt 1024 ]; then
    echo "ERROR: AFL_MAX_SEED_BYTES must be at least 1024" >&2
    exit 2
fi

case "$build_type" in
    Debug|Release|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        echo "ERROR: unsupported build type: $build_type" >&2
        exit 2
        ;;
esac

IFS=',' read -r -a targets <<< "$targets_csv"
if [ "${#targets[@]}" -eq 0 ]; then
    echo "ERROR: at least one target is required" >&2
    exit 2
fi
if [ "${#targets[@]}" -gt 7 ]; then
    echo "ERROR: at most seven AFL target entries may be selected" >&2
    exit 2
fi

trim_target() {
    local value="$1"

    value="${value#"${value%%[![:space:]]*}"}"
    value="${value%"${value##*[![:space:]]}"}"
    printf '%s' "$value"
}

selected_targets=()
for target in "${targets[@]}"; do
    target="$(trim_target "$target")"
    if [ -z "$target" ]; then
        echo "ERROR: empty AFL target entry in --targets" >&2
        exit 2
    fi
    if [[ "$target" =~ [[:space:]] ]]; then
        echo "ERROR: malformed AFL target contains whitespace: $target" >&2
        exit 2
    fi
    case "$target" in
        dump|toxml|fromxml|tojson|fromjson|roundtrip|fromcube)
            ;;
        *)
            echo "ERROR: unsupported AFL target: $target" >&2
            exit 2
            ;;
    esac
    for selected_target in "${selected_targets[@]}"; do
        if [ "$target" = "$selected_target" ]; then
            echo "ERROR: duplicate AFL target: $target" >&2
            exit 2
        fi
    done
    selected_targets+=("$target")
done

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: required tool not found: $1" >&2
        exit 127
    fi
}

require_tool cmake
require_tool afl-fuzz
require_tool afl-clang-fast
require_tool afl-clang-fast++

mkdir -p "$work_dir"

core_pattern_is_safe=0
if [ -r /proc/sys/kernel/core_pattern ]; then
    core_pattern="$(cat /proc/sys/kernel/core_pattern)"
    case "$core_pattern" in
        '|'*)
            echo "WARNING: piped Linux core_pattern can make AFL crashes look like hangs" >&2
            ;;
        *)
            core_pattern_is_safe=1
            ;;
    esac
else
    echo "WARNING: cannot read Linux core_pattern; AFL hangs remain blocking" >&2
fi

if [ "$skip_build" -eq 0 ]; then
    if [ "$apply_patches" != "0" ]; then
        patch_applicator="${ICCDEV_FUZZ_PATCH_APPLICATOR:-$repo_root/.github/scripts/iccdev-apply-fuzz-patches.sh}"
        "$patch_applicator" --mode afl --patch-dir "$patch_dir"
    fi

    export AFL_USE_ASAN=1
    export AFL_USE_UBSAN=1

    cmake_zlib_args=()
    if [ -e /usr/lib/x86_64-linux-gnu/libz.so ]; then
        cmake_zlib_args+=("-DZLIB_LIBRARY=/usr/lib/x86_64-linux-gnu/libz.so")
    fi
    if [ -r /usr/include/zlib.h ]; then
        cmake_zlib_args+=("-DZLIB_INCLUDE_DIR=/usr/include")
    fi

    cmake -S "$repo_root/Build/Cmake" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_C_COMPILER=afl-clang-fast \
        -DCMAKE_CXX_COMPILER=afl-clang-fast++ \
        -DCMAKE_C_FLAGS="-O1 -g -fno-omit-frame-pointer" \
        -DCMAKE_CXX_FLAGS="-O1 -g -fno-omit-frame-pointer" \
        -DENABLE_TOOLS=ON \
        -DENABLE_TESTS=OFF \
        -DENABLE_WXWIDGETS=OFF \
        -DENABLE_IMAGE_TOOLS=OFF \
        -DENABLE_SHARED_LIBS=OFF \
        -DENABLE_STATIC_LIBS=ON \
        -DENABLE_LTO=OFF \
        "${cmake_zlib_args[@]}" \
        -Wno-dev
    cmake --build "$build_dir" --parallel "$(nproc)"
fi

seed_root="$work_dir/seeds"
seed_inventory_tsv="$work_dir/seed-inventory.tsv"
skipped_seeds_tsv="$work_dir/skipped-seeds.tsv"
committed_test_data="$repo_root/.github/ci/test-data"
committed_afl_cube="$repo_root/.github/ci/afl-seeds/cube"
# Every committed root the staging below draws from. prune_large_seeds resolves a
# staged seed back through this same list, so a root added here is protected there in
# the same edit. Keeping the two apart is how the guard would fail open -- silently
# back to the old rm, which is the regression #2120 exists to prevent.
committed_seed_roots=("$committed_test_data" "$committed_afl_cube")

mkdir -p "$seed_root/icc" "$seed_root/xml" "$seed_root/json" "$seed_root/cube"
cp "$committed_test_data"/*.icc "$seed_root/icc/"
cp "$committed_test_data"/*.xml "$seed_root/xml/" 2>/dev/null || true
cp "$committed_test_data/test-identity.cube" "$seed_root/cube/"
cp "$committed_afl_cube"/*.cube "$seed_root/cube/"

if [ -x "$build_dir/Tools/IccToXml/iccToXml" ]; then
    first_icc="$(find "$seed_root/icc" -maxdepth 1 -type f -name '*.icc' | sort | head -n 1)"
    if [ -n "$first_icc" ]; then
        "$build_dir/Tools/IccToXml/iccToXml" "$first_icc" "$seed_root/xml/generated-from-seed.xml" >/dev/null 2>&1 || true
    fi
fi
if [ -x "$build_dir/Tools/IccToJson/iccToJson" ]; then
    first_icc="$(find "$seed_root/icc" -maxdepth 1 -type f -name '*.icc' | sort | head -n 1)"
    if [ -n "$first_icc" ]; then
        "$build_dir/Tools/IccToJson/iccToJson" "$first_icc" "$seed_root/json/generated-from-seed.json" >/dev/null 2>&1 || true
    fi
fi
if ! find "$seed_root/xml" -maxdepth 1 -type f | grep -q .; then
    printf '<IccProfile></IccProfile>\n' > "$seed_root/xml/minimal.xml"
fi
if ! find "$seed_root/json" -maxdepth 1 -type f | grep -q .; then
    printf '{"IccProfile":{}}\n' > "$seed_root/json/minimal.json"
fi

printf 'kind\tsize\tfile\n' > "$seed_inventory_tsv"
printf 'kind\tsize\tfile\treason\n' > "$skipped_seeds_tsv"

# Report the committed source of a staged seed, if it has one, walking the same
# committed_seed_roots the staging above copied from. Everything else in the seed
# tree is generated per run -- generated-from-seed.{xml,json} and the
# minimal.{xml,json} fallbacks -- and only the committed ones are protected below,
# which is why this resolves a real path rather than returning a flag: the error
# names the file a maintainer would have to act on.
committed_seed_source() {
    local base="$1"
    local root=""

    for root in "${committed_seed_roots[@]}"; do
        if [ -f "$root/$base" ]; then
            printf '%s\n' "$root/$base"
            return 0
        fi
    done
    return 1
}

prune_large_seeds() {
    local kind="$1"
    local dir="$2"
    local file=""
    local size=""
    local committed_source=""

    [ -d "$dir" ] || return 0
    while IFS= read -r -d '' file; do
        size="$(stat -c '%s' "$file")"
        printf '%s\t%s\t%s\n' "$kind" "$size" "$file" >> "$seed_inventory_tsv"
        if [ "$size" -gt "$max_seed_bytes" ]; then
            # A seed committed under .github/ci is a deliberate regression
            # artifact, not corpus bloat: dropping one removes whatever defect
            # class it was added to cover while the run still reports execs and a
            # clean exit. #2120 is exactly that -- the corpus's only CLUT seed was
            # staged and deleted on every run for as long as the cap sat below it,
            # traced only by the TSV line below and the summary it feeds.
            #
            # So committed seeds are never pruned, they are a hard error, as in
            # cfl/build.sh. Adding an oversized regression seed now forces an
            # explicit decision about the cap. Generated seeds still prune
            # silently, which is what the cap is for.
            if committed_source="$(committed_seed_source "$(basename "$file")")"; then
                # Record it before refusing. ci-afl-smoke.yml summarizes this TSV
                # under "Seed Size Review" with if: always(), so without a row here
                # an aborted run reports "No oversized AFL smoke seeds were skipped."
                # -- the reassuring branch -- on the very run an oversized seed
                # stopped, leaving the reason only in the raw step log.
                printf '%s\t%s\t%s\tcommitted-oversized-hard-error\n' "$kind" "$size" "$file" >> "$skipped_seeds_tsv"
                echo "ERROR: committed seed exceeds AFL_MAX_SEED_BYTES and would be dropped:" >&2
                echo "       $committed_source is $size bytes, cap is $max_seed_bytes" >&2
                echo "       Raise AFL_MAX_SEED_BYTES, or remove the seed deliberately (#2120)." >&2
                # exit 2, not 1: this script already spells configuration rejection
                # as 2 (the argument checks above, the unsupported-target case below),
                # and 1 is what a real fuzzing failure returns. A caller can then tell
                # "nothing was fuzzed, the cap is wrong" from "AFL found something".
                exit 2
            fi
            printf '%s\t%s\t%s\tlarger-than-%s-bytes\n' "$kind" "$size" "$file" "$max_seed_bytes" >> "$skipped_seeds_tsv"
            rm -f -- "$file"
        fi
    done < <(find "$dir" -maxdepth 1 -type f -print0 | sort -z)
}

prune_large_seeds "icc" "$seed_root/icc"
prune_large_seeds "xml" "$seed_root/xml"
prune_large_seeds "json" "$seed_root/json"
prune_large_seeds "cube" "$seed_root/cube"

if [ "$(awk 'END { print NR }' "$skipped_seeds_tsv")" -gt 1 ]; then
    echo "Skipped AFL smoke seeds larger than $max_seed_bytes bytes:"
    tail -n +2 "$skipped_seeds_tsv"
fi

summary_tsv="$work_dir/summary.tsv"
findings_dir="$work_dir/findings"
findings_tsv="$work_dir/findings.tsv"
printf 'target\tstatus\tseconds\texecs_done\tsaved_crashes\tsaved_hangs\tout_dir\n' > "$summary_tsv"
rm -rf "$findings_dir"
mkdir -p "$findings_dir"
printf 'target\tkind\tsource_file\tartifact_file\n' > "$findings_tsv"

collect_afl_findings() {
    local target="$1"
    local kind=""
    local source_dir=""
    local dest_dir=""
    local file=""
    local base_name=""
    local safe_name=""
    local name_hash=""
    local artifact_file=""

    for kind in crashes hangs; do
        source_dir="$work_dir/out-$target/default/$kind"
        [ -d "$source_dir" ] || continue
        dest_dir="$findings_dir/$target/$kind"
        mkdir -p "$dest_dir"
        while IFS= read -r -d '' file; do
            base_name="$(basename "$file")"
            safe_name="$base_name"
            safe_name="${safe_name//\"/_}"
            safe_name="${safe_name//:/_}"
            safe_name="${safe_name//</_}"
            safe_name="${safe_name//>/_}"
            safe_name="${safe_name//|/_}"
            safe_name="${safe_name//\*/_}"
            safe_name="${safe_name//\?/_}"
            name_hash="$(printf '%s' "$base_name" | sha256sum | awk '{ print substr($1, 1, 12) }')"
            safe_name="$name_hash-$safe_name"
            artifact_file="$target/$kind/$safe_name"
            cp -- "$file" "$dest_dir/$safe_name"
            printf '%s\t%s\t%s\t%s\n' "$target" "$kind" "$file" "$artifact_file" >> "$findings_tsv"
        done < <(find "$source_dir" -maxdepth 1 -type f ! -name 'README*' -print0 | sort -z)
    done
}

run_afl_target() {
    local target="$1"
    local binary=""
    local in_dir=""
    local out_dir="$work_dir/out-$target"
    local result_tsv="$work_dir/result-$target.tsv"
    local generated="$work_dir/generated"
    local afl_status=0
    local stats_file=""
    local execs_done="0"
    local crashes="0"
    local hangs="0"

    mkdir -p "$generated"

    case "$target" in
        dump)
            binary="$build_dir/Tools/IccDumpProfile/iccDumpProfile"
            in_dir="$seed_root/icc"
            ;;
        toxml)
            binary="$build_dir/Tools/IccToXml/iccToXml"
            in_dir="$seed_root/icc"
            ;;
        fromxml)
            binary="$build_dir/Tools/IccFromXml/iccFromXml"
            in_dir="$seed_root/xml"
            ;;
        tojson)
            binary="$build_dir/Tools/IccToJson/iccToJson"
            in_dir="$seed_root/icc"
            ;;
        fromjson)
            binary="$build_dir/Tools/IccFromJson/iccFromJson"
            in_dir="$seed_root/json"
            ;;
        roundtrip)
            binary="$build_dir/Tools/IccRoundTrip/iccRoundTrip"
            in_dir="$seed_root/icc"
            ;;
        fromcube)
            binary="$build_dir/Tools/IccFromCube/iccFromCube"
            in_dir="$seed_root/cube"
            ;;
        *)
            echo "ERROR: unsupported AFL target: $target" >&2
            return 2
            ;;
    esac

    if [ ! -x "$binary" ]; then
        echo "ERROR: target binary missing or not executable: $binary" >&2
        return 1
    fi

    rm -rf "$out_dir"
    mkdir -p "$out_dir"

    set +e
    case "$target" in
        dump)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ ALL
            afl_status="$?"
            ;;
        toxml)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ "$generated/out.xml"
            afl_status="$?"
            ;;
        fromxml)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ "$generated/fromxml-out.icc"
            afl_status="$?"
            ;;
        tojson)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ "$generated/tojson-out.json"
            afl_status="$?"
            ;;
        fromjson)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ "$generated/fromjson-out.icc"
            afl_status="$?"
            ;;
        roundtrip)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ 1 0
            afl_status="$?"
            ;;
        fromcube)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_TRY_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ "$generated/out.icc"
            afl_status="$?"
            ;;
    esac
    set -e

    stats_file="$out_dir/default/fuzzer_stats"
    stat_value() {
        local key="$1"
        local fallback_key="${2:-}"

        awk -F: -v key="$key" -v fallback_key="$fallback_key" '
            $1 ~ "^[[:space:]]*" key "[[:space:]]*$" {
                gsub(/[[:space:]]/, "", $2)
                print $2
                found = 1
            }
            !found && fallback_key != "" && $1 ~ "^[[:space:]]*" fallback_key "[[:space:]]*$" {
                gsub(/[[:space:]]/, "", $2)
                print $2
                found = 1
            }
        ' "$stats_file"
    }

    if [ -r "$stats_file" ]; then
        execs_done="$(stat_value execs_done)"
        crashes="$(stat_value saved_crashes unique_crashes)"
        hangs="$(stat_value saved_hangs unique_hangs)"
    fi
    execs_done="${execs_done:-0}"
    crashes="${crashes:-0}"
    hangs="${hangs:-0}"

    if [ "$afl_status" -ne 0 ]; then
        printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
        return "$afl_status"
    fi

    case "$execs_done" in
        ''|*[!0-9]*)
            printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
            echo "ERROR: AFL target $target did not report a positive execs_done value" >&2
            return 1
            ;;
    esac
    if [ "$execs_done" -le 0 ]; then
        printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
        echo "ERROR: AFL target $target completed without executing test cases" >&2
        return 1
    fi

    if [ "$crashes" != "0" ]; then
        printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
        echo "ERROR: AFL target $target reported crashes=$crashes hangs=$hangs" >&2
        return 1
    fi
    if [ "$hangs" != "0" ]; then
        if [ "$core_pattern_is_safe" -eq 1 ]; then
            printf '%s\twarn\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
            echo "WARNING: AFL target $target reported hangs=$hangs" >&2
            return 0
        fi
        printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
        echo "ERROR: AFL target $target reported hangs=$hangs with unsafe or unknown core_pattern" >&2
        return 1
    fi

    printf '%s\tpass\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" > "$result_tsv"
}

failures=0
pids=()
for target in "${selected_targets[@]}"; do
    log_file="$work_dir/log-$target.txt"
    rm -f "$work_dir/result-$target.tsv" "$log_file"
    run_afl_target "$target" > "$log_file" 2>&1 &
    pids+=("$!")
done

for index in "${!selected_targets[@]}"; do
    target="${selected_targets[$index]}"
    pid="${pids[$index]}"
    log_file="$work_dir/log-$target.txt"
    if ! wait "$pid"; then
        failures=$((failures + 1))
    fi
    echo "AFL target log: $target"
    cat "$log_file"
    if [ -r "$work_dir/result-$target.tsv" ]; then
        cat "$work_dir/result-$target.tsv" >> "$summary_tsv"
        collect_afl_findings "$target"
    else
        printf '%s\tfail\t%s\t0\t0\t0\t%s\n' "$target" "$seconds" "$work_dir/out-$target" >> "$summary_tsv"
        failures=$((failures + 1))
    fi
done

echo "AFL smoke summary: $summary_tsv"
cat "$summary_tsv"
if [ "$(awk 'END { print NR }' "$findings_tsv")" -gt 1 ]; then
    cp -- "$findings_tsv" "$findings_dir/manifest.tsv"
    triage_status=0
    "$repo_root/.github/scripts/iccdev-fuzz-triage.sh" \
        --work-dir "$work_dir" \
        --build-dir "$build_dir" \
        --findings "$findings_tsv" \
        --timeout 20 || triage_status="$?"
    if [ -d "$work_dir/triage" ]; then
        rm -rf "$findings_dir/triage"
        cp -R "$work_dir/triage" "$findings_dir/triage"
    fi
    if [ "$triage_status" -ne 0 ]; then
        failures=$((failures + 1))
    fi
fi

if [ "$failures" -ne 0 ]; then
    exit 1
fi
