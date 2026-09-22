#!/bin/bash
# Focused ICC.2 XML header, profile validation, and reporting QA.
#
# This suite covers the spectral PCS, bi-spectral range, and MCS header
# branches with canonical upstream XML fixtures. It also verifies XML to ICC
# to XML fidelity, negative parser/validation cases, iccRoundTrip capability,
# and iccPawgReport report generation.

#
# Copyright (c) International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. In the absence of prior written permission, the names "ICC" and "The
#    International Color Consortium" must not be used to imply that the ICC
#    organization endorses or promotes products derived from this software.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# This software consists of voluntary contributions made by many individuals on
# behalf of the International Color Consortium. Membership in the ICC is
# encouraged when this software is used for commercial purposes. For more
# information, see <http://www.color.org/>.
#
# Environment:
#   ICCDEV_TOOLS_DIR   path to Build/Tools
#   ICCDEV_TESTING_DIR path to Testing
#   ICCDEV_TEST_OUTDIR output directory for temporary profiles and logs
#   ICCDEV_XML_SPEC_FIXTURES optional fixture directory override
#   ICCDEV_PYTHON       Python 3 interpreter used for PAWG JSON validation

set -uo pipefail

TOOLS="${ICCDEV_TOOLS_DIR:-Build/Tools}"
ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-xml-spec-qa}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FIXTURE_DIR="${ICCDEV_XML_SPEC_FIXTURES:-$REPO_ROOT/.github/ci/test-data/xml-spec-qa}"
PYTHON="${ICCDEV_PYTHON:-python3}"
mkdir -p "$OUTDIR"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
CRASH=0
TOTAL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0

