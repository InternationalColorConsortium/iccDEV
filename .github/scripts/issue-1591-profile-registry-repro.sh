#!/usr/bin/env bash
###############################################################################
# Issue #1591 ICC profile registry reproduction helper.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

RESULT_DIR="${ISSUE_1591_RESULT_DIR:-$REPO_ROOT/Build/issue-1591-profile-registry}"
PROFILE_LIMIT="${ISSUE_1591_PROFILE_LIMIT:-0}"
INCLUDE_LOCAL_FIXTURES="${ISSUE_1591_INCLUDE_LOCAL_FIXTURES:-false}"
STRICT_TOOL_FAILURES="${ISSUE_1591_STRICT_TOOL_FAILURES:-false}"
COMMAND_TIMEOUT="${ISSUE_1591_COMMAND_TIMEOUT:-180}"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
LIVE_LOG_MODE="${ISSUE_1591_LIVE_LOG_MODE:-key}"
LIVE_LOG_INTERVAL="${ISSUE_1591_LIVE_LOG_INTERVAL:-15}"
LIVE_LOG_LINES="${ISSUE_1591_LIVE_LOG_LINES:-80}"
PARALLEL_JOBS="${ISSUE_1591_PARALLEL_JOBS:-4}"

case "$LIVE_LOG_MODE" in
  key|full|quiet)
    ;;
  *)
    echo "[FAIL] ISSUE_1591_LIVE_LOG_MODE must be key, full, or quiet"
    exit 2
    ;;
esac
if ! [[ "$LIVE_LOG_INTERVAL" =~ ^[1-9][0-9]*$ ]]; then
  echo "[FAIL] ISSUE_1591_LIVE_LOG_INTERVAL must be a positive integer"
  exit 2
fi
if ! [[ "$LIVE_LOG_LINES" =~ ^[1-9][0-9]*$ ]]; then
  echo "[FAIL] ISSUE_1591_LIVE_LOG_LINES must be a positive integer"
  exit 2
fi
if ! [[ "$PARALLEL_JOBS" =~ ^[1-9][0-9]*$ ]]; then
  echo "[FAIL] ISSUE_1591_PARALLEL_JOBS must be a positive integer"
  exit 2
fi

DOWNLOAD_DIR="$RESULT_DIR/downloads"
PROFILE_DIR="$RESULT_DIR/profiles"
CHARDATA_DIR="$RESULT_DIR/chardata"
GENERATED_DIR="$RESULT_DIR/generated"
LOG_DIR="$RESULT_DIR/logs"
REPORT_DIR="$RESULT_DIR/reports"
SUMMARY_TSV="$REPORT_DIR/issue-1591-results.tsv"
DOWNLOAD_TSV="$REPORT_DIR/downloads.tsv"
PREPARE_TSV="$REPORT_DIR/prepopulated-targets.tsv"

if [ -f "$SCRIPT_DIR/sanitize-sed.sh" ]; then
  # shellcheck disable=SC1091
  source "$SCRIPT_DIR/sanitize-sed.sh"
fi

sanitize_for_summary()
{
  if declare -F sanitize_line >/dev/null 2>&1; then
    sanitize_line "$1"
  else
    printf "%s" "$1" | tr -d "\000-\011\013\014\016-\037\177"
  fi
}

safe_name()
{
  local name="$1"
  if declare -F sanitize_filename >/dev/null 2>&1; then
    sanitize_filename "$name"
  else
    printf "%s" "$name" | LC_ALL=C sed -E 's#[^A-Za-z0-9._-]#_#g; s#_+#_#g; s#^_+##; s#_+$##'
  fi
}

find_tool()
{
  local tool_name="$1"
  find "$TOOLS_DIR" -maxdepth 3 -type f -name "$tool_name" 2>/dev/null | sed -n '1p'
}

