#!/bin/bash
###############################################################################
# iccDEV issues #1954 and #1931 -- follow-up
#   the atoi() sites OUTSIDE IccMpeXml.cpp: the profile-header spectral ranges,
#   the sparse-matrix type, the spectral viewing-conditions ranges, and the two
#   embedded-image tags together with the writers that fed them
###############################################################################
#
# #1962 fixed the MPE element readers in IccMpeXml.cpp and left the remaining
# raw `atoi(icXmlAttrValue(...))` sites for this change, since they sit in the
# territory #1909 covered at the tag level. They are the same defect: an
# unsigned attribute parsed with atoi(), which returns a signed int, then stored
# or cast into a narrower or unsigned field, so a value that does not fit
# changes on the way in instead of being refused.
#
# As in #1962, the cases with teeth in an ordinary CI job are NOT the negatives
# the issues reported -- those tend to wrap into a range a later check already
# rejects. The ones that matter land on a value the rest of the library accepts:
#
#   SpectralRange   steps="65567"      stored and re-serialized as 31
#   BiSpectralRange steps="65577"      stored and re-serialized as 41
#   SparseMatrixArray matrixType="65540"  stored as 4 (icSparseMatrixFloat32,
#                                      a fully supported type)
#
# In each case master reports "Profile parsed and saved correctly" and writes a
# profile carrying a number the source document never contained.
#
# CASE W -- the writers, and why they had to change in the same commit.
#
# CIccTagXmlEmbeddedHeightImage::ToXml and its NormalImage twin cast two
# unsigned 32-bit fields to unsigned and then printed them with "%d", so the
# cast was a no-op:
#
#   SeamlessIndicator -- an icUInt32Number; a value with bit 31 set was written
#     out negative, and the reader (which floored negatives to 0 after #1342)
#     then read it back as 0. Those two do NOT cancel: SeamlessIndicator=
#     "2147483650" round-trips to "0" on master, i.e. the value is destroyed.
#
#   EncodingFormat -- an icImageEncodingType, whose underlying type is fixed at
#     icUInt32Number and whose own header defines icImageTypeMaximum =
#     0xffffffff. Here the two halves DO cancel: master accepts
#     EncodingFormat="-12623518", stores 4282343778, and prints it back as
#     "-12623518", so a malformed attribute survives a full round-trip
#     unchanged. Fixing the reader alone would have stopped iccDEV's own output
#     loading, which is exactly the coupling #1962 documented for MPE Flags.
#
# CASE T -- atoi() where atof() was meant.
#
# CIccTagXmlSpectralViewingConditions::ParseXml read the ObserverFuncs "end"
# wavelength with atoi() while the sibling "start", and both endpoints of the
# IlluminantSPD range beside it, use atof(). "end" is a nm wavelength held as a
# float16, so the integer parse truncated the fraction: end="702.5" was stored
# and re-serialized as 702.
#
# CASE X -- a writer typo, found while fixing the reader beside it.
#
# CIccProfileXml::ToXmlWithBlanks ended the BiSpectralRange format string with
# "/>\n)", so a stray ')' was emitted after the newline, immediately before the
# closing tag, in every profile carrying a bi-spectral range. It round-tripped
# only because the reader finds <Wavelengths> with icXmlFindNode(), which walks
# past text nodes. The tracked PoC fixture ub-heightimage-parsexml-1342.xml has
# the junk baked into line 29, which is how long it has been emitted.
#
# Every fixture is a tracked document with ONE attribute changed, applied here
# rather than checked in, and the script FAILS if a mutation did not apply: a
# silently-unmutated fixture would convert cleanly and every assertion below
# would pass while testing nothing.
#
# CONTROLS. Each laundering case is paired with the value it wrapped to, as a
# legal document that must still convert (steps="31", steps="41",
# matrixType="4"). That pairing separates "refuses a value that does not fit the
# field" from "lowered the ceiling". SeamlessIndicator="2147483650" is a control
# in the same sense: it is representable in its 32-bit field, so it must be
# ACCEPTED and must survive the round trip intact.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (or skipped cleanly)
#   2 - regression detected
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1954-followup}"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi
TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"

