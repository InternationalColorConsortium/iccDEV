#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Build and optionally smoke-test the core CFL fuzzers.
###############################################################################

set -euo pipefail

usage() {
  echo "Usage: $0 [--targets CSV] [--seconds N] [--build-dir DIR] [--work-dir DIR] [--patches [DIR]] [--skip-run]"
  echo "Targets: dump, toxml, fromxml, tojson, fromjson, roundtrip, profilevisualize, writerserialize"
}

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_dir="${ICCDEV_CFL_BUILD_DIR:-$repo_root/build-cfl-smoke}"
work_dir="${ICCDEV_CFL_WORK_DIR:-$repo_root/.cfl-smoke}"
targets_csv="${ICCDEV_CFL_TARGETS:-dump,toxml,fromxml,tojson,fromjson,roundtrip,profilevisualize,writerserialize}"
seconds="${ICCDEV_CFL_SECONDS:-30}"
# Raised from 49152 to admit the whole committed seed corpus (#2120). The old
# default sat below exactly one file in .github/ci/test-data --
# fuzz-clut-hbo-69197729.icc at 61,015 bytes -- which is also the ONLY seed in
# that corpus carrying a CLUT, i.e. the only one that reaches RenderRaster and
# therefore the writers. Measured per seed: 1 of 11 reaches WriteTIFF, and it
# was the one being pruned.
#
# IMPLICATION, deliberate: the icc seed corpus is shared, so this widens it for
# ALL targets, not just the two in-process ones. The six fork/exec CLI targets
# now also replay a 61 KB profile -- slower per execution, more coverage per
# execution. Lower it per-run with ICCDEV_CFL_MAX_SEED_BYTES if that trade is
# wrong for a given lane; dropping a committed seed is then a hard error rather
# than a silent rm, so the choice stays visible (see prune_large_seeds).
max_seed_bytes="${ICCDEV_CFL_MAX_SEED_BYTES:-262144}"
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
    --seconds|--duration-seconds)
      [ "$#" -ge 2 ] || { echo "ERROR: $1 requires a value" >&2; exit 2; }
      seconds="$2"
      shift 2
      ;;
    --runs)
      echo "ERROR: --runs used an execution-count unit and is no longer accepted; use --seconds" >&2
      exit 2
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

case "$max_seed_bytes" in
  ''|*[!0-9]*)
    echo "ERROR: ICCDEV_CFL_MAX_SEED_BYTES must be numeric" >&2
    exit 2
    ;;
esac

if [ "$max_seed_bytes" -lt 1024 ]; then
  echo "ERROR: ICCDEV_CFL_MAX_SEED_BYTES must be at least 1024" >&2
  exit 2
fi

IFS=',' read -r -a requested_targets <<< "$targets_csv"
selected_targets=()
for target in "${requested_targets[@]}"; do
  target="${target#"${target%%[![:space:]]*}"}"
  target="${target%"${target##*[![:space:]]}"}"
  if [ -z "$target" ]; then
    echo "ERROR: empty CFL target entry in --targets" >&2
    exit 2
  fi
  if [[ "$target" =~ [[:space:]] ]]; then
    echo "ERROR: malformed CFL target contains whitespace: $target" >&2
    exit 2
  fi
  case "$target" in
    dump|toxml|fromxml|tojson|fromjson|roundtrip|profilevisualize|writerserialize)
      ;;
    *)
      echo "ERROR: unsupported CFL target: $target" >&2
      exit 2
      ;;
  esac
  for selected_target in "${selected_targets[@]}"; do
    if [ "$target" = "$selected_target" ]; then
      echo "ERROR: duplicate CFL target: $target" >&2
      exit 2
    fi
  done
  selected_targets+=("$target")
done
cc="${CC:-clang}"
cxx="${CXX:-clang++}"

for tool in cmake "$cc" "$cxx"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required tool not found: $tool" >&2
    exit 127
  fi
done

if [ "$apply_patches" != "0" ]; then
  patch_applicator="${ICCDEV_FUZZ_PATCH_APPLICATOR:-$repo_root/.github/scripts/iccdev-apply-fuzz-patches.sh}"
  "$patch_applicator" --mode cfl --patch-dir "$patch_dir"
fi

