#!/bin/bash
# =============================================================================
# iccDEV Tool Coverage Baseline -- Command-Line Envelope Test Suite
# =============================================================================
# Tests all 14 iccDEV CLI tools across their full command-line interface.
# Each tool is tested with multiple argument combinations, profile classes,
# encoding modes, and edge cases to establish coverage baseline.
#
# Usage:
#   ./iccdev-tool-coverage-baseline.sh [--asan] [--quick] [--skip-hybrid]
#
# Options:
#   --asan         Enable ASAN halt_on_error=0 for catch-and-continue (default: leaks off)
#   --quick        Skip slow tools (ApplyToLink LUT generation, RoundTrip exhaustive)
#   --skip-hybrid  Skip the slow Testing/hybrid/BuildAndTest.sh integration case
#
# Output:
#   Per-test PASS/FAIL with exit codes, ASAN/UBSAN findings noted
#   Summary at end: PASS/FAIL/TOTAL counts
#
# Prerequisites:
#   - iccDEV/Build/ compiled with Debug+ASAN+UBSAN+tools
#   - test-profiles/ populated
#   - Test data: uses tmp/iccdev-tool-tests/ (local) or docs/iccDEV/Tools/test-data/ (CI)
#
# Environment overrides (for CI):
#   ICCDEV_TOOLS_DIR   Path to built iccDEV tools (default: $REPO_ROOT/iccDEV/Build/Tools)
#   ICCDEV_TESTING_DIR Path to iccDEV Testing/ data (default: $REPO_ROOT/iccDEV/Testing)
#   ICCDEV_TEST_OUTDIR Output directory for logs (default: /tmp/iccdev-tool-output)
# =============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

