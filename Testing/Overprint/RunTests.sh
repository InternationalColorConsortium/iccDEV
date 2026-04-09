#!/bin/bash
#################################################################################
# Overprint/RunTests.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium. 
#                                        All rights reserved.
# 
#
#  Last Updated: Sun Jan  4 10:03:49 PM UTC 2026 by David Hoyt
#                Fix PATH Issue
#                PATH error was masked by CI hardcoding
#
#
#
#
# Intent: iccMAX CICD
#
#
#
#
#################################################################################

echo "====================== Entering Overprint/RunTests.sh =========================="

# Add tool directories to PATH (process substitution avoids subshell)
while IFS= read -r d; do
  abs_path=$(cd "$d" && pwd)
  PATH="$abs_path:$PATH"
done < <(find ../../Build/Tools -type f -perm -111 -exec dirname {} \; | sort -u)

export PATH

iccApplyNamedCmm 17ChanData.txt 3 0 17ChanPart1.icc 1

echo "====================== Exiting Overprint/RunTests.sh =========================="
