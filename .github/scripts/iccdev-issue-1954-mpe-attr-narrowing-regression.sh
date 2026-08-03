#!/bin/bash
###############################################################################
# iccDEV issues #1954 and #1931
#   atoi() narrowing in the IccMpeXml.cpp MPE element readers, and the
#   "%d"-printed 32-bit fields in the matching writers
###############################################################################
#
# The MPE half of the family #1908 and #1909 fixed at the tag-level readers.
# Every affected site parsed an unsigned attribute with atoi(), which returns a
# signed int, and then stored or cast the result into an icUInt16Number or
# icUInt32Number member:
#
#   m_nFunctionType = atoi(icXmlAttrValue(funcType));
#   m_flags         = atoi(icXmlAttrValue(pNode, "Flags", "0"));
#   m_storageType   = (icUInt16Number)atoi(icXmlAttrValue(attr));
#
# A value that does not fit changed on the way in instead of being refused.
# The reported breadcrumbs are the negative ones -- FunctionType="-143" landing
# as 65393 (#1954) and FunctionType="-1" landing as 65535 (#1931) -- but those
# are also the half a later constraint already caught, because the function-type
# switch rejects anything it does not recognise. The half with teeth in an
# ordinary CI job is the overflow, which lands on a value the switch DOES
# recognise:
#
#   FunctionType="65536"  stored and re-serialized as 0    (a valid function)
#   Reserved2="-1"        stored and re-serialized as 65535
#   Flags="-1"            stored as 0xFFFFFFFF
#
# CASE W -- the writer, and why it had to be fixed in the same change.
# CIccMpeXmlEmissionCLUT::ToXml cast its 32-bit m_flags to unsigned and then
# printed it with "%d", so the cast was a no-op and any profile whose Flags had
# bit 31 set was written out negative. Flags="2147483650" is a legal value that
# Read32() accepts from disk, and master re-serializes it as "-2147483646".
# Pre-fix that still round-tripped, because the reader's atoi() wrapped the
# negative straight back -- two defects cancelling. Fixing only the reader would
# therefore have stopped those profiles loading at all. This case asserts the
# attribute is emitted unsigned AND that the value survives a full
# XML -> ICC -> XML -> ICC trip, which is what pins the two halves together.
#
# Every fixture is a tracked Testing/ document with ONE attribute changed, so
# everything else it exercises is a known-good path. The mutation is applied
# here rather than checked in, and the script FAILS if a mutation did not apply:
# a silently-unmutated fixture would convert cleanly and every assertion below
# would pass while testing nothing.
#
# CONTROLS. Each laundering case is paired with the value it wrapped to, as a
# legal document that must still convert: FunctionType="0", Reserved2="65535",
# Flags="0". That pairing separates "refuses a value that does not fit the
# field" from "lowered the ceiling" -- a fix that rejected both would satisfy
# every rejection assertion here while having broken ordinary profiles.
# Flags="2147483650" is a control in the same sense: it is representable in the
# 32-bit field, so it must still be ACCEPTED, and its stored value checked.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1954}"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi
TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"

status=0

# Donors. Each is a tracked fixture that already exercises the element under test.
CALC=Testing/Calc/RGBWProjector.xml          # <FormulaSegment FunctionType="0">
TONEMAP=Testing/HDR/BT2100HlgFullScene.xml   # <ToneMapFunction FunctionType="0">
ECLUT=Testing/Display/LaserProjector.xml     # <EmissionCLutElement ... Flags="0">

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
    # Only the first occurrence: these donors repeat the same element, and one
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

echo "=================================================================="
echo " MPE XML attribute narrowing regression (issues #1954, #1931)"
echo "=================================================================="

# --- Case A: overflow laundered into a value the later check accepts ---------

# 65536 & 0xFFFF == 0, and function type 0 is "Y = a*X^g" -- a supported type,
# so the switch downstream cannot tell this from a well-formed document.
if mutate "$CALC" formula-ft-launder-1931.xml 'FunctionType="0"' 'FunctionType="65536"'; then
  expect_reject formula-ft-launder-1931.xml \
    'FormulaSegment FunctionType="65536"' 'FunctionType="[-0-9]+"'
fi
# Control: the wrapped value itself is a legal function type and must convert.
if mutate "$CALC" formula-ft-control-1931.xml 'FunctionType="0"' 'FunctionType="0"'; then
  expect_accept formula-ft-control-1931.xml 'FunctionType="0" (the wrapped value)'
