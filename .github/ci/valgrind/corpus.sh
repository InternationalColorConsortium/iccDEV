#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Run one iccDEV report tool over an ICC corpus under a Valgrind-family tool.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_dir="${ICCDEV_VALGRIND_BUILD_DIR:-$repo_root/out/valgrind}"
output_dir=""
analysis_tool="memcheck"
icc_tool="pawg"
target_timeout=120
allow_findings=0
corpus_dir=""

# shellcheck source=.github/ci/valgrind/common.sh
source "$script_dir/common.sh"

usage()
{
  echo "Usage: $0 [options] CORPUS_DIR"
  echo "Options:"
  echo "  --tool memcheck|helgrind|drd|massif|callgrind"
  echo "  --icc-tool pawg|dump"
  echo "  --build-dir DIR     non-sanitized Debug build"
  echo "  --output-dir DIR    new evidence directory"
  echo "  --timeout N         per-profile timeout in seconds (default: 120)"
  echo "  --allow-findings    return success after recording analyzer findings"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --tool) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; analysis_tool="$2"; shift 2 ;;
    --icc-tool) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; icc_tool="$2"; shift 2 ;;
    --build-dir) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; build_dir="$2"; shift 2 ;;
    --output-dir) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; output_dir="$2"; shift 2 ;;
    --timeout) [ "$#" -ge 2 ] || { usage >&2; exit 2; }; target_timeout="$2"; shift 2 ;;
    --allow-findings) allow_findings=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)
      [ -z "$corpus_dir" ] || { echo "ERROR: only one corpus directory is allowed" >&2; exit 2; }
      corpus_dir="$1"
      shift
      ;;
  esac
done

case "$analysis_tool" in
  memcheck|helgrind|drd|massif|callgrind) ;;
  *) echo "ERROR: unsupported Valgrind tool: $analysis_tool" >&2; exit 2 ;;
esac
case "$icc_tool" in
  pawg) binary="$build_dir/Tools/IccPawgReport/iccPawgReport" ;;
  dump) binary="$build_dir/Tools/IccDumpProfile/iccDumpProfile" ;;
  *) echo "ERROR: unsupported iccDEV tool: $icc_tool" >&2; exit 2 ;;
esac
case "$target_timeout" in
  ''|0*|*[!0-9]*) echo "ERROR: --timeout must be a positive integer" >&2; exit 2 ;;
esac
[ -n "$corpus_dir" ] && [ -d "$corpus_dir" ] || { usage >&2; exit 2; }
command -v valgrind >/dev/null 2>&1 || { echo "ERROR: valgrind is not installed" >&2; exit 127; }
command -v timeout >/dev/null 2>&1 || { echo "ERROR: timeout is not installed" >&2; exit 127; }
vg_assert_unsanitized_cache "$build_dir/CMakeCache.txt"
vg_assert_unsanitized_binary "$binary"

corpus_dir="$(cd "$corpus_dir" && pwd)"
if [ -z "$output_dir" ]; then
  output_dir="$repo_root/out/valgrind-corpus/$(date -u +%Y%m%dT%H%M%SZ)-$analysis_tool-$icc_tool-$$"
fi
[ ! -e "$output_dir" ] || { echo "ERROR: evidence directory already exists: $output_dir" >&2; exit 2; }
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
summary="$output_dir/summary.tsv"
printf 'profile\ticc_tool\tvalgrind_tool\trc\terrors\tresult\tlog\n' > "$summary"

runtime_library_path="$build_dir/IccProfLib:$build_dir/IccXML"
runtime_library_path+=":$build_dir/IccJSON:$build_dir/IccConnect"
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  runtime_library_path="$runtime_library_path:$LD_LIBRARY_PATH"
fi

count=0
failures=0
while IFS= read -r -d '' profile; do
  count=$((count + 1))
  case "$profile" in
    *$'\t'*|*$'\n'*) echo "ERROR: profile path contains a tab or newline: $profile" >&2; exit 2 ;;
  esac
  base="$(basename "$profile")"
  safe_base="$(printf '%s' "$base" | tr -c 'A-Za-z0-9._-' '_')"
  profile_dir="$output_dir/$(printf '%06d-%s' "$count" "$safe_base")"
  mkdir -p "$profile_dir"
  valgrind_log="$profile_dir/$analysis_tool.log"
  stdout_log="$profile_dir/stdout.log"
  stderr_log="$profile_dir/stderr.log"

  case "$icc_tool" in
    pawg) icc_args=("$profile") ;;
    dump) icc_args=(-v 100 "$profile" ALL) ;;
  esac
  common_args=(--error-exitcode=86 --num-callers=40 "--log-file=$valgrind_log")
  case "$analysis_tool" in
    memcheck)
      tool_args=(--tool=memcheck --leak-check=full --show-leak-kinds=all
        "--errors-for-leak-kinds=definite,indirect,possible" --track-origins=yes --fair-sched=yes)
      ;;
    helgrind) tool_args=(--tool=helgrind --history-level=full --fair-sched=yes) ;;
    drd) tool_args=(--tool=drd --check-stack-var=yes) ;;
    massif)
      common_args=(--tool=massif "--massif-out-file=$profile_dir/massif.out" "--log-file=$valgrind_log")
      tool_args=(--stacks=yes)
      ;;
    callgrind)
      common_args=(--tool=callgrind "--callgrind-out-file=$profile_dir/callgrind.out" "--log-file=$valgrind_log")
      tool_args=(--collect-jumps=yes)
      ;;
  esac

  echo "[RUN] $analysis_tool $icc_tool: $profile"
  set +e
  DEBUGINFOD_URLS='' LD_LIBRARY_PATH="$runtime_library_path" \
    timeout -k 5s "${target_timeout}s" valgrind \
    "${tool_args[@]}" "${common_args[@]}" "$binary" "${icc_args[@]}" \
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
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$profile" "$icc_tool" "$analysis_tool" "$status" "$errors" "$result" "$valgrind_log" >> "$summary"
  if [ "$result" = "clean" ]; then
    echo "[PASS] $profile: clean (rc=$status errors=$errors)"
  else
    echo "[WARN] $profile: $result (rc=$status errors=$errors)"
  fi
done < <(find "$corpus_dir" -type f -iname '*.icc' -print0 | sort -z)

[ "$count" -gt 0 ] || { echo "ERROR: no .icc files found under $corpus_dir" >&2; exit 2; }
echo "Evidence: $output_dir"
[ "$failures" -eq 0 ]
