#!/bin/sh
#################################################################################
# CalcTest/runtests.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#                                        All rights reserved.
#
# Intent: Validate Calculator MPE rejection and valid execution cases.
#################################################################################
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$SCRIPT_DIR/../path.sh" ]; then
    . "$SCRIPT_DIR/../path.sh"
fi

cd "$SCRIPT_DIR"
"$SCRIPT_DIR/checkInvalidProfiles.sh"

iccFromXml calcCheckInit.xml calcCheckInit.icc
iccApplyNamedCmm -debugcalc rgbExercise8bit.txt 0 1 calcCheckInit.icc 1 >> report.txt
iccFromXml calcExercizeOps.xml calcExercizeOps.icc
iccApplyNamedCmm -debugcalc rgbExercise8bit.txt 0 1 calcExercizeOps.icc 1 >> report.txt
