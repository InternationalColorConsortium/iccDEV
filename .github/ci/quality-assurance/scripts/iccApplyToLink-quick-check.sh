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

BIN="$ICCDEV_TOOLS_DIR/IccApplyToLink/iccApplyToLink"
FROM_CUBE="$ICCDEV_TOOLS_DIR/IccFromCube/iccFromCube"
FROM_XML="$ICCDEV_TOOLS_DIR/IccFromXml/iccFromXml"
DUMP_PROFILE="$ICCDEV_TOOLS_DIR/IccDumpProfile/iccDumpProfile"
PROFILE="$ICCDEV_ROOT/Testing/sRGB_v4_ICC_preference.icc"
PCC_PROFILE="$ICCDEV_ROOT/Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
ENV_PROFILE_XML="$ICCDEV_ROOT/Testing/Display/RgbGSDF.xml"
IDENTITY_CUBE="$ICCDEV_ROOT/.github/ci/test-data/test-identity.cube"
NEAR_DEFAULT_CUBE="$ICCDEV_ROOT/.github/ci/test-data/test-near-default-domain.cube"
LEGACY_RANGE_CUBE="$ICCDEV_ROOT/.github/ci/test-data/test-legacy-input-range.cube"
INPUT_VIDEO_RANGE_CUBE="$ICCDEV_ROOT/.github/ci/test-data/test-input-video-range.cube"
OUTPUT_VIDEO_RANGE_CUBE="$ICCDEV_ROOT/.github/ci/test-data/test-output-video-range.cube"
TELEMETRY="$QA_OUTDIR/link.jsonl"
EVIDENCE="$QA_OUTDIR/link-evidence.json"

qa_require_tool "$BIN"
qa_require_tool "$FROM_CUBE"
qa_require_tool "$FROM_XML"
qa_require_tool "$DUMP_PROFILE"
qa_require_file "$PROFILE"
qa_require_file "$PCC_PROFILE"
qa_require_file "$ENV_PROFILE_XML"
qa_require_file "$IDENTITY_CUBE"
qa_require_file "$NEAR_DEFAULT_CUBE"
qa_require_file "$LEGACY_RANGE_CUBE"
qa_require_file "$INPUT_VIDEO_RANGE_CUBE"
qa_require_file "$OUTPUT_VIDEO_RANGE_CUBE"
if [ "$QA_FAILURES" -ne 0 ]; then
    qa_finish
    exit $?
fi

qa_assert_cube_range() {
    local file="$1"
    local expected_min="$2"
    local expected_max="$3"
    local label="$4"
    local min_line max_line min_keyword max_keyword
    local min_r min_g min_b min_extra max_r max_g max_b max_extra

    min_line="$(grep -m 1 '^DOMAIN_MIN ' "$file" 2>/dev/null || true)"
    max_line="$(grep -m 1 '^DOMAIN_MAX ' "$file" 2>/dev/null || true)"
    read -r min_keyword min_r min_g min_b min_extra <<< "$min_line"
    read -r max_keyword max_r max_g max_b max_extra <<< "$max_line"
    if [ "$min_keyword" != "DOMAIN_MIN" ] || [ "$max_keyword" != "DOMAIN_MAX" ] ||
       [ -z "$min_r" ] || [ -z "$min_g" ] || [ -z "$min_b" ] ||
       [ -z "$max_r" ] || [ -z "$max_g" ] || [ -z "$max_b" ] ||
       [ -n "${min_extra:-}" ] || [ -n "${max_extra:-}" ]; then
        qa_fail "$label: missing or malformed DOMAIN_MIN/DOMAIN_MAX in $file"
        return
    fi

    if awk -v min_r="$min_r" -v min_g="$min_g" -v min_b="$min_b" \
           -v max_r="$max_r" -v max_g="$max_g" -v max_b="$max_b" \
           -v expected_min="$expected_min" -v expected_max="$expected_max" '
        function abs(value) { return value < 0 ? -value : value }
        BEGIN {
            min_tol = abs(expected_min) * 1e-6 + 1e-38
            max_tol = abs(expected_max) * 1e-6 + 1e-38
            if (!(min_r < max_r) || !(min_g < max_g) || !(min_b < max_b) ||
                abs(min_r - expected_min) > min_tol ||
                abs(min_g - expected_min) > min_tol ||
                abs(min_b - expected_min) > min_tol ||
                abs(max_r - expected_max) > max_tol ||
                abs(max_g - expected_max) > max_tol ||
                abs(max_b - expected_max) > max_tol)
                exit 1
        }'; then
        echo "[PASS] $label"
    else
        qa_fail "$label: expected three ordered channels at $expected_min $expected_max"
    fi
}

