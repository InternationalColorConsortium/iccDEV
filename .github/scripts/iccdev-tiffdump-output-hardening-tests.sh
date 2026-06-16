#!/bin/bash
# Regression tests for iccTiffDump attacker-controlled console output.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tiffdump-output-hardening}"
TIFFDUMP="$TOOLS/IccTiffDump/iccTiffDump"

mkdir -p "$OUTDIR"

if [ ! -x "$TIFFDUMP" ]; then
  echo "[FAIL] missing executable: $TIFFDUMP"
  exit 1
fi

SAMPLE_TIFF="$ICCDEV_TESTING/hybrid/Data/TShirtDesignKW.tif"
if [ ! -f "$SAMPLE_TIFF" ]; then
  echo "[FAIL] missing sample TIFF: $SAMPLE_TIFF"
  exit 1
fi

pass=0
fail=0

run_ok() {
  local name="$1"
  shift

  if "$@"; then
    echo "  [PASS] $name"
    pass=$((pass + 1))
  else
    echo "  [FAIL] $name"
    fail=$((fail + 1))
  fi
}

test_profile_description_escaping() {
  local mutated="$OUTDIR/tiffdump-mutated-desc.tif"
  local log="$OUTDIR/tiffdump-mutated-desc.log"

  cp "$SAMPLE_TIFF" "$mutated"
  perl -0pi -e 's/Dot Gain 20%/bad\n\e[31mX!!/g' "$mutated"
  "$TIFFDUMP" "$mutated" > "$log" 2>&1

  grep -Fq 'Description:      bad\n\x1B[31mX!!' "$log" || return 1
  ! grep -q "$(printf '\033')" "$log"
}

test_input_path_escaping() {
  local odd="$OUTDIR/name_with_
newline.tif"
  local log="$OUTDIR/tiffdump-path-escape.log"

  cp "$SAMPLE_TIFF" "$odd"
  "$TIFFDUMP" "$odd" > "$log" 2>&1

  grep -Fq 'Filename:          '"$OUTDIR"'/name_with_\nnewline.tif' "$log" || return 1
  ! grep -q "$(printf '\033')" "$log"
}

test_export_path_escaping() {
  local dst="$OUTDIR/export_with_
newline.icc"
  local log="$OUTDIR/tiffdump-export-escape.log"

  "$TIFFDUMP" "$SAMPLE_TIFF" "$dst" > "$log" 2>&1

  [ -s "$dst" ] || return 1
  grep -Fq 'Profile extracted to: '"$OUTDIR"'/export_with_\nnewline.icc' "$log"
}

# ---------------------------------------------------------------------------
# #1380: palette photometric must be reported (not silently "Min Is White"), and
# a parsed embedded ICC that fails conformance must be rejected, not rewritten.
# ---------------------------------------------------------------------------
PYTHON="$(command -v python3 || true)"

generate_palette_tiff() {
  # Minimal 2x2 8-bit PHOTOMETRIC_PALETTE (=3) TIFF with the required ColorMap.
  "$PYTHON" - "$1" <<'PY'
import struct, sys, pathlib
path = pathlib.Path(sys.argv[1])
w = h = 2; bps = 8; ncol = 1 << bps
pix = bytes(w * h); doff = 8; ifd = doff + len(pix)
if ifd % 2:
    pix += b"\0"; ifd += 1
cmap = b"".join(struct.pack("<H", (i * 65535) // (ncol - 1))
                for _ in range(3) for i in range(ncol))
entries = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, 1, bps), (259, 3, 1, 1),
           (262, 3, 1, 3), (273, 4, 1, doff), (277, 3, 1, 1), (278, 4, 1, h),
           (279, 4, 1, len(pix)), (284, 3, 1, 1), (296, 3, 1, 2),
           (320, 3, 3 * ncol, 0)]
entries.sort()
cmap_off = ifd + 2 + len(entries) * 12 + 4
entries = [(t, ty, c, (cmap_off if t == 320 else v)) for (t, ty, c, v) in entries]
d = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd)); d += pix
d += struct.pack("<H", len(entries))
for t, ty, c, v in entries:
    d += struct.pack("<HHI", t, ty, c)
    d += (struct.pack("<H", v) + b"\0\0") if (ty == 3 and c == 1) else struct.pack("<I", v)
