#!/usr/bin/env bash
###############################################################################
# High-output 3D CLUT regression test
###############################################################################
#
# Converts the checked-in ICC v5 fixtures, validates them, and applies their 3D
# CLUTs. The eight-output case covers a full vector, the nine-output case
# covers fallback behavior, and the eleven-output case covers the AVX2 masked
# tail and AVX-512 masking. Other builds retain the
# same expected scalar/SSE results. Each grid point is distinct so the expected
# vectors also check corner offsets and weights.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-clut-eight-output}"

if [ ! -d "$TOOLS_DIR" ]; then
    for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
        if [ -d "$candidate" ]; then
            TOOLS_DIR="$candidate"
            break
        fi
    done
fi

BUILD_ROOT=""
if [ -d "$TOOLS_DIR/.." ]; then
    BUILD_ROOT="$(cd "$TOOLS_DIR/.." && pwd -P)"
fi
if [ -n "$BUILD_ROOT" ]; then
    export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

FROM_XML="$TOOLS_DIR/IccFromXml/iccFromXml"
DUMP_PROFILE="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
APPLY_NAMED_CMM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
FIXTURE_INPUT="$TESTING_DIR/CLUT/avx2-3d-8-output.txt"

fail() {
    echo "[FAIL] clut-eight-output-regression: $1" >&2
    exit 1
}

require_file() {
    [ -f "$1" ] || fail "missing fixture: $1"
}

require_tool() {
    [ -x "$1" ] || fail "missing executable: $1"
}

check_sanitizers() {
    local logfile="$1"

    if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile"; then
        sed -n '1,120p' "$logfile"
        fail "sanitizer diagnostic in $(basename "$logfile")"
    fi
}

mkdir -p "$OUTDIR"
require_tool "$FROM_XML"
require_tool "$DUMP_PROFILE"
require_tool "$APPLY_NAMED_CMM"
require_file "$FIXTURE_INPUT"

run_case() {
    local name="$1"
    local expected="$2"
    local fixture_xml="$TESTING_DIR/CLUT/$name.xml"
    local fixture_icc="$OUTDIR/$name.icc"
    local from_xml_log="$OUTDIR/$name-from-xml.log"
    local dump_log="$OUTDIR/$name-dump.log"
    local apply_log="$OUTDIR/$name-apply.log"

    require_file "$fixture_xml"
    "$FROM_XML" "$fixture_xml" "$fixture_icc" > "$from_xml_log" 2>&1 ||
        fail "could not convert $name fixture to ICC"
    "$DUMP_PROFILE" -v 100 "$fixture_icc" > "$dump_log" 2>&1 ||
        fail "$name fixture validation command failed"
    "$APPLY_NAMED_CMM" "$FIXTURE_INPUT" 3 0 "$fixture_icc" 0 > "$apply_log" 2>&1 ||
        fail "could not apply $name fixture"

    check_sanitizers "$from_xml_log"
    check_sanitizers "$dump_log"
    check_sanitizers "$apply_log"

    grep -q "Profile is valid for version 5.10" "$dump_log" ||
        fail "converted $name fixture is not ICC-valid"
    grep -qE "$expected" "$apply_log" ||
        fail "$name CLUT output differs from the expected vector"
}

run_case \
    "avx2-3d-8-output" \
    "0\\.2200 +0\\.2300 +0\\.2400 +0\\.2500 +0\\.2600 +0\\.2700 +0\\.2800 +0\\.2900"
run_case \
    "avx2-3d-9-output" \
    "0\\.2200 +0\\.2300 +0\\.2400 +0\\.2500 +0\\.2600 +0\\.2700 +0\\.2800 +0\\.2900 +0\\.3000"
run_case \
    "avx2-3d-11-output" \
    "0\\.2200 +0\\.2300 +0\\.2400 +0\\.2500 +0\\.2600 +0\\.2700 +0\\.2800 +0\\.2900 +0\\.3000 +0\\.3100 +0\\.3200"

echo "[PASS] clut-eight-output-regression"
