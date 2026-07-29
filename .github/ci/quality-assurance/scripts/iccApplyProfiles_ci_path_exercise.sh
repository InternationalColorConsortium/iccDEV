#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccApplyProfiles_ci_path_exercise.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -u

# iccApplyProfiles CI-aligned path exerciser.
#
# This generator is based on the actual CI command shapes:
#   Phase 2: embedded spectral source -> MultSpectralRGB
#   Phase 3: embedded source/MS + PCC -> sRGB
#   Phase 6: T-shirt overprint simulation, including:
#       - embedded source transform
#       - direct overprint profile transform
#       - multi-profile MCS/mid-profile chains
#       - -ENV:sig value before profile stages
#
# It exercises documented iccApplyProfiles usage:
#   iccApplyProfiles {-threads N} -cfg config_file
#   iccApplyProfiles {-threads N} {-exportcfg config_file}
#     src_tiff dst_tiff enc comp planar embed interpolation
#     {{-ENV:sig value} profile rendering_intent {-PCC connection_conditions}}
#
# Default:
#   generate/run 1000 exportcfg mutations and replay successful generated configs with -cfg.

MUTATIONS=1000
START_AT=1
JOBS=1
LOG_DIR="icc-ci-path-logs"
MODE="run"                 # run | generate | count
OUT_FILE=""
DRY_RUN=0
KEEP_GOING=1
REPLAY_CFG=1
SHOW_SANITIZER=0
SYMBOLIZER_PATH=""
PCC_OVERRIDE=""
ICC_OVERRIDE=""
INCLUDE_CASE_VARIANTS=0    # default off because CI uses lowercase -embedded/-pcc
INCLUDE_RISKY_SCALARS=1
INCLUDE_ENV_MUTATIONS=1

usage() {
  cat <<'EOF'
Usage:
  ./iccApplyProfiles_ci_path_exercise.sh [options]

Modes:
  --count                         Print computed mutation-space size and exit.
  --generate FILE                 Write generated exportcfg commands to FILE and exit.
  --dry-run                       Print commands instead of running.

Generation:
  --mutations N                   Number of mutations. Default: 1000.
                                  Use --mutations=max for full computed space.
  --start-at N                    1-based generated mutation index. Default: 1.
  --no-replay-cfg                 Do not replay generated JSON with -cfg.
  --case-variants                 Also test -EMBEDDED/-PCC spelling variants.
  --no-risky-scalars              Keep scalar image-output args closer to CI defaults.
  --no-env-mutations              Use only CI environment values, no extra env values.

Execution:
  --jobs N                        Parallel jobs. Default: 1.
  --log-dir DIR                   Default: icc-ci-path-logs.
  --stop-on-fail                  Stop on first failure in serial mode.
  --show-sanitizer                Echo sanitizer snippets live.

Overrides:
  --pcc-directory DIR             Replace PCC connection condition files with *.icc from DIR, cycling by index.
  --pcc-profile FILE              Replace PCC connection condition file with FILE.
  --icc-profile FILE              Replace final output ICC profile before final rendering_intent.
  --icc-profile-index N           Replace final output ICC profile with Nth *.icc found under cwd.

Sanitizer:
  --symbolizer PATH               Explicit llvm-symbolizer path.

Examples:
  ./iccApplyProfiles_ci_path_exercise.sh --count
  ./iccApplyProfiles_ci_path_exercise.sh --mutations=1000 --jobs 8
  ./iccApplyProfiles_ci_path_exercise.sh --generate ci-path-cmds.txt --mutations=5000
  ./iccApplyProfiles_ci_path_exercise.sh --mutations=max --no-risky-scalars
  ./iccApplyProfiles_ci_path_exercise.sh --pcc-directory ../specified/directory --mutations=2000
EOF
}

die() { echo "error: $*" >&2; exit 2; }

SANITIZER_GREP='ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|AddressSanitizer:|UndefinedBehaviorSanitizer|runtime error:|SUMMARY: UndefinedBehaviorSanitizer|ERROR: LeakSanitizer|LeakSanitizer|DEADLYSIGNAL|SEGV|heap-use-after-free|stack-use-after-return|stack-use-after-scope|global-buffer-overflow|heap-buffer-overflow|stack-buffer-overflow|use-after-poison|double-free|bad-free|allocation-size-too-big|signed integer overflow|unsigned integer overflow|null pointer|misaligned address|shift exponent|shift-base|division by zero|out of bounds'

