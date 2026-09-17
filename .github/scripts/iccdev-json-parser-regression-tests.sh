#!/bin/bash
###############################################################################
# iccDEV JSON parser/config regression tests
###############################################################################
#
# Exercises malformed ICC JSON and JSON config helpers that must fail cleanly
# and sibling XML payloads that must fail cleanly instead of being silently
# accepted or retaining stale state.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-json-parser-regressions}"
mkdir -p "$OUTDIR"

# Resolve both to absolute paths before deriving any tool path from them. CTest
# passes these already absolute, but the defaults above are relative to the repo
# root, and a test that has to run a tool from another working directory (the
# issue #1856 fixture below builds its profile from Testing/hybrid, whose XML
# references sibling data files) would otherwise fail to find the binary.
if [ -d "$TOOLS_DIR" ]; then
  TOOLS_DIR="$(cd "$TOOLS_DIR" && pwd)"
fi
if [ -d "$TESTING_DIR" ]; then
  TESTING_DIR="$(cd "$TESTING_DIR" && pwd)"
fi

TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd)"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
SKIPPED=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=0

# Bound the JSON readers were built with, so a size-threshold test can tell a
# real regression from a build that simply configured a different limit.
JSON_MAX_FILE_MB="${ICC_JSON_MAX_FILE_MB:-128}"

check_sanitizers() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    echo "  [ASAN] $name -- AddressSanitizer finding"
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    echo "  [UBSAN] $name -- undefined behavior"
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    return 1
  fi

  return 0
}

run_reject_test() {
  local name="$1"
  local json_file="$2"
  local expected_text="$3"
  local output_file="$OUTDIR/${name}.icc"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  timeout 30 "$FROMJSON" "$json_file" "$output_file" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- timed out"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- crashed with signal $((exit_code - 128))"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 0 ]; then
    echo "  [FAIL] $name -- malformed JSON parsed successfully"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ -s "$output_file" ]; then
    echo "  [FAIL] $name -- parser wrote output profile despite failure"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! grep -Fq "$expected_text" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- expected diagnostic not found: $expected_text"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (rejected, exit=$exit_code)"
  PASS=$((PASS + 1))
}

run_xml_reject_test() {
  local name="$1"
  local xml_file="$2"
  local expected_text="$3"
  local output_file="$OUTDIR/${name}.icc"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  timeout 30 "$FROMXML" "$xml_file" "$output_file" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- timed out"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- crashed with signal $((exit_code - 128))"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 0 ]; then
    echo "  [FAIL] $name -- malformed XML parsed successfully"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ -s "$output_file" ]; then
    echo "  [FAIL] $name -- parser wrote output profile despite failure"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! grep -Fq "$expected_text" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- expected diagnostic not found: $expected_text"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (rejected, exit=$exit_code)"
  PASS=$((PASS + 1))
}

run_fromjson_success_test() {
  local name="$1"
  local json_file="$2"
  local output_file="$OUTDIR/${name}.icc"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  timeout 30 "$FROMJSON" "$json_file" "$output_file" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- timed out"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- crashed with signal $((exit_code - 128))"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ] || [ ! -s "$output_file" ]; then
    echo "  [FAIL] $name -- iccFromJson failed with exit=$exit_code"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (iccFromJson completed without sanitizer findings)"
  PASS=$((PASS + 1))
}

run_xml_success_test() {
  local name="$1"
  local xml_file="$2"
  local output_file="$OUTDIR/${name}.icc"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  timeout 30 "$FROMXML" "$xml_file" "$output_file" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ] || [ ! -s "$output_file" ]; then
    echo "  [FAIL] $name -- iccFromXml failed with exit=$exit_code"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (iccFromXml completed without sanitizer findings)"
  PASS=$((PASS + 1))
}

run_tojson_success_test() {
  local name="$1"
  local icc_file="$2"
  local output_file="$OUTDIR/${name}.json"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  timeout 30 "$TOJSON" "$icc_file" "$output_file" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- timed out"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- crashed with signal $((exit_code - 128))"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ] || [ ! -s "$output_file" ]; then
    echo "  [FAIL] $name -- iccToJson failed with exit=$exit_code"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! python3 -c '
import json
import sys

with open(sys.argv[1], encoding="utf-8") as fh:
    doc = json.load(fh)

for entry in doc["IccProfile"]["Tags"]:
    for tag in entry.values():
        data = tag.get("data", {})
        if data.get("type") == "namedColor2Type":
            if not data.get("colorantSuffix", "").startswith("?bad"):
                raise SystemExit("namedColor2 suffix was not sanitized")
            colors = data.get("colors", [])
            if not colors or not colors[0].get("name", "").startswith("?"):
                raise SystemExit("namedColor2 root name was not sanitized")
            raise SystemExit(0)

raise SystemExit("namedColor2 tag not found")
' "$output_file" > "$OUTDIR/${name}-inspect.log" 2>&1; then
    echo "  [FAIL] $name -- JSON output did not contain sanitized namedColor2 strings"
    sed -n '1,5p' "$OUTDIR/${name}-inspect.log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (iccToJson emitted valid sanitized JSON)"
  PASS=$((PASS + 1))
}

run_tojson_valid_json_test() {
  local name="$1"
  local icc_file="$2"
  local output_file="$OUTDIR/${name}.json"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$output_file" "$logfile"

  timeout 30 "$TOJSON" "$icc_file" "$output_file" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "  [FAIL] $name -- timed out"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ge 129 ] && [ "$exit_code" -le 192 ]; then
    echo "  [FAIL] $name -- crashed with signal $((exit_code - 128))"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ] || [ ! -s "$output_file" ]; then
    echo "  [FAIL] $name -- iccToJson failed with exit=$exit_code"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! python3 -m json.tool "$output_file" > "$OUTDIR/${name}-jsoncheck.log" 2>&1; then
    echo "  [FAIL] $name -- iccToJson output is not valid JSON"
    sed -n '1,5p' "$OUTDIR/${name}-jsoncheck.log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (iccToJson emitted valid JSON)"
  PASS=$((PASS + 1))
}