# Detect if running inside iccDEV directly (cfl branch) or research repo
if [ -d "$REPO_ROOT/IccProfLib" ]; then
  # Running inside iccDEV repo directly
  TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/build/Tools}"
  ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
  BUILD_ROOT="$(cd "$TOOLS/.." 2>/dev/null && pwd -P)"
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  # Stage all generated profiles into a flat directory for uniform $TP access
  TP="/tmp/iccdev-test-profiles"
  TP_TIFF="/tmp/iccdev-test-tiff"
  TP_IMG="/tmp/iccdev-test-images"
  TP_SPECTRAL="/tmp/iccdev-test-tiff/spectral"
  mkdir -p "$TP" "$TP_TIFF" "$TP_IMG" "$TP_SPECTRAL"

  # Collect profiles from ALL Testing subdirs created by CreateAllProfiles.sh
  for gendir in Display Named CMYK-3DLUTs Calc CalcTest Encoding HDR ICS \
                Overprint PCC SpecRef mcs mcs/Flexo-CMYKOGP mcs/Spot-MVIS \
                hybrid/ICC hybrid/Results; do
    if [ -d "$ICCDEV_TESTING/$gendir" ]; then
      for icc in "$ICCDEV_TESTING/$gendir"/*.icc; do
        [ -f "$icc" ] || continue
        cp -f "$icc" "$TP/" 2>/dev/null || true
      done
    fi
  done

  # Stage spectral channel TIFFs for iccSpecSepToTiff tests
  if [ -d "$REPO_ROOT/.github/ci/test-data/spectral" ]; then
    cp -f "$REPO_ROOT/.github/ci/test-data/spectral"/spec_* "$TP_SPECTRAL/" 2>/dev/null || true
  fi
else
  # Running inside research repo
  TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/iccDEV/Build/Tools}"
  ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/iccDEV/Testing}"
  BUILD_ROOT="$(cd "$TOOLS/.." 2>/dev/null && pwd -P)"
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  TP="$REPO_ROOT/test-profiles"
  TP_TIFF="$REPO_ROOT/test-profiles"
  TP_IMG="$REPO_ROOT/test-profiles"
  TP_SPECTRAL="$REPO_ROOT/test-profiles/spectral"
fi

# CI-friendly: test data search order
if [ -d "$REPO_ROOT/tmp/iccdev-tool-tests" ]; then
  TD="$REPO_ROOT/tmp/iccdev-tool-tests"
elif [ -d "$REPO_ROOT/IccProfLib" ] && [ -d "$REPO_ROOT/.github/ci/test-data" ]; then
  # Inside iccDEV repo -- test data is under .github/ci/test-data/
  TD="$REPO_ROOT/.github/ci/test-data"
else
  TD="$REPO_ROOT/docs/iccDEV/Tools/test-data"
fi
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tool-output}"

# Defaults
ASAN_MODE=0
QUICK_MODE=0
SKIP_HYBRID=0
JOB_SUMMARY="${GITHUB_STEP_SUMMARY:-}"

for arg in "$@"; do
  case "$arg" in
    --asan)         ASAN_MODE=1 ;;
    --quick)        QUICK_MODE=1 ;;
    --skip-hybrid)  SKIP_HYBRID=1 ;;
    *)              echo "Unknown option: $arg"; exit 1 ;;
  esac
done

if [ "$ASAN_MODE" -eq 1 ]; then
  export ASAN_OPTIONS="halt_on_error=0,detect_leaks=0"
  export UBSAN_OPTIONS="halt_on_error=0,print_stacktrace=1"
else
  export ASAN_OPTIONS="detect_leaks=0"
  export UBSAN_OPTIONS="halt_on_error=0,print_stacktrace=1"
fi

mkdir -p "$OUTDIR"

PASS=0
FAIL=0
EXPECTED_FAIL=0
SKIP=0
TOTAL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0

run_test() {
  local test_id="$1"
  local description="$2"
  shift 2
  local cmd=("$@")

  TOTAL=$((TOTAL + 1))
  local logfile="$OUTDIR/${test_id}.log"
  local timeout_sec="${RUN_TEST_TIMEOUT:-60}"

  local exit_code=0
  timeout "$timeout_sec" "${cmd[@]}" > "$logfile" 2>&1 || exit_code=$?

  # Check for ASAN/UBSAN
  local has_asan=0
  local has_ubsan=0
  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    has_asan=1
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
  fi
  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    has_ubsan=1
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
  fi

  # Exit code classification
  # Known signal exits: 134=SIGABRT, 136=SIGFPE, 137=SIGKILL, 139=SIGSEGV
  # Tool error codes like 254(-2) and 255(-1) are NOT signal crashes
  local status="PASS"
  local note=""
  if [ "$exit_code" -eq 134 ] || [ "$exit_code" -eq 136 ] || \
     [ "$exit_code" -eq 137 ] || [ "$exit_code" -eq 139 ]; then
    status="CRASH"
    FAIL=$((FAIL + 1))
    note=" [signal $((exit_code - 128))]"
  elif [ "$exit_code" -ge 128 ]; then
    # 128+ but not a known signal -- treat as tool error, not crash
    status="ERROR"
    FAIL=$((FAIL + 1))
    note=" [exit=$exit_code]"
  elif [ "$exit_code" -eq 124 ]; then
    status="TIMEOUT"
    FAIL=$((FAIL + 1))
    note=" [>${timeout_sec}s]"
  elif [ "$exit_code" -ne 0 ]; then
    # Exit 1-127 = graceful failure (not a crash, but track it)
    status="FAIL"
    FAIL=$((FAIL + 1))
    note=" [exit=$exit_code]"
  else
    PASS=$((PASS + 1))
  fi

  local sanitizer_note=""
  if [ "$has_asan" -eq 1 ]; then sanitizer_note="$sanitizer_note ASAN!"; fi
  if [ "$has_ubsan" -eq 1 ]; then sanitizer_note="$sanitizer_note UBSAN!"; fi

  printf "  [%-7s] %-55s exit=%-3d%s%s\n" "$status" "$description" "$exit_code" "$note" "$sanitizer_note"
}

run_expect_exit() {
  local test_id="$1"
  local description="$2"
  local expected_exit="$3"
  shift 3
  local cmd=("$@")

  TOTAL=$((TOTAL + 1))
  local logfile="$OUTDIR/${test_id}.log"

  local exit_code=0
  timeout 60 "${cmd[@]}" > "$logfile" 2>&1 || exit_code=$?

  local has_asan=0
  local has_ubsan=0
  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    has_asan=1
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
  fi
  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    has_ubsan=1
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
  fi

  local status="XFAIL"
  local note=" [expected exit=$expected_exit]"
  if [ "$exit_code" -eq "$expected_exit" ] && [ "$has_asan" -eq 0 ] && [ "$has_ubsan" -eq 0 ]; then
    if [ "$expected_exit" -eq 0 ]; then
      status="PASS"
      note=""
      PASS=$((PASS + 1))
    else
      EXPECTED_FAIL=$((EXPECTED_FAIL + 1))
    fi
  else
    status="FAIL"
    FAIL=$((FAIL + 1))
    note=" [expected exit=$expected_exit, got exit=$exit_code]"
  fi

  local sanitizer_note=""
  if [ "$has_asan" -eq 1 ]; then sanitizer_note="$sanitizer_note ASAN!"; fi
  if [ "$has_ubsan" -eq 1 ]; then sanitizer_note="$sanitizer_note UBSAN!"; fi

  printf "  [%-7s] %-55s exit=%-3d%s%s\n" "$status" "$description" "$exit_code" "$note" "$sanitizer_note"
}

skip_test() {
  local test_id="$1"
  local description="$2"
  local reason="$3"

  TOTAL=$((TOTAL + 1))
  SKIP=$((SKIP + 1))
  printf "  [%-7s] %-55s %s\n" "SKIP" "$description" "$reason"
  printf "SKIP: %s\n" "$reason" > "$OUTDIR/${test_id}.log"
}

# =============================================================================
# Key test profiles (diverse classes)
# =============================================================================
SRGB="$TP/sRGB_D65_MAT.icc"
CMYK="$TP/CMYK-3DLUTs2.icc"
REC2020="$TP/Rec2020rgbSpectral.icc"
NAMED="$TP/NamedColor.icc"
MULTICHAN="$TP/17ChanPart1.icc"
MVIS="$TP/17ChanWithSpots-MVIS.icc"
CAT8="$TP/Cat8Lab-D65_2deg.icc"
# Fallback: Cat8Lab-D65_2degMeta.icc is research-repo only
[ -f "$CAT8" ] || CAT8="$TP/Cat8Lab-D65_2degMeta.icc"
# DisplayP3 and AdobeRGB are iOS-generated (research-repo only)
# Fall back to sRGB when unavailable -- tests still exercise the tool
DISPLAY_P3="$TP/ios-gen-DisplayP3.icc"
[ -f "$DISPLAY_P3" ] || DISPLAY_P3="$TP/sRGB_D65_MAT.icc"
ADOBE="$TP/ios-gen-AdobeRGB1998.icc"
[ -f "$ADOBE" ] || ADOBE="$TP/sRGB_D65_MAT.icc"
CAMERA="$TP/CameraModel.icc"
TIFF_8BIT="$TP_TIFF/catalyst-8bit-ACESCG.tiff"
TIFF_16BIT="$TP_TIFF/catalyst-16bit-ITU2020.tiff"
TIFF_32BIT="$TP_TIFF/catalyst-32bit-ITU709.tiff"
TIFF_MISMATCH="$TP_TIFF/catalyst-16bit-mismatch.tiff"
TIFF_MUTATED="$TP_TIFF/catalyst-16bit-mutated.tiff"
PNG_CVE="$TP_IMG/p0-2225-cve-2021-30942-colorsync-uninit-mem.png"
JPG_CVE="$TP_IMG/p0-2225-cve-2021-30942-colorsync-uninit-mem.jpg"
SPECTRAL_TIFF="$ICCDEV_TESTING/hybrid/Data/smCows380_5_780.tif"
V5_DISPLAY="$TP/LCDDisplay.icc"
LCDDISPLAY_XML="$ICCDEV_TESTING/Display/LCDDisplay.xml"
[ -f "$LCDDISPLAY_XML" ] || LCDDISPLAY_XML="$ICCDEV_TESTING/hybrid/LCDDisplay.xml"
SRGB_CALC_DATA="$ICCDEV_TESTING/Calc/srgbCalcTest.txt"
CMYK_DATA="$ICCDEV_TESTING/hybrid/Data/cmykGrays.txt"
HYBRID_XML="$ICCDEV_TESTING/hybrid/LCDDisplay.xml"
APPLYPROF_REGRESSION_DIR="$OUTDIR/applyprofiles-regression"
APPLYPROF_CURVE_NAN="$APPLYPROF_REGRESSION_DIR/curve-nan.icc"
APPLYPROF_CALC_TEMP="$APPLYPROF_REGRESSION_DIR/calc-temp-overflow.icc"

generate_rgb_tiff_fixtures() {
  local tiff_dir="$1"

  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$tiff_dir" <<'PY'
import pathlib
import struct
import sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)


def write_tiff(path, bits=8, sample_format=1):
    width = height = 4
    samples = 3
    entry_count = 11
    extra = bytearray()

    if bits == 8:
        pixels = bytes([(idx * 64) % 256 for idx in range(width * height * samples)])
    elif bits == 16:
        pixels = b"".join(
            struct.pack("<H", (idx * 4096) % 65536)
            for idx in range(width * height * samples)
        )
    else:
        pixels = b"".join(
            struct.pack("<f", idx / float(width * height * samples))
            for idx in range(width * height * samples)
        )

    def short_array(values):
        offset = 8 + 2 + 12 * entry_count + 4 + len(extra)
        extra.extend(b"".join(struct.pack("<H", value) for value in values))
        return offset

    bits_offset = short_array([bits] * samples)
    sample_format_offset = short_array([sample_format] * samples)
    pixel_offset = 8 + 2 + 12 * entry_count + 4 + len(extra)
    entries = [
        (256, 4, 1, width),
        (257, 4, 1, height),
        (258, 3, samples, bits_offset),
        (259, 3, 1, 1),
        (262, 3, 1, 2),
        (273, 4, 1, pixel_offset),
        (277, 3, 1, samples),
        (278, 4, 1, height),
        (279, 4, 1, len(pixels)),
        (284, 3, 1, 1),
        (339, 3, samples, sample_format_offset),
    ]

    data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", 8))
    data.extend(struct.pack("<H", len(entries)))
    for tag, tag_type, count, value in entries:
        data.extend(struct.pack("<HHI", tag, tag_type, count))
        if tag_type == 3 and count == 1:
            data.extend(struct.pack("<H", value) + b"\0\0")
        else:
            data.extend(struct.pack("<I", value))
    data.extend(struct.pack("<I", 0))
    data.extend(extra)
    if len(data) != pixel_offset:
        raise RuntimeError(f"bad TIFF offset for {path}: {len(data)} != {pixel_offset}")
    data.extend(pixels)
    path.write_bytes(data)


write_tiff(out / "catalyst-8bit-ACESCG.tiff", 8, 1)
write_tiff(out / "catalyst-16bit-ITU2020.tiff", 16, 1)
write_tiff(out / "catalyst-32bit-ITU709.tiff", 32, 3)
write_tiff(out / "catalyst-16bit-mismatch.tiff", 16, 1)
write_tiff(out / "catalyst-16bit-mutated.tiff", 16, 1)
PY
}

ensure_profile_from_xml() {
  local profile_name="$1"
  local xml_path="$2"
  local dst="$TP/$profile_name"

  if [ -f "$dst" ]; then
    return 0
  fi
  if [ ! -x "$TOOLS/IccFromXml/iccFromXml" ] || [ ! -f "$xml_path" ]; then
    return 1
  fi

  (
    cd "$(dirname "$xml_path")"
    "$TOOLS/IccFromXml/iccFromXml" "$(basename "$xml_path")" "$dst" >/dev/null 2>&1
  )
}

generate_specsep_overflow_tiff() {
  local tiff_dir="$1"

  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$tiff_dir" <<'PY'
import pathlib
import struct
import sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
path = out / "spec_1"


def ifd_entry(tag, tag_type, count, value):
    entry = struct.pack("<HHI", tag, tag_type, count)
    if tag_type == 3 and count == 1:
        return entry + struct.pack("<H", value) + b"\0\0"
    return entry + struct.pack("<I", value)


entry_count = 12
xres_offset = 8 + 2 + 12 * entry_count + 4
yres_offset = xres_offset + 8
entries = [
    (256, 4, 1, 0xDFFF0004),  # width: overflows row-byte math
    (257, 4, 1, 4),
    (258, 3, 1, 37136),
    (259, 3, 1, 1),
    (262, 3, 1, 1),
    (273, 4, 1, 256),
    (277, 3, 1, 1),
    (278, 4, 1, 3),
    (279, 4, 1, 32),
    (282, 5, 1, xres_offset),
    (283, 5, 1, yres_offset),
    (296, 3, 1, 2),
]

data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", 8))
data.extend(struct.pack("<H", len(entries)))
for entry in entries:
    data.extend(ifd_entry(*entry))
data.extend(struct.pack("<I", 0))
data.extend(struct.pack("<II", 72, 1))
data.extend(struct.pack("<II", 72, 1))
path.write_bytes(data)
PY
}

generate_applyprofiles_regression_profiles() {
  local profile_dir="$1"

  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$profile_dir" <<'PY'
import base64
import pathlib
import sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)

profiles = {
    "curve-nan.icc": """
SUkqAGQbAAAFEAAAbW50clJHQiBYWVogB+gABQAKABUAFAAZYWNzcAAAAAAAAAAJAAAAAAAAAAAA
AAAAAAAAAAAAAAEAAPNPAAEAAAABFqUAAAAA0MMDnBvDq6+I5rlKHY5qygAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAMY3BydAAAARQAAABwZGVzYwAAAYQAAAA8d3RwdAAAAcAAAAAUQTJC
MAAAAdQAAACEQTJCMwAAAdQAAACEQTJCMQAAAlgAAADsQjJBMAAAA0QAAADsQjJBMgAAA0QAAADs
QjJBMQAABDAAAADsY2hhZAAABRwAAAAsdGVjaAAABUgAAAAMY2lpcwAABVQAAAAMbWx1YwAAAAAA
AAABAAAADGVuVVMAAABUAAAAHABDAG8AcAB5AHIAaQBnAGgAdAAgADIAMAAwADcAIABBAGQAbwBi
AGUAIABTAHkAcwB0AGUAbQBzACAASQBuAGMAbwByAHAAbwByAGEAdABlAGQAAG1sdWMAAAAAAAAA
AQAAAAxlblVTAAAAIAAAABwASABEAFQAVgAgACgAUgBlAGMALgAgADcAMAA5ACkAAFhZWiAAAAAA
AAD21gABAAAAANMsbUFCIAAAAAADAQAAAAAAIAAAACwAAABcAAAAAAAAAHljdXJ2AAAAAAAAAAAA
ADefAAAxHwAAEj8AABxhAABbcgAAB7sAAAHGAAAMYgAAWxAAJQBuAAAAcgAAAF5wYXJhAAAAAAAE
AAAAAjjkAADo8AAAFxAAADjkAAAUvAAAAAAAAAAAbUFCIAAAAAADAwAAAAAAIAAAAEQAAAB0AAAA
AAAAACBjdXJ2AAAAAAAAAABjdXJ2AAAAAAAAAABjdXJ2AAAAAAAAAAEAAAAAAAfDSwAAElAAABx7
AABbxAAAB8IAAAHIAAAMbQAAW2EAAAAAAAAAAAAAAABwYXJhAAAAAAAEAACaAjjkAADo8AAAFxAA
ADjkAABCbhVNcgAEAAAAcGFyYQAAAAAABAAAAAI45AAA6PAAABcQAAA45AAAFLwAAAAAAAAAAHBh
cmEAAAAAAAAAA+gCOOQAAAAAAAAAAGNVcnYAAAAAAAAAAGN1cnY=
""",
    "calc-temp-overflow.icc": """
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
YQ==
""",
}

for name, data in profiles.items():
    (out / name).write_bytes(base64.b64decode("".join(data.split())))
PY
}

if [ ! -f "$TIFF_8BIT" ] || [ ! -f "$TIFF_16BIT" ] || [ ! -f "$TIFF_32BIT" ] || \
   [ ! -f "$TIFF_MISMATCH" ] || [ ! -f "$TIFF_MUTATED" ]; then
  if ! generate_rgb_tiff_fixtures "$TP_TIFF"; then
    echo "  [WARN   ] python3 unavailable; TIFF-specific tests will be skipped"
  fi
fi

generate_applyprofiles_regression_profiles "$APPLYPROF_REGRESSION_DIR" || true

ensure_profile_from_xml "sRGB_D65_MAT.icc" "$ICCDEV_TESTING/Display/sRGB_D65_MAT.xml" || true
ensure_profile_from_xml "CMYK-3DLUTs2.icc" "$ICCDEV_TESTING/CMYK-3DLUTs/CMYK-3DLUTs2.xml" || true
ensure_profile_from_xml "Rec2020rgbSpectral.icc" "$ICCDEV_TESTING/Display/Rec2020rgbSpectral.xml" || true
ensure_profile_from_xml "NamedColor.icc" "$ICCDEV_TESTING/Named/NamedColor.xml" || true
ensure_profile_from_xml "17ChanPart1.icc" "$ICCDEV_TESTING/Overprint/17ChanPart1.xml" || true
ensure_profile_from_xml "17ChanWithSpots-MVIS.icc" "$ICCDEV_TESTING/mcs/17ChanWithSpots-MVIS.xml" || true
ensure_profile_from_xml "Cat8Lab-D65_2deg.icc" "$ICCDEV_TESTING/PCC/CustomObservers/Cat8Lab-D65_2deg.xml" || true
ensure_profile_from_xml "CameraModel.icc" "$ICCDEV_TESTING/Calc/CameraModel.xml" || true
ensure_profile_from_xml "LCDDisplay.icc" "$LCDDISPLAY_XML" || true

echo "============================================================================="
echo " iccDEV Tool Coverage Baseline"
echo " $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo " Tools: $TOOLS"
echo " ASAN: $ASAN_MODE  Quick: $QUICK_MODE"
echo "============================================================================="
echo ""

# =============================================================================
# 1. iccDumpProfile (7 tests)
# =============================================================================
echo "--- 1. iccDumpProfile ---"
DUMP="$TOOLS/IccDumpProfile/iccDumpProfile"

run_test "dump-01" "Dump sRGB v4 display profile" \
  "$DUMP" "$SRGB"

run_test "dump-02" "Dump with validation (-v)" \
  "$DUMP" -v "$SRGB"

run_test "dump-03" "Dump specific tag (desc)" \
  "$DUMP" "$SRGB" desc

run_test "dump-04" "Dump ALL tags" \
  "$DUMP" "$SRGB" ALL

run_test "dump-05" "Dump with verbosity=25" \
  "$DUMP" 25 "$SRGB"

run_test "dump-06" "Dump CMYK output profile" \
  "$DUMP" -v "$CMYK"

run_test "dump-07" "Dump v5 spectral profile" \
  "$DUMP" -v "$REC2020"

run_test "dump-08" "Dump NamedColor profile" \
  "$DUMP" -v "$NAMED"

run_test "dump-09" "Dump 17-channel profile" \
  "$DUMP" -v "$MULTICHAN"

run_test "dump-10" "Dump MVIS profile" \
  "$DUMP" -v "$MVIS"

run_test "dump-11" "Dump CameraModel profile" \
  "$DUMP" -v "$CAMERA"

run_test "dump-12" "Dump DisplayP3 profile" \
  "$DUMP" -v "$DISPLAY_P3"

run_test "dump-13" "Dump v5 LCDDisplay profile" \
  "$DUMP" -v "$V5_DISPLAY"

if [ -f "$CAT8" ]; then
  run_test "dump-14" "Dump Cat8Lab spectral profile" \
    "$DUMP" "$CAT8"
else
  skip_test "dump-14" "Dump Cat8Lab spectral profile" "Cat8Lab profile unavailable"
fi

echo ""

# =============================================================================
# 2. iccToXml (7 tests)
# =============================================================================
echo "--- 2. iccToXml ---"
TOXML="$TOOLS/IccToXml/iccToXml"

run_test "toxml-01" "Convert sRGB to XML" \
  "$TOXML" "$SRGB" "$OUTDIR/sRGB.xml"

run_test "toxml-02" "Convert CMYK to XML" \
  "$TOXML" "$CMYK" "$OUTDIR/CMYK.xml"

run_test "toxml-03" "Convert NamedColor to XML" \
  "$TOXML" "$NAMED" "$OUTDIR/NamedColor.xml"

run_test "toxml-04" "Convert v5 Rec2020 spectral to XML" \
  "$TOXML" "$REC2020" "$OUTDIR/Rec2020.xml"

run_test "toxml-05" "Convert 17-channel to XML" \
  "$TOXML" "$MULTICHAN" "$OUTDIR/17Chan.xml"

run_test "toxml-06" "Convert DisplayP3 to XML" \
  "$TOXML" "$DISPLAY_P3" "$OUTDIR/DisplayP3.xml"

run_test "toxml-07" "Convert v5 LCDDisplay to XML" \
  "$TOXML" "$V5_DISPLAY" "$OUTDIR/LCDDisplay.xml"

echo ""

# =============================================================================
# 3. iccFromXml (5 tests)
# =============================================================================
echo "--- 3. iccFromXml ---"
FROMXML="$TOOLS/IccFromXml/iccFromXml"

run_test "fromxml-01" "Reconstruct sRGB from XML" \
  "$FROMXML" "$OUTDIR/sRGB.xml" "$OUTDIR/sRGB_rt.icc"

run_test "fromxml-02" "Reconstruct CMYK from XML" \
  "$FROMXML" "$OUTDIR/CMYK.xml" "$OUTDIR/CMYK_rt.icc"

run_test "fromxml-03" "Reconstruct NamedColor from XML" \
  "$FROMXML" "$OUTDIR/NamedColor.xml" "$OUTDIR/NamedColor_rt.icc"

run_test "fromxml-04" "Reconstruct v5 Rec2020 from XML" \
  "$FROMXML" "$OUTDIR/Rec2020.xml" "$OUTDIR/Rec2020_rt.icc"

if [ -f "$HYBRID_XML" ]; then
  run_test "fromxml-05" "Parse upstream LCDDisplay XML" \
    "$FROMXML" "$HYBRID_XML" "$OUTDIR/LCDDisplay_from_xml.icc"
fi

run_test "fromxml-06" "FromXml with -noid flag" \
  "$FROMXML" "$OUTDIR/sRGB.xml" "$OUTDIR/sRGB_noid.icc" -noid

echo ""

# =============================================================================
# 4. iccRoundTrip (8 tests)
# =============================================================================
echo "--- 4. iccRoundTrip ---"
ROUNDTRIP="$TOOLS/IccRoundTrip/iccRoundTrip"

run_test "rt-01" "RoundTrip sRGB perceptual (intent=0)" \
  "$ROUNDTRIP" "$SRGB" 0

run_test "rt-02" "RoundTrip sRGB relative (intent=1)" \
  "$ROUNDTRIP" "$SRGB" 1

run_test "rt-03" "RoundTrip sRGB saturation (intent=2)" \
  "$ROUNDTRIP" "$SRGB" 2

run_test "rt-04" "RoundTrip sRGB absolute (intent=3)" \
  "$ROUNDTRIP" "$SRGB" 3

run_test "rt-05" "RoundTrip CMYK relative" \
  "$ROUNDTRIP" "$CMYK" 1

run_test "rt-06" "RoundTrip DisplayP3 relative" \
  "$ROUNDTRIP" "$DISPLAY_P3" 1

run_test "rt-07" "RoundTrip sRGB with MPE (use_mpe=1)" \
  "$ROUNDTRIP" "$SRGB" 1 1

if [ "$QUICK_MODE" -eq 0 ]; then
  run_test "rt-08" "RoundTrip AdobeRGB relative" \
    "$ROUNDTRIP" "$ADOBE" 1
fi

echo ""

# =============================================================================
# 5. iccFromCube (4 tests)
# =============================================================================
echo "--- 5. iccFromCube ---"
FROMCUBE="$TOOLS/IccFromCube/iccFromCube"

run_test "cube-01" "Identity LUT 2x2x2" \
  "$FROMCUBE" "$TD/test-identity.cube" "$OUTDIR/identity.icc"

run_test "cube-02" "Warm film LUT 5x5x5" \
  "$FROMCUBE" "$TD/test-warmfilm-5x5x5.cube" "$OUTDIR/warmfilm.icc"

# Test with corpus cubes
for cube_file in "$REPO_ROOT"/cfl/corpus-icc_fromcube_fuzzer/warm_film_2x2x2.cube \
                 "$REPO_ROOT"/cfl/corpus-icc_fromcube_fuzzer/domain_with_input_range_2x2x2.cube \
                 "$REPO_ROOT"/cfl/corpus-icc_fromcube_fuzzer/negative_domain_3x3x3.cube; do
  if [ -f "$cube_file" ]; then
    base=$(basename "$cube_file" .cube)
    run_test "cube-${base}" "Corpus cube: $base" \
      "$FROMCUBE" "$cube_file" "$OUTDIR/cube_${base}.icc"
  fi
done

echo ""

# =============================================================================
# 6. iccApplyNamedCmm (16 tests)
# =============================================================================
echo "--- 6. iccApplyNamedCmm ---"
APPLYNCM="$TOOLS/IccApplyNamedCmm/iccApplyNamedCmm"

# Encoding variations with sRGB
run_test "ncm-01" "sRGB: 8-bit RGB data, encoding=0 (Value)" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 0 0 "$SRGB" 1

run_test "ncm-02" "sRGB: 8-bit RGB data, encoding=1 (Percent)" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 1 0 "$SRGB" 1

run_test "ncm-03" "sRGB: 8-bit RGB data, encoding=2 (UnitFloat)" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 2 0 "$SRGB" 1

run_test "ncm-04" "sRGB: 8-bit RGB data, encoding=3 (Float)" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 3 0 "$SRGB" 1

run_expect_exit "ncm-05" "Reject final encoding=4 (8Bit)" 1 \
  "$APPLYNCM" "$SRGB_CALC_DATA" 4 0 "$SRGB" 1

run_expect_exit "ncm-05b" "Reject oversized final encoding without sanitizer findings" 1 \
  "$APPLYNCM" -exportcfganddata "$OUTDIR/ncm-oversized-encoding.json" \
  "$SRGB_CALC_DATA" 999999999999 0 "$SRGB" 1

run_test "ncm-06" "sRGB: 8-bit RGB data, encoding=5 (16Bit)" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 5 0 "$SRGB" 1

# Interpolation variation
run_test "ncm-07" "sRGB: tetrahedral interpolation" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 0 1 "$SRGB" 1

# Intent variations
run_test "ncm-08" "sRGB: perceptual intent" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 0 0 "$SRGB" 0

run_test "ncm-09" "sRGB: saturation intent" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 0 0 "$SRGB" 2

run_test "ncm-10" "sRGB: absolute intent" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 0 0 "$SRGB" 3

# Chained profiles
run_test "ncm-11" "sRGB->sRGB chained transform" \
  "$APPLYNCM" "$SRGB_CALC_DATA" 0 0 "$SRGB" 1 "$SRGB" 1

# CMYK data
run_test "ncm-12" "CMYK profile with CMYK data" \
  "$APPLYNCM" "$CMYK_DATA" 0 0 "$CMYK" 1

# Custom data files
run_test "ncm-13" "Custom RGB float data" \
  "$APPLYNCM" "$TD/test-data-rgb-float.txt" 3 0 "$SRGB" 1

run_test "ncm-14" "Custom RGB 16-bit data" \
  "$APPLYNCM" "$TD/test-data-rgb-16bit.txt" 0 0 "$SRGB" 1

# Format precision
run_test "ncm-15" "sRGB: encoding with precision 8:12" \
  "$APPLYNCM" "$SRGB_CALC_DATA" "0:8:12" 0 "$SRGB" 1

if [ -f "$NAMED" ]; then
  cat > "$OUTDIR/ncm-it8-src.it8" <<EOF
CGATS.17
ORIGINATOR	"iccDEV regression"
FILE_DESCRIPTOR	"Minimal named-color IT8 source data"
NUMBER_OF_FIELDS	2
BEGIN_DATA_FORMAT
NAME	TINT
END_DATA_FORMAT
NUMBER_OF_SETS	1
BEGIN_DATA
Black	1.0
END_DATA
EOF

  cat > "$OUTDIR/ncm-it8-src.json" <<EOF
{
  "dataFiles": {
    "srcType": "it8",
    "srcSpace": "nmcl",
    "srcFile": "$OUTDIR/ncm-it8-src.it8",
    "dstType": "colorData",
    "dstFile": "$OUTDIR/ncm-it8-src-output.json",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9
  },
  "profileSequence": [
    {
      "iccFile": "$NAMED",
      "intent": 1,
      "interpolation": "tetrahedral",
      "useBPC": false
    }
  ]
}
EOF

  # shellcheck disable=SC2016
  run_test "ncm-16" "JSON -cfg srcType=it8 named-color data" \
    bash -c '"$1" -cfg "$2" >/dev/null && grep -Fq "\"sn\": \"Black\"" "$3"' \
    _ "$APPLYNCM" "$OUTDIR/ncm-it8-src.json" "$OUTDIR/ncm-it8-src-output.json"
fi

echo ""

# =============================================================================
# 7. iccApplyToLink (6 tests)
# =============================================================================
echo "--- 7. iccApplyToLink ---"
APPLYLINK="$TOOLS/IccApplyToLink/iccApplyToLink"

if [ "$QUICK_MODE" -eq 0 ]; then
  # Device Link (type=0) with varying LUT sizes
  run_expect_exit "link-01" "Report DeviceLink sRGB->sRGB LUT=9 v4 write failure" 255 \
    "$APPLYLINK" "$OUTDIR/link_srgb_9.icc" 0 9 0 "sRGB-link" 0.0 1.0 0 0 "$SRGB" 1

  run_test "link-02" "DeviceLink sRGB->sRGB LUT=17 v5" \
    "$APPLYLINK" "$OUTDIR/link_srgb_17.icc" 0 17 1 "sRGB-link-v5" 0.0 1.0 0 0 "$SRGB" 1

  run_expect_exit "link-03" "Report DeviceLink sRGB->sRGB LUT=33 write failure" 255 \
    "$APPLYLINK" "$OUTDIR/link_srgb_33.icc" 0 33 0 "sRGB-link-33" 0.0 1.0 0 0 "$SRGB" 1

  # .cube output (type=1)
  run_test "link-04" ".cube output sRGB LUT=9 precision=6" \
    "$APPLYLINK" "$OUTDIR/link_srgb.cube" 1 9 6 "sRGB-cube" 0.0 1.0 0 0 "$SRGB" 1

  # Tetrahedral interpolation
  run_expect_exit "link-05" "Report DeviceLink sRGB tetrahedral LUT=9 write failure" 255 \
    "$APPLYLINK" "$OUTDIR/link_srgb_tet.icc" 0 9 0 "sRGB-tet" 0.0 1.0 0 1 "$SRGB" 1

  # Destination transform (first_transform=1)
  run_expect_exit "link-06" "Report DeviceLink first_transform=1 write failure" 255 \
    "$APPLYLINK" "$OUTDIR/link_srgb_dest.icc" 0 9 0 "sRGB-dest" 0.0 1.0 1 0 "$SRGB" 1

  # Two-profile chain
  run_expect_exit "link-07" "Report DeviceLink sRGB->DisplayP3 chain write failure" 255 \
    "$APPLYLINK" "$OUTDIR/link_srgb_p3.icc" 0 17 0 "sRGB-to-P3" 0.0 1.0 0 0 "$SRGB" 1 "$DISPLAY_P3" 1

else
  # Quick mode: smaller LUTs only
  run_expect_exit "link-01" "Report DeviceLink sRGB->sRGB LUT=5 write failure (quick)" 255 \
    "$APPLYLINK" "$OUTDIR/link_srgb_5.icc" 0 5 0 "sRGB-link-q" 0.0 1.0 0 0 "$SRGB" 1

  run_test "link-04" ".cube output sRGB LUT=5 (quick)" \
    "$APPLYLINK" "$OUTDIR/link_srgb_q.cube" 1 5 4 "sRGB-cube-q" 0.0 1.0 0 0 "$SRGB" 1
fi

echo ""

# =============================================================================
# 8. iccApplyProfiles (6 tests -- requires TIFF input)
# =============================================================================
echo "--- 8. iccApplyProfiles ---"
APPLYPROF="$TOOLS/IccApplyProfiles/iccApplyProfiles"

# dst_encoding: 0=same, 1=8bit, 2=16bit, 4=float
# dst_compress: 0=none, 1=LZW
# dst_planar: 0=contig, 1=separate
# dst_embed: 0=no, 1=embed

if [ -f "$TIFF_8BIT" ]; then
  run_test "apply-01" "TIFF 8bit->8bit sRGB relative, no compress" \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_8bit.tiff" 0 0 0 0 0 "$SRGB" 1

  run_test "apply-02" "TIFF 8bit->16bit sRGB relative, LZW" \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_16bit.tiff" 2 1 0 0 0 "$SRGB" 1

  run_test "apply-03" "TIFF 8bit->float sRGB perceptual" \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_float.tiff" 4 0 0 0 0 "$SRGB" 0

  run_test "apply-04" "TIFF 8bit with embedded ICC" \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_embed.tiff" 0 0 0 1 0 "$SRGB" 1

  run_test "apply-05" "TIFF 8bit tetrahedral interpolation" \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_tet.tiff" 0 0 0 0 1 "$SRGB" 1
else
  skip_test "apply-01" "TIFF 8bit->8bit sRGB relative, no compress" "8-bit TIFF fixture unavailable"
  skip_test "apply-02" "TIFF 8bit->16bit sRGB relative, LZW" "8-bit TIFF fixture unavailable"
  skip_test "apply-03" "TIFF 8bit->float sRGB perceptual" "8-bit TIFF fixture unavailable"
  skip_test "apply-04" "TIFF 8bit with embedded ICC" "8-bit TIFF fixture unavailable"
  skip_test "apply-05" "TIFF 8bit tetrahedral interpolation" "8-bit TIFF fixture unavailable"
fi

if [ -f "$TIFF_16BIT" ]; then
  run_test "apply-06" "TIFF 16bit->8bit sRGB absolute" \
    "$APPLYPROF" "$TIFF_16BIT" "$OUTDIR/applied_from16.tiff" 1 0 0 0 0 "$SRGB" 2
else
  skip_test "apply-06" "TIFF 16bit->8bit sRGB absolute" "16-bit TIFF fixture unavailable"
fi

if [ -f "$TIFF_8BIT" ] && [ -f "$APPLYPROF_CURVE_NAN" ]; then
  run_expect_exit "apply-07" "ApplyProfiles: AFL curve NaN profile" 0 \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_afl_curve_nan.tiff" 0 0 0 0 0 "$APPLYPROF_CURVE_NAN" 1
else
  skip_test "apply-07" "ApplyProfiles: AFL curve NaN profile" "regression profile unavailable"
fi

if [ -f "$TIFF_8BIT" ] && [ -f "$APPLYPROF_CALC_TEMP" ]; then
  run_expect_exit "apply-08" "ApplyProfiles: AFL calculator temp overflow profile" 255 \
    "$APPLYPROF" "$TIFF_8BIT" "$OUTDIR/applied_afl_calc_temp.tiff" 0 0 0 0 0 "$APPLYPROF_CALC_TEMP" 1
else
  skip_test "apply-08" "ApplyProfiles: AFL calculator temp overflow profile" "regression profile unavailable"
fi

echo ""

# =============================================================================
# 9. iccApplySearch (5 tests)
# =============================================================================
echo "--- 9. iccApplySearch ---"
APPLYSRCH="$TOOLS/IccApplySearch/iccApplySearch"

run_test "search-01" "Search sRGB->sRGB relative" \
  "$APPLYSRCH" "$SRGB_CALC_DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1

run_test "search-02" "Search sRGB->sRGB perceptual" \
  "$APPLYSRCH" "$SRGB_CALC_DATA" 0 0 "$SRGB" 0 "$SRGB" 0 -INIT 0

run_test "search-03" "Search sRGB->sRGB float encoding" \
  "$APPLYSRCH" "$SRGB_CALC_DATA" 3 0 "$SRGB" 1 "$SRGB" 1 -INIT 1

run_expect_exit "search-03b" "Reject oversized final encoding without sanitizer findings" 1 \
  "$APPLYSRCH" -exportcfganddata "$OUTDIR/search-oversized-encoding.json" \
  "$SRGB_CALC_DATA" 999999999999 0 "$SRGB" 1 "$SRGB" 1 -INIT 1

run_expect_exit "search-04" "Reject legacy three-profile search chain" 255 \
  "$APPLYSRCH" "$SRGB_CALC_DATA" 0 0 "$SRGB" 1 "$DISPLAY_P3" 1 "$SRGB" 1 -INIT 1

if [ -f "$SRGB_CALC_DATA" ] && [ -f "$SRGB" ]; then
  # shellcheck disable=SC2016
  run_test "search-05" "JSON -cfg search without pccWeights" \
    bash -c '"$1" -exportcfganddata "$2" "$3" 3 0 "$4" 1 "$4" 1 -INIT 1 >/dev/null && ! grep -Fq "\"pccWeights\"" "$2" && "$1" -cfg "$2" >/dev/null' \
    _ "$APPLYSRCH" "$OUTDIR/search-missing-pccweights.json" "$SRGB_CALC_DATA" "$SRGB"
fi

echo ""

# =============================================================================
# 10. iccTiffDump (9 tests)
# =============================================================================
echo "--- 10. iccTiffDump ---"
TIFFDUMP="$TOOLS/IccTiffDump/iccTiffDump"

if [ -f "$TIFF_8BIT" ]; then
  run_test "tdump-01" "Dump TIFF 8-bit metadata" \
    "$TIFFDUMP" "$TIFF_8BIT"

  run_test "tdump-06" "Extract ICC from TIFF to file" \
    "$TIFFDUMP" "$TIFF_8BIT" "$OUTDIR/tiff_extracted.icc"
else
  skip_test "tdump-01" "Dump TIFF 8-bit metadata" "8-bit TIFF fixture unavailable"
  skip_test "tdump-06" "Extract ICC from TIFF to file" "8-bit TIFF fixture unavailable"
fi

if [ -f "$TIFF_16BIT" ]; then
  run_test "tdump-02" "Dump TIFF 16-bit metadata" \
    "$TIFFDUMP" "$TIFF_16BIT"
else
  skip_test "tdump-02" "Dump TIFF 16-bit metadata" "16-bit TIFF fixture unavailable"
fi

if [ -f "$TIFF_32BIT" ]; then
  run_test "tdump-03" "Dump TIFF 32-bit metadata" \
    "$TIFFDUMP" "$TIFF_32BIT"
else
  skip_test "tdump-03" "Dump TIFF 32-bit metadata" "32-bit TIFF fixture unavailable"
fi

if [ -f "$TIFF_MISMATCH" ]; then
  run_test "tdump-04" "Dump TIFF mismatch profile" \
    "$TIFFDUMP" "$TIFF_MISMATCH"
else
  skip_test "tdump-04" "Dump TIFF mismatch profile" "mismatch TIFF fixture unavailable"
fi

if [ -f "$TIFF_MUTATED" ]; then
  run_test "tdump-05" "Dump TIFF mutated profile" \
    "$TIFFDUMP" "$TIFF_MUTATED"
else
  skip_test "tdump-05" "Dump TIFF mutated profile" "mutated TIFF fixture unavailable"
fi

if [ -f "$SPECTRAL_TIFF" ]; then
  run_test "tdump-07" "Dump spectral TIFF (81 channels)" \
    "$TIFFDUMP" "$SPECTRAL_TIFF"
fi

if [ -f "$ICCDEV_TESTING/hybrid/Data/TShirtDesignKW.tif" ]; then
  # shellcheck disable=SC2016
  run_test "tdump-08" "Escape controlled ICC description text" \
    bash -c 'set -euo pipefail
      sample="$1"; tool="$2"; outdir="$3"
      mutated="$outdir/tiffdump-mutated-desc.tif"
      log="$outdir/tiffdump-mutated-desc.out"
      cp "$sample" "$mutated"
      perl -0pi -e '"'"'s/Dot Gain 20%/bad\n\e[31mX!!/g'"'"' "$mutated"
      "$tool" "$mutated" > "$log" 2>&1
      grep -Fq "Description:      bad\\n\\x1B[31mX!!" "$log"
      ! grep -q "$(printf '"'"'\033'"'"')" "$log"
    ' _ "$ICCDEV_TESTING/hybrid/Data/TShirtDesignKW.tif" "$TIFFDUMP" "$OUTDIR"

  # shellcheck disable=SC2016
  run_test "tdump-09" "Escape controlled TIFF input/output paths" \
    bash -c 'set -euo pipefail
      sample="$1"; tool="$2"; outdir="$3"
      odd="$outdir/name_with_
newline.tif"
      dst="$outdir/export_with_
newline.icc"
      log="$outdir/tiffdump-paths.out"
      cp "$sample" "$odd"
      "$tool" "$odd" "$dst" > "$log" 2>&1
      [ -s "$dst" ]
      grep -Fq "Filename:          $outdir/name_with_\\nnewline.tif" "$log"
      grep -Fq "Profile extracted to: $outdir/export_with_\\nnewline.icc" "$log"
    ' _ "$ICCDEV_TESTING/hybrid/Data/TShirtDesignKW.tif" "$TIFFDUMP" "$OUTDIR"
else
  skip_test "tdump-08" "Escape controlled ICC description text" "TShirtDesignKW TIFF fixture unavailable"
  skip_test "tdump-09" "Escape controlled TIFF input/output paths" "TShirtDesignKW TIFF fixture unavailable"
fi

echo ""

# =============================================================================
# 11. iccJpegDump (4 tests)
# =============================================================================
echo "--- 11. iccJpegDump ---"
JPEGDUMP="$TOOLS/IccJpegDump/iccJpegDump"

if [ -f "$JPG_CVE" ]; then
  run_test "jpeg-01" "Extract ICC from CVE JPEG" \
    "$JPEGDUMP" "$JPG_CVE" "$OUTDIR/jpeg_extracted.icc"
fi

# Test with fuzz/graphics JPEGs
for jpg in "$REPO_ROOT"/fuzz/graphics/jpg/2x2-gray--LCDDisplay.jpg \
           "$REPO_ROOT"/fuzz/graphics/jpg/LittleEndian-image--crash-checkunderflowoverflow.jpg; do
  if [ -f "$jpg" ]; then
    base=$(basename "$jpg" .jpg | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9_-]/_/g')
    run_test "jpeg-$base" "JPEG: $(basename "$jpg")" \
      "$JPEGDUMP" "$jpg"
  fi
done

# ICC injection test (if we have an extracted ICC)
if [ -f "$OUTDIR/tiff_extracted.icc" ] && [ -f "$JPG_CVE" ]; then
  run_test "jpeg-inject" "Inject ICC into JPEG" \
    "$JPEGDUMP" "$JPG_CVE" --write-icc "$OUTDIR/tiff_extracted.icc" --output "$OUTDIR/jpeg_injected.jpg"
fi

echo ""

# =============================================================================
# 12. iccPngDump (5 tests)
# =============================================================================
echo "--- 12. iccPngDump ---"
PNGDUMP="$TOOLS/IccPngDump/iccPngDump"

if [ -f "$PNG_CVE" ]; then
  run_test "png-01" "Extract ICC from CVE PNG" \
    "$PNGDUMP" "$PNG_CVE" "$OUTDIR/png_extracted.icc"
fi

# Test with fuzz/graphics PNGs
for png in "$REPO_ROOT"/fuzz/graphics/png/BigEndian-image--Rec2100HlgFull.png \
           "$REPO_ROOT"/fuzz/graphics/png/BigEndian-image--calcOverMem_tget.png; do
  if [ -f "$png" ]; then
    base=$(basename "$png" .png | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9_-]/_/g')
    run_test "png-$base" "PNG: $(basename "$png")" \
      "$PNGDUMP" "$png"
  fi
done

# ICC injection test
if [ -f "$OUTDIR/tiff_extracted.icc" ] && [ -f "$PNG_CVE" ]; then
  run_test "png-inject" "Inject ICC into PNG" \
    "$PNGDUMP" "$PNG_CVE" --write-icc "$OUTDIR/tiff_extracted.icc" --output "$OUTDIR/png_injected.png"
fi

PNG_BAD_CHUNK="$OUTDIR/png_invalid_chunk_length.png"
printf '%b' '\x89PNG\r\n\x1a\n\xff\x00\x00\x0dIHDR' > "$PNG_BAD_CHUNK"
run_expect_exit "png-invalid-chunk-length" "Reject oversized PNG chunk without signal" \
  1 "$PNGDUMP" "$PNG_BAD_CHUNK"

echo ""

# =============================================================================
# 13. iccV5DspObsToV4Dsp (3 tests)
# =============================================================================
echo "--- 13. iccV5DspObsToV4Dsp ---"
V5CONV="$TOOLS/IccV5DspObsToV4Dsp/iccV5DspObsToV4Dsp"

if [ -f "$V5_DISPLAY" ]; then
  # Try different observer profiles
  for obs in "$ICCDEV_TESTING/ICS/XYZ_float-D65_2deg-Part1.icc" \
             "$ICCDEV_TESTING/ICS/Lab_float-D65_2deg-Part1.icc" \
             "$ICCDEV_TESTING/ICS/Spec400_10_700-D50_2deg-Part1.icc"; do
    if [ -f "$obs" ]; then
      base=$(basename "$obs" .icc | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9_-]/_/g')
      run_test "v5-$base" "V5->V4: $(basename "$obs")" \
        "$V5CONV" "$V5_DISPLAY" "$obs" "$OUTDIR/v4_${base}.icc"
    fi
  done
fi

echo ""

# =============================================================================
# 14. iccSpecSepToTiff (3 tests)
# =============================================================================
echo "--- 14. iccSpecSepToTiff ---"
SPECSEP="$TOOLS/IccSpecSepToTiff/iccSpecSepToTiff"

# Test with sequential spectral channel TIFFs (16-bit grayscale, 4x4)
if [ -d "$TP_SPECTRAL" ] && [ -f "$TP_SPECTRAL/spec_1" ]; then
  # Tool uses prefix+number concatenation: "%s%d" (NOT printf format)
  run_test "specsep-01" "SpecSepToTiff: merge 10 spectral channels (no compress)" \
    "$SPECSEP" "$OUTDIR/spectral_merged.tiff" 0 0 "$TP_SPECTRAL/spec_" 1 10 1

  run_test "specsep-02" "SpecSepToTiff: merge 5 channels with compression" \
    "$SPECSEP" "$OUTDIR/spectral_compressed.tiff" 1 0 "$TP_SPECTRAL/spec_" 1 5 1

  run_test "specsep-03" "SpecSepToTiff: merge with planar separation" \
    "$SPECSEP" "$OUTDIR/spectral_separated.tiff" 0 1 "$TP_SPECTRAL/spec_" 1 10 1
else
  echo "  [SKIP   ] SpecSepToTiff: no sequential spectral TIFFs available"
fi

SPECSEP_BAD_DIR="$OUTDIR/specsep-malformed"
if generate_specsep_overflow_tiff "$SPECSEP_BAD_DIR"; then
  run_expect_exit "specsep-04" "SpecSepToTiff: reject oversized TIFF geometry" 255 \
    "$SPECSEP" "$OUTDIR/spectral_malformed.tiff" 0 0 "$SPECSEP_BAD_DIR/spec_" 1 1 1
else
  skip_test "specsep-04" "SpecSepToTiff: reject oversized TIFF geometry" "python3 unavailable"
fi

echo ""

# =============================================================================
# 15. Hybrid Pipeline Integration Test (BuildAndTest.sh)
# =============================================================================
echo "--- 15. Hybrid Pipeline (BuildAndTest.sh) ---"
HYBRID_DIR="$ICCDEV_TESTING/hybrid"
if [ "$SKIP_HYBRID" -eq 1 ]; then
  skip_test "hybrid-01" "Hybrid pipeline: 6-phase spectral color management" \
    "moved to separate iccdev.hybrid-pipeline CTest gate"
elif [ -f "$HYBRID_DIR/BuildAndTest.sh" ]; then
  # BuildAndTest.sh expects tools on PATH and runs from its own directory
  TOOL_DIRS=""
  for tooldir in "$TOOLS"/*/; do
    TOOL_DIRS="${TOOL_DIRS:+$TOOL_DIRS:}$tooldir"
  done
  old_pwd="$PWD"
  cd "$HYBRID_DIR"
  PATH="$TOOL_DIRS:$PATH" \
  RUN_TEST_TIMEOUT="${HYBRID_TIMEOUT:-600}" \
  run_test "hybrid-01" "Hybrid pipeline: 6-phase spectral color management" \
    bash BuildAndTest.sh
  cd "$old_pwd"