THREADS_PREFIX=("" "-threads 1 " "-threads 0 " "-threads 2 " "-threads 4 ")

if [[ "$INCLUDE_RISKY_SCALARS" -eq 1 ]]; then
  SAMPLE_ENCODINGS=(0 1 2 4)
  COMPRESSIONS=(0 1)
  PLANARS=(0 1)
  DST_EMBED_ICC=(0 1)
  INTERPOLATIONS=(0 1)
else
  SAMPLE_ENCODINGS=(1 2)
  COMPRESSIONS=(1)
  PLANARS=(0)
  DST_EMBED_ICC=(0 1)
  INTERPOLATIONS=(0 1)
fi

EMBEDDED_SWITCHES=(-embedded)
PCC_SWITCHES=(-pcc)

# ENV sets copied from CI plus a few low-risk numeric variants.
ENV_NONE=""
ENV_R="-ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 "
ENV_G="-ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 "
ENV_B="-ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 "
ENV_K="-ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 "
ENV_MCS_G="-ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -ENV:0ni? 1 "
ENV_MCS_W="-ENV:0ni? 1 "

ENV_SETS=("$ENV_NONE" "$ENV_R" "$ENV_G" "$ENV_B" "$ENV_K" "$ENV_MCS_G" "$ENV_MCS_W")

# Direct rendering intent variants. CI uses: 1, 3, 80, 10001, 10003, 10080.
# Keep intent sets per chain shape to avoid flooding with expected-incompatible chains.
INTENTS_RGB=(0 1 2 3 10 11 12 13 20 21 22 23 40 41 42 1000 1001 1002 1003 10000 10001 10002 10003 10040 10041 10042)
INTENTS_SPECTRAL=(3 10003)
INTENTS_PREVIEW=(1 10001)
INTENTS_MCS=(80 10080)

while [[ $# -gt 0 ]]; do
  case "$1" in
    --count) MODE="count"; shift ;;
    --generate) MODE="generate"; OUT_FILE="$2"; shift 2 ;;
    --mutations=*) MUTATIONS="${1#--mutations=}"; shift ;;
    --mutations) MUTATIONS="$2"; shift 2 ;;
    --start-at=*) START_AT="${1#--start-at=}"; shift ;;
    --start-at) START_AT="$2"; shift 2 ;;
    --jobs=*) JOBS="${1#--jobs=}"; shift ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --log-dir=*) LOG_DIR="${1#--log-dir=}"; shift ;;
    --log-dir) LOG_DIR="$2"; shift 2 ;;
    --pcc-directory) PCC_OVERRIDE="dir:$2"; shift 2 ;;
    --pcc-profile) PCC_OVERRIDE="file:$2"; shift 2 ;;
    --icc-profile) ICC_OVERRIDE="file:$2"; shift 2 ;;
    --icc-profile-index) ICC_OVERRIDE="index:$2"; shift 2 ;;
    --symbolizer) SYMBOLIZER_PATH="$2"; shift 2 ;;
    --case-variants) INCLUDE_CASE_VARIANTS=1; EMBEDDED_SWITCHES=(-embedded -EMBEDDED); PCC_SWITCHES=(-pcc -PCC); shift ;;
    --no-risky-scalars) INCLUDE_RISKY_SCALARS=0; SAMPLE_ENCODINGS=(1 2); COMPRESSIONS=(1); PLANARS=(0); DST_EMBED_ICC=(0 1); INTERPOLATIONS=(0 1); shift ;;
    --no-env-mutations) INCLUDE_ENV_MUTATIONS=0; ENV_SETS=("$ENV_NONE" "$ENV_R" "$ENV_G" "$ENV_B" "$ENV_K" "$ENV_MCS_G" "$ENV_MCS_W"); shift ;;
    --no-replay-cfg) REPLAY_CFG=0; shift ;;
    --show-sanitizer) SHOW_SANITIZER=1; shift ;;
    --stop-on-fail) KEEP_GOING=0; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

if [[ "$INCLUDE_ENV_MUTATIONS" -eq 1 ]]; then
  ENV_SETS+=("-ENV:bkgX -0.0 -ENV:bkgY 0 -ENV:bkgZ 0 ")
  ENV_SETS+=("-ENV:bkgX 1 -ENV:bkgY 1 -ENV:bkgZ 1 ")
  ENV_SETS+=("-ENV:abcd 1234 ")