fi

# Same element, the 16-bit Reserved2 field: -1 landed as 65535.
if mutate "$CALC" formula-res2-launder-1931.xml \
     '<FormulaSegment Start="-inf" End="0.0" FunctionType="0"' \
     '<FormulaSegment Start="-inf" End="0.0" Reserved2="-1" FunctionType="0"'; then
  expect_reject formula-res2-launder-1931.xml \
    'FormulaSegment Reserved2="-1"' 'Reserved2="[-0-9]+"'
fi
# Control: 65535 is representable in the u16 and must still be accepted.
if mutate "$CALC" formula-res2-control-1931.xml \
     '<FormulaSegment Start="-inf" End="0.0" FunctionType="0"' \
     '<FormulaSegment Start="-inf" End="0.0" Reserved2="65535" FunctionType="0"'; then
  expect_accept formula-res2-control-1931.xml 'Reserved2="65535" (the wrapped value)'
fi

# The tone map function reader, which is the #1954 call site.
#
# The literal below is anchored on "<ToneMapFunction " rather than on
# FunctionType alone, and that matters: this donor carries nine <FormulaSegment>
# elements before its single <ToneMapFunction>, so a bare 'FunctionType="0"'
# substitution lands on the segment at line 29 and re-tests
# CIccFormulaCurveSegmentXml::ParseXml while appearing to cover the tone map.
# The mislabelled version of this case did exactly that -- it was red on master
# for the wrong reader, and IccMpeXml.cpp:1920 was never reached.
if mutate "$TONEMAP" tonemap-ft-launder-1954.xml \
     '<ToneMapFunction FunctionType="0">' '<ToneMapFunction FunctionType="65536">'; then
  expect_reject tonemap-ft-launder-1954.xml \
    'ToneMapFunction FunctionType="65536"' '<ToneMapFunction FunctionType="[-0-9]+"'
fi
if mutate "$TONEMAP" tonemap-ft-control-1954.xml \
     '<ToneMapFunction FunctionType="0">' '<ToneMapFunction FunctionType="0">'; then
  expect_accept tonemap-ft-control-1954.xml 'ToneMapFunction FunctionType="0"'
fi

# The 32-bit spectral CLUT flags. Unlike the cases above this one was accepted
# outright pre-fix: 0xFFFFFFFF is a value the element is willing to hold, so
# nothing downstream objected.
if mutate "$ECLUT" eclut-flags-launder-1931.xml 'Flags="0"' 'Flags="-1"'; then
  expect_reject eclut-flags-launder-1931.xml \
    'EmissionCLutElement Flags="-1"' 'Flags="[-0-9]+"'
fi
if mutate "$ECLUT" eclut-flags-control-1931.xml 'Flags="0"' 'Flags="0"'; then
  expect_accept eclut-flags-control-1931.xml 'EmissionCLutElement Flags="0"'
fi

# --- Case B: the reported breadcrumbs ---------------------------------------
#
# Both negatives wrap to a function type the switch does not recognise, so they
# were already refused pre-fix and these assertions pass either way in an
# ordinary build. They are kept because they are what the issues reported, and
# because on an integer/implicit-conversion build the UBSan diagnostic they used
# to raise is the thing that has to be gone. The log check below is what gives
# them teeth there.
if mutate "$CALC" formula-ft-breadcrumb-1931.xml 'FunctionType="0"' 'FunctionType="-1"'; then
  expect_reject formula-ft-breadcrumb-1931.xml \
    'FormulaSegment FunctionType="-1" (#1931 breadcrumb)' 'FunctionType="[-0-9]+"'
  if grep -q "runtime error:.*implicit conversion" "$OUTDIR/formula-ft-breadcrumb-1931.log" 2>/dev/null; then
    echo "[FAIL] #1931: UBSan implicit-conversion report still raised at the"
    echo "       formula segment reader:"
    grep -m1 "runtime error:" "$OUTDIR/formula-ft-breadcrumb-1931.log" | sed 's/^/         /'
    status=2
  fi
