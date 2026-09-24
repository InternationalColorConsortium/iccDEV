#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# ClusterFuzzLite adapter for the in-process IccVizModel fuzz targets.
###############################################################################

set -euo pipefail

: "${SRC:?ClusterFuzzLite must provide SRC}"
: "${OUT:?ClusterFuzzLite must provide OUT}"
: "${WORK:?ClusterFuzzLite must provide WORK}"

repo_root="$SRC/iccDEV"
targets=( profilevisualize writerserialize )
msan_runtime_dir="$WORK/iccdev-msan-runtime"
issue_2687_base64="$repo_root/.github/ci/regression/issue-2687-profile-list-node.icc.base64"
issue_2687_profile="$WORK/issue-2687-profile-list-node.icc"

if [ "${SANITIZER:-}" = "memory" ]; then
  "$repo_root/.github/scripts/iccdev-build-msan-libcxx.sh" \
    --prefix "$msan_runtime_dir" \
    --skip-libxml2

  CXXFLAGS="${CXXFLAGS//-stdlib=libc++/}"
  export CXXFLAGS="$CXXFLAGS -nostdinc++ -isystem $msan_runtime_dir/include/c++/v1"
  export ICCDEV_CFL_CXX_LINK_FLAGS="-nostdlib++ -L$msan_runtime_dir/lib -Wl,-rpath,\$ORIGIN -lc++ -lc++abi"
fi

ICCDEV_CLUSTERFUZZLITE_BUILD=1 \
ICCDEV_CFL_BUILD_DIR="$WORK/iccdev-build" \
ICCDEV_CFL_WORK_DIR="$WORK/iccdev-cfl" \
  "$repo_root/.github/ci/cfl/build.sh" \
    --targets profilevisualize,writerserialize \
    --skip-run

for target in "${targets[@]}"; do
  fuzzer="icc_${target}_fuzzer"
  install -m 0755 "$repo_root/.github/ci/cfl/bin/$fuzzer" "$OUT/$fuzzer"
  install -m 0644 \
    "$repo_root/.github/ci/cfl/bin/$fuzzer.options" \
    "$OUT/$fuzzer.options"
  (
    cd "$repo_root/.github/ci/test-data"
    zip -q "$OUT/${fuzzer}_seed_corpus.zip" ./*.icc
  )
done

if [ "${SANITIZER:-}" = "memory" ]; then
  install -m 0755 "$msan_runtime_dir/lib/libc++.so.1.0" \
    "$OUT/libc++.so.1"
  install -m 0755 "$msan_runtime_dir/lib/libc++abi.so.1.0" \
    "$OUT/libc++abi.so.1"

  base64 --decode "$issue_2687_base64" > "$issue_2687_profile"
  echo "bb9c4ad53f9269920947bac185a634da2971a94b17da6cff117dd7ad45bcfe85  $issue_2687_profile" |
    sha256sum --check --status

  for target in "${targets[@]}"; do
    fuzzer="icc_${target}_fuzzer"
    ldd "$OUT/$fuzzer" > "$WORK/$fuzzer.ldd"
    if grep -Fq 'libstdc++.so' "$WORK/$fuzzer.ldd" ||
       ! grep -Fq "$OUT/libc++.so.1" "$WORK/$fuzzer.ldd" ||
       ! grep -Fq "$OUT/libc++abi.so.1" "$WORK/$fuzzer.ldd"; then
      echo "ERROR: $fuzzer crossed an uninstrumented C++ runtime boundary" >&2
      cat "$WORK/$fuzzer.ldd" >&2
      exit 1
    fi

    MSAN_OPTIONS=halt_on_error=1:exit_code=86:origin_history_size=7 \
      "$OUT/$fuzzer" -runs=1 "$issue_2687_profile"
  done
fi
