#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccApplyNamedCmm_ci_path_exercise.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -u

# iccApplyNamedCmm CI-aligned path exerciser.
#
# Tool argv shape:
#   iccApplyNamedCmm (-exportcfg|-exportcfganddata config_file_path}
#     data_file_path final_data_encoding{:FmtPrecision{:FmtDigits}} interpolation
#     {{-ENV:Name value} profile_file_path Rendering_intent
#     {-PCC connection_conditions_path}}

MUTATIONS=200
START_AT=1
LOG_DIR="icc-namedcmm-qa-logs"
MODE="run"
OUT_FILE=""
DRY_RUN=0
KEEP_GOING=1
REPLAY_CFG=1
SHOW_SANITIZER=0
SYMBOLIZER_PATH=""

usage() {
  cat <<'EOF'
Usage:
  ./iccApplyNamedCmm_ci_path_exercise.sh [options]

Modes:
  --count                         Print unique command-template count and exit.
  --generate FILE                 Write generated commands to FILE and exit.
  --dry-run                       Print commands instead of running.

Generation:
  --mutations N                   Number of commands. Default: 200.
  --start-at N                    1-based command index. Default: 1.
  --no-replay-cfg                 Do not replay generated JSON with -cfg.

Execution:
  --log-dir DIR                   Default: icc-namedcmm-qa-logs.
  --stop-on-fail                  Stop on first failure.
  --show-sanitizer                Echo sanitizer snippets live.
  --symbolizer PATH               Explicit llvm-symbolizer path.

Examples:
  ./iccApplyNamedCmm_ci_path_exercise.sh --count
  ./iccApplyNamedCmm_ci_path_exercise.sh --mutations 200
  ./iccApplyNamedCmm_ci_path_exercise.sh --generate iccApplyNamedCmm_QA_200.txt --mutations 200
EOF
}

die() {
  echo "error: $*" >&2
  exit 2
}

SANITIZER_GREP='ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|AddressSanitizer:|UndefinedBehaviorSanitizer|runtime error:|SUMMARY: UndefinedBehaviorSanitizer|ERROR: LeakSanitizer|LeakSanitizer|DEADLYSIGNAL|SEGV|heap-use-after-free|stack-use-after-return|stack-use-after-scope|global-buffer-overflow|heap-buffer-overflow|stack-buffer-overflow|use-after-poison|double-free|bad-free|allocation-size-too-big|signed integer overflow|unsigned integer overflow|null pointer|misaligned address|shift exponent|shift-base|division by zero|out of bounds'

ENCODINGS=(0 1 2 3 4 5 6 3:6:12 0:4 6:0:5)
INTERPS=(0 1)
MODES=(run exportcfg exportcfganddata)
CHAINS=(
  "base|ICC/CMYK_Hybrid_Profile.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3"
  "env|'-ENV:bkgX' 0.0985 '-ENV:bkgY' 0.159 '-ENV:bkgZ' 0.122 ICC/CMYK_Hybrid_Profile.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3"
  "pcc|ICC/CMYK_Hybrid_Profile.icc 10003 -PCC ICC/Spec400_10_700-IllumA_2deg-Abs.icc ICC/Spec380_10_730-D50_2deg.icc 3"
  "env_pcc|'-ENV:bkgX' 0.0985 '-ENV:bkgY' 0.159 '-ENV:bkgZ' 0.122 ICC/CMYK_Hybrid_Profile.icc 10003 -PCC ICC/Spec400_10_700-IllumA_2deg-Abs.icc ICC/Spec380_10_730-D50_2deg.icc 3"
)

while [[ $# -gt 0 ]]; do
  case "$1" in
    --count) MODE="count"; shift ;;
    --generate) MODE="generate"; OUT_FILE="$2"; shift 2 ;;
    --mutations=*) MUTATIONS="${1#--mutations=}"; shift ;;
    --mutations) MUTATIONS="$2"; shift 2 ;;
    --start-at=*) START_AT="${1#--start-at=}"; shift ;;
    --start-at) START_AT="$2"; shift 2 ;;
    --log-dir=*) LOG_DIR="${1#--log-dir=}"; shift ;;
    --log-dir) LOG_DIR="$2"; shift 2 ;;
    --symbolizer) SYMBOLIZER_PATH="$2"; shift 2 ;;
    --no-replay-cfg) REPLAY_CFG=0; shift ;;
    --show-sanitizer) SHOW_SANITIZER=1; shift ;;
    --stop-on-fail) KEEP_GOING=0; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

