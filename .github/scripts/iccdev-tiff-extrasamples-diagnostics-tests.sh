#!/bin/bash
###############################################################################
# ExtraSamples diagnostic-fidelity regression (#2386)
###############################################################################
#
# CTiffImg::Open() discarded the return of its TIFFGetField(TIFFTAG_EXTRASAMPLES)
# call, so nothing downstream could tell "tag 338 absent" from "tag 338 stored".
# iccTiffDump then printed its ExtraSamples line only when the count was nonzero
# and said nothing at all about a directory libtiff had repaired.
#
# libtiff repairs a file whose photometric model plus stored ExtraSamples do not
# account for SamplesPerPixel, warns on stderr ("Defining non-color channels as
# ExtraSamples") and names no count.  It publishes the repaired figure ONLY
# through TIFFGetFieldDefaulted; the plain Get still fails.  So the dump reported
# SamplesPerPixel 81 against a one-channel photometric model and left the other
# 80 channels unmentioned.
#
# The count colour management uses -- CTiffImg::GetExtraSamples() -- is
# deliberately NOT changed by the fix: libtiff calls every unaccounted channel an
# extra sample, and feeding 80 to iccApplyProfiles would reclassify an 81-band
# spectral image's bands as alpha.  Case 4 pins that separation.
#
# The PRESENT-and-repaired half is closed too.  Where tag 338 is present and libtiff
# still repairs, it overwrites the stored count in place while leaving the field
# marked present, so every libtiff getter returns the repaired figure -- a 6-sample
# MinIsBlack file storing one extra and an honest five-extra file produced
# byte-identical dumps.  CTiffImg::Open() now reads tag 338's own count field out of
# the IFD, which is the only way to recover it, and iccTiffDump reports both numbers
# when they disagree.  Case 2 pins that, and carries an honest-directory control so an
# unconditional annotation cannot satisfy it.
#
# Fixtures are written byte by byte rather than through an encoder so the tag
# under test is exactly what the test intends, and tag presence is read back out
# of the IFD directly rather than through a tool, so a tool reporting presence
# wrongly cannot make a case pass for the wrong reason.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tiff-extrasamples-diagnostics}"
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

TIFFDUMP="$TOOLS_DIR/IccTiffDump/iccTiffDump"
# Tracked, and the file the report was filed against: 81 bands, MinIsBlack, no
# tag 338.  Every Testing/*.icc is generated, but this .tif is in the repo.
TRACKED_SPECTRAL="$REPO_ROOT/Testing/hybrid/Data/smCows380_5_780.tif"

WORKDIR="$OUTDIR/tiff-extrasamples-diagnostics"
LOGFILE="$OUTDIR/tiff-extrasamples-diagnostics.log"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

pass_case() {
  PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))
  printf '[PASS] %s -- %s\n' "$1" "$2" | tee -a "$LOGFILE"
}

fail_case() {
  FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1))
  printf '[FAIL] %s -- %s\n' "$1" "$2" | tee -a "$LOGFILE"
}

skip_case() {
  SKIP=$((SKIP + 1)); TOTAL=$((TOTAL + 1))
  printf '[SKIP] %s -- %s\n' "$1" "$2" | tee -a "$LOGFILE"
}