status=0

# Donors. Each already exercises the reader under test on a known-good path.
FLUOR=Testing/Named/FluorescentNamedColor.xml        # spectral + bi-spectral ranges,
                                                     # and an <ObserverFuncs> range
SPARSE=Testing/Named/SparseMatrixNamedColor.xml      # <SparseMatrixArray matrixType="1">
HEIGHT=.github/ci/test-data/ub-heightimage-parsexml-1342.xml  # the only tracked
                                                     # document carrying a <HeightImage>

# mutate <donor-relative-path> <output-name> <find> <replace> [<find> <replace>]...
#
# Applies literal substitutions to a tracked fixture and writes the result to
# OUTDIR. Refuses to write a fixture a substitution did not change, which keeps a
# donor edited upstream from turning this test into a silent no-op.
mutate() {
  local donor="$REPO_ROOT/$1" out="$OUTDIR/$2"
  local rel="$1"
  shift 2
  if [ ! -f "$donor" ]; then
    echo "[SKIP] donor fixture missing: $rel"
    return 1
  fi
  python3 - "$donor" "$out" "$@" <<'PYEOF'
import sys
donor, out = sys.argv[1], sys.argv[2]
pairs = sys.argv[3:]
src = open(donor, encoding="utf-8", errors="surrogateescape").read()
for find, repl in zip(pairs[0::2], pairs[1::2]):
    if find not in src:
        sys.stderr.write(find[:70] + "\n")
        sys.exit(3)
    src = src.replace(find, repl, 1)
open(out, "w", encoding="utf-8", errors="surrogateescape").write(src)
PYEOF
  local rc=$?
  if [ $rc -eq 3 ]; then
    echo "[FAIL] fixture drift: a literal no longer appears in $rel --"
    echo "       the case below would have run against an unmutated document"
    status=2
    return 1
  elif [ $rc -ne 0 ]; then
    echo "[SKIP] could not build $2 from $rel"
    return 1
  fi
  return 0
}

TOOL_RC=0
TOOL_LOG=""
TOOL_ICC=""
run_tool() {
  # Sets globals rather than echoing: a caller using $(run_tool ...) would run
  # this in a subshell and TOOL_RC would never reach the caller.
  local xml="$OUTDIR/$1" base="${1%.xml}"
  TOOL_LOG="$OUTDIR/$base.log"
  TOOL_ICC="$OUTDIR/$base.icc"
  rm -f "$TOOL_ICC"
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0:allocator_may_return_null=1" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$FROMXML" "$xml" "$TOOL_ICC" -noid > "$TOOL_LOG" 2>&1
  TOOL_RC=$?
}

