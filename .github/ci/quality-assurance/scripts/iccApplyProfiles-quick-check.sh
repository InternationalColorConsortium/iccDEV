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

BIN="$ICCDEV_TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
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
qa_run export-config success "" "$BIN" --exportcfg "$CFG" "$SRC" "$QA_OUTDIR/export.tif" 1 0 0 0 0 "$PROFILE" 1
qa_require_file "$CFG"
qa_run replay-config success "" "$BIN" --cfg "$CFG"
qa_run config-extra reject "Unexpected extra arguments for --cfg" "$BIN" --cfg "$CFG" ignored-extra
qa_run legacy-extra reject "Unexpected extra arguments" \
    "$BIN" "$SRC" "$QA_OUTDIR/legacy-extra.tif" 1 0 0 0 0 "$PROFILE" 1 ignored-extra
qa_run threads-one success "" \
    "$BIN" --threads 1 "$SRC" "$QA_OUTDIR/threads-one.tif" 1 0 0 0 0 "$PROFILE" 1
qa_run legacy-single-dash-options success "" \
    "$BIN" -threads 1 -exportcfg "$QA_OUTDIR/legacy-options.json" \
    "$SRC" "$QA_OUTDIR/legacy-options.tif" 1 0 0 0 0 "$PROFILE" 1
qa_require_file "$QA_OUTDIR/legacy-options.json"
qa_run threads-before-human-telemetry success "effective_threads=4" \
    "$BIN" --threads 4 --telemetry=human \
    "$SRC" "$QA_OUTDIR/threads-before-human-telemetry.tif" 1 0 0 0 0 "$PROFILE" 1
qa_run human-telemetry-before-threads success "effective_threads=4" \
    "$BIN" --telemetry=human --threads 4 \
    "$SRC" "$QA_OUTDIR/human-telemetry-before-threads.tif" 1 0 0 0 0 "$PROFILE" 1
if ! grep -Eq '^\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\] iccApplyProfiles: started .*requested_threads=4 effective_threads=4 mode=' "$QA_LAST_LOG"; then
    qa_fail "human telemetry start line is missing its UTC timestamp or thread context"
fi
if ! grep -Eq '^\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\] iccApplyProfiles: progress .*percent=100\.000 .*eta_ms=0 .*requested_threads=4 effective_threads=4 mode=' "$QA_LAST_LOG"; then
    qa_fail "human telemetry progress line is missing computed progress or thread context"
fi
if grep -Eq '[0-9]+%' "$QA_LAST_LOG"; then
    qa_fail "human telemetry duplicated the legacy stdout percentage"
fi
if [[ "$(tail -c 1 "$QA_LAST_LOG" | od -An -tu1 | tr -d '[:space:]')" != "10" ]]; then
    qa_fail "human telemetry output does not end with a newline"
fi

TELEMETRY_BASELINE="$QA_OUTDIR/telemetry-baseline.tif"
TELEMETRY_OFF_OUTPUT="$QA_OUTDIR/telemetry-off.tif"
TELEMETRY_EVIDENCE_ONLY_OUTPUT="$QA_OUTDIR/telemetry-evidence-only.tif"
TELEMETRY_EVIDENCE_ONLY="$QA_OUTDIR/telemetry-evidence-only.json"
TELEMETRY_OUTPUT="$QA_OUTDIR/telemetry-output.tif"
TELEMETRY_JSONL="$QA_OUTDIR/telemetry.jsonl"
TELEMETRY_EVIDENCE="$QA_OUTDIR/telemetry-evidence.json"
qa_run telemetry-default success "" \
    "$BIN" "$SRC" "$TELEMETRY_BASELINE" 1 0 0 1 0 "$PROFILE" 1
qa_run telemetry-off-quiet success "" \
    "$BIN" --telemetry=off --quiet \
    "$SRC" "$TELEMETRY_OFF_OUTPUT" 1 0 0 1 0 "$PROFILE" 1
qa_run telemetry-evidence-only success "" \
    "$BIN" --evidence-file "$TELEMETRY_EVIDENCE_ONLY" --quiet \
    "$SRC" "$TELEMETRY_EVIDENCE_ONLY_OUTPUT" 1 0 0 1 0 "$PROFILE" 1
