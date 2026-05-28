#!/bin/bash
###############################################################################
# iccDEV multiLocalizedUnicodeType setter regression tests
###############################################################################
#
# Issue:
#   https://github.com/InternationalColorConsortium/iccDEV/issues/928
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-mluc-setter-regressions}"
mkdir -p "$OUTDIR"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
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

check_mluc_desc() {
  local icc="$1"
  local expected_desc_size="$2"
  local expected_first_len="$3"

  python3 - "$icc" "$expected_desc_size" "$expected_first_len" <<'PY'
import struct
import sys
from pathlib import Path

icc_path = Path(sys.argv[1])
expected_desc_size = int(sys.argv[2], 0)
expected_first_len = int(sys.argv[3], 0)

data = icc_path.read_bytes()
if len(data) < 132:
    raise SystemExit("profile too small for ICC tag table")

tag_count = struct.unpack(">I", data[128:132])[0]
for index in range(tag_count):
    entry = 132 + index * 12
    if entry + 12 > len(data):
        raise SystemExit("tag table entry exceeds file size")
    signature = data[entry:entry + 4]
    offset, size = struct.unpack(">II", data[entry + 4:entry + 12])
    if signature != b"desc":
        continue
    if offset + size > len(data):
        raise SystemExit("desc tag exceeds file size")
    tag_type = data[offset:offset + 4]
    if tag_type != b"mluc":
        raise SystemExit(f"desc tag type {tag_type!r} is not mluc")
    if size != expected_desc_size:
        raise SystemExit(f"desc size 0x{size:x} != expected 0x{expected_desc_size:x}")
    if size < 28:
        raise SystemExit("mluc desc tag too small for first record")
    records = struct.unpack(">I", data[offset + 8:offset + 12])[0]
    record_size = struct.unpack(">I", data[offset + 12:offset + 16])[0]
    first_len = struct.unpack(">I", data[offset + 20:offset + 24])[0]
    if records < 1:
        raise SystemExit("mluc desc tag has no records")
    if record_size != 12:
        raise SystemExit(f"mluc record size {record_size} != expected 12")
    if first_len != expected_first_len:
        raise SystemExit(f"first mluc length 0x{first_len:x} != expected 0x{expected_first_len:x}")
    print(
        f"file={len(data)} desc_size=0x{size:x} "
        f"records={records} first_record_len=0x{first_len:x}"
    )
    sys.exit(0)

raise SystemExit("desc tag not found")
PY
}

run_mluc_case() {
  local name="$1"
  local xml_rel="$2"
  local expected_desc_size="$3"
  local expected_first_len="$4"
  local source_xml="$TESTING_DIR/$xml_rel"
  local xml_base
  local xml_dir
  local case_icc="$OUTDIR/${name}.icc"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local dump_log="$OUTDIR/${name}-dump.log"
  local inspect_log="$OUTDIR/${name}-inspect.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$case_icc" "$fromxml_log" "$dump_log" "$inspect_log"

  if [ ! -f "$source_xml" ]; then
    echo "  [FAIL] $name -- missing source XML: $source_xml"
    FAIL=$((FAIL + 1))
    return
  fi

  xml_base="$(basename "$source_xml")"
  xml_dir="$(dirname "$source_xml")"

  (cd "$xml_dir" && timeout 30 "$FROMXML" "$xml_base" "$case_icc") > "$fromxml_log" 2>&1 || exit_code=$?
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
  timeout 30 "$DUMP" "$case_icc" desc > "$dump_log" 2>&1 || exit_code=$?
  if ! check_log "$name/dump" "$dump_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    echo "  [FAIL] $name -- iccDumpProfile desc failed with exit=$exit_code"
    sed -n '1,10p' "$dump_log"
    FAIL=$((FAIL + 1))
    return
  fi

  if check_mluc_desc "$case_icc" "$expected_desc_size" "$expected_first_len" > "$inspect_log" 2>&1; then
    echo "  [PASS] $name -- $(cat "$inspect_log")"
    PASS=$((PASS + 1))
  else
    echo "  [FAIL] $name -- mluc desc record mismatch"
    sed -n '1,10p' "$inspect_log"
    FAIL=$((FAIL + 1))
  fi
}

echo "=== Issue #928: multiLocalizedUnicodeType setter regression ==="

if [ ! -x "$FROMXML" ] || [ ! -x "$DUMP" ]; then
  echo "ERROR: IccFromXml and IccDumpProfile are required"
  exit 1
fi

run_mluc_case "issue-928-srgb-desc-mluc" "Display/sRGB_D65_MAT.xml" 0x34 0x18
run_mluc_case "issue-928-namedcolor-desc-mluc" "Named/NamedColor.xml" 0x54 0x38

echo "Issue #928 mluc setter regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

exit 0