count_matches()
{
  local pattern="$1"
  local file="$2"
  local count
  if [ ! -f "$file" ]; then
    printf "0"
    return 0
  fi
  count="$(grep -Eaci "$pattern" "$file" 2>/dev/null || true)"
  printf "%s" "$count" | tr -d ' '
}

progress()
{
  printf "[issue-1591] %s %s\n" "$(date -u "+%Y-%m-%dT%H:%M:%SZ")" "$*"
}

wait_for_slot()
{
  while [ "$(jobs -pr | wc -l | tr -d ' ')" -ge "$PARALLEL_JOBS" ]; do
    wait -n || true
  done
}

emit_log_excerpt()
{
  local raw_log="$1"
  local mode="$2"
  local max_lines="$3"

  if [ ! -s "$raw_log" ] || [ "$mode" = "quiet" ]; then
    return 0
  fi

  if [ "$mode" = "full" ]; then
    sed -n "1,${max_lines}p" "$raw_log"
    return 0
  fi

  grep -Eai "Warning!|NonCompliant!|Error!|Unable to Parse|JSON file exceeds|PCSValues contains non-numeric|parser error|runtime error|AddressSanitizer|UndefinedBehaviorSanitizer|SUMMARY:|SCARINESS:" \
    "$raw_log" | tail -n "$max_lines" || true
}

record_result()
{
  local phase="$1"
  local profile="$2"
  local status="$3"
  local raw_log="$4"
  local output_path="$5"
  local warnings errors noncompliant sanitizer bytes

  warnings="$(count_matches 'Warning!|warning:' "$raw_log")"
  errors="$(count_matches 'Error!|ERROR:|error :|Unable to Parse|JSON file exceeds|runtime error:' "$raw_log")"
  noncompliant="$(count_matches 'NonCompliant!' "$raw_log")"
  sanitizer="$(count_matches 'AddressSanitizer|UndefinedBehaviorSanitizer|SUMMARY:|SCARINESS:' "$raw_log")"
  bytes=0
  if [ -f "$output_path" ]; then
    bytes="$(wc -c < "$output_path" | tr -d ' ')"
  fi

  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "$phase" "$profile" "$status" "$warnings" "$errors" "$noncompliant" "$sanitizer" "$bytes" "$raw_log" >> "$SUMMARY_TSV"
}

append_prefixed_log()
{
  local profile="$1"
  local raw_log="$2"
  local combined_log="$3"
  local line=""

  while IFS= read -r line || [ -n "${line:-}" ]; do
    printf "[%s] %s\n" "$profile" "${line-}"
  done < "$raw_log" >> "$combined_log"
}

run_tool()
{
  local phase="$1"
  local profile_path="$2"
  local output_path="$3"
  local tool_path="$4"
  shift 4

  local profile_name phase_dir raw_log combined_log status pid log_bytes
  profile_name="$(basename "$profile_path")"
  phase_dir="$LOG_DIR/$phase"
  raw_log="$phase_dir/${profile_name}.log"
  combined_log="$LOG_DIR/${phase}.combined.log"
  mkdir -p "$phase_dir"

  if [ -z "$tool_path" ] || [ ! -x "$tool_path" ]; then
    printf "missing tool for phase %s\n" "$phase" > "$raw_log"
    append_prefixed_log "$profile_name" "$raw_log" "$combined_log"
    record_result "$phase" "$profile_name" "127" "$raw_log" "$output_path"
    progress "MISSING_TOOL phase=$phase profile=$profile_name log=$raw_log"
    return 0
  fi

  progress "START phase=$phase profile=$profile_name timeout=${COMMAND_TIMEOUT}s"
  set +e
  timeout "$COMMAND_TIMEOUT" "$tool_path" "$@" > "$raw_log" 2>&1 &
  pid=$!
  while kill -0 "$pid" 2>/dev/null; do
    sleep "$LIVE_LOG_INTERVAL"
    if kill -0 "$pid" 2>/dev/null; then
      log_bytes=0
      if [ -f "$raw_log" ]; then
        log_bytes="$(wc -c < "$raw_log" | tr -d ' ')"
      fi
      progress "RUNNING phase=$phase profile=$profile_name log_bytes=$log_bytes"
      emit_log_excerpt "$raw_log" "$LIVE_LOG_MODE" "$LIVE_LOG_LINES"
    fi
  done
  wait "$pid"
  status=$?
  set -e

  append_prefixed_log "$profile_name" "$raw_log" "$combined_log"
  record_result "$phase" "$profile_name" "$status" "$raw_log" "$output_path"
  progress "DONE phase=$phase profile=$profile_name status=$status log=$raw_log"
  emit_log_excerpt "$raw_log" "$LIVE_LOG_MODE" "$LIVE_LOG_LINES"
}

