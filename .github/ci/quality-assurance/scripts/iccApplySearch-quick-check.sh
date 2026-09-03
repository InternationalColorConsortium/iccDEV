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

BIN="$ICCDEV_BUILD_DIR/Tools/IccApplySearch/iccApplySearch"
DATA="$ICCDEV_ROOT/Testing/ApplyDataFiles/rgb8bit.txt"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"
CFG="$QA_OUTDIR/search.json"

qa_require_tool "$BIN"
qa_require_file "$DATA"
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
qa_run export-config success "" "$BIN" -exportcfganddata "$CFG" \
    "$DATA" 3 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1
qa_require_file "$CFG"
qa_run replay-config success "" "$BIN" -cfg "$CFG"
qa_run config-extra reject "Unexpected extra arguments for -cfg" "$BIN" -cfg "$CFG" ignored-extra
threaded_data="$QA_OUTDIR/rgb8bit-threaded.txt"
awk '
    /^icEncode/ { print; have_encoding = 1; next }
    !have_encoding { print; next }
    { rows = rows $0 ORS }
    END {
        for (i = 0; i < 64; i++)
            printf "%s", rows
    }
' "$DATA" > "$threaded_data"
qa_run threads-one success "" \
    "$BIN" -threads 1 "$threaded_data" 0 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1
threads_reference="$QA_LAST_LOG"
for nthreads in 0 2 4; do
    qa_run "threads-$nthreads" success "" \
        "$BIN" -threads "$nthreads" "$threaded_data" 0 0 "$PROFILE" 1 "$PROFILE" 1 -INIT 1
    if ! cmp -s "$threads_reference" "$QA_LAST_LOG"; then
        qa_fail "threads-$nthreads output differs from threads-one"
    fi
done

qa_finish
