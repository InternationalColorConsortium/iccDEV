#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Convert the hand-authored format corpus and audit its generated ICC profiles.
# Set ICCDEV_TOOLS_DIR to the build's Tools directory, or put all six CLIs on PATH.
# Optional: --qa-dir /fresh/path or ICCDEV_FORMAT_QA_OUTDIR=/fresh/path.

set -euo pipefail

qa_override="${ICCDEV_FORMAT_QA_OUTDIR:-}"
case "$#" in
  0) ;;
  1)
    if [[ "$1" == -h || "$1" == --help ]]; then
      printf 'Usage: %s [--qa-dir ABSOLUTE_FRESH_PATH]\n' "${0##*/}"
      exit 0
    fi
    printf 'Usage: %s [--qa-dir ABSOLUTE_FRESH_PATH]\n' "${0##*/}" >&2
    exit 2
    ;;
  2)
    if [[ "$1" != --qa-dir ]]; then
      printf 'Usage: %s [--qa-dir ABSOLUTE_FRESH_PATH]\n' "${0##*/}" >&2
      exit 2
    fi
    qa_override="$2"
    ;;
  *)
    printf 'Usage: %s [--qa-dir ABSOLUTE_FRESH_PATH]\n' "${0##*/}" >&2
    exit 2
    ;;
esac

tools_dir="${ICCDEV_TOOLS_DIR:-}"
if [[ -n "$tools_dir" ]]; then
  if [[ ! -d "$tools_dir" ]]; then
    printf 'Tools directory does not exist: %s\n' "$tools_dir" >&2
    exit 2
  fi
  tools_dir="$(cd -- "$tools_dir" && pwd -P)"
fi

format_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd -- "$format_dir"

