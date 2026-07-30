#!/bin/bash
###############################################################################
# iccDEV issue #1909
#   (icUInt8Number)/(icUInt16Number) icXmlAttrToUInt() narrowing family in
#   IccTagXml.cpp -- buckets 1 and 2
###############################################################################
#
# The follow-up surface of #1908.  Tag-level readers parsed an XML attribute
# into a wider integer and then narrowed it with an explicit cast:
#
#   m_nColorPrimaries = (icUInt8Number)icXmlAttrToUInt(icXmlAttrValue(attr));
#   m_spectralRange.steps = (icUInt16Number)icXmlAttrToUInt(...);
#   nRows = (icUInt16Number)icXmlAttrToUInt(icXmlAttrValue(rows));
#
# Being explicit, the cast also suppresses UBSan's implicit-integer-truncation
# check, so nothing reported it.  The document was accepted, the tool exited 0,
# and the profile written to disk carried a value the document never contained:
#
#   ColorPrimaries="265"    stored and re-serialized as 9
#   steps="65937"           stored and re-serialized as 401
#   channels="65539"        stored and re-serialized as 3
#   rows="65567"            stored and re-serialized as 31
#   outputChannels="66304"  stored and re-serialized as 768
#   GridGranularity="258"   produced a profile byte-identical to "2"
#
# Every case is the tracked fixture named beside it with ONE attribute changed,
# so everything else each document exercises is a known-good path.  The mutation
# is applied here rather than checked in, and the script FAILS if a mutation did
# not apply -- a silently-unmutated fixture would convert cleanly and every
# assertion below would pass while testing nothing.
#
# CONTROLS.  Each laundering case is paired with the wrapped value itself as a
# legal document: 9, 401, 3, 31, 768, 2.  These must still convert.  That pairing
# is what separates "refuses a value that does not fit the field" from "lowered
# the ceiling": a fix that rejected both would satisfy every rejection assertion
# here while having broken ordinary profiles.
#
# CRASH.  One case is not laundering.  CIccSparseMatrix::Init() enforces its own
# dimension ceiling and returns false having left the matrix empty --
# GetRowStart() NULL, GetMaxEntries() 0.  CIccTagXmlSparseMatrixArray::ParseXml
# discarded that result, so a <SparseMatrix> declaring more rows than Init()
# accepts and carrying no <SparseRow> children fell through to the row-start fill
# and wrote through the NULL pointer:
#
#   Program received signal SIGSEGV, Segmentation fault.
#   0x... in CIccTagXmlSparseMatrixArray::ParseXml(_xmlNode*, std::string&)
#   $1 = (void *) 0x0
#
# That one asserts on the exit status, not on the absence of an output file: a
# rejection and a crash both leave no profile behind, so only the status tells
# them apart.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1909}"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi
TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"

status=0

# mutate <donor-relative-path> <output-name> <find> <replace> [<find> <replace>]...
#
# Applies literal substitutions to a tracked fixture and writes the result to
# OUTDIR. Refuses to write a fixture a substitution did not change, which is what
# keeps a donor edited upstream from turning this test into a silent no-op.
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
    # Only the first occurrence: several donors repeat the same element, and one
    # changed attribute is enough to reach the reader under test.
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
    "$FROMXML" "$xml" "$TOOL_ICC" > "$TOOL_LOG" 2>&1
  TOOL_RC=$?
}