qa_assert_cube_identity_order() {
    local file="$1"
    local label="$2"

    if awk '
        BEGIN {
            expected[1] = "0 0 0"
            expected[2] = "1 0 0"
            expected[3] = "0 1 0"
            expected[4] = "1 1 0"
            expected[5] = "0 0 1"
            expected[6] = "1 0 1"
            expected[7] = "0 1 1"
            expected[8] = "1 1 1"
        }
        NF == 3 {
            count++
            split(expected[count], value, " ")
            if (($1 + 0) != value[1] || ($2 + 0) != value[2] ||
                ($3 + 0) != value[3])
                bad = 1
        }
        END { exit bad || count != 8 }
    ' "$file"; then
        echo "[PASS] $label"
    else
        qa_fail "$label: Cube rows are not red-fastest identity order"
    fi
}

qa_check_intent_matrix() {
    local matrix_dir="$QA_OUTDIR/intent-matrix"
    local output="$matrix_dir/output.icc"
    local failures_before="$QA_FAILURES"
    local modifier type intent code unit log rc

    mkdir -p "$matrix_dir"

    # Every canonical combination must pass the decoder. Profile-dependent
    # transform families may still be rejected later by AddXform(), so the
    # parser diagnostic, sanitizer scan, timeout, and signal status are the
    # oracles here rather than exit zero alone.
    for modifier in 0 100 1000 1100; do
        for type in 0 1 2 3 4 5 6 7 8 9; do
            for intent in 0 1 2 3; do
                code=$((modifier + type * 10 + intent))
                log="$matrix_dir/accept-$code.log"
                timeout "$QA_TIMEOUT_SECONDS" "$BIN" "$output" \
                    0 2 1 IntentMatrix 0 1 0 0 "$PROFILE" "$code" \
                    >"$log" 2>&1
                rc=$?
                qa_scan_sanitizers "$log" >/dev/null || true
                if [ "$rc" -eq 124 ]; then
                    qa_fail "intent matrix code $code timed out"
                elif [ "$rc" -ge 129 ] && [ "$rc" -le 192 ]; then
                    qa_fail "intent matrix code $code terminated by signal $((rc - 128))"
                elif grep -qF "Invalid rendering intent" "$log"; then
                    qa_fail "intent matrix rejected canonical code $code"
                fi
            done
        done
    done

    # Units 4..9 are invalid for every transform type and modifier combination.
    for modifier in 0 100 1000 1100; do
        for type in 0 1 2 3 4 5 6 7 8 9; do
            for unit in 4 5 6 7 8 9; do
                code=$((modifier + type * 10 + unit))
                log="$matrix_dir/reject-$code.log"
                timeout "$QA_TIMEOUT_SECONDS" "$BIN" "$output" \
                    0 2 1 IntentMatrix 0 1 0 0 "$PROFILE" "$code" \
                    >"$log" 2>&1
                rc=$?
                qa_scan_sanitizers "$log" >/dev/null || true
                if [ "$rc" -eq 0 ]; then
                    qa_fail "intent matrix accepted invalid units code $code"
                elif [ "$rc" -eq 124 ]; then
                    qa_fail "invalid intent matrix code $code timed out"
                elif [ "$rc" -ge 129 ] && [ "$rc" -le 192 ]; then
                    qa_fail "invalid intent matrix code $code terminated by signal $((rc - 128))"
                elif ! grep -qF "decoded intent is out of range" "$log"; then
                    qa_fail "invalid intent matrix code $code lacked its decoder diagnostic"
                fi
            done
        done
    done

    if [ "$QA_FAILURES" -eq "$failures_before" ]; then
        echo "[PASS] rendering-intent decoder matrix (160 accepted, 240 rejected)"
    fi
}

