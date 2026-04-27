#!/bin/bash
###############################################################################
# iccDEV standard observer compatibility regression tests
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-stdobserver-regressions}"
mkdir -p "$OUTDIR"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"
DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

check_log() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    echo "  [FAIL] $name -- AddressSanitizer finding"
    FAIL=$((FAIL + 1))
    return 1
  fi

  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    if grep "runtime error:" "$logfile" | grep -v "/IccProfLib/IccMD5.cpp:" >/dev/null 2>&1; then
      echo "  [FAIL] $name -- undefined behavior"
      FAIL=$((FAIL + 1))
      return 1
    fi
    echo "  [WARN] $name -- ignored known MD5 unsigned-shift instrumentation noise"
  fi

  return 0
}

run_observer_test() {
  local name="$1"
  local source_xml="$2"
  local search_text="$3"
  local replace_text="$4"
  local expected_dump="$5"
  local case_xml="$OUTDIR/${name}.xml"
  local case_icc="$OUTDIR/${name}.icc"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local dump_log="$OUTDIR/${name}-dump.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$case_xml" "$case_icc" "$fromxml_log" "$dump_log"

  if [ ! -f "$source_xml" ]; then
    echo "  [FAIL] $name -- missing source XML: $source_xml"
    FAIL=$((FAIL + 1))
    return
  fi

  sed "s|$search_text|$replace_text|g" "$source_xml" > "$case_xml"

  timeout 30 "$FROMXML" "$case_xml" "$case_icc" > "$fromxml_log" 2>&1 || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$case_icc" ]; then
    echo "  [FAIL] $name -- iccFromXml failed with exit=$exit_code"
    sed -n '1,10p' "$fromxml_log"
    FAIL=$((FAIL + 1))
    return
  fi

  exit_code=0
  timeout 30 "$DUMP" "$case_icc" svcn > "$dump_log" 2>&1 || exit_code=$?
  if ! check_log "$name/dump" "$dump_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    echo "  [FAIL] $name -- iccDumpProfile failed with exit=$exit_code"
    sed -n '1,10p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! grep -Fq "$expected_dump" "$dump_log"; then
    echo "  [FAIL] $name -- expected observer not found: $expected_dump"
    sed -n '1,20p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  if grep -Fq "StdObserver: Unknown observer" "$dump_log"; then
    echo "  [FAIL] $name -- observer was parsed as unknown"
    sed -n '1,20p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name"
  PASS=$((PASS + 1))
}

run_observer_json_test() {
  local name="$1"
  local source_xml="$2"
  local search_text="$3"
  local replace_text="$4"
  local json_observer="$5"
  local expected_dump="$6"
  local case_xml="$OUTDIR/${name}.xml"
  local base_icc="$OUTDIR/${name}-base.icc"
  local base_json="$OUTDIR/${name}-base.json"
  local case_json="$OUTDIR/${name}.json"
  local case_icc="$OUTDIR/${name}.icc"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local tojson_log="$OUTDIR/${name}-tojson.log"
  local fromjson_log="$OUTDIR/${name}-fromjson.log"
  local dump_log="$OUTDIR/${name}-dump.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$case_xml" "$base_icc" "$base_json" "$case_json" "$case_icc"
  rm -f "$fromxml_log" "$tojson_log" "$fromjson_log" "$dump_log"

  if [ ! -f "$source_xml" ]; then
    echo "  [FAIL] $name -- missing source XML: $source_xml"
    FAIL=$((FAIL + 1))
    return
  fi

  sed "s|$search_text|$replace_text|g" "$source_xml" > "$case_xml"

  timeout 30 "$FROMXML" "$case_xml" "$base_icc" > "$fromxml_log" 2>&1 || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$base_icc" ]; then
    echo "  [FAIL] $name -- iccFromXml failed with exit=$exit_code"
    sed -n '1,10p' "$fromxml_log"
    FAIL=$((FAIL + 1))
    return
  fi

  exit_code=0
  timeout 30 "$TOJSON" "$base_icc" "$base_json" > "$tojson_log" 2>&1 || exit_code=$?
  if ! check_log "$name/tojson" "$tojson_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$base_json" ]; then
    echo "  [FAIL] $name -- iccToJson failed with exit=$exit_code"
    sed -n '1,10p' "$tojson_log"
    FAIL=$((FAIL + 1))
    return
  fi

  python3 - "$base_json" "$case_json" "$json_observer" <<'PY'
