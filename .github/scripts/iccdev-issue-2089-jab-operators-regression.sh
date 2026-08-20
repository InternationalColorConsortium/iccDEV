#!/bin/bash
###############################################################################
# iccDEV tJab / fJab calculator operator functional regression (#2089)
###############################################################################
#
# ICC.2:2023 section 11.2.1.5, Table 94 defines the calculator operators
# 'tJab' (icSigApplyToJabOp) and 'fJab' (icSigApplyFromJabOp), which apply an
# XYZToJabElement / JabToXYZElement sub-element to the top three stack values.
#
# Before this test, nothing in the tree asserted what those two operators
# compute:
#
#   * Testing/CalcTest/calcExercizeOps.xml -- the operator exercise fixture --
#     contains no Jab op at all, and neither does the exercise.txt trace beside
#     it (which nothing diffs in any case).
#   * Testing/CalcTest/calcUnderStack_{tJab,fJab}.icc cover stack underflow only.
#   * Testing/Calc/srgbCalcTest.xml does carry two numeric Jab assertions, but
#     it reports every failure as an all-zero conversion rather than a non-zero
#     exit status, so the verdict was discarded until the sentinel check was
#     added to iccdev-calculator-regression-tests.sh.
#
# This test asserts the two operators directly, so a Jab regression is named as
# such instead of surfacing as "one of 86 assertion verdicts failed".
#
# WHAT IS ASSERTED, AND WHY ONLY THIS:
#
#   1. The achromatic anchor. Feeding the CAM's own white point through tJab
#      must give J=100, a=0, b=0. This is definitional rather than a recorded
#      baseline: J is normalised against the adapted white, so the white point
#      maps to lightness 100 by construction, and a neutral has no chroma.
#   2. The inverse anchor. fJab(100,0,0) must return that same white point.
#   3. Round-trip identity in both directions over non-neutral colours. This is
#      a property of the operator pair, not a recorded value, so it stays valid
#      no matter how the appearance model is parameterised.
#
# Deliberately NOT asserted: absolute J for non-white samples. Those values are
# currently under question -- the CAM computes n = Yb / whitePoint[1] with a
# default Yb of 20.0 against a default white point of icD50XYZ = {0.9642, 1.0,
# 0.8249}, giving n = 20 where CIECAM02 uses n = Yb/Yw = 0.2 -- and pinning
# today's mid-tone output would encode that open question as expected
# behaviour. The anchors and round trips above hold either way.
#
# Every profile and data file used here is generated below, so this test needs
# no Testing/ corpus fixture and therefore no FIXTURES_REQUIRED ordering: it
# cannot be perturbed by CreateAllProfiles.sh regenerating the corpus.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-jab-operators-regression}"
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0:detect_leaks=0}"
export LSAN_OPTIONS="${LSAN_OPTIONS:-detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
APPLYNCM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"

# The CAM white point, repeated in every generated profile below. Kept on the
# 0..1 XYZ scale that icD50XYZ and the rest of the Testing/Calc fixtures use.
WP_X="0.9642"
WP_Y="1.0000"
WP_Z="0.8249"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  echo "  [FAIL] $1 -- $2"
  FAIL=$((FAIL + 1))
}

pass_case() {
  echo "  [PASS] $1"
  PASS=$((PASS + 1))
}

require_tool() {
  if [ ! -x "$1" ]; then
    fail_case "$2" "missing executable: $1"
    return 1
  fi
  return 0
}

check_log() {
  local name="$1"
  local logfile="$2"

  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    fail_case "$name" "AddressSanitizer finding"
    return 1
  fi

  # Mirrors the silence list in iccdev-calculator-regression-tests.sh: MD5 and
  # the libstdc++ headers produce instrumentation noise that is not ours.
  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    if grep "runtime error:" "$logfile" | grep -Ev '(/IccProfLib/IccMD5.cpp:|/include/c\+\+/[^/]+/(bits/(basic_string\.h|basic_string\.tcc|stl_bvector\.h|stl_uninitialized\.h|vector\.tcc)|ext/string_conversions\.h):)' >/dev/null 2>&1; then
      fail_case "$name" "undefined behavior"
      return 1
    fi
  fi

  return 0
}

