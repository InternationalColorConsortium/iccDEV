#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Replay AFL/CFL findings with iccDEV command-line tools and summarize results.
###############################################################################

set -euo pipefail

usage() {
  echo "Usage: $0 --work-dir DIR --build-dir DIR [--findings FILE] [--timeout N]"
}

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
work_dir=""
build_dir=""
findings=""
timeout_seconds=20

while [ "$#" -gt 0 ]; do
  case "$1" in
    --work-dir)
      [ "$#" -ge 2 ] || { echo "ERROR: --work-dir requires a directory" >&2; exit 2; }
      work_dir="$2"
      shift 2
      ;;
    --build-dir)
      [ "$#" -ge 2 ] || { echo "ERROR: --build-dir requires a directory" >&2; exit 2; }
      build_dir="$2"
      shift 2
      ;;
    --findings)
      [ "$#" -ge 2 ] || { echo "ERROR: --findings requires a TSV file" >&2; exit 2; }
      findings="$2"
      shift 2
      ;;
    --timeout)
      [ "$#" -ge 2 ] || { echo "ERROR: --timeout requires seconds" >&2; exit 2; }
      timeout_seconds="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

[ -n "$work_dir" ] || { echo "ERROR: --work-dir is required" >&2; exit 2; }
[ -n "$build_dir" ] || { echo "ERROR: --build-dir is required" >&2; exit 2; }
[ -n "$findings" ] || findings="$work_dir/findings.tsv"

case "$timeout_seconds" in
  ''|*[!0-9]*)
    echo "ERROR: --timeout must be numeric" >&2
    exit 2
    ;;
esac

triage_dir="$work_dir/triage"
logs_dir="$triage_dir/logs"
repro_dir="$triage_dir/reproducers"
summary_tsv="$triage_dir/triage-summary.tsv"
report_md="$triage_dir/triage-report.md"
one_liners_txt="$triage_dir/reproducer-one-liners.txt"
one_liners_md="$triage_dir/reproducer-one-liners.md"
mkdir -p "$logs_dir" "$repro_dir"
printf 'target\tkind\tclassification\texit\tinput\tlog\treproducer\n' > "$summary_tsv"
printf '# Fuzz Triage Reproducer One-Liners\n\n' > "$one_liners_txt"
printf '# Fuzz Triage Reproducer One-Liners\n\n' > "$one_liners_md"

sanitize_name() {
  printf '%s' "$1" | LC_ALL=C sed -E 's#[^A-Za-z0-9._-]#-#g; s#-+#-#g; s#^-##; s#-$##'
}

tool_for_target() {
  case "$1" in
    dump) printf '%s' "$build_dir/Tools/IccDumpProfile/iccDumpProfile" ;;
    toxml) printf '%s' "$build_dir/Tools/IccToXml/iccToXml" ;;
    fromxml) printf '%s' "$build_dir/Tools/IccFromXml/iccFromXml" ;;
    tojson) printf '%s' "$build_dir/Tools/IccToJson/iccToJson" ;;
    fromjson) printf '%s' "$build_dir/Tools/IccFromJson/iccFromJson" ;;
    roundtrip) printf '%s' "$build_dir/Tools/IccRoundTrip/iccRoundTrip" ;;
    fromcube) printf '%s' "$build_dir/Tools/IccFromCube/iccFromCube" ;;
    *) return 1 ;;
  esac
}

build_replay_command() {
  local target="$1"
  local input="$2"
  local tool="$3"
  local out_base="$4"

  case "$target" in
    dump)
      printf 'timeout %ss %q %q ALL' "$timeout_seconds" "$tool" "$input"
      ;;
    toxml)
      printf 'timeout %ss %q %q %q' "$timeout_seconds" "$tool" "$input" "$out_base.xml"
      ;;
    fromxml)
      printf 'timeout %ss %q %q %q' "$timeout_seconds" "$tool" "$input" "$out_base.icc"
      ;;
    tojson)
      printf 'timeout %ss %q %q %q' "$timeout_seconds" "$tool" "$input" "$out_base.json"
      ;;
    fromjson)
      printf 'timeout %ss %q %q %q' "$timeout_seconds" "$tool" "$input" "$out_base.icc"
      ;;
    roundtrip)
      printf 'timeout %ss %q %q 1 0' "$timeout_seconds" "$tool" "$input"
      ;;
    fromcube)
      printf 'timeout %ss %q %q %q' "$timeout_seconds" "$tool" "$input" "$out_base.icc"
      ;;
    *)
      return 1
      ;;
  esac
}

