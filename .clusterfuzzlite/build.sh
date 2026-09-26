#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# ClusterFuzzLite adapter for iccDEV's in-process library fuzz targets.
###############################################################################

set -euo pipefail

: "${SRC:?ClusterFuzzLite must provide SRC}"
: "${OUT:?ClusterFuzzLite must provide OUT}"
: "${WORK:?ClusterFuzzLite must provide WORK}"

repo_root="$SRC/iccDEV"
source_sha="${ICCDEV_CFL_SOURCE_SHA:-${CFL_EXTRA_ICCDEV_CFL_SOURCE_SHA:-}}"
case "$source_sha" in
  ''|*[!0-9a-f]*)
    echo "ERROR: ICCDEV_CFL_SOURCE_SHA must be a lowercase Git commit SHA" >&2
    exit 2
    ;;
esac
if [ "${#source_sha}" -ne 40 ]; then
  echo "ERROR: ICCDEV_CFL_SOURCE_SHA must contain exactly 40 characters" >&2
  exit 2
fi
target_group_file="$repo_root/.clusterfuzzlite/target-group"
target_group="${ICCDEV_CFL_TARGET_GROUP:-${CFL_EXTRA_ICCDEV_CFL_TARGET_GROUP:-}}"
if [ -z "$target_group" ]; then
  target_group="$(<"$target_group_file")"
fi
case "$target_group" in
  all)
    targets=(
      profileparse cmmapply profilevisualize writerserialize
      xmlparse jsonparse connectconfig pawgreport
    )
    ;;
  core)
    targets=( profileparse cmmapply profilevisualize writerserialize )
    ;;
  formats)
    targets=( xmlparse jsonparse connectconfig )
    ;;
  assessment)
    targets=( pawgreport )
    ;;
  *)
    echo "ERROR: target group must be all, core, formats, or assessment" >&2
    exit 2
    ;;
esac
targets_csv="$(IFS=,; echo "${targets[*]}")"
msan_runtime_dir="$WORK/iccdev-msan-runtime"
issue_2687_base64="$repo_root/.github/ci/regression/issue-2687-profile-list-node.icc.base64"
issue_2687_profile="$WORK/issue-2687-profile-list-node.icc"
patch_mode_file="$repo_root/.clusterfuzzlite/known-bug-patch-mode"
patch_mode="${ICCDEV_CFL_KNOWN_BUG_PATCH_MODE:-${CFL_EXTRA_ICCDEV_CFL_KNOWN_BUG_PATCH_MODE:-}}"

if [ -z "$patch_mode" ]; then
  patch_mode="$(<"$patch_mode_file")"
fi

case "$patch_mode" in
  patched)
    patch_args=(
      --patches
      "$repo_root/.github/ci/fuzz-patches/cfl"
    )
    ;;
  unpatched)
    patch_args=()
    ;;
  *)
    echo "ERROR: known bug patch mode must be patched or unpatched" >&2
    exit 2
    ;;
esac

if [ "${SANITIZER:-}" = "memory" ]; then
  msan_runtime_args=( --prefix "$msan_runtime_dir" )
  needs_libxml2=0
  for target in "${targets[@]}"; do
    if [ "$target" = "xmlparse" ]; then
      needs_libxml2=1
    fi
  done
  if [ "$needs_libxml2" -eq 0 ]; then
    msan_runtime_args+=( --skip-libxml2 )
  fi
  "$repo_root/.github/scripts/iccdev-build-msan-libcxx.sh" \
    "${msan_runtime_args[@]}"

  CXXFLAGS="${CXXFLAGS//-stdlib=libc++/}"
  export CXXFLAGS="$CXXFLAGS -nostdinc++ -isystem $msan_runtime_dir/include/c++/v1"
  export ICCDEV_CFL_CXX_LINK_FLAGS="-nostdlib++ -L$msan_runtime_dir/lib -Wl,-rpath,\$ORIGIN -lc++ -lc++abi"
  if [ "$needs_libxml2" -eq 1 ]; then
    export ICCDEV_CFL_LIBXML2_PREFIX="$msan_runtime_dir"
    export PKG_CONFIG_PATH="$msan_runtime_dir/lib/pkgconfig"
  fi
fi

ICCDEV_CLUSTERFUZZLITE_BUILD=1 \
ICCDEV_CFL_BUILD_DIR="$WORK/iccdev-build" \
ICCDEV_CFL_WORK_DIR="$WORK/iccdev-cfl" \
  "$repo_root/.github/ci/cfl/build.sh" \
    --targets "$targets_csv" \
    "${patch_args[@]}" \
    --skip-run