# Emits a v5 colour-space profile whose A2B1 calculator element runs $1 -- a
# MainFunction body -- over three input channels. Sub-element 0 is XYZToJab and
# sub-element 1 is JabToXYZ, so bodies below address them as tJab(0)/fJab(1).
write_profile_xml() {
  local xml_path="$1"
  local body="$2"

  cat > "$xml_path" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <PreferredCMMType></PreferredCMMType>
    <ProfileVersion>5.0</ProfileVersion>
    <ProfileDeviceClass>spac</ProfileDeviceClass>
    <DataColourSpace>XYZ </DataColourSpace>
    <PCS>XYZ </PCS>
    <CreationDateTime>now</CreationDateTime>
    <ProfileFlags EmbeddedInFile="false" UseWithEmbeddedDataOnly="false"/>
    <DeviceAttributes ReflectiveOrTransparency="reflective" GlossyOrMatte="glossy" MediaPolarity="positive" MediaColour="colour"/>
    <RenderingIntent>Relative</RenderingIntent>
    <PCSIlluminant>
      <XYZNumber X="0.96420288" Y="1.00000000" Z="0.82490540"/>
    </PCSIlluminant>
    <ProfileCreator>ICC </ProfileCreator>
  </Header>
  <Tags>
    <multiLocalizedUnicodeType>
      <TagSignature>desc</TagSignature>
      <LocalizedText LanguageCountry="enUS"><![CDATA[issue 2089 Jab operator probe]]></LocalizedText>
    </multiLocalizedUnicodeType>
    <multiProcessElementType>
      <TagSignature>A2B1</TagSignature>
      <MultiProcessElements InputChannels="3" OutputChannels="3">
        <CalculatorElement InputChannels="3" OutputChannels="3">
          <SubElements>
            <XYZToJabElement InputChannels="3" OutputChannels="3">
              <ColorAppearanceParams>
                <WhitePoint><XYZNumber X="$WP_X" Y="$WP_Y" Z="$WP_Z"/></WhitePoint>
                <Luminance>500.0</Luminance>
                <BackgroundLuminance>20.0</BackgroundLuminance>
                <ImpactSurround>0.69</ImpactSurround>
                <ChromaticInductionFactor>1.00</ChromaticInductionFactor>
                <AdaptationFactor>1.00</AdaptationFactor>
              </ColorAppearanceParams>
            </XYZToJabElement>
            <JabToXYZElement InputChannels="3" OutputChannels="3">
              <ColorAppearanceParams>
                <WhitePoint><XYZNumber X="$WP_X" Y="$WP_Y" Z="$WP_Z"/></WhitePoint>
                <Luminance>500.0</Luminance>
                <BackgroundLuminance>20.0</BackgroundLuminance>
                <ImpactSurround>0.69</ImpactSurround>
                <ChromaticInductionFactor>1.00</ChromaticInductionFactor>
                <AdaptationFactor>1.00</AdaptationFactor>
              </ColorAppearanceParams>
            </JabToXYZElement>
          </SubElements>
          <MainFunction>
{
in(0,3)
$body
out(0,3)
}
          </MainFunction>
        </CalculatorElement>
      </MultiProcessElements>
    </multiProcessElementType>
  </Tags>
</IccProfile>
EOF
}