classify_log() {
  local exit_code="$1"
  local log_file="$2"

  if grep -qaE 'AddressSanitizer|ERROR: AddressSanitizer' "$log_file"; then
    printf 'asan'
  elif grep -qaE 'UndefinedBehaviorSanitizer|runtime error:' "$log_file"; then
    printf 'ubsan'
  elif [ "$exit_code" -eq 124 ]; then
    printf 'timeout'
  elif [ "$exit_code" -ge 128 ]; then
    printf 'signal'
  elif [ "$exit_code" -eq 0 ]; then
    printf 'clean'
  else
    printf 'graceful-fail'
  fi
}

if [ ! -r "$findings" ] || [ "$(awk 'END { print NR }' "$findings" 2>/dev/null || echo 0)" -le 1 ]; then
  {
    echo "# Fuzz Finding Triage"
    echo ""
    echo "No AFL/CFL crash or hang findings were available for replay."
  } > "$report_md"
  {
    echo "No AFL/CFL crash or hang findings were available for replay."
  } >> "$one_liners_txt"
  {
    echo "No AFL/CFL crash or hang findings were available for replay."
  } >> "$one_liners_md"
  echo "No fuzz findings to triage."
  exit 0
fi

failures=0
while IFS="$(printf '\t')" read -r target kind source_file artifact_file; do
  [ -n "$target" ] || continue
  tool="$(tool_for_target "$target" || true)"
  if [ -z "$tool" ] || [ ! -x "$tool" ] || [ ! -r "$source_file" ]; then
    safe="$(sanitize_name "$target-$kind-$(basename "$artifact_file")")"
    log_file="$logs_dir/$safe.skipped.log"
    repro_file="$repro_dir/$safe.skipped.txt"
    {
      printf 'Skipped replay for %s %s %s\n' "$target" "$kind" "$artifact_file"
      printf 'Tool: %s\n' "${tool:-unavailable}"
      printf 'Input: %s\n' "$source_file"
    } > "$log_file"
    printf 'Skipped replay: missing tool or unreadable input.\n' > "$repro_file"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$target" "$kind" "skipped" "0" "$source_file" "$log_file" "$repro_file" >> "$summary_tsv"
    failures=$((failures + 1))
    continue
  fi

  safe="$(sanitize_name "$target-$kind-$(basename "$artifact_file")")"
  log_file="$logs_dir/$safe.log"
  repro_file="$repro_dir/$safe.cmd"
  out_base="$triage_dir/$safe.out"
  command_line="$(build_replay_command "$target" "$source_file" "$tool" "$out_base")"

  printf 'cd %q && ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:symbolize=1:allocator_may_return_null=1 UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 %s\n' \
    "$repo_root" "$command_line" > "$repro_file"
  {
    printf '%s %s %s\n' "$target" "$kind" "$(basename "$artifact_file")"
    cat "$repro_file"
    printf '\n'
  } >> "$one_liners_txt"
  {
    printf '## %s %s %s\n\n' "$target" "$kind" "$(basename "$artifact_file")"
    printf '```bash\n'
    cat "$repro_file"
    printf '```\n\n'
  } >> "$one_liners_md"

  set +e
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:symbolize=1:allocator_may_return_null=1 \
  UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
  bash "$repro_file" > "$log_file" 2>&1
  exit_code="$?"
  set -e

  classification="$(classify_log "$exit_code" "$log_file")"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$target" "$kind" "$classification" "$exit_code" "$source_file" "$log_file" "$repro_file" >> "$summary_tsv"
  case "$classification" in
    asan|ubsan|timeout|signal)
      failures=$((failures + 1))
      ;;
  esac
done < <(tail -n +2 "$findings")

{
  echo "# Fuzz Finding Triage"
  echo ""
  echo "| Target | Kind | Classification | Exit | Reproducer |"
  echo "|--------|------|----------------|------|------------|"
  tail -n +2 "$summary_tsv" | while IFS="$(printf '\t')" read -r target kind classification exit_code input log_file repro_file; do
    printf '| %s | %s | %s | %s | %s |\n' \
      "$target" "$kind" "$classification" "$exit_code" "$repro_file"
    printf 'Input: %s\nLog: %s\n' "$input" "$log_file" >/dev/null
  done
  echo ""
  echo "Copy-paste reproducer bundles:"
  echo ""
  echo "- $one_liners_txt"
  echo "- $one_liners_md"
} > "$report_md"

echo "Fuzz triage summary: $summary_tsv"
cat "$summary_tsv"

if awk -F '\t' 'NR > 1 && ($3 == "asan" || $3 == "ubsan" || $3 == "timeout" || $3 == "signal" || $3 == "skipped") { found = 1 } END { exit found ? 0 : 1 }' "$summary_tsv"; then
  exit 1
fi
