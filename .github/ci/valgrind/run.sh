#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Run registered targets under Memcheck, Helgrind, DRD, Massif, or Callgrind.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
VG_SOURCE_DIR="${ICCDEV_VALGRIND_SOURCE_DIR:-$repo_root}"
VG_BUILD_DIR="${ICCDEV_VALGRIND_BUILD_DIR:-$repo_root/out/valgrind}"
output_base="${ICCDEV_VALGRIND_OUTPUT_DIR:-$repo_root/out/valgrind-evidence}"
output_explicit=0
analysis_tool="memcheck"
target_timeout=120
allow_findings=0
selected=()

# shellcheck source=.github/ci/valgrind/targets.sh
source "$script_dir/targets.sh"
# shellcheck source=.github/ci/valgrind/common.sh
source "$script_dir/common.sh"

usage()
{
  echo "Usage: $0 [options] TARGET [TARGET ...]"
  echo "       $0 [options] all"
  echo "Options:"
  echo "  --tool memcheck|helgrind|drd|massif|callgrind"
  echo "  --timeout N          per-target timeout in seconds (default: 120)"
  echo "  --output-dir DIR     new evidence directory"
  echo "  --allow-findings     return success after recording findings"
  echo "  --list               list targets"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --tool) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; analysis_tool="$2"; shift 2 ;;
    --timeout) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; target_timeout="$2"; shift 2 ;;
    --output-dir) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; output_base="$2"; output_explicit=1; shift 2 ;;
    --allow-findings) allow_findings=1; shift ;;
    --list) vg_print_targets; exit 0 ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    *) selected+=("$1"); shift ;;
  esac
done

case "$analysis_tool" in
  memcheck|helgrind|drd|massif|callgrind) ;;
  *) echo "ERROR: unsupported Valgrind tool: $analysis_tool" >&2; exit 2 ;;
esac
case "$target_timeout" in
  ''|0*|*[!0-9]*) echo "ERROR: --timeout must be a positive integer" >&2; exit 2 ;;
esac
[ "${#selected[@]}" -gt 0 ] || { usage >&2; exit 2; }
command -v valgrind >/dev/null 2>&1 || { echo "ERROR: valgrind is not installed" >&2; exit 127; }
command -v timeout >/dev/null 2>&1 || { echo "ERROR: timeout is not installed" >&2; exit 127; }
vg_assert_unsanitized_cache "$VG_BUILD_DIR/CMakeCache.txt"

if [ "${selected[0]}" = "all" ]; then
  [ "${#selected[@]}" -eq 1 ] || { echo "ERROR: all cannot be combined with named targets" >&2; exit 2; }
  selected=("${VG_TARGETS[@]}")
fi

run_id="$(date -u +%Y%m%dT%H%M%SZ)-$analysis_tool-$$"
if [ "$output_explicit" -eq 0 ]; then
  output_base="$output_base/$run_id"
fi
if [ -e "$output_base" ]; then
  echo "ERROR: evidence directory already exists: $output_base" >&2
  exit 2
fi
mkdir -p "$output_base"
output_base="$(cd "$output_base" && pwd)"
summary="$output_base/summary.tsv"
printf 'target\ttool\trc\terrors\tresult\tlog\n' > "$summary"

runtime_library_path="$VG_BUILD_DIR/IccProfLib:$VG_BUILD_DIR/IccXML"
runtime_library_path+=":$VG_BUILD_DIR/IccJSON:$VG_BUILD_DIR/IccConnect"
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  runtime_library_path="$runtime_library_path:$LD_LIBRARY_PATH"
fi

failures=0
for target in "${selected[@]}"; do
  VG_RUN_WORK="$output_base/$target/work"
  if ! vg_configure_target "$target"; then
    echo "ERROR: unknown target: $target" >&2
    vg_print_targets >&2
    exit 2
  fi
  mkdir -p "$VG_RUN_WORK"
  target_dir="$output_base/$target"
  valgrind_log="$target_dir/$analysis_tool.log"
  stdout_log="$target_dir/stdout.log"
  stderr_log="$target_dir/stderr.log"

  [ -x "$VG_BINARY" ] || { echo "ERROR: missing binary for $target: $VG_BINARY" >&2; exit 2; }
  for required in "${VG_REQUIRED_FILES[@]}"; do
    [ -e "$required" ] || { echo "ERROR: missing input for $target: $required" >&2; exit 2; }
  done
  vg_assert_unsanitized_binary "$VG_BINARY"

  if [ "${#VG_PREPARE[@]}" -gt 0 ]; then
    LD_LIBRARY_PATH="$runtime_library_path" "${VG_PREPARE[@]}" >/dev/null
  fi

  common_args=(--error-exitcode=86 --num-callers=40 "--log-file=$valgrind_log")
  case "$analysis_tool" in
    memcheck)
      tool_args=(--tool=memcheck --leak-check=full --show-leak-kinds=all
        "--errors-for-leak-kinds=definite,indirect,possible" --track-origins=yes --fair-sched=yes)
      ;;
    helgrind)
      tool_args=(--tool=helgrind --history-level=full --fair-sched=yes)
      ;;
    drd)
      tool_args=(--tool=drd --check-stack-var=yes)
      ;;
    massif)
      common_args=(--tool=massif "--massif-out-file=$target_dir/massif.out" "--log-file=$valgrind_log")
      tool_args=(--stacks=yes)
      ;;
    callgrind)
      common_args=(--tool=callgrind "--callgrind-out-file=$target_dir/callgrind.out" "--log-file=$valgrind_log")
      tool_args=(--collect-jumps=yes)
      ;;
  esac

  echo "[RUN] $analysis_tool $target: $VG_NOTE"
  if [ "$VG_RECOMMENDED_TOOL" != "$analysis_tool" ]; then
    echo "[WARN] $target is usually most useful with $VG_RECOMMENDED_TOOL"
  fi
  set +e
  DEBUGINFOD_URLS='' LD_LIBRARY_PATH="$runtime_library_path" \
    timeout -k 5s "${target_timeout}s" valgrind \
    "${tool_args[@]}" "${common_args[@]}" "$VG_BINARY" "${VG_ARGS[@]}" \
    >"$stdout_log" 2>"$stderr_log"
  status=$?
  set -e

  errors=""
  if [ -f "$valgrind_log" ]; then
    errors="$(sed -n 's/.*ERROR SUMMARY: \([0-9][0-9]*\) errors.*/\1/p' "$valgrind_log" | tail -1)"
  fi
  errors="${errors:-0}"
  result="clean"
  if [ "$status" -eq 124 ] || [ "$status" -eq 137 ]; then
    result="timeout"
    failures=$((failures + 1))
  elif [ "$status" -eq 86 ] || { [ "$status" -eq 0 ] && [ "$errors" -ne 0 ]; }; then
    result="finding"
    [ "$allow_findings" -eq 1 ] || failures=$((failures + 1))
  elif [ "$status" -ne 0 ]; then
    result="target-error"
    failures=$((failures + 1))
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$target" "$analysis_tool" "$status" "$errors" "$result" "$valgrind_log" >> "$summary"
  if [ "$result" = "clean" ]; then
    echo "[PASS] $target: clean (rc=$status errors=$errors)"
  else
    echo "[WARN] $target: $result (rc=$status errors=$errors)"
  fi
done

echo "Evidence: $output_base"
[ "$failures" -eq 0 ]