# Issues #2550 and #2541, through the tools. iccToJson used to write every
# colorantTableType as raw "16bit" because the tag could not find its profile, so
# its Lab and XYZ branches were unreachable from the CLI. This builds a profile
# with iccFromJson, converts it back with iccToJson, and requires the declared
# PCS encoding and the document's coordinates. It then rebuilds the profile from
# that JSON and requires the 'clrt' tag bytes to match the first build exactly.
# json-colorant-parent-profile.cpp covers every 16-bit value in-process; this
# covers the shipped tools.
run_colorant_table_cli_roundtrip_test() {
  local name="$1"
  local json_file="$2"
  local encoding="$3"
  local first_icc="$OUTDIR/${name}-first.icc"
  local mid_json="$OUTDIR/${name}-tojson.json"
  local second_icc="$OUTDIR/${name}-second.icc"
  local logfile="$OUTDIR/${name}.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$first_icc" "$mid_json" "$second_icc" "$logfile"

  {
    timeout 30 "$FROMJSON" "$json_file" "$first_icc" &&
    timeout 30 "$TOJSON" "$first_icc" "$mid_json" &&
    timeout 30 "$FROMJSON" "$mid_json" "$second_icc"
  } > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$exit_code" -ne 0 ] || [ ! -s "$first_icc" ] || [ ! -s "$mid_json" ] || [ ! -s "$second_icc" ]; then
    echo "  [FAIL] $name -- iccFromJson/iccToJson/iccFromJson failed with exit=$exit_code"
    sed -n '1,5p' "$logfile"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! python3 -c '
import json
import struct
import sys

source_path, mid_path, first_path, second_path, encoding = sys.argv[1:6]

def colorant_table(path):
    with open(path, encoding="utf-8") as fh:
        doc = json.load(fh)
    for entry in doc["IccProfile"]["Tags"]:
        for tag in entry.values():
            data = tag.get("data", {})
            if data.get("type") == "colorantTableType":
                return data
    raise SystemExit(path + ": no colorantTableType tag")

source = colorant_table(source_path)
mid = colorant_table(mid_path)
if mid.get("pcsEncoding") != encoding:
    raise SystemExit("iccToJson wrote pcsEncoding %r, expected %r"
                     % (mid.get("pcsEncoding"), encoding))
if len(mid["colorantTable"]) != len(source["colorantTable"]):
    raise SystemExit("iccToJson changed the number of colorants")
for want, got in zip(source["colorantTable"], mid["colorantTable"]):
    if got.get("name") != want["name"]:
        raise SystemExit("colorant name %r came back as %r" % (want["name"], got.get("name")))
    # One U16 step is about 0.0015 of L*, 0.0039 of a*/b*, and 0.00003 of XYZ.
    for w, g in zip(want["pcs"], got["pcs"]):
        if abs(w - g) > 0.005:
            raise SystemExit("colorant %r pcs %r came back as %r" % (want["name"], want["pcs"], got["pcs"]))

def clrt_bytes(path):
    data = open(path, "rb").read()
    count = struct.unpack(">I", data[128:132])[0]
    for i in range(count):
        sig, offset, size = struct.unpack(">III", data[132 + 12 * i:144 + 12 * i])
        if sig == 0x636C7274:
            return data[offset:offset + size]
    raise SystemExit(path + ": no clrt tag")

if clrt_bytes(first_path) != clrt_bytes(second_path):
    raise SystemExit("clrt tag bytes differ after the JSON round trip")
' "$json_file" "$mid_json" "$first_icc" "$second_icc" "$encoding" > "$OUTDIR/${name}-inspect.log" 2>&1; then
    echo "  [FAIL] $name -- colorant table did not round-trip as $encoding"
    sed -n '1,5p' "$OUTDIR/${name}-inspect.log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (iccToJson wrote $encoding; clrt bytes survived the round trip)"
  PASS=$((PASS + 1))
}

# Issue #1856. iccToJson emits, at its own documented -indent=4, a document that
# the reader's former fixed 64 MiB cap refused, so the tool could write a file it
# could not read back. This pins that round trip.
#
# The two thresholds are deliberately distinct. Below 64 MiB the fixture has
# stopped exercising anything and the test has lost its teeth, which is a
# failure. Above the bound this build was actually configured with, refusal is
# correct behaviour rather than a regression, so the case is skipped instead.
run_large_indented_roundtrip_test() {
  local xml_name="$1"
  local name="issue-1856-indent4"
  local profile="$OUTDIR/$name-source.icc"
  local json_file="$OUTDIR/$name.json"
  local output_file="$OUTDIR/$name.icc"
  local fromxml_log="$OUTDIR/$name-fromxml.log"
  local tojson_log="$OUTDIR/$name-tojson.log"
  local fromjson_log="$OUTDIR/$name-fromjson.log"
  local exit_code=0
  local json_size=0
  local former_cap=$((64 * 1024 * 1024))
  local configured_cap=$((JSON_MAX_FILE_MB * 1024 * 1024))

  TOTAL=$((TOTAL + 1))
  rm -f "$profile" "$json_file" "$output_file" "$fromxml_log" "$tojson_log" "$fromjson_log"

  if [ ! -f "$TESTING_DIR/hybrid/$xml_name" ]; then
    echo "  [FAIL] $name -- missing fixture $TESTING_DIR/hybrid/$xml_name"
    FAIL=$((FAIL + 1))
    return
  fi

  # Only the source XML is tracked; the profile it builds is ~3.7 MB and the
  # JSON it then emits is ~93 MiB, so both are generated here. iccFromXml runs
  # with Testing/hybrid as its working directory because the XML pulls in
  # sibling data files by relative path.
  ( cd "$TESTING_DIR/hybrid" && timeout 300 "$FROMXML" "$xml_name" "$profile" ) \
    > "$fromxml_log" 2>&1 || exit_code=$?
  if ! check_sanitizers "$name-fromxml" "$fromxml_log"; then
    FAIL=$((FAIL + 1))
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$profile" ]; then
    echo "  [FAIL] $name -- iccFromXml failed to build the fixture with exit=$exit_code"
    sed -n '1,5p' "$fromxml_log"
    FAIL=$((FAIL + 1))
    return
  fi

  exit_code=0
  timeout 300 "$TOJSON" "$profile" "$json_file" -indent=4 > "$tojson_log" 2>&1 || exit_code=$?
  if ! check_sanitizers "$name-tojson" "$tojson_log"; then
    FAIL=$((FAIL + 1))
    rm -f "$profile" "$json_file"
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$json_file" ]; then
    echo "  [FAIL] $name -- iccToJson failed with exit=$exit_code"
    sed -n '1,5p' "$tojson_log"
    FAIL=$((FAIL + 1))
    rm -f "$profile" "$json_file"
    return
  fi

  json_size="$(wc -c < "$json_file")"
  if [ "$json_size" -le "$former_cap" ]; then
    echo "  [FAIL] $name -- fixture emits $json_size bytes, no longer above the former 64 MiB cap"
    FAIL=$((FAIL + 1))
    rm -f "$profile" "$json_file"
    return
  fi
  if [ "$json_size" -gt "$configured_cap" ]; then
    echo "  [SKIP] $name -- $json_size bytes exceeds this build's ${JSON_MAX_FILE_MB} MiB limit; refusal is correct"
    SKIPPED=$((SKIPPED + 1))
    TOTAL=$((TOTAL - 1))
    rm -f "$profile" "$json_file"
    return
  fi

  exit_code=0
  timeout 300 "$FROMJSON" "$json_file" "$output_file" > "$fromjson_log" 2>&1 || exit_code=$?
  # Drop the multi-MiB intermediates as soon as they have been read, so this
  # test's peak disk footprint is not carried through the rest of the suite.
  rm -f "$profile" "$json_file"
  if ! check_sanitizers "$name-fromjson" "$fromjson_log"; then
    FAIL=$((FAIL + 1))
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$output_file" ]; then
    echo "  [FAIL] $name -- iccFromJson rejected $json_size bytes under a ${JSON_MAX_FILE_MB} MiB limit (exit=$exit_code)"
    sed -n '1,5p' "$fromjson_log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name (${json_size} byte -indent=4 document round-tripped)"
  PASS=$((PASS + 1))
  rm -f "$output_file"
}

