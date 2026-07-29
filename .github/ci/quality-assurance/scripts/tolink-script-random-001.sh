#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/tolink-script-random-001.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -u

# Randomized QA driver for iccApplyToLink.
#
# Tool argv shape:
#   iccApplyToLink dst_link_file link_type lut_size option title range_min range_max
#     first_transform interp {{-ENV:sig value} profile_file_path rendering_intent
#     {-PCC connection_conditions_path}}

MODE="run"
OUT_FILE=""
TOTAL_TESTS=1000
START_AT=1
LOG_DIR="logs/tolink-random"
LINK_DIR="Links"
DRY_RUN=0
KEEP_GOING=1
SHOW_SANITIZER=0
SYMBOLIZER_PATH=""

usage() {
  cat <<'EOF'
Usage:
  ./tolink-script-random-001.sh [options] [profile_dir] [total_tests]

Modes:
  --count                         Print command-template count and exit.
  --generate FILE                 Write generated commands to FILE and exit.
  --dry-run                       Print generated commands instead of running.

Generation:
  --tests N                       Number of commands to generate/run. Default: 1000.
  --start-at N                    1-based command index. Default: 1.
  --link-dir DIR                  Output directory for generated links. Default: Links.
  --log-dir DIR                   Log directory. Default: logs/tolink-random.

Execution:
  --stop-on-fail                  Stop after the first failing command.
  --show-sanitizer                Echo sanitizer snippets live.
  --symbolizer PATH               Explicit llvm-symbolizer path.

Compatibility:
  profile_dir total_tests         Accepted for the older script interface. The
                                  current generator uses vetted hybrid profile
                                  chains; profile_dir is retained only so older
                                  invocations do not fail argument parsing.

Examples:
  ./tolink-script-random-001.sh --tests 20
  ./tolink-script-random-001.sh --generate tolink-commands.txt --tests 200
  ./tolink-script-random-001.sh --dry-run --tests 5
EOF
}

die() {
  echo "error: $*" >&2
  exit 2
}

SANITIZER_GREP='ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|AddressSanitizer:|UndefinedBehaviorSanitizer|runtime error:|SUMMARY: UndefinedBehaviorSanitizer|ERROR: LeakSanitizer|LeakSanitizer|DEADLYSIGNAL|SEGV|heap-use-after-free|stack-use-after-return|stack-use-after-scope|global-buffer-overflow|heap-buffer-overflow|stack-buffer-overflow|use-after-poison|double-free|bad-free|allocation-size-too-big|signed integer overflow|unsigned integer overflow|null pointer|misaligned address|shift exponent|shift-base|division by zero|out of bounds'

while [[ $# -gt 0 ]]; do
  case "$1" in
    --count) MODE="count"; shift ;;
    --generate) MODE="generate"; OUT_FILE="$2"; shift 2 ;;
    --tests=*) TOTAL_TESTS="${1#--tests=}"; shift ;;
    --tests) TOTAL_TESTS="$2"; shift 2 ;;
    --start-at=*) START_AT="${1#--start-at=}"; shift ;;
    --start-at) START_AT="$2"; shift 2 ;;
    --link-dir=*) LINK_DIR="${1#--link-dir=}"; shift ;;
    --link-dir) LINK_DIR="$2"; shift 2 ;;
    --log-dir=*) LOG_DIR="${1#--log-dir=}"; shift ;;
    --log-dir) LOG_DIR="$2"; shift 2 ;;
    --symbolizer) SYMBOLIZER_PATH="$2"; shift 2 ;;
    --show-sanitizer) SHOW_SANITIZER=1; shift ;;
    --stop-on-fail) KEEP_GOING=0; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --*) die "unknown option: $1" ;;
    *)
      # Backward-compatible positional args: profile_dir total_tests.
      if [[ -z "${PROFILE_DIR_SEEN:-}" ]]; then
        PROFILE_DIR_SEEN="$1"
      elif [[ -z "${TOTAL_TESTS_SEEN:-}" ]]; then
        TOTAL_TESTS="$1"
        TOTAL_TESTS_SEEN=1
      else
        die "unexpected positional argument: $1"
      fi
      shift
      ;;
  esac
done

[[ "$TOTAL_TESTS" =~ ^[0-9]+$ ]] || die "--tests must be numeric"
[[ "$START_AT" =~ ^[0-9]+$ ]] || die "--start-at must be numeric"
[[ "$TOTAL_TESTS" -gt 0 ]] || die "--tests must be greater than zero"
[[ "$START_AT" -gt 0 ]] || die "--start-at must be greater than zero"

LUT_SIZES=(2 3 4)
CUBE_PRECISIONS=(1 6 10 16)
RANGES=("0.0 1.0" "0 1" "0.001 0.999")
INTERPS=(0 1)

TEMPLATES=(
  "srgb_srgb|both|../sRGB_v4_ICC_preference.icc 0 ../sRGB_v4_ICC_preference.icc 0"
  "lab_srgb|both|ICC/Lab_float-D50_2deg.icc 3 ../sRGB_v4_ICC_preference.icc 1"
  "srgb_lab|both|../sRGB_v4_ICC_preference.icc 1 ICC/Lab_float-D50_2deg.icc 3"
  "cmyk_srgb|devlink|ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1"
  "cmyk_env_srgb|devlink|-ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1"
  "cmyk_pcc_srgb|devlink|ICC/CMYK-S_Overprint_Profile.icc 10001 -PCC ICC/Spec400_10_700-IllumA_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1"
)

