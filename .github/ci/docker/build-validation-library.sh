#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.
# Build only the public validation ABI for an unsanitized Python process.
set -euo pipefail

source_dir="${1:?source directory required}"
library_dir="${2:?destination library directory required}"
jobs="${3:-32}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo "Invalid job count" >&2; exit 2; }
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT

# Never inherit CLI sanitizer, coverage, or linker flags into Python.
env -u CFLAGS -u CXXFLAGS -u CPPFLAGS -u LDFLAGS \
  cmake -S "$source_dir/Build/Cmake" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    -DENABLE_TOOLS=OFF -DENABLE_TESTS=OFF -DENABLE_WXWIDGETS=OFF \
    -DENABLE_SHARED_LIBS=ON -DENABLE_STATIC_LIBS=OFF \
    -DENABLE_SANITIZERS=OFF -DENABLE_COVERAGE=OFF -DENABLE_PROFILING=OFF
cmake --build "$build_dir" --target IccProfLib2 --parallel "$jobs"
install -d "$library_dir"
install -m 0755 "$build_dir/IccProfLib/libIccProfLib2.so" "$library_dir/libIccProfLib2.so"
