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

BIN="$ICCDEV_TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
FROM_XML="$ICCDEV_TOOLS_DIR/IccFromXml/iccFromXml"
DATA="$ICCDEV_ROOT/.github/ci/test-data/test-data-rgb-16bit.txt"
DATA8="$ICCDEV_ROOT/Testing/ApplyDataFiles/rgb8bit.txt"
FLOAT_DATA="$ICCDEV_ROOT/.github/ci/test-data/test-data-rgb-float.txt"
CMYK_DATA="$ICCDEV_ROOT/Testing/hybrid/Data/cmykGrays.txt"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"
BRDF_XML="$ICCDEV_ROOT/.github/ci/test-data/dtob-brdf.xml"
CALC_XML="$ICCDEV_ROOT/Testing/Calc/argbCalc.xml"
HYBRID_XML="$ICCDEV_ROOT/Testing/hybrid/CMYK_Hybrid_Profile.xml"
SPECTRAL_XML="$ICCDEV_ROOT/Testing/PCC/Spec380_10_730-D50_2deg.xml"
BRDF="$QA_OUTDIR/dtob-brdf.icc"
CALC="$QA_OUTDIR/argbCalc.icc"
HYBRID="$QA_OUTDIR/CMYK_Hybrid_Profile.icc"
SPECTRAL="$QA_OUTDIR/Spec380_10_730-D50_2deg.icc"
CFG="$QA_OUTDIR/named.json"
TELEMETRY="$QA_OUTDIR/named.jsonl"
EVIDENCE="$QA_OUTDIR/named-evidence.json"

qa_require_tool "$BIN"
qa_require_tool "$FROM_XML"
for fixture in "$DATA" "$DATA8" "$FLOAT_DATA" "$CMYK_DATA" "$PROFILE" \
    "$BRDF_XML" "$CALC_XML" "$HYBRID_XML" "$SPECTRAL_XML"; do
    qa_require_file "$fixture"
done
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

qa_run build-brdf success "" "$FROM_XML" "$BRDF_XML" "$BRDF"
qa_run build-calc success "" "$FROM_XML" "$CALC_XML" "$CALC"
qa_run build-hybrid success "" "$FROM_XML" "$HYBRID_XML" "$HYBRID"
qa_run build-spectral success "" "$FROM_XML" "$SPECTRAL_XML" "$SPECTRAL"

qa_run basic success "" "$BIN" "$DATA8" 0 0 "$PROFILE" 1
qa_run v5-brdf-direct success "" "$BIN" "$DATA" 6 1 "$BRDF" 10063
qa_run v5-spectral-chain success "" "$BIN" \
    "$CMYK_DATA" 3 1 "$HYBRID" 10103 "$SPECTRAL" 10
qa_run debug success "" "$BIN" -debugcalc "$FLOAT_DATA" 3:8:12 0 "$CALC" 0
qa_run environment success "" "$BIN" "$DATA" 0 0 \
    -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 "$PROFILE" 1
qa_run pcc success "" "$BIN" "$DATA" 0 0 "$PROFILE" 1 -PCC "$PROFILE"
qa_run telemetry-human success "iccApplyNamedCmm: completed" \
    "$BIN" --telemetry=human "$DATA8" 0 0 "$PROFILE" 1
if ! grep -Eq '^\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\] iccApplyNamedCmm: completed ' "$QA_LAST_LOG"; then
    qa_fail "NamedCmm human telemetry completion is missing its UTC timestamp"
fi
if [[ "$(tail -c 1 "$QA_LAST_LOG" | od -An -tu1 | tr -d '[:space:]')" != "10" ]]; then
    qa_fail "NamedCmm human telemetry output does not end with a newline"
fi
qa_run export-config success "" "$BIN" --exportcfganddata "$CFG" "$DATA" 0 0 "$PROFILE" 1
qa_require_file "$CFG"
qa_run telemetry-jsonl success "" \
    "$BIN" --telemetry=jsonl --telemetry-file "$TELEMETRY" \
    --evidence-file "$EVIDENCE" --cfg "$CFG"
qa_require_file "$TELEMETRY"
qa_require_file "$EVIDENCE"
if command -v python3 >/dev/null 2>&1; then
    if python3 - "$TELEMETRY" "$EVIDENCE" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="ascii") as stream:
    events = [json.loads(line) for line in stream if line.strip()]
with open(sys.argv[2], encoding="ascii") as stream:
    receipt = json.load(stream)

assert [event["sequence"] for event in events] == list(range(1, len(events) + 1))
assert events[0]["event"] == "run_started"
assert events[-1]["event"] == "run_completed"
assert sum(event["event"] == "run_completed" for event in events) == 1
assert receipt["status"] == "success"
assert receipt["output_records"] >= 1
PY
    then
        echo "[PASS] NamedCmm telemetry JSONL lifecycle and evidence receipt"
    else
        qa_fail "NamedCmm telemetry JSONL lifecycle or evidence receipt is invalid"
    fi
else
    echo "[SKIP] NamedCmm JSON validation requires python3"
fi
RECEIPT_FAILURE_DIR="$QA_OUTDIR/receipt-write-failure"
RECEIPT_FAILURE_TELEMETRY="$QA_OUTDIR/receipt-write-failure.jsonl"
# A missing parent fails on both root and non-root runners. chmod 500 does not
# prevent writes when the CI container runs as root.
qa_run receipt-write-failure reject "Unable to create Evidence file" \
    "$BIN" --telemetry=jsonl --telemetry-file "$RECEIPT_FAILURE_TELEMETRY" \
    --evidence-file "$RECEIPT_FAILURE_DIR/receipt.json" --cfg "$CFG"
if command -v python3 >/dev/null 2>&1; then
    if python3 - "$RECEIPT_FAILURE_TELEMETRY" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="ascii") as stream:
    events = [json.loads(line) for line in stream if line.strip()]

assert events[-1]["event"] == "run_failed"
assert events[-1]["reason"] == "evidence_write_failed"
assert all(event["event"] != "run_completed" for event in events)
PY
    then
        echo "[PASS] NamedCmm receipt write failure emits only run_failed"
    else
        qa_fail "NamedCmm receipt failure emitted a successful terminal event"
    fi
else
    echo "[SKIP] NamedCmm receipt failure JSON validation requires python3"
fi
qa_run replay-config success "" "$BIN" --cfg "$CFG"
qa_run config-extra reject "Unexpected extra arguments for --cfg" "$BIN" --cfg "$CFG" ignored-extra
qa_run legacy-extra reject "Unexpected extra arguments" "$BIN" "$DATA" 0 0 "$PROFILE" 1 ignored-extra

qa_finish
