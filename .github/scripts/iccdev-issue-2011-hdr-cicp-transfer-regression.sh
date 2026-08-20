#!/bin/bash
###############################################################################
# iccDEV issue #2011
#   BT.2100 fixtures must declare the cicp TransferCharacteristics that matches
#   the transfer function they actually encode
###############################################################################
#
# All four Testing/HDR/BT2100PQ*.xml declared TransferCharacteristics="18" while
# carrying the SMPTE ST 2084 constants in their FormulaSegments.  Per ITU-T
# H.273 Table 3, 18 is ARIB STD-B67 (HLG); ST 2084 (PQ) is 16.  The curve maths
# was right and the label was wrong, so the fixtures were internally
# inconsistent -- and these files exist to be read as worked examples of a PQ
# profile's cicpTag, which is exactly the field that lied.
#
# The cicp code point was not the only claim that was wrong.  Three of the four
# also carried "BT.2100 HLG ..." in their profileDescriptionTag.  Both fields
# were copied from the HLG siblings the PQ fixtures were derived from, which is
# what makes them one defect rather than two: the same wrong answer in the field
# a CMM reads and in the field a person reads.  The two are asserted separately
# below, because they did not break together -- BT2100PQFullScene.xml had a
# correct description and a wrong code point, and it is the file the CI
# regressions happen to use, which is part of why this survived.
#
# Nothing in iccDEV noticed, and the reason is narrower than "no cross-check":
# CIccTagCicp::Validate (IccProfLib/IccTagBasic.cpp) reads m_nMatrixCoefficients
# against the profile's colour space and nothing else.  It never inspects
# m_nTransferCharacteristics at all, at any value.  So all ten Testing/HDR
# fixtures reported "Profile is valid for version 5.10" before the correction
# and report it after, with byte-identical validation output.  A range check
# would not have helped either: 18 is a perfectly legal code point, just not
# this profile's.  Only a consistency check catches this shape, and that check
# does not exist in the library -- so this test supplies it for the corpus.
#
# WHAT IS ASSERTED.  For every tracked XML carrying a recognisable BT.2100
# transfer function, both of its claims about that curve must agree with the
# constants present in the document:
#
#   ST 2084 (PQ)      m1 = 0.1593017578125  (2610/16384)  -> 16, "BT.2100 PQ"
#   ARIB STD-B67 HLG  a  = 0.17883277                     -> 18, "BT.2100 HLG"
#
# The classification keys on the curve constants rather than on the file name,
# so a fixture renamed or copied under a misleading name is still judged by what
# it encodes -- which is the whole point, since the file names here were the one
# thing that was right.  Files carrying neither constant family are skipped
# rather than failed: this test pins BT.2100 fixtures, not every profile with a
# cicpTag.  Files carrying both are a failure -- that is an ambiguous document,
# not a passing one.  A fixture with no cicpTag is still description-checked;
# the two SceneToDisplayLink fixtures are that case.
#
# ROUND TRIP.  Where iccFromXml accepts the document, the corrected value is
# also asserted after XML -> ICC -> XML.  A cicp code point that were dropped or
# narrowed on the way through would satisfy a text-only check while the profile
# a consumer actually opens still carried the wrong number.
#
# CONTROLS.  A text check that stopped matching -- a constant reformatted, an
# attribute or a description reworded -- would pass silently on every fixture
# forever, so each half of the defect is reintroduced into a copy and must be
# caught: one copy with TransferCharacteristics put back to 18, one described
# again as "BT.2100 HLG".  They are separate because the two claims broke
# independently.  If either control is reported consistent, the assertions above
# are not testing anything and this script exits non-zero on that basis alone.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2011}"
mkdir -p "$OUTDIR"

# The tools are optional here.  The core assertion is about document content and
# needs nothing built; only the round-trip half is skipped when they are absent,
# which keeps this test useful in a checkout-only job.
FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"

status=0

# ITU-T H.273 Table 3 code points, and the constant that identifies each curve.
# The PQ probe is m1 = 2610/16384 and the HLG probe is the ARIB STD-B67 'a'.
# Both are long enough to be unambiguous and appear in every fixture of their
# family, in the forward transfer function and again in its inverse.
PQ_CONST='0\.1593017578125'
HLG_CONST='0\.17883277'
PQ_TC=16
HLG_TC=18

# classify <file>
# Echoes the TransferCharacteristics the document's curve content requires, or
# an empty string when the file is not a BT.2100 fixture.  Echoes "AMBIGUOUS"
# when a document somehow carries both families.
classify() {
  local f="$1" pq hlg
  pq=$(grep -c "$PQ_CONST" "$f" 2>/dev/null || true)
  hlg=$(grep -c "$HLG_CONST" "$f" 2>/dev/null || true)
  if [ "$pq" -gt 0 ] && [ "$hlg" -gt 0 ]; then echo "AMBIGUOUS"
  elif [ "$pq" -gt 0 ]; then echo "$PQ_TC"
  elif [ "$hlg" -gt 0 ]; then echo "$HLG_TC"
  else echo ""
  fi
}