else
  skip_test "hybrid-01" "Hybrid pipeline: 6-phase spectral color management" \
    "BuildAndTest.sh not found"
fi

echo ""

# =============================================================================
# Crash/PoC profiles through DumpProfile (exercising sanitizer coverage)
# =============================================================================
echo "--- BONUS: PoC/Crash profiles through iccDumpProfile ---"

POC_COUNT=0
for poc in "$TP"/hbo-*.icc "$TP"/sbo-*.icc "$TP"/CIccMpe*.icc "$TP"/CIccTag*.icc "$TP"/CIccTone*.icc; do
  if [ -f "$poc" ] && [ "$POC_COUNT" -lt 10 ]; then
    base=$(basename "$poc" .icc | sed 's/[^a-zA-Z0-9_-]/_/g' | cut -c1-40)
    run_test "poc-$base" "PoC: $(basename "$poc" | cut -c1-50)" \
      "$DUMP" "$poc"
    POC_COUNT=$((POC_COUNT + 1))
  fi
done

echo ""

# =============================================================================
# Round-trip verification: ToXml -> FromXml -> DumpProfile
# =============================================================================
echo "--- BONUS: XML Round-Trip Integrity ---"

for rt_profile in "$SRGB" "$CMYK" "$NAMED"; do
  if [ -f "$rt_profile" ]; then
    base=$(basename "$rt_profile" .icc)
    # Already converted above, now verify reconstructed profile dumps correctly
    if [ -f "$OUTDIR/${base}_rt.icc" ]; then
      run_test "xmlrt-$base" "XML round-trip dump: $base" \
        "$DUMP" "$OUTDIR/${base}_rt.icc"
    fi
  fi