qa_assert_same_file() {
    local first="$1"
    local second="$2"
    local label="$3"

    if cmp -s "$first" "$second"; then
        echo "[PASS] $label"
    else
        qa_fail "$label: files differ: $first $second"
    fi
}

qa_assert_different_file() {
    local first="$1"
    local second="$2"
    local label="$3"

    if cmp -s "$first" "$second"; then
        qa_fail "$label: files are identical: $first $second"
    else
        echo "[PASS] $label"
    fi
}

qa_run_stderr_reject() {
    local name="$1"
    local message="$2"
    local stdout_log="$QA_OUTDIR/$name.stdout"
    local stderr_log="$QA_OUTDIR/$name.stderr"
    local rc
    shift 2

    timeout "$QA_TIMEOUT_SECONDS" "$@" >"$stdout_log" 2>"$stderr_log"
    rc=$?
    qa_scan_sanitizers "$stdout_log" >/dev/null || true
    qa_scan_sanitizers "$stderr_log" >/dev/null || true
    if [ "$rc" -eq 0 ]; then
        qa_fail "$name exited 0; expected rejection"
    elif [ "$rc" -eq 124 ]; then
        qa_fail "$name timed out"
    elif [ "$rc" -ge 129 ] && [ "$rc" -le 192 ]; then
        qa_fail "$name terminated by signal $((rc - 128))"
    elif [ -s "$stdout_log" ]; then
        qa_fail "$name wrote its argument diagnostic to stdout"
    elif ! grep -qF "$message" "$stderr_log"; then
        qa_fail "$name rejected without stderr diagnostic: $message"
    else
        echo "[PASS] $name rejected on stderr"
    fi
}

qa_check_devicelink_intents() {
    local option version intent intent_name output

    for option in 0 1; do
        if [ "$option" -eq 0 ]; then
            version="v4"
        else
            version="v5"
        fi
        for intent in 0 1 2 3; do
            case "$intent" in
                0) intent_name="Perceptual" ;;
                1) intent_name="Relative Colorimetric" ;;
                2) intent_name="Saturation" ;;
                3) intent_name="Absolute Colorimetric" ;;
            esac
            output="$QA_OUTDIR/header-$version-intent-$intent.icc"
            qa_run "header-$version-intent-$intent" success "" \
                "$BIN" "$output" 0 2 "$option" "QA $version intent" \
                0 1 0 0 "$PROFILE" "$intent"
            qa_run "header-$version-intent-$intent-dump" success "" \
                "$DUMP_PROFILE" -v "$output"
            qa_assert_contains "$QA_LAST_LOG" \
                "Rendering Intent:   $intent_name" \
                "$version header records intent $intent"
            qa_assert_contains "$QA_LAST_LOG" "Profile is valid" \
                "$version intent $intent output validates"
            if grep -Eq 'NonCompliant!|Critical Error' "$QA_LAST_LOG"; then
                qa_fail "$version intent $intent output has a validation error"
            fi
        done
    done
}

qa_run help success "" "$BIN" --help
qa_assert_contains "$QA_LAST_LOG" \
    "0 through 20 - digits after the decimal point for .cube LUT samples" \
    "Help defines Cube sample precision"
qa_assert_contains "$QA_LAST_LOG" "90 + Intent - Colorimetric Only" \
    "Help lists the Colorimetric Only intent family"

qa_run valid-v4 success "" "$BIN" "$QA_OUTDIR/valid-v4.icc" \
    0 2 0 "QA v4" 0 1 0 0 "$PROFILE" 1