prepare_target()
{
  local phase="$1"
  local profile_path="$2"
  local output_path="$3"
  local tool_path="$4"
  shift 4

  local profile_name phase_dir raw_log status bytes
  profile_name="$(basename "$profile_path")"
  phase_dir="$LOG_DIR/$phase"
  raw_log="$phase_dir/${profile_name}.log"
  mkdir -p "$phase_dir" "$(dirname "$output_path")"

  if [ -z "$tool_path" ] || [ ! -x "$tool_path" ]; then
    printf "missing tool for phase %s\n" "$phase" > "$raw_log"
    printf "%s\t%s\t127\t0\t%s\n" "$phase" "$profile_name" "$raw_log" >> "$PREPARE_TSV"
    progress "PREPOPULATE_MISSING_TOOL phase=$phase profile=$profile_name log=$raw_log"
    return 0
  fi

  progress "PREPOPULATE_START phase=$phase profile=$profile_name"
  set +e
  timeout "$COMMAND_TIMEOUT" "$tool_path" "$@" > "$raw_log" 2>&1
  status=$?
  set -e

  bytes=0
  if [ -f "$output_path" ]; then
    bytes="$(wc -c < "$output_path" | tr -d ' ')"
  fi
  printf "%s\t%s\t%s\t%s\t%s\n" "$phase" "$profile_name" "$status" "$bytes" "$raw_log" >> "$PREPARE_TSV"
  progress "PREPOPULATE_DONE phase=$phase profile=$profile_name status=$status bytes=$bytes log=$raw_log"
  emit_log_excerpt "$raw_log" "$LIVE_LOG_MODE" "$LIVE_LOG_LINES"
}

download_one()
{
  local url="$1"
  local dest_dir="$2"
  local name status bytes

  name="$(safe_name "$(basename "$url")")"
  mkdir -p "$dest_dir"

  progress "DOWNLOAD_START name=$name url=$url"
  set +e
  curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 300 \
    -o "$dest_dir/$name" "$url" > "$LOG_DIR/download-${name}.log" 2>&1
  status=$?
  set -e

  bytes=0
  if [ -f "$dest_dir/$name" ]; then
    bytes="$(wc -c < "$dest_dir/$name" | tr -d ' ')"
  fi
  printf "%s\t%s\t%s\t%s\n" "$status" "$bytes" "$name" "$url" >> "$DOWNLOAD_TSV"
  progress "DOWNLOAD_DONE name=$name status=$status bytes=$bytes"
  emit_log_excerpt "$LOG_DIR/download-${name}.log" "$LIVE_LOG_MODE" "$LIVE_LOG_LINES"
}

mkdir -p "$DOWNLOAD_DIR" "$PROFILE_DIR" "$CHARDATA_DIR" "$GENERATED_DIR" "$LOG_DIR" "$REPORT_DIR"
rm -f "$SUMMARY_TSV" "$DOWNLOAD_TSV" "$PREPARE_TSV"
printf "phase\tprofile\tstatus\twarnings\terrors\tnoncompliant\tsanitizer\toutput_bytes\tlog\n" > "$SUMMARY_TSV"
printf "status\tbytes\tname\turl\n" > "$DOWNLOAD_TSV"
printf "phase\tprofile\tstatus\toutput_bytes\tlog\n" > "$PREPARE_TSV"