import json
import sys

base_path, out_path, observer = sys.argv[1:4]
with open(base_path, encoding="utf-8") as fh:
    doc = json.load(fh)

seen = False

def replace_std_observer(value):
    global seen
    if isinstance(value, dict):
        if "StdObserver" in value:
            value["StdObserver"] = observer
            seen = True
        for child in value.values():
            replace_std_observer(child)
    elif isinstance(value, list):
        for child in value:
            replace_std_observer(child)

replace_std_observer(doc)
if not seen:
    raise SystemExit("StdObserver field not found in JSON")

with open(out_path, "w", encoding="utf-8") as fh:
    json.dump(doc, fh, indent=2)
    fh.write("\n")
PY

  exit_code=0
  timeout 30 "$FROMJSON" "$case_json" "$case_icc" > "$fromjson_log" 2>&1 || exit_code=$?
  if ! check_log "$name/fromjson" "$fromjson_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$case_icc" ]; then
    echo "  [FAIL] $name -- iccFromJson failed with exit=$exit_code"
    sed -n '1,10p' "$fromjson_log"
    FAIL=$((FAIL + 1))
    return
  fi

  exit_code=0
  timeout 30 "$DUMP" "$case_icc" svcn > "$dump_log" 2>&1 || exit_code=$?
  if ! check_log "$name/dump" "$dump_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    echo "  [FAIL] $name -- iccDumpProfile failed with exit=$exit_code"
    sed -n '1,10p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  if ! grep -Fq "$expected_dump" "$dump_log"; then
    echo "  [FAIL] $name -- expected observer not found: $expected_dump"
    sed -n '1,20p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  if grep -Fq "StdObserver: Unknown observer" "$dump_log"; then
    echo "  [FAIL] $name -- observer was parsed as unknown"
    sed -n '1,20p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  echo "  [PASS] $name"
  PASS=$((PASS + 1))
}

if [ ! -x "$FROMXML" ] || [ ! -x "$TOJSON" ] || [ ! -x "$FROMJSON" ] || [ ! -x "$DUMP" ]; then
  echo "ERROR: IccFromXml, IccToJson, IccFromJson, and IccDumpProfile are required"
  echo "FROMXML=$FROMXML"
  echo "TOJSON=$TOJSON"
  echo "FROMJSON=$FROMJSON"
  echo "DUMP=$DUMP"
  exit 1
fi

echo "Tools dir: $TOOLS_DIR"
echo "Testing dir: $TESTING_DIR"
echo "Output dir: $OUTDIR"
echo ""

run_observer_test \
  legacy-1931-colorimetric-observer \
  "$TESTING_DIR/Calc/CameraModel.xml" \
  "CIE 1931 standard colorimetric observer" \
  "CIE 1931 standard colorimetric observer" \
  "StdObserver: CIE 1931 (two degree) standard observer"

run_observer_test \
  legacy-1964-colorimetric-observer \
  "$TESTING_DIR/Display/LCDDisplay.xml" \
  "CIE 1964 (ten degree) standard observer" \
  "CIE 1964 standard colorimetric observer" \
  "StdObserver: CIE 1964 (ten degree) standard observer"

run_observer_json_test \
  json-legacy-1931-colorimetric-observer \
  "$TESTING_DIR/Calc/CameraModel.xml" \
  "CIE 1931 standard colorimetric observer" \
  "CIE 1931 standard colorimetric observer" \
  "CIE 1931 standard colorimetric observer" \
  "StdObserver: CIE 1931 (two degree) standard observer"

run_observer_json_test \
  json-legacy-1964-colorimetric-observer \
  "$TESTING_DIR/Display/LCDDisplay.xml" \
  "CIE 1964 (ten degree) standard observer" \
  "CIE 1964 standard colorimetric observer" \
  "CIE 1964 standard colorimetric observer" \
  "StdObserver: CIE 1964 (ten degree) standard observer"

echo ""
echo "Standard observer regression tests: $PASS/$TOTAL passed"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