# generate_tiff <path> <width> <height> <samples> <photometric> <extrasamples...>
# Writes a minimal single-strip 8-bit TIFF.  Tag 338 is emitted only when extra
# sample values are supplied, so "no trailing arguments" produces exactly the
# malformed shape this issue is about: a photometric model that accounts for
# fewer channels than SamplesPerPixel, with nothing to explain the difference.
generate_tiff() {
  python3 - "$@" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
width = int(sys.argv[2])
height = int(sys.argv[3])
samples = int(sys.argv[4])
photometric = int(sys.argv[5])
extras = [int(a) for a in sys.argv[6:]]
path.parent.mkdir(parents=True, exist_ok=True)

pixel_data = bytes([0x40]) * (width * height * samples)

data_offset = 8
strip_bytes = len(pixel_data)
cursor = data_offset + strip_bytes
if cursor % 2:
    # Pad only to keep the following IFD word-aligned; StripByteCounts stays the
    # unpadded length so libtiff does not read past the pixel data.
    pixel_data += b"\0"
    cursor += 1

# A SHORT array of two or fewer values fits in the entry's own 4-byte value field,
# and libtiff reads it from there (TIFFReadDirEntryArrayWithLimit memcpys out of
# tdir_offset when datasize <= 4).  Writing an offset instead made a 2-sample
# fixture decode BitsPerSample as the offset's two halves.  Tag 338 below already
# had this case; tag 258 needs it too.
bps_inline = samples <= 2

bps_offset = cursor
if samples > 1 and not bps_inline:
    cursor += 2 * samples

# ExtraSamples needs an out-of-line array once it carries more than two values.
extras_offset = cursor
extras_inline = len(extras) <= 2
if extras and not extras_inline:
    cursor += 2 * len(extras)
    if cursor % 2:
        cursor += 1

ifd_offset = cursor

entries = [
    (256, 4, 1, width),
    (257, 4, 1, height),
    (258, 3, samples, 8 if samples == 1 else (0 if bps_inline else bps_offset)),
    (259, 3, 1, 1),
    (262, 3, 1, photometric),
    (273, 4, 1, data_offset),
    (277, 3, 1, samples),
    (278, 4, 1, height),
    (279, 4, 1, strip_bytes),
    (284, 3, 1, 1),
    (296, 3, 1, 2),
]
if extras:
    entries.append((338, 3, len(extras), extras[0] if extras_inline else extras_offset))
entries.sort()

data = bytearray(b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset))
data.extend(pixel_data)
if samples > 1 and not bps_inline:
    data.extend(struct.pack("<H", 8) * samples)
if extras and not extras_inline:
    for value in extras:
        data.extend(struct.pack("<H", value))
    if len(data) % 2:
        data.extend(b"\0")

data.extend(struct.pack("<H", len(entries)))
for tag, kind, count, value in entries:
    data.extend(struct.pack("<HHI", tag, kind, count))
    if kind == 3 and count == 1:
        data.extend(struct.pack("<H", value))
        data.extend(b"\0\0")
    elif tag == 258 and bps_inline:
        data.extend(struct.pack("<H", 8) * samples)
        data.extend(b"\0\0" * (2 - samples))
    elif tag == 338 and extras_inline:
        for v in extras:
            data.extend(struct.pack("<H", v))
        data.extend(b"\0\0" * (2 - len(extras)))
    else:
        data.extend(struct.pack("<I", value))
data.extend(struct.pack("<I", 0))
path.write_bytes(data)
PY
}

# Print the value count stored for tag 338 in a TIFF's first IFD, or "absent".
# Read from the bytes so the assertion does not depend on the tool under test.
read_stored_extrasamples_count() {
  python3 - "$1" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if len(data) < 8 or data[:2] not in (b"II", b"MM"):
    print("unreadable")
    sys.exit(0)

endian = "<" if data[:2] == b"II" else ">"
offset = struct.unpack_from(endian + "I", data, 4)[0]
if offset + 2 > len(data):
    print("unreadable")
    sys.exit(0)

count = struct.unpack_from(endian + "H", data, offset)[0]
for index in range(count):
    entry = offset + 2 + 12 * index
    if entry + 12 > len(data):
        break
    tag, kind, values = struct.unpack_from(endian + "HHI", data, entry)
    if tag == 338:
        print(values)
        sys.exit(0)

print("absent")
PY
}

