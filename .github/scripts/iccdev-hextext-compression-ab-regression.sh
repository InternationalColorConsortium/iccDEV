#!/bin/bash
###############################################################################
# HexTextData and zlib-compressed utf8ZipType XML import A/B regression
###############################################################################
#
# Generates the HexCompressedData and HexTextData strings used by XML profile
# fixtures, then verifies XML import behavior with ICC_USE_ZLIB enabled or
# disabled.  The HexTextData path must work in both builds; utf8ZipType
# compressed import must work only when zlib support is compiled in.
#
# Usage:
#   iccdev-hextext-compression-ab-regression.sh [ON|OFF]
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for generated XML/ICC/log files
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-hextext-compression-ab}"
EXPECT_ZLIB="${1:-${ICCDEV_EXPECT_ZLIB:-AUTO}}"
mkdir -p "$OUTDIR"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
TOXML="$TOOLS_DIR/IccToXml/iccToXml"
DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1,print_stacktrace=1}"

PASS=0
FAIL=0
TOTAL=0

fail_case()
{
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

pass_case()
{
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

check_sanitizers()
{
  local name="$1"
  local logfile="$2"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in $logfile"
    sed -n '1,80p' "$logfile"
    return 1
  fi

  return 0
}

find_base_xml()
{
  local candidate
  for candidate in \
    "$TESTING_DIR/ICS/Spec400_10_700-D50_2deg-Part1.xml" \
    "$TESTING_DIR/HDR/BT2100HlgFullScene.xml" \
    "$TESTING_DIR/ICS/Rec2100HlgFull-Part2.xml"; do
    if [ -f "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  find "$TESTING_DIR" -name '*.xml' -size +100c -print 2>/dev/null | sed -n '1p'
}

generate_xml_cases()
{
  local base_xml="$1"
  local compressed_xml="$2"
  local hextext_xml="$3"
  local manifest="$4"

  python3 - "$base_xml" "$compressed_xml" "$hextext_xml" "$manifest" <<'PY'
import pathlib
import re
import sys
import zlib

base_path, compressed_path, hextext_path, manifest_path = map(pathlib.Path, sys.argv[1:5])
text = base_path.read_text(encoding="utf-8")

compressed_payload = b"\x1b[31mPOC\x1b[0m"
compressed_hex = zlib.compress(compressed_payload).hex()
expected_compressed = "789c938e3636cc0df077968e36c8050013fb033d"
if compressed_hex != expected_compressed:
    raise SystemExit(f"compressed hex drift: {compressed_hex}")

copyright_payload = b"Copyright 2017 International Color Consortium\x00"
copyright_hex = copyright_payload.hex()
expected_copyright = (
    "436f70797269676874203230313720496e7465726e6174696f6e616c20436f6c"
    "6f7220436f6e736f727469756d00"
)
if copyright_hex != expected_copyright:
    raise SystemExit(f"copyright hex drift: {copyright_hex}")

desc_re = re.compile(r"<profileDescriptionTag>.*?</profileDescriptionTag>", re.S)
cprt_re = re.compile(r"<copyrightTag>.*?</copyrightTag>", re.S)

desc_zip = (
    "<profileDescriptionTag> <utf8ZipType>\n"
    "      <HexCompressedData>\n"
    f"       {compressed_hex}\n"
    "      </HexCompressedData>\n"
    "    </utf8ZipType> </profileDescriptionTag>"
)
cprt_hex = (
    "<copyrightTag> <multiLocalizedUnicodeType>\n"
    "      <LocalizedText LanguageCountry=\"enUS\">\n"
    "        <HexTextData>\n"
    f"         {copyright_hex[:64]}\n"
    f"         {copyright_hex[64:]}\n"
    "        </HexTextData>\n"
    "      </LocalizedText>\n"
    "    </multiLocalizedUnicodeType> </copyrightTag>"
)

if not desc_re.search(text):
    raise SystemExit("profileDescriptionTag not found")
if not cprt_re.search(text):
    raise SystemExit("copyrightTag not found")

compressed_doc = desc_re.sub(desc_zip, text, count=1)
compressed_doc = cprt_re.sub(cprt_hex, compressed_doc, count=1)
hextext_doc = cprt_re.sub(cprt_hex, text, count=1)

compressed_path.write_text(compressed_doc, encoding="ascii")
hextext_path.write_text(hextext_doc, encoding="ascii")
manifest_path.write_text(
    "\n".join([
        f"compressed_hex={compressed_hex}",
        f"compressed_payload_hex={compressed_payload.hex()}",
        "compressed_payload_label=ESC[31mPOCESC[0m",
        f"copyright_hex={copyright_hex}",
        "copyright_text=Copyright 2017 International Color Consortium\\0",
        "",
    ]),
    encoding="ascii",
)
PY
}

validate_hextext_nodes()
{
  local xml_file="$1"
  local expected_hex="$2"

  python3 - "$xml_file" "$expected_hex" <<'PY'
import pathlib
import re
import sys

xml_path = pathlib.Path(sys.argv[1])
expected = sys.argv[2].lower()
text = xml_path.read_text(encoding="utf-8")
nodes = re.findall(r"<HexTextData>(.*?)</HexTextData>", text, re.S)
if not nodes:
    raise SystemExit("no HexTextData nodes found")

matched = False
for node in nodes:
    compact = "".join(node.split()).lower()
    if not compact or len(compact) % 2:
        raise SystemExit(f"invalid HexTextData length: {compact!r}")
    try:
        bytes.fromhex(compact)
    except ValueError as exc:
        raise SystemExit(f"invalid HexTextData hex: {compact!r}") from exc
    if compact == expected:
        matched = True

if not matched:
    raise SystemExit("generated copyright HexTextData was not found")
PY
}

run_hextext_only_import()
{
  local name="hextext-only-import"
  local xml="$OUTDIR/hextext-only.xml"
  local icc="$OUTDIR/hextext-only.icc"
  local roundtrip_xml="$OUTDIR/hextext-only-roundtrip.xml"
  local log="$OUTDIR/hextext-only.log"
  local expected_hex
  expected_hex="$(sed -n 's/^copyright_hex=//p' "$OUTDIR/manifest.txt")"

  TOTAL=$((TOTAL + 1))
  rm -f "$icc" "$roundtrip_xml" "$log"

  if ! "$FROMXML" "$xml" "$icc" > "$log" 2>&1; then
    check_sanitizers "$name" "$log" || true
    fail_case "$name" "HexTextData-only XML did not import"
    sed -n '1,80p' "$log"
    return
  fi

  if ! "$TOXML" "$icc" "$roundtrip_xml" >> "$log" 2>&1 ||
     ! validate_hextext_nodes "$roundtrip_xml" "$expected_hex" >> "$log" 2>&1; then
    check_sanitizers "$name" "$log" || true
    fail_case "$name" "HexTextData was not preserved through XML round-trip"
    sed -n '1,80p' "$log"
    return
  fi

  if ! check_sanitizers "$name" "$log"; then
    return
  fi

  pass_case "$name" "HexTextData import and round-trip works independent of zlib"
}

run_compressed_import()
{
  local expect_zlib="$1"
  local name="ziputf8-hexcompresseddata-import-${expect_zlib}"
  local xml="$OUTDIR/compressed-and-hextext.xml"
  local icc="$OUTDIR/compressed-and-hextext.icc"
  local roundtrip_xml="$OUTDIR/compressed-and-hextext-roundtrip.xml"
  local dump_log="$OUTDIR/compressed-and-hextext.dump.log"
  local log="$OUTDIR/compressed-and-hextext.log"
  local expected_hex
  local exit_code=0
  expected_hex="$(sed -n 's/^copyright_hex=//p' "$OUTDIR/manifest.txt")"

  TOTAL=$((TOTAL + 1))
  rm -f "$icc" "$roundtrip_xml" "$dump_log" "$log"

  # Bounded: the guard below no longer keys off the status, so without a timeout
  # a post-write hang would wedge the run rather than fail this case.
  timeout 60 "$FROMXML" "$xml" "$icc" > "$log" 2>&1 || exit_code=$?
  check_sanitizers "$name" "$log" || return

  # Status still matters for the outcomes it is the ONLY witness to.  Relaxing
  # the guard below to the artifact must not also stop this case noticing that
  # iccFromXml wrote the profile and then died in teardown, or hung: a SIGSEGV
  # in a tag destructor leaves a complete .icc on disk and, on the ordinary
  # uninstrumented lanes, no sanitizer text for check_sanitizers to
  # find -- so an artifact-only assertion would report PASS for a crash.  The
  # #2384 suite excludes the same range for the same reason.
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "iccFromXml timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "iccFromXml died on signal $((exit_code - 128))"
    sed -n '1,40p' "$log"
    return
  fi

  if [ "$expect_zlib" = "ON" ]; then
    # The artifact, not the status.  This fixture REPLACES profileDescriptionTag
    # with a utf8ZipType carrying HexCompressedData -- the thing under test --
    # and Validate() calls that "profileDescriptionTag utf8ZipType: Invalid tag
    # type", so the profile is non-conformant BY CONSTRUCTION and cannot be made
    # otherwise without deleting what this case exists to measure.  Since #2384
    # iccFromXml exits non-zero for such a profile while still writing it, so
    # "the zlib build imported the stream" is the file plus the absence of a
    # parse error, exactly as the #1898/#1845 fixtures already assert it.
    if [ ! -s "$icc" ] || grep -q "Unable to Parse" "$log"; then
      fail_case "$name" "zlib-enabled build rejected generated HexCompressedData (exit=$exit_code)"
      sed -n '1,80p' "$log"
      return
    fi

    if ! "$DUMP" "$icc" ALL > "$dump_log" 2>&1 ||
       ! "$TOXML" "$icc" "$roundtrip_xml" >> "$log" 2>&1 ||
       ! validate_hextext_nodes "$roundtrip_xml" "$expected_hex" >> "$log" 2>&1; then
      check_sanitizers "$name" "$dump_log" || true
      check_sanitizers "$name" "$log" || true
      fail_case "$name" "zlib-enabled round-trip did not preserve generated hex data"
      sed -n '1,80p' "$log"
      sed -n '1,80p' "$dump_log"
      return
    fi

    if ! grep -F -q "ZLib Compressed String" "$dump_log"; then
      fail_case "$name" "dump did not describe zlib-compressed utf8 text"
      sed -n '1,80p' "$dump_log"
      return
    fi

    check_sanitizers "$name" "$dump_log" || return
    pass_case "$name" "zlib-enabled build imports generated HexCompressedData"
    return
  fi

  if [ "$exit_code" -eq 0 ] || [ -s "$icc" ]; then
    fail_case "$name" "zlib-disabled build accepted compressed utf8ZipType XML"
    sed -n '1,80p' "$log"
    return
  fi

  if ! grep -Eq "Invalid HexCompressedData zlib stream|Unable to Parse" "$log"; then
    fail_case "$name" "zlib-disabled failure did not include expected diagnostic"
    sed -n '1,80p' "$log"
    return
  fi

  pass_case "$name" "zlib-disabled build rejects compressed utf8ZipType while leaving HexTextData path testable"
}

if [ "$EXPECT_ZLIB" != "ON" ] && [ "$EXPECT_ZLIB" != "OFF" ]; then
  echo "ERROR: expected zlib mode must be ON or OFF"
  exit 2
fi

for tool in "$FROMXML" "$TOXML" "$DUMP"; do
  if [ ! -x "$tool" ]; then
    echo "ERROR: required tool missing: $tool"
    exit 2
  fi
done

BASE_XML="$(find_base_xml)"
if [ -z "$BASE_XML" ] || [ ! -f "$BASE_XML" ]; then
  echo "ERROR: no XML profile fixture found under $TESTING_DIR"
  exit 2
fi

generate_xml_cases "$BASE_XML" \
  "$OUTDIR/compressed-and-hextext.xml" \
  "$OUTDIR/hextext-only.xml" \
  "$OUTDIR/manifest.txt"

if ! validate_hextext_nodes "$OUTDIR/compressed-and-hextext.xml" \
     "$(sed -n 's/^copyright_hex=//p' "$OUTDIR/manifest.txt")"; then
  echo "ERROR: generated XML did not contain valid HexTextData"
  exit 2
fi

echo "=== HexTextData / HexCompressedData A/B regression (${EXPECT_ZLIB}) ==="
echo "Base XML: $BASE_XML"
sed -n '1,4p' "$OUTDIR/manifest.txt"
run_hextext_only_import
run_compressed_import "$EXPECT_ZLIB"
echo "HexTextData / HexCompressedData A/B regression: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
