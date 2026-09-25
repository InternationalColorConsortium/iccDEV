#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Validate the source, options, dictionaries, and seed families for every
# in-process ClusterFuzzLite target without requiring a sanitizer toolchain.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
cfl_dir="$repo_root/.github/ci/cfl"
adapter="$repo_root/.clusterfuzzlite/build.sh"
builder="$cfl_dir/build.sh"

targets=(
  profileparse cmmapply profilevisualize writerserialize
  xmlparse jsonparse connectconfig pawgreport
)

for target in "${targets[@]}"; do
  source_file="$cfl_dir/icc_${target}_fuzzer.cpp"
  if [ ! -s "$source_file" ]; then
    echo "[FAIL] Missing in-process target source: $source_file" >&2
    exit 1
  fi
  grep -Fq 'LLVMFuzzerTestOneInput' "$source_file"
  grep -Fq 'Redistribution and use in source and binary forms' "$source_file"
  grep -Fq 'THIS SOFTWARE IS PROVIDED ``AS IS' "$source_file"
  grep -Fq "$target" "$adapter"
  grep -Fq "$target" "$builder"
done

for family in profile text; do
  options="$cfl_dir/icc_${family}_fuzzer.options"
  grep -qx 'max_len = 1048576' "$options"
  grep -qx 'timeout = 30' "$options"
  grep -qx 'rss_limit_mb = 4096' "$options"
  grep -qx 'use_value_profile = 1' "$options"
done

for family in profile xml json config; do
  dictionary="$cfl_dir/icc_${family}_fuzzer.dict"
  if [ ! -s "$dictionary" ]; then
    echo "[FAIL] Missing dictionary: $dictionary" >&2
    exit 1
  fi
done

config_seed="$repo_root/.github/ci/test-data/connect-config-complete.json"
if [ ! -s "$config_seed" ]; then
  echo "[FAIL] Missing schema-shaped IccConnect configuration seed" >&2
  exit 1
fi
for key in dataFiles imageFiles connect createLink profileSequence \
    searchApply pccWeights colorData; do
  grep -Fq "\"$key\"" "$config_seed"
  grep -Fq "\\\"$key\\\"" "$cfl_dir/icc_config_fuzzer.dict"
done

cmmapply="$cfl_dir/icc_cmmapply_fuzzer.cpp"
grep -Fq 'readBigEndian32(data + 64)' "$cmmapply"
grep -Fq 'const uint8_t *control = data + profile_size' "$cmmapply"
grep -Fq 'size - profile_size >= 3' "$cmmapply"
if grep -Fq 'const bool first_is_input = (data[0]' "$cmmapply" ||
   grep -Fq 'static_cast<icRenderingIntent>(data[1]' "$cmmapply"; then
  echo "[FAIL] CMM controls must not alias the ICC profile-size field" >&2
  exit 1
fi

connectconfig="$cfl_dir/icc_connectconfig_fuzzer.cpp"
for config_type in CIccCfgDataApply CIccCfgImageApply CIccCfgConnectOptions \
    CIccCfgCreateLink CIccCfgProfile CIccCfgProfileSequence CIccCfgPccWeight \
    CIccCfgSearchApply CIccCfgDataEntry CIccCfgColorData; do
  grep -Fq "exerciseRoundTrip<$config_type>" "$connectconfig"
done

for extension in icc xml json; do
  if ! find "$repo_root/.github/ci/test-data" -maxdepth 1 \
      -type f -name "*.$extension" -print -quit | grep -q .; then
    echo "[FAIL] No tracked $extension seed for ClusterFuzzLite" >&2
    exit 1
  fi
done

echo "[PASS] ClusterFuzzLite target contract"