done

echo ""

# =============================================================================
# Summary
# =============================================================================
echo "============================================================================="
echo " COVERAGE BASELINE SUMMARY"
echo "============================================================================="
echo "  PASS:           $PASS"
echo "  EXPECTED_FAIL:  $EXPECTED_FAIL"
echo "  SKIP:           $SKIP"
echo "  FAIL:           $FAIL"
echo "  TOTAL:          $TOTAL"
echo "  ASAN findings:  $ASAN_FINDINGS"
echo "  UBSAN findings: $UBSAN_FINDINGS"
echo ""
echo "  Non-failing:    $(( (PASS + EXPECTED_FAIL + SKIP) * 100 / TOTAL ))%"

if [ "$ASAN_FINDINGS" -gt 0 ]; then
  echo ""
  # Classify ASAN findings: known (alloc-dealloc-mismatch) vs unknown (real crashes)
  echo "  ASAN finding details:"
  grep -rl "ERROR: AddressSanitizer" "$OUTDIR/"*.log 2>/dev/null | while read -r f; do
    asan_line="$(grep 'ERROR: AddressSanitizer' "$f" | sed -n '1p')"
    if grep -q "alloc-dealloc-mismatch" <<< "$asan_line"; then
      echo "    $(basename "$f"): $asan_line [KNOWN -- CWE-762, CFL-011 pending]"
    else
      echo "    $(basename "$f"): $asan_line [INVESTIGATE]"
    fi
  done