cmake -S "$repo_root/Build/Cmake" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_CXX_COMPILER="$cxx" \
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
  # The library-linking targets are built by the loop below instead: they link
  # iccDEV objects directly rather than fork/exec'ing a built CLI, so they do
  # not come from icc_cli_fuzzer.cpp.
  case "$target" in
    profilevisualize|writerserialize) continue ;;
  esac
  "$cxx" -std=c++17 -g -O1 -fno-omit-frame-pointer \
    -fsanitize=fuzzer,address,undefined \
    -DICCDEV_CFL_TARGET="\"$target\"" \
    "$script_dir/icc_cli_fuzzer.cpp" \
    -o "$bin_dir/icc_${target}_fuzzer"
done

# Library-linking harnesses. Each one links iccDEV translation units directly,
# so unlike the icc_cli_fuzzer.cpp targets above it needs its own source list.
# Kept as a single loop with a per-target list rather than one copied block per
# target: the same hand-maintained-list drift that #2034 fixed in the sanitizer
# silence files applies here the moment a third target arrives.
for target in "${selected_targets[@]}"; do
  case "$target" in
    profilevisualize|writerserialize) ;;
    *) continue ;;
  esac

  profile_lib="$build_dir/IccProfLib/libIccProfLib2-staticd.a"
  if [ ! -r "$profile_lib" ]; then
    profile_lib="$build_dir/IccProfLib/libIccProfLib2-static.a"
  fi
  if [ ! -r "$profile_lib" ]; then
    echo "ERROR: static IccProfLib library not found under $build_dir/IccProfLib" >&2
    exit 1
  fi

  # Translation units this target links beside its own harness source.
  # writerserialize adds the three writers because the serialization seam it
  # drives (#2116) lives in Mini{PDF,SVG,TIFF}.cpp, which IccVizModel does not
  # pull in -- that separation is exactly why the writers were unreachable from
  # the data-API harness in the first place.
  #
  # Pinned to the IccProfilePlot copy by absolute path. There is a second,
  # diverged copy of all three writers under Tools/CmdLine/IccProfileVisualize/
  # and it has no Serialize declaration, so linking the wrong one would fail to
  # compile rather than silently fuzz the other tool.
  local_viz_dir="$repo_root/Tools/CmdLine/IccProfilePlot"
  case "$target" in
    profilevisualize)
      extra_sources=( "$local_viz_dir/IccVizModel.cpp" )
      ;;
    writerserialize)
      extra_sources=(
        "$local_viz_dir/IccVizModel.cpp"
        "$local_viz_dir/MiniPDF.cpp"
        "$local_viz_dir/MiniSVG.cpp"
        "$local_viz_dir/MiniTIFF.cpp"
      )
      ;;
  esac

  object_dir="$work_dir/objects/$target"
  mkdir -p "$object_dir"
  common_flags=(
    "-std=c++17" "-g" "-O1" "-fno-omit-frame-pointer"
    "-Wall" "-Wextra" "-Werror"
    "-fsanitize=fuzzer-no-link,address,undefined"
    "-I$repo_root/IccProfLib"
    "-I$local_viz_dir"
  )

  objects=()
  for source in "${extra_sources[@]}"; do
    object="$object_dir/$(basename "${source%.cpp}").o"
    "$cxx" "${common_flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
  done

  "$cxx" "${common_flags[@]}" \
    -c "$script_dir/icc_${target}_fuzzer.cpp" \
    -o "$object_dir/icc_${target}_fuzzer.o"
  "$cxx" -g -O1 -fno-omit-frame-pointer \
    -fsanitize=fuzzer,address,undefined \
    "$object_dir/icc_${target}_fuzzer.o" \
    "${objects[@]}" \
    -Wl,--whole-archive "$profile_lib" -Wl,--no-whole-archive \
    -lz -o "$bin_dir/icc_${target}_fuzzer"
  cp "$script_dir/icc_${target}_fuzzer.options" \
    "$bin_dir/icc_${target}_fuzzer.options"
done

seed_root="$work_dir/seeds"
seed_inventory_tsv="$work_dir/seed-inventory.tsv"
skipped_seeds_tsv="$work_dir/skipped-seeds.tsv"
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

printf 'kind\tsize\tfile\n' > "$seed_inventory_tsv"
printf 'kind\tsize\tfile\treason\n' > "$skipped_seeds_tsv"

