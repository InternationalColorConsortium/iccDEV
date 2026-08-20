#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccApplyNamedCmm-quick-check.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/qa-common.sh"
qa_init "iccApplyNamedCmm-quick-check"

BIN="$ICCDEV_BUILD_DIR/Tools/IccApplyNamedCmm/iccApplyNamedCmm"
DATA="$ICCDEV_ROOT/Testing/ApplyDataFiles/rgb8bit.txt"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"
CFG="$QA_OUTDIR/named.json"

qa_require_tool "$BIN"
qa_require_file "$DATA"
qa_require_file "$PROFILE"
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

qa_run basic success "" "$BIN" "$DATA" 0 0 "$PROFILE" 1
qa_run debug success "" "$BIN" -debugcalc "$DATA" 0 0 "$PROFILE" 1
qa_run environment success "" "$BIN" "$DATA" 0 0 \
    -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 "$PROFILE" 1
qa_run pcc success "" "$BIN" "$DATA" 0 0 "$PROFILE" 1 -PCC "$PROFILE"
qa_run export-config success "" "$BIN" -exportcfganddata "$CFG" "$DATA" 0 0 "$PROFILE" 1
qa_require_file "$CFG"
qa_run replay-config success "" "$BIN" -cfg "$CFG"
qa_run config-extra reject "Unexpected extra arguments for -cfg" "$BIN" -cfg "$CFG" ignored-extra
qa_run legacy-extra reject "Unexpected extra arguments" "$BIN" "$DATA" 0 0 "$PROFILE" 1 ignored-extra

qa_finish
