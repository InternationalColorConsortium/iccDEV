#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Generate llvm-cov reports with profraw data matched to the owning object.
###############################################################################

set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 BUILD_DIR PROFRAW_DIR REPORT_DIR" >&2
  exit 2
fi

BUILD_DIR="$1"
PROFRAW_DIR="$2"
REPORT_DIR="$3"

LLVM_COV="${LLVM_COV:-llvm-cov-18}"
LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata-18}"
IGNORE_REGEX="${IGNORE_FILENAME_REGEX:-(third_party|/usr/|test)}"

mkdir -p \
  "$REPORT_DIR/html" \
  "$REPORT_DIR/lcov" \
  "$REPORT_DIR/matched" \
  "$REPORT_DIR/profdata" \
  "$REPORT_DIR/probes"

candidates_file="$REPORT_DIR/candidate-objects.txt"
signatures_file="$REPORT_DIR/profile-signatures.txt"
mapping_file="$REPORT_DIR/matched-objects.tsv"
summary_file="$REPORT_DIR/coverage-summary.txt"
env_file="$REPORT_DIR/coverage-env.txt"
lcov_file="$REPORT_DIR/lcov.info"

: > "$candidates_file"
: > "$mapping_file"
: > "$summary_file"
: > "$env_file"
: > "$lcov_file"

find "$BUILD_DIR" -path '*/Tools/*' -type f -executable \
  ! -name iccDumpProfileGui -print >> "$candidates_file"
find "$BUILD_DIR" -name 'lib*.so' \( -type f -o -type l \) \
  -print >> "$candidates_file"
sort -u -o "$candidates_file" "$candidates_file"

find "$PROFRAW_DIR" -name '*.profraw' -printf '%f\n' \
  | sed -nE 's/.*-([0-9]+(_[0-9]+)?)\.profraw$/\1/p' \
  | sort -u > "$signatures_file"

if [ ! -s "$candidates_file" ]; then
  echo "no coverage objects found under $BUILD_DIR" >&2
  exit 1
fi

if [ ! -s "$signatures_file" ]; then
  echo "no profraw binary signatures found under $PROFRAW_DIR" >&2
  exit 1
fi

safe_name()
{
  basename "$1" | tr -c 'A-Za-z0-9_.-' '_'
}

total_score()
{
  awk '
    $1 == "TOTAL" {
      regions = $2 - $3
      funcs = $5 - $6
      lines = $8 - $9
      branches = $11 - $12
      print regions + funcs + lines + branches
      found = 1
      exit
    }
    END {
      if (!found) {
        print 0
      }
    }
  ' "$1"
}

total_fields()
{
  awk '
    $1 == "TOTAL" {
      print $2, $3, $5, $6, $8, $9, $11, $12
      found = 1
      exit
    }
    END {
      if (!found) {
        print "0 0 0 0 0 0 0 0"
      }
    }
  ' "$1"
}

percent_covered()
{
  local total="$1"
  local missed="$2"

  awk -v total="$total" -v missed="$missed" '
    BEGIN {
      if (total == 0) {
        print "-"
      } else {
        printf "%.2f%%", ((total - missed) * 100.0) / total
      }
    }
  '
}

sum_regions=0
sum_missed_regions=0
sum_functions=0
sum_missed_functions=0
sum_lines=0
sum_missed_lines=0
sum_branches=0
sum_missed_branches=0
matched_count=0
unmatched_count=0
max_mismatch=0
max_html_mismatch=0

printf 'signature\tobject\tprofraw_files\tcovered_score\n' > "$mapping_file"