d += struct.pack("<I", 0); d += cmap
path.write_bytes(d)
PY
}

generate_tiff_with_icc() {
  # 2x2 RGB TIFF embedding the ICC bytes from file $2 (tag 34675, out-of-line).
  "$PYTHON" - "$1" "$2" <<'PY'
import struct, sys, pathlib
path = pathlib.Path(sys.argv[1]); icc = pathlib.Path(sys.argv[2]).read_bytes()
w = h = 2; bps = 8; spp = 3
pix = bytes(w * h * spp); doff = 8; ifd = doff + len(pix)
if ifd % 2:
    pix += b"\0"; ifd += 1
entries = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, spp, 0), (259, 3, 1, 1),
           (262, 3, 1, 2), (273, 4, 1, doff), (277, 3, 1, spp), (278, 4, 1, h),
           (279, 4, 1, len(pix)), (284, 3, 1, 1), (296, 3, 1, 2),
           (34675, 7, len(icc), 0)]
entries.sort()
base = ifd + 2 + len(entries) * 12 + 4
bps_off = base; icc_off = bps_off + spp * 2
entries = [(t, ty, c, (bps_off if t == 258 else icc_off if t == 34675 else v))
           for (t, ty, c, v) in entries]
d = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd)); d += pix
d += struct.pack("<H", len(entries))
for t, ty, c, v in entries:
    d += struct.pack("<HHI", t, ty, c)
    d += (struct.pack("<H", v) + b"\0\0") if (ty == 3 and c == 1) else struct.pack("<I", v)
d += struct.pack("<I", 0)
d += b"".join(struct.pack("<H", bps) for _ in range(spp))  # BitsPerSample (out-of-line)
d += icc                                                   # embedded ICC profile
path.write_bytes(d)
PY
}

test_palette_photometric_report() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  local pal="$OUTDIR/palette.tif"
  local log="$OUTDIR/tiffdump-palette.log"
  generate_palette_tiff "$pal" || return 1
  "$TIFFDUMP" "$pal" > "$log" 2>&1 || true
  grep -Eq 'Photometric:[[:space:]]+Palette' "$log"
}

test_noncompliant_embedded_icc_rejected() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  local srgb="$ICCDEV_TESTING/sRGB_v4_ICC_preference.icc"
  [ -f "$srgb" ] || { echo "    [SKIP] missing $srgb"; return 0; }
  local bad="$OUTDIR/noncompliant.icc"
  local tif="$OUTDIR/rgb-noncompliant-icc.tif"
  local out="$OUTDIR/extracted-noncompliant.icc"
  local log="$OUTDIR/tiffdump-noncompliant-icc.log"
  # Make a non-conformant profile by zeroing the data colour space @offset 16.
  "$PYTHON" - "$srgb" "$bad" <<'PY'
import sys, pathlib
b = bytearray(pathlib.Path(sys.argv[1]).read_bytes())
b[16:20] = b"\0\0\0\0"
pathlib.Path(sys.argv[2]).write_bytes(b)
PY
  generate_tiff_with_icc "$tif" "$bad" || return 1
  rm -f "$out"
  "$TIFFDUMP" "$tif" "$out" > "$log" 2>&1 || true
  grep -Fq "violates the ICC specification" "$log" || return 1
  [ ! -e "$out" ]   # must NOT have written the non-conformant profile (#1380)
}

echo "=== iccTiffDump output hardening regression ==="
run_ok "tiffdump-profile-description-escape" test_profile_description_escaping
run_ok "tiffdump-input-path-escape" test_input_path_escaping
run_ok "tiffdump-export-path-escape" test_export_path_escaping
run_ok "tiffdump-palette-photometric-report" test_palette_photometric_report
run_ok "tiffdump-noncompliant-embedded-icc-reject" test_noncompliant_embedded_icc_rejected

echo "iccTiffDump output hardening regression: $pass passed, $fail failed, $((pass + fail)) total"

if [ "$fail" -ne 0 ]; then
  exit 1
fi