qa_require_file "$QA_OUTDIR/valid-v4.icc"
qa_check_devicelink_intents
qa_run valid-cube success "" "$BIN" "$QA_OUTDIR/valid.cube" \
    1 2 4 "QA cube" 0 1 0 0 "$PROFILE" 1
qa_assert_contains "$QA_OUTDIR/valid.cube" "LUT_3D_SIZE 2" "Cube records grid size"
qa_run cube-identity-profile success "" "$FROM_CUBE" "$IDENTITY_CUBE" \
    "$QA_OUTDIR/cube-identity-profile.icc"
qa_run cube-identity-order success "" "$BIN" "$QA_OUTDIR/cube-identity-order.cube" \
    1 2 6 "QA Cube identity" 0 1 1 0 "$QA_OUTDIR/cube-identity-profile.icc" 0
qa_assert_cube_identity_order "$QA_OUTDIR/cube-identity-order.cube" \
    "Cube rows use Adobe red-fastest order"
qa_run cube-quoted-title success "" "$BIN" "$QA_OUTDIR/cube-quoted-title.cube" \
    1 2 6 'QA "quoted" title' 0 1 0 0 "$PROFILE" 1
qa_assert_contains "$QA_OUTDIR/cube-quoted-title.cube" \
    'TITLE "QA \x22quoted\x22 title"' "Cube title excludes raw delimiter quotes"
qa_run cube-quoted-title-import success "" "$FROM_CUBE" \
    "$QA_OUTDIR/cube-quoted-title.cube" "$QA_OUTDIR/cube-quoted-title.icc"
qa_run cube-range-p0 success "" "$BIN" "$QA_OUTDIR/cube-range-p0.cube" \
    1 2 0 "QA cube range p0" 0.1 0.2 0 0 "$PROFILE" 1
qa_assert_cube_range "$QA_OUTDIR/cube-range-p0.cube" 0.1 0.2 \
    "Cube precision zero preserves input range"
qa_run cube-range-p0-import success "" "$FROM_CUBE" \
    "$QA_OUTDIR/cube-range-p0.cube" "$QA_OUTDIR/cube-range-p0.icc"
qa_run cube-range-p0-dump success "" "$DUMP_PROFILE" -v \
    "$QA_OUTDIR/cube-range-p0.icc"
qa_assert_contains "$QA_LAST_LOG" "Profile is valid" \
    "Imported precision-zero Cube validates"
if grep -qF "invalid sample range" "$QA_LAST_LOG"; then
    qa_fail "Imported precision-zero Cube has an invalid sample range"
else
    echo "[PASS] Imported precision-zero Cube range is valid"
fi
qa_run cube-range-p20 success "" "$BIN" "$QA_OUTDIR/cube-range-p20.cube" \
    1 2 20 "QA cube range p20" 1e-30 2e-30 0 0 "$PROFILE" 1
qa_assert_cube_range "$QA_OUTDIR/cube-range-p20.cube" 1e-30 2e-30 \
    "Cube precision twenty preserves small input range"
qa_run cube-near-default success "" "$BIN" "$QA_OUTDIR/cube-near-default.cube" \
    1 2 20 "QA cube near default" 5e-9 1 0 0 "$PROFILE" 1
qa_assert_cube_range "$QA_OUTDIR/cube-near-default.cube" 5e-9 1 \
    "Cube records a non-default range below the old near threshold"
qa_run cube-near-default-import success "" "$FROM_CUBE" \
    "$NEAR_DEFAULT_CUBE" "$QA_OUTDIR/cube-near-default.icc"
qa_run cube-near-default-dump success "" "$DUMP_PROFILE" -v 100 \
    "$QA_OUTDIR/cube-near-default.icc" A2B0
qa_assert_contains "$QA_LAST_LOG" "Single Sampled Curve [0.00000000, 1.00000000]" \
    "iccFromCube preserves a range below the old near threshold"
qa_run cube-legacy-range-import success "" "$FROM_CUBE" \
    "$LEGACY_RANGE_CUBE" "$QA_OUTDIR/cube-legacy-range.icc"