# declared_tc <file>
# Echoes the TransferCharacteristics attribute value, or "" when there is no
# cicpFields element at all.  The two SceneToDisplayLink fixtures are BT.2100
# curves with no cicpTag, and that is legitimate -- a link profile has no device
# encoding to describe -- so they land here and are skipped, not failed.
declared_tc() {
  sed -n 's/.*<cicpFields[^>]*TransferCharacteristics="\([0-9]*\)".*/\1/p' "$1" 2>/dev/null | head -1
}

# declared_curve_name <file>
# Echoes "PQ" or "HLG" as named in the profileDescriptionTag, or "" when the
# description names neither.  The cicp code point was not the only place these
# fixtures claimed to be HLG: three of the four PQ files also carried an HLG
# profileDescriptionTag, copied wholesale from their HLG siblings, which is the
# same defect in the field a human reads instead of the field a CMM reads.
# The corpus spells the same thing several ways -- "BT.2100 PQ ...",
# "Rec. 2100 RGB with Hlg", "Algorithmic Rec. 2100 RGB with HLG" -- so the
# curve name is matched as a standalone word anywhere in the description rather
# than against one phrasing.  A description naming both, or neither, yields ""
# and is not judged: only an unambiguous claim can be wrong.
declared_curve_name() {
  local d hlg pq
  d=$(sed -n 's/.*CDATA\[\([^]]*\)\].*/\1/p' "$1" 2>/dev/null | head -1)
  hlg=$(printf '%s' "$d" | grep -ciE '(^|[^A-Za-z])hlg([^A-Za-z]|$)' || true)
  pq=$(printf '%s' "$d" | grep -ciE '(^|[^A-Za-z])pq([^A-Za-z]|$)' || true)
  if [ "$hlg" -gt 0 ] && [ "$pq" -eq 0 ]; then echo "HLG"
  elif [ "$pq" -gt 0 ] && [ "$hlg" -eq 0 ]; then echo "PQ"
  else echo ""
  fi
}

# check_one <label> <file> -- returns 0 when consistent, 1 when it is not,
# and 0 with a [SKIP] line when the file is out of scope.
check_one() {
  local label="$1" f="$2" want got name wantname rc=0
  want=$(classify "$f")
  got=$(declared_tc "$f")

  if [ "$want" = "AMBIGUOUS" ]; then
    echo "[FAIL] $label -- carries both ST 2084 and ARIB STD-B67 constants"
    return 1
  fi
  if [ -z "$want" ]; then
    echo "[SKIP] $label -- no BT.2100 transfer function"
    return 0
  fi

  # The human-readable claim and the machine-readable one are checked against
  # the same curve content, so a fixture cannot satisfy one by breaking the
  # other. The description is checked even where there is no cicpTag: the two
  # link fixtures name their curve too.
  if [ "$want" = "$PQ_TC" ]; then wantname=PQ; else wantname=HLG; fi
  name=$(declared_curve_name "$f")
  if [ -n "$name" ] && [ "$name" != "$wantname" ]; then
    echo "[FAIL] $label -- profileDescriptionTag says $name, curve content is $wantname"
    rc=1
  fi

  if [ -z "$got" ]; then
    # No cicpTag is legitimate here -- a link profile has no device encoding to
    # describe. Say which half ran so the log does not read as no coverage.
    if [ "$rc" -eq 0 ]; then
      if [ -n "$name" ]; then
        echo "[PASS] $label -- no cicpTag; description names $name, matching the encoded curve"
      else
        echo "[SKIP] $label -- no cicpTag, and the description names neither curve"
      fi
    fi
    return "$rc"
  fi
  if [ "$got" != "$want" ]; then
    echo "[FAIL] $label -- declares TransferCharacteristics=$got, curve content is $want"
    return 1
  fi
  [ "$rc" -eq 0 ] && echo "[PASS] $label -- TransferCharacteristics=$got and the description both match the encoded curve"
  return "$rc"
}

echo "=================================================================="
echo " BT.2100 cicp TransferCharacteristics consistency (issue #2011)"
echo "=================================================================="

# Both roots that carry cicp-bearing XML today: the HDR fixtures themselves and
# the CI test-data copy of one of them.  Enumerated by content rather than by a
# hard-coded list so a fixture added later is covered without editing this file.
# Read into an array rather than a whitespace-split string: nothing in the tree
# has a space in its path today, but that is an assumption about the checkout
# directory as much as about the repo, and it does not need to be made.
CANDIDATES=()
while IFS= read -r _f; do
  CANDIDATES+=("$_f")
done < <(find "$REPO_ROOT/Testing" "$REPO_ROOT/.github/ci/test-data" \
           -name '*.xml' -type f 2>/dev/null | sort)
if [ "${#CANDIDATES[@]}" -eq 0 ]; then
  echo "[SKIP] no XML fixtures found under Testing/ or .github/ci/test-data/"
  exit 0
fi

checked=0
for f in "${CANDIDATES[@]}"; do
  # Scope is "carries a BT.2100 transfer function", not "carries a cicpTag":
  # the two link fixtures have no cicpTag but do name their curve in the
  # description, and that claim is checked as well.  Everything else would
  # produce the same skip line and drown the log.
  [ -n "$(classify "$f")" ] || continue
  rel="${f#"$REPO_ROOT"/}"
  check_one "$rel" "$f" || status=2
  checked=$((checked + 1))