# A laundering case: the document must be refused, and no profile written.
# $1 = fixture  $2 = description  $3 = grep pattern showing the value that landed
expect_reject() {
  run_tool "$1"
  if [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1954/#1931: $2 -- profile written (rc=$TOOL_RC)"
    if [ -n "${TOXML:-}" ] && [ -x "${TOXML:-}" ] && \
       "$TOXML" "$TOOL_ICC" "$OUTDIR/${1%.xml}.roundtrip.xml" >/dev/null 2>&1; then
      echo "       written profile re-serializes as:"
      grep -oE "$3" "$OUTDIR/${1%.xml}.roundtrip.xml" | head -2 | sed 's/^/         /'
    fi
    status=2
  else
    echo "[PASS] $2 -- refused"
  fi
  rm -f "$TOOL_ICC"
}

# A control: the value is representable and must still be accepted.
expect_accept() {
  run_tool "$1"
  if [ -f "$TOOL_ICC" ]; then
    echo "[PASS] control: $2 still converts"
  else
    echo "[FAIL] control: $2 was rejected -- the fix is refusing representable"
    echo "       values, not just unrepresentable ones"
    sed -n '1,6p' "$TOOL_LOG"
    status=2
  fi
  rm -f "$TOOL_ICC"
}

# A value-preservation case: convert, re-serialize, and require the attribute to
# come back carrying exactly what the source document said.
# $1 = fixture  $2 = description  $3 = extended regex the round-tripped XML must match
expect_roundtrip() {
  run_tool "$1"
  local rt="$OUTDIR/${1%.xml}.roundtrip.xml"
  if [ ! -f "$TOOL_ICC" ]; then
    echo "[FAIL] $2 -- profile not written (rc=$TOOL_RC)"
    sed -n '1,6p' "$TOOL_LOG"
    status=2
    return
  fi
  if [ -z "${TOXML:-}" ] || [ ! -x "${TOXML:-}" ]; then
    echo "[SKIP] $2 -- iccToXml not available to re-serialize"
    return
  fi
  if ! "$TOXML" "$TOOL_ICC" "$rt" >/dev/null 2>&1; then
    echo "[FAIL] $2 -- iccToXml could not re-serialize the profile"
    status=2
    return
  fi
  if grep -qE "$3" "$rt"; then
    echo "[PASS] $2 -- survives XML -> ICC -> XML"
  else
    echo "[FAIL] $2 -- value did not survive the round trip; got:"
    grep -oE 'SeamlessIndicator="[^"]*"|EncodingFormat="[^"]*"|<ObserverFuncs[^>]*|steps="[0-9]*"' \
      "$rt" | head -4 | sed 's/^/         /'
    status=2
  fi
}

echo "=================================================================="
echo " XML attribute narrowing follow-up (issues #1954, #1931)"
echo " readers outside IccMpeXml.cpp, and the writers coupled to them"
echo "=================================================================="

# --- Case A: profile-header spectral ranges ---------------------------------
#
# steps is the sample count for the whole spectral PCS. 65567 & 0xFFFF == 31,
# which is the donor's real count, so master stores a profile indistinguishable
# from the unmutated one while the document asked for something else. This is
# the header-level source of the very disagreement #1932 had to reject in
# CIccPcsXform::Connect.
if mutate "$FLUOR" hdr-spectral-steps-launder.xml \
     '<Wavelengths start="400" end="700" steps="31"/>' \
     '<Wavelengths start="400" end="700" steps="65567"/>'; then
  expect_reject hdr-spectral-steps-launder.xml \
    'SpectralRange steps="65567"' 'steps="[-0-9]+"'
fi
# Control: 31 is the wrapped value and the donor's own count -- must convert.
if mutate "$FLUOR" hdr-spectral-steps-control.xml \
     '<Wavelengths start="400" end="700" steps="31"/>' \
     '<Wavelengths start="400" end="700" steps="31"/>'; then
  expect_accept hdr-spectral-steps-control.xml 'SpectralRange steps="31" (the wrapped value)'
fi

# The bi-spectral (fluorescence) axis, same reader shape. 65577 & 0xFFFF == 41.
if mutate "$FLUOR" hdr-bispectral-steps-launder.xml \
     '<Wavelengths start="300" end="700" steps="41"/>' \
     '<Wavelengths start="300" end="700" steps="65577"/>'; then
  expect_reject hdr-bispectral-steps-launder.xml \
    'BiSpectralRange steps="65577"' 'steps="[-0-9]+"'
fi
if mutate "$FLUOR" hdr-bispectral-steps-control.xml \
     '<Wavelengths start="300" end="700" steps="41"/>' \
     '<Wavelengths start="300" end="700" steps="41"/>'; then
  expect_accept hdr-bispectral-steps-control.xml 'BiSpectralRange steps="41" (the wrapped value)'
fi

# --- Case B: sparse matrix type ---------------------------------------------
#
# icSparseMatrixType has a fixed underlying type of uint16_t. 65540 & 0xFFFF == 4
# == icSparseMatrixFloat32, a fully supported type, so nothing downstream could
# tell the malformed document from a valid one.
if mutate "$SPARSE" sparse-matrixtype-launder.xml \
     'matrixType="1"' 'matrixType="65540"'; then
  expect_reject sparse-matrixtype-launder.xml \
    'SparseMatrixArray matrixType="65540"' 'matrixType="[-0-9]+"'
fi
if mutate "$SPARSE" sparse-matrixtype-control.xml \
     'matrixType="1"' 'matrixType="4"'; then
  expect_accept sparse-matrixtype-control.xml 'matrixType="4" (the wrapped value)'
fi

# --- Case T: atoi() where atof() was meant ----------------------------------
#
# The ObserverFuncs "end" wavelength is a float16 in nm; the integer parse threw
# the fraction away. Its sibling "start" always used atof(), so this asserts the
# two endpoints of one range finally agree.
if mutate "$FLUOR" observer-end-fraction.xml \
     '<ObserverFuncs start="400" end="700" steps="31">' \
     '<ObserverFuncs start="400" end="702.5" steps="31">'; then
  expect_roundtrip observer-end-fraction.xml \
    'ObserverFuncs end="702.5" keeps its fractional nm' \
    '<ObserverFuncs[^>]*end="702\.5'
fi

# --- Case W: the embedded-image readers and their writers -------------------
#
# The donor is an intentionally-invalid fuzz PoC -- the only tracked document
# with a <HeightImage> -- so these mutations also normalize EncodingFormat to a
# real value (icTiffImageType == 1) where the case is about SeamlessIndicator.
#
# EncodingFormat="-12623518" as tracked: master accepts it, stores 4282343778,
# and its "%d" writer prints it straight back, so the malformed attribute
# survived a full round trip. Both halves are fixed, so it must now be refused.
if mutate "$HEIGHT" heightimage-encformat-negative.xml \
     'EncodingFormat="-12623518"' 'EncodingFormat="-12623518"'; then
  expect_reject heightimage-encformat-negative.xml \
    'HeightImage EncodingFormat="-12623518"' 'EncodingFormat="[-0-9]+"'
fi

# SeamlessIndicator with bit 31 set is representable in its icUInt32Number
# field, so it is a control: it must be ACCEPTED and must come back as itself.
# On master the "%d" writer emitted it negative and the reader's floor-to-0
# swallowed it, so it round-tripped to "0" -- the value was destroyed rather
# than merely misprinted.
if mutate "$HEIGHT" heightimage-seamless-hibit.xml \
     'SeamlessIndicator="-98" EncodingFormat="-12623518"' \
     'SeamlessIndicator="2147483650" EncodingFormat="1"'; then
  expect_roundtrip heightimage-seamless-hibit.xml \
    'HeightImage SeamlessIndicator="2147483650" (bit 31 set)' \
    'SeamlessIndicator="2147483650"'
fi

# --- Case X: the BiSpectralRange writer typo --------------------------------
#
# Assert the stray ')' is gone from freshly written output. Checked against a
# profile iccDEV produces itself, so this fails if the format string regresses.
if mutate "$FLUOR" bispectral-writer-junk.xml \
     '<Wavelengths start="300" end="700" steps="41"/>' \
     '<Wavelengths start="300" end="700" steps="41"/>'; then
  run_tool bispectral-writer-junk.xml
  RT="$OUTDIR/bispectral-writer-junk.roundtrip.xml"
  if [ -f "$TOOL_ICC" ] && [ -n "${TOXML:-}" ] && [ -x "${TOXML:-}" ] && \
     "$TOXML" "$TOOL_ICC" "$RT" >/dev/null 2>&1; then
    if grep -qE '^\)' "$RT"; then
      echo "[FAIL] BiSpectralRange writer emits a stray ')' text node:"
      grep -nE '^\)' "$RT" | head -2 | sed 's/^/         /'
      status=2
    else
      echo "[PASS] BiSpectralRange writer emits no stray ')'"
    fi
  else
    echo "[SKIP] BiSpectralRange writer check -- could not produce round-trip XML"
  fi
fi

echo "------------------------------------------------------------------"
if [ "$status" -eq 0 ]; then
  echo "RESULT: PASS"
else
  echo "RESULT: FAIL"
fi
exit "$status"