MAX_TEMPLATES="${#TEMPLATES[@]}"

if [[ "$MODE" == "count" ]]; then
  echo "$MAX_TEMPLATES"
  exit 0
fi

[[ -z "$SYMBOLIZER_PATH" ]] && SYMBOLIZER_PATH="$(command -v llvm-symbolizer || true)"

required_profiles=(
  "../sRGB_v4_ICC_preference.icc"
  "ICC/Lab_float-D50_2deg.icc"
  "ICC/CMYK-S_Overprint_Profile.icc"
  "ICC/Spec400_10_700-IllumA_2deg-Abs.icc"
)

for profile in "${required_profiles[@]}"; do
  [[ -f "$profile" ]] || die "missing required profile: $profile; run ./BuildAndTest.sh first"
done

if [[ -x "../../Build/Tools/IccApplyToLink/iccApplyToLink" ]]; then
  PATH="$PWD/../../Build/Tools/IccApplyToLink:$PATH"
fi
command -v iccApplyToLink >/dev/null 2>&1 || die "iccApplyToLink not found on PATH"

pick() {
  local idx="$1"
  shift
  local values=("$@")
  printf '%s\n' "${values[$((idx % ${#values[@]}))]}"
}

emit_commands() {
  local wanted="$1"
  local start_at="$2"
  local emitted=0
  local idx tmpl name kind chain link_type ext option lut_size range range_min range_max first interp title out

  idx="$start_at"
  while [[ "$emitted" -lt "$wanted" ]]; do
    tmpl="${TEMPLATES[$(((idx - 1) % ${#TEMPLATES[@]}))]}"
    IFS='|' read -r name kind chain <<< "$tmpl"

    if [[ "$kind" == "both" && $((idx % 2)) -eq 0 ]]; then
      link_type=1
      ext="cube"
      option="$(pick "$idx" "${CUBE_PRECISIONS[@]}")"
    else
      link_type=0
      ext="icc"
      option=1
    fi

    lut_size="$(pick "$((idx / 2))" "${LUT_SIZES[@]}")"
    range="$(pick "$((idx / 3))" "${RANGES[@]}")"
    range_min="${range%% *}"
    range_max="${range##* }"
    first=1
    interp="$(pick "$((idx / 7))" "${INTERPS[@]}")"
    title="$(printf 'QA_ToLink_%05d_%s' "$idx" "$name")"
    out="$(printf '%s/qa_tolink_%05d_%s.%s' "$LINK_DIR" "$idx" "$name" "$ext")"

    printf 'iccApplyToLink %q %s %s %s %q %s %s %s %s %s\n' \
      "$out" "$link_type" "$lut_size" "$option" "$title" "$range_min" "$range_max" "$first" "$interp" "$chain"
    idx=$((idx + 1))
    emitted=$((emitted + 1))
  done
}

extract_sanitizer() {
  local prefix="$1"
  grep -E "$SANITIZER_GREP" "${prefix}.err" > "${prefix}.san" 2>/dev/null || true
}

run_one() {
  local idx="$1"
  local cmd="$2"
  local prefix rc

  if [[ "$DRY_RUN" -eq 1 ]]; then
    printf '%s\n' "$cmd"
    return 0
  fi

  mkdir -p "$LINK_DIR" "$LOG_DIR"
  prefix="$(printf '%s/%05d' "$LOG_DIR" "$idx")"
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
      echo "===== $idx ====="
      cat "${prefix}.cmd"
      cat "${prefix}.san"
      echo
    } >> "$LOG_DIR/sanitizer-summary.txt"
    [[ "$SHOW_SANITIZER" -eq 1 ]] && { echo "SANITIZER $idx:"; cat "${prefix}.san"; }
  fi

  if [[ "$rc" -eq 0 ]]; then
    echo "PASS $idx"
  else
    echo "FAIL $idx rc=$rc ; see ${prefix}.cmd ${prefix}.out ${prefix}.err ${prefix}.san"
    {
      printf '%05d rc=%s\n' "$idx" "$rc"
      cat "${prefix}.cmd"
      echo
    } >> "$LOG_DIR/failures.txt"
  fi

  return "$rc"
}

if [[ "$MODE" == "generate" ]]; then
  [[ -n "$OUT_FILE" ]] || die "--generate requires output file"
  emit_commands "$TOTAL_TESTS" "$START_AT" > "$OUT_FILE"
  echo "Wrote $OUT_FILE" >&2
  exit 0
fi

mkdir -p "$LINK_DIR" "$LOG_DIR"
: > "$LOG_DIR/sanitizer-summary.txt"
: > "$LOG_DIR/failures.txt"
: > "$LOG_DIR/replay.sh"
chmod +x "$LOG_DIR/replay.sh"

failures=0
current="$START_AT"
while IFS= read -r cmd; do
  printf '%s\n' "$cmd" >> "$LOG_DIR/replay.sh"
  if ! run_one "$current" "$cmd"; then
    failures=$((failures + 1))
    [[ "$KEEP_GOING" -eq 1 ]] || break
  fi
  current=$((current + 1))
done < <(emit_commands "$TOTAL_TESTS" "$START_AT")

echo "=== Done: failures=$failures ==="
echo "Sanitizer summary: $LOG_DIR/sanitizer-summary.txt"
echo "Failure summary:   $LOG_DIR/failures.txt"
[[ "$failures" -eq 0 ]]