qa_run telemetry-jsonl success "" \
    "$BIN" --telemetry=jsonl --telemetry-file "$TELEMETRY_JSONL" \
    --evidence-file "$TELEMETRY_EVIDENCE" --quiet \
    "$SRC" "$TELEMETRY_OUTPUT" 1 0 0 1 0 "$PROFILE" 1
if [ -s "$TELEMETRY_JSONL" ] && [ -s "$TELEMETRY_EVIDENCE" ] &&
   [ -s "$TELEMETRY_EVIDENCE_ONLY" ]; then
    if command -v python3 >/dev/null 2>&1; then
        if python3 - "$TELEMETRY_JSONL" "$TELEMETRY_EVIDENCE" \
            "$TELEMETRY_EVIDENCE_ONLY" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="ascii") as stream:
    events = [json.loads(line) for line in stream if line.strip()]
with open(sys.argv[2], encoding="ascii") as stream:
    receipt = json.load(stream)
with open(sys.argv[3], encoding="ascii") as stream:
    evidence_only = json.load(stream)

assert [event["sequence"] for event in events] == list(range(1, len(events) + 1))
assert events[0]["event"] == "run_started"
assert events[-1]["event"] == "run_completed"
assert sum(event["event"] == "run_completed" for event in events) == 1
ready = next(event for event in events if event["event"] == "transform_ready")
progress = [event for event in events if event["event"] == "progress"]
completed = events[-1]
assert ready["effective_threads"] >= 1
assert ready["rows_per_apply"] >= 1
assert ready["pixels_per_apply"] >= 1
assert ready["total_batches"] >= 1
assert progress
assert progress[-1]["completed_rows"] == progress[-1]["total_rows"]
assert progress[-1]["completed_pixels"] == progress[-1]["total_pixels"]
assert progress[-1]["percent_complete"] == 100.0
assert progress[-1]["elapsed_ms"] >= 0
assert progress[-1]["throughput_pixels_per_second"] >= 0
assert progress[-1]["estimated_remaining_ms"] == 0
assert progress[-1]["estimated_total_ms"] is None or progress[-1]["estimated_total_ms"] >= 0
assert progress[-1]["requested_threads"] == 1
assert progress[-1]["effective_threads"] == 1
assert progress[-1]["execution_mode"] == "single-pixel"
assert completed["effective_threads"] >= 1
assert completed["completed_pixels"] == completed["total_pixels"]
assert completed["total_pixels"] == ready["input_width"] * ready["input_height"]
assert completed["throughput_pixels_per_second"] >= 0
assert receipt["status"] == "success"
assert receipt["output_digest"]
assert receipt["effective_threads"] >= 1
assert receipt["pixels_per_apply"] >= 1
assert receipt["completed_pixels"] == receipt["total_pixels"]
assert receipt["throughput_pixels_per_second"] >= 0
assert evidence_only["status"] == "success"
assert evidence_only["output_digest"] == receipt["output_digest"]
PY
        then
            echo "[PASS] telemetry JSONL lifecycle and final evidence receipt"
        else
            qa_fail "telemetry JSONL lifecycle or final evidence receipt is invalid"
        fi
    else
        echo "[SKIP] telemetry JSON validation requires python3"
    fi
else
    qa_fail "telemetry JSONL or final evidence receipt is missing"
fi
if cmp -s "$TELEMETRY_BASELINE" "$TELEMETRY_OUTPUT" &&
   cmp -s "$TELEMETRY_BASELINE" "$TELEMETRY_OFF_OUTPUT" &&
   cmp -s "$TELEMETRY_BASELINE" "$TELEMETRY_EVIDENCE_ONLY_OUTPUT"; then
    echo "[PASS] telemetry and evidence modes leave TIFF output byte-identical"
else
    qa_fail "telemetry changed TIFF output bytes"
fi
qa_run telemetry-existing-sidecar reject "Telemetry file already exists" \
    "$BIN" --telemetry=jsonl --telemetry-file "$TELEMETRY_JSONL" \
    "$SRC" "$QA_OUTDIR/telemetry-existing-sidecar.tif" 1 0 0 0 0 "$PROFILE" 1