registry_urls=(
  "https://registry.color.org/profile-registry/profiles/APTEC_CMYKOGV_Coated_LinearCTV_2025.icc"
  "https://registry.color.org/profile-registry/profiles/APTEC_Flexo_Coated_LinearCTV_2025.icc"
  "https://registry.color.org/profile-registry/profiles/APTEC_Flexo_Label_LinearCTV_2025.icc"
  "https://registry.color.org/profile-registry/profiles/APTEC_Flexo_PVC_LinearCTV_2025.icc"
  "https://registry.color.org/profile-registry/profiles/APTEC_Offset_Uncoated_LinearCTV_2025.icc"
  "https://registry.color.org/profile-registry/profiles/APTEC_Offset_Coated_LinearCTV_2025.icc"
  "https://registry.color.org/profile-registry/chardata/APTEC_PC10_CardBoard_2023_v1.txt"
  "https://registry.color.org/profile-registry/profiles/APTEC_PC11_CCNB_2023_v1.icc"
  "https://registry.color.org/profile-registry/profiles/PSOcoated_v3.icc"
  "https://registry.color.org/profile-registry/profiles/JapanColor2011Coated.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC7.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC6.icc"
  "https://registry.color.org/profile-registry/profiles/GRACoL2013_CRPC6.icc"
  "https://registry.color.org/profile-registry/profiles/GRACoL2006_Coated1v2.icc"
  "https://registry.color.org/profile-registry/profiles/Coated_Fogra39L_VIGC_300.icc"
  "https://registry.color.org/profile-registry/profiles/Coated_Fogra39L_VIGC_260.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC5.icc"
  "https://registry.color.org/profile-registry/profiles/SWOP2006_Coated3v2.icc"
  "https://registry.color.org/profile-registry/profiles/PSOuncoated_v3_FOGRA52.icc"
  "https://registry.color.org/profile-registry/profiles/Uncoated_Fogra47L_VIGC_260.icc"
  "https://registry.color.org/profile-registry/profiles/Uncoated_Fogra47L_VIGC_300.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC4.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC3.icc"
  "https://registry.color.org/profile-registry/profiles/GRACoL2013UNC_CRPC3.icc"
  "https://registry.color.org/profile-registry/profiles/SWOP2006_Coated5v2.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC2.icc"
  "https://registry.color.org/profile-registry/profiles/PSOsc-b_paper_v3_FOGRA54.icc"
  "https://registry.color.org/profile-registry/profiles/SC_paper_eci.icc"
  "https://registry.color.org/profile-registry/profiles/SNAP2007.icc"
  "https://registry.color.org/profile-registry/profiles/CGATS21_CRPC1.icc"
)

icc_count=0
for url in "${registry_urls[@]}"; do
  case "$url" in
    *.icc)
      if [ "$PROFILE_LIMIT" != "0" ] && [ "$icc_count" -ge "$PROFILE_LIMIT" ]; then
        continue
      fi
      wait_for_slot
      download_one "$url" "$DOWNLOAD_DIR" &
      icc_count=$((icc_count + 1))
      ;;
    *)
      wait_for_slot
      download_one "$url" "$CHARDATA_DIR" &
      ;;
  esac
done
wait

find "$DOWNLOAD_DIR" -maxdepth 1 -type f -name "*.icc" -exec cp {} "$PROFILE_DIR/" \;

if [ "$INCLUDE_LOCAL_FIXTURES" = "true" ]; then
  local_fixtures=(
    "added-bytes.icc"
    "bad-CMM.icc"
    "bad-illuminant.icc"
    "max-redcurvevalue.icc"
    "missing-required.icc"
    "sRGB_v4_ICC_preference.icc"
    "version-unknown.icc"
    "zero-tags.icc"
  )
  for fixture in "${local_fixtures[@]}"; do
    found="$(find "$REPO_ROOT/Testing" -type f -name "$fixture" 2>/dev/null | sed -n '1p')"
    if [ -n "$found" ]; then
      cp "$found" "$PROFILE_DIR/$fixture"
    fi
  done
