#!/usr/bin/env bash
###############################################################################
# iccPngDump issue-2854 metadata, limits, extraction, and CLI regressions
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2854-pngdump-regressions}"
PNGDUMP="$TOOLS_DIR/IccPngDump/iccPngDump"
INPUT_PNG="$OUTDIR/metadata.png"
INPUT_ICC="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
OUTPUT_PNG="$OUTDIR/metadata-with-icc.png"
EXTRACTED_ICC="$OUTDIR/extracted.icc"
LOGFILE="$OUTDIR/issue-2854-pngdump-regressions.log"

mkdir -p "$OUTDIR"

fail() {
  echo "  [FAIL] issue-2854-pngdump-regressions -- $1"
  exit 1
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1"; then
    sed -n '1,100p' "$1"
    fail "sanitizer diagnostic"
  fi
}

if [ ! -x "$PNGDUMP" ]; then
  fail "missing executable: $PNGDUMP"
fi
if [ ! -s "$INPUT_ICC" ]; then
  fail "missing input profile: $INPUT_ICC"
fi
if ! command -v python3 >/dev/null 2>&1; then
  fail "python3 is required to generate deterministic PNG fixtures"
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

python3 - "$INPUT_PNG" "$OUTDIR/oversized-dimension.png" "$OUTDIR/oversized-raster.png" <<'PY'
import binascii
import pathlib
import struct
import sys
import zlib

def chunk(kind, payload):
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", binascii.crc32(kind + payload) & 0xffffffff))

def png(width, height, chunks):
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunks +
            chunk(b"IEND", b""))

width = height = 2
image_data = b"\x00" + bytes([10, 20, 30] * width)
image_data += b"\x00" + bytes([40, 50, 60] * width)
metadata = [
    chunk(b"cHRM", struct.pack(">8I", 31270, 32900, 64000, 33000,
                               30000, 60000, 15000, 6000)),
    chunk(b"gAMA", struct.pack(">I", 45455)),
    chunk(b"sBIT", bytes([8, 8, 8])),
    chunk(b"sRGB", b"\x00"),
    chunk(b"bKGD", struct.pack(">HHH", 100, 200, 30)),
    chunk(b"tRNS", struct.pack(">HHH", 255, 128, 0)),
    chunk(b"pHYs", struct.pack(">IIB", 3780, 3780, 1)),
    chunk(b"eXIf", b"II*\x00\x08\x00\x00\x00\x00\x00\x00\x00"),
    chunk(b"tEXt", b"Comment\x00metadata-preservation"),
    chunk(b"iTXt", b"XML:com.adobe.xmp\x00\x00\x00\x00\x00<x:xmpmeta/>"),
    chunk(b"vpAg", b"before-image"),
]
trailing = [
    chunk(b"tIME", struct.pack(">HBBBBB", 2026, 10, 18, 19, 0, 0)),
    chunk(b"iTXt", b"After\x00\x00\x00\x00\x00post-image text"),
    chunk(b"vpAg", b"after-image"),
]
pathlib.Path(sys.argv[1]).write_bytes(
    png(width, height, b"".join(metadata) + chunk(b"IDAT", zlib.compress(image_data)) +
        b"".join(trailing)))

pathlib.Path(sys.argv[2]).write_bytes(
    png(100001, 1, chunk(b"IDAT", zlib.compress(b""))))
pathlib.Path(sys.argv[3]).write_bytes(
    png(20000, 20000, chunk(b"IDAT", zlib.compress(b""))))

PY

rm -f "$OUTPUT_PNG" "$EXTRACTED_ICC" "$LOGFILE"
timeout 30 "$PNGDUMP" "$INPUT_PNG" --write-icc "$INPUT_ICC" \
  --output "$OUTPUT_PNG" >"$LOGFILE" 2>&1 || fail "metadata injection failed"
check_sanitizers "$LOGFILE"

python3 - "$INPUT_PNG" "$OUTPUT_PNG" <<'PY'
import collections
import pathlib
import struct
import sys

