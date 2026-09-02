#!/usr/bin/env bash
###############################################################################
# Exercise numbered spectral TIFF fixtures as single-channel inputs under all
# compression and planar-configuration combinations.
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
#   iccdev-specsep-corpus-matrix.sh [CORPUS_DIR ...]
#
# Environment:
#   ICCDEV_REPO_ROOT    iccDEV checkout or parent research checkout
#   ICCDEV_TOOLS_DIR    path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR  output directory for generated TIFFs
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-$(mktemp -d /tmp/iccdev-specsep-corpus-matrix.XXXXXX)}"
SPECSEP="$TOOLS_DIR/IccSpecSepToTiff/iccSpecSepToTiff"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  echo "Usage: $0 [CORPUS_DIR ...]"
  exit 0
fi

if [ ! -x "$SPECSEP" ]; then
  echo "iccSpecSepToTiff not found or not executable: $SPECSEP" >&2
  exit 2
fi

if [ "$#" -gt 0 ]; then
  CORPUS_DIRS=("$@")
else
  CORPUS_DIRS=(
    "$ICCDEV_ROOT/.github/ci/test-data/spectral"
    "$ICCDEV_ROOT/.github/ci/test-data/specsep-harvest/gray300"
  )
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

mkdir -p "$OUTDIR"

files=0
total=0
pass=0
reject=0
abnormal=0
sanitizer=0
missing_output=0
corpus_index=0

for corpus_dir in "${CORPUS_DIRS[@]}"; do
  corpus_index=$((corpus_index + 1))
  if [ ! -d "$corpus_dir" ]; then
    echo "Corpus directory not found: $corpus_dir" >&2
    exit 2
  fi

  corpus_files=0
  corpus_outdir="$OUTDIR/corpus-$corpus_index"
  mkdir -p "$corpus_outdir"

  for path in "$corpus_dir"/*; do
    [ -f "$path" ] || continue
    file_name=${path##*/}
    [[ $file_name =~ ^(.*[^0-9])([0-9]+)$ ]] || continue

    prefix="$corpus_dir/${BASH_REMATCH[1]}"
    channel=${BASH_REMATCH[2]}
    files=$((files + 1))
    corpus_files=$((corpus_files + 1))

    for compress in 0 1; do
      for separate in 0 1; do
        total=$((total + 1))
        output="$corpus_outdir/${file_name}-c${compress}-s${separate}.tif"
        message=$(timeout 60 "$SPECSEP" "$output" "$compress" "$separate" \
          "$prefix" "$channel" "$channel" 1 2>&1)
        rc=$?

        printf '%s\n' "$message" | sed \
          "s|^|[$corpus_index:$file_name c=$compress s=$separate] |"

        if grep -Eq \
          'ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:' \
          <<<"$message"; then
          sanitizer=$((sanitizer + 1))
        fi

        if [ "$rc" -eq 0 ]; then
          if [ ! -s "$output" ]; then
            missing_output=$((missing_output + 1))
            printf '[%d:%s c=%d s=%d] output TIFF missing or empty\n' \
              "$corpus_index" "$file_name" "$compress" "$separate" >&2
          else
            # A conversion that exits 0 and leaves a file is not yet proof: the
            # tool reports what it produced, and echoing that report without
            # checking it made this whole matrix pass against a build that
            # printed no summary at all.
            summary_missing=""
            for field in "Output:" "Size:" "BitsPerSample:" "SamplesPerPixel:" \
                         "Planar:" "Compression:" "Profile:" \
                         "Image successfully written!"; do
              if ! grep -Fq "$field" <<<"$message"; then
                summary_missing="$field"
                break
              fi
            done

            if [ -n "$summary_missing" ]; then
              missing_output=$((missing_output + 1))
              printf '[%d:%s c=%d s=%d] success summary missing: %s\n' \
                "$corpus_index" "$file_name" "$compress" "$separate" \
                "$summary_missing" >&2
            else
              pass=$((pass + 1))
            fi
          fi
        elif [ "$rc" -eq 124 ]; then
          abnormal=$((abnormal + 1))
          printf '[%d:%s c=%d s=%d] timed out\n' \
            "$corpus_index" "$file_name" "$compress" "$separate" >&2
        elif [ "$rc" -eq 255 ] || { [ "$rc" -ge 1 ] && [ "$rc" -le 123 ]; }; then
          # main() returns -1 on every error path, which the shell reports as
          # 255.  Reading that as "abnormal" labelled every ordinary rejection a
          # crash; a real signal death lands in 128..192.
          reject=$((reject + 1))
          printf '[%d:%s c=%d s=%d] rejected with exit=%d\n' \
            "$corpus_index" "$file_name" "$compress" "$separate" "$rc" >&2
        else
          abnormal=$((abnormal + 1))
          printf '[%d:%s c=%d s=%d] abnormal exit=%d\n' \
            "$corpus_index" "$file_name" "$compress" "$separate" "$rc" >&2
        fi
      done
    done
  done

  if [ "$corpus_files" -eq 0 ]; then
    echo "No extensionless files ending in a channel number found in: $corpus_dir" >&2
    exit 2
  fi
done

printf 'FILES=%d TOTAL=%d PASS=%d REJECT=%d ABNORMAL=%d SANITIZER=%d MISSING_OUTPUT=%d\n' \
  "$files" "$total" "$pass" "$reject" "$abnormal" "$sanitizer" "$missing_output"
printf 'Generated TIFFs: %s\n' "$OUTDIR"

if [ "$reject" -ne 0 ] || [ "$abnormal" -ne 0 ] || \
   [ "$sanitizer" -ne 0 ] || [ "$missing_output" -ne 0 ]; then
  exit 1
fi

exit 0
