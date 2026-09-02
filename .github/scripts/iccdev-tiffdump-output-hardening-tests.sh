#!/bin/bash
# Regression tests for iccTiffDump attacker-controlled console output.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tiffdump-output-hardening}"
TIFFDUMP="$TOOLS/IccTiffDump/iccTiffDump"
TOXML="$TOOLS/IccToXml/iccToXml"

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
NO_PROFILE_TIFF="$REPO_ROOT/.github/ci/test-data/spectral/spec_1"
if [ ! -f "$NO_PROFILE_TIFF" ]; then
  echo "[FAIL] missing no-profile sample TIFF: $NO_PROFILE_TIFF"
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
  grep -Fq 'Profile extracted byte-for-byte to: '"$OUTDIR"'/export_with_\nnewline.icc' "$log"
}

test_extra_arg_rejected() {
  local dst="$OUTDIR/extra-arg.icc"
  local log="$OUTDIR/tiffdump-extra-arg.log"

  rm -f "$dst"
  if "$TIFFDUMP" "$SAMPLE_TIFF" "$dst" extra-token > "$log" 2>&1; then
    return 1
  fi

  grep -Fq "Usage: iccTiffDump {--evidence-json} tiff_file {exported_icc_file}" "$log" || return 1
  [ ! -e "$dst" ]
}

test_missing_embedded_profile_export_rejected() {
  local dst="$OUTDIR/no-profile-export.icc"
  local log="$OUTDIR/tiffdump-no-profile-export.log"

  rm -f "$dst"
  if "$TIFFDUMP" "$NO_PROFILE_TIFF" "$dst" > "$log" 2>&1; then
    return 1
  fi

  grep -Fq "Profile:           None" "$log" || return 1
  grep -Fq "No embedded ICC profile to extract" "$log" || return 1
  [ ! -e "$dst" ]
}

# ---------------------------------------------------------------------------
# #1380: palette photometric must be reported (not silently "Min Is White").
# Nonconformant embedded bytes must remain available as forensic artifacts even
# when parsing or validation returns a failure status.
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

generate_defaulted_packbits_gray_tiff() {
  # 32x32 8-bit grayscale PackBits TIFF.  It intentionally omits
  # SamplesPerPixel, SampleFormat, and Orientation so CTiffImg must accept the
  # baseline/defaulted tag values reported by libtiff.
  "$PYTHON" - "$1" <<'PY'
import struct, sys, pathlib
path = pathlib.Path(sys.argv[1])
w = h = 32; bps = 8
packbits = bytes([129, 0]) * ((w * h) // 128)
doff = 8; ifd = doff + len(packbits)
if ifd % 2:
    packbits += b"\0"; ifd += 1
entries = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, 1, bps),
           (259, 3, 1, 32773), (262, 3, 1, 1), (273, 4, 1, doff),
           (278, 4, 1, h), (279, 4, 1, len(packbits)), (284, 3, 1, 1),
           (296, 3, 1, 2)]
entries.sort()
d = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd))
d += packbits
d += struct.pack("<H", len(entries))
for t, ty, c, v in entries:
    d += struct.pack("<HHI", t, ty, c)
    d += (struct.pack("<H", v) + b"\0\0") if (ty == 3 and c == 1) else struct.pack("<I", v)
d += struct.pack("<I", 0)
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

generate_nested_icc() {
  # Build an ICC.2 profile with $2 nested embeddedV5ProfileTag entries. Each
  # level is structurally small, so depth rather than payload size drives the
  # parser path under test.
  "$PYTHON" - "$1" "$2" <<'PY'
import pathlib, struct, sys
path = pathlib.Path(sys.argv[1]); depth = int(sys.argv[2])

def header(size):
    h = bytearray(128)
    struct.pack_into(">I", h, 0, size)
    h[8:12] = b"\x05\0\0\0"
    h[12:16] = b"mntr"
    h[16:20] = b"RGB "
    h[20:24] = b"XYZ "
    h[36:40] = b"acsp"
    return h

profile = header(132) + struct.pack(">I", 0)
for _ in range(depth):
    tag = b"ICCp" + b"\0\0\0\0" + profile
    size = 144 + len(tag)
    profile = (header(size) + struct.pack(">I", 1) + b"ICC5" +
               struct.pack(">II", 144, len(tag)) + tag)
path.write_bytes(profile)
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

test_defaulted_packbits_gray_loads() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  local tif="$OUTDIR/defaulted-packbits-gray.tif"
  local log="$OUTDIR/tiffdump-defaulted-packbits-gray.log"
  generate_defaulted_packbits_gray_tiff "$tif" || return 1
  "$TIFFDUMP" "$tif" > "$log" 2>&1
  grep -Eq 'SamplesPerPixel:[[:space:]]+1' "$log" || return 1
  grep -Eq 'Compression:[[:space:]]+PackBits' "$log"
}

test_noncompliant_embedded_icc_preserved() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  local srgb="$ICCDEV_TESTING/sRGB_v4_ICC_preference.icc"
  [ -f "$srgb" ] || { echo "    [SKIP] missing $srgb"; return 0; }
  local bad="$OUTDIR/noncompliant.icc"
  local tif="$OUTDIR/rgb-noncompliant-icc.tif"
  local out="$OUTDIR/extracted-noncompliant.icc"
  local dump_log="$OUTDIR/tiffdump-noncompliant-icc-dump.log"
  local export_log="$OUTDIR/tiffdump-noncompliant-icc-export.log"
  local status=0
  # Make a non-conformant profile by zeroing the data colour space @offset 16.
  "$PYTHON" - "$srgb" "$bad" <<'PY'
