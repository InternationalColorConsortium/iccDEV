#!/usr/bin/env bash
###############################################################################
# Sweep ICC profiles through the optional iccSpecSepToTiff profile argument.
###############################################################################
#
# Copyright (c) 2026 The International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
#
# Usage:
#   iccdev-specsep-profile-sweep.sh [--profile-dir DIR] [--max-profiles N]
#
# Environment:
#   ICCDEV_REPO_ROOT    iccDEV checkout or parent research checkout
#   ICCDEV_TOOLS_DIR    path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR  output directory for generated TIFFs and logs
#   ICCDEV_TEST_TIMEOUT per-profile timeout in seconds, default: 60
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
REPO_ROOT="${ICCDEV_REPO_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd -P)}"

if [ -d "$REPO_ROOT/Tools/CmdLine/IccSpecSepToTiff" ]; then
  ICCDEV_ROOT="$REPO_ROOT"
elif [ -d "$REPO_ROOT/iccDEV/Tools/CmdLine/IccSpecSepToTiff" ]; then
  ICCDEV_ROOT="$REPO_ROOT/iccDEV"
else
  echo "Unable to locate an iccDEV checkout below: $REPO_ROOT" >&2
  exit 2
fi

TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$ICCDEV_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-$(mktemp -d /tmp/iccdev-specsep-profile-sweep.XXXXXX)}"
TIMEOUT_SECONDS="${ICCDEV_TEST_TIMEOUT:-60}"
SPECSEP="$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff"
INPUT_PREFIX="$ICCDEV_ROOT/.github/ci/test-data/spectral/spec_"
PROFILE_DIR=""
MAX_PROFILES=0
# Channel count of the generated TIFF.  A profile is accepted only when its
# sample count matches, so this is what decides which corpora can exercise the
# accept path at all: the checked-in .icc corpus is 1/3/4-channel, so the
# default 8 can only ever produce rejections.
CHANNELS=8

usage() {
  echo "Usage: $0 [--profile-dir DIR] [--max-profiles N] [--channels N]"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --profile-dir)
      [ "$#" -ge 2 ] || { echo "Missing value for --profile-dir" >&2; exit 2; }
      PROFILE_DIR="$2"
      shift 2
      ;;
    --max-profiles)
      [ "$#" -ge 2 ] || { echo "Missing value for --max-profiles" >&2; exit 2; }
      MAX_PROFILES="$2"
      shift 2
      ;;
    --channels)
      [ "$#" -ge 2 ] || { echo "Missing value for --channels" >&2; exit 2; }
      CHANNELS="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if ! [[ "$MAX_PROFILES" =~ ^[0-9]+$ ]]; then
  echo "--max-profiles must be a non-negative integer" >&2
  exit 2
fi
if ! [[ "$CHANNELS" =~ ^[0-9]+$ ]] || [ "$CHANNELS" -lt 1 ] || [ "$CHANNELS" -gt 10 ]; then
  echo "--channels must be an integer between 1 and 10 (spec_1 .. spec_10 are the checked-in inputs)" >&2
  exit 2
fi
if ! [[ "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]] || [ "$TIMEOUT_SECONDS" -lt 1 ]; then
  echo "ICCDEV_TEST_TIMEOUT must be a positive integer" >&2
  exit 2
fi

if [ -z "$PROFILE_DIR" ]; then
  if compgen -G "$ICCDEV_ROOT/Testing/reg/*.icc" >/dev/null; then
    PROFILE_DIR="$ICCDEV_ROOT/Testing/reg"
  else
    PROFILE_DIR="$ICCDEV_ROOT/Testing/CalcTest"
  fi
fi

if [ ! -d "$PROFILE_DIR" ]; then
  echo "Profile directory not found: $PROFILE_DIR" >&2
  exit 2
fi
if [ ! -x "$SPECSEP" ]; then
  echo "iccSpecSepToTiff not found or not executable: $SPECSEP" >&2
  exit 2
fi
for channel in $(seq 1 "$CHANNELS"); do
  if [ ! -s "$INPUT_PREFIX$channel" ]; then
    echo "Missing numbered SpecSep input: $INPUT_PREFIX$channel" >&2
    exit 2
  fi
done

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

mkdir -p "$OUTDIR/logs" "$OUTDIR/tiffs"
mapfile -d '' PROFILES < <(find "$PROFILE_DIR" -maxdepth 1 -type f -name '*.icc' -print0 | LC_ALL=C sort -z)

if [ "${#PROFILES[@]}" -eq 0 ]; then
  echo "No .icc profiles found in: $PROFILE_DIR" >&2
  exit 2
fi

total=0
accepted=0
expected_reject=0
failed=0
sanitizer=0

for profile in "${PROFILES[@]}"; do
  if [ "$MAX_PROFILES" -gt 0 ] && [ "$total" -ge "$MAX_PROFILES" ]; then
    break
  fi

  total=$((total + 1))
  profile_name="${profile##*/}"
  safe_name="$(printf '%s' "$profile_name" | tr -c 'A-Za-z0-9._-' '_')"
  output="$OUTDIR/tiffs/${total}-${safe_name}.tif"
  log="$OUTDIR/logs/${total}-${safe_name}.log"

  rm -f "$output"
  timeout "$TIMEOUT_SECONDS" "$SPECSEP" "$output" 0 0 \
    "$INPUT_PREFIX" 1 "$CHANNELS" 1 "$profile" >"$log" 2>&1
  rc=$?

  sed "s|^|[$profile_name] |" "$log"

  if grep -Eq \
    'ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:' \
    "$log"; then
    sanitizer=$((sanitizer + 1))
    failed=$((failed + 1))
    echo "[$profile_name] FAIL: sanitizer diagnostic"
  elif [ "$rc" -eq 0 ]; then
    if [ -s "$output" ] && grep -Eq '^Profile:[[:space:]]+embedded$' "$log"; then
      accepted=$((accepted + 1))
      echo "[$profile_name] ACCEPT"
    else
      failed=$((failed + 1))
      echo "[$profile_name] FAIL: success did not produce an embedded-profile TIFF"
    fi
  elif { [ "$rc" -eq 1 ] || [ "$rc" -eq 255 ]; } && \
       grep -Eqi 'profile' "$log" && grep -Fq "$profile" "$log" && \
       [ ! -e "$output" ]; then
    expected_reject=$((expected_reject + 1))
    echo "[$profile_name] EXPECTED_REJECT"
  elif [ "$rc" -eq 124 ]; then
    failed=$((failed + 1))
    echo "[$profile_name] FAIL: timeout after $TIMEOUT_SECONDS seconds"
  else
    failed=$((failed + 1))
    echo "[$profile_name] FAIL: exit=$rc output_exists=$(test -e "$output" && echo yes || echo no)"
  fi
done

# Say so out loud when the accept arm never fired.  A corpus of deliberately
# broken or mismatched profiles makes this sweep pass while exercising only half
# the tool, and a silent green there reads as coverage it does not have.
if [ "$accepted" -eq 0 ]; then
  printf 'NOTE: no profile matched %d channels, so the accept path was not exercised by %s\n' \
    "$CHANNELS" "$PROFILE_DIR"
fi

printf 'SpecSep profile sweep: %d profiles, %d accepted, %d expected rejects, %d failed, %d sanitizer findings\n' \
  "$total" "$accepted" "$expected_reject" "$failed" "$sanitizer"
printf 'Generated TIFFs and logs: %s\n' "$OUTDIR"

if [ "$failed" -ne 0 ]; then
  exit 1
fi

exit 0