if [ ! -x "$TOJSON" ] || [ ! -x "$FROMJSON" ] || [ ! -x "$FROMXML" ]; then
  echo "ERROR: IccToJson, IccFromJson, and IccFromXml are required"
  exit 1
fi

PROFILE=""
for candidate in \
  "$TESTING_DIR/Display/sRGB_D65_MAT.icc" \
  "$TESTING_DIR/sRGB_v4_ICC_preference.icc" \
  "$TESTING_DIR/Display/sRGB_D65_MAT-500cdm2.icc"; do
  if [ -f "$candidate" ]; then
    PROFILE="$candidate"
    break
  fi
done

if [ -z "$PROFILE" ]; then
  PROFILE="$(find "$TESTING_DIR" -name '*.icc' -size +100c 2>/dev/null | sed -n '1p')"
fi

if [ -z "$PROFILE" ]; then
  echo "ERROR: No ICC profile found in $TESTING_DIR"
  exit 1
fi

XML_PROFILE=""
for candidate in \
  "$TESTING_DIR/ICS/Spec400_10_700-D50_2deg-Part1.xml" \
  "$TESTING_DIR/HDR/BT2100HlgFullScene.xml" \
  "$TESTING_DIR/ICS/Rec2100HlgFull-Part2.xml"; do
  if [ -f "$candidate" ]; then
    XML_PROFILE="$candidate"
    break
  fi
done

if [ -z "$XML_PROFILE" ]; then
  XML_PROFILE="$(find "$TESTING_DIR" -name '*.xml' -size +100c 2>/dev/null | sed -n '1p')"
fi

if [ -z "$XML_PROFILE" ]; then
  echo "ERROR: No XML profile found in $TESTING_DIR"
  exit 1
fi

BASE_JSON="$OUTDIR/base-profile.json"
base_exit=0
timeout 30 "$TOJSON" "$PROFILE" "$BASE_JSON" > "$OUTDIR/tojson.log" 2>&1 || base_exit=$?
if [ "$base_exit" -ne 0 ] || [ ! -s "$BASE_JSON" ]; then
  echo "ERROR: Unable to create base JSON from $PROFILE"
  sed -n '1,10p' "$OUTDIR/tojson.log"
  exit 1
fi

python3 -c '
import copy
import json
import os
import sys

base_path, outdir = sys.argv[1:3]
base = json.load(open(base_path, encoding="utf-8"))

