#!/usr/bin/env bash
# Shared ICC command-line QA scanner. Source from a tool-specific wrapper.

# shellcheck disable=SC2016
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

sanitize_line() {
  local s="$1"
  s="${s//$'\r'/ }"
  s="${s//$'\n'/ }"
  printf '%s' "$s" | sed -E 's/\x1b\[[0-9;]*[A-Za-z]//g; s/\x1b\][^\x07]*\x07//g; s/\x1b//g' |
    tr -d '\000-\010\013\014\016-\037\177' |
    sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//'
}

status_is_fail_on() {
  local status="$1"
  local item
  [[ "$FAIL_ON" != "none" ]] || return 1
  IFS=',' read -r -a fail_items <<< "$FAIL_ON"
  for item in "${fail_items[@]}"; do
    item="$(sanitize_line "$item")"
    if [[ "$item" == "$status" ]]; then
      return 0
    fi
  done
  return 1
}

write_summary_md() {
  local shown=0
  {
    printf '# %s QA scan summary\n\n' "$(sanitize_line "$TOOL_NAME")"
    printf '| Field | Value |\n'
    printf '|-------|-------|\n'
    printf '| Tool | `%s` |\n' "$(sanitize_line "$TOOL_NAME")"
    printf '| Files | %d |\n' "$total_files"
    printf '| Runs | %d |\n' "$total_runs"
    printf '| PASS | %d |\n' "$pass"
    printf '| QA-ISSUE | %d |\n' "$qa_issue"
    printf '| FAIL | %d |\n' "$fail"
    printf '| CRASH | %d |\n' "$crash"
    printf '| TIMEOUT | %d |\n' "$timeout_count"
    printf '| Fail-on | `%s` |\n' "$(sanitize_line "$FAIL_ON")"
    printf '| Results | `%s` |\n' "$(sanitize_line "$RESULTS")"
    printf '| Findings | `%s` |\n\n' "$(sanitize_line "$FINDINGS")"
    printf '## Non-pass rows\n\n'
    printf '| File | Variant | Exit | Status | Sanitizer | Signals | Errors | Warnings |\n'
    printf '|------|---------|------|--------|-----------|---------|--------|----------|\n'
    while IFS=$'\t' read -r file variant exit_code status sanitizer signals errors warnings _rest; do
      [[ "$file" != "file" ]] || continue
      [[ "$status" != "PASS" ]] || continue
      [[ "$shown" -lt 80 ]] || break
      shown=$((shown + 1))
      printf '| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` |\n' \
        "$(sanitize_line "$file")" "$(sanitize_line "$variant")" \
        "$(sanitize_line "$exit_code")" "$(sanitize_line "$status")" \
        "$(sanitize_line "$sanitizer")" "$(sanitize_line "$signals")" \
        "$(sanitize_line "$errors")" "$(sanitize_line "$warnings")"
    done < "$RESULTS"
  } > "$SUMMARY_MD"
}

sanitize_filename() {
  sanitize_line "$1" | LC_ALL=C sed -E 's#[^A-Za-z0-9._-]#-#g; s/-+/-/g; s/^-//; s/-$//'
}

TOOL_NAME="${ICC_QA_TOOL_NAME:?ICC_QA_TOOL_NAME is required}"
TOOL="${ICC_QA_TOOL-}"
TOOL_EXPLICIT="${ICC_QA_TOOL_EXPLICIT-}"
TOOL_USAGE="${ICC_QA_TOOL_USAGE:-$TOOL_NAME profile.icc}"
VARIANTS="${ICC_QA_VARIANTS:?ICC_QA_VARIANTS is required}"
INPUT_DIR="."
OUT_PREFIX="${ICC_QA_OUT_PREFIX:-icc-tool-qa}"
OUTDIR="${ICC_QA_OUTDIR:-$HOME/work/copilot/${OUT_PREFIX}-$(date -u +%Y%m%dT%H%M%SZ)}"
TIMEOUT_SECONDS="${ICC_QA_TIMEOUT:-${ICC_QA_TIMEOUT_DEFAULT:-60}}"
MAX_DEPTH=1
SHOW_ALL=0
PROGRESS=0
LIST_VARIANTS=0
VARIANT_FILTER=""
MAX_FILES=0
LOG_TAIL_LINES="${ICC_QA_LOG_TAIL_LINES:-0}"
FAIL_ON="${ICC_QA_FAIL_ON:-CRASH,TIMEOUT}"
SUMMARY_MD=""