run_test() {
  local id="$1" description="$2"
  shift 2
  local log="$OUTDIR/$id.log" rc=0

  timeout 60 "$@" > "$log" 2>&1 || rc=$?
  if grep -q "ERROR: AddressSanitizer" "$log" 2>/dev/null; then
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %-55s exit=%d ASAN\n" "$description" "$rc"
  elif grep -q "runtime error:" "$log" 2>/dev/null; then
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %-55s exit=%d UBSAN\n" "$description" "$rc"
  elif is_timeout_or_signal_status "$rc"; then
    record_abnormal_exit "$description" "$rc"
  elif [ "$rc" -ne 0 ]; then
    FAIL=$((FAIL + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [FAIL   ] %-55s exit=%d\n" "$description" "$rc"
  else
    PASS=$((PASS + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [PASS   ] %-55s exit=0\n" "$description"
  fi
}

print_summary() {
  echo
  echo "--- $1 Summary ---"
  echo "  PASS=$PASS  FAIL=$FAIL  CRASH=$CRASH  TOTAL=$TOTAL"
  echo "  ASAN=$ASAN_FINDINGS  UBSAN=$UBSAN_FINDINGS"
}

FROMXML="$TOOLS/IccFromXml/iccFromXml"
TOXML="$TOOLS/IccToXml/iccToXml"
DUMP="$TOOLS/IccDumpProfile/iccDumpProfile"
ROUNDTRIP="$TOOLS/IccRoundTrip/iccRoundTrip"
PAWG="$TOOLS/IccPawgReport/iccPawgReport"

SPECTRAL_XML="$FIXTURE_DIR/spectral-reflectance-5.xml"
BISPECTRAL_XML="$FIXTURE_DIR/bispectral-reflectance-2x3.xml"
MCS_XML="$FIXTURE_DIR/mcs-3-channel-mid.xml"
SPECTRAL_STEPS_OVERFLOW_XML="$FIXTURE_DIR/spectral-steps-overflow.xml"
BISPECTRAL_STEPS_OVERFLOW_XML="$FIXTURE_DIR/bispectral-steps-overflow.xml"
SPECTRAL_NONFINITE_START_XML="$FIXTURE_DIR/spectral-nonfinite-start.xml"
BISPECTRAL_NONFINITE_END_XML="$FIXTURE_DIR/bispectral-nonfinite-end.xml"
BISPECTRAL_CHANNEL_MISMATCH_XML="$FIXTURE_DIR/bispectral-channel-mismatch.xml"
NONBISPECTRAL_WITH_BI_RANGE_XML="$FIXTURE_DIR/nonbispectral-with-bi-range.xml"
MCS_CHANNEL_MISMATCH_XML="$FIXTURE_DIR/mcs-channel-mismatch.xml"
ROUNDTRIP_DIR="$ICCDEV_TESTING/Display"
ROUNDTRIP_XML="$ROUNDTRIP_DIR/sRGB_D65_MAT.xml"
ROUNDTRIP_CONTROL="$OUTDIR/sRGB_D65_MAT.icc"

echo "=== ICC.2 XML spectral, bi-spectral, and MCS QA ==="

record_pass() {
  local description="$1"
  TOTAL=$((TOTAL + 1))
  PASS=$((PASS + 1))
  printf "  [PASS   ] %s\n" "$description"
}

record_fail() {
  local description="$1"
  TOTAL=$((TOTAL + 1))
  FAIL=$((FAIL + 1))
  printf "  [FAIL   ] %s\n" "$description"
}

assert_contains() {
  local description="$1" file="$2" pattern="$3"
  if grep -Fq "$pattern" "$file" 2>/dev/null; then
    record_pass "$description"
  else
    record_fail "$description -- missing: $pattern"
  fi
}

record_abnormal_exit() {
  local description="$1" rc="$2"
  if [ "$rc" -eq 124 ]; then
    record_fail "$description -- timed out"
  else
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s -- signal-band exit=%d\n" "$description" "$rc"
  fi
}

is_timeout_or_signal_status() {
  local rc="$1"
  [ "$rc" -eq 124 ] || { [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; }
}

expect_reject() {
  local id="$1" description="$2" xml="$3" expected="$4"
  local icc="$OUTDIR/$id.icc" log="$OUTDIR/$id.log" rc=0

  timeout 60 "$FROMXML" "$xml" "$icc" > "$log" 2>&1 || rc=$?
  if grep -q "ERROR: AddressSanitizer" "$log" 2>/dev/null; then
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s -- AddressSanitizer\n" "$description"
  elif grep -q "runtime error:" "$log" 2>/dev/null; then
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s -- UndefinedBehaviorSanitizer\n" "$description"
  elif is_timeout_or_signal_status "$rc"; then
    record_abnormal_exit "$description" "$rc"
  elif [ "$rc" -eq 0 ]; then
    record_fail "$description -- malformed input was accepted"
  elif ! grep -Fq "$expected" "$log"; then
    record_fail "$description -- expected diagnostic not found: $expected"
  else
    record_pass "$description"
  fi
}

expect_roundtrip_unsupported() {
  local name="$1" profile="$2"
  local log="$OUTDIR/$name.roundtrip-unsupported.log" rc=0

  timeout 60 "$ROUNDTRIP" "$profile" 1 1 > "$log" 2>&1 || rc=$?
  if grep -q "ERROR: AddressSanitizer" "$log"; then
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s round-trip capability probe -- ASAN\n" "$name"
  elif grep -q "runtime error:" "$log"; then
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s round-trip capability probe -- UBSAN\n" "$name"
  elif is_timeout_or_signal_status "$rc"; then
    record_abnormal_exit "$name round-trip capability probe" "$rc"
  elif [ "$rc" -ne 0 ] && grep -Fq "Unsupported profile class" "$log"; then
    record_pass "$name round-trip capability limit is explicit"
  else
    record_fail "$name round-trip capability limit changed or was ambiguous"
  fi
}

validate_pawg_json() {
  "$PYTHON" - "$1" <<'PYEOF'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as stream:
    report = json.load(stream)

if not isinstance(report, dict):
    raise ValueError("top-level PAWG report is not an object")
if report.get("tool") != "iccPawgReport":
    raise ValueError("unexpected or missing tool field")
if report.get("load") != "parsed by IccProfLib":
    raise ValueError("profile load evidence is missing")

summary = report.get("summary")
if not isinstance(summary, dict):
    raise ValueError("summary is missing or is not an object")
for field in ("total", "pass", "warn", "fail", "notApplicable", "gap", "notRun"):
    if not isinstance(summary.get(field), int):
        raise ValueError("summary.%s is missing or is not an integer" % field)

items = report.get("items")
if not isinstance(items, list):
    raise ValueError("items is missing or is not an array")
if summary["total"] != len(items):
    raise ValueError("summary.total does not match the item count")
PYEOF
}

run_pawg_evidence() {
  local name="$1" profile="$2"
  local json="$OUTDIR/$name.pawg.json" log="$OUTDIR/$name.pawg.log" rc=0
  local json_error=""

  timeout 60 "$PAWG" --json "$profile" > "$json" 2> "$log" || rc=$?
  if grep -q "ERROR: AddressSanitizer" "$log"; then
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s PAWG report generation -- ASAN\n" "$name"
  elif grep -q "runtime error:" "$log"; then
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    CRASH=$((CRASH + 1))
    TOTAL=$((TOTAL + 1))
    printf "  [CRASH  ] %s PAWG report generation -- UBSAN\n" "$name"
  elif is_timeout_or_signal_status "$rc"; then
    record_abnormal_exit "$name PAWG report generation" "$rc"
  elif [ "$rc" -gt 1 ]; then
    record_fail "$name PAWG report generation -- exit=$rc"
  elif ! json_error="$(validate_pawg_json "$json" 2>&1)"; then
    record_fail "$name PAWG report generation -- invalid JSON: $json_error"
  else
    record_pass "$name PAWG JSON evidence generated (assessment exit=$rc)"
  fi
}

for required in "$FROMXML" "$TOXML" "$DUMP" "$ROUNDTRIP" "$PAWG"; do
  if [ ! -x "$required" ]; then
    echo "[FAIL] required tool is not executable: $required"
    exit 1
  fi
done

if ! command -v "$PYTHON" >/dev/null 2>&1; then
  echo "[SKIP] Python 3 is required to validate PAWG JSON: $PYTHON"
  exit 77
fi

for required in "$SPECTRAL_XML" "$BISPECTRAL_XML" "$MCS_XML" \
                "$SPECTRAL_STEPS_OVERFLOW_XML" \
                "$BISPECTRAL_STEPS_OVERFLOW_XML" \
                "$SPECTRAL_NONFINITE_START_XML" \
                "$BISPECTRAL_NONFINITE_END_XML" \
                "$BISPECTRAL_CHANNEL_MISMATCH_XML" \
                "$NONBISPECTRAL_WITH_BI_RANGE_XML" \
                "$MCS_CHANNEL_MISMATCH_XML" "$ROUNDTRIP_XML"; do
  if [ ! -f "$required" ]; then
    echo "[FAIL] required fixture is missing: $required"
    exit 1
  fi
done

run_test "xmlspec-spectral-fromxml" "Build five-sample reflectance spectral profile" \
  "$FROMXML" "$SPECTRAL_XML" "$OUTDIR/spectral.icc"
run_test "xmlspec-bispectral-fromxml" "Build 2x3 bi-spectral profile" \
  "$FROMXML" "$BISPECTRAL_XML" "$OUTDIR/bispectral.icc"
run_test "xmlspec-mcs-fromxml" "Build three-channel MCS profile" \
  "$FROMXML" "$MCS_XML" "$OUTDIR/mcs.icc"
run_test "xmlspec-control-fromxml" "Build ICC.1 display control profile" \
  "$FROMXML" "$ROUNDTRIP_XML" "$ROUNDTRIP_CONTROL"

for name in spectral bispectral mcs; do
  run_test "xmlspec-$name-dump" "Validate and dump $name profile" \
    "$DUMP" -v "$OUTDIR/$name.icc"
  run_test "xmlspec-$name-toxml" "Serialize $name profile back to XML" \
    "$TOXML" "$OUTDIR/$name.icc" "$OUTDIR/$name.roundtrip.xml"
done

for name in spectral bispectral mcs; do
  assert_contains "$name profile validates as ICCmax" \
    "$OUTDIR/xmlspec-$name-dump.log" \
    "Profile is valid for version 5.00"
done

assert_contains "spectral signature has five channels" \
  "$OUTDIR/xmlspec-spectral-dump.log" \
  "Spectral PCS:       0x0005ChannelReflectanceData"
assert_contains "spectral range is 410-690 nm in five steps" \
  "$OUTDIR/xmlspec-spectral-dump.log" \
  "Spectral PCS Range: start=410.0nm, end=690.0nm, steps=5"
assert_contains "normal spectral PCS has no bi-spectral range" \
  "$OUTDIR/xmlspec-spectral-dump.log" \
  "BiSpectral Range:   Not Defined"

assert_contains "bi-spectral signature has six channels" \
  "$OUTDIR/xmlspec-bispectral-dump.log" \
  "Spectral PCS:       0x0006ChannelBiSpectralReflectanceData"
assert_contains "bi-spectral reflected range is two samples" \
  "$OUTDIR/xmlspec-bispectral-dump.log" \
  "Spectral PCS Range: start=420.0nm, end=680.0nm, steps=2"
assert_contains "bi-spectral incident range is three samples" \
  "$OUTDIR/xmlspec-bispectral-dump.log" \
  "start=300.0nm, end=500.0nm, steps=3"

assert_contains "MCS signature has three channels" \
  "$OUTDIR/xmlspec-mcs-dump.log" \
  "MCS Color Space:    0x0003ChannelMCSData"
assert_contains "MCS profile includes multiplex type array" \
  "$OUTDIR/xmlspec-mcs-dump.log" \
  "multiplexTypeArrayTag"

assert_contains "spectral signature survives XML round trip" \
  "$OUTDIR/spectral.roundtrip.xml" "<SpectralPCS>rs0005</SpectralPCS>"
assert_contains "bi-spectral signature survives XML round trip" \
  "$OUTDIR/bispectral.roundtrip.xml" "<SpectralPCS>bs0006</SpectralPCS>"
assert_contains "MCS signature survives XML round trip" \
  "$OUTDIR/mcs.roundtrip.xml" "<MCS>mc0003</MCS>"

run_pawg_evidence "spectral" "$OUTDIR/spectral.icc"
run_pawg_evidence "bispectral" "$OUTDIR/bispectral.icc"
run_pawg_evidence "mcs" "$OUTDIR/mcs.icc"
run_pawg_evidence "roundtrip-control" "$ROUNDTRIP_CONTROL"
assert_contains "ICC.1 PAWG control has no failing assessments" \
  "$OUTDIR/roundtrip-control.pawg.json" '"fail": 0'

for intent in 0 1 2 3; do
  run_test "xmlspec-roundtrip-intent-$intent" \
    "Run ICC.1 display round trip for intent $intent" \
    "$ROUNDTRIP" "$ROUNDTRIP_CONTROL" "$intent" 0
  assert_contains "round-trip intent $intent processes the PRMG sample set" \
    "$OUTDIR/xmlspec-roundtrip-intent-$intent.log" "Total     (  201613)"
done

expect_roundtrip_unsupported "spectral" "$OUTDIR/spectral.icc"
expect_roundtrip_unsupported "bispectral" "$OUTDIR/bispectral.icc"
expect_roundtrip_unsupported "mcs" "$OUTDIR/mcs.icc"

expect_reject "spectral-steps-overflow" \
  "Reject spectral step count outside uInt16 range" \
  "$SPECTRAL_STEPS_OVERFLOW_XML" \
  "Invalid SpectralRange Wavelengths steps"
expect_reject "bispectral-steps-overflow" \
  "Reject bi-spectral step count outside uInt16 range" \
  "$BISPECTRAL_STEPS_OVERFLOW_XML" \
  "Invalid BiSpectralRange Wavelengths steps"
expect_reject "spectral-nonfinite-start" \
  "Reject non-finite spectral start wavelength" \
  "$SPECTRAL_NONFINITE_START_XML" \
  "Invalid SpectralRange Wavelengths start or end"
expect_reject "bispectral-nonfinite-end" \
  "Reject non-finite bi-spectral end wavelength" \
  "$BISPECTRAL_NONFINITE_END_XML" \
  "Invalid BiSpectralRange Wavelengths start or end"
expect_reject "bispectral-channel-mismatch" \
  "Reject bi-spectral signature/range channel mismatch" \
  "$BISPECTRAL_CHANNEL_MISMATCH_XML" \
  "Number of channels defined for spectral PCS do not match spectral range definitions"
expect_reject "nonbispectral-with-bi-range" \
  "Reject bi-spectral range on normal spectral PCS" \
  "$NONBISPECTRAL_WITH_BI_RANGE_XML" \
  "Spectral PCS wavelengths defined with no spectral PCS"
expect_reject "mcs-channel-mismatch" \
  "Reject MCS signature/multiplex array channel mismatch" \
  "$MCS_CHANNEL_MISMATCH_XML" \
  "Number of multiplex channel names does not match MCS in header"

print_summary "ICC.2 XML specification QA"
exit $((FAIL + CRASH > 0 ? 1 : 0))
