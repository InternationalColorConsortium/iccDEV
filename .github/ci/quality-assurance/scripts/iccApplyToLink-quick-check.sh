#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccApplyToLink-quick-check.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/qa-common.sh"
qa_init "iccApplyToLink-quick-check"

BIN="$ICCDEV_BUILD_DIR/Tools/IccApplyToLink/iccApplyToLink"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"

qa_require_tool "$BIN"
qa_require_file "$PROFILE"
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

qa_run valid-v4 success "" "$BIN" "$QA_OUTDIR/valid-v4.icc" \
    0 2 0 "QA v4" 0 1 0 0 "$PROFILE" 1
qa_require_file "$QA_OUTDIR/valid-v4.icc"
qa_run valid-cube success "" "$BIN" "$QA_OUTDIR/valid.cube" \
    1 2 4 "QA cube" 0 1 0 0 "$PROFILE" 1
qa_assert_contains "$QA_OUTDIR/valid.cube" "LUT_3D_SIZE 2" "Cube records grid size"
qa_run link-type-2 reject "expected 0 (Device Link) or 1 (.cube text file)" \
    "$BIN" "$QA_OUTDIR/bad-type.icc" 2 2 0 "QA bad type" 0 1 0 0 "$PROFILE" 1
qa_run lut-below-min reject "expected an integer between 2 and 255" \
    "$BIN" "$QA_OUTDIR/lut-low.icc" 0 1 0 "QA low" 0 1 0 0 "$PROFILE" 1
qa_run zero-width-range reject "must be greater than" \
    "$BIN" "$QA_OUTDIR/zero.icc" 0 2 0 "QA zero" 1 1 0 0 "$PROFILE" 1
qa_run restricted-v4 reject "V4 device links cannot record" \
    "$BIN" "$QA_OUTDIR/restricted-v4.icc" 0 2 0 "QA restricted" 0.1 0.9 0 0 "$PROFILE" 1
qa_run restricted-v5 success "" "$BIN" "$QA_OUTDIR/restricted-v5.icc" \
    0 2 1 "QA restricted v5" 0.1 0.9 0 0 "$PROFILE" 1

qa_finish