# Runs $2 (a MainFunction body) over the sample rows in $3 and checks each
# converted triplet against the expected triplet on the same row of $4, to an
# absolute tolerance of $5. $3 and $4 are newline-delimited "x y z" rows rather
# than array names: namerefs would need bash 4.3, and the macOS legs cannot be
# assumed to have anything newer than the 3.2 that ships with the system.
run_case() {
  local name="$1"
  local body="$2"
  local samples="$3"
  local expected="$4"
  local tol="$5"

  local -a samples_ref=()
  local -a expected_ref=()
  local row
  while IFS= read -r row; do
    [ -n "$row" ] && samples_ref+=("$row")
  done <<< "$samples"
  while IFS= read -r row; do
    [ -n "$row" ] && expected_ref+=("$row")
  done <<< "$expected"

  local xml="$OUTDIR/${name}.xml"
  local icc="$OUTDIR/${name}.icc"
  local data="$OUTDIR/${name}-in.txt"
  local fromxml_log="$OUTDIR/${name}-fromxml.log"
  local apply_log="$OUTDIR/${name}-apply.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$xml" "$icc" "$data" "$fromxml_log" "$apply_log"

  write_profile_xml "$xml" "$body"

  {
    printf "'XYZ '\t; Data Format\n"
    printf 'icEncodeFloat\t; Encoding\n\n'
    printf '%s\n' "${samples_ref[@]}"
  } > "$data"

  timeout 30 "$FROMXML" "$xml" "$icc" > "$fromxml_log" 2>&1 || exit_code=$?
  if ! check_log "$name/fromxml" "$fromxml_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ] || [ ! -s "$icc" ]; then
    fail_case "$name" "iccFromXml failed with exit=$exit_code"
    sed -n '1,20p' "$fromxml_log"
    return
  fi

  # Encoding 3 is icEncodeFloat: J runs to 100, so the unit-float encodings
  # would clip the lightness channel and hide exactly what is being measured.
  exit_code=0
  timeout 30 "$APPLYNCM" "$data" 3 0 "$icc" 1 > "$apply_log" 2>&1 || exit_code=$?
  if ! check_log "$name/apply" "$apply_log"; then
    return
  fi
  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "iccApplyNamedCmm failed with exit=$exit_code"
    sed -n '1,20p' "$apply_log"
    return
  fi

  # Converted values sit left of the ';' that echoes the source sample.
  local -a got=()
  local line
  while IFS= read -r line; do
    got+=("$line")
  done < <(sed -n 's/^\(.*\);.*$/\1/p' "$apply_log" | grep -E '[0-9]')

  if [ "${#got[@]}" -ne "${#expected_ref[@]}" ]; then
    fail_case "$name" "expected ${#expected_ref[@]} converted rows, got ${#got[@]}"
    sed -n '1,30p' "$apply_log"
    return
  fi

  local idx=0
  local problems=""
  while [ "$idx" -lt "${#expected_ref[@]}" ]; do
    local verdict
    verdict="$(TOL="$tol" GOT="${got[$idx]}" WANT="${expected_ref[$idx]}" awk 'BEGIN {
      tol = ENVIRON["TOL"] + 0
      ng = split(ENVIRON["GOT"], g, /[ \t]+/)
      nw = split(ENVIRON["WANT"], w, /[ \t]+/)
      gi = 0
      for (i = 1; i <= ng; i++) if (g[i] != "") gv[++gi] = g[i] + 0
      wi = 0
      for (i = 1; i <= nw; i++) if (w[i] != "") wv[++wi] = w[i] + 0
      if (gi != wi) { print "channel count " gi " vs " wi; exit }
      for (i = 1; i <= wi; i++) {
        d = gv[i] - wv[i]
        if (d < 0) d = -d
        if (d > tol) { printf "channel %d: got %.4f want %.4f (delta %.4f > %.4f)", i, gv[i], wv[i], d, tol; exit }
      }
    }')"
    if [ -n "$verdict" ]; then
      problems="${problems}
    row $((idx + 1)) [${samples_ref[$idx]}] -- $verdict"
    fi
    idx=$((idx + 1))
  done

  if [ -n "$problems" ]; then
    fail_case "$name" "value mismatch:$problems"
    return
  fi

  pass_case "$name"
}

require_tool "$FROMXML" "iccFromXml" || true
require_tool "$APPLYNCM" "iccApplyNamedCmm" || true

if [ "$FAIL" -eq 0 ]; then
  # 1. tJab achromatic anchor: the adapted white point is lightness 100 with no
  #    chroma. The 0.05 tolerance absorbs the float rounding in the CAM's
  #    forward/inverse matrix pair; the observed residual is under 0.03.
  run_case "tJab-white-point-is-J100" "tJab(0)" \
    "$WP_X $WP_Y $WP_Z" \
    "100.0 0.0 0.0" \
    0.05

  # 2. fJab inverse anchor: lightness 100 with no chroma is the white point.
  run_case "fJab-J100-is-white-point" "fJab(1)" \
    "100.0 0.0 0.0" \
    "$WP_X $WP_Y $WP_Z" \
    0.001

  # 3. tJab then fJab returns the original XYZ, including for strongly
  #    chromatic samples that the anchors above never reach.
  JAB_XYZ_SAMPLES="$WP_X $WP_Y $WP_Z
0.2000 0.2000 0.2000
0.4000 0.2000 0.1000
0.1000 0.2000 0.4000"
  run_case "tJab-fJab-round-trip-xyz" "tJab(0)
fJab(1)" "$JAB_XYZ_SAMPLES" "$JAB_XYZ_SAMPLES" 0.001

  # 4. And the other way round, so a one-sided break cannot cancel itself out.
  JAB_JAB_SAMPLES="100.0 0.0 0.0
50.0 10.0 -5.0
25.0 -8.0 12.0"
  run_case "fJab-tJab-round-trip-jab" "fJab(1)
tJab(0)" "$JAB_JAB_SAMPLES" "$JAB_JAB_SAMPLES" 0.05
fi

echo "Jab operator regression tests: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

exit 0
