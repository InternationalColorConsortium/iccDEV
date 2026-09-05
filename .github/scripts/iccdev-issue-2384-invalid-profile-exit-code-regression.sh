#!/bin/bash
###############################################################################
# #2384 -- iccFromXml / iccFromJson reported success for a profile they had
#          just declared invalid
###############################################################################
#
# Both converters validate the profile they built and then take one of two
# branches.  The above-warning branch printed
#
#   Profile parsed.  Profile is invalid, but saved correctly
#
# followed by the whole validation report, ON STDOUT, and then fell through to
# "return EXIT_SUCCESS".  So an invalid profile was indistinguishable from a
# conformant one to any caller that checks $?, and a caller that separates the
# streams saw a silent stderr as well.  #2384 found one reaching three TIFFs and
# package generation that way.
#
# The contract applied here is the one this tree already uses for every other
# failing exit in these two main()s, and which #1514/#2387 settled for the
# usage paths:
#
#   nothing was produced, or what was produced is invalid
#                                -> diagnostics on STDERR, non-zero status
#   a conformant profile         -> message on STDOUT, exit 0, stderr empty
#
# What deliberately does NOT change: the invalid profile is still WRITTEN.  That
# is what lets a caller inspect or repair it, and this tree's own #1898/#1901/
# #1902/#1845 fixtures depend on the artifact appearing -- they gate on the file
# existing and never on $?, which is why they still pass.  Every "invalid" case
# below therefore asserts the file IS there as well as that the status is
# non-zero; a fix that simply refused to write would satisfy the status half and
# break five fixtures.
#
# Red/green, measured against master 2fd99a03: 4 of the 6 cases below fail.
# Both "invalid" cases fail twice over (exit 0, and the report on stdout), and
# both "parse-failure" cases fail on the stream alone -- those already returned
# EXIT_FAILURE but answered on stdout.  The two "valid" controls PASS against
# master and must keep passing: they are what separates "reports invalid
# profiles" from "started failing on everything", which a status-only test on
# the invalid cases alone could not tell apart.
#
# Scope note: #2384 also reports the SpectralEncoding BuildAndTest.sh/.bat
# wrappers converting a non-zero iccTiffDump into a warning.  Those live in the
# ICS-POC repository, not here; the in-tree Testing/hybrid/BuildAndTest.sh does
# not mask, and runs under "set -eu" against 18 XML documents that all validate
# clean, so it is unaffected by this change.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-invalid-profile-exit-code-regression}"
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

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"

STDOUT_LOG="$OUTDIR/invalid-profile.stdout.log"
STDERR_LOG="$OUTDIR/invalid-profile.stderr.log"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

fail_case() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
pass_case() { echo "  [PASS] $1 -- $2"; PASS=$((PASS + 1)); }
skip_case() { echo "  [SKIP] $1 -- $2"; SKIP=$((SKIP + 1)); }

check_sanitizers() {
  local name="$1" log="$2"
  if grep -Eq "ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in tool output"
    return 1
  fi
  return 0
}

###############################################################################
# Fixtures.  A minimal but complete v4 GRAY profile, and the same document with
# mediaWhitePointTag removed -- which CIccProfile::Validate reports as
# "Error! - Media white point tag missing." (above icValidateWarning) while the
# document still parses, so the tool reaches the branch under test rather than
# failing earlier.
###############################################################################
cat > "$OUTDIR/valid.xml" <<'XMLEOF'
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <ProfileVersion>4.30</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
    <RenderingIntent>Perceptual</RenderingIntent>
  </Header>
  <Tags>
    <grayTRCTag> <curveType><Curve>563</Curve></curveType> </grayTRCTag>
    <profileDescriptionTag> <textDescriptionType><TextData>2384 CLI contract fixture</TextData></textDescriptionType> </profileDescriptionTag>
    <copyrightTag> <textType><TextData>ICC regression fixture</TextData></textType> </copyrightTag>
    <mediaWhitePointTag> <XYZArrayType><XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/></XYZArrayType> </mediaWhitePointTag>
  </Tags>
</IccProfile>
XMLEOF

# The same profile without mediaWhitePointTag.
grep -v 'mediaWhitePointTag' "$OUTDIR/valid.xml" > "$OUTDIR/invalid.xml"

# Well-formed XML that is not a profile: LoadXml fails, so this exercises the
# parse-failure exit rather than the validation one.
cat > "$OUTDIR/unparseable.xml" <<'XMLEOF'
<?xml version="1.0" encoding="UTF-8"?>
<NotAProfile><Nothing/></NotAProfile>
XMLEOF

cat > "$OUTDIR/valid.json" <<'JSONEOF'
{
  "IccProfile": {
    "Header": {
      "ProfileVersion": "4.30",
      "ProfileDeviceClass": "mntr",
      "DataColourSpace": "GRAY",
      "PCS": "XYZ ",
      "RenderingIntent": "Perceptual"
    },
    "Tags": [
      { "mediaWhitePointTag": { "data": { "type": "XYZArrayType", "XYZ": [ [ 0.964202880859375, 1.0, 0.8249053955078125 ] ] } } },
      { "grayTRCTag": { "data": { "type": "curveType", "curveType": "gamma", "gamma": 2.19921880909169 } } },
      { "profileDescriptionTag": { "data": { "type": "multiLocalizedUnicodeType",
          "localizedStrings": [ { "country": "US", "language": "en", "text": "2384 CLI contract fixture" } ] } } },
      { "copyrightTag": { "data": { "type": "multiLocalizedUnicodeType",
          "localizedStrings": [ { "country": "US", "language": "en", "text": "ICC regression fixture" } ] } } }
    ]
  }
}
JSONEOF