[[ "$MUTATIONS" =~ ^[0-9]+$ ]] || die "--mutations must be numeric"
[[ "$START_AT" =~ ^[0-9]+$ ]] || die "--start-at must be numeric"
[[ "$MUTATIONS" -gt 0 ]] || die "--mutations must be greater than zero"
[[ "$START_AT" -gt 0 ]] || die "--start-at must be greater than zero"

MAX_TEMPLATES=$(( ${#ENCODINGS[@]} * ${#INTERPS[@]} * ${#MODES[@]} * ${#CHAINS[@]} ))
if [[ "$MODE" == "count" ]]; then
  echo "$MAX_TEMPLATES"
  exit 0
fi

[[ -z "$SYMBOLIZER_PATH" ]] && SYMBOLIZER_PATH="$(command -v llvm-symbolizer || true)"

for required in Data/cmykGrays.txt ICC/CMYK_Hybrid_Profile.icc ICC/Spec380_10_730-D50_2deg.icc ICC/Spec400_10_700-IllumA_2deg-Abs.icc; do
  [[ -f "$required" ]] || die "missing required input: $required; run ./BuildAndTest.sh first"
done

if [[ -x "../../Build/Tools/IccApplyNamedCmm/iccApplyNamedCmm" ]]; then
  PATH="$PWD/../../Build/Tools/IccApplyNamedCmm:$PATH"
fi
command -v iccApplyNamedCmm >/dev/null 2>&1 || die "iccApplyNamedCmm not found on PATH"

pick() {
  local idx="$1"
  shift
  local values=("$@")
  printf '%s\n' "${values[$((idx % ${#values[@]}))]}"
}

emit_commands() {
  local wanted="$1" start_at="$2"
  local emitted=0 idx enc interp mode chain_spec chain_name chain tag config result

  idx="$start_at"
  while [[ "$emitted" -lt "$wanted" ]]; do
    enc="$(pick "$idx" "${ENCODINGS[@]}")"
    interp="$(pick "$((idx / ${#ENCODINGS[@]}))" "${INTERPS[@]}")"
    mode="$(pick "$((idx / (${#ENCODINGS[@]} * ${#INTERPS[@]})))" "${MODES[@]}")"
    chain_spec="$(pick "$((idx / (${#ENCODINGS[@]} * ${#INTERPS[@]} * ${#MODES[@]})))" "${CHAINS[@]}")"
    chain_name="${chain_spec%%|*}"
    chain="${chain_spec#*|}"
    tag="$(printf 'qa_namedcmm_%05d_%s_enc%s_i%s' "$idx" "$chain_name" "${enc//:/_}" "$interp")"
    config="config/${tag}.json"
    result="Results/${tag}.txt"

    case "$mode" in
      run)
        printf 'iccApplyNamedCmm Data/cmykGrays.txt %s %s %s > %q\n' "$enc" "$interp" "$chain" "$result"
        ;;
      exportcfg)
        printf 'iccApplyNamedCmm -exportcfg %q Data/cmykGrays.txt %s %s %s\n' "$config" "$enc" "$interp" "$chain"
        ;;
      exportcfganddata)
        printf 'iccApplyNamedCmm -exportcfganddata %q Data/cmykGrays.txt %s %s %s > %q\n' "$config" "$enc" "$interp" "$chain" "$result"
        ;;
    esac

    idx=$((idx + 1))
    emitted=$((emitted + 1))
  done
}

cmd_config_path() {
  awk '{
    for (i=1; i<=NF; i++) {
      if (($i == "-exportcfg" || $i == "-exportcfganddata") && i<NF) {
        print $(i+1)
        exit
      }
    }
  }' <<< "$1"
}

extract_sanitizer() {
  local prefix="$1"
  mkdir -p "$(dirname "$prefix")"
  grep -E "$SANITIZER_GREP" "${prefix}.err" > "${prefix}.san" 2>/dev/null || true
}

run_cmd() {
  local idx="$1" phase="$2" cmd="$3"
  local prefix rc
  prefix="$(printf '%s/%05d.%s' "$LOG_DIR" "$idx" "$phase")"

  if [[ "$DRY_RUN" -eq 1 ]]; then
    printf '%s\n' "$cmd"
    return 0
  fi

  mkdir -p config Results "$LOG_DIR"
  printf '%s\n' "$cmd" > "${prefix}.cmd"

  local asan_opts="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=1:detect_leaks=0:abort_on_error=1:symbolize=1}"
  local ubsan_opts="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1:symbolize=1}"

  if [[ -n "$SYMBOLIZER_PATH" ]]; then
    ASAN_SYMBOLIZER_PATH="$SYMBOLIZER_PATH" UBSAN_SYMBOLIZER_PATH="$SYMBOLIZER_PATH" \
    ASAN_OPTIONS="$asan_opts" UBSAN_OPTIONS="$ubsan_opts" \
    bash -lc "$cmd" > "${prefix}.out" 2> "${prefix}.err"
    rc=$?
  else
    ASAN_OPTIONS="$asan_opts" UBSAN_OPTIONS="$ubsan_opts" \
    bash -lc "$cmd" > "${prefix}.out" 2> "${prefix}.err"
    rc=$?
  fi

  extract_sanitizer "$prefix"
  if [[ -s "${prefix}.san" ]]; then
    {
      echo "===== $idx $phase ====="
      cat "${prefix}.cmd"
      cat "${prefix}.san"
      echo
    } >> "$LOG_DIR/sanitizer-summary.txt"
    [[ "$SHOW_SANITIZER" -eq 1 ]] && { echo "SANITIZER $idx $phase:"; cat "${prefix}.san"; }
  fi

  if [[ "$rc" -eq 0 ]]; then
    echo "PASS $idx $phase"
  else
    echo "FAIL $idx $phase rc=$rc ; see ${prefix}.cmd ${prefix}.out ${prefix}.err ${prefix}.san"
    {
      printf '%05d %s rc=%s\n' "$idx" "$phase" "$rc"
      cat "${prefix}.cmd"
      echo
    } >> "$LOG_DIR/failures.txt"
  fi

  return "$rc"
}

run_one() {
  local idx="$1" cmd="$2"
  local rc=0 cfg

  run_cmd "$idx" "run" "$cmd" || rc=$?
  if [[ "$REPLAY_CFG" -eq 1 ]]; then
    cfg="$(cmd_config_path "$cmd")"
    if [[ -n "$cfg" && -f "$cfg" ]]; then
      run_cmd "$idx" "cfg" "iccApplyNamedCmm -cfg $cfg" || rc=$?
    elif [[ -n "$cfg" && "$DRY_RUN" -ne 1 ]]; then
      echo "SKIP $idx cfg ; config not generated: $cfg"
    fi
  fi

  return "$rc"
}

if [[ "$MODE" == "generate" ]]; then
  [[ -n "$OUT_FILE" ]] || die "--generate requires output file"
  emit_commands "$MUTATIONS" "$START_AT" > "$OUT_FILE"
  echo "Wrote $OUT_FILE" >&2
  exit 0
fi

mkdir -p "$LOG_DIR"
: > "$LOG_DIR/sanitizer-summary.txt"
: > "$LOG_DIR/failures.txt"

failures=0
current="$START_AT"
while IFS= read -r cmd; do
  if ! run_one "$current" "$cmd"; then
    failures=$((failures + 1))
    [[ "$KEEP_GOING" -eq 1 ]] || break
  fi
  current=$((current + 1))
done < <(emit_commands "$MUTATIONS" "$START_AT")

echo "=== Done: failures=$failures ==="
echo "Sanitizer summary: $LOG_DIR/sanitizer-summary.txt"
echo "Failure summary:   $LOG_DIR/failures.txt"
[[ "$failures" -eq 0 ]]