qa_run cube-legacy-range-dump success "" "$DUMP_PROFILE" -v 100 \
    "$QA_OUTDIR/cube-legacy-range.icc" A2B0
qa_assert_contains "$QA_LAST_LOG" "Single Sampled Curve [0.25000000, 0.75000000]" \
    "iccFromCube retains legacy LUT_3D_INPUT_RANGE compatibility"
qa_run cube-input-video-range reject "video-range scaling is not implemented" \
    "$FROM_CUBE" "$INPUT_VIDEO_RANGE_CUBE" "$QA_OUTDIR/cube-input-video-range.icc"
qa_run cube-output-video-range reject "video-range scaling is not implemented" \
    "$FROM_CUBE" "$OUTPUT_VIDEO_RANGE_CUBE" "$QA_OUTDIR/cube-output-video-range.icc"
qa_run link-type-2 reject "expected 0 (Device Link) or 1 (.cube text file)" \
    "$BIN" "$QA_OUTDIR/bad-type.icc" 2 2 0 "QA bad type" 0 1 0 0 "$PROFILE" 1
qa_run link-type-text reject "Invalid link_type" \
    "$BIN" "$QA_OUTDIR/type-text.icc" text 2 0 "QA type text" 0 1 0 0 "$PROFILE" 1
qa_run link-type-junk reject "Invalid link_type" \
    "$BIN" "$QA_OUTDIR/type-junk.icc" 0x 2 0 "QA type junk" 0 1 0 0 "$PROFILE" 1
qa_run lut-below-min reject "expected an integer between 2 and 255" \
    "$BIN" "$QA_OUTDIR/lut-low.icc" 0 1 0 "QA low" 0 1 0 0 "$PROFILE" 1
qa_run lut-above-max reject "expected an integer between 2 and 255" \
    "$BIN" "$QA_OUTDIR/lut-high.icc" 0 256 0 "QA high" 0 1 0 0 "$PROFILE" 1
qa_run lut-junk reject "expected an integer between 2 and 255" \
    "$BIN" "$QA_OUTDIR/lut-junk.icc" 0 2x 0 "QA LUT junk" 0 1 0 0 "$PROFILE" 1
qa_run device-option-high reject "DeviceLink option must be 0 (v4) or 1 (v5)" \
    "$BIN" "$QA_OUTDIR/device-option-high.icc" 0 2 2 "QA option" 0 1 0 0 "$PROFILE" 1
qa_run cube-option-low reject ".cube precision must be between 0 and 20" \
    "$BIN" "$QA_OUTDIR/cube-option-low.cube" 1 2 -1 "QA option" 0 1 0 0 "$PROFILE" 1
qa_run cube-option-high reject ".cube precision must be between 0 and 20" \
    "$BIN" "$QA_OUTDIR/cube-option-high.cube" 1 2 21 "QA option" 0 1 0 0 "$PROFILE" 1
qa_run cube-option-junk reject ".cube precision must be between 0 and 20" \
    "$BIN" "$QA_OUTDIR/cube-option-junk.cube" 1 2 6x "QA option" 0 1 0 0 "$PROFILE" 1
qa_run cube-option-overflow reject ".cube precision must be between 0 and 20" \
    "$BIN" "$QA_OUTDIR/cube-option-overflow.cube" 1 2 2147483648 "QA option" 0 1 0 0 "$PROFILE" 1
qa_run zero-width-range reject "must be greater than" \
    "$BIN" "$QA_OUTDIR/zero.icc" 0 2 0 "QA zero" 1 1 0 0 "$PROFILE" 1
qa_run reversed-range reject "must be greater than" \
    "$BIN" "$QA_OUTDIR/reversed.icc" 0 2 1 "QA reversed" 1 0 0 0 "$PROFILE" 1
qa_run nan-range reject "must be finite numbers" \
    "$BIN" "$QA_OUTDIR/nan-range.icc" 0 2 1 "QA NaN" nan 1 0 0 "$PROFILE" 1
