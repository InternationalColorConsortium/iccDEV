#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccApplyProfiles-quick-check.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/qa-common.sh"
qa_init "iccApplyProfiles-quick-check"

BIN="$ICCDEV_BUILD_DIR/Tools/IccApplyProfiles/iccApplyProfiles"
SRC="$ICCDEV_ROOT/Testing/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
PROFILE="$ICCDEV_ROOT/Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
CFG="$QA_OUTDIR/profiles.json"

qa_require_tool "$BIN"
qa_require_file "$SRC"
qa_require_file "$PROFILE"
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

for encoding in 0 1 2 3; do
    out="$QA_OUTDIR/encoding-$encoding.tif"
    qa_run "encoding-$encoding" success "" "$BIN" "$SRC" "$out" "$encoding" 0 0 0 0 "$PROFILE" 1
    [ -s "$out" ] || qa_fail "encoding-$encoding did not create a TIFF"
done

qa_run encoding-4 reject "Unable to parse configuration arguments" \
    "$BIN" "$SRC" "$QA_OUTDIR/encoding-4.tif" 4 0 0 0 0 "$PROFILE" 1
qa_run export-config success "" "$BIN" -exportcfg "$CFG" "$SRC" "$QA_OUTDIR/export.tif" 1 0 0 0 0 "$PROFILE" 1
qa_require_file "$CFG"
qa_run replay-config success "" "$BIN" -cfg "$CFG"
qa_run config-extra reject "Unexpected extra arguments for -cfg" "$BIN" -cfg "$CFG" ignored-extra
qa_run legacy-extra reject "Unexpected extra arguments" \
    "$BIN" "$SRC" "$QA_OUTDIR/legacy-extra.tif" 1 0 0 0 0 "$PROFILE" 1 ignored-extra
qa_run threads-one success "" \
    "$BIN" -threads 1 "$SRC" "$QA_OUTDIR/threads-one.tif" 1 0 0 0 0 "$PROFILE" 1

qa_finish
