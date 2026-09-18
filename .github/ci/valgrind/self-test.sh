#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Exercise Valgrind build isolation and runner failure classification.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
source_dir="${ICCDEV_VALGRIND_SOURCE_DIR:-$repo_root}"
valgrind_build_dir="${ICCDEV_VALGRIND_BUILD_DIR:-$repo_root/out/valgrind}"
main_build_dir="${ICCDEV_BUILD_DIR:-}"
sanitized_binary=""

usage()
{
  echo "Usage: $0 [--sanitized-binary FILE]"
  echo "Requires the registered dump target in the Valgrind build directory."
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --sanitized-binary)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      sanitized_binary="$2"
      shift 2
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

# shellcheck source=.github/ci/valgrind/common.sh
source "$script_dir/common.sh"

dump_binary="$valgrind_build_dir/Tools/IccDumpProfile/iccDumpProfile"
[ -x "$dump_binary" ] || {
  echo "ERROR: build the dump target before running the self-test" >&2
  exit 2
}

work_dir="$(mktemp -d)"
trap 'rm -rf -- "$work_dir"' EXIT

cache_before=""
if [ -n "$main_build_dir" ] && [ -f "$main_build_dir/CMakeCache.txt" ]; then
  cache_before="$(sha256sum "$main_build_dir/CMakeCache.txt")"
fi

unsafe_paths=("$source_dir" "$source_dir/Build")
if [ -n "$main_build_dir" ]; then
  unsafe_paths+=("$main_build_dir")
fi

for unsafe_path in "${unsafe_paths[@]}"; do
  set +e
  output="$("$script_dir/build.sh" --build-dir "$unsafe_path" --target dump 2>&1)"
  rc=$?
  set -e
  [ "$rc" -eq 2 ] || {
    echo "ERROR: unsafe build path returned rc=$rc: $unsafe_path" >&2
    exit 1
  }
  case "$output" in
    *"refusing unsafe Valgrind build directory"*|*"refusing configured main build directory"*) ;;
    *) echo "ERROR: missing isolation diagnostic for $unsafe_path" >&2; exit 1 ;;
  esac
done

if [ -n "$cache_before" ]; then
  cache_after="$(sha256sum "$main_build_dir/CMakeCache.txt")"
  [ "$cache_before" = "$cache_after" ] || {
    echo "ERROR: main build cache changed during isolation probes" >&2
    exit 1
  }
  if vg_assert_unsanitized_cache "$main_build_dir/CMakeCache.txt" >"$work_dir/cache.out" 2>&1; then
    echo "ERROR: sanitizer-instrumented main cache was accepted" >&2
    exit 1
  fi
fi

if [ -n "$sanitized_binary" ]; then
  if vg_assert_unsanitized_binary "$sanitized_binary" >"$work_dir/binary.out" 2>&1; then
    echo "ERROR: sanitizer-instrumented binary was accepted" >&2
    exit 1
  fi
fi

run_classification_probe()
{
  local fake_valgrind="$1"
  local label="$2"
  local fake_bin="$work_dir/$label-bin"
  local evidence="$work_dir/$label-evidence"
  local rc=0

  mkdir "$fake_bin"
  ln -s "$fake_valgrind" "$fake_bin/valgrind"
  set +e
  PATH="$fake_bin:$PATH" "$script_dir/run.sh" --allow-findings \
    --timeout 10 --output-dir "$evidence" dump >"$work_dir/$label.out" 2>&1
  rc=$?
  set -e

  [ "$rc" -eq 1 ] || {
    echo "ERROR: $label classification returned rc=$rc" >&2
    exit 1
  }
  awk -F '\t' '
    $1 == "dump" && $2 == "memcheck" && $3 == "1" &&
      $4 == "0" && $5 == "target-error" { found=1 }
    END { exit(found ? 0 : 1) }
  ' "$evidence/summary.tsv" || {
    echo "ERROR: $label did not record target-error" >&2
    exit 1
  }
}

run_classification_probe "$script_dir/fixtures/valgrind-target-error" with-log
run_classification_probe /bin/false without-log

echo "[PASS] Valgrind isolation and runner classification self-test"