qa_run infinite-range reject "must be finite numbers" \
    "$BIN" "$QA_OUTDIR/infinite-range.icc" 0 2 1 "QA Inf" 0 inf 0 0 "$PROFILE" 1
qa_run restricted-v4 reject "V4 device links cannot record" \
    "$BIN" "$QA_OUTDIR/restricted-v4.icc" 0 2 0 "QA restricted" 0.1 0.9 0 0 "$PROFILE" 1
qa_run near-default-v4 reject "V4 device links cannot record" \
    "$BIN" "$QA_OUTDIR/near-default-v4.icc" 0 2 0 "QA near restricted" 5e-9 1 0 0 "$PROFILE" 1
qa_run restricted-v5 success "" "$BIN" "$QA_OUTDIR/restricted-v5.icc" \
    0 2 1 "QA restricted v5" 0.1 0.9 0 0 "$PROFILE" 1
qa_run v5-device-link success "" "$BIN" "$QA_OUTDIR/v5-device-link.icc" \
    0 9 1 "AFL v5 DeviceLink" -0.25 1.25 1 0 "$PROFILE" 1 "$PROFILE" 1
qa_run mixed-device-intents reject "require one base rendering intent" \
    "$BIN" "$QA_OUTDIR/mixed-device-intents.icc" \
    0 2 1 "QA mixed DeviceLink" 0 1 1 0 "$PROFILE" 0 "$PROFILE" 1
qa_run mixed-cube-intents success "" \
    "$BIN" "$QA_OUTDIR/mixed-cube-intents.cube" \
    1 2 6 "QA mixed Cube" 0 1 1 0 "$PROFILE" 0 "$PROFILE" 1
qa_run cube-intent-13 success "" "$BIN" "$QA_OUTDIR/cube-intent-13.cube" \
    1 2 4 "AFL" 0 1 0 0 "$PROFILE" 13
qa_run first-transform-high reject "Invalid first_transform" \
    "$BIN" "$QA_OUTDIR/first-transform-high.icc" 0 2 1 "QA first" 0 1 2 0 "$PROFILE" 1
qa_run interp-high reject "Invalid interp" \
    "$BIN" "$QA_OUTDIR/interp-high.icc" 0 2 1 "QA interp" 0 1 0 2 "$PROFILE" 1
qa_run intent-text reject "expected an integer code" \
    "$BIN" "$QA_OUTDIR/intent-text.icc" 0 2 1 "QA intent" 0 1 0 0 "$PROFILE" text
qa_run intent-junk reject "expected an integer code" \
    "$BIN" "$QA_OUTDIR/intent-junk.icc" 0 2 1 "QA intent" 0 1 0 0 "$PROFILE" 1x
qa_run intent-overflow reject "expected an integer code" \
    "$BIN" "$QA_OUTDIR/intent-overflow.icc" 0 2 1 "QA intent" 0 1 0 0 "$PROFILE" 2147483648
qa_run intent-negative reject "a negative intent code is not a valid form" \
    "$BIN" "$QA_OUTDIR/intent-negative.icc" 0 2 1 "QA intent" 0 1 0 0 "$PROFILE" -110
qa_run_stderr_reject first-transform-stderr "Invalid first_transform" \
    "$BIN" "$QA_OUTDIR/first-transform-stderr.icc" \
    0 2 1 "QA first stderr" 0 1 2 0 "$PROFILE" 1
qa_run_stderr_reject intent-stderr "Invalid rendering intent" \
    "$BIN" "$QA_OUTDIR/intent-stderr.icc" \
    0 2 1 "QA intent stderr" 0 1 0 0 "$PROFILE" 9
qa_run_stderr_reject env-stderr "Invalid environment value" \
    "$BIN" "$QA_OUTDIR/env-stderr.cube" \
    1 2 6 "QA ENV stderr" 0 1 0 0 -ENV:bkgX nan "$PROFILE" 1