usage() {
  cat <<USAGE
Usage: $0 [options] [input-dir]

Tool:
  $TOOL_USAGE

Options:
  --tool PATH       tool binary to run
  --out-dir PATH    output directory for logs and TSV summary
  --timeout SEC     per-file/per-variant timeout, default: $TIMEOUT_SECONDS
  --recursive       scan input-dir recursively
  --progress        print each file and variant to stderr before running it
  --all-lines       echo every output line, not only matched findings
  --variant NAME    run one variant from --list-variants
  --max-files N     stop after N input files, useful for smoke tests
  --log-tail-lines N
                    after classification, keep bounded log excerpts with the
                    first 200 lines and final N lines; 0 keeps full logs
  --fail-on LIST    comma-separated statuses that make the script fail
                    default: $FAIL_ON; use "none" to always exit 0
  --summary-md PATH write Markdown summary, default: <out-dir>/summary.md
  --list-variants   list command variants and exit
  -h, --help        show this help

Outputs:
  <out-dir>/logs/<variant>-<file>.log
  <out-dir>/results.tsv
  <out-dir>/findings.txt

Statuses:
  CRASH      sanitizer finding or signal evidence
  TIMEOUT    timeout(1) exit 124
  FAIL       non-zero tool exit without sanitizer/signal evidence
  QA-ISSUE   exit 0 with FAIL/WARN/GAP/NOT RUN or error/warning text
  PASS       exit 0 without matched issue indicators
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --tool) TOOL="$2"; TOOL_EXPLICIT=x; shift 2 ;;
    --out-dir) OUTDIR="$2"; shift 2 ;;
    --timeout) TIMEOUT_SECONDS="$2"; shift 2 ;;
    --recursive) MAX_DEPTH=0; shift ;;
    --progress) PROGRESS=1; shift ;;
    --all-lines) SHOW_ALL=1; shift ;;
    --variant) VARIANT_FILTER="$2"; shift 2 ;;
    --max-files) MAX_FILES="$2"; shift 2 ;;
    --log-tail-lines) LOG_TAIL_LINES="$2"; shift 2 ;;
    --fail-on) FAIL_ON="$2"; shift 2 ;;
    --summary-md) SUMMARY_MD="$2"; shift 2 ;;
    --list-variants) LIST_VARIANTS=1; shift ;;
    -h|--help) usage; exit 0 ;;
    -*) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    *) INPUT_DIR="$1"; shift ;;
  esac
done