fi

profile_total="$(find "$PROFILE_DIR" -maxdepth 1 -type f -name "*.icc" | wc -l | tr -d ' ')"
if [ "$profile_total" -eq 0 ]; then
  echo "[FAIL] no ICC profiles available for issue #1591 reproduction"
  exit 1
fi
progress "PROFILE_SET profiles=$profile_total include_local_fixtures=$INCLUDE_LOCAL_FIXTURES live_log_mode=$LIVE_LOG_MODE parallel_jobs=$PARALLEL_JOBS"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/Build/build-issue-1591/Tools" "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT=""
if BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"; then
  :
fi
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

DUMP_TOOL="$(find_tool iccDumpProfile)"
ROUNDTRIP_TOOL="$(find_tool iccRoundTrip)"
PAWG_TOOL="$(find_tool iccPawgReport)"
TOXML_TOOL="$(find_tool iccToXml)"
TOJSON_TOOL="$(find_tool iccToJson)"
FROMXML_TOOL="$(find_tool iccFromXml)"
FROMJSON_TOOL="$(find_tool iccFromJson)"

mkdir -p "$GENERATED_DIR/xml" "$GENERATED_DIR/json" "$GENERATED_DIR/fromxml" "$GENERATED_DIR/fromjson"

progress "PREPOPULATE_TARGETS_START profiles=$profile_total"
while IFS= read -r profile; do
  base="$(basename "$profile")"
  xml_out="$GENERATED_DIR/xml/${base}.xml"
  json_out="$GENERATED_DIR/json/${base}.json"

  wait_for_slot
  prepare_target "prepopulate-xml" "$profile" "$xml_out" "$TOXML_TOOL" "$profile" "$xml_out" &
  wait_for_slot
  prepare_target "prepopulate-json" "$profile" "$json_out" "$TOJSON_TOOL" "$profile" "$json_out" &
done < <(find "$PROFILE_DIR" -maxdepth 1 -type f -name "*.icc" | sort)
wait
progress "PREPOPULATE_TARGETS_DONE"

run_profile_repro()
{
  local profile="$1"
  local base xml_out json_out fromxml_out fromjson_out missing_log

  base="$(basename "$profile")"
  xml_out="$GENERATED_DIR/xml/${base}.xml"
  json_out="$GENERATED_DIR/json/${base}.json"
  fromxml_out="$GENERATED_DIR/fromxml/${base}.xml.icc"
  fromjson_out="$GENERATED_DIR/fromjson/${base}.json.icc"

  progress "PROFILE_START profile=$base"
  run_tool "dump-all" "$profile" "$REPORT_DIR/${base}.dump.marker" "$DUMP_TOOL" -v 100 "$profile" ALL
  run_tool "pawg-report" "$profile" "$REPORT_DIR/${base}.pawg.marker" "$PAWG_TOOL" "$profile"
  run_tool "roundtrip" "$profile" "$REPORT_DIR/${base}.roundtrip.marker" "$ROUNDTRIP_TOOL" "$profile"

  if [ -f "$xml_out" ]; then
    run_tool "from-xml" "$xml_out" "$fromxml_out" "$FROMXML_TOOL" "$xml_out" "$fromxml_out"
  else
    missing_log="$LOG_DIR/from-xml/${base}.xml.log"
    mkdir -p "$(dirname "$missing_log")"
    printf "missing XML output: %s\n" "$xml_out" > "$missing_log"
    append_prefixed_log "${base}.xml" "$missing_log" "$LOG_DIR/from-xml.combined.log"
    record_result "from-xml" "${base}.xml" "127" "$missing_log" "$fromxml_out"
  fi

  if [ -f "$json_out" ]; then
    run_tool "from-json" "$json_out" "$fromjson_out" "$FROMJSON_TOOL" "$json_out" "$fromjson_out"
  else
    missing_log="$LOG_DIR/from-json/${base}.json.log"
    mkdir -p "$(dirname "$missing_log")"
    printf "missing JSON output: %s\n" "$json_out" > "$missing_log"
    append_prefixed_log "${base}.json" "$missing_log" "$LOG_DIR/from-json.combined.log"
    record_result "from-json" "${base}.json" "127" "$missing_log" "$fromjson_out"
  fi
  progress "PROFILE_DONE profile=$base"
}