qa_run deep-options success "" \
    "$BIN" "$SRC" "$QA_OUTDIR/deep-options.tif" 2 1 0 1 1 "$PROFILE" 12
[ -s "$QA_OUTDIR/deep-options.tif" ] || qa_fail "deep-options did not create a TIFF"
if command -v tiffinfo >/dev/null 2>&1; then
    tiffinfo "$QA_OUTDIR/deep-options.tif" >"$QA_OUTDIR/tiff-deep.log" 2>&1
    qa_assert_contains "$QA_OUTDIR/tiff-deep.log" "Bits/Sample: 16" "deep lane writes 16-bit samples"
    qa_assert_contains "$QA_OUTDIR/tiff-deep.log" "Compression Scheme: LZW" "deep lane writes LZW"
fi

qa_run threads-four-row success "" \
    "$BIN" --threads 4 "$SRC" "$QA_OUTDIR/threads-four-row.tif" 3 1 1 1 1 "$PROFILE" 40
#################################################################################
# connect.useSearch: build a CIccCmmSearch instead of a forward CIccCmm.
#
# Every config below is written as JSON because useSearch is --cfg only: the
# positional form has no room for pccWeights or the reverse-search start.
#################################################################################

# One profile stage, as a JSON object.  $1 is the ICC path ("" selects the
# source image's embedded profile).
search_stage() {
    printf '{ "iccFile": "%s", "intent": "relative" }' "$1"
}

# Destination float encoding keeps the search residual, which is what separates
# a search apply from the forward chain: inverting a matrix/TRC profile lands
# within search tolerance rather than exactly, while the forward chain is an
# analytic round trip.  At 8 bit both quantize to the same bytes.
write_search_cfg() {
    local cfg="$1" src="$2" dst="$3" use_search="$4" threads="$5" extra="$6"
    shift 6

    local stages="" stage
    for stage in "$@"; do
        [ -n "$stages" ] && stages="$stages,"
        stages="$stages$(search_stage "$stage")"
    done

    # A search chain lives in "searchApply", the same block iccApplySearch uses;
    # forward mode keeps the top-level profileSequence.
    local key='"profileSequence"' close=""
    if [ "$use_search" = "true" ]; then
        key='"searchApply": { "profileSequence"'
        close=" }"
    fi

    cat >"$cfg" <<EOF
{
  "imageFiles": { "srcImageFile": "$src", "dstImageFile": "$dst",
                  "dstEncoding": "float", "dstEmbedIcc": false },
  "connect": { "threads": $threads, "useSearch": $use_search },
  $key: [ $stages ]$extra$close
}
EOF
}

qa_assert_files_match() {
    if cmp -s "$1" "$2"; then
        echo "[PASS] $3"
    else
        qa_fail "$3: '$1' and '$2' differ"
    fi
}

qa_assert_files_differ() {
    if cmp -s "$1" "$2"; then
        qa_fail "$3: '$1' and '$2' are identical"
    else
        echo "[PASS] $3"
    fi
}

SEARCH_TWO="$QA_OUTDIR/search-two.tif"
SEARCH_TWO_MT="$QA_OUTDIR/search-two-mt.tif"
FORWARD_FLOAT="$QA_OUTDIR/forward-float.tif"

write_search_cfg "$QA_OUTDIR/search-two.json" "$SRC" "$SEARCH_TWO" true 1 "" \
    "$PROFILE" "$PROFILE"
qa_run search-two-profile success "" "$BIN" --cfg "$QA_OUTDIR/search-two.json"
[ -s "$SEARCH_TWO" ] || qa_fail "search-two-profile did not create a TIFF"

# Each CIccThreadedCmm worker gets its own CIccApplyCmmSearch, which owns a
# private CIccApplyCmm per sub-chain, and the per-pixel search carries nothing
# between pixels -- so strip partitioning must be bit-for-bit reproducible.
write_search_cfg "$QA_OUTDIR/search-two-mt.json" "$SRC" "$SEARCH_TWO_MT" true 0 "" \
    "$PROFILE" "$PROFILE"