# The same profile without mediaWhitePointTag, by whole-line delete -- the same
# shape as the XML twin above.
#
# The tag is written on ONE line, and is deliberately the FIRST entry, so the
# delete leaves the array valid: dropping a trailing entry would strand the
# previous one's comma.  Do not reach for `sed '/x/,+1d'` to span two lines --
# `addr,+N` is a GNU extension that BSD sed (macOS) rejects outright, leaving a
# zero-byte file, and the case would then exercise the parse-failure path
# instead of the validation branch it is about.
#
# CI would not have told us: no lane runs this suite on macOS.  The only ctest
# gate in _build-test-unix.yml is the step literally named "Run Linux CTest
# gate", guarded by `inputs.run-full-tests`, which _build-matrix.yml sets from
# `matrix.os == 'ubuntu-latest'`; ci-pr-action's two macOS legs go through
# ci-pr-unix.yml, which runs no ctest at all.  So this would have stayed green
# in CI and failed only for someone running the suite on a Mac.
grep -v 'mediaWhitePointTag' "$OUTDIR/valid.json" > "$OUTDIR/invalid.json"

cat > "$OUTDIR/unparseable.json" <<'JSONEOF'
{ "NotAProfile": { } }
JSONEOF

###############################################################################
# run <name> <tool> <input> <want-exit: zero|nonzero> <want-stream: out|err>
#     <want-artifact: file|nofile> <needle>
###############################################################################
run_case() {
  local name="$1" tool="$2" input="$3" want_exit="$4" want_stream="$5" want_file="$6" needle="$7"
  TOTAL=$((TOTAL + 1))
  local exit_code=0
  local icc="$OUTDIR/$name.icc"

  rm -f "$icc"
  : > "$STDOUT_LOG"; : > "$STDERR_LOG"
  timeout 60 "$tool" "$input" "$icc" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  # A crash is also non-zero, so exclude the signal range rather than accepting
  # any non-zero as proof the guard fired.
  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    return
  fi

  if [ "$want_exit" = "zero" ] && [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected exit=0, got exit=$exit_code"
    sed -n '1,5p' "$STDERR_LOG"
    return
  fi
  if [ "$want_exit" = "nonzero" ] && [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "reported success (exit=0) for a run that did not produce a conformant profile"
    sed -n '1,5p' "$STDOUT_LOG"
    return
  fi

  # The stream is the discriminator: both branches print a full report, so a
  # status-only assertion would accept an implementation that answered on both.
  local loud quiet
  if [ "$want_stream" = "err" ]; then
    loud="$STDERR_LOG"; quiet="$STDOUT_LOG"
  else
    loud="$STDOUT_LOG"; quiet="$STDERR_LOG"
  fi
  if [ -s "$quiet" ]; then
    fail_case "$name" "wrote $(wc -c < "$quiet") bytes to the wrong stream (wanted $want_stream only)"
    sed -n '1,5p' "$quiet"
    return
  fi
  if ! grep -Fq "$needle" "$loud" 2>/dev/null; then
    fail_case "$name" "no \"$needle\" on $want_stream"
    sed -n '1,5p' "$loud"
    return
  fi

  # The artifact half.  An invalid profile is still written -- see the header.
  if [ "$want_file" = "file" ] && [ ! -f "$icc" ]; then
    fail_case "$name" "no profile written; the artifact must survive a non-zero status"
    return
  fi
  if [ "$want_file" = "nofile" ] && [ -f "$icc" ]; then
    fail_case "$name" "a run that parsed nothing still wrote $(wc -c < "$icc") bytes"
    return
  fi

  pass_case "$name" "exit=$exit_code, $want_stream only, $want_file"
}

echo "=== iccFromXml/iccFromJson invalid-profile exit-code regression (#2384) ==="

if [ -x "$FROMXML" ]; then
  run_case "xml-invalid"     "$FROMXML"  "$OUTDIR/invalid.xml"     nonzero err file   "Profile is invalid"
  run_case "xml-valid"       "$FROMXML"  "$OUTDIR/valid.xml"       zero    out file   "Profile parsed and saved correctly"
  run_case "xml-unparseable" "$FROMXML"  "$OUTDIR/unparseable.xml" nonzero err nofile "Unable to Parse"
else
  skip_case "iccFromXml" "not built at $FROMXML"
fi

if [ -x "$FROMJSON" ]; then
  run_case "json-invalid"     "$FROMJSON" "$OUTDIR/invalid.json"     nonzero err file   "Profile is invalid"
  run_case "json-valid"       "$FROMJSON" "$OUTDIR/valid.json"       zero    out file   "Profile parsed and saved correctly"
  run_case "json-unparseable" "$FROMJSON" "$OUTDIR/unparseable.json" nonzero err nofile "Unable to Parse"
else
  skip_case "iccFromJson" "not built at $FROMJSON"
fi

echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

# A run that measured nothing must not report green -- the same defect class
# this suite exists to pin.
if [ "$PASS" -eq 0 ]; then
  echo "no case was measured -- treating as skipped rather than passed"
  exit 77
fi

exit 0