while IFS= read -r profile; do
  wait_for_slot
  run_profile_repro "$profile" &
done < <(find "$PROFILE_DIR" -maxdepth 1 -type f -name "*.icc" | sort)
wait

grep -Eai "point" "$LOG_DIR/dump-all.combined.log" > "$REPORT_DIR/dump-point-lines.log" || true
grep -Eai "zip" "$LOG_DIR/dump-all.combined.log" > "$REPORT_DIR/dump-zip-lines.log" || true
grep -Eai "Sani|Warning!|NonCompliant!|Error!" "$LOG_DIR/pawg-report.combined.log" > "$REPORT_DIR/pawg-key-lines.log" || true
grep -Eai "Warning!|NonCompliant!|Error!|Unable to Parse|JSON file exceeds|PCSValues contains non-numeric|parser error|runtime error|AddressSanitizer|UndefinedBehaviorSanitizer|SUMMARY:" \
  "$LOG_DIR"/*.combined.log > "$REPORT_DIR/warnings-errors-sanitizers.log" || true

tool_failures="$(awk -F '\t' 'NR > 1 && $3 != "0" { count++ } END { print count + 0 }' "$SUMMARY_TSV")"
warning_lines="$(wc -l < "$REPORT_DIR/warnings-errors-sanitizers.log" | tr -d ' ')"

{
  printf "issue: 1591\n"
  printf "profiles: %s\n" "$profile_total"
  printf "tool_failures: %s\n" "$tool_failures"
  printf "warning_error_sanitizer_lines: %s\n" "$warning_lines"
  printf "results_tsv: %s\n" "$SUMMARY_TSV"
  printf "download_tsv: %s\n" "$DOWNLOAD_TSV"
  printf "prepopulated_targets_tsv: %s\n" "$PREPARE_TSV"
  printf "logs: %s\n" "$LOG_DIR"
  printf "generated: %s\n" "$GENERATED_DIR"
} > "$REPORT_DIR/summary.txt"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    printf "### Issue #1591 Profile Registry Reproduction\n\n"
    printf "| Field | Value |\n"
    printf "|-------|-------|\n"
    printf "| Profiles | %s |\n" "$(sanitize_for_summary "$profile_total")"
    printf "| Tool non-zero exits | %s |\n" "$(sanitize_for_summary "$tool_failures")"
    printf "| Warning/error/sanitizer lines | %s |\n" "$(sanitize_for_summary "$warning_lines")"
    printf "| Results TSV | %s |\n" "$(sanitize_for_summary "$SUMMARY_TSV")"
    printf "| Full logs | %s |\n\n" "$(sanitize_for_summary "$LOG_DIR")"
    printf "Top warning/error/sanitizer lines:\n\n"
    printf '```\n'
    while IFS= read -r summary_line; do
      sanitize_for_summary "${summary_line-}"
      printf "\n"
    done < <(sed -n '1,80p' "$REPORT_DIR/warnings-errors-sanitizers.log")
    printf '```\n'
  } >> "$GITHUB_STEP_SUMMARY"
fi

echo "[OK] issue #1591 reproduction complete"
cat "$REPORT_DIR/summary.txt"

if [ "$STRICT_TOOL_FAILURES" = "true" ] && [ "$tool_failures" -gt 0 ]; then
  echo "[FAIL] strict mode: tool failures detected"
  exit 1
fi