# A laundering case: the document must be refused, and no profile written.
# $1 = fixture  $2 = description  $3 = grep pattern showing the value that landed
expect_reject() {
  run_tool "$1"
  if [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1909: $2 -- profile written (rc=$TOOL_RC)"
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

# A control: the wrapped value is representable and must still be accepted.
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

echo "=================================================================="
echo " XML attribute narrowing regression (issue #1909)"
echo "=================================================================="

# --- bucket 1: 8-bit fields -------------------------------------------------

# cicp code points are 8-bit syntax elements in ITU-T H.273. 265 & 0xFF == 9,
# which is BT.2020 -- a different and entirely valid set of primaries.
CICP=Testing/HDR/BT2100PQFullScene.xml
if mutate "$CICP" cicp-launder-1909.xml 'ColorPrimaries="9"' 'ColorPrimaries="265"'; then
  expect_reject cicp-launder-1909.xml 'cicp ColorPrimaries="265"' 'ColorPrimaries="[0-9]+"'
fi
# The donor already carries ColorPrimaries="9", the wrapped value, so it is its
# own control -- copied through mutate so donor drift is caught here as well.
if mutate "$CICP" cicp-control-1909.xml 'ColorPrimaries="9"' 'ColorPrimaries="9"'; then
  expect_accept cicp-control-1909.xml 'ColorPrimaries="9" (the wrapped value)'
fi
# Negative code points are refused rather than floored to 0. #1346 floored them,
# which kept the arithmetic well-defined but still wrote a value -- 0, meaning
# BT.709 primaries -- that the document did not ask for.
if mutate "$CICP" cicp-negative-1909.xml 'ColorPrimaries="9"' 'ColorPrimaries="-1"'; then
  expect_reject cicp-negative-1909.xml 'cicp ColorPrimaries="-1"' 'ColorPrimaries="[0-9]+"'
fi

# A CLUT holds one grid point count per input channel in an icUInt8Number.
# 258 & 0xFF == 2, and Init(2) is a perfectly ordinary grid, so pre-fix this
# produced a profile byte-identical to the one GridGranularity="2" gives. The
# GridPoints element has to go: the reader only consults GridGranularity when
# no explicit grid is present.
CLUT=Testing/CalcTest/calcExercizeOps.xml
CLUT_OPEN='<CLutElement Name="clutElem" InputChannels="2" OutputChannels="2">'
CLUT_GRID='<GridPoints>2 2</GridPoints>'
if mutate "$CLUT" gridgran-launder-1909.xml \
     "$CLUT_OPEN" '<CLutElement Name="clutElem" InputChannels="2" OutputChannels="2" GridGranularity="258">' \
     "$CLUT_GRID" ''; then
  expect_reject gridgran-launder-1909.xml 'CLUT GridGranularity="258"' 'GridPoints>[0-9 ]+<'
fi
if mutate "$CLUT" gridgran-control-1909.xml \
     "$CLUT_OPEN" '<CLutElement Name="clutElem" InputChannels="2" OutputChannels="2" GridGranularity="2">' \
     "$CLUT_GRID" ''; then
  expect_accept gridgran-control-1909.xml 'GridGranularity="2" (the wrapped value)'
fi

# --- bucket 2: 16-bit counts ------------------------------------------------

# Tag-level spectral step counts, the tag-side siblings of the MPE step counts
# #1908 fixed. 65937 & 0xFFFF == 401, the donor's own step count.
SDIN=Testing/Display/Rec2020rgbSpectral.xml
SDIN_WL='<Wavelengths start="380.000000" end="780.000000" steps="401"/>'
if mutate "$SDIN" sdin-launder-1909.xml \
     "$SDIN_WL" '<Wavelengths start="380.000000" end="780.000000" steps="65937"/>'; then
  expect_reject sdin-launder-1909.xml 'spectralDataInfo steps="65937"' 'steps="[0-9]+"'
fi
if mutate "$SDIN" sdin-control-1909.xml "$SDIN_WL" "$SDIN_WL"; then
  expect_accept sdin-control-1909.xml 'steps="401" (the wrapped value)'
fi

# GamutBoundaryDesc channel counts divide the parsed value list into vertices,
# so a wrapped count yields a different geometry rather than an error.
# 65539 & 0xFFFF == 3.
GBD=Testing/Display/sRGB_D65_MAT.xml
if mutate "$GBD" gbd-launder-1909.xml '<PCSValues channels="3"' '<PCSValues channels="65539"'; then
  expect_reject gbd-launder-1909.xml 'GamutBoundaryDesc PCSValues channels="65539"' 'PCSValues channels="[0-9]+"'
fi
if mutate "$GBD" gbd-control-1909.xml '<PCSValues channels="3"' '<PCSValues channels="3"'; then
  expect_accept gbd-control-1909.xml 'PCSValues channels="3" (the wrapped value)'
fi

# SparseMatrix dimensions and the array's own channel count.
# 65567 & 0xFFFF == 31; 66304 & 0xFFFF == 768.
SPM=Testing/Named/SparseMatrixNamedColor.xml
SPM_FULL='<FullMatrix rows="31" cols="41">'
if mutate "$SPM" spm-rows-launder-1909.xml "$SPM_FULL" '<FullMatrix rows="65567" cols="41">'; then
  expect_reject spm-rows-launder-1909.xml 'FullMatrix rows="65567"' 'rows="[0-9]+" cols="[0-9]+"'
fi
if mutate "$SPM" spm-chan-launder-1909.xml \
     '<SparseMatrixArray outputChannels="768"' '<SparseMatrixArray outputChannels="66304"'; then
  expect_reject spm-chan-launder-1909.xml 'SparseMatrixArray outputChannels="66304"' 'outputChannels="[0-9]+"'
fi
# The unmutated donor controls both of the above at once: it carries rows="31"
# and outputChannels="768", which are exactly the wrapped values.
if mutate "$SPM" spm-control-1909.xml "$SPM_FULL" "$SPM_FULL"; then
  expect_accept spm-control-1909.xml 'rows="31" / outputChannels="768" (the wrapped values)'
fi

# --- the NULL row-start write ------------------------------------------------
# One FullMatrix element is preceded by an empty SparseMatrix declaring more rows
# than CIccSparseMatrix::Init() accepts. Pre-fix this reached the row-start fill
# with a NULL pointer; a rejection and a segfault both leave no profile behind,
# so this case keys on the exit status.
if mutate "$SPM" spm-nullrow-1909.xml \
     "$SPM_FULL" '<SparseMatrix rows="5000" cols="41"></SparseMatrix><FullMatrix rows="31" cols="41">'; then
  run_tool spm-nullrow-1909.xml
  if [ "$TOOL_RC" -ge 128 ]; then
    echo "[FAIL] #1909: unchecked CIccSparseMatrix::Init() -- iccFromXml died on"
    echo "       signal $((TOOL_RC - 128)) parsing an over-large <SparseMatrix>"
    sed -n '1,6p' "$TOOL_LOG"
    status=2
  elif [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1909: over-large <SparseMatrix> dimensions produced a profile (rc=$TOOL_RC)"
    status=2
  else
    echo "[PASS] over-large <SparseMatrix> rows refused without a crash (rc=$TOOL_RC)"
  fi
  rm -f "$TOOL_ICC"
fi

# Honouring Init() also refuses an array whose outputChannels is too small to
# hold its own declared row count -- Init() sizes the row-start table out of
# GetBytesPerMatrix(), so the two are coupled. Pre-fix this converted with exit 0
# while the tool itself printed "Profile is invalid, but saved correctly", and
# the matrix was absent from the profile it wrote. Pinned here because it is a
# deliberate acceptance change, not a side effect: rows="31" needs 132 bytes of
# row-start table, which outputChannels="32" cannot provide.
if mutate "$SPM" spm-undersized-1909.xml \
     '<SparseMatrixArray outputChannels="768"' '<SparseMatrixArray outputChannels="32"'; then
  expect_reject spm-undersized-1909.xml \
    'SparseMatrixArray outputChannels="32" too small for rows="31"' 'outputChannels="[0-9]+"'
fi

echo "------------------------------------------------------------------"
if [ "$status" -ne 0 ]; then
  echo "RESULT: FAIL"
else
  echo "RESULT: PASS"
fi
exit "$status"