fi

# Template format:
#   name|src|dstbase|stage_spec
#
# Stage spec tokens are separated by semicolon. Each stage:
#   embedded:<intent-set-name>[:pccA|pccF11|nopcc]
#   profile:<path>:<intent-set-name>
#   envprofile:<env-index>:<path>:<intent-set-name>
#
# ENV prefixes are inserted before the profile they affect, matching CI.
TEMPLATES=(
  # Phase 2: Make multi-spectral image
  "makeMS_smCows|Data/smCows380_5_780.tif|MS_smCows|embedded:SPECTRAL:nopcc;profile:ICC/MultSpectralRGB.icc:SPECTRAL"

  # Phase 3: Apply PCCs for colorimetric renderings
  "cowsA_fromRef|Data/smCows380_5_780.tif|cowsA_fromRef|embedded:REF:pccA;profile:../sRGB_v4_ICC_preference.icc:RGB"
  "cowsA_fromMS|Results/MS_smCows.tif|cowsA_fromMS|embedded:MS:pccA;profile:../sRGB_v4_ICC_preference.icc:RGB"
  "cowsF11_fromRef|Data/smCows380_5_780.tif|cowsF11_fromRef|embedded:REF:pccF11;profile:../sRGB_v4_ICC_preference.icc:RGB"
  "cowsF11_fromMS|Results/MS_smCows.tif|cowsF11_fromMS|embedded:MS:pccF11;profile:../sRGB_v4_ICC_preference.icc:RGB"

  # Phase 6 UW embedded preview
  "TShirtPrevUW|Data/TShirtDesignCMYKW.tif|TShirtPrevUW|embedded:PREVIEW:nopcc;profile:../sRGB_v4_ICC_preference.icc:RGB"

  # Phase 6 US/OS overprint direct preview
  "TShirtPrevUS|Data/TShirtDesignCMYKW.tif|TShirtPrevUS|profile:ICC/CMYK-S_Overprint_Profile.icc:PREVIEW;profile:../sRGB_v4_ICC_preference.icc:RGB"
  "TShirtPrevOS|Data/TShirtDesignCMYKW.tif|TShirtPrevOS|profile:ICC/CMYK-STop_Overprint_Profile.icc:PREVIEW;profile:../sRGB_v4_ICC_preference.icc:RGB"

  # Phase 6 MCS/mid-profile chains. ENV appears before second profile in CI.
  "TShirtPrevUW_G_M|Data/TShirtDesignKW.tif|TShirtPrevUW_G_M|profile:ICC/MW-Mid_Overprint.icc:MCS;envprofile:5:ICC/CMYK-W_Overprint_Profile.icc:MCS;profile:../sRGB_v4_ICC_preference.icc:RGB"
  "TShirtPrevUS_G_M|Data/TShirtDesignKW.tif|TShirtPrevUS_G_M|profile:ICC/MS-Mid_Overprint.icc:MCS;envprofile:5:ICC/CMYK-S_Overprint_Profile.icc:MCS;profile:../sRGB_v4_ICC_preference.icc:RGB"
  "TShirtPrevUS_W_CS|Data/TShirtDesignKW.tif|TShirtPrevUS_W_CS|profile:ICC/SC-Mid_Overprint.icc:MCS;envprofile:6:ICC/CMYK-STop_Overprint_Profile.icc:MCS;profile:../sRGB_v4_ICC_preference.icc:RGB"
)

intent_values() {
  case "$1" in
    RGB) printf '%s\n' "${INTENTS_RGB[@]}" ;;
    SPECTRAL) printf '%s\n' "${INTENTS_SPECTRAL[@]}" ;;
    REF) printf '%s\n' 3 ;;
    MS) printf '%s\n' 10003 ;;
    PREVIEW) printf '%s\n' "${INTENTS_PREVIEW[@]}" ;;
    MCS) printf '%s\n' "${INTENTS_MCS[@]}" ;;
    *) die "unknown intent set: $1" ;;
  esac
}

pcc_path() {
  case "$1" in
    pccA) printf '%s\n' "ICC/Spec400_10_700-IllumA_2deg-Abs.icc" ;;
    pccF11) printf '%s\n' "ICC/Spec400_10_700-F11_2deg-Abs.icc" ;;
    nopcc|"") printf '%s\n' "" ;;
    *) die "unknown pcc key: $1" ;;
  esac
}

