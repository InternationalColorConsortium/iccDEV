#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Shared fail-closed checks for Valgrind build and run entry points.
###############################################################################

vg_assert_unsanitized_cache()
{
  local cache_file="$1"
  local sanitizer_flags=""

  [ -f "$cache_file" ] || {
    echo "ERROR: missing CMake cache: $cache_file" >&2
    return 1
  }
  sanitizer_flags="$(grep -E '^[^#/][^=]*=.*(-fsanitize=|/fsanitize=)' \
    "$cache_file" 2>/dev/null || true)"
  if [ -n "$sanitizer_flags" ]; then
    echo "ERROR: sanitizer compiler flags found in Valgrind CMake cache: $cache_file" >&2
    printf '%s\n' "$sanitizer_flags" >&2
    return 1
  fi
}

vg_assert_unsanitized_binary()
{
  local binary="$1"
  local dependencies=""
  local symbols=""
  local text=""

  [ -x "$binary" ] || {
    echo "ERROR: missing Valgrind executable: $binary" >&2
    return 1
  }
  if command -v ldd >/dev/null 2>&1; then
    dependencies="$(ldd "$binary" 2>/dev/null || true)"
    case "$dependencies" in
      *libasan*|*libubsan*|*libtsan*|*libmsan*|*liblsan*|*clang_rt.asan*|\
      *clang_rt.ubsan*|*clang_rt.tsan*|*clang_rt.msan*|*clang_rt.lsan*)
        echo "ERROR: sanitizer runtime dependency found in Valgrind binary: $binary" >&2
        return 1
        ;;
    esac
  fi

  if ! command -v nm >/dev/null 2>&1 && ! command -v strings >/dev/null 2>&1; then
    echo "ERROR: cannot verify Valgrind binary without nm or strings: $binary" >&2
    return 1
  fi
  if command -v nm >/dev/null 2>&1; then
    symbols="$({ nm -D "$binary" 2>/dev/null || true; nm "$binary" 2>/dev/null || true; })"
    case "$symbols" in
      *__asan_init*|*__ubsan_handle_*|*__tsan_init*|*__msan_init*|*__lsan_init*)
        echo "ERROR: sanitizer instrumentation found in Valgrind binary: $binary" >&2
        return 1
        ;;
    esac
  fi
  if command -v strings >/dev/null 2>&1; then
    text="$(strings "$binary" 2>/dev/null || true)"
    case "$text" in
      *libclang_rt.asan*|*libclang_rt.ubsan*|*libclang_rt.tsan*|\
      *libclang_rt.msan*|*libclang_rt.lsan*|*libasan.so*|*libubsan.so*|\
      *libtsan.so*|*libmsan.so*|*liblsan.so*)
        echo "ERROR: sanitizer runtime string found in Valgrind binary: $binary" >&2
        return 1
        ;;
    esac
  fi
}