qa_run intent-int-max-class success "" \
    "$BIN" "$QA_OUTDIR/intent-int-max-class.icc" 0 2 1 "QA intent max" \
    0 1 0 0 "$PROFILE" 2147483640
qa_check_intent_matrix
qa_run intent-alias-luminance-100 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-luminance-100.cube" \
    1 2 6 "QA intent alias luminance" 0 1 0 0 "$PROFILE" 100
qa_run intent-alias-luminance-200 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-luminance-200.cube" \
    1 2 6 "QA intent alias luminance" 0 1 0 0 "$PROFILE" 200
qa_run intent-alias-luminance-900 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-luminance-900.cube" \
    1 2 6 "QA intent alias luminance" 0 1 0 0 "$PROFILE" 900
qa_assert_same_file "$QA_OUTDIR/intent-alias-luminance-100.cube" \
    "$QA_OUTDIR/intent-alias-luminance-200.cube" \
    "Rendering-intent codes 100 and 200 retain identical flag semantics"
qa_assert_same_file "$QA_OUTDIR/intent-alias-luminance-100.cube" \
    "$QA_OUTDIR/intent-alias-luminance-900.cube" \
    "Rendering-intent codes 100 and 900 retain identical flag semantics"
qa_run intent-alias-v5-1000 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-v5-1000.cube" \
    1 2 6 "QA intent alias v5" 0 1 0 0 "$PROFILE" 1000
qa_run intent-alias-v5-10000 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-v5-10000.cube" \
    1 2 6 "QA intent alias v5" 0 1 0 0 "$PROFILE" 10000
qa_run intent-alias-v5-100000 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-v5-100000.cube" \
    1 2 6 "QA intent alias v5" 0 1 0 0 "$PROFILE" 100000
qa_assert_same_file "$QA_OUTDIR/intent-alias-v5-1000.cube" \
    "$QA_OUTDIR/intent-alias-v5-10000.cube" \
    "Rendering-intent codes 1000 and 10000 retain identical flag semantics"
qa_assert_same_file "$QA_OUTDIR/intent-alias-v5-1000.cube" \
    "$QA_OUTDIR/intent-alias-v5-100000.cube" \
    "Rendering-intent codes 1000 and 100000 retain identical flag semantics"
qa_run intent-alias-combined-1101 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-combined-1101.cube" \
    1 2 6 "QA intent alias combined" 0 1 0 0 "$PROFILE" 1101
qa_run intent-alias-combined-11101 success "" "$BIN" \
    "$QA_OUTDIR/intent-alias-combined-11101.cube" \
    1 2 6 "QA intent alias combined" 0 1 0 0 "$PROFILE" 11101
qa_assert_same_file "$QA_OUTDIR/intent-alias-combined-1101.cube" \
    "$QA_OUTDIR/intent-alias-combined-11101.cube" \
    "Combined rendering-intent aliases retain identical flag semantics"
qa_run valid-env success "" "$BIN" "$QA_OUTDIR/valid-env.cube" \
    1 2 4 "QA ENV" 0 1 0 0 -ENV:bkgX 0.0985 "$PROFILE" 1
qa_run env-profile-from-xml success "" "$FROM_XML" \
    "$ENV_PROFILE_XML" "$QA_OUTDIR/env-profile.icc"
qa_run env-runtime-zero success "" "$BIN" "$QA_OUTDIR/env-runtime-zero.cube" \
    1 3 9 "QA ENV runtime" 0 1 1 0 \
    -ENV:ambL 0 "$QA_OUTDIR/env-profile.icc" 0
qa_run env-runtime-hundred success "" "$BIN" \
    "$QA_OUTDIR/env-runtime-hundred.cube" \
    1 3 9 "QA ENV runtime" 0 1 1 0 \
    -ENV:ambL 100 "$QA_OUTDIR/env-profile.icc" 0
qa_assert_different_file "$QA_OUTDIR/env-runtime-zero.cube" \
    "$QA_OUTDIR/env-runtime-hundred.cube" \
    "CMM environment values affect calculator output"
