#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/iccBenchApply-quick-check.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/qa-common.sh"
qa_init "iccBenchApply-quick-check"

BIN="$ICCDEV_BUILD_DIR/Tools/IccBenchApply/iccBenchApply"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"
COMMON_ARGS=(-pixels 4 -repeats 1 1)

qa_require_tool "$BIN"
qa_require_file "$PROFILE"
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

checksum_from_log() {
    sed -n 's/.*\(0x[0-9a-fA-F][0-9a-fA-F]*\)[[:space:]]*OK[[:space:]]*$/\1/p' "$1" | tail -1
}

qa_run plain success "" "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" 0
plain_checksum="$(checksum_from_log "$QA_LAST_LOG")"
[ -n "$plain_checksum" ] || qa_fail "plain run did not emit a checksum"

qa_run bpc success "" "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" 40
bpc_checksum="$(checksum_from_log "$QA_LAST_LOG")"
[ -n "$bpc_checksum" ] || qa_fail "BPC run did not emit a checksum"
if [ -n "$plain_checksum" ] && [ -n "$bpc_checksum" ] &&
   [ "$plain_checksum" != "$bpc_checksum" ]; then
    echo "[PASS] BPC changes the applied checksum"
else
    qa_fail "BPC did not change the applied checksum"
fi

qa_run luminance success "" "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" 100
luminance_checksum="$(checksum_from_log "$QA_LAST_LOG")"
qa_run luminance-nonzero-flag success "" "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" 200
luminance_flag_checksum="$(checksum_from_log "$QA_LAST_LOG")"
if [ -n "$luminance_checksum" ] &&
   [ "$luminance_checksum" = "$luminance_flag_checksum" ]; then
    echo "[PASS] nonzero luminance column behaves as a flag"
else
    qa_fail "luminance column values 1 and 2 produced different checksums"
fi

qa_run bpc-luminance success "" "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" 140
qa_run negative-intent reject "negative intent code is not a valid form" \
    "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" -1
qa_run invalid-decoded-intent reject "decoded intent out of range" \
    "$BIN" "${COMMON_ARGS[@]}" "$PROFILE" 45
qa_run missing-profile reject "Cannot open profile" \
    "$BIN" "${COMMON_ARGS[@]}" "$QA_OUTDIR/missing.icc" 0

qa_finish
