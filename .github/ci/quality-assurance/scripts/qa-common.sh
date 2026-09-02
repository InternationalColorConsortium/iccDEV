#!/usr/bin/env bash
#################################################################################
# .github/ci/quality-assurance/scripts/qa-common.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################
set -uo pipefail

QA_FAILURES=0
QA_LAST_LOG=""
QA_LAST_RC=0

qa_init() {
    local tag="$1"
    local script_dir repo_root

    script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd -P)"
    repo_root="$(git -C "$script_dir" rev-parse --show-toplevel)" ||
        { echo "error: unable to find the iccDEV repository root" >&2; exit 2; }

    ICCDEV_ROOT="${ICCDEV_ROOT:-$repo_root}"
    ICCDEV_BUILD_DIR="${ICCDEV_BUILD_DIR:-$ICCDEV_ROOT/Build}"
    QA_OUTDIR="${QA_OUTDIR:-$(mktemp -d "/tmp/${tag}.XXXXXX")}"
    mkdir -p "$QA_OUTDIR"

    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1:abort_on_error=1:symbolize=1:allocator_may_return_null=1}"
    export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:abort_on_error=1:print_stacktrace=1}"
    export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

    echo "[INFO] repository=$ICCDEV_ROOT build=$ICCDEV_BUILD_DIR output=$QA_OUTDIR"
}

qa_fail() {
    echo "[FAIL] $*"
    QA_FAILURES=$((QA_FAILURES + 1))
}

qa_require_file() {
    if [ ! -f "$1" ]; then
        qa_fail "missing file: $1"
    fi
}

qa_require_tool() {
    if [ ! -x "$1" ]; then
        qa_fail "missing executable: $1"
    fi
}

qa_scan_sanitizers() {
    local logfile="$1"

    if ! grep -qE 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL|SEGV' "$logfile" 2>/dev/null; then
        return 0
    fi

    qa_fail "sanitizer diagnostic in $logfile"
    grep -m 10 -E 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL|SEGV' "$logfile"
    return 1
}

qa_run() {
    local name="$1"
    local expectation="$2"
    local message="$3"
    local sanitizer_failed=0
    shift 3

    QA_LAST_LOG="$QA_OUTDIR/$name.log"
    timeout 30 "$@" >"$QA_LAST_LOG" 2>&1
    QA_LAST_RC=$?
    if ! qa_scan_sanitizers "$QA_LAST_LOG"; then
        sanitizer_failed=1
    fi

    if [ "$QA_LAST_RC" -eq 124 ]; then
        qa_fail "$name timed out"
        return
    fi
    if [ "$QA_LAST_RC" -ge 129 ] && [ "$QA_LAST_RC" -le 192 ]; then
        qa_fail "$name terminated by signal $((QA_LAST_RC - 128))"
        return
    fi

    case "$expectation" in
        success)
            if [ "$QA_LAST_RC" -ne 0 ]; then
                qa_fail "$name exited $QA_LAST_RC; expected success"
            elif [ "$sanitizer_failed" -eq 0 ]; then
                echo "[PASS] $name"
            fi
            ;;
        reject)
            if [ "$QA_LAST_RC" -eq 0 ]; then
                qa_fail "$name exited 0; expected rejection"
            elif [ -n "$message" ] && ! grep -qF "$message" "$QA_LAST_LOG"; then
                qa_fail "$name rejected without expected message: $message"
            elif [ "$sanitizer_failed" -eq 0 ]; then
                echo "[PASS] $name rejected cleanly"
            fi
            ;;
        *)
            qa_fail "$name has unknown expectation: $expectation"
            ;;
    esac
}

qa_assert_contains() {
    local file="$1"
    local text="$2"
    local label="$3"

    if grep -qF "$text" "$file"; then
        echo "[PASS] $label"
    else
        qa_fail "$label: missing '$text' in $file"
    fi
}

qa_finish() {
    if [ "$QA_FAILURES" -ne 0 ]; then
        echo "[FAIL] failures=$QA_FAILURES logs=$QA_OUTDIR"
        return 1
    fi

    echo "[PASS] failures=0 logs=$QA_OUTDIR"
    return 0
}
