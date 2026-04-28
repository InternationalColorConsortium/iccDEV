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

  for tool in "$DUMP" "$TOXML"; do
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

echo "=== multiLocalizedUnicodeType read/UTF-16 validation regression ==="

if ! check_tools; then
  exit 1
fi

run_expect_reject "mluc-odd-record-length" "odd-length"
run_expect_reject "mluc-header-overlap-offset" "header-overlap-offset"
run_expect_reject "mluc-record-overlap-offset" "record-overlap-offset"
run_expect_reject "mluc-unpaired-high-surrogate" "unpaired-high-surrogate"

echo "mluc read/UTF-16 validation regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
