#!/bin/bash
###############################################################################
# iccDEV multiLocalizedUnicodeType read and UTF-16 validation regressions
###############################################################################
#
# Issue:
#   Follow-up to https://github.com/InternationalColorConsortium/iccDEV/issues/928
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-mluc-read-utf16-regressions}"
mkdir -p "$OUTDIR"

DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
TOXML="$TOOLS_DIR/IccToXml/iccToXml"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"
BASE_PROFILE="$TESTING_DIR/Display/sRGB_D65_MAT.icc"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

pass_case() {
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

check_tools() {
  local missing=0

  for tool in "$DUMP" "$TOXML" "$FROMXML" "$TOJSON" "$FROMJSON"; do
    if [ ! -x "$tool" ]; then
      echo "  [FAIL] missing tool: $tool"
      missing=1
    fi
  done

  if [ ! -f "$BASE_PROFILE" ]; then
    echo "  [FAIL] missing base profile: $BASE_PROFILE"
    missing=1
  fi

  return "$missing"
}

make_mluc_case() {
  local mode="$1"
  local dst="$2"

  cp "$BASE_PROFILE" "$dst"
  python3 - "$dst" "$mode" <<'PY'
import struct
import sys

path, mode = sys.argv[1], sys.argv[2]
data = bytearray(open(path, "rb").read())
tag_count = struct.unpack_from(">I", data, 128)[0]

for idx in range(tag_count):
    entry = 132 + idx * 12
    sig, off, size = struct.unpack_from(">4sII", data, entry)
    if sig != b"desc":
        continue

    if data[off:off + 4] != b"mluc":
        raise SystemExit("desc tag is not mluc")

    rec = off + 16
    first_len = struct.unpack_from(">I", data, rec + 4)[0]
    first_off = struct.unpack_from(">I", data, rec + 8)[0]

    if mode == "odd-length":
        struct.pack_into(">I", data, rec + 4, first_len + 1)
    elif mode == "header-overlap-offset":
        struct.pack_into(">I", data, rec + 8, 0)
    elif mode == "record-overlap-offset":
        struct.pack_into(">I", data, rec + 8, 16)
    elif mode == "unpaired-high-surrogate":
        struct.pack_into(">I", data, rec + 4, 2)
        struct.pack_into(">H", data, off + first_off, 0xD800)
    elif mode == "two-byte-utf8":
        struct.pack_into(">I", data, rec + 4, 2)
        struct.pack_into(">H", data, off + first_off, 0x00E9)
    elif mode == "three-byte-utf8":
        struct.pack_into(">I", data, rec + 4, 2)
        struct.pack_into(">H", data, off + first_off, 0x20AC)
    elif mode == "four-byte-utf8":
        struct.pack_into(">I", data, rec + 4, 4)
        struct.pack_into(">H", data, off + first_off, 0xD83D)
        struct.pack_into(">H", data, off + first_off + 2, 0xDE00)
    elif mode == "embedded-nul":
        struct.pack_into(">I", data, rec + 4, 6)
        struct.pack_into(">H", data, off + first_off, 0x0041)
        struct.pack_into(">H", data, off + first_off + 2, 0x0000)
        struct.pack_into(">H", data, off + first_off + 4, 0x0042)
    else:
        raise SystemExit(f"unknown mode: {mode}")

    with open(path, "wb") as f:
        f.write(data)
    print(f"{mode}: desc_offset={off} desc_size={size} first_len=0x{first_len:x} first_offset=0x{first_off:x}")
    break
else:
    raise SystemExit("desc tag not found")
PY
}

check_mluc_payload() {
  local icc="$1"
  local expected_hex="$2"

  python3 - "$icc" "$expected_hex" <<'PY'
import binascii
import struct
import sys

path, expected_hex = sys.argv[1], sys.argv[2]
expected = binascii.unhexlify(expected_hex)
data = open(path, "rb").read()
tag_count = struct.unpack_from(">I", data, 128)[0]

for idx in range(tag_count):
    entry = 132 + idx * 12
    sig, off, size = struct.unpack_from(">4sII", data, entry)
    if sig != b"desc":
        continue
    if data[off:off + 4] != b"mluc":
        raise SystemExit("desc tag is not mluc")
    rec = off + 16
    first_len = struct.unpack_from(">I", data, rec + 4)[0]
    first_off = struct.unpack_from(">I", data, rec + 8)[0]
    payload = data[off + first_off:off + first_off + first_len]
    if payload != expected:
        raise SystemExit(
            f"mluc payload {payload.hex()} != expected {expected.hex()}"
        )
    break
else:
    raise SystemExit("desc tag not found")
PY
}

json_has_text_hex() {
  local json="$1"
  local expected="$2"

  python3 - "$json" "$expected" <<'PY'
import json
import sys

path, expected = sys.argv[1], sys.argv[2]

def walk(value):
    if isinstance(value, dict):
        if value.get("textHex") == expected:
            return True
        return any(walk(v) for v in value.values())
    if isinstance(value, list):
        return any(walk(v) for v in value)
    return False

with open(path, "r", encoding="utf-8") as f:
    data = json.load(f)

if not walk(data):
    raise SystemExit(f"textHex {expected} not found")
PY
}

run_expect_preserve_embedded_nul() {
  local name="mluc-embedded-nul-roundtrip"
  local icc="$OUTDIR/$name.icc"
  local xml="$OUTDIR/$name.xml"
  local json="$OUTDIR/$name.json"
  local xml_icc="$OUTDIR/$name-from-xml.icc"
  local json_icc="$OUTDIR/$name-from-json.icc"
  local meta="$OUTDIR/$name.meta"
  local log="$OUTDIR/$name.log"

  TOTAL=$((TOTAL + 1))

  if ! make_mluc_case "embedded-nul" "$icc" > "$meta" 2>&1; then
    fail_case "$name" "failed to create valid embedded-NUL mluc profile"
    cat "$meta"
    return 1
  fi

  if ! check_mluc_payload "$icc" "004100000042" > "$log" 2>&1; then
    fail_case "$name" "source mluc payload mismatch"
    cat "$log"
    return 1
  fi

  if ! "$TOXML" "$icc" "$xml" >> "$log" 2>&1; then
    fail_case "$name" "iccToXml rejected valid embedded-NUL mluc"
    sed -n '1,80p' "$log"
    return 1
  fi

  if ! grep -q "<HexTextData>" "$xml" || ! grep -q "410042" "$xml"; then
    fail_case "$name" "XML did not preserve embedded NUL as HexTextData"
    sed -n '1,80p' "$xml"
    return 1
  fi

  if ! "$FROMXML" "$xml" "$xml_icc" >> "$log" 2>&1 ||
     ! check_mluc_payload "$xml_icc" "004100000042" >> "$log" 2>&1; then
    fail_case "$name" "iccFromXml did not round-trip embedded-NUL mluc"
    sed -n '1,80p' "$log"
    return 1
  fi

  if ! "$TOJSON" "$icc" "$json" >> "$log" 2>&1 ||
     ! json_has_text_hex "$json" "410042" >> "$log" 2>&1; then
    fail_case "$name" "iccToJson did not preserve embedded NUL as textHex"
    sed -n '1,80p' "$log"
    return 1
  fi

  if ! "$FROMJSON" "$json" "$json_icc" >> "$log" 2>&1 ||
     ! check_mluc_payload "$json_icc" "004100000042" >> "$log" 2>&1; then
    fail_case "$name" "iccFromJson did not round-trip embedded-NUL mluc"
    sed -n '1,80p' "$log"
    return 1
  fi

  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding during embedded-NUL round-trip"
    sed -n '1,80p' "$log"
    return 1
  fi

  pass_case "$name" "embedded-NUL mluc preserved through XML and JSON round-trips"
  return 0
}

run_expect_reject_invalid_text_hex() {
  local name="mluc-invalid-utf8-hex-input"
  local icc="$OUTDIR/$name.icc"
  local xml="$OUTDIR/$name.xml"
  local json="$OUTDIR/$name.json"
  local bad_xml="$OUTDIR/$name-bad.xml"
  local bad_json="$OUTDIR/$name-bad.json"
  local xml_icc="$OUTDIR/$name-from-xml.icc"
  local json_icc="$OUTDIR/$name-from-json.icc"
  local meta="$OUTDIR/$name.meta"
  local log="$OUTDIR/$name.log"
  local xml_ec=0
  local json_ec=0

  TOTAL=$((TOTAL + 1))

  rm -f "$xml_icc" "$json_icc" "$log"

  if ! make_mluc_case "embedded-nul" "$icc" > "$meta" 2>&1 ||
     ! "$TOXML" "$icc" "$xml" > "$log" 2>&1 ||
     ! "$TOJSON" "$icc" "$json" >> "$log" 2>&1; then
    fail_case "$name" "failed to create valid hex-encoded XML/JSON fixtures"
    cat "$meta"
    sed -n '1,80p' "$log"
    return 1
  fi

  python3 - "$xml" "$bad_xml" "$json" "$bad_json" <<'PY'
import json
import re
import sys

xml_in, xml_out, json_in, json_out = sys.argv[1:5]

xml = open(xml_in, "r", encoding="utf-8").read()
xml, count = re.subn(r"(<HexTextData>)[0-9A-Fa-f]*(</HexTextData>)", r"\1ff\2", xml, count=1)
if count != 1:
    raise SystemExit("HexTextData fixture not found")
open(xml_out, "w", encoding="utf-8").write(xml)

with open(json_in, "r", encoding="utf-8") as f:
    data = json.load(f)

def replace_text_hex(value):
    if isinstance(value, dict):
        if "textHex" in value:
            value["textHex"] = "ff"
            return True
        return any(replace_text_hex(v) for v in value.values())
    if isinstance(value, list):
        return any(replace_text_hex(v) for v in value)
    return False

if not replace_text_hex(data):
    raise SystemExit("textHex fixture not found")

with open(json_out, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2)
    f.write("\n")
PY

  "$FROMXML" "$bad_xml" "$xml_icc" >> "$log" 2>&1
  xml_ec=$?
  "$FROMJSON" "$bad_json" "$json_icc" >> "$log" 2>&1
  json_ec=$?

  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding while rejecting invalid UTF-8 hex"
    sed -n '1,80p' "$log"
    return 1
  fi

  if [ "$xml_ec" -eq 0 ] || [ -s "$xml_icc" ]; then
    fail_case "$name" "iccFromXml accepted invalid UTF-8 HexTextData (exit=$xml_ec)"
    sed -n '1,80p' "$log"
    return 1
  fi

  if [ "$json_ec" -eq 0 ] || [ -s "$json_icc" ]; then
    fail_case "$name" "iccFromJson accepted invalid UTF-8 textHex (exit=$json_ec)"
    sed -n '1,80p' "$log"
    return 1
  fi

  pass_case "$name" "invalid UTF-8 HexTextData/textHex rejected without sanitizer findings"
  return 0
}

run_expect_reject() {
  local name="$1"
  local mode="$2"
  local icc="$OUTDIR/$name.icc"
  local meta="$OUTDIR/$name.meta"
  local dump_log="$OUTDIR/$name.dump.log"
  local xml_log="$OUTDIR/$name.toxml.log"
  local dump_ec=0
  local xml_ec=0

  TOTAL=$((TOTAL + 1))

  if ! make_mluc_case "$mode" "$icc" > "$meta" 2>&1; then
    fail_case "$name" "failed to create malformed profile"
    cat "$meta"
    return 1
  fi

  "$DUMP" "$icc" ALL > "$dump_log" 2>&1
  dump_ec=$?
  "$TOXML" "$icc" "$OUTDIR/$name.xml" > "$xml_log" 2>&1
  xml_ec=$?

  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$dump_log" "$xml_log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding while rejecting malformed mluc"
    cat "$meta"
    sed -n '1,40p' "$dump_log"
    sed -n '1,40p' "$xml_log"
    return 1
  fi

  if ! grep -q "Tag ('desc' = 64657363) not found in profile" "$dump_log"; then
    fail_case "$name" "malformed desc mluc was decoded by iccDumpProfile"
    cat "$meta"
    sed -n '1,40p' "$dump_log"
    return 1
  fi

  if [ "$xml_ec" -eq 0 ]; then
    fail_case "$name" "malformed mluc accepted by iccToXml (dump=$dump_ec toxml=$xml_ec)"
    cat "$meta"
    sed -n '1,30p' "$dump_log"
    sed -n '1,30p' "$xml_log"
    return 1
  fi

  pass_case "$name" "malformed desc mluc not decoded; iccToXml rejected (dump=$dump_ec toxml=$xml_ec)"
  return 0
}

run_expect_decode() {
  local name="$1"
  local mode="$2"
  local icc="$OUTDIR/$name.icc"
  local meta="$OUTDIR/$name.meta"
  local dump_log="$OUTDIR/$name.dump.log"
  local dump_ec=0

  TOTAL=$((TOTAL + 1))

  if ! make_mluc_case "$mode" "$icc" > "$meta" 2>&1; then
    fail_case "$name" "failed to create valid mluc profile"
    cat "$meta"
    return 1
  fi

  "$DUMP" "$icc" ALL > "$dump_log" 2>&1
  dump_ec=$?

  if grep -q "ERROR: AddressSanitizer\\|runtime error:" "$dump_log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding while decoding valid non-ASCII mluc"
    cat "$meta"
    sed -n '1,40p' "$dump_log"
    return 1
  fi

  if [ "$dump_ec" -eq 124 ]; then
    fail_case "$name" "iccDumpProfile timed out"
    return 1
  fi

  if [ "$dump_ec" -eq 134 ] || [ "$dump_ec" -eq 136 ] || [ "$dump_ec" -eq 139 ]; then
    fail_case "$name" "iccDumpProfile crashed with signal $((dump_ec - 128))"
    sed -n '1,40p' "$dump_log"
    return 1
  fi

  if grep -q "Tag ('desc' = 64657363) not found in profile" "$dump_log"; then
    fail_case "$name" "valid desc mluc was not decoded"
    cat "$meta"
    sed -n '1,40p' "$dump_log"
    return 1
  fi

  pass_case "$name" "valid non-ASCII desc mluc decoded without sanitizer findings (dump=$dump_ec)"
  return 0
}

echo "=== multiLocalizedUnicodeType read/UTF-16 validation regression ==="

if ! check_tools; then
  exit 1
fi

run_expect_reject "mluc-odd-record-length" "odd-length"
run_expect_reject "mluc-header-overlap-offset" "header-overlap-offset"
run_expect_reject "mluc-record-overlap-offset" "record-overlap-offset"
run_expect_reject "mluc-unpaired-high-surrogate" "unpaired-high-surrogate"
run_expect_decode "mluc-two-byte-utf8" "two-byte-utf8"
run_expect_decode "mluc-three-byte-utf8" "three-byte-utf8"
run_expect_decode "mluc-four-byte-utf8" "four-byte-utf8"
run_expect_preserve_embedded_nul
run_expect_reject_invalid_text_hex

echo "mluc read/UTF-16 validation regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
