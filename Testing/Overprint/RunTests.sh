#!/bin/bash
#################################################################################
# Overprint/RunTests.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-04-09
#
# Intent: iccDEV CICD
#################################################################################

echo "====================== Entering Overprint/RunTests.sh =========================="

# Auto-source path.sh if present (sets PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$SCRIPT_DIR/../path.sh" ]; then
	. "$SCRIPT_DIR/../path.sh"
fi

iccApplyNamedCmm 17ChanData.txt 3 0 17ChanPart1.icc 1

echo "====================== Exiting Overprint/RunTests.sh =========================="
