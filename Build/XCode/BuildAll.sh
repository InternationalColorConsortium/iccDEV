#!/bin/sh
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
set -eu

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
  printf '%s\n' \
    'Usage: BuildAll.sh [CMake configure options...]' \
    'Builds macOS with the macos-xcode preset; no files are copied into Testing/.' \
    'ICCDEV_XCODE_BUILD_DIR: build directory (default: out/macos-xcode, relative to repo)' \
    'ICCDEV_XCODE_CONFIG: Debug, Release (default), RelWithDebInfo, or MinSizeRel' \
    'CMAKE_BUILD_PARALLEL_LEVEL: positive build job count (default: available CPUs)' \
    'For iOS device tests, see docs/build.md and Build/AppleMobile.'
  exit 0
fi

script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH='' cd -- "$script_dir/../.." && pwd)
build_dir=${ICCDEV_XCODE_BUILD_DIR:-out/macos-xcode}
config=${ICCDEV_XCODE_CONFIG:-Release}
case "$build_dir" in
  /*) ;;
  *) build_dir="$repo_root/$build_dir" ;;
esac
case "$config" in
  Debug|Release|RelWithDebInfo|MinSizeRel) ;;
  *) printf 'Unsupported ICCDEV_XCODE_CONFIG: %s\n' "$config" >&2; exit 2 ;;
esac

jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu)}
case "$jobs" in
  ''|*[!0-9]*) printf 'Build job count must be a positive integer\n' >&2; exit 2 ;;
esac
if [ "$jobs" -eq 0 ]; then
  printf 'Build job count must be a positive integer\n' >&2
  exit 2
fi

cmake --preset macos-xcode -S "$repo_root/Build/Cmake" -B "$build_dir" "$@"
cmake --build "$build_dir" --config "$config" --parallel "$jobs"