# Print the BitsPerSample values stored in a TIFF's first IFD, space separated.
# Resolves the inline-vs-offset distinction the same way libtiff does, so it can
# tell a correctly built fixture from one whose values are really an offset.
read_bits_per_sample() {
  python3 - "$1" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if len(data) < 8 or data[:2] not in (b"II", b"MM"):
    print("unreadable")
    sys.exit(0)

endian = "<" if data[:2] == b"II" else ">"
offset = struct.unpack_from(endian + "I", data, 4)[0]
count = struct.unpack_from(endian + "H", data, offset)[0]
for index in range(count):
    entry = offset + 2 + 12 * index
    if entry + 12 > len(data):
        break
    tag, kind, values = struct.unpack_from(endian + "HHI", data, entry)
    if tag != 258:
        continue
    # A SHORT array of 4 bytes or fewer lives in the entry's value field.
    base = entry + 8 if values * 2 <= 4 else struct.unpack_from(endian + "I", data, entry + 8)[0]
    print(" ".join(str(struct.unpack_from(endian + "H", data, base + 2 * i)[0])
                   for i in range(values)))
    sys.exit(0)

print("absent")
PY
}

# The ExtraSamples line iccTiffDump prints, empty when it prints no such line.
dump_extrasamples_line() {
  "$TIFFDUMP" "$1" 2>/dev/null | sed -n 's/^ExtraSamples: *//p' | head -1
}

# Assertions about an ABSENT line cannot stand on their own: a tool that fails
# outright prints nothing, so "no ExtraSamples line" is satisfied by a broken
# iccTiffDump -- including an ASan abort, on a test carrying the asan label.
# Require the dump to have succeeded and produced the line that must always be
# there before any such assertion is trusted.
dump_succeeded() {
  local f="$1" expected_samples="$2" out
  out="$("$TIFFDUMP" "$f" 2>/dev/null)" || return 1
  printf '%s\n' "$out" | grep -q "^SamplesPerPixel: *${expected_samples}\$"
}

require_tools() {
  local name="$1"
  if [ ! -x "$TIFFDUMP" ]; then
    skip_case "$name" "iccTiffDump not built at $TIFFDUMP"
    return 1
  fi
  if ! command -v python3 >/dev/null 2>&1; then
    skip_case "$name" "python3 unavailable, cannot build or inspect TIFF fixtures"
    return 1
  fi
  return 0
}

###############################################################################
# Case 1 -- the defect itself, on a synthetic 6-channel MinIsBlack file.
# MinIsBlack accounts for one channel, SamplesPerPixel is 6, tag 338 is absent.
# libtiff repairs the directory to 5 extra samples and warns; before the fix the
# dump printed no ExtraSamples line at all, so the report never accounted for
# five of the six channels.
###############################################################################
case_absent_tag_is_explained() {
  local name="absent-tag-is-explained"
  require_tools "$name" || return

  local f="$WORKDIR/minisblack-6ch-no-338.tif"
  generate_tiff "$f" 4 4 6 1

  local stored
  stored="$(read_stored_extrasamples_count "$f")"
  if [ "$stored" != "absent" ]; then
    fail_case "$name" "fixture is wrong: tag 338 should be absent, IFD says '$stored'"
    return
  fi

  local line
  line="$(dump_extrasamples_line "$f")"
  if [ -z "$line" ]; then
    fail_case "$name" "iccTiffDump reported no ExtraSamples line for a 6-sample MinIsBlack file with no tag 338"
    return
  fi
  case "$line" in
    *"not stored"*"5 of 6"*)
      pass_case "$name" "dump explains the repaired layout: $line" ;;
    *)
      fail_case "$name" "expected a 'not stored ... 5 of 6' explanation, got: $line" ;;
  esac
}

