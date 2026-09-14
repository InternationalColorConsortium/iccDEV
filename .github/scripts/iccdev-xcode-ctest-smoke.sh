#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Dependency-free Xcode coverage of native tests, direct CLI paths, and
# find -type f discovery in the configuration-specific shell runtime view.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"
build="out/xcode-ctest-smoke"
cmake --preset macos-xcode -S Build/Cmake -B "$build" \
  -DENABLE_TOOLS=ON -DENABLE_TESTS=ON \
  -DENABLE_SHARED_LIBS=ON -DENABLE_STATIC_LIBS=ON \
  -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=OFF -DICC_USE_ZLIB=OFF \
  -DENABLE_IMAGE_TOOLS=OFF -DENABLE_WXWIDGETS=OFF -DENABLE_CMM_TOOLS=OFF
for config in Release Debug; do
  cmake --build "$build" --config "$config" --parallel "$(sysctl -n hw.ncpu)" \
    --target iccFromCube iccDumpProfile iccProfileWriteFailureTest
  rm -f "$build/Testing/ctest-output/$config/iccdev-fromcube-cli-args/domain.icc"
  ctest --test-dir "$build" -C "$config" \
    -R '^iccdev\.(unix-runtime-layout|profile-write-failure|fromcube-cli-args)$' \
    --parallel "$(sysctl -n hw.ncpu)" \
    --output-on-failure --no-tests=error
  # A legacy discovery failure is reported as SKIP by this script, so require
  # the successful operation's output as well as CTest's process status.
  test -s "$build/Testing/ctest-output/$config/iccdev-fromcube-cli-args/domain.icc"
done