import sys, pathlib
b = bytearray(pathlib.Path(sys.argv[1]).read_bytes())
b[16:20] = b"\0\0\0\0"
pathlib.Path(sys.argv[2]).write_bytes(b)
PY
  generate_tiff_with_icc "$tif" "$bad" || return 1

  "$TIFFDUMP" "$tif" > "$dump_log" 2>&1 || status=$?
  [ "$status" -ge 1 ] && [ "$status" -le 127 ] || return 1
  grep -Fq "violates the ICC specification" "$dump_log" || return 1

  rm -f "$out"
  status=0
  "$TIFFDUMP" "$tif" "$out" > "$export_log" 2>&1 || status=$?
  [ "$status" -ge 1 ] && [ "$status" -le 127 ] || return 1
  grep -Fq "violates the ICC specification" "$export_log" || return 1
  grep -Fq "Profile extracted byte-for-byte" "$export_log" || return 1
  cmp -s "$bad" "$out"
}

test_malformed_embedded_icc_preserved() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  local bad="$OUTDIR/malformed.icc"
  local tif="$OUTDIR/rgb-malformed-icc.tif"
  local out="$OUTDIR/extracted-malformed.icc"
  local dump_log="$OUTDIR/tiffdump-malformed-icc-dump.log"
  local export_log="$OUTDIR/tiffdump-malformed-icc-export.log"
  local status=0

  "$PYTHON" - "$bad" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_bytes(b"not an ICC profile")
PY
  generate_tiff_with_icc "$tif" "$bad" || return 1

  "$TIFFDUMP" "$tif" > "$dump_log" 2>&1 || status=$?
  [ "$status" -ge 1 ] && [ "$status" -le 127 ] || return 1
  grep -Fq "Unable to open embedded ICC profile" "$dump_log" || return 1

  rm -f "$out"
  status=0
  "$TIFFDUMP" "$tif" "$out" > "$export_log" 2>&1 || status=$?
  [ "$status" -ge 1 ] && [ "$status" -le 127 ] || return 1
  grep -Fq "Unable to open embedded ICC profile" "$export_log" || return 1
  grep -Fq "Profile extracted byte-for-byte" "$export_log" || return 1
  cmp -s "$bad" "$out"
}

test_nested_embedded_icc_is_bounded_and_preserved() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  local nested="$OUTDIR/nested-depth-512.icc"
  local tif="$OUTDIR/nested-depth-512.tif"
  local out="$OUTDIR/extracted-nested-depth-512.icc"
  local log="$OUTDIR/tiffdump-nested-depth-512.log"
  local status=0

  generate_nested_icc "$nested" 512 || return 1
  generate_tiff_with_icc "$tif" "$nested" || return 1
  rm -f "$out"

  timeout 10 "$TIFFDUMP" "$tif" "$out" > "$log" 2>&1 || status=$?
  [ "$status" -ne 124 ] || { echo "    iccTiffDump timed out"; return 1; }
  [ "$status" -ge 1 ] && [ "$status" -le 127 ] || return 1
  grep -Fq "Profile extracted byte-for-byte" "$log" || return 1
  grep -Fq "Subprofile recursion halted" "$log" || return 1
  cmp -s "$nested" "$out"
}

test_recursive_full_tag_read_is_bounded() {
  [ -n "$PYTHON" ] || { echo "    [SKIP] python3 unavailable"; return 0; }
  [ -x "$TOXML" ] || { echo "    [SKIP] missing executable: $TOXML"; return 0; }
  local nested="$OUTDIR/readtags-nested-depth-512.icc"
  local xml="$OUTDIR/readtags-nested-depth-512.xml"
  local log="$OUTDIR/readtags-nested-depth-512.log"
  local status=0

  generate_nested_icc "$nested" 512 || return 1
  rm -f "$xml"

  timeout 10 "$TOXML" "$nested" "$xml" > "$log" 2>&1 || status=$?
  [ "$status" -ne 124 ] || { echo "    iccToXml timed out"; return 1; }
  [ "$status" -ge 1 ] && [ "$status" -le 127 ] || return 1
  grep -Fq "Unable to read" "$log"
}

echo "=== iccTiffDump output hardening regression ==="
run_ok "tiffdump-profile-description-escape" test_profile_description_escaping
run_ok "tiffdump-input-path-escape" test_input_path_escaping
run_ok "tiffdump-export-path-escape" test_export_path_escaping
run_ok "tiffdump-extra-arg-reject" test_extra_arg_rejected
run_ok "tiffdump-no-profile-export-reject" test_missing_embedded_profile_export_rejected
run_ok "tiffdump-palette-photometric-report" test_palette_photometric_report
run_ok "tiffdump-defaulted-packbits-gray-load" test_defaulted_packbits_gray_loads
run_ok "tiffdump-noncompliant-embedded-icc-preserve" test_noncompliant_embedded_icc_preserved
run_ok "tiffdump-malformed-embedded-icc-preserve" test_malformed_embedded_icc_preserved
run_ok "tiffdump-nested-embedded-icc-bounded-preserve" test_nested_embedded_icc_is_bounded_and_preserved
run_ok "embedded-profile-full-tag-read-bounded" test_recursive_full_tag_read_is_bounded

echo "iccTiffDump output hardening regression: $pass passed, $fail failed, $((pass + fail)) total"

if [ "$fail" -ne 0 ]; then
  exit 1
fi