###############################################################################
# Case 2 -- a repaired tag names BOTH counts; an honest one prints the plain count.
#
# The fixture is deliberately the shape where stored and repaired DISAGREE: 6
# samples, MinIsBlack (one colour channel), tag 338 storing a single value.  One
# colour plus one extra does not account for six samples, so libtiff repairs --
# and because the tag IS present it overwrites the stored count in place while
# leaving the field marked present.  The plain getter therefore succeeds and
# returns 5, not the 1 the IFD holds, and libtiff offers no way to read the
# original back.
#
# This case previously pinned that as a documented LIMIT, asserting a bare "5".
# The limit is now lifted: CTiffImg::Open() reads tag 338's own count field out
# of the IFD, which is the only way to recover it, so the dump can name both
# numbers.  The assertion below is therefore the reverse of what it once was --
# a bare "5" for this fixture is now a FAILURE, because it means a repaired
# directory is again indistinguishable from an honest one.
#
# The honest 5-extra control at the end is what gives that teeth.  An
# implementation appending the wording unconditionally would satisfy the first
# assertion; case 3's well-formed fixture does not cover it, because that file
# carries no tag 338 at all and so never reaches this branch.
###############################################################################
case_stored_tag_prints_count() {
  local name="stored-tag-prints-count"
  require_tools "$name" || return

  local f="$WORKDIR/minisblack-6ch-338-one.tif"
  generate_tiff "$f" 4 4 6 1 2

  local stored
  stored="$(read_stored_extrasamples_count "$f")"
  if [ "$stored" != "1" ]; then
    fail_case "$name" "fixture is wrong: tag 338 should store 1 value, IFD says '$stored'"
    return
  fi

  if ! dump_succeeded "$f" 6; then
    fail_case "$name" "iccTiffDump did not report SamplesPerPixel 6 for the fixture"
    return
  fi

  # UPDATED for the second half of #2386. This case previously asserted a bare "5" and
  # carried a failure arm reading "if that is now recoverable, this case and the header
  # comment both need updating" -- the stored count IS now recoverable, because Open()
  # reads tag 338's count field straight out of the IFD rather than asking libtiff,
  # which cannot answer once it has repaired the directory in place.
  #
  # Both numbers are required. The effective count leads, because it is the layout the
  # rest of libtiff is using; the stored count follows, because it is what tells the
  # reader the file is malformed at all.
  local line
  line="$(dump_extrasamples_line "$f")"
  case "$line" in
    "5 (file stores 1"*) ;;
    "5")
      fail_case "$name" "dump printed a bare 5 -- the repaired directory is again indistinguishable from an honest one"
      return ;;
    *"not stored"*)
      fail_case "$name" "a file that DOES store tag 338 was reported as not stored: $line"
      return ;;
    *)
      fail_case "$name" "expected the effective count then the stored one, got: '${line:-<no line>}'"
      return ;;
  esac

  # The control that makes the assertion above mean something: a file whose stored
  # count is honest must NOT be annotated. Without this, an implementation that
  # appended the wording unconditionally would pass. case_wellformed_file_is_silent
  # does not cover it -- that fixture has no tag 338 at all, so it never reaches this
  # branch; this one stores five extras and is annotation-eligible.
  local honest="$WORKDIR/minisblack-6ch-338-five.tif"
  generate_tiff "$honest" 4 4 6 1 0 0 0 0 0

  local honest_stored
  honest_stored="$(read_stored_extrasamples_count "$honest")"
  if [ "$honest_stored" != "5" ]; then
    fail_case "$name" "control fixture should store 5 extras, IFD says '$honest_stored'"
    return
  fi

  local honest_line
  honest_line="$(dump_extrasamples_line "$honest")"
  if [ "$honest_line" != "5" ]; then
    fail_case "$name" "an honest 5-extra file must print a bare '5', got '${honest_line:-<no line>}'"
    return
  fi

  pass_case "$name" "a repaired tag names both counts; an honest one prints a bare 5"
}

###############################################################################
# Case 3 -- a well-formed file gains no line.  RGB accounts for all three of its
# channels, so there is nothing to explain and the dump must stay silent.  This
# fails against a fix that prints the new wording unconditionally.
###############################################################################
case_wellformed_file_is_silent() {
  local name="wellformed-file-is-silent"
  require_tools "$name" || return

  local f="$WORKDIR/rgb-3ch.tif"
  generate_tiff "$f" 4 4 3 2

  local stored
  stored="$(read_stored_extrasamples_count "$f")"
  if [ "$stored" != "absent" ]; then
    fail_case "$name" "fixture is wrong: tag 338 should be absent, IFD says '$stored'"
    return
  fi

  # Before trusting an absence, require the dump to have actually run: an empty
  # ExtraSamples line is also what a crashed or rejecting iccTiffDump produces.
  if ! dump_succeeded "$f" 3; then
    fail_case "$name" "iccTiffDump did not report SamplesPerPixel 3 for a well-formed RGB file"
    return
  fi

  local line
  line="$(dump_extrasamples_line "$f")"
  if [ -n "$line" ]; then
    fail_case "$name" "a well-formed RGB file gained an ExtraSamples line: $line"
    return
  fi

  pass_case "$name" "no ExtraSamples line for a file whose photometric model accounts for every channel"
}