qa_run search-two-profile-threaded success "" "$BIN" --cfg "$QA_OUTDIR/search-two-mt.json"
if [ -s "$SEARCH_TWO" ] && [ -s "$SEARCH_TWO_MT" ]; then
    qa_assert_files_match "$SEARCH_TWO" "$SEARCH_TWO_MT" \
        "search apply is stable across threads 1 and threads 0"
fi

for rows in 2 8; do
    band_output="$QA_OUTDIR/search-band-$rows.tif"
    band_events="$QA_OUTDIR/search-band-$rows.jsonl"
    write_search_cfg "$QA_OUTDIR/search-band-$rows.json" "$SRC" \
        "$band_output" true 4 "" "$PROFILE" "$PROFILE"
    qa_run "search-band-$rows" success "" \
        env ICC_APPLY_PROFILES_BAND_ROWS="$rows" "$BIN" \
        --telemetry=jsonl --telemetry-file "$band_events" \
        --cfg "$QA_OUTDIR/search-band-$rows.json"
    if [ -s "$band_output" ] && [ -s "$band_events" ]; then
        qa_assert_files_match "$SEARCH_TWO" "$band_output" \
            "search band $rows preserves the serial TIFF"
        qa_assert_contains "$band_events" "\"rows_per_apply\":$rows" \
            "search band $rows reports its applied row count"
    else
        qa_fail "search band $rows did not create TIFF and telemetry"
    fi
done

write_search_cfg "$QA_OUTDIR/search-band-invalid.json" "$SRC" \
    "$QA_OUTDIR/search-band-invalid.tif" true 4 "" "$PROFILE" "$PROFILE"
qa_run search-band-invalid reject "Invalid ICC_APPLY_PROFILES_BAND_ROWS" \
    env ICC_APPLY_PROFILES_BAND_ROWS=0 "$BIN" \
    --cfg "$QA_OUTDIR/search-band-invalid.json"

write_search_cfg "$QA_OUTDIR/forward-float.json" "$SRC" "$FORWARD_FLOAT" false 1 "" \
    "$PROFILE" "$PROFILE"
qa_run search-forward-baseline success "" "$BIN" --cfg "$QA_OUTDIR/forward-float.json"
if [ -s "$SEARCH_TWO" ] && [ -s "$FORWARD_FLOAT" ]; then
    qa_assert_files_differ "$SEARCH_TWO" "$FORWARD_FLOAT" \
        "search apply differs from the same chain applied forward"
fi

# Embedded source profile: an empty first iccFile still means "use the ICC
# profile carried by the source TIFF", which CreateSearch honours through the
# memory AddXform overload.  Build a source that actually carries one first.
EMBED_SRC="$QA_OUTDIR/embedded-src.tif"
qa_run search-embedded-seed success "" \
    "$BIN" "$SRC" "$EMBED_SRC" 1 0 0 1 0 "$PROFILE" 1
if [ -s "$EMBED_SRC" ]; then
    write_search_cfg "$QA_OUTDIR/search-embedded.json" "$EMBED_SRC" \
        "$QA_OUTDIR/search-embedded.tif" true 1 "" "" "$PROFILE"
    qa_run search-embedded success "" "$BIN" --cfg "$QA_OUTDIR/search-embedded.json"
    [ -s "$QA_OUTDIR/search-embedded.tif" ] ||
        qa_fail "search-embedded did not create a TIFF"
else
    qa_fail "search-embedded-seed did not create a source TIFF with an embedded profile"
fi

# A 3-profile search needs the weighted PCC set it optimises across.  The Lab
# PCC is generated on demand so this is not a permanent skip on a tree where the
# hybrid pipeline has not run.
PCC_ICC=""
FROMXML="$ICCDEV_TOOLS_DIR/IccFromXml/iccFromXml"
if [ -x "$FROMXML" ]; then
    mkdir -p "$QA_OUTDIR/pcc"
    if "$FROMXML" "$ICCDEV_ROOT/Testing/PCC/Lab_float-D50_2deg.xml" \
                  "$QA_OUTDIR/pcc/Lab_float-D50_2deg.icc" >/dev/null 2>&1; then
        PCC_ICC="$QA_OUTDIR/pcc/Lab_float-D50_2deg.icc"
    fi