prune_large_seeds() {
  local kind="$1"
  local dir="$2"
  local file=""
  local size=""

  [ -d "$dir" ] || return 0
  while IFS= read -r -d '' file; do
    size="$(stat -c '%s' "$file")"
    printf '%s\t%s\t%s\n' "$kind" "$size" "$file" >> "$seed_inventory_tsv"
    if [ "$size" -gt "$max_seed_bytes" ]; then
      # A seed committed under .github/ci/test-data is a deliberate regression
      # artifact, not corpus bloat: dropping one silently removes whatever
      # defect class it was added to cover, and the run still reports coverage
      # and a clean exit. #2120 is exactly that -- the corpus's only CLUT seed
      # was being pruned, taking the entire writer path with it, for as long as
      # the cap sat below it.
      #
      # So committed seeds are never pruned, they are a hard error. Adding an
      # oversized regression seed now forces an explicit decision about the cap
      # instead of quietly costing coverage.
      if [ -f "$repo_root/.github/ci/test-data/$(basename "$file")" ]; then
        echo "ERROR: committed seed exceeds ICCDEV_CFL_MAX_SEED_BYTES and would be dropped:" >&2
        echo "       $(basename "$file") is $size bytes, cap is $max_seed_bytes" >&2
        echo "       Raise ICCDEV_CFL_MAX_SEED_BYTES, or remove the seed deliberately (#2120)." >&2
        return 1
      fi
      printf '%s\t%s\t%s\tlarger-than-%s-bytes\n' "$kind" "$size" "$file" "$max_seed_bytes" >> "$skipped_seeds_tsv"
      rm -f -- "$file"
    fi
  done < <(find "$dir" -maxdepth 1 -type f -print0 | sort -z)
}

prune_large_seeds "icc" "$seed_root/icc"
prune_large_seeds "xml" "$seed_root/xml"
prune_large_seeds "json" "$seed_root/json"

if [ "$(awk 'END { print NR }' "$skipped_seeds_tsv")" -gt 1 ]; then
  echo "Skipped CFL smoke seeds larger than $max_seed_bytes bytes:"
  tail -n +2 "$skipped_seeds_tsv"
fi

summary_tsv="$work_dir/summary.tsv"
logs_dir="$work_dir/logs"
artifacts_dir="$work_dir/artifacts"
mkdir -p "$logs_dir" "$artifacts_dir"
printf 'target\tstatus\tduration_seconds\tcorpus\n' > "$summary_tsv"

failures=0
if [ "$skip_run" -eq 0 ]; then
  for target in "${selected_targets[@]}"; do
    case "$target" in
      fromxml) corpus="$seed_root/xml" ;;
        fromjson) corpus="$seed_root/json" ;;
        *) corpus="$seed_root/icc" ;;
    esac
    log_file="$logs_dir/$target.log"
    set +e
    fuzzer_args=(
      -artifact_prefix="$artifacts_dir/$target-"
      -max_total_time="$seconds"
    )
    case "$target" in
      profilevisualize|writerserialize)
        # max_len is load-bearing for BOTH in-process targets, and it is a
        # second, independent gate from the seed cap above: the cap decides
        # whether the file reaches the corpus, max_len decides how much of it
        # libFuzzer feeds to the target.
        #
        # Only a CLUT-bearing profile reaches RenderRaster, and the corpus holds
        # exactly one -- fuzz-clut-hbo-69197729.icc, 61,015 bytes. Measured by
        # trapping inside the WriteTIFF FILE* overload and truncating that seed:
        #
        #   61,015 bytes -> writers reached
        #   49,152 bytes -> writers reached
        #    8,192 bytes -> DEAD, no raster rendered at all
        #
        # 8192 was the value this target originally carried, so its raster half
        # never executed while the session still reported runs, coverage and a
        # clean exit (#2120). 256 KiB admits the seed with headroom for larger
        # real CMYK profiles.
        fuzzer_args+=(
          -max_len=262144
          -timeout=30
          -rss_limit_mb=4096
          -use_value_profile=1
        )
        ;;
    esac
    ICCDEV_CFL_TOOL_DIR="$build_dir/Tools" \
      ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:allocator_may_return_null=1 \
      UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
      "$bin_dir/icc_${target}_fuzzer" \
        "${fuzzer_args[@]}" "$corpus" 2>&1 | tee "$log_file"
    fuzzer_status="${PIPESTATUS[0]}"
    set -e
    if [ "$fuzzer_status" -eq 0 ]; then
      printf '%s\tpass\t%s\t%s\n' "$target" "$seconds" "$corpus" >> "$summary_tsv"
    else
      printf '%s\tfail\t%s\t%s\n' "$target" "$seconds" "$corpus" >> "$summary_tsv"
      failures=$((failures + 1))
    fi
  done
fi

echo "CFL smoke summary: $summary_tsv"
cat "$summary_tsv"
if [ "$failures" -ne 0 ]; then
  exit 1
fi