for target in "${targets[@]}"; do
  fuzzer="icc_${target}_fuzzer"
  install -m 0755 "$repo_root/.github/ci/cfl/bin/$fuzzer" "$OUT/$fuzzer"
  install -m 0644 \
    "$repo_root/.github/ci/cfl/bin/$fuzzer.options" \
    "$OUT/$fuzzer.options"
  install -m 0644 \
    "$repo_root/.github/ci/cfl/bin/$fuzzer.dict" \
    "$OUT/$fuzzer.dict"
  case "$target" in
    xmlparse) corpus_glob='./*.xml' ;;
    jsonparse|connectconfig) corpus_glob='./*.json' ;;
    *) corpus_glob='./*.icc' ;;
  esac
  (
    cd "$repo_root/.github/ci/test-data"
    # shellcheck disable=SC2086 # The validated extension glob must expand.
    zip -q "$OUT/${fuzzer}_seed_corpus.zip" $corpus_glob
  )
done

if [ "${SANITIZER:-}" = "memory" ]; then
  install -m 0755 "$msan_runtime_dir/lib/libc++.so.1.0" \
    "$OUT/libc++.so.1"
  install -m 0755 "$msan_runtime_dir/lib/libc++abi.so.1.0" \
    "$OUT/libc++abi.so.1"

  libxml2_soname=""
  if [ "$needs_libxml2" -eq 1 ]; then
    libxml2_soname="$(readelf -d "$msan_runtime_dir/lib/libxml2.so" |
      sed -n 's/.*SONAME.*\[\(.*\)\].*/\1/p')"
    if [ -z "$libxml2_soname" ]; then
      echo "ERROR: unable to resolve instrumented libxml2 SONAME" >&2
      exit 1
    fi
    install -m 0755 "$msan_runtime_dir/lib/libxml2.so" \
      "$OUT/$libxml2_soname"
  fi

  base64 --decode "$issue_2687_base64" > "$issue_2687_profile"
  echo "bb9c4ad53f9269920947bac185a634da2971a94b17da6cff117dd7ad45bcfe85  $issue_2687_profile" |
    sha256sum --check --status

  for target in "${targets[@]}"; do
    fuzzer="icc_${target}_fuzzer"
    ldd_report="$WORK/$fuzzer.ldd"
    ldd "$OUT/$fuzzer" > "$ldd_report"
    if grep -Fq 'libstdc++.so' "$ldd_report" ||
       ! grep -Fq "$OUT/libc++.so.1" "$ldd_report" ||
       ! grep -Fq "$OUT/libc++abi.so.1" "$ldd_report"; then
      echo "ERROR: $fuzzer crossed an uninstrumented C++ runtime boundary" >&2
      cat "$ldd_report" >&2
      exit 1
    fi
    if [ "$target" = "xmlparse" ] &&
       ! grep -Fq "$OUT/$libxml2_soname" "$ldd_report"; then
      echo "ERROR: $fuzzer crossed an uninstrumented libxml2 boundary" >&2
      cat "$ldd_report" >&2
      exit 1
    fi

    libcxx_path="$(awk '$1 == "libc++.so.1" { print $3; exit }' "$ldd_report")"
    libcxxabi_path="$(awk '$1 == "libc++abi.so.1" { print $3; exit }' "$ldd_report")"
    printf '[EVIDENCE] msan_runtime fuzzer=%s libcxx=%s libcxxabi=%s\n' \
      "$fuzzer" "$libcxx_path" "$libcxxabi_path"
    if [ "$target" = "xmlparse" ]; then
      libxml2_path="$(awk '$1 ~ /^libxml2\.so/ { print $3; exit }' "$ldd_report")"
      printf '[EVIDENCE] msan_xml_runtime fuzzer=%s libxml2=%s\n' \
        "$fuzzer" "$libxml2_path"
    fi

    MSAN_OPTIONS=halt_on_error=1:exit_code=86:origin_history_size=7 \
      "$OUT/$fuzzer" -runs=1 "$issue_2687_profile"
    printf '[PASS] MSan replay fuzzer=%s fixture=issue-2687-profile-list-node.icc\n' \
      "$fuzzer"
  done
fi

provenance_file="$OUT/iccdev-cfl-build-provenance.txt"
{
  printf 'source_sha=%s\n' "$source_sha"
  printf 'target_group=%s\n' "$target_group"
  printf 'patch_mode=%s\n' "$patch_mode"
} > "$provenance_file"