done

if [ "$checked" -eq 0 ]; then
  echo "[FAIL] no BT.2100 fixture was found -- the corpus moved, or the curve"
  echo "       constants were reformatted, and this test is looking at nothing"
  status=2
fi
echo "------------------------------------------------------------------"
echo "Checked $checked BT.2100 fixture(s)"

# --- round trip -------------------------------------------------------------
# The value has to survive conversion, not just sit in the document.  Anything
# iccFromXml refuses is reported and skipped rather than failed: this test is
# not the acceptance test for those documents, and .github/ci/test-data holds
# deliberately-invalid PoCs that are meant to be refused.

if [ -n "$FROMXML" ] && [ -x "$FROMXML" ] && [ -n "$TOXML" ] && [ -x "$TOXML" ]; then
  echo ""
  echo "--- XML -> ICC -> XML ---"
  for f in "${CANDIDATES[@]}"; do
    grep -q '<cicpFields' "$f" 2>/dev/null || continue
    want=$(classify "$f")
    if [ -z "$want" ] || [ "$want" = "AMBIGUOUS" ]; then continue; fi
    rel="${f#"$REPO_ROOT"/}"
    base="$(basename "$f" .xml)"
    if ! "$FROMXML" "$f" "$OUTDIR/$base.icc" > "$OUTDIR/$base.fromxml.log" 2>&1 \
       || [ ! -s "$OUTDIR/$base.icc" ]; then
      echo "[SKIP] $rel -- iccFromXml did not produce a profile (see $base.fromxml.log)"
      continue
    fi
    if ! "$TOXML" "$OUTDIR/$base.icc" "$OUTDIR/$base.rt.xml" > "$OUTDIR/$base.toxml.log" 2>&1 \
       || [ ! -s "$OUTDIR/$base.rt.xml" ]; then
      echo "[FAIL] $rel -- iccToXml did not re-serialize the profile"
      status=2
      continue
    fi
    rt=$(declared_tc "$OUTDIR/$base.rt.xml")
    if [ "$rt" != "$want" ]; then
      echo "[FAIL] $rel -- round-tripped as TransferCharacteristics=${rt:-<absent>}, expected $want"
      status=2
    else
      echo "[PASS] $rel -- TransferCharacteristics=$rt survives XML -> ICC -> XML"
    fi
  done
else
  echo ""
  echo "[SKIP] round trip -- iccFromXml / iccToXml not found under $TOOLS_DIR"
fi

# --- control ----------------------------------------------------------------
# Put the defect back into a copy and require the classifier to catch it.  A
# check whose grep stopped matching -- a constant reformatted, an attribute
# renamed -- would otherwise report [PASS] on every fixture forever.

echo ""
echo "--- control: the defect, reintroduced ---"
CONTROL_SRC="$REPO_ROOT/Testing/HDR/BT2100PQFullDisplay.xml"
if [ ! -f "$CONTROL_SRC" ]; then
  echo "[FAIL] control donor Testing/HDR/BT2100PQFullDisplay.xml is missing"
  status=2
else
  # Control 1: the cicp code point put back to 18.
  CONTROL="$OUTDIR/control-pq-cicp-2011.xml"
  sed 's/\(<cicpFields[^>]*\)TransferCharacteristics="16"/\1TransferCharacteristics="18"/' \
    "$CONTROL_SRC" > "$CONTROL"
  if [ "$(declared_tc "$CONTROL")" != "18" ]; then
    echo "[FAIL] control -- the cicp mutation did not apply, so it proves nothing"
    status=2
  elif check_one "control" "$CONTROL" > /dev/null 2>&1; then
    echo "[FAIL] control -- a PQ fixture declaring 18 was reported consistent;"
    echo "       the cicp assertions above are not testing anything"
    status=2
  else
    echo "[PASS] control -- a PQ fixture declaring TransferCharacteristics=18 is caught"
  fi

  # Control 2: the description put back to HLG.  Checked separately because the
  # two claims are read by different audiences and broke independently -- one
  # PQ fixture had a correct description and a wrong cicp code point.
  CONTROL2="$OUTDIR/control-pq-desc-2011.xml"
  sed 's/BT\.2100 PQ /BT.2100 HLG /' "$CONTROL_SRC" > "$CONTROL2"
  if [ "$(declared_curve_name "$CONTROL2")" != "HLG" ]; then
    echo "[FAIL] control -- the description mutation did not apply, so it proves nothing"
    status=2
  elif check_one "control" "$CONTROL2" > /dev/null 2>&1; then
    echo "[FAIL] control -- a PQ fixture described as HLG was reported consistent;"
    echo "       the description assertions above are not testing anything"
    status=2
  else
    echo "[PASS] control -- a PQ fixture described as 'BT.2100 HLG' is caught"
  fi
fi

echo "------------------------------------------------------------------"
if [ "$status" -eq 0 ]; then
  echo "RESULT: PASS"
else
  echo "RESULT: FAIL"
fi
exit "$status"
