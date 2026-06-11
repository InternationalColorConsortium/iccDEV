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

TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd)"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=0

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

if [ ! -x "$TOJSON" ] || [ ! -x "$FROMJSON" ] || [ ! -x "$FROMXML" ]; then
  echo "ERROR: IccToJson, IccFromJson, and IccFromXml are required"
  exit 1
fi

PROFILE=""
for candidate in \
  "$TESTING_DIR/Display/sRGB_D65_MAT.icc" \
  "$TESTING_DIR/sRGB_v4_ICC_preference.icc" \
  "$TESTING_DIR/Display/sRGB_D65_MAT-500lx.icc"; do
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
run_reject_test "struct-empty-member-name" "$OUTDIR/struct-empty-member-name.json" "MemberTag entry has empty name"
run_xml_reject_test "xml-matrix-huge-channels" "$XML_MATRIX_HUGE" "Invalid InputChannels or OutputChannels In MatrixElement"
run_xml_reject_test "xml-empty-private-type" "$XML_EMPTY_PRIVATE_TYPE" "Invalid private tag type attribute"
run_tojson_valid_json_test "text-invalid-ascii-tojson" "$TEXT_INVALID_ASCII"
run_tojson_success_test "namedcolor-invalid-ascii-tojson" "$NAMED_INVALID_ASCII"

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
  if (!weight.fromJson(json::object(), true)) {
    std::cerr << "pcc weight failed reset parse\n";
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
    -I"$REPO_ROOT/IccConnect/IccLibConnect" \
    "$HELPER_SRC" \
    "$REPO_ROOT/IccConnect/IccLibConnect/IccCmmConfig.cpp" \
    "$REPO_ROOT/IccConnect/IccLibConnect/IccJsonUtil.cpp" \
    -Wl,-rpath,"$BUILD_ROOT/IccProfLib" \
    -Wl,-rpath,"$BUILD_ROOT/IccXML" \
    "$PROFLIB" "$XMLLIB" \
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
echo "  ASAN:       $ASAN_FINDINGS"
echo "  UBSAN:      $UBSAN_FINDINGS"

if [ "$ASAN_FINDINGS" -gt 0 ] || [ "$UBSAN_FINDINGS" -gt 0 ]; then
  exit 2
fi

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

exit 0
