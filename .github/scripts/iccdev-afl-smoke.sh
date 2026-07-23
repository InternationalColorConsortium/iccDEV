#!/bin/bash
###############################################################################
# Build iccDEV with AFL++ instrumentation and run bounded CLI smoke fuzzing.
###############################################################################

set -euo pipefail

usage() {
    sed -n '2,4p' "$0" | sed 's/^# \?//'
    echo ""
    echo "Usage: $0 [--seconds N] [--targets CSV] [--build-type TYPE] [--exec-timeout-ms N] [--build-dir DIR] [--work-dir DIR] [--skip-build]"
    echo ""
    echo "Targets: dump, toxml, fromcube"
}

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
seconds="${AFL_SECONDS:-300}"
targets_csv="${AFL_TARGETS:-dump}"
build_dir="${AFL_BUILD_DIR:-$repo_root/build-afl-smoke}"
work_dir="${AFL_WORK_DIR:-$repo_root/.afl-smoke}"
build_type="${AFL_BUILD_TYPE:-Debug}"
exec_timeout_ms="${AFL_EXEC_TIMEOUT_MS:-5000}"
skip_build=0

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

trim_target() {
    local value="$1"

    value="${value#"${value%%[![:space:]]*}"}"
    value="${value%"${value##*[![:space:]]}"}"
    printf '%s' "$value"
}

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

if [ "$skip_build" -eq 0 ]; then
    cmake -S "$repo_root/Build/Cmake" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_C_COMPILER=afl-clang-fast \
        -DCMAKE_CXX_COMPILER=afl-clang-fast++ \
        -DCMAKE_C_FLAGS="-O1 -g -fno-omit-frame-pointer" \
        -DCMAKE_CXX_FLAGS="-O1 -g -fno-omit-frame-pointer" \
        -DENABLE_TOOLS=ON \
        -DENABLE_TESTS=OFF \
        -DENABLE_WXWIDGETS=OFF \
        -DENABLE_SHARED_LIBS=OFF \
        -DENABLE_STATIC_LIBS=ON \
        -DENABLE_LTO=OFF \
        -Wno-dev
    cmake --build "$build_dir" --parallel "$(nproc)"
fi

seed_root="$work_dir/seeds"
mkdir -p "$seed_root/icc" "$seed_root/cube"
cp "$repo_root"/.github/ci/test-data/*.icc "$seed_root/icc/"
cp "$repo_root"/.github/ci/test-data/test-identity.cube "$seed_root/cube/"
cp "$repo_root"/.github/ci/afl-seeds/cube/*.cube "$seed_root/cube/"

summary_tsv="$work_dir/summary.tsv"
printf 'target\tstatus\tseconds\texecs_done\tsaved_crashes\tsaved_hangs\tout_dir\n' > "$summary_tsv"

run_afl_target() {
    local target="$1"
    local binary=""
    local in_dir=""
    local out_dir="$work_dir/out-$target"
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
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1,detect_leaks=0,allocator_may_return_null=1,symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1,print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ ALL
            afl_status="$?"
            ;;
        toxml)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1,detect_leaks=0,allocator_may_return_null=1,symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1,print_stacktrace=1 \
              afl-fuzz -i "$in_dir" -o "$out_dir" -V "$seconds" -t "$exec_timeout_ms" -- "$binary" @@ "$generated/out.xml"
            afl_status="$?"
            ;;
        fromcube)
            AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
              ASAN_OPTIONS=abort_on_error=1,detect_leaks=0,allocator_may_return_null=1,symbolize=0 \
              UBSAN_OPTIONS=halt_on_error=1,print_stacktrace=1 \
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
        printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" >> "$summary_tsv"
        return "$afl_status"
    fi

    if [ "$crashes" != "0" ] || [ "$hangs" != "0" ]; then
        printf '%s\tfail\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" >> "$summary_tsv"
        echo "ERROR: AFL target $target reported crashes=$crashes hangs=$hangs" >&2
        return 1
    fi

    printf '%s\tpass\t%s\t%s\t%s\t%s\t%s\n' "$target" "$seconds" "$execs_done" "$crashes" "$hangs" "$out_dir" >> "$summary_tsv"
}

failures=0
for target in "${targets[@]}"; do
    target="$(trim_target "$target")"
    if [ -z "$target" ]; then
        echo "ERROR: empty AFL target entry in --targets" >&2
        failures=$((failures + 1))
        continue
    fi
    if [[ "$target" =~ [[:space:]] ]]; then
        echo "ERROR: malformed AFL target contains whitespace: $target" >&2
        failures=$((failures + 1))
        continue
    fi
    if ! run_afl_target "$target"; then
        failures=$((failures + 1))
    fi
done

echo "AFL smoke summary: $summary_tsv"
cat "$summary_tsv"

if [ "$failures" -ne 0 ]; then
    exit 1
fi
