#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Build and optionally smoke-test the core CFL command-line fuzzers.
###############################################################################

set -euo pipefail

usage() {
  echo "Usage: $0 [--targets CSV] [--runs N] [--build-dir DIR] [--work-dir DIR] [--patches [DIR]] [--skip-run]"
  echo "Targets: dump, toxml, fromxml, tojson, fromjson, roundtrip"
}

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
build_dir="${ICCDEV_CFL_BUILD_DIR:-$repo_root/build-cfl-smoke}"
work_dir="${ICCDEV_CFL_WORK_DIR:-$repo_root/.cfl-smoke}"
targets_csv="${ICCDEV_CFL_TARGETS:-dump,toxml,fromxml,tojson,fromjson,roundtrip}"
runs="${ICCDEV_CFL_RUNS:-1}"
apply_patches="${ICCDEV_CFL_APPLY_PATCHES:-0}"
patch_dir="${ICCDEV_CFL_PATCH_DIR:-$repo_root/.github/ci/fuzz-patches/cfl}"
skip_run=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --targets)
      [ "$#" -ge 2 ] || { echo "ERROR: --targets requires a value" >&2; exit 2; }
      targets_csv="$2"
      shift 2
      ;;
    --runs)
      [ "$#" -ge 2 ] || { echo "ERROR: --runs requires a value" >&2; exit 2; }
      runs="$2"
      shift 2
      ;;
    --build-dir)
      [ "$#" -ge 2 ] || { echo "ERROR: --build-dir requires a directory" >&2; exit 2; }
      build_dir="$2"
      shift 2
      ;;
    --work-dir)
      [ "$#" -ge 2 ] || { echo "ERROR: --work-dir requires a directory" >&2; exit 2; }
      work_dir="$2"
      shift 2
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
      [ "$#" -ge 2 ] || { echo "ERROR: --patch-dir requires a directory" >&2; exit 2; }
      patch_dir="$2"
      shift 2
      ;;
    --skip-run)
      skip_run=1
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

case "$runs" in
  ''|*[!0-9]*)
    echo "ERROR: --runs must be numeric" >&2
    exit 2
    ;;
esac

if [ "$runs" -lt 1 ] || [ "$runs" -gt 1000000 ]; then
  echo "ERROR: --runs must be between 1 and 1000000" >&2
  exit 2
fi

IFS=',' read -r -a requested_targets <<< "$targets_csv"
selected_targets=()
for target in "${requested_targets[@]}"; do
  target="${target#"${target%%[![:space:]]*}"}"
  target="${target%"${target##*[![:space:]]}"}"
  case "$target" in
    dump|toxml|fromxml|tojson|fromjson|roundtrip)
      selected_targets+=("$target")
      ;;
    *)
      echo "ERROR: unsupported CFL target: $target" >&2
      exit 2
      ;;
  esac
done

for tool in cmake clang++; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required tool not found: $tool" >&2
    exit 127
  fi
done

if [ "$apply_patches" != "0" ]; then
  "$repo_root/.github/scripts/iccdev-apply-fuzz-patches.sh" --mode cfl --patch-dir "$patch_dir"
fi

cmake -S "$repo_root/Build/Cmake" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined" \
  -DCMAKE_CXX_FLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined" \
  -DENABLE_TOOLS=ON \
  -DENABLE_TESTS=OFF \
  -DENABLE_WXWIDGETS=OFF \
  -DENABLE_SHARED_LIBS=OFF \
  -DENABLE_STATIC_LIBS=ON \
  -DENABLE_LTO=OFF \
  -Wno-dev
cmake --build "$build_dir" --parallel "$(nproc)"

bin_dir="$script_dir/bin"
mkdir -p "$bin_dir"
for target in "${selected_targets[@]}"; do
  clang++ -std=c++17 -g -O1 -fno-omit-frame-pointer \
    -fsanitize=fuzzer,address,undefined \
    -DICCDEV_CFL_TARGET="\"$target\"" \
    "$script_dir/icc_cli_fuzzer.cpp" \
    -o "$bin_dir/icc_${target}_fuzzer"
done

seed_root="$work_dir/seeds"
mkdir -p "$seed_root/icc" "$seed_root/xml" "$seed_root/json"
cp "$repo_root"/.github/ci/test-data/*.icc "$seed_root/icc/"
cp "$repo_root"/.github/ci/test-data/*.xml "$seed_root/xml/" 2>/dev/null || true

first_icc="$(find "$seed_root/icc" -maxdepth 1 -type f -name '*.icc' | sort | head -n 1)"
if [ -n "$first_icc" ]; then
  "$build_dir/Tools/IccToXml/iccToXml" "$first_icc" "$seed_root/xml/generated-from-seed.xml" >/dev/null 2>&1 || true
  "$build_dir/Tools/IccToJson/iccToJson" "$first_icc" "$seed_root/json/generated-from-seed.json" >/dev/null 2>&1 || true
fi
if ! find "$seed_root/xml" -maxdepth 1 -type f | grep -q .; then
  printf '<IccProfile></IccProfile>\n' > "$seed_root/xml/minimal.xml"
fi
if ! find "$seed_root/json" -maxdepth 1 -type f | grep -q .; then
  printf '{"IccProfile":{}}\n' > "$seed_root/json/minimal.json"
fi

summary_tsv="$work_dir/summary.tsv"
logs_dir="$work_dir/logs"
artifacts_dir="$work_dir/artifacts"
mkdir -p "$logs_dir" "$artifacts_dir"
printf 'target\tstatus\truns\tcorpus\n' > "$summary_tsv"

if [ "$skip_run" -eq 0 ]; then
  for target in "${selected_targets[@]}"; do
    case "$target" in
      fromxml) corpus="$seed_root/xml" ;;
        fromjson) corpus="$seed_root/json" ;;
        *) corpus="$seed_root/icc" ;;
    esac
    log_file="$logs_dir/$target.log"
    ICCDEV_CFL_TOOL_DIR="$build_dir/Tools" \
      ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:allocator_may_return_null=1 \
      UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
      "$bin_dir/icc_${target}_fuzzer" \
        -artifact_prefix="$artifacts_dir/$target-" \
        -runs="$runs" "$corpus" 2>&1 | tee "$log_file"
    printf '%s\tpass\t%s\t%s\n' "$target" "$runs" "$corpus" >> "$summary_tsv"
  done
fi

echo "CFL smoke summary: $summary_tsv"
cat "$summary_tsv"