# Count expands profile intent combinations by multiplying stage intent counts.
template_stage_count() {
  local spec="$1"
  local count=1 stage kind rest intent_set vals n
  IFS=';' read -r -a stages <<< "$spec"
  for stage in "${stages[@]}"; do
    kind="${stage%%:*}"
    rest="${stage#*:}"
    case "$kind" in
      embedded)
        intent_set="${rest%%:*}"
        ;;
      profile)
        # profile:path:intent
        intent_set="${stage##*:}"
        ;;
      envprofile)
        intent_set="${stage##*:}"
        ;;
      *) die "bad stage kind: $kind" ;;
    esac
    mapfile -t vals < <(intent_values "$intent_set")
    n="${#vals[@]}"
    count=$((count * n))
  done
  echo "$count"
}

max_count() {
  local tmpl name src dst spec tcount total=0
  for tmpl in "${TEMPLATES[@]}"; do
    IFS='|' read -r name src dst spec <<< "$tmpl"
    tcount="$(template_stage_count "$spec")"
    total=$((total + tcount))
  done
  echo $(( total *
           ${#SAMPLE_ENCODINGS[@]} *
           ${#COMPRESSIONS[@]} *
           ${#PLANARS[@]} *
           ${#DST_EMBED_ICC[@]} *
           ${#INTERPOLATIONS[@]} *
           ${#THREADS_PREFIX[@]} *
           ${#EMBEDDED_SWITCHES[@]} *
           ${#PCC_SWITCHES[@]} ))
}

MAX_MUTATIONS="$(max_count)"

if [[ "$MODE" == "count" ]]; then
  echo "$MAX_MUTATIONS"
  exit 0
fi

[[ "$MUTATIONS" == "max" || "$MUTATIONS" == "all" ]] && MUTATIONS="$MAX_MUTATIONS"
[[ "$MUTATIONS" =~ ^[0-9]+$ ]] || die "--mutations must be numeric or max"
[[ "$START_AT" =~ ^[0-9]+$ ]] || die "--start-at must be numeric"
[[ "$JOBS" =~ ^[0-9]+$ ]] || die "--jobs must be numeric"
[[ "$MUTATIONS" -le "$MAX_MUTATIONS" ]] || die "--mutations=$MUTATIONS exceeds max=$MAX_MUTATIONS"
[[ "$START_AT" -ge 1 && "$START_AT" -le "$MAX_MUTATIONS" ]] || die "--start-at=$START_AT outside 1..$MAX_MUTATIONS"

[[ -z "$SYMBOLIZER_PATH" ]] && SYMBOLIZER_PATH="$(command -v llvm-symbolizer || true)"

PCC_FILES=()
if [[ "$PCC_OVERRIDE" == dir:* ]]; then
  dir="${PCC_OVERRIDE#dir:}"
  [[ -d "$dir" ]] || die "missing --pcc-directory: $dir"
  mapfile -t PCC_FILES < <(find "$dir" -type f -name '*.icc' | sort)
  [[ "${#PCC_FILES[@]}" -gt 0 ]] || die "no *.icc files found in $dir"
elif [[ "$PCC_OVERRIDE" == file:* ]]; then
  [[ -f "${PCC_OVERRIDE#file:}" ]] || die "missing --pcc-profile: ${PCC_OVERRIDE#file:}"
fi

ICC_FILE_OVERRIDE=""
if [[ "$ICC_OVERRIDE" == file:* ]]; then
  ICC_FILE_OVERRIDE="${ICC_OVERRIDE#file:}"
  [[ -f "$ICC_FILE_OVERRIDE" ]] || die "missing --icc-profile: $ICC_FILE_OVERRIDE"
elif [[ "$ICC_OVERRIDE" == index:* ]]; then
  n="${ICC_OVERRIDE#index:}"
  [[ "$n" =~ ^[0-9]+$ ]] || die "--icc-profile-index must be numeric"
  mapfile -t ALL_ICC < <(find . -type f -name '*.icc' | sort)
  [[ "$n" -ge 1 && "$n" -le "${#ALL_ICC[@]}" ]] || die "--icc-profile-index $n out of range; found ${#ALL_ICC[@]} *.icc files"
  ICC_FILE_OVERRIDE="${ALL_ICC[$((n-1))]}"
fi

rewrite_pcc_and_final_icc() {
  local idx="$1" cmd="$2"
  if [[ "$PCC_OVERRIDE" == file:* ]]; then
    local repl="${PCC_OVERRIDE#file:}"
    cmd="$(awk -v repl="$repl" '{ for (i=1;i<=NF;i++) if (($i=="-pcc" || $i=="-PCC") && i<NF) { $(i+1)=repl } print }' <<< "$cmd")"
  elif [[ "$PCC_OVERRIDE" == dir:* ]]; then
    local pidx=$(( (idx - 1) % ${#PCC_FILES[@]} ))
    local repl="${PCC_FILES[$pidx]}"
    cmd="$(awk -v repl="$repl" '{ for (i=1;i<=NF;i++) if (($i=="-pcc" || $i=="-PCC") && i<NF) { $(i+1)=repl } print }' <<< "$cmd")"
  fi

  if [[ -n "$ICC_FILE_OVERRIDE" ]]; then
    cmd="$(awk -v repl="$ICC_FILE_OVERRIDE" '{ if (NF>=2) $(NF-1)=repl; print }' <<< "$cmd")"
  fi
  printf '%s\n' "$cmd"
}

# Recursive-ish expansion of stage intents using arrays encoded as text.
emit_stage_chains() {
  local spec="$1"
  local embedded_sw="$2"
  local pcc_sw="$3"
  local current="$4"
  local stage rest kind intent_set profile env_idx pcc_key pcc vals val new_current
  local first="${spec%%;*}"
  local remaining=""
  [[ "$spec" == *";"* ]] && remaining="${spec#*;}"

  kind="${first%%:*}"
  rest="${first#*:}"

  case "$kind" in
    embedded)
      # embedded:intentset:pcckey
      intent_set="${rest%%:*}"
      pcc_key="${rest#*:}"
      mapfile -t vals < <(intent_values "$intent_set")
      for val in "${vals[@]}"; do
        pcc="$(pcc_path "$pcc_key")"
        if [[ -n "$pcc" ]]; then
          new_current="${current}${embedded_sw} ${val} ${pcc_sw} ${pcc} "
        else
          new_current="${current}${embedded_sw} ${val} "
        fi
        if [[ -n "$remaining" ]]; then
          emit_stage_chains "$remaining" "$embedded_sw" "$pcc_sw" "$new_current"
        else
          printf '%s\n' "$new_current"
        fi
      done
      ;;
    profile)
      # profile:path:intentset
      profile="${rest%:*}"
      intent_set="${rest##*:}"
      mapfile -t vals < <(intent_values "$intent_set")
      for val in "${vals[@]}"; do
        new_current="${current}${profile} ${val} "
        if [[ -n "$remaining" ]]; then
          emit_stage_chains "$remaining" "$embedded_sw" "$pcc_sw" "$new_current"
        else
          printf '%s\n' "$new_current"
        fi
      done
      ;;
    envprofile)
      # envprofile:envindex:path:intentset
      env_idx="${rest%%:*}"
      rest="${rest#*:}"
      profile="${rest%:*}"
      intent_set="${rest##*:}"
      mapfile -t vals < <(intent_values "$intent_set")
      for val in "${vals[@]}"; do
        new_current="${current}${ENV_SETS[$env_idx]}${profile} ${val} "
        if [[ -n "$remaining" ]]; then
          emit_stage_chains "$remaining" "$embedded_sw" "$pcc_sw" "$new_current"
        else
          printf '%s\n' "$new_current"
        fi
      done
      ;;
    *) die "bad stage kind: $kind" ;;
  esac
}

emit_export_mutations() {
  local wanted="$1" start_at="$2" emitted=0 idx=0
  local tmpl name src dstbase spec enc comp planar dst_embed interp thread embedded_sw pcc_sw chain tag cmd

  for tmpl in "${TEMPLATES[@]}"; do
    IFS='|' read -r name src dstbase spec <<< "$tmpl"
    for enc in "${SAMPLE_ENCODINGS[@]}"; do
      for comp in "${COMPRESSIONS[@]}"; do
        for planar in "${PLANARS[@]}"; do
          for dst_embed in "${DST_EMBED_ICC[@]}"; do
            for interp in "${INTERPOLATIONS[@]}"; do
              for thread in "${THREADS_PREFIX[@]}"; do
                for embedded_sw in "${EMBEDDED_SWITCHES[@]}"; do
                  for pcc_sw in "${PCC_SWITCHES[@]}"; do
                    while IFS= read -r chain; do
                      idx=$((idx + 1))
                      [[ "$idx" -lt "$start_at" ]] && continue
                      [[ "$emitted" -ge "$wanted" ]] && return 0

                      tag="$(printf '%05d_%s_e%s_c%s_p%s_emb%s_i%s' "$idx" "$name" "$enc" "$comp" "$planar" "$dst_embed" "$interp")"
                      cmd="iccApplyProfiles ${thread}-exportcfg config/ci_path_${tag}.json ${src} Results/ci_path_${tag}.tif ${enc} ${comp} ${planar} ${dst_embed} ${interp} ${chain}"
                      cmd="$(sed 's/[[:space:]][[:space:]]*/ /g; s/[[:space:]]$//' <<< "$cmd")"
                      rewrite_pcc_and_final_icc "$idx" "$cmd"
                      emitted=$((emitted + 1))
                    done < <(emit_stage_chains "$spec" "$embedded_sw" "$pcc_sw" "")
                  done
                done
              done
            done
          done
        done
      done
    done
  done
}

cmd_config_path() {
  awk '{ for (i=1; i<=NF; i++) if ($i == "-exportcfg" && i<NF) { print $(i+1); exit } }' <<< "$1"
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

  local asan_opts="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=1:detect_leaks=0:abort_on_error=1:symbolize=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1}"
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
  local idx="$1" export_cmd="$2"
  local rc=0 cfg cfg_cmd thread_prefix

  run_cmd "$idx" "export" "$export_cmd" || rc=$?

  if [[ "$REPLAY_CFG" -eq 1 ]]; then
    cfg="$(cmd_config_path "$export_cmd")"
    if [[ -n "$cfg" && -f "$cfg" ]]; then
      if (( idx % 2 == 0 )); then thread_prefix="-threads 1 "; else thread_prefix=""; fi
      cfg_cmd="iccApplyProfiles ${thread_prefix}-cfg ${cfg}"
      run_cmd "$idx" "cfg" "$cfg_cmd" || rc=$?
    elif [[ "$DRY_RUN" -ne 1 ]]; then
      echo "SKIP $idx cfg ; config not generated: $cfg"
    fi
  fi

  return "$rc"
}

echo "Computed CI-aligned export mutation space: $MAX_MUTATIONS" >&2
echo "Selected mutations: $MUTATIONS starting at index $START_AT" >&2
echo "Replay -cfg mode: $REPLAY_CFG" >&2
[[ -n "$SYMBOLIZER_PATH" ]] && echo "Using symbolizer: $SYMBOLIZER_PATH" >&2

if [[ "$MODE" == "generate" ]]; then
  [[ -n "$OUT_FILE" ]] || die "--generate requires output file"
  emit_export_mutations "$MUTATIONS" "$START_AT" > "$OUT_FILE"
  echo "Wrote $OUT_FILE" >&2
  exit 0
fi

mkdir -p "$LOG_DIR"
: > "$LOG_DIR/sanitizer-summary.txt"
: > "$LOG_DIR/failures.txt"

tmp="$(mktemp)"
emit_export_mutations "$MUTATIONS" "$START_AT" > "$tmp"

if [[ "$JOBS" -le 1 ]]; then
  failures=0
  current="$START_AT"
  while IFS= read -r cmd; do
    if ! run_one "$current" "$cmd"; then
      failures=$((failures + 1))
      [[ "$KEEP_GOING" -eq 1 ]] || break
    fi
    current=$((current + 1))
  done < "$tmp"
  rm -f "$tmp"
  echo "=== Done: failures=$failures ==="
  echo "Sanitizer summary: $LOG_DIR/sanitizer-summary.txt"
  echo "Failure summary:   $LOG_DIR/failures.txt"
  if [[ "$failures" -eq 0 ]]; then
    exit 0
  fi
  exit 1
fi

# Simple parallel queue preserving full command lines.
current="$START_AT"
while IFS= read -r cmd; do
  idx="$current"
  current=$((current + 1))
  (
    run_one "$idx" "$cmd"
  ) &
  while (( $(jobs -rp | wc -l) >= JOBS )); do
    wait -n || true
  done
done < "$tmp"

while wait -n 2>/dev/null; do :; done

rm -f "$tmp"
echo "Sanitizer summary: $LOG_DIR/sanitizer-summary.txt"
echo "Failure summary:   $LOG_DIR/failures.txt"
exit 0
