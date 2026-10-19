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

python3 - "$INPUT_PNG" "$OUTDIR/oversized-dimension.png" "$OUTDIR/oversized-raster.png" "$OUTDIR/unknown-critical.png" <<'PY'
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
pathlib.Path(sys.argv[4]).write_bytes(
    png(width, height, chunk(b"ABCD", b"required image information") +
        chunk(b"IDAT", zlib.compress(image_data))))

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

output_chunks = chunks(sys.argv[2])
ignored_kinds = (b"IDAT", b"IEND", b"iCCP")
if not any(kind == b"sRGB" for kind, _ in output_chunks):
    ignored_kinds += (b"sRGB",)
source = collections.Counter(
    (kind, payload) for kind, payload in chunks(sys.argv[1])
    if kind not in ignored_kinds)
output = collections.Counter(
    (kind, payload) for kind, payload in output_chunks
    if kind not in ignored_kinds)
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
  exit_code=0
  timeout 30 "$PNGDUMP" "$png_path" --write-icc "$INPUT_ICC" \
    --output "$OUTDIR/rejected.png" >"$LOGFILE" 2>&1 || exit_code=$?
  if [ "$exit_code" -ne 1 ]; then
    fail "resource-limit PNG did not fail gracefully (status $exit_code): $png_path"
  fi
  check_sanitizers "$LOGFILE"
  if [ "$png_path" = "$OUTDIR/oversized-raster.png" ]; then
    expected="PNG raster exceeds the 256 MiB processing limit."
  else
    expected="exceeds user limit in IHDR"
  fi
  grep -Fq "$expected" "$LOGFILE" || fail "missing resource-limit diagnostic: $png_path"
done

exit_code=0
timeout 30 "$PNGDUMP" "$OUTDIR/unknown-critical.png" --write-icc "$INPUT_ICC" \
  --output "$OUTDIR/rejected-critical.png" >"$LOGFILE" 2>&1 || exit_code=$?
[ "$exit_code" -eq 1 ] || fail "unknown critical chunk did not fail gracefully (status $exit_code)"
check_sanitizers "$LOGFILE"
grep -Fq "ABCD: unhandled critical chunk" "$LOGFILE" || fail "missing unknown critical chunk diagnostic"

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

python3 - "$PNGDUMP" "$INPUT_PNG" "$INPUT_ICC" "$OUTDIR" <<'PY'
import binascii
import os
import pathlib
import stat
import struct
import subprocess
import sys
import time
import zlib

if os.name == "posix":
    tool, image, profile, outdir = sys.argv[1:]
    for mask, existing_mode, expected_mode in (
        (0o022, 0o600, 0o600),
        (0o022, 0o640, 0o640),
        (0o022, None, 0o644),
        (0o077, None, 0o600),
    ):
        output = pathlib.Path(outdir) / "permissions.png"
        output.unlink(missing_ok=True)
        if existing_mode is not None:
            output.write_bytes(b"private destination")
            output.chmod(existing_mode)
        previous_mask = os.umask(mask)
        try:
            result = subprocess.run(
                [tool, image, "--write-icc", profile, "--output", str(output)],
                capture_output=True, timeout=30)
        finally:
            os.umask(previous_mask)
        diagnostics = result.stdout + result.stderr
        if result.returncode != 0:
            raise SystemExit(f"permission injection failed: {diagnostics!r}")
        if any(marker in diagnostics for marker in (
                b"AddressSanitizer", b"runtime error:", b"LeakSanitizer",
                b"UndefinedBehaviorSanitizer", b"DEADLYSIGNAL")):
            raise SystemExit(f"sanitizer diagnostic: {diagnostics!r}")
        actual_mode = stat.S_IMODE(output.stat().st_mode)
        if actual_mode != expected_mode:
            raise SystemExit(
                f"output permissions: expected {expected_mode:o}, got {actual_mode:o}")

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", binascii.crc32(kind + payload) & 0xffffffff))

    staging_image = pathlib.Path(outdir) / "staging-input.png"
    staging_image.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", 2048, 2048, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(b"\x00" * (2048 * (1 + 2048 * 3)))) +
        chunk(b"IEND", b""))
    output.chmod(0o600)
    previous_mask = os.umask(0o022)
    observed_private_staging = False
    try:
        with (pathlib.Path(outdir) / "staging-permissions.log").open("wb") as log:
            child = subprocess.Popen(
                [tool, str(staging_image), "--write-icc", profile, "--output", str(output)],
                stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 30
                while child.poll() is None:
                    for temporary in output.parent.glob(output.name + ".tmp-*"):
                        try:
                            mode = stat.S_IMODE(temporary.stat().st_mode)
                        except FileNotFoundError:
                            continue
                        if mode != 0o600:
                            raise SystemExit(f"temporary PNG is not private: {mode:o}")
                        observed_private_staging = True
                    if time.monotonic() > deadline:
                        raise SystemExit("temporary permission test timed out")
                    time.sleep(0.001)
                if child.returncode != 0:
                    raise SystemExit(f"temporary permission test exited {child.returncode}")
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
    finally:
        os.umask(previous_mask)
    if not observed_private_staging:
        raise SystemExit("temporary permission test did not observe a staged file")
    if stat.S_IMODE(output.stat().st_mode) != 0o600:
        raise SystemExit("private destination permissions changed")
PY
if [ -f "$OUTDIR/staging-permissions.log" ]; then
  check_sanitizers "$OUTDIR/staging-permissions.log"
fi

echo "  [PASS] issue-2854-pngdump-regressions -- metadata, resource bounds, raw export, and CLI"