resolve_tool() {
  local binary="$1" target_dir="$2" candidate
  if [[ -n "$tools_dir" ]]; then
    for candidate in "$tools_dir/$target_dir/$binary" "$tools_dir/$binary"; do
      if [[ -x "$candidate" && ! -d "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
    printf 'Required tool not found under %s: %s\n' "$tools_dir" "$binary" >&2
    return 2
  fi
  if ! command -v "$binary"; then
    printf 'Required tool is not on PATH: %s\n' "$binary" >&2
    return 2
  fi
}

from_json="$(resolve_tool iccFromJson IccFromJson)"
from_xml="$(resolve_tool iccFromXml IccFromXml)"
to_json="$(resolve_tool iccToJson IccToJson)"
to_xml="$(resolve_tool iccToXml IccToXml)"
dump_tool="$(resolve_tool iccDumpProfile IccDumpProfile)"
roundtrip_tool="$(resolve_tool iccRoundTrip IccRoundTrip)"
if ! command -v timeout >/dev/null 2>&1; then
  printf 'GNU timeout is required for bounded tool calls\n' >&2
  exit 2
fi

names=(
  acs-bookends-coverage
  compressed-text-coverage
  format-feature-coverage
  sequence-metadata-coverage
  metadata-numeric-coverage
  dict-localization-coverage
  response-curve-coverage
)
precision_name=u16fixed16-xml-precision-coverage
for name in "${names[@]}"; do
  for ext in json xml; do
    input="$name.$ext"
    if [[ ! -s "$input" ]]; then
      printf 'Missing or empty input: %s\n' "$input" >&2
      exit 2
    fi
  done
done
if [[ ! -s "$precision_name.xml" ]]; then
  printf 'Missing or empty input: %s\n' "$precision_name.xml" >&2
  exit 2
fi

# A fresh external directory prevents stale report files from being mistaken
# for results from this run. The override's parent must already exist.
if [[ -n "$qa_override" ]]; then
  if [[ "$qa_override" != /* ]]; then
    printf 'QA output override must be an absolute path: %s\n' "$qa_override" >&2
    exit 2
  fi
  qa_parent="$(dirname -- "$qa_override")"
  if [[ ! -d "$qa_parent" ]]; then
    printf 'QA output parent does not exist: %s\n' "$qa_parent" >&2
    exit 2
  fi
  qa_dir="$(cd -- "$qa_parent" && pwd -P)/$(basename -- "$qa_override")"
  if [[ -e "$qa_dir" || -L "$qa_dir" ]]; then
    printf 'QA output directory must be fresh: %s\n' "$qa_dir" >&2
    exit 2
  fi
  case "$qa_dir" in
    "$format_dir"|"$format_dir"/*)
      printf 'QA output directory must be outside Testing/Format: %s\n' "$qa_dir" >&2
      exit 2
      ;;
  esac
  mkdir -- "$qa_dir"
else
  mkdir -p -- "$HOME/qa"
  qa_dir="$(mktemp -d "$HOME/qa/iccdev-format-qa.XXXXXXXX")"
fi
mkdir -p -- "$qa_dir/reparsed" "$qa_dir/logs" "$qa_dir/diffs" "$qa_dir/profraw"
# Instrumented tools must not leave default.profraw beside the authored inputs.
export LLVM_PROFILE_FILE="$qa_dir/profraw/format-%4m.profraw"
printf 'QA evidence: %s\n' "$qa_dir"

convert() {
  local binary="$1" input="$2" output="$3" log="$4" rc
  rm -f -- "$output"
  printf '%s %s %s\n' "$(basename -- "$binary")" "$input" "$output"
  if timeout 30s "$binary" "$input" "$output" >"$log" 2>&1; then
    rc=0
  else
    rc=$?
  fi
  if [[ "$rc" -ne 0 || ! -s "$output" ]]; then
    rm -f -- "$output"
    printf 'Conversion failed or produced no ICC: %s (exit %s; %s)\n' \
      "$input" "$rc" "$log" >&2
    return 1
  fi
}

icc_names=()
for name in "${names[@]}"; do
  convert "$from_json" "$name.json" "$name.json.icc" \
    "$qa_dir/logs/$name.json.icc.generate.log"
  icc_names+=("$name.json.icc")
  convert "$from_xml" "$name.xml" "$name.xml.icc" \
    "$qa_dir/logs/$name.xml.icc.generate.log"
  icc_names+=("$name.xml.icc")
done
convert "$from_xml" "$precision_name.xml" "$precision_name.xml.icc" \
  "$qa_dir/logs/$precision_name.xml.icc.generate.log"
icc_names+=("$precision_name.xml.icc")

conversion_tsv="$qa_dir/conversion-results.tsv"
roundtrip_tsv="$qa_dir/roundtrip-results.tsv"
dump_tsv="$qa_dir/dump-results.tsv"
printf 'icc\tformat\tauthored_text_bytes\treturned_text_bytes\ttext_equal\tdiff_path\treparsed_icc_equal\n' \
  > "$conversion_tsv"
printf 'icc\tprofile_class\texit_code\tstatus\n' > "$roundtrip_tsv"
printf 'icc\texit_code\tvalid\tlog\n' > "$dump_tsv"

valid_count=0
fidelity_pass=0
fidelity_fail=0
transform_pass=0
transform_unsupported=0
transform_fail=0
other_fail=0

for icc in "${icc_names[@]}"; do
  dump_log="$qa_dir/logs/$icc.dump.log"
  if timeout 30s "$dump_tool" -v "$icc" >"$dump_log" 2>&1; then
    dump_rc=0
  else
    dump_rc=$?
  fi
  if [[ "$dump_rc" -eq 0 ]] && \
     grep -Eq '^Profile is valid for version [0-9]+\.[0-9]+$' "$dump_log"; then
    dump_valid=yes
    valid_count=$((valid_count + 1))
  else
    dump_valid=no
    other_fail=$((other_fail + 1))
  fi
  printf '%s\t%s\t%s\t%s\n' "$icc" "$dump_rc" "$dump_valid" \
    "logs/$icc.dump.log" >> "$dump_tsv"

  base="${icc%.icc}"
  base="${base%.json}"
  base="${base%.xml}"
  for format in xml json; do
    returned="$qa_dir/$icc.$format"
    to_log="$qa_dir/logs/$icc.to-$format.log"
    from_log="$qa_dir/logs/$icc.reparse-$format.log"
    reparsed="$qa_dir/reparsed/$icc.$format.icc"
    if [[ "$format" == xml ]]; then
      to_tool="$to_xml"
      from_tool="$from_xml"
    else
      to_tool="$to_json"
      from_tool="$from_json"
    fi

    authored="$format_dir/$base.$format"
    authored_bytes=-
    returned_bytes=-
    text_equal=not_applicable
    diff_path=-
    reparsed_equal=no
    if timeout 30s "$to_tool" "$icc" "$returned" >"$to_log" 2>&1 && \
       [[ -s "$returned" ]]; then
      returned_bytes="$(wc -c < "$returned" | tr -d ' ')"
      if [[ -f "$authored" ]]; then
        authored_bytes="$(wc -c < "$authored" | tr -d ' ')"
        if cmp -s -- "$authored" "$returned"; then
          text_equal=yes
        else
          text_equal=no
          diff_path="diffs/$icc.$format.diff"
          if diff -u -- "$authored" "$returned" >"$qa_dir/$diff_path"; then
            :
          else
            diff_rc=$?
            if [[ "$diff_rc" -ne 1 ]]; then
              other_fail=$((other_fail + 1))
            fi
          fi
        fi
      fi
      if timeout 30s "$from_tool" "$returned" "$reparsed" \
          >"$from_log" 2>&1 && [[ -s "$reparsed" ]] && \
          cmp -s -- "$icc" "$reparsed"; then
        reparsed_equal=yes
        fidelity_pass=$((fidelity_pass + 1))
      else
        fidelity_fail=$((fidelity_fail + 1))
      fi
    else
      fidelity_fail=$((fidelity_fail + 1))
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$icc" "$format" "$authored_bytes" "$returned_bytes" \
      "$text_equal" "$diff_path" "$reparsed_equal" >> "$conversion_tsv"
  done

  class="$(dd if="$icc" bs=1 skip=12 count=4 status=none)"
  roundtrip_log="$qa_dir/logs/$icc.roundtrip.log"
  if timeout 30s "$roundtrip_tool" "$icc" >"$roundtrip_log" 2>&1; then
    roundtrip_rc=0
  else
    roundtrip_rc=$?
  fi
  if [[ "$class" == mntr && "$roundtrip_rc" -eq 0 ]]; then
    transform_status=pass
    transform_pass=$((transform_pass + 1))
  elif [[ "$icc" == acs-bookends-coverage.*.icc && "$class" == abst && \
          "$roundtrip_rc" -eq 255 ]] && \
       grep -Fq 'Unsupported profile class' "$roundtrip_log"; then
    transform_status=unsupported
    transform_unsupported=$((transform_unsupported + 1))
  else
    transform_status=fail
    transform_fail=$((transform_fail + 1))
  fi
  printf '%s\t%s\t%s\t%s\n' "$icc" "$class" "$roundtrip_rc" \
    "$transform_status" >> "$roundtrip_tsv"
done

summary="$qa_dir/summary.txt"
{
  printf 'Authored ICC profiles: %d\n' "${#icc_names[@]}"
  printf 'iccDumpProfile -v valid: %d/%d\n' "$valid_count" "${#icc_names[@]}"
  printf 'Returned XML/JSON reparses byte-identical: %d/%d\n' \
    "$fidelity_pass" "$((2 * ${#icc_names[@]}))"
  printf 'Returned XML/JSON reparse failures: %d\n' "$fidelity_fail"
  printf 'iccRoundTrip transform diagnostic: %d pass, %d unsupported, %d fail\n' \
    "$transform_pass" "$transform_unsupported" "$transform_fail"
  printf 'Text differences are informational; see conversion-results.tsv and diffs/.\n'
} > "$summary"
cat "$summary"
printf 'Conversion TSV: %s\n' "$conversion_tsv"
printf 'Round-trip TSV: %s\n' "$roundtrip_tsv"
printf 'Dump TSV: %s\n' "$dump_tsv"
printf 'Summary: %s\n' "$summary"

if [[ "$valid_count" -ne "${#icc_names[@]}" || \
      "$fidelity_pass" -ne "$((2 * ${#icc_names[@]}))" || \
      "$fidelity_fail" -ne 0 || "$transform_pass" -ne 13 || \
      "$transform_unsupported" -ne 2 || "$transform_fail" -ne 0 || \
      "$other_fail" -ne 0 ]]; then
  exit 1
fi