fi
if mutate "$TONEMAP" tonemap-ft-breadcrumb-1954.xml \
     '<ToneMapFunction FunctionType="0">' '<ToneMapFunction FunctionType="-143">'; then
  expect_reject tonemap-ft-breadcrumb-1954.xml \
    'ToneMapFunction FunctionType="-143" (#1954 breadcrumb)' '<ToneMapFunction FunctionType="[-0-9]+"'
  if grep -q "runtime error:.*implicit conversion" "$OUTDIR/tonemap-ft-breadcrumb-1954.log" 2>/dev/null; then
    echo "[FAIL] #1954: UBSan implicit-conversion report still raised at the"
    echo "       tone map reader:"
    grep -m1 "runtime error:" "$OUTDIR/tonemap-ft-breadcrumb-1954.log" | sed 's/^/         /'
    status=2
  fi
fi

# --- Case W: the writer, and the reader/writer coupling ---------------------
#
# Flags="2147483650" (0x80000002) is legal: Read32() accepts the whole 32-bit
# range from disk. Pre-fix the tool ACCEPTED it -- atoi() overflowed to a
# negative that wrapped back to the right number by accident -- and then wrote
# it back out as "-2147483646". So the assertion that has teeth here is not
# acceptance but what the writer emits, and whether the value survives a second
# trip through the parser now that negatives are refused.
if [ -z "${TOXML:-}" ] || [ ! -x "${TOXML:-}" ]; then
  echo "[SKIP] iccToXml not found; writer round-trip case not run"
elif mutate "$ECLUT" eclut-flags-hibit-1931.xml 'Flags="0"' 'Flags="2147483650"'; then
  expect_accept eclut-flags-hibit-1931.xml 'EmissionCLutElement Flags="2147483650" (representable)'
  run_tool eclut-flags-hibit-1931.xml
  if [ ! -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1931: a representable 32-bit Flags was refused; the writer"
    echo "       case below cannot run"
    status=2
  else
    RT_XML="$OUTDIR/eclut-flags-hibit-1931.rt.xml"
    RT_ICC="$OUTDIR/eclut-flags-hibit-1931.rt.icc"
    if ! "$TOXML" "$TOOL_ICC" "$RT_XML" >/dev/null 2>&1; then
      echo "[FAIL] #1931: iccToXml could not re-serialize the profile"
      status=2
    else
      EMITTED="$(grep -oE '<EmissionCLutElement[^>]*Flags="[-0-9]+"' "$RT_XML" \
                 | head -1 | grep -oE 'Flags="[-0-9]+"')"
      if [ "$EMITTED" = 'Flags="2147483650"' ]; then
        echo "[PASS] writer emits $EMITTED unsigned"
      else
        echo "[FAIL] #1931: writer emitted $EMITTED for a 32-bit Flags of"
        echo "       0x80000002 -- '%d' is reinterpreting the field as signed,"
        echo "       and the parser now refuses the negative it produces, so"
        echo "       such profiles no longer round-trip"
        status=2
      fi
      # Second trip: the emitted attribute has to be readable again. Pre-fix
      # this worked only because a negative attribute wrapped back; post-fix it
      # has to work because the attribute is correct.
      rm -f "$RT_ICC"
      "$FROMXML" "$RT_XML" "$RT_ICC" -noid >/dev/null 2>&1
      if [ ! -f "$RT_ICC" ]; then
        echo "[FAIL] #1931: the writer's own output no longer parses -- the"
        echo "       reader and writer halves of this fix disagree"
        status=2
      else
        FLAGS_HEX="$(python3 - "$RT_ICC" <<'PYEOF'
import sys
d = open(sys.argv[1], 'rb').read()
o = d.find(b'eclt')
# Element header: sig(4) reserved(4) inChannels(2) outChannels(2) then flags(4).
print(f"0x{int.from_bytes(d[o+12:o+16],'big'):08x}" if o >= 0 else "no-eclt")
PYEOF
)"
        if [ "$FLAGS_HEX" = "0x80000002" ]; then
          echo "[PASS] round-trip preserves eclt flags $FLAGS_HEX"
        else
          echo "[FAIL] #1931: eclt flags came back as $FLAGS_HEX, expected 0x80000002"
          status=2
        fi
      fi
      rm -f "$RT_ICC"
    fi
    rm -f "$TOOL_ICC"
  fi
fi

echo "------------------------------------------------------------------"
if [ $status -eq 0 ]; then
  echo "[OK] MPE XML attribute narrowing regression: all cases pass"
else
  echo "[REGRESSION] MPE XML attribute narrowing: see failures above"
fi
exit $status