while IFS= read -r signature; do
  profdata="$REPORT_DIR/profdata/${signature}.profdata"
  mapfile -d '' -t profraw_files < <(
    find "$PROFRAW_DIR" -name "*-${signature}.profraw" -print0 | sort -z
  )
  if [ "${#profraw_files[@]}" -eq 0 ]; then
    unmatched_count=$((unmatched_count + 1))
    continue
  fi

  "$LLVM_PROFDATA" merge -sparse "${profraw_files[@]}" -o "$profdata"

  best_object=""
  best_report=""
  best_error=""
  best_score=0

  while IFS= read -r object; do
    object_safe="$(safe_name "$object")"
    probe_report="$REPORT_DIR/probes/${signature}-${object_safe}.txt"
    probe_error="$REPORT_DIR/probes/${signature}-${object_safe}.err"

    set +e
    "$LLVM_COV" report "$object" \
      -instr-profile="$profdata" \
      -ignore-filename-regex="$IGNORE_REGEX" \
      > "$probe_report" 2> "$probe_error"
    cov_rc="$?"
    set -e

    if [ "$cov_rc" -ne 0 ]; then
      continue
    fi
    if grep -qE '[0-9]+ functions have mismatched data' "$probe_error"; then
      continue
    fi

    score="$(total_score "$probe_report")"
    if [ "$score" -gt "$best_score" ]; then
      best_score="$score"
      best_object="$object"
      best_report="$probe_report"
      best_error="$probe_error"
    fi
  done < "$candidates_file"

  if [ -z "$best_object" ] || [ "$best_score" -eq 0 ]; then
    echo "no aligned coverage object found for signature $signature" >&2
    unmatched_count=$((unmatched_count + 1))
    continue
  fi

  matched_count=$((matched_count + 1))
  object_safe="$(safe_name "$best_object")"
  matched_report="$REPORT_DIR/matched/${matched_count}-${object_safe}.txt"
  matched_error="$REPORT_DIR/matched/${matched_count}-${object_safe}.err"
  cp "$best_report" "$matched_report"
  cp "$best_error" "$matched_error"

  read -r regions missed_regions funcs missed_funcs lines missed_lines \
    branches missed_branches < <(total_fields "$matched_report")
  sum_regions=$((sum_regions + regions))
  sum_missed_regions=$((sum_missed_regions + missed_regions))
  sum_functions=$((sum_functions + funcs))
  sum_missed_functions=$((sum_missed_functions + missed_funcs))
  sum_lines=$((sum_lines + lines))
  sum_missed_lines=$((sum_missed_lines + missed_lines))
  sum_branches=$((sum_branches + branches))
  sum_missed_branches=$((sum_missed_branches + missed_branches))

  printf '%s\t%s\t%s\t%s\n' \
    "$signature" "$best_object" "${#profraw_files[@]}" "$best_score" \
    >> "$mapping_file"

  html_dir="$REPORT_DIR/html/${matched_count}-${object_safe}"
  set +e
  "$LLVM_COV" show "$best_object" \
    -instr-profile="$profdata" \
    -format=html \
    -output-dir="$html_dir" \
    -show-line-counts-or-regions \
    -show-expansions \
    -show-branches=count \
    -ignore-filename-regex="$IGNORE_REGEX" \
    > "$REPORT_DIR/matched/${matched_count}-${object_safe}-html.out" \
    2> "$REPORT_DIR/matched/${matched_count}-${object_safe}-html.err"
  html_rc="$?"
  set -e
  if [ "$html_rc" -ne 0 ]; then
    echo "llvm-cov show failed for $best_object (exit $html_rc)" >&2
  fi
  html_mismatch="$(
    grep -oE '[0-9]+ functions have mismatched data' \
      "$REPORT_DIR/matched/${matched_count}-${object_safe}-html.err" \
      | awk '{print $1}' \
      | head -1 || true
  )"
  if [ "${html_mismatch:-0}" -gt "$max_html_mismatch" ]; then
    max_html_mismatch="$html_mismatch"
  fi

  lcov_part="$REPORT_DIR/lcov/${matched_count}-${object_safe}.info"
  set +e
  "$LLVM_COV" export "$best_object" \
    -instr-profile="$profdata" \
    -format=lcov \
    -ignore-filename-regex="$IGNORE_REGEX" \
    > "$lcov_part" \
    2> "$REPORT_DIR/lcov/${matched_count}-${object_safe}.err"
  lcov_rc="$?"
  set -e
  if [ "$lcov_rc" -ne 0 ]; then
    : > "$lcov_part"
    echo "llvm-cov export failed for $best_object (exit $lcov_rc)" >&2
  fi
  cat "$lcov_part" >> "$lcov_file"
done < "$signatures_file"

if [ "$unmatched_count" -ne 0 ]; then
  echo "$unmatched_count profraw signature(s) could not be matched" >&2
  exit 1
fi

if [ "$max_mismatch" -ne 0 ] || [ "$max_html_mismatch" -ne 0 ]; then
  echo "matched reports still produced llvm-cov mismatch diagnostics" >&2
  exit 1
fi

region_pct="$(percent_covered "$sum_regions" "$sum_missed_regions")"
func_pct="$(percent_covered "$sum_functions" "$sum_missed_functions")"
line_pct="$(percent_covered "$sum_lines" "$sum_missed_lines")"
branch_pct="$(percent_covered "$sum_branches" "$sum_missed_branches")"

{
  printf '%-60s %10s %10s %9s %10s %10s %9s %10s %10s %9s %10s %10s %9s\n' \
    Filename Regions Missed Cover Functions Missed Cover Lines Missed Cover \
    Branches Missed Cover
  printf '%-60s %10s %10s %9s %10s %10s %9s %10s %10s %9s %10s %10s %9s\n' \
    TOTAL "$sum_regions" "$sum_missed_regions" "$region_pct" \
    "$sum_functions" "$sum_missed_functions" "$func_pct" \
    "$sum_lines" "$sum_missed_lines" "$line_pct" \
    "$sum_branches" "$sum_missed_branches" "$branch_pct"
  printf '\nMatched coverage objects\n'
  printf '========================\n\n'
  cat "$mapping_file"
  printf '\nPer-object llvm-cov reports\n'
  printf '===========================\n'
  for report in "$REPORT_DIR"/matched/*.txt; do
    [ -f "$report" ] || continue
    printf '\n## %s\n\n' "$(basename "$report")"
    cat "$report"
  done
} > "$summary_file"

html_count="$(find "$REPORT_DIR/html" -name '*.html' 2>/dev/null | wc -l)"
lcov_records="$(grep -c '^SF:' "$lcov_file" 2>/dev/null || printf 0)"
profdata_kb="$(
  find "$REPORT_DIR/profdata" -name '*.profdata' -printf '%s\n' \
    | awk '{sum += $1} END {printf "%d", sum / 1024}'
)"

{
  printf 'mismatch_count=0\n'
  printf 'html_mismatch_count=0\n'
  printf 'matched_object_count=%s\n' "$matched_count"
  printf 'profile_signature_count=%s\n' "$(wc -l < "$signatures_file")"
  printf 'html_count=%s\n' "$html_count"
  printf 'lcov_records=%s\n' "$lcov_records"
  printf 'profdata_kb=%s\n' "$profdata_kb"
  printf 'region_pct=%s\n' "$region_pct"
  printf 'func_pct=%s\n' "$func_pct"
  printf 'line_pct=%s\n' "$line_pct"
  printf 'branch_pct=%s\n' "$branch_pct"
} > "$env_file"

printf 'Matched %s profraw signature(s) to coverage objects without mismatches\n' \
  "$matched_count"
