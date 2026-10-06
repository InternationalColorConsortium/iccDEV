#!/bin/bash
###############################################################################
# libxml2 diagnostics console-injection regression (#2698)
#
# The parse and RelaxNG diagnostics libxml2 emits name the document and schema
# files, and through its default handler they reached stderr as given, so a
# file name carrying a CSI colour payload was interpreted by the terminal.
# iccFromXml's own prints have escaped such names since #2406; this covers the
# library's handler, which no call-site fix could reach.  Each case feeds a
# path carrying ESC into one diagnostic route and requires that no raw 0x1B
# byte reaches either stream while the escape is still rendered as \xHH.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2698-xml-diagnostics-injection}"
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
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
WORKDIR="$OUTDIR/work"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
PASS=0
FAIL=0
SKIP=0
TOTAL=0
pass_case() { PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1)); echo "  [PASS] $1 -- $2"; }
fail_case() { FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1)); echo "  [FAIL] $1 -- $2"; }
skip_case() { SKIP=$((SKIP + 1)); TOTAL=$((TOTAL + 1)); echo "  [SKIP] $1 -- $2"; }
echo "=== libxml2 diagnostics console-injection regression (#2698) ==="
if [ ! -x "$FROMXML" ]; then
  skip_case "xml-diagnostics-injection" "iccFromXml not built at $FROMXML"
  echo ""
  echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
  exit 77
fi
ESC=$(printf '\033')
PWN="${ESC}[31mXML-PWN${ESC}[0m"
count_esc() {
  od -An -tx1 -v "$1" | tr ' ' '\n' | grep -c '^1b$' || true
}
# A document libxml2 cannot parse: the parser names the file in its diagnostic.
printf '<?xml version="1.0"?>\n<IccProfile>\n  <Header>\n' > "$WORKDIR/broken-${PWN}.xml"
# A well-formed document for the schema routes.
cat > "$WORKDIR/base.xml" <<'XMLEOF'
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
    <profileDescriptionTag> <textDescriptionType><TextData>2698 fixture</TextData></textDescriptionType> </profileDescriptionTag>
    <copyrightTag> <textType><TextData>ICC regression fixture</TextData></textType> </copyrightTag>
    <mediaWhitePointTag> <XYZArrayType><XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/></XYZArrayType> </mediaWhitePointTag>
  </Tags>
</IccProfile>
XMLEOF
# A schema libxml2 cannot compile, named with the payload: the schema parser
# names it.  And a reject-all schema, so the validator names the document.
printf '<grammar xmlns="http://relaxng.org/ns/structure/1.0"><start>\n' > "$WORKDIR/broken-${PWN}.rng"
cat > "$WORKDIR/reject.rng" <<'RNGEOF'
<?xml version="1.0" encoding="UTF-8"?>
<element name="ThisWillNeverMatch" xmlns="http://relaxng.org/ns/structure/1.0"><empty/></element>
RNGEOF
cp "$WORKDIR/base.xml" "$WORKDIR/doc-${PWN}.xml"
run_case() {
  local name="$1" desc="$2"; shift 3   # and the "--"
  local log="$OUTDIR/$name.log" rc=0
  ( cd "$WORKDIR" && timeout 60 "$FROMXML" "$@" ) > "$log" 2>&1 || rc=$?
  if [ "$rc" -ge 128 ]; then
    fail_case "$name" "iccFromXml died on a signal (exit $rc)"
    return
  fi
  if [ "$rc" -eq 0 ]; then
    fail_case "$name" "the invocation succeeded, so no diagnostic was produced ($desc)"
    return
  fi
  if ! grep -q -- 'XML-PWN' "$log" 2>/dev/null; then
    fail_case "$name" "no diagnostic names the path -- nothing was measured"
    sed -n '1,8p' "$log"
    return
  fi
  local esc
  esc="$(count_esc "$log")"
  if [ "$esc" -ne 0 ]; then
    fail_case "$name" "$esc raw ESC byte(s) reached the output"
    return
  fi
  if ! grep -q 'x1B\|x1b' "$log" 2>/dev/null; then
    fail_case "$name" "path is named without ESC, but the escape was not rendered as \\xHH"
    sed -n '1,8p' "$log"
    return
  fi
  pass_case "$name" "$desc"
}
echo "tools: $TOOLS_DIR"
run_case "parser-names-document" "the parser's diagnostic names the unparseable document with the escape neutralised" -- "broken-${PWN}.xml" out.icc
run_case "schema-parser-names-schema" "the RelaxNG parser's diagnostic names the uncompilable schema with the escape neutralised" -- base.xml out.icc "-v=broken-${PWN}.rng"
run_case "validator-names-document" "the validator's diagnostic and the rejection line name the document with the escape neutralised" -- "doc-${PWN}.xml" out.icc -v=reject.rng
echo ""
echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
if [ "$PASS" -eq 0 ] && [ "$FAIL" -eq 0 ]; then
  exit 77
fi
[ "$FAIL" -eq 0 ]