qa_run valid-pcc success "" "$BIN" "$QA_OUTDIR/valid-pcc.cube" \
    1 2 4 "QA PCC" 0 1 0 0 "$PROFILE" 1 -PCC "$PCC_PROFILE"
qa_run no-profile-stage reject "expected at least one profile" \
    "$BIN" "$QA_OUTDIR/no-profile.cube" 1 2 4 "QA no profile" 0 1 0 0
qa_run trailing-env reject "Environment options must be followed" \
    "$BIN" "$QA_OUTDIR/trailing-env.cube" 1 2 4 "QA trailing ENV" \
    0 1 0 0 "$PROFILE" 1 -ENV:bkgX 0.1
qa_run leading-pcc reject "must immediately follow a profile" \
    "$BIN" "$QA_OUTDIR/leading-pcc.cube" 1 2 4 "QA leading PCC" \
    0 1 0 0 -PCC "$QA_OUTDIR/missing-pcc.icc" "$PROFILE" 1
qa_run duplicate-pcc reject "must immediately follow a profile" \
    "$BIN" "$QA_OUTDIR/duplicate-pcc.cube" 1 2 4 "QA duplicate PCC" \
    0 1 0 0 "$PROFILE" 1 -PCC "$PCC_PROFILE" \
    -PCC "$QA_OUTDIR/missing-pcc.icc"
qa_run empty-env-signature reject "expected exactly four characters" \
    "$BIN" "$QA_OUTDIR/empty-env.cube" 1 2 4 "QA empty ENV" \
    0 1 0 0 -ENV: 0.1 "$PROFILE" 1
qa_run short-env-signature reject "expected exactly four characters" \
    "$BIN" "$QA_OUTDIR/short-env.cube" 1 2 4 "QA short ENV" \
    0 1 0 0 -ENV:abc 0.1 "$PROFILE" 1
qa_run long-env-signature reject "expected exactly four characters" \
    "$BIN" "$QA_OUTDIR/long-env.cube" 1 2 4 "QA long ENV" \
    0 1 0 0 -ENV:abcde 0.1 "$PROFILE" 1
qa_run telemetry-jsonl success "" "$BIN" "$QA_OUTDIR/telemetry.cube" \
    1 2 4 "QA telemetry" 0 1 0 0 "$PROFILE" 1 \
    --telemetry=jsonl --telemetry-file "$TELEMETRY" --evidence-file "$EVIDENCE"
qa_require_file "$TELEMETRY"
qa_require_file "$EVIDENCE"
qa_assert_contains "$TELEMETRY" "\"event\":\"run_started\"" "Link telemetry starts"
qa_assert_contains "$TELEMETRY" "\"event\":\"transform_ready\"" "Link telemetry is ready"
qa_assert_contains "$TELEMETRY" "\"event\":\"run_completed\"" "Link telemetry completes"
qa_assert_contains "$EVIDENCE" "\"grid_nodes\":" "Link evidence records grid nodes"
qa_run telemetry-human success "iccApplyToLink: completed" "$BIN" \
    -telemetry=human "$QA_OUTDIR/human.cube" 1 2 4 "QA human" 0 1 0 0 "$PROFILE" 1
qa_assert_contains "$QA_LAST_LOG" "iccApplyToLink: progress nodes=" \
    "ApplyToLink human telemetry reports LUT progress"
if ! grep -Eq '^\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\] iccApplyToLink: completed ' "$QA_LAST_LOG"; then
    qa_fail "ApplyToLink human telemetry completion is missing its UTC timestamp"
fi
if grep -Eq '[0-9]+%' "$QA_LAST_LOG"; then
    qa_fail "ApplyToLink human telemetry duplicated the legacy percentage"
fi
if [[ "$(tail -c 1 "$QA_LAST_LOG" | od -An -tu1 | tr -d '[:space:]')" != "10" ]]; then
    qa_fail "ApplyToLink human telemetry output does not end with a newline"
fi

qa_finish