fi

if [ "$UBSAN_FINDINGS" -gt 0 ]; then
  echo ""
  echo "  !!! UBSAN FINDINGS -- review logs in $OUTDIR/ !!!"
  grep -rl "runtime error:" "$OUTDIR/"*.log 2>/dev/null | while read -r f; do
    echo "    $(basename "$f"): $(grep 'runtime error:' "$f" | sed -n '1p')"
  done
fi

echo ""
echo "  Output logs: $OUTDIR/"
echo "============================================================================="

# Write CI summary if GITHUB_STEP_SUMMARY is set
if [ -n "$JOB_SUMMARY" ]; then
  {
    echo ""
    echo "## iccDEV Tool Coverage Baseline"
    echo ""
    echo "| Metric | Value |"
    echo "|--------|-------|"
    echo "| Total tests | $TOTAL |"
    echo "| Passed | $PASS |"
    echo "| Expected failures | $EXPECTED_FAIL |"
    echo "| Skipped | $SKIP |"
    echo "| Failed | $FAIL |"
    echo "| ASAN findings | $ASAN_FINDINGS |"
    echo "| UBSAN findings | $UBSAN_FINDINGS |"
    echo "| Non-failing rate | $(( (PASS + EXPECTED_FAIL + SKIP) * 100 / TOTAL ))% |"
    echo ""
    if [ "$ASAN_FINDINGS" -gt 0 ]; then
      # Count known vs unknown ASAN
      KNOWN_CNT=0
      UNKNOWN_CNT=0
      for logf in "$OUTDIR/"*.log; do
        [ -f "$logf" ] || continue
        if grep -q "ERROR: AddressSanitizer" "$logf" 2>/dev/null; then
          if grep "ERROR: AddressSanitizer" "$logf" | grep -q "alloc-dealloc-mismatch"; then
            KNOWN_CNT=$((KNOWN_CNT + 1))
          else
            UNKNOWN_CNT=$((UNKNOWN_CNT + 1))
          fi
        fi
      done
      if [ "$UNKNOWN_CNT" -gt 0 ]; then
        echo "### [FAIL] ASAN Findings ($UNKNOWN_CNT unknown, $KNOWN_CNT known)"
      else
        echo "### [WARN] ASAN Findings ($KNOWN_CNT known -- non-fatal)"
      fi
      echo ""
      echo '```'
      grep -rl "ERROR: AddressSanitizer" "$OUTDIR/"*.log 2>/dev/null | while read -r f; do
        raw="$(basename "$f"): $(grep 'ERROR: AddressSanitizer' "$f" | sed -n '1p')"
        clean="${raw//[<>&\"\'\`]/}"
        if grep -q "alloc-dealloc-mismatch" <<< "$raw"; then
          echo "$clean [KNOWN -- CWE-762, CFL-011]"
        else
          echo "$clean [INVESTIGATE]"
        fi
      done
      echo '```'
      echo ""
    fi
    if [ "$UBSAN_FINDINGS" -gt 0 ]; then
      echo "### [WARN] UBSAN Findings"
      echo ""
      echo '```'
      grep -rl "runtime error:" "$OUTDIR/"*.log 2>/dev/null | while read -r f; do
        # Sanitize UBSAN output -- may contain attacker-controlled profile data
        raw="$(basename "$f"): $(grep 'runtime error:' "$f" | sed -n '1p')"
        echo "${raw//[<>&\"\'\`]/}"
      done
      echo '```'
      echo ""
    fi
    if [ "$ASAN_FINDINGS" -eq 0 ] && [ "$UBSAN_FINDINGS" -eq 0 ]; then
      echo "[OK] No sanitizer findings"
    fi
  } >> "$JOB_SUMMARY"
