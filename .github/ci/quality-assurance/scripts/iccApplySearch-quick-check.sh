#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccApplySearch-quick-check.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/qa-common.sh"
qa_init "iccApplySearch-quick-check"

BIN="$ICCDEV_TOOLS_DIR/IccApplySearch/iccApplySearch"
DATA="$ICCDEV_ROOT/Testing/ApplyDataFiles/rgb8bit.txt"
DATA16="$ICCDEV_ROOT/.github/ci/test-data/test-data-rgb-16bit.txt"
FLOAT_DATA="$ICCDEV_ROOT/.github/ci/test-data/test-data-rgb-float.txt"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"
CFG="$QA_OUTDIR/search.json"

qa_require_tool "$BIN"
qa_require_file "$DATA"
qa_require_file "$DATA16"
qa_require_file "$FLOAT_DATA"
qa_require_file "$PROFILE"
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

for encoding in 0 1 2 3 4 5 6; do
    qa_run "encoding-$encoding" success "" \
        "$BIN" "$DATA" "$encoding" 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1
done

qa_run encoding-7 reject "Unable to parse configuration arguments" \
    "$BIN" "$DATA" 7 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1

qa_run encoding5-tetra-init success "" \
    "$BIN" "$DATA16" 5 1 "$PROFILE" 1 "$PROFILE" 1 -INIT 1 "$PROFILE" 1
qa_run encoding4-linear-init success "" \
    "$BIN" "$DATA16" 4 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1 "$PROFILE" 1
qa_run float-noinit success "" \
    "$BIN" "$FLOAT_DATA" 3 0 "$PROFILE" 1 "$PROFILE" 1
qa_run weight-positive success "" \
    "$BIN" "$DATA" 0 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1 "$PROFILE" 1
qa_run weight-zero reject "AttachPCC failed" \
    "$BIN" "$DATA" 0 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1 "$PROFILE" 0
qa_run weight-negative reject "AttachPCC failed" \
    "$BIN" "$DATA" 0 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1 "$PROFILE" -1
qa_run weight-max-finite success "" \
    "$BIN" "$DATA" 0 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1 "$PROFILE" 3.402823e+38
qa_run export-config success "" "$BIN" -exportcfganddata "$CFG" \
    "$DATA" 3 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1
qa_require_file "$CFG"
qa_run replay-config success "" "$BIN" -cfg "$CFG"
qa_run config-extra reject "Unexpected extra arguments for -cfg" "$BIN" -cfg "$CFG" ignored-extra

qa_finish