if ! [[ "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]] || [[ "$TIMEOUT_SECONDS" -lt 1 ]]; then
  echo "ERROR: --timeout must be a positive integer" >&2
  exit 2
fi

if ! [[ "$MAX_FILES" =~ ^[0-9]+$ ]]; then
  echo "ERROR: --max-files must be a non-negative integer" >&2
  exit 2
fi

if ! [[ "$LOG_TAIL_LINES" =~ ^[0-9]+$ ]]; then
  echo "ERROR: --log-tail-lines must be a non-negative integer" >&2
  exit 2
fi

TIMEOUT_SECONDS=$((10#$TIMEOUT_SECONDS))
MAX_FILES=$((10#$MAX_FILES))
LOG_TAIL_LINES=$((10#$LOG_TAIL_LINES))

list_variants() {
  printf "variant\tbefore_input\tafter_input\n"
  while IFS='|' read -r variant before after; do
    [[ -n "$variant" ]] || continue
    printf "%s\t%s\t%s\n" "$variant" "$before" "$after"
  done <<< "$VARIANTS"
}

if [[ "$LIST_VARIANTS" -eq 1 ]]; then
  list_variants
  exit 0
fi

if [[ -z "$TOOL_EXPLICIT" ]]; then
  # Match build-tree subdirectories and flat installed tool directories.
  # Explicit per-tool overrides never fall back, even when empty or invalid.
  for tools_dir in "${ICCDEV_TOOLS_DIR-}" "$REPO_ROOT/Build/Tools"; do
    [[ -n "$tools_dir" ]] || continue
    for candidate in "$tools_dir/${ICC_QA_TOOL_SUBDIR:?}/$TOOL_NAME" "$tools_dir/$TOOL_NAME"; do
      if [[ -f "$candidate" && -x "$candidate" ]]; then
        TOOL="$candidate"
        break 2
      fi
    done
  done
  if [[ -z "$TOOL" ]]; then
    TOOL="$(command -v "$TOOL_NAME" || true)"
  fi
fi

if [[ ! -f "$TOOL" || ! -x "$TOOL" ]]; then
  echo "ERROR: $TOOL_NAME not executable: $TOOL" >&2
  exit 2
fi

if [[ "$TOOL_NAME" == "iccPawgReport" ]] && ! command -v python3 >/dev/null; then
  echo "ERROR: PAWG structured classification requires python3" >&2
  exit 2
fi

if [[ ! -d "$INPUT_DIR" ]]; then
  echo "ERROR: input directory not found: $INPUT_DIR" >&2
  exit 2
fi

mkdir -p "$OUTDIR/logs"
RESULTS="$OUTDIR/results.tsv"
FINDINGS="$OUTDIR/findings.txt"
SUMMARY_MD="${SUMMARY_MD:-$OUTDIR/summary.md}"

printf "file\tvariant\texit\tstatus\tsanitizer\tsignals\terrors\twarnings\tqa_fail\tqa_warn\tqa_gap\tqa_not_run\tcompliant\n" > "$RESULTS"
: > "$FINDINGS"

if [[ -d "$REPO_ROOT/Build/IccProfLib" ]]; then
  export LD_LIBRARY_PATH="$REPO_ROOT/Build/IccProfLib:$REPO_ROOT/Build/IccXML${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0,halt_on_error=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

SANITIZER_RE='AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer'
SANITIZER_RE+='|MemorySanitizer|ThreadSanitizer|runtime error:|SCARINESS|SUMMARY:'
SIGNAL_RE='DEADLYSIGNAL|Segmentation fault|core dumped'
SIGNAL_RE+='|SIG(SEGV|ABRT|BUS|ILL|FPE|KILL)'
SIGNAL_RE+='|stack-buffer-overflow|heap-buffer-overflow|global-buffer-overflow'
SIGNAL_RE+='|use-after-free|double-free|null pointer'
ERROR_WORDS='ERROR|Error|FAILURE|Failed|failed|Fatal|fatal'
ERROR_WORDS+='|ASSERT|Assert|assert|ABORT|Abort|abort|Exception|exception'
ERROR_WORDS+='|invalid|Invalid|INVALID|corrupt|Corrupt|CORRUPT'
ERROR_WORDS+='|malformed|Malformed|MALFORMED|truncated|Truncated|TRUNCATED'
ERROR_WORDS+='|unsupported|Unsupported|UNSUPPORTED|undefined|Undefined|UNDEFINED'
ERROR_WORDS+='|overflow|Overflow|OVERFLOW|underflow|Underflow|UNDERFLOW'
ERROR_WORDS+='|out of range|Out of range|OUT OF RANGE'
ERROR_WORDS+='|cannot|Cannot|CANNOT|unable|Unable|UNABLE|bad|Bad|BAD'
ERROR_WORDS+='|Zip|zip|compress|compressed|compression'
WARNING_WORDS='WARNING|Warning|warning'
WARNING_WORDS+='|caution|Caution|CAUTION|notice|Notice|NOTICE'
ERROR_RE="(^|[^[:alpha:]])($ERROR_WORDS)([^[:alpha:]]|$)"
WARNING_RE="(^|[^[:alpha:]])($WARNING_WORDS)([^[:alpha:]]|$)"
QA_FAIL_RE='(\[FAIL\]|(^|[[:space:]])FAIL:[[:space:]]*[1-9][0-9]*)'
QA_WARN_RE='(\[WARN\]|(^|[[:space:]])WARN:[[:space:]]*[1-9][0-9]*)'
QA_GAP_RE='(\[GAP\]|(^|[[:space:]])GAP:[[:space:]]*[1-9][0-9]*)'
QA_NOT_RUN_RE='(\[NOT RUN\]|(^|[[:space:]])NOT RUN:[[:space:]]*[1-9][0-9]*)'
COMPLIANT_RE='(^|[^[:alpha:]])(Compliant|compliant|COMPLIANT|Non-Compliant|non-compliant|NON-COMPLIANT|Noncompliant|noncompliant)([^[:alpha:]]|$)'
MATCH_RE="$SANITIZER_RE|$SIGNAL_RE|$ERROR_RE|$WARNING_RE"
MATCH_RE+="|$QA_FAIL_RE|$QA_WARN_RE|$QA_GAP_RE|$QA_NOT_RUN_RE|$COMPLIANT_RE"
if [[ "$TOOL_NAME" == "iccPawgReport" ]]; then
  MATCH_RE="$SANITIZER_RE|$SIGNAL_RE"
fi

safe_log_name() {
  local name
  name="$(sanitize_filename "$1")"
  printf '%s' "${name:-input}.log"
}

count_matches() {
  local regex="$1" file="$2"
  grep -Ec "$regex" "$file" 2>/dev/null || true
}

classify_exit() {
  local exit_code="$1" sanitizer="$2" signals="$3" issue_count="$4"
  if [[ "$sanitizer" -gt 0 || "$signals" -gt 0 ]]; then
    printf 'CRASH'
  elif [[ "$exit_code" -ge 128 ]]; then
    printf 'CRASH'
  elif [[ "$exit_code" -eq 124 ]]; then
    printf 'TIMEOUT'
  elif [[ "$exit_code" -ne 0 ]]; then
    printf 'FAIL'
  elif [[ "$issue_count" -gt 0 ]]; then
    printf 'QA-ISSUE'
  else
    printf 'PASS'
  fi
}

emit_findings() {
  local input="$1" variant="$2" logfile="$3" status="$4" exit_code="$5"
  local matched=0
  if [[ "$SHOW_ALL" -eq 1 ]]; then
    while IFS= read -r line; do
      printf '[%s][%s] [OUTPUT] %s\n' "$(sanitize_line "$input")" "$(sanitize_line "$variant")" "$(sanitize_line "$line")"
    done < "$logfile"
    return
  fi
  while IFS= read -r line; do
    matched=1
    printf '[%s][%s] %s\n' "$(sanitize_line "$input")" "$(sanitize_line "$variant")" "$(sanitize_line "$line")"
  done < <(grep -En "$MATCH_RE" "$logfile" 2>/dev/null || true)
  if [[ "$matched" -eq 0 ]]; then
    case "$status" in
      TIMEOUT|FAIL|CRASH)
        printf '[%s][%s] [%s] exit=%s no matched diagnostic output\n' \
          "$(sanitize_line "$input")" "$(sanitize_line "$variant")" "$status" "$exit_code"
        ;;
    esac
  fi
}

compact_logfile() {
  local logfile="$1"
  local line_count="$2"
  local keep_tail="$LOG_TAIL_LINES"
  local keep_head=200
  local tmp

  [[ "$keep_tail" -gt 0 ]] || return 0
  [[ "$line_count" -gt $((keep_head + keep_tail)) ]] || return 0

  tmp="${logfile}.compact"
  {
    printf '%s\n' \
      "--- log excerpt: first ${keep_head} lines, final ${keep_tail} lines; original had ${line_count} lines ---"
    head -n "$keep_head" "$logfile"
    printf '\n%s\n\n' \
      "--- middle omitted: $((line_count - keep_head - keep_tail)) lines ---"
    tail -n "$keep_tail" "$logfile"
  } > "$tmp"
  mv -f "$tmp" "$logfile"
}

run_variant() {
  local input="$1" rel="$2" variant="$3" before="$4" after="$5"
  local logfile exit_code sanitizer signals errors warnings
  local qa_fail_count qa_warn_count qa_gap_count qa_not_run_count compliant_count
  local issue_count status line_count
  local -a before_args=()
  local -a after_args=()

  [[ -z "$before" ]] || read -r -a before_args <<< "$before"
  [[ -z "$after" ]] || read -r -a after_args <<< "$after"

  if [[ "$PROGRESS" -eq 1 ]]; then
    printf '[SCAN] %s [%s]\n' "$(sanitize_line "$rel")" "$(sanitize_line "$variant")" >&2
  fi

  logfile="$OUTDIR/logs/$(safe_log_name "${variant}-${rel}")"
  exit_code=0
  timeout -k 2s "${TIMEOUT_SECONDS}s" "$TOOL" "${before_args[@]}" "$input" "${after_args[@]}" > "$logfile" 2>&1 || exit_code=$?

  sanitizer="$(count_matches "$SANITIZER_RE" "$logfile")"
  signals="$(count_matches "$SIGNAL_RE" "$logfile")"
  errors="$(count_matches "$ERROR_RE" "$logfile")"
  warnings="$(count_matches "$WARNING_RE" "$logfile")"
  qa_fail_count="$(count_matches "$QA_FAIL_RE" "$logfile")"
  qa_warn_count="$(count_matches "$QA_WARN_RE" "$logfile")"
  qa_gap_count="$(count_matches "$QA_GAP_RE" "$logfile")"
  qa_not_run_count="$(count_matches "$QA_NOT_RUN_RE" "$logfile")"
  compliant_count="$(count_matches "$COMPLIANT_RE" "$logfile")"
  if [[ "$TOOL_NAME" == "iccPawgReport" ]]; then
    local pawg_counts
    pawg_counts="$(python3 "$SCRIPT_DIR/icc-pawg-qa-classify.py" "$variant" "$logfile")"
    read -r errors qa_fail_count qa_warn_count qa_gap_count qa_not_run_count <<< "$pawg_counts"
    warnings=0
    compliant_count=0
  fi
  line_count="$(wc -l < "$logfile" | tr -d '[:space:]')"
  issue_count=$((errors + warnings + qa_fail_count + qa_warn_count + qa_gap_count + qa_not_run_count))
  status="$(classify_exit "$exit_code" "$sanitizer" "$signals" "$issue_count")"

  case "$status" in
    PASS) pass=$((pass + 1)) ;;
    QA-ISSUE) qa_issue=$((qa_issue + 1)) ;;
    FAIL) fail=$((fail + 1)) ;;
    CRASH) crash=$((crash + 1)) ;;
    TIMEOUT) timeout_count=$((timeout_count + 1)) ;;
  esac
  if status_is_fail_on "$status"; then
    fail_on_count=$((fail_on_count + 1))
  fi

  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "$rel" "$variant" "$exit_code" "$status" "$sanitizer" "$signals" \
    "$errors" "$warnings" "$qa_fail_count" "$qa_warn_count" "$qa_gap_count" \
    "$qa_not_run_count" "$compliant_count" >> "$RESULTS"
  emit_findings "$rel" "$variant" "$logfile" "$status" "$exit_code" | tee -a "$FINDINGS"
  if [[ "$TOOL_NAME" == "iccPawgReport" && "$issue_count" -gt 0 ]]; then
    printf "[%s][%s] PAWG errors=%s FAIL=%s WARN=%s GAP=%s NOT RUN=%s (see log)\n" \
      "$(sanitize_line "$rel")" "$(sanitize_line "$variant")" "$errors" \
      "$qa_fail_count" "$qa_warn_count" "$qa_gap_count" "$qa_not_run_count" | tee -a "$FINDINGS"
  fi
  compact_logfile "$logfile" "$line_count"
}

total_files=0
total_runs=0
pass=0
qa_issue=0
fail=0
crash=0
timeout_count=0
fail_on_count=0

if [[ "$MAX_DEPTH" -eq 0 ]]; then
  FIND_CMD=(find "$INPUT_DIR" '(' -type f -o -type l ')' -print0)
else
  FIND_CMD=(find "$INPUT_DIR" -maxdepth "$MAX_DEPTH" '(' -type f -o -type l ')' -print0)
fi

while IFS= read -r -d '' input; do
  [[ -f "$input" ]] || continue
  if [[ "$MAX_FILES" -gt 0 && "$total_files" -ge "$MAX_FILES" ]]; then
    break
  fi
  total_files=$((total_files + 1))
  rel="${input#"$INPUT_DIR"/}"
  while IFS='|' read -r variant before after; do
    [[ -n "$variant" ]] || continue
    if [[ -n "$VARIANT_FILTER" && "$variant" != "$VARIANT_FILTER" ]]; then
      continue
    fi
    total_runs=$((total_runs + 1))
    run_variant "$input" "$rel" "$variant" "$before" "$after"
  done <<< "$VARIANTS"
done < <("${FIND_CMD[@]}" | sort -z)

printf '\n[SUMMARY] tool=%s files=%d runs=%d pass=%d qa_issue=%d fail=%d crash=%d timeout=%d\n' \
  "$TOOL_NAME" "$total_files" "$total_runs" "$pass" "$qa_issue" "$fail" "$crash" "$timeout_count"
printf '[SUMMARY] results=%s\n' "$RESULTS"
printf '[SUMMARY] findings=%s\n' "$FINDINGS"
write_summary_md
printf '[SUMMARY] markdown=%s\n' "$SUMMARY_MD"
printf '[SUMMARY] fail_on=%s count=%d\n' "$FAIL_ON" "$fail_on_count"
if [[ "$fail_on_count" -gt 0 ]]; then
  exit 1
fi