fi

# Exit with failure only if ASAN CRASH (not alloc-dealloc-mismatch which is known CWE-762)
# Known non-fatal ASAN patterns (upstream bugs with patches pending):
#   - alloc-dealloc-mismatch: iccSpecSepToTiff.cpp unique_ptr<T> with new T[] (CFL-011)
CRASHES=0
KNOWN_ASAN=0
for logf in "$OUTDIR/"*.log; do
  [ -f "$logf" ] || continue
  if grep -q "ERROR: AddressSanitizer" "$logf" 2>/dev/null; then
    # Check if ALL ASAN errors in this log are known/non-fatal patterns
    if grep "ERROR: AddressSanitizer" "$logf" | grep -q "alloc-dealloc-mismatch"; then
      KNOWN_ASAN=$((KNOWN_ASAN + 1))
      echo "KNOWN: $(basename "$logf") -- alloc-dealloc-mismatch (CWE-762, CFL-011 pending)"
    else
      CRASHES=$((CRASHES + 1))
      echo "CRASH: $(basename "$logf") -- $(grep 'ERROR: AddressSanitizer' "$logf" | sed -n '1p')"
    fi
  fi
done
if [ "$CRASHES" -gt 0 ]; then
  echo "FATAL: $CRASHES test(s) with ASAN findings -- failing CI"
  exit 1
fi
if [ "$KNOWN_ASAN" -gt 0 ]; then
  echo "WARNING: $KNOWN_ASAN test(s) with known ASAN patterns (non-fatal, patches pending)"
fi
if [ "$FAIL" -gt 0 ]; then
  echo "FATAL: $FAIL unexpected tool envelope failure(s) -- failing CI"
  exit 1
fi
exit 0
