#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/icc_apply_qa_suite.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -u

MUTATIONS=12
LOG_DIR="icc-apply-qa-suite-logs"
KEEP_OUTPUT=0

usage() {
  cat <<'EOF'
Usage:
  ./icc_apply_qa_suite.sh [options]

Options:
  --mutations N                   Commands per tool smoke. Default: 12.
  --log-dir DIR                   Suite log directory. Default: icc-apply-qa-suite-logs.
  --keep-output                   Keep generated Links/config/Results smoke outputs.
  -h, --help                      Show this help.

Run from iccDEV/Testing/hybrid after ./BuildAndTest.sh has generated profiles.
EOF
}

die() {
  echo "error: $*" >&2
  exit 2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mutations=*) MUTATIONS="${1#--mutations=}"; shift ;;
    --mutations) MUTATIONS="$2"; shift 2 ;;
    --log-dir=*) LOG_DIR="${1#--log-dir=}"; shift ;;
    --log-dir) LOG_DIR="$2"; shift 2 ;;
    --keep-output) KEEP_OUTPUT=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

[[ "$MUTATIONS" =~ ^[0-9]+$ ]] || die "--mutations must be numeric"
[[ "$MUTATIONS" -gt 0 ]] || die "--mutations must be greater than zero"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
for script in \
  "$SCRIPT_DIR/tolink-script-random-001.sh" \
  "$SCRIPT_DIR/iccApplyProfiles_ci_path_exercise.sh" \
  "$SCRIPT_DIR/icc_ci_tool_path_exercise.sh" \
  "$SCRIPT_DIR/iccApplyNamedCmm_ci_path_exercise.sh" \
  "$SCRIPT_DIR/iccBenchApply-quick-check.sh"; do
  [[ -x "$script" ]] || die "missing executable script: $script"
done

for required in Data/cmykGrays.txt Results/cmykGraysRef.txt ICC/CMYK_Hybrid_Profile.icc ../sRGB_v4_ICC_preference.icc; do
  [[ -f "$required" ]] || die "missing required input: $required; run ./BuildAndTest.sh first"
done

for tool_dir in IccApplyToLink IccApplyProfiles IccApplySearch IccApplyNamedCmm; do
  if [[ -d "$PWD/../../Build/Tools/$tool_dir" ]]; then
    PATH="$PWD/../../Build/Tools/$tool_dir:$PATH"
  fi
done

mkdir -p "$LOG_DIR"
: > "$LOG_DIR/summary.txt"

run_step() {
  local name="$1"
  shift
  echo "== $name =="
  if "$@" > "$LOG_DIR/$name.out" 2> "$LOG_DIR/$name.err"; then
    echo "PASS $name" | tee -a "$LOG_DIR/summary.txt"
  else
    local rc=$?
    echo "FAIL $name rc=$rc" | tee -a "$LOG_DIR/summary.txt"
    sed -n '1,80p' "$LOG_DIR/$name.out"
    sed -n '1,80p' "$LOG_DIR/$name.err"
    return "$rc"
  fi
}

failures=0
run_step bench env QA_OUTDIR="$LOG_DIR/bench" "$SCRIPT_DIR/iccBenchApply-quick-check.sh" || failures=$((failures + 1))
run_step tolink "$SCRIPT_DIR/tolink-script-random-001.sh" --tests "$MUTATIONS" --log-dir "$LOG_DIR/tolink" --link-dir "$LOG_DIR/Links/tolink" || failures=$((failures + 1))
run_step profiles "$SCRIPT_DIR/iccApplyProfiles_ci_path_exercise.sh" --mutations "$MUTATIONS" --no-replay-cfg --log-dir "$LOG_DIR/profiles" || failures=$((failures + 1))
run_step search "$SCRIPT_DIR/icc_ci_tool_path_exercise.sh" --tool search --mutations "$MUTATIONS" --no-replay-cfg --allow-clean-rejections --log-dir "$LOG_DIR/search" || failures=$((failures + 1))
run_step namedcmm "$SCRIPT_DIR/iccApplyNamedCmm_ci_path_exercise.sh" --mutations "$MUTATIONS" --no-replay-cfg --log-dir "$LOG_DIR/namedcmm" || failures=$((failures + 1))

if grep -R -E 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|DEADLYSIGNAL|SEGV' "$LOG_DIR" >/dev/null 2>&1; then
  echo "FAIL sanitizer-pattern-scan" | tee -a "$LOG_DIR/summary.txt"
  failures=$((failures + 1))
else
  echo "PASS sanitizer-pattern-scan" | tee -a "$LOG_DIR/summary.txt"
fi

if [[ "$KEEP_OUTPUT" -eq 0 && "$failures" -eq 0 ]]; then
  rm -rf "$LOG_DIR/Links"
fi

echo "=== Suite done: failures=$failures ==="
echo "Summary: $LOG_DIR/summary.txt"
[[ "$failures" -eq 0 ]]
