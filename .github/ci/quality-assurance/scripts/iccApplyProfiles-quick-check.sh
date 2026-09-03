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
# This 1024-pixel-wide RGB TIFF exceeds the threaded CMM's 256-pixel
# per-worker threshold, so the -threads cases dispatch worker threads.
SRC="$ICCDEV_ROOT/Testing/mcs/prev.tif"
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
qa_require_file "$QA_OUTDIR/threads-one.tif"
qa_run threads-nonnumeric reject "Invalid thread count 'abc'" \
    "$BIN" -threads abc "$SRC" "$QA_OUTDIR/threads-abc.tif" 1 0 0 0 0 "$PROFILE" 1
qa_run threads-over-limit reject "Invalid thread count '257'" \
    "$BIN" -threads 257 "$SRC" "$QA_OUTDIR/threads-257.tif" 1 0 0 0 0 "$PROFILE" 1
for nthreads in 0 2 4; do
    out="$QA_OUTDIR/threads-$nthreads.tif"
    qa_run "threads-$nthreads" success "" \
        "$BIN" -threads "$nthreads" "$SRC" "$out" 1 0 0 0 0 "$PROFILE" 1
    if [ ! -s "$out" ]; then
        qa_fail "threads-$nthreads did not create a TIFF"
    elif ! cmp -s "$QA_OUTDIR/threads-one.tif" "$out"; then
        qa_fail "threads-$nthreads output differs from threads-one"
    fi
done

qa_run threads-2-timing success "" env ICC_APPLY_PROFILES_TIMING=1 \
    "$BIN" -threads 2 "$SRC" "$QA_OUTDIR/threads-2-timing.tif" \
    1 0 0 0 0 "$PROFILE" 1
if ! grep -Eq '^\[TIMING\] Async worker strips: [1-9][0-9]*$' "$QA_LAST_LOG"; then
    qa_fail "threads-2-timing did not report asynchronous worker strips"
fi

trace_dst="$QA_OUTDIR/trace"$'\n'"[APPLY_TRACE] event=forged.tif"
qa_run trace-escapes success "" env ICC_APPLY_TRACE=2 \
    "$BIN" "$SRC" "$trace_dst" 1 0 0 0 0 "$PROFILE" 1
qa_assert_contains "$QA_LAST_LOG" '\x0A' "trace-escapes encodes newlines"
if grep -q '^\[APPLY_TRACE\] event=forged' "$QA_LAST_LOG"; then
    qa_fail "trace-escapes permitted a forged trace record"
fi

trace_utf8_dst="$QA_OUTDIR/trace-"$'\302\205'"[APPLY_TRACE] event=forged-utf8.tif"
qa_run trace-escapes-nonascii success "" env ICC_APPLY_TRACE=2 \
    "$BIN" "$SRC" "$trace_utf8_dst" 1 0 0 0 0 "$PROFILE" 1
qa_assert_contains "$QA_LAST_LOG" '\xC2\x85' "trace-escapes-nonascii encodes UTF-8 bytes"
if grep -q '^\[APPLY_TRACE\] event=forged-utf8' "$QA_LAST_LOG"; then
    qa_fail "trace-escapes-nonascii permitted a forged trace record"
fi

qa_finish
