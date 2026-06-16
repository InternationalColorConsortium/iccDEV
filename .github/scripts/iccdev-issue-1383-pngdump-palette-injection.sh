#!/bin/bash
###############################################################################
# iccPngDump issue-1383 paletted-PNG ICC injection regression
###############################################################################
#
# Injecting an ICC profile into an indexed-colour (PNG_COLOR_TYPE_PALETTE) PNG
# failed with "libpng error: Valid palette required for paletted images" because
# the tool copied IHDR but not the PLTE/tRNS chunks to the output before
# png_write_info().  This test generates a paletted PNG, injects an ICC profile,
# and verifies the profile round-trips back out byte-for-byte.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1383-pngdump-palette-injection}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

PNGDUMP="$TOOLS_DIR/IccPngDump/iccPngDump"
INPUT_PNG="$OUTDIR/palette-2x2.png"
INPUT_ICC="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
OUTPUT_PNG="$OUTDIR/palette-2x2-icc.png"
EXTRACTED_ICC="$OUTDIR/extracted.icc"
LOGFILE="$OUTDIR/issue-1383-pngdump-palette-injection.log"

fail() {
  echo "  [FAIL] issue-1383-pngdump-palette-injection -- $1"
  exit 1
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null; then
    sed -n '1,80p' "$1"
    return 1
  fi
  return 0
}

generate_palette_png() {
  command -v python3 >/dev/null 2>&1 || return 1
  python3 - "$INPUT_PNG" <<'PY'
import sys, zlib, struct, binascii, pathlib

def chunk(typ, data):
    return (struct.pack(">I", len(data)) + typ + data +
            struct.pack(">I", binascii.crc32(typ + data) & 0xffffffff))

w = h = 2
ihdr = struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)        # bitdepth 8, colour type 3 (palette)
plte = bytes([0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255])  # 4-entry palette
raw = b"".join(b"\x00" + bytes(w) for _ in range(h))       # filter 0 + index bytes
png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
       chunk(b"PLTE", plte) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
pathlib.Path(sys.argv[1]).write_bytes(png)
PY
}

echo "=== iccPngDump issue-1383 paletted-PNG ICC injection regression ==="

if [ ! -x "$PNGDUMP" ]; then
  echo "  [SKIP] missing executable: $PNGDUMP"
  exit 0
fi
if [ ! -f "$INPUT_ICC" ]; then
  echo "  [SKIP] missing input ICC: $INPUT_ICC"
  exit 0
fi
if ! generate_palette_png; then
  echo "  [SKIP] python3 unavailable; cannot synthesize palette PNG"
  exit 0
fi

# Inject the ICC profile into the paletted PNG.
rm -f "$LOGFILE" "$OUTPUT_PNG" "$EXTRACTED_ICC"
exit_code=0
timeout 30 "$PNGDUMP" "$INPUT_PNG" --write-icc "$INPUT_ICC" --output "$OUTPUT_PNG" > "$LOGFILE" 2>&1 || exit_code=$?

check_sanitizers "$LOGFILE" || fail "sanitizer finding during injection"

if grep -Fq "Valid palette required for paletted images" "$LOGFILE" 2>/dev/null; then
  sed -n '1,40p' "$LOGFILE"
  fail "paletted-PNG injection still rejected by libpng (#1383)"
fi
if [ "$exit_code" -ne 0 ]; then
  sed -n '1,40p' "$LOGFILE"
  fail "injection exited non-zero ($exit_code)"
fi
if [ ! -s "$OUTPUT_PNG" ]; then
  fail "no output PNG written"
fi

# Re-extract the profile and confirm it round-trips byte-for-byte.
exit_code=0
timeout 30 "$PNGDUMP" "$OUTPUT_PNG" "$EXTRACTED_ICC" >> "$LOGFILE" 2>&1 || exit_code=$?
check_sanitizers "$LOGFILE" || fail "sanitizer finding during extraction"
if [ "$exit_code" -ne 0 ] || [ ! -s "$EXTRACTED_ICC" ]; then
  sed -n '1,40p' "$LOGFILE"
  fail "could not re-extract injected ICC profile"
fi
if ! cmp -s "$INPUT_ICC" "$EXTRACTED_ICC"; then
  fail "extracted ICC does not match the injected profile"
fi

echo "  [PASS] issue-1383-pngdump-palette-injection -- paletted PNG ICC round-trip"
exit 0