def write(name, tag):
    doc = copy.deepcopy(base)
    doc["IccProfile"]["Tags"].append({"PrivateTag_999": {"sig": "ZZZ1", "data": tag}})
    with open(os.path.join(outdir, name + ".json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)

write("mpe-calculator-missing-main", {
    "type": "multiProcessElementType",
    "inputChannels": 1,
    "outputChannels": 1,
    "elements": [{
        "type": "CalculatorElement",
        "inputChannels": 1,
        "outputChannels": 1
    }]
})

write("inline-clut-truncated", {
    "type": "multiProcessElementType",
    "inputChannels": 1,
    "outputChannels": 1,
    "elements": [{
        "type": "CLutElement",
        "inputChannels": 1,
        "outputChannels": 1,
        "clut": {"gridPoints": [2], "data": [0.0]}
    }]
})

write("curve-gamma-out-of-range", {
    "type": "curveType",
    "curveType": "gamma",
    "gamma": 2.19921880909168E8
})

write("mpe-matrix-huge-channels", {
    "type": "multiProcessElementType",
    "inputChannels": 1,
    "outputChannels": 1,
    "elements": [{
        "type": "MatrixElement",
        "inputChannels": 65535,
        "outputChannels": 65535,
        "matrix": []
    }]
})

write("mpe-matrix-outputchannels-out-of-int-range", {
    "type": "multiProcessElementType",
    "inputChannels": 1,
    "outputChannels": 1,
    "elements": [{
        "type": "MatrixElement",
        "inputChannels": 3,
        "outputChannels": 3.3333333333333333E25,
        "matrix": []
    }]
})

write("mpe-matrix-short-data", {
    "type": "multiProcessElementType",
    "inputChannels": 3,
    "outputChannels": 3,
    "elements": [{
        "type": "MatrixElement",
        "inputChannels": 3,
        "outputChannels": 3,
        "matrix": [1.0]
    }]
})

write("spectral-white-short", {
    "type": "multiProcessElementType",
    "inputChannels": 1,
    "outputChannels": 3,
    "elements": [{
        "type": "EmissionMatrixElement",
        "inputChannels": 1,
        "outputChannels": 3,
        "wavelengths": {"start": 400.0, "end": 420.0, "steps": 3},
        "whiteData": [1.0],
        "matrixData": [1.0, 1.0, 1.0]
    }]
})

write("spectral-matrix-huge-channels", {
    "type": "multiProcessElementType",
    "inputChannels": 65535,
    "outputChannels": 3,
    "elements": [{
        "type": "EmissionMatrixElement",
        "inputChannels": 65535,
        "outputChannels": 3,
        "wavelengths": {"start": 400.0, "end": 420.0, "steps": 3},
        "whiteData": [1.0, 1.0, 1.0],
        "matrixData": []
    }]
})

write("spectral-offset-short", {
    "type": "multiProcessElementType",
    "inputChannels": 1,
    "outputChannels": 3,
    "elements": [{
        "type": "EmissionMatrixElement",
        "inputChannels": 1,
        "outputChannels": 3,
        "wavelengths": {"start": 400.0, "end": 420.0, "steps": 3},
        "whiteData": [1.0, 1.0, 1.0],
        "matrixData": [1.0, 1.0, 1.0],
        "offsetData": [0.0]
    }]
})

write("struct-bad-member", {
    "type": "tagStructType",
    "structureType": "privateStruct",
    "structureSignature": "tst1",
    "memberTags": [{"badMember": {"sig": "abcd"}}]
})

write("utf16-short-text", {
    "type": "utf16Type",
    "text": "AA"
})

def write_tag_entry(name, entry):
    doc = copy.deepcopy(base)
    doc["IccProfile"]["Tags"].append(entry)
    with open(os.path.join(outdir, name + ".json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)

write_tag_entry("empty-tag-name", {
    "": {
        "sig": "ZZE1",
        "data": {
            "type": "utf16Type",
            "text": ""
        }
    }
})

write("struct-empty-member-name", {
    "type": "tagStructType",
    "structureType": "brdfTransformStructure",
    "memberTags": [{
        "": {
            "sig": "abcd",
            "type": "utf16Type",
            "text": ""
        }
    }]
})

# #2547: a FormulaSegment needs an integer functionType and exactly the
# parameter count that type defines (ICC.2-2023 Table 111). Each case changes
# one thing from the control, so a refusal can only come from that change.
def formula(name, **changes):
    seg = {"start": "-infinity", "end": "+infinity", "type": "FormulaSegment",
           "functionType": 0, "parameters": [2.0, 1.0, 0.0, 0.0]}
    for key, value in changes.items():
        if value is None:
            del seg[key]
        else:
            seg[key] = value
    write("formula-" + name, {
        "type": "multiProcessElementType",
        "inputChannels": 1,
        "outputChannels": 1,
        "elements": [{
            "type": "CurveSetElement",
            "inputChannels": 1,
            "outputChannels": 1,
            "curves": [{"type": "SegmentedCurve", "segments": [seg]}]
        }]
    })

formula("control")
formula("type6-control", functionType=6, parameters=[0.5, 0.5, 0.0, 1.0, 0.5, 1.0, 1.0])
formula("missing-functiontype", functionType=None)
formula("string-functiontype", functionType="0")
formula("fractional-functiontype", functionType=1.5, parameters=[2.0, 1.0, 1.0, 1.0, 0.0])
formula("missing-parameters", parameters=None)
formula("nonarray-parameters", parameters={"gamma": 2.0})
formula("short-parameters", parameters=[2.0, 1.0, 0.0])
formula("long-parameters", parameters=[2.0, 1.0, 0.0, 0.0, 0.0])
formula("nonnumeric-parameter", parameters=[2.0, "1.0", 0.0, 0.0])
formula("nonfinite-parameter", parameters=[2.0, 1e39, 0.0, 0.0])
' "$BASE_JSON" "$OUTDIR"

XML_MATRIX_HUGE="$OUTDIR/xml-matrix-huge-channels.xml"
python3 -c '
import pathlib
import re
import sys

src_path, dst_path = sys.argv[1:3]
text = pathlib.Path(src_path).read_text(encoding="utf-8")
pattern = re.compile(r"(<MatrixElement\b[^>]*\bInputChannels=\")[^\"]+(\"[^>]*\bOutputChannels=\")[^\"]+(\")")

def replace(match):
    return match.group(1) + "65535" + match.group(2) + "65535" + match.group(3)

text, count = pattern.subn(replace, text, count=1)
if count != 1:
    raise SystemExit("No MatrixElement InputChannels/OutputChannels pair found")
pathlib.Path(dst_path).write_text(text, encoding="utf-8")
' "$XML_PROFILE" "$XML_MATRIX_HUGE"

XML_EMPTY_PRIVATE_TYPE="$OUTDIR/xml-empty-private-type.xml"
python3 -c '
import pathlib
import re
import sys

src_path, dst_path = sys.argv[1:3]
text = pathlib.Path(src_path).read_text(encoding="utf-8")
pattern = re.compile(r"<profileDescriptionTag>\s*<multiLocalizedUnicodeType>.*?</multiLocalizedUnicodeType>\s*</profileDescriptionTag>", re.S)
replacement = "<profileDescriptionTag> <PrivateType type=\"\"><UnknownData>00</UnknownData></PrivateType> </profileDescriptionTag>"
text, count = pattern.subn(replacement, text, count=1)
if count != 1:
    raise SystemExit("No profileDescriptionTag found for private type mutation")
pathlib.Path(dst_path).write_text(text, encoding="utf-8")
' "$XML_PROFILE" "$XML_EMPTY_PRIVATE_TYPE"

# #2547: the XML FormulaSegment reader dropped parameters beyond the count the
# FunctionType defines, and loaded "nan". Both mutate the first segment of a
# tracked document with no external file references; the unmutated document is
# the control.
XML_FORMULA_SRC="$REPO_ROOT/Testing/Encoding/ISO22028-Encoded-sRGB.xml"
XML_FORMULA_EXTRA="$OUTDIR/xml-formula-extra-parameter.xml"
XML_FORMULA_NAN="$OUTDIR/xml-formula-nan-parameter.xml"
python3 -c '
import pathlib
import re
import sys

src_path, extra_path, nan_path = sys.argv[1:4]
text = pathlib.Path(src_path).read_text(encoding="utf-8")
pattern = re.compile(r"(<FormulaSegment\b[^>]*\bFunctionType=\"0\">)1\.0 12\.92 0 0(</FormulaSegment>)")

def mutate(path, body):
    out, count = pattern.subn(lambda m: m.group(1) + body + m.group(2), text, count=1)
    if count != 1:
        raise SystemExit("No type 0 FormulaSegment \"1.0 12.92 0 0\" found")
    pathlib.Path(path).write_text(out, encoding="utf-8")

mutate(extra_path, "1.0 12.92 0 0 0")
mutate(nan_path, "1.0 nan 0 0")
' "$XML_FORMULA_SRC" "$XML_FORMULA_EXTRA" "$XML_FORMULA_NAN"

TEXT_INVALID_ASCII="$OUTDIR/text-invalid-ascii.icc"
python3 -c '
import pathlib
import struct
import sys

src, dst = sys.argv[1:3]
data = bytearray(pathlib.Path(src).read_bytes())
if len(data) < 132:
    raise SystemExit("profile too small")

tag_count = struct.unpack(">I", data[128:132])[0]
for idx in range(tag_count):
    pos = 132 + idx * 12
    if pos + 12 > len(data):
        raise SystemExit("tag table truncated")
    offset, size = struct.unpack(">II", data[pos + 4:pos + 12])
    if offset + size > len(data) or size < 16:
        continue
    data[offset:offset + 8] = b"text\0\0\0\0"
    data[offset + 8] = 0xb0
    data[offset + 9:offset + 16] = b" text\0\0"
    pathlib.Path(dst).write_bytes(data)
    raise SystemExit(0)

raise SystemExit("no tag large enough for textType mutation")
' "$PROFILE" "$TEXT_INVALID_ASCII"

NAMED_PROFILE="$TESTING_DIR/Named/NamedColor.icc"
if [ ! -f "$NAMED_PROFILE" ] && [ -f "$TESTING_DIR/Named/NamedColor.xml" ]; then
  NAMED_PROFILE="$OUTDIR/NamedColor.icc"
  named_exit=0
  timeout 30 "$FROMXML" "$TESTING_DIR/Named/NamedColor.xml" "$NAMED_PROFILE" > "$OUTDIR/namedcolor-fromxml.log" 2>&1 || named_exit=$?
  if [ "$named_exit" -ne 0 ] || [ ! -s "$NAMED_PROFILE" ]; then
    echo "ERROR: Unable to create NamedColor.icc from NamedColor.xml"
    sed -n '1,10p' "$OUTDIR/namedcolor-fromxml.log"
    exit 1
  fi
fi
if [ ! -f "$NAMED_PROFILE" ]; then
  echo "ERROR: NamedColor.icc fixture not found and NamedColor.xml fallback is unavailable"
  exit 1
fi
NAMED_INVALID_ASCII="$OUTDIR/namedcolor-invalid-ascii.icc"
python3 -c '
import pathlib
import struct
import sys

src, dst = sys.argv[1:3]
data = bytearray(pathlib.Path(src).read_bytes())
if len(data) < 132:
    raise SystemExit("profile too small")

tag_count = struct.unpack(">I", data[128:132])[0]
for idx in range(tag_count):
    pos = 132 + idx * 12
    if pos + 12 > len(data):
        raise SystemExit("tag table truncated")
    sig = bytes(data[pos:pos + 4])
    offset, size = struct.unpack(">II", data[pos + 4:pos + 12])
    if sig != b"ncl2":
        continue
    if offset + size > len(data) or size < 122:
        raise SystemExit("namedColor2 tag is truncated")
    payload = bytearray(size)
    payload[0:4] = b"ncl2"
    payload[12:16] = struct.pack(">I", 1)
    payload[20:24] = b"pre\0"
    payload[52:57] = b"\xb0bad\0"
    payload[84:90] = b"\xb1root\0"
    data[offset:offset + size] = payload
    pathlib.Path(dst).write_bytes(data)
    raise SystemExit(0)

raise SystemExit("namedColor2 tag not found")
' "$NAMED_PROFILE" "$NAMED_INVALID_ASCII"

###############################################################################
# responseCurveSet16Type measurement/channel counts (#2398)
###############################################################################
#
# CIccTagJsonResponseCurveSet16::ParseJson read CountOfChannels, DeviceCode and
# Reserved into an int and cast each to the icUInt16Number that actually stores it,
# so a value above the field's ceiling was truncated and the parser wrote a profile
# for a document it had not been given.  Measured against the unfixed parser:
# "CountOfChannels": 65537 produced a file byte-identical to the count-of-1 control,
# "DeviceCode": 65537 one byte-identical to DeviceCode 1, and -1 one byte-identical
# to 65535 -- all three at exit 0.  This is the JSON half of #2398; the XML twin is
# pinned in iccdev-xml-parser-regression-tests.sh.
#
# The fixture is written here in full rather than produced by iccToJson from an ICC
# profile.  Generating it would make these cases depend on the resp tag's READ path,
# and when that path was broken (#2399) they would have gone green by skipping rather
# than red -- the failure mode this file's own reject helper exists to prevent.
# Every document carries the four tags a v2 GRAY mntr profile must have, so the
# CONTROL converts with exit 0.  They were omitted while iccFromJson returned
# EXIT_SUCCESS for a profile it had just declared invalid; #2384 made that an
# EXIT_FAILURE, so a control asserting exit 0 now means what it always claimed.
# The outputResponseTag under test -- its CountOfChannels and DeviceCode -- is
# unchanged, and the overflow cases are still refused by the parser before
# validation is reached.  Mirrors the same change in the XML sibling suite.
#
# The tag TYPES are the v2 ones -- textDescriptionType and textType, not
# multiLocalizedUnicodeType -- because these documents declare ProfileVersion
# 2.10.  Validate() rejects the v4 spellings here as "Invalid tag type (Might
# be critical!)", so the v4 forms would leave the control failing for a second,
# unrelated reason.
write_responsecurve_json() {
  # write_responsecurve_json <path> <count> <device-code>
  cat > "$1" <<JSONEOF
{
  "Header": {
    "ProfileVersion": "2.10.0",
    "ProfileDeviceClass": "mntr",
    "DataColourSpace": "GRAY",
    "PCS": "XYZ ",
    "RenderingIntent": "Perceptual"
  },
  "Tags": [
    {
      "outputResponseTag": {
        "data": {
          "type": "responseCurveSet16Type",
          "CountOfChannels": $2,
          "ResponseCurves": [
            {
              "MeasurementUnit": "Status A",
              "Channels": [
                {
                  "MaxColorantXYZ": [ 0.0, 0.0, 0.0 ],
                  "Measurements": [ { "DeviceCode": $3, "MeasValue": 0.0 } ]
                }
              ]
            }
          ]
        }
      }
    },
    { "grayTRCTag": { "data": { "type": "curveType", "curveType": "gamma", "gamma": 2.19921880909169 } } },
    { "profileDescriptionTag": { "data": { "type": "textDescriptionType",
        "description": "responseCurveSet16 regression fixture" } } },
    { "copyrightTag": { "data": { "type": "textType", "text": "ICC regression fixture" } } },
    { "mediaWhitePointTag": { "data": { "type": "XYZArrayType",
        "XYZ": [ [ 0.964202880859375, 1.0, 0.8249053955078125 ] ] } } }
  ]
}
JSONEOF
}

write_responsecurve_json "$OUTDIR/responsecurve-control.json" 1 1
write_responsecurve_json "$OUTDIR/responsecurve-count-overflow.json" 65537 1
write_responsecurve_json "$OUTDIR/responsecurve-devicecode-overflow.json" 1 65537
write_responsecurve_json "$OUTDIR/responsecurve-devicecode-negative.json" 1 -1

python3 -c '
import copy
import json
import os
import sys

source_path, outdir = sys.argv[1:3]
with open(source_path, encoding="utf-8") as source:
    control = json.load(source)

for name, value in (
    ("short", [50.0, 0.0]),
    ("long", [50.0, 0.0, 0.0, 0.0]),
    ("nonarray", 50.0),
    ("missing", None),
):
    document = copy.deepcopy(control)
    entry = document["IccProfile"]["Tags"][0]["colorantTableTag"]["data"]["colorantTable"][0]
    if value is None:
        del entry["pcs"]
    else:
        entry["pcs"] = value
    with open(os.path.join(outdir, "colorant-table-" + name + "-pcs.json"),
              "w", encoding="utf-8") as output:
        json.dump(document, output, indent=2)
' "$REPO_ROOT/.github/ci/test-data/json-colorant-table-complete-pcs.json" "$OUTDIR"

# #2541/#2550: the structural refusals #2568 added to the colorantTable and
# chromaticity JSON readers, driven through iccFromJson, plus the Lab and XYZ
# documents for the tool round trip. The colorantTable refusals derive from the
# tracked control above. Documents that must load add their tag to the base
# profile instead, because iccFromJson exits 1 for a saved profile that fails
# validation, and the tag-only control does.
python3 -c '
import copy
import json
import os
import sys

source_path, base_path, outdir = sys.argv[1:4]
with open(source_path, encoding="utf-8") as source:
    control = json.load(source)
with open(base_path, encoding="utf-8") as source:
    base = json.load(source)

def save(name, document):
    with open(os.path.join(outdir, name + ".json"), "w", encoding="utf-8") as output:
        json.dump(document, output, indent=2)

def table_variant(name, mutate):
    document = copy.deepcopy(control)
    mutate(document["IccProfile"]["Tags"][0]["colorantTableTag"]["data"])
    save("colorant-table-" + name, document)

table_variant("missing-table", lambda d: d.pop("colorantTable"))
table_variant("nonarray-table", lambda d: d.__setitem__("colorantTable", {"name": "control"}))
table_variant("nonobject-entry", lambda d: d["colorantTable"].__setitem__(0, "control"))
table_variant("missing-name", lambda d: d["colorantTable"][0].pop("name"))
table_variant("nonstring-name", lambda d: d["colorantTable"][0].__setitem__("name", 7))
table_variant("unknown-encoding", lambda d: d.__setitem__("pcsEncoding", "Luv"))
table_variant("nonstring-encoding", lambda d: d.__setitem__("pcsEncoding", 3))

# Three colorants, matching the RGB data colour space of the base profile.
def roundtrip(name, pcs, encoding, entries):
    document = copy.deepcopy(base)
    document["IccProfile"]["Header"]["PCS"] = pcs
    document["IccProfile"]["Tags"].append({"colorantTableTag": {
        "sig": "clrt",
        "data": {
            "type": "colorantTableType",
            "pcsEncoding": encoding,
            "colorantTable": [{"name": n, "pcs": v} for n, v in entries],
        }
    }})
    save(name, document)

roundtrip("colorant-table-lab-roundtrip", "Lab ", "Lab",
          [("red", [54.29, 80.8, 69.89]), ("green", [87.82, -79.28, 80.99]),
           ("blue", [29.57, 68.3, -112.03])])
roundtrip("colorant-table-xyz-roundtrip", "XYZ ", "XYZ",
          [("red", [0.4361, 0.2225, 0.0139]), ("green", [0.3851, 0.7169, 0.0971]),
           ("blue", [0.1431, 0.0606, 0.7141])])

def chroma(name, channels):
    document = copy.deepcopy(base)
    tag = {"sig": "chrm", "data": {"type": "chromaticityType", "colorantType": 0}}
    if channels is not None:
        tag["data"]["channels"] = channels
    document["IccProfile"]["Tags"].append({"chromaticityTag": tag})
    save("chromaticity-" + name, document)

chroma("control", [[0.64, 0.33], [0.3, 0.6], [0.15, 0.06]])
chroma("missing-channels", None)
chroma("nonarray-channels", {"x": 0.64, "y": 0.33})
chroma("three-coordinates", [[0.64, 0.33, 0.03], [0.3, 0.6], [0.15, 0.06]])
chroma("one-coordinate", [[0.64], [0.3, 0.6], [0.15, 0.06]])
chroma("nonnumeric-coordinate", [[0.64, "0.33"], [0.3, 0.6], [0.15, 0.06]])
chroma("xy-object", [{"x": 0.64, "y": 0.33}, [0.3, 0.6], [0.15, 0.06]])
' "$REPO_ROOT/.github/ci/test-data/json-colorant-table-complete-pcs.json" "$BASE_JSON" "$OUTDIR"

echo "Using base profile: $PROFILE"
echo "Using XML profile:  $XML_PROFILE"
echo "Tools dir: $TOOLS_DIR"
echo ""

run_reject_test "mpe-calculator-missing-main" "$OUTDIR/mpe-calculator-missing-main.json" "Missing mainFunction in CalculatorElement"
run_reject_test "inline-clut-truncated" "$OUTDIR/inline-clut-truncated.json" "Inline CLUT data count does not match CLUT size"
run_reject_test "curve-gamma-out-of-range" "$OUTDIR/curve-gamma-out-of-range.json" "Invalid gamma in curveType"
run_reject_test "mpe-matrix-huge-channels" "$OUTDIR/mpe-matrix-huge-channels.json" "Invalid inputChannels or outputChannels in MatrixElement"
run_reject_test "mpe-matrix-outputchannels-out-of-int-range" "$OUTDIR/mpe-matrix-outputchannels-out-of-int-range.json" "Invalid inputChannels or outputChannels in MatrixElement"
run_reject_test "mpe-matrix-short-data" "$OUTDIR/mpe-matrix-short-data.json" "matrix count does not match MatrixElement size"
run_reject_test "spectral-white-short" "$OUTDIR/spectral-white-short.json" "whiteData count does not match spectral element size"
run_reject_test "spectral-matrix-huge-channels" "$OUTDIR/spectral-matrix-huge-channels.json" "Invalid inputChannels or outputChannels in spectral matrix element"
run_reject_test "spectral-offset-short" "$OUTDIR/spectral-offset-short.json" "offsetData count does not match spectral element size"
run_reject_test "struct-bad-member" "$OUTDIR/struct-bad-member.json" "MemberTag 'badMember' missing 'type' field"
run_fromjson_success_test "utf16-short-text" "$OUTDIR/utf16-short-text.json"
run_reject_test "empty-tag-name" "$OUTDIR/empty-tag-name.json" "Tag entry has empty name"
run_reject_test "colorant-table-nonnumeric-pcs" "$REPO_ROOT/.github/ci/test-data/json-colorant-table-nonnumeric-pcs.json" "colorantTableType pcs must contain three numeric values"
run_reject_test "colorant-table-short-pcs" "$OUTDIR/colorant-table-short-pcs.json" "colorantTableType pcs must contain three numeric values"
run_reject_test "colorant-table-long-pcs" "$OUTDIR/colorant-table-long-pcs.json" "colorantTableType pcs must contain three numeric values"
run_reject_test "colorant-table-nonarray-pcs" "$OUTDIR/colorant-table-nonarray-pcs.json" "colorantTableType pcs must contain three numeric values"
run_reject_test "colorant-table-missing-pcs" "$OUTDIR/colorant-table-missing-pcs.json" "colorantTableType pcs must contain three numeric values"
run_reject_test "colorant-table-missing-table" "$OUTDIR/colorant-table-missing-table.json" "colorantTableType requires a colorantTable array"
run_reject_test "colorant-table-nonarray-table" "$OUTDIR/colorant-table-nonarray-table.json" "colorantTableType requires a colorantTable array"
run_reject_test "colorant-table-nonobject-entry" "$OUTDIR/colorant-table-nonobject-entry.json" "colorantTableType entry requires a string name"
run_reject_test "colorant-table-missing-name" "$OUTDIR/colorant-table-missing-name.json" "colorantTableType entry requires a string name"
run_reject_test "colorant-table-nonstring-name" "$OUTDIR/colorant-table-nonstring-name.json" "colorantTableType entry requires a string name"
run_reject_test "colorant-table-unknown-encoding" "$OUTDIR/colorant-table-unknown-encoding.json" "colorantTableType pcsEncoding must be"
run_reject_test "colorant-table-nonstring-encoding" "$OUTDIR/colorant-table-nonstring-encoding.json" "colorantTableType pcsEncoding must be"
run_colorant_table_cli_roundtrip_test "colorant-table-lab-roundtrip" "$OUTDIR/colorant-table-lab-roundtrip.json" "Lab"
run_colorant_table_cli_roundtrip_test "colorant-table-xyz-roundtrip" "$OUTDIR/colorant-table-xyz-roundtrip.json" "XYZ"
run_fromjson_success_test "chromaticity-control" "$OUTDIR/chromaticity-control.json"
run_reject_test "chromaticity-missing-channels" "$OUTDIR/chromaticity-missing-channels.json" "chromaticityType requires a channels array"
run_reject_test "chromaticity-nonarray-channels" "$OUTDIR/chromaticity-nonarray-channels.json" "chromaticityType requires a channels array"
run_reject_test "chromaticity-three-coordinates" "$OUTDIR/chromaticity-three-coordinates.json" "chromaticityType channel must be a pair of numbers"
run_reject_test "chromaticity-one-coordinate" "$OUTDIR/chromaticity-one-coordinate.json" "chromaticityType channel must be a pair of numbers"
run_reject_test "chromaticity-nonnumeric-coordinate" "$OUTDIR/chromaticity-nonnumeric-coordinate.json" "chromaticityType channel must be a pair of numbers"
run_reject_test "chromaticity-xy-object" "$OUTDIR/chromaticity-xy-object.json" "chromaticityType channel must be a pair of numbers"

# The control runs first and must convert: without it, a change that refused every
# responseCurveSet16Type document would satisfy all three reject cases below while
# removing the guards they exist to pin.
run_fromjson_success_test "responsecurve-control" "$OUTDIR/responsecurve-control.json"
run_reject_test "responsecurve-count-overflow" "$OUTDIR/responsecurve-count-overflow.json" "Missing or invalid CountOfChannels in ResponseCurveSet16"
run_reject_test "responsecurve-devicecode-overflow" "$OUTDIR/responsecurve-devicecode-overflow.json" "Invalid Measurement DeviceCode in ResponseCurveSet16"
run_reject_test "responsecurve-devicecode-negative" "$OUTDIR/responsecurve-devicecode-negative.json" "Invalid Measurement DeviceCode in ResponseCurveSet16"
run_reject_test "struct-empty-member-name" "$OUTDIR/struct-empty-member-name.json" "MemberTag entry has empty name"
run_xml_reject_test "xml-matrix-huge-channels" "$XML_MATRIX_HUGE" "Invalid InputChannels or OutputChannels In MatrixElement"
run_xml_reject_test "xml-empty-private-type" "$XML_EMPTY_PRIVATE_TYPE" "Invalid private tag type attribute"
run_fromjson_success_test "formula-control" "$OUTDIR/formula-control.json"
run_fromjson_success_test "formula-type6-control" "$OUTDIR/formula-type6-control.json"
run_reject_test "formula-missing-functiontype" "$OUTDIR/formula-missing-functiontype.json" "FormulaSegment requires an integer functionType"
run_reject_test "formula-string-functiontype" "$OUTDIR/formula-string-functiontype.json" "FormulaSegment requires an integer functionType"
run_reject_test "formula-fractional-functiontype" "$OUTDIR/formula-fractional-functiontype.json" "FormulaSegment requires an integer functionType"
run_reject_test "formula-missing-parameters" "$OUTDIR/formula-missing-parameters.json" "FormulaSegment parameters must be an array of the count its functionType requires"
run_reject_test "formula-nonarray-parameters" "$OUTDIR/formula-nonarray-parameters.json" "FormulaSegment parameters must be an array of the count its functionType requires"
run_reject_test "formula-short-parameters" "$OUTDIR/formula-short-parameters.json" "FormulaSegment parameters must be an array of the count its functionType requires"
run_reject_test "formula-long-parameters" "$OUTDIR/formula-long-parameters.json" "FormulaSegment parameters must be an array of the count its functionType requires"
run_reject_test "formula-nonnumeric-parameter" "$OUTDIR/formula-nonnumeric-parameter.json" "parameters contains a non-numeric or non-finite value in FormulaSegment"
run_reject_test "formula-nonfinite-parameter" "$OUTDIR/formula-nonfinite-parameter.json" "parameters contains a non-numeric or non-finite value in FormulaSegment"
run_xml_success_test "xml-formula-control" "$XML_FORMULA_SRC"
run_xml_reject_test "xml-formula-extra-parameter" "$XML_FORMULA_EXTRA" "FormulaSegment parameter count does not match its FunctionType"
run_xml_reject_test "xml-formula-nan-parameter" "$XML_FORMULA_NAN" "Non-finite parameter in FormulaSegment"
run_tojson_valid_json_test "text-invalid-ascii-tojson" "$TEXT_INVALID_ASCII"
run_tojson_success_test "namedcolor-invalid-ascii-tojson" "$NAMED_INVALID_ASCII"
run_large_indented_roundtrip_test "CMYK-W_Overprint_Profile.xml"

HELPER_SRC="$OUTDIR/json-config-helper-test.cpp"
HELPER_BIN="$OUTDIR/json-config-helper-test"
HELPER_LOG="$OUTDIR/json-config-helper-test.log"

find_library_file() {
  local dir="$1"
  local base="$2"
  local candidate

  for candidate in \
    "$dir/lib${base}d.so" \
    "$dir/lib${base}d.so."* \
    "$dir/lib${base}.so" \
    "$dir/lib${base}.so."* \
    "$dir/lib${base}-staticd.a" \
    "$dir/lib${base}-static.a"; do
    if [ -f "$candidate" ] || [ -L "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

cat > "$HELPER_SRC" <<'CPP'
#include "IccCmmConfig.h"
#include "IccJsonUtil.h"

#include <cmath>
#include <iostream>

int main()
{
  int failures = 0;
  double vals[2] = {0.0, 0.0};

  if (!jsonToArray(json::array({1.0, 2.0}), vals, 2)) {
    std::cerr << "fixed-size array rejected exact count\n";
    failures++;
  }
  if (jsonToArray(json::array({1.0}), vals, 2)) {
    std::cerr << "fixed-size array accepted short count\n";
    failures++;
  }
  if (jsonToArray(json::array({1.0, 2.0, 3.0}), vals, 2)) {
    std::cerr << "fixed-size array accepted long count\n";
    failures++;
  }
  if (jsonToArray(json::array({1.0, "bad"}), vals, 2)) {
    std::cerr << "fixed-size array accepted non-numeric value\n";
    failures++;
  }

  CIccCfgPccWeight weight;
  if (!weight.fromJson(json{{"pccFile", "stale.icc"}, {"weight", 2.5}}, true)) {
    std::cerr << "pcc weight failed initial parse\n";
    failures++;
  }
  if (weight.fromJson(json::object(), true)) {
    std::cerr << "pcc weight accepted empty object\n";
    failures++;
  }
  if (!weight.m_pccPath.empty() || std::fabs(weight.m_dWeight) > 0.000001) {
    std::cerr << "pcc weight retained stale fields\n";
    failures++;
  }

  CIccCfgSearchApply search;
  const char *args[] = {"1", "dummy.icc", "1", "-INIT", "1"};
  if (!search.fromArgs(args, 5, true) || !search.isInitialized()) {
    std::cerr << "search apply failed initial state setup\n";
    failures++;
  }
  search.reset();
  if (search.isInitialized()) {
    std::cerr << "search apply retained stale initial state\n";
    failures++;
  }

  return failures ? 1 : 0;
}
CPP

TOTAL=$((TOTAL + 3))
helper_compile=0
if [ -n "${CXX:-}" ]; then
  HELPER_CXX="$CXX"
elif command -v clang++-18 >/dev/null 2>&1; then
  HELPER_CXX="clang++-18"
elif command -v clang++ >/dev/null 2>&1; then
  HELPER_CXX="clang++"
else
  HELPER_CXX="c++"
fi
PROFLIB="$(find_library_file "$BUILD_ROOT/IccProfLib" "IccProfLib2" 2>/dev/null || true)"
XMLLIB="$(find_library_file "$BUILD_ROOT/IccXML" "IccXML2" 2>/dev/null || true)"
LINK_EXTRA=()
if [ -n "$PROFLIB" ] && [[ "$PROFLIB" == *.a ]] &&
    grep -q '^ICC_USE_ZLIB:BOOL=ON$' "${ICCDEV_CMAKE_CACHE:-$BUILD_ROOT/CMakeCache.txt}" 2>/dev/null; then
  LINK_EXTRA+=(-lz)
fi

if [ -z "$PROFLIB" ] || [ -z "$XMLLIB" ]; then
  {
    echo "Missing linkable iccDEV libraries"
    echo "BUILD_ROOT=$BUILD_ROOT"
    echo "IccProfLib candidates:"
    find "$BUILD_ROOT/IccProfLib" -maxdepth 1 -name 'libIccProfLib2*' -print 2>/dev/null | sort
    echo "IccXML candidates:"
    find "$BUILD_ROOT/IccXML" -maxdepth 1 -name 'libIccXML2*' -print 2>/dev/null | sort
  } > "$HELPER_LOG"
  helper_compile=1
else
  "$HELPER_CXX" -std=c++17 \
    -fsanitize=address,undefined \
    -I"$REPO_ROOT" \
    -I"$REPO_ROOT/IccProfLib" \
    -I"$REPO_ROOT/IccXML/IccLibXML" \
    -I"$REPO_ROOT/IccJSON/IccLibJSON" \
    -I"$REPO_ROOT/IccConnect/IccLibConnect" \
    "$HELPER_SRC" \
    "$REPO_ROOT/IccConnect/IccLibConnect/IccCmmConfig.cpp" \
    "$REPO_ROOT/IccConnect/IccLibConnect/IccJsonUtil.cpp" \
    -Wl,-rpath,"$BUILD_ROOT/IccProfLib" \
    -Wl,-rpath,"$BUILD_ROOT/IccXML" \
    "$PROFLIB" "$XMLLIB" \
    "${LINK_EXTRA[@]}" \
    -o "$HELPER_BIN" > "$HELPER_LOG" 2>&1 || helper_compile=$?
fi

if [ "$helper_compile" -ne 0 ]; then
  echo "  [FAIL] json-config-helper-build -- compile failed"
  sed -n '1,10p' "$HELPER_LOG"
  FAIL=$((FAIL + 1))
elif "$HELPER_BIN" > "$HELPER_LOG" 2>&1; then
  echo "  [PASS] fixed-size-json-array-helper"
  echo "  [PASS] pcc-weight-reset"
  echo "  [PASS] search-apply-reset"
  PASS=$((PASS + 3))
else
  echo "  [FAIL] json-config-helper-runtime"
  sed -n '1,10p' "$HELPER_LOG"
  FAIL=$((FAIL + 1))
fi
rm -f "$HELPER_BIN"

echo ""
echo "JSON/XML parser/config regression summary:"
echo "  Total:      $TOTAL"
echo "  Passed:     $PASS"
echo "  Failed:     $FAIL"
echo "  Skipped:    $SKIPPED"
echo "  ASAN:       $ASAN_FINDINGS"
echo "  UBSAN:      $UBSAN_FINDINGS"

if [ "$ASAN_FINDINGS" -gt 0 ] || [ "$UBSAN_FINDINGS" -gt 0 ]; then
  exit 2
fi

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

exit 0