###############################################################################
# Case 4 -- the tracked 81-band spectral file the report was filed against, and
# the separation that matters: the dump names 80 of 81, while iccApplyProfiles
# still sees a stored count of 0.  Reading them from one invocation would not
# prove that, so the stored count is read from the IFD and the effective count
# from the tool, and the two are required to DISAGREE.
###############################################################################
case_tracked_spectral_file() {
  local name="tracked-spectral-file"
  require_tools "$name" || return

  if [ ! -f "$TRACKED_SPECTRAL" ]; then
    skip_case "$name" "tracked fixture missing: $TRACKED_SPECTRAL"
    return
  fi

  local stored
  stored="$(read_stored_extrasamples_count "$TRACKED_SPECTRAL")"
  if [ "$stored" != "absent" ]; then
    fail_case "$name" "tracked fixture changed: tag 338 should be absent, IFD says '$stored'"
    return
  fi

  local line
  line="$(dump_extrasamples_line "$TRACKED_SPECTRAL")"
  case "$line" in
    *"not stored"*"80 of 81"*)
      pass_case "$name" "dump explains libtiff's repair without adopting it: $line" ;;
    "")
      fail_case "$name" "iccTiffDump reported no ExtraSamples line for the tracked 81-band file" ;;
    *)
      fail_case "$name" "expected a 'not stored ... 80 of 81' explanation, got: $line" ;;
  esac
}

###############################################################################
# Case 5 -- the fixture generator itself, at the sample count where TIFF switches
# a SHORT array from an out-of-line offset to the entry's own value field.  Two
# 16-bit values are exactly 4 bytes, so BitsPerSample must be written inline;
# emitting an offset instead made libtiff decode it as the offset's two halves,
# and the fixture -- not the tool -- would have been what a future case measured.
# Read back through the IFD parser rather than the tool, for the same reason.
###############################################################################
case_generator_two_sample_bitspersample() {
  local name="generator-two-sample-bitspersample"
  require_tools "$name" || return

  local f="$WORKDIR/minisblack-2ch.tif"
  generate_tiff "$f" 4 4 2 1

  local bps
  bps="$(read_bits_per_sample "$f")"
  if [ "$bps" != "8 8" ]; then
    fail_case "$name" "2-sample fixture decodes BitsPerSample as '$bps', expected '8 8'"
    return
  fi

  if ! dump_succeeded "$f" 2; then
    fail_case "$name" "iccTiffDump could not read the 2-sample fixture"
    return
  fi

  pass_case "$name" "a 2-sample fixture stores BitsPerSample inline and reads back as 8 8"
}

###############################################################################

: > "$LOGFILE"
printf '=== ExtraSamples diagnostic-fidelity regression (#2386) ===\n' | tee -a "$LOGFILE"
printf 'tools: %s\n' "$TOOLS_DIR" | tee -a "$LOGFILE"

case_absent_tag_is_explained
case_stored_tag_prints_count
case_wellformed_file_is_silent
case_tracked_spectral_file
case_generator_two_sample_bitspersample

printf '\n=== summary: %d passed, %d failed, %d skipped, %d total ===\n' \
  "$PASS" "$FAIL" "$SKIP" "$TOTAL" | tee -a "$LOGFILE"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

# Every case skipped means nothing was measured.  Report that as a skip rather
# than success: a script CTest that exits 0 having asserted nothing reports GREEN
# and hides the gap.
if [ "$PASS" -eq 0 ]; then
  exit 77
fi

exit 0