fi
if [ -z "$PCC_ICC" ]; then
    for candidate in \
        "$ICCDEV_ROOT/Testing/hybrid/ICC/Lab_float-D50_2deg.icc" \
        "$ICCDEV_ROOT/Testing/PCC/Lab_float-D50_2deg.icc"; do
        if [ -f "$candidate" ]; then PCC_ICC="$candidate"; break; fi
    done
fi

if [ -n "$PCC_ICC" ]; then
    write_search_cfg "$QA_OUTDIR/search-three.json" "$SRC" \
        "$QA_OUTDIR/search-three.tif" true 1 \
        ",
  \"initial\": { \"intent\": \"relative\" },
  \"pccWeights\": [ { \"pccFile\": \"$PCC_ICC\", \"weight\": 1.0 } ]" \
        "$PROFILE" "$PROFILE" "$PROFILE"
    qa_run search-three-profile success "" "$BIN" --cfg "$QA_OUTDIR/search-three.json"
    [ -s "$QA_OUTDIR/search-three.tif" ] ||
        qa_fail "search-three-profile did not create a TIFF"
else
    echo "[SKIP] search-three-profile: no Lab PCC profile available"
fi

# Negatives.  Each must exit non-zero with the message that names the rule
# rather than an icStatusCMM number from deep inside the library.
write_search_cfg "$QA_OUTDIR/search-one.json" "$SRC" "$QA_OUTDIR/search-one.tif" \
    true 1 "" "$PROFILE"
qa_run search-one-profile reject \
    "useSearch requires 2 or 3 profiles in profileSequence (found 1)" \
    "$BIN" --cfg "$QA_OUTDIR/search-one.json"

write_search_cfg "$QA_OUTDIR/search-four.json" "$SRC" "$QA_OUTDIR/search-four.tif" \
    true 1 "" "$PROFILE" "$PROFILE" "$PROFILE" "$PROFILE"
qa_run search-four-profiles reject \
    "useSearch requires 2 or 3 profiles in profileSequence (found 4)" \
    "$BIN" --cfg "$QA_OUTDIR/search-four.json"

write_search_cfg "$QA_OUTDIR/search-three-nopcc.json" "$SRC" \
    "$QA_OUTDIR/search-three-nopcc.tif" true 1 "" "$PROFILE" "$PROFILE" "$PROFILE"
qa_run search-three-no-pccweights reject \
    "useSearch with 3 profiles requires at least one pccWeights entry" \
    "$BIN" --cfg "$QA_OUTDIR/search-three-nopcc.json"

cat >"$QA_OUTDIR/search-nochain.json" <<EOF
{
  "imageFiles": { "srcImageFile": "$SRC", "dstImageFile": "$QA_OUTDIR/search-nochain.tif" },
  "connect": { "threads": 1, "useSearch": true }
}
EOF
qa_run search-no-searchapply reject \
    "Unable to parse searchApply configuration" \
    "$BIN" --cfg "$QA_OUTDIR/search-nochain.json"

# A searchApply block with useSearch off must be refused, not quietly applied
# as a forward chain.  Written directly: the helper only emits searchApply in
# search mode, and this case is deliberately the mismatched pair.
cat >"$QA_OUTDIR/search-block-off.json" <<EOF
{
  "imageFiles": { "srcImageFile": "$SRC", "dstImageFile": "$QA_OUTDIR/search-block-off.tif" },
  "connect": { "threads": 1, "useSearch": false },
  "profileSequence": [ $(search_stage "$PROFILE"),$(search_stage "$PROFILE") ],
  "searchApply": {
    "profileSequence": [ $(search_stage "$PROFILE"),$(search_stage "$PROFILE") ],
    "pccWeights": [ { "pccFile": "$PROFILE", "weight": 1.0 } ]
  }
}
EOF
qa_run search-block-without-usesearch reject \
    "'searchApply' requires connect.useSearch to be true" \
    "$BIN" --cfg "$QA_OUTDIR/search-block-off.json"

qa_finish
