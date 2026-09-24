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
