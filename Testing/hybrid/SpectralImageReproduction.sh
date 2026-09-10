#!/bin/bash
#################################################################################
# Testing/hybrid/SpectralImageReproduction.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
# Intent: Reproduce the FULL 600x420 multispectral cows image into the hybrid
#         CMYK printer profile by spectral inverse search, under the same four
#         observing conditions (D93 / Illuminant A / D50 / F11) that the
#         cmykGrays search in BuildAndTest.sh uses.
#
#         This is deliberately NOT part of BuildAndTest.sh.  Every pixel runs a
#         Nelder-Mead search across four weighted PCCs, so the full image costs
#         minutes rather than the seconds the 92x64 icon step costs -- far too
#         long for the sanitizer legs that run the hybrid pipeline.  Run this by
#         hand when you want the full-resolution result.
#
# Usage:  ./SpectralImageReproduction.sh          (from Testing/hybrid)
#
#         Requires BuildAndTest.sh to have run first: it builds the ICC profiles
#         in ICC/ and produces Results/MS_smCows.tif, the 8-channel
#         multispectral image this consumes.
#################################################################################
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SILENCE_FILE="$SCRIPT_DIR/../silence.txt"
if [ -r "$SILENCE_FILE" ]; then
  export UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}suppressions=$SILENCE_FILE"
fi

for required in Results/MS_smCows.tif \
                ICC/CMYK_Hybrid_Profile.icc \
                ICC/Lab_float-D50_2deg.icc \
                ICC/Lab_float-D93_2deg-MAT.icc \
                ICC/Lab_float-F11_2deg-MAT.icc \
                ICC/Lab_float-IllumA_2deg-MAT.icc; do
  if [ ! -f "$required" ]; then
    echo "missing $required -- run ./BuildAndTest.sh first" >&2
    exit 1
  fi
done

echo "========== Spectral image reproduction (full 600x420) =========="
echo "This runs an inverse search per pixel; expect minutes, not seconds."

# connect.threads is 0 (hardware concurrency) in the config.  A search CMM gives
# every worker its own CIccApplyCmmSearch with private sub-chain apply objects,
# so the threaded result is identical to the scalar one -- only faster.
iccApplyProfiles -cfg config/msCowsToCmyk.json
iccTiffDump   Results/MS_smCowsCmyk.tif

echo "Wrote Results/MS_smCowsCmyk.tif"