def chunks(path):
    data = pathlib.Path(path).read_bytes()
    pos = 8
    found = []
    while pos + 12 <= len(data):
        size = struct.unpack_from(">I", data, pos)[0]
        kind = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + size]
        if len(payload) != size:
            raise SystemExit(f"truncated chunk in {path}")
        found.append((kind, payload))
        pos += size + 12
        if kind == b"IEND":
            break
    return found

source = collections.Counter(
    (kind, payload) for kind, payload in chunks(sys.argv[1])
    if kind not in (b"IDAT", b"IEND", b"iCCP"))
output_chunks = chunks(sys.argv[2])
output = collections.Counter(
    (kind, payload) for kind, payload in output_chunks
    if kind not in (b"IDAT", b"IEND", b"iCCP"))
if source != output:
    missing = source - output
    extra = output - source
    raise SystemExit(f"ancillary chunks changed or lost: missing={missing}, extra={extra}")
if not any(kind == b"iCCP" for kind, _ in output_chunks):
    raise SystemExit("injected PNG has no iCCP chunk")
PY

timeout 30 "$PNGDUMP" "$OUTPUT_PNG" "$EXTRACTED_ICC" >>"$LOGFILE" 2>&1
check_sanitizers "$LOGFILE"
cmp -s "$INPUT_ICC" "$EXTRACTED_ICC" || fail "injected ICC did not round-trip"

python3 - "$OUTDIR/oversized.icc" <<'PY'
import pathlib
import sys
with pathlib.Path(sys.argv[1]).open("wb") as oversized:
    oversized.truncate(67108865)
PY
if timeout 30 "$PNGDUMP" "$INPUT_PNG" --write-icc "$OUTDIR/oversized.icc" \
    --output "$OUTDIR/oversized-profile.png" >"$LOGFILE" 2>&1; then
  fail "ICC profile above the size cap was accepted"
fi
check_sanitizers "$LOGFILE"

for png_path in "$OUTDIR/oversized-dimension.png" "$OUTDIR/oversized-raster.png"; do
  if timeout 30 "$PNGDUMP" "$png_path" --write-icc "$INPUT_ICC" \
      --output "$OUTDIR/rejected.png" >"$LOGFILE" 2>&1; then
    fail "resource-limit PNG was accepted: $png_path"
  fi
  check_sanitizers "$LOGFILE"
done

if timeout 30 "$PNGDUMP" "$INPUT_PNG" "$OUTDIR/out.icc" unexpected \
    >"$LOGFILE" 2>&1; then
  fail "surplus positional argument was accepted"
fi
check_sanitizers "$LOGFILE"

if timeout 30 "$PNGDUMP" "$INPUT_PNG" --write-icc >"$LOGFILE" 2>&1; then
  fail "missing --write-icc value was accepted"
fi
check_sanitizers "$LOGFILE"

if timeout 30 "$PNGDUMP" "$INPUT_PNG" --output >"$LOGFILE" 2>&1; then
  fail "missing --output value was accepted"
fi
check_sanitizers "$LOGFILE"

if timeout 30 "$PNGDUMP" "$INPUT_PNG" --unknown >"$LOGFILE" 2>&1; then
  fail "unknown option was accepted"
fi
check_sanitizers "$LOGFILE"

printf 'not a PNG\n' > "$OUTDIR/invalid-input.png"
printf 'preserve existing output\n' > "$OUTDIR/preserved-output.png"
cp "$OUTDIR/preserved-output.png" "$OUTDIR/preserved-output.expected"
if timeout 30 "$PNGDUMP" "$OUTDIR/invalid-input.png" --write-icc "$INPUT_ICC" \
    --output "$OUTDIR/preserved-output.png" >"$LOGFILE" 2>&1; then
  fail "invalid PNG was accepted for injection"
fi
check_sanitizers "$LOGFILE"
if ! cmp -s "$OUTDIR/preserved-output.expected" "$OUTDIR/preserved-output.png"; then
  fail "invalid PNG injection modified the existing output"
fi

echo "  [PASS] issue-2854-pngdump-regressions -- metadata, resource bounds, raw export, and CLI"
