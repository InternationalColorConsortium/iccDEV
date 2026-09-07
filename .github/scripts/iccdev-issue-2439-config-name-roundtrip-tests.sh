#!/bin/bash
###############################################################################
# #2439 -- the legacy config writer hands a name token to the CONSOLE sanitizer,
#          so the only text fromLegacy() reads back is encoded for a terminal
###############################################################################
#
# #2420 asked what a file-safe encoder should look like at IccCmmConfig.cpp:2393
# and :2403. Measuring it turned up two defects, not one.
#
# 1. The round trip does not close. icSanitizeConsoleText() is written for a
#    terminal and is deliberately lossy -- every non-ASCII codepoint becomes an
#    escape and a literal backslash is left alone. A profile colour named
#    Gr<U+00FC>n went out as { "Gr\u00FCn" } and came back as the eight
#    characters G r \ u 0 0 F C n, which matches no colour. Measured on
#    764fc176: exit 0 on the way out, "Profile application failed." on the way
#    back in. This dates from #1683 (d59c072d), which introduced the sanitize
#    call, NOT from #2419 -- worth stating because the sub-issue that spawned
#    this suite says #2419.
#
# 2. That encoding made a stack-buffer-overflow reachable from the tool's own
#    output. ParseName() strncpy()'d the token into a caller-supplied char[256]
#    using a length read straight out of the token, with nothing bounding it
#    (CWE-787). It is not a sanitizer-only finding: on a stock Release build
#    glibc's _FORTIFY_SOURCE catches the strncpy and the tool ABORTS, exit 134,
#    "*** buffer overflow detected ***". A colour name of 43 accented characters
#    is 86 bytes going in and 258 characters coming back out at six characters
#    per escape -- so the crash is reachable by round-tripping a legitimate
#    profile, not only by hand-writing a long name.
#
# The fix splits the sink instead of widening the escape set. A named file gets
# EncodeCfgName(), which escapes only \ " and control codepoints and writes every
# printable codepoint as its own bytes; stdout keeps icSanitizeConsoleText(),
# because stdout may be a terminal and #2406/#2420 put that escaping there.
#
# Anti-vacuity. The three CONTROL cases carry as much weight as the defect cases
# and MUST pass on both builds:
#
#   stdout-console-escaping-kept  The cheap way to "fix" the round trip is to
#       drop the escaping. That re-opens #2406 and #2420 -- U+202E and the C1
#       controls would reach a terminal again -- and every defect case here
#       would still go green. This case is the only thing standing between the
#       two.
#   ascii-corpus-unchanged  All three tracked legacy fixtures must produce the
#       output they always did. Verified byte-for-byte across builds while
#       developing the fix (96KB over the three); asserted here on the token
#       shape, which is what a future encoder change would move.
#   backslash-name-preserved  Adding a decoder is an acceptance change: a name
#       that already contained a backslash must not start decoding. The escape
#       set is deliberately small ({ \\ \" \xNN }) and unknown escapes are kept
#       verbatim, so C:\temp still reads as C:\temp -- \t is NOT an escape this
#       writer emits. Zero tracked corpus files carry a backslash or a non-ASCII
#       byte in a name token (git ls-files '*.txt', 3 files), so this case is the
#       only cover for the acceptance change.
#
# Both profiles are built here from tracked XML rather than taken from Testing/,
# where every .icc is generated and may not exist yet.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# An explicit ICCDEV_TOOLS_DIR is never second-guessed: resolving past it would
# measure a different build than the caller meant, which is how a stale tree
# fakes a result.
if [ -n "${ICCDEV_TOOLS_DIR:-}" ]; then
  TOOLS_DIR="$ICCDEV_TOOLS_DIR"
  TOOLS_DIR_EXPLICIT=1
else
  TOOLS_DIR="$REPO_ROOT/Build/Tools"
  TOOLS_DIR_EXPLICIT=0
fi

OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2439-config-name-roundtrip}"
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"

# Resolve on the BINARIES. Build/Tools exists in a configured tree as CMake
# scaffolding with no executables under it, and a directory test would resolve
# to a path where nothing is built, skip every case and still exit 0.
tools_dir_ok() {
  [ -x "$1/IccApplyNamedCmm/iccApplyNamedCmm" ] && [ -x "$1/IccFromXml/iccFromXml" ]
}

if [ "$TOOLS_DIR_EXPLICIT" -eq 0 ] && ! tools_dir_ok "$TOOLS_DIR"; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/out/build/Tools"; do
    if tools_dir_ok "$candidate"; then
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

APPLY="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
SEARCH="$TOOLS_DIR/IccApplySearch/iccApplySearch"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
NAMED_XML="$REPO_ROOT/Testing/Named/NamedColor.xml"

PASS=0
FAIL=0
SKIP=0

# Counted separately: the overflow and round-trip cases are the ones that cover
# the defect, so the suite must not be able to report success without them.
DEFECT_MEASURED=0

fail_case() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
pass_case() { echo "  [PASS] $1 -- $2"; PASS=$((PASS + 1)); }
skip_case() { echo "  [SKIP] $1 -- $2"; SKIP=$((SKIP + 1)); }

echo "=== #2439 legacy config name encoding ==="

if [ ! -x "$FROMXML" ] || [ ! -x "$APPLY" ] || [ ! -f "$NAMED_XML" ]; then
  echo "  [SKIP] iccFromXml/iccApplyNamedCmm not built, or $NAMED_XML missing"
  exit 77
fi

# --------------------------------------------------------------------------
# Fixtures, built here so the suite does not depend on generated Testing/*.icc
# --------------------------------------------------------------------------
# U+00FC as its two UTF-8 bytes. Written with printf rather than as a literal so
# this file stays pure ASCII and cannot be mangled by an editor or a checkout.
UCH="$(printf '\303\274')"
UNAME="Gr${UCH}n"
# 43 codepoints: 86 bytes in, 258 characters out under the console escaping --
# just past the 256-byte buffer, which is the point.
WIDENAME="$(printf '\303\274%.0s' $(seq 1 43))"
BSNAME='C:\temp'

make_profile() {  # $1 = colour name to substitute for "Gray", $2 = output .icc
  local name="$1" out="$2"
  # The name goes through the environment, not the command line: BSNAME is
  # C:\temp and an argument would have to survive two layers of quoting.
  if ! NEWNAME="$name" python3 - "$NAMED_XML" "$OUTDIR/tmp.xml" <<'PY'
import os, sys
src = open(sys.argv[1], encoding='utf-8').read()
open(sys.argv[2], 'w', encoding='utf-8').write(
    src.replace('CDATA[Gray]', 'CDATA[%s]' % os.environ['NEWNAME']))
PY
  then
    return 1
  fi
  timeout 120 "$FROMXML" "$OUTDIR/tmp.xml" "$out" > "$OUTDIR/fromxml.log" 2>&1
  [ -f "$out" ]
}

make_legacy() {  # $1 = name token contents, $2 = output .txt
  printf "'nmcl'\t; Data Format\nicEncodeValue\t; Encoding\n\n{ \"%s\" } 1.0\n" "$1" > "$2"
}

if ! command -v python3 > /dev/null 2>&1; then
  echo "  [SKIP] python3 unavailable; every case needs a generated profile"
  exit 77
fi

# Called one at a time on purpose. Packing "$name:$path" into a loop variable
# and splitting on ':' silently built the backslash profile under the name "C",
# because BSNAME is C:\temp -- the control then failed for a reason that had
# nothing to do with the code under test.
build_or_skip() {
  if ! make_profile "$1" "$2"; then
    echo "  [SKIP] could not build $2 from $NAMED_XML"
    sed -n '1,20p' "$OUTDIR/fromxml.log" 2>/dev/null
    exit 77
  fi
}

build_or_skip "$UNAME"    "$OUTDIR/utf8.icc"
build_or_skip "$BSNAME"   "$OUTDIR/bslash.icc"
build_or_skip "$WIDENAME" "$OUTDIR/wide.icc"

# --------------------------------------------------------------------------
# 1/2. The overflow, on iccApplyNamedCmm and iccApplySearch
#
# 400 'A' characters. Deliberately past 256 rather than exactly at it: at
# exactly 256 the strncpy still fits and only the trailing NUL store is out of
# bounds, which _FORTIFY_SOURCE does not see -- ASan does, but this case has to
# be red on an uninstrumented build too. Exit 134 (SIGABRT) on 764fc176.
# --------------------------------------------------------------------------
make_legacy "$(printf 'A%.0s' $(seq 1 400))" "$OUTDIR/long.txt"

check_no_crash() {  # $1 = case, $2 = log, $3 = rc
  local name="$1" log="$2" rc="$3"

  if grep -qE "ERROR: AddressSanitizer|buffer overflow detected|stack smashing" "$log" 2>/dev/null; then
    fail_case "$name" "overflow reported: $(grep -m1 -oE 'ERROR: AddressSanitizer[^ ]*|buffer overflow detected|stack smashing' "$log")"
    return 1
  fi
  # 134 = SIGABRT, 139 = SIGSEGV. A tool that dies without a message still died.
  # Bounded above at 192: iccApplySearch answers a refused profile chain with
  # `return -1`, which the shell reports as 255, and an unbounded test read that
  # as a signal.
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail_case "$name" "killed by signal $((rc - 128)) (rc=$rc)"
    return 1
  fi
  pass_case "$name" "parsed a 400-character name without an out-of-bounds write (rc=$rc)"
  return 0
}

timeout 60 "$APPLY" "$OUTDIR/long.txt" 0 0 "$OUTDIR/utf8.icc" 1 > "$OUTDIR/long-apply.log" 2>&1
check_no_crash overflow-longname-namedcmm "$OUTDIR/long-apply.log" $?
DEFECT_MEASURED=$((DEFECT_MEASURED + 1))

if [ -x "$SEARCH" ]; then
  # Usage 2 of iccApplySearch needs two profiles and an -INIT intent; with one
  # profile it rejects the command line before it ever reads the data file, and
  # the case would pass against the unfixed build.
  timeout 60 "$SEARCH" "$OUTDIR/long.txt" 0 0 "$OUTDIR/utf8.icc" 1 "$OUTDIR/utf8.icc" 1 -INIT 1 \
    > "$OUTDIR/long-search.log" 2>&1
  rc=$?
  # The chain of two named-colour profiles is refused at AddXform, which is fine
  # -- that happens AFTER fromLegacy. What would make this case vacuous is an
  # exit BEFORE the data file is read, so those two diagnostics are disqualifying.
  if grep -qE "Unable to parse legacy data file|Unable to parse profile sequence arguments" \
       "$OUTDIR/long-search.log" 2>/dev/null; then
    skip_case overflow-longname-applysearch "the run exited before it read the data file"
  else
    check_no_crash overflow-longname-applysearch "$OUTDIR/long-search.log" "$rc"
    DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
  fi
else
  skip_case overflow-longname-applysearch "iccApplySearch not built"
fi

# --------------------------------------------------------------------------
# 3. The tool's own output, fed back in
#
# 43 accented characters: legal, unremarkable, 86 bytes. Under the console
# escaping the token comes back 258 characters long and the re-read aborts.
# --------------------------------------------------------------------------
make_legacy "$WIDENAME" "$OUTDIR/wide-in.txt"
timeout 60 "$APPLY" "$OUTDIR/wide-in.txt" 0 0 "$OUTDIR/wide.icc" 1 > "$OUTDIR/wide-1.log" 2>&1
WIDE_TOKEN="$(grep -o '{ "[^"]*" }' "$OUTDIR/wide-1.log" | head -1)"

if [ -z "$WIDE_TOKEN" ]; then
  skip_case overflow-self-output "first pass emitted no name token"
else
  printf "'nmcl'\t; Data Format\nicEncodeValue\t; Encoding\n\n%s 1.0\n" "$WIDE_TOKEN" > "$OUTDIR/wide-in2.txt"
  timeout 60 "$APPLY" "$OUTDIR/wide-in2.txt" 0 0 "$OUTDIR/wide.icc" 1 > "$OUTDIR/wide-2.log" 2>&1
  rc=$?
  if grep -qE "ERROR: AddressSanitizer|buffer overflow detected|stack smashing" "$OUTDIR/wide-2.log" 2>/dev/null \
     || { [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; }; then
    fail_case overflow-self-output "re-reading the tool's own 43-codepoint name token crashed it (rc=$rc)"
  else
    pass_case overflow-self-output "the tool's own name token re-reads without an out-of-bounds write (rc=$rc)"
  fi
  DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
fi

# --------------------------------------------------------------------------
# 4. The round trip, through a named destination FILE
#
# This is the sink the fix is about: a config file iccApplyNamedCmm writes and
# then reads back. Two assertions, because either alone is weak -- the token
# must hold the name's own bytes, AND the re-read must find the colour.
# --------------------------------------------------------------------------
make_legacy "$UNAME" "$OUTDIR/utf8-in.txt"
cat > "$OUTDIR/rt.json" <<EOF_CFG
{
  "dataFiles": {
    "srcType": "legacy",
    "srcFile": "$OUTDIR/utf8-in.txt",
    "dstType": "legacy",
    "dstFile": "$OUTDIR/rt-out.txt",
    "dstEncoding": "value"
  },
  "profileSequence": [
    { "iccFile": "$OUTDIR/utf8.icc", "intent": "relative" }
  ]
}
EOF_CFG

timeout 60 "$APPLY" -cfg "$OUTDIR/rt.json" > "$OUTDIR/rt.log" 2>&1
if [ ! -f "$OUTDIR/rt-out.txt" ]; then
  skip_case roundtrip-nonascii-file "the tool wrote no destination file"
  skip_case roundtrip-nonascii-reread "the tool wrote no destination file"
else
  FILE_TOKEN="$(grep -o '{ "[^"]*" }' "$OUTDIR/rt-out.txt" | head -1)"

  if [ "$FILE_TOKEN" = "{ \"$UNAME\" }" ]; then
    pass_case roundtrip-nonascii-file "the config file carries the name's own UTF-8 bytes"
  else
    fail_case roundtrip-nonascii-file "config file token is '$FILE_TOKEN', expected '{ \"$UNAME\" }'"
  fi
  DEFECT_MEASURED=$((DEFECT_MEASURED + 1))

  printf "'nmcl'\t; Data Format\nicEncodeValue\t; Encoding\n\n%s 1.0\n" "$FILE_TOKEN" > "$OUTDIR/rt-back.txt"
  timeout 60 "$APPLY" "$OUTDIR/rt-back.txt" 0 0 "$OUTDIR/utf8.icc" 1 > "$OUTDIR/rt-back.log" 2>&1
  if grep -q "Profile application failed" "$OUTDIR/rt-back.log"; then
    fail_case roundtrip-nonascii-reread "the colour written to the config file no longer matches the profile"
  else
    pass_case roundtrip-nonascii-reread "the name written to a config file reads back and applies"
  fi
  DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
fi

# --------------------------------------------------------------------------
# 4b. A name containing the terminator sequence itself
#
# ParseName() finds the end of the token with strstr(..., '" }'), which knows
# nothing about escapes. The first cut of this fix spelled a quote \" and a name
# holding '" }' matched at its OWN escaped quote: A" }B went out as A\" }B and
# came back as A\. The quote is written \x22 for exactly this reason, so no raw
# '"' byte survives inside a token and the scan has only one thing it can match.
#
# Red against that first cut, and against 764fc176 for a different reason -- the
# console encoder left the quote raw, so the name truncated at 'A' there too.
# --------------------------------------------------------------------------
QNAME='A" }B'
if make_profile "$QNAME" "$OUTDIR/quote.icc"; then
  cat > "$OUTDIR/q.json" <<EOF_QCFG
{
  "dataFiles": {
    "srcType": "legacy",
    "srcFile": "$OUTDIR/qin.txt",
    "dstType": "legacy",
    "dstFile": "$OUTDIR/qout.txt",
    "dstEncoding": "value"
  },
  "profileSequence": [
    { "iccFile": "$OUTDIR/quote.icc", "intent": "relative" }
  ]
}
EOF_QCFG
  # The source name is spelled with the escape the writer itself emits, so the
  # reader has to decode it to find the colour at all.
  make_legacy 'A\x22 }B' "$OUTDIR/qin.txt"
  timeout 60 "$APPLY" -cfg "$OUTDIR/q.json" > "$OUTDIR/q.log" 2>&1

  if [ ! -f "$OUTDIR/qout.txt" ]; then
    fail_case terminator-in-name "no destination file: the name holding '\" }' was not matched"
  else
    Q_TOKEN="$(grep -o '{ "[^"]*" }' "$OUTDIR/qout.txt" | head -1)"
    if [ "$Q_TOKEN" = '{ "A\x22 }B" }' ]; then
      pass_case terminator-in-name "a name containing the token terminator survives the round trip"
    else
      fail_case terminator-in-name "token '$Q_TOKEN', expected '{ \"A\\x22 }B\" }' -- a raw quote lets the terminator scan match inside the name"
    fi
  fi
  DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
else
  skip_case terminator-in-name "could not build the quote fixture"
fi

# --------------------------------------------------------------------------
# 5. CONTROL -- stdout must KEEP the #2420 codepoint escaping.
#
# Passes on both builds. Without it, deleting the escaping altogether would turn
# every case above green while re-opening #2406/#2420 on the terminal.
# --------------------------------------------------------------------------
timeout 60 "$APPLY" "$OUTDIR/utf8-in.txt" 0 0 "$OUTDIR/utf8.icc" 1 > "$OUTDIR/stdout.log" 2>&1
STDOUT_TOKEN="$(grep -o '{ "[^"]*" }' "$OUTDIR/stdout.log" | head -1)"
if [ "$STDOUT_TOKEN" = '{ "Gr\u00FCn" }' ]; then
  pass_case stdout-console-escaping-kept "stdout still escapes U+00FC by codepoint"
else
  fail_case stdout-console-escaping-kept "stdout token is '$STDOUT_TOKEN', expected '{ \"Gr\\u00FCn\" }' -- console escaping must not be dropped to close the round trip"
fi

# --------------------------------------------------------------------------
# 6. CONTROL -- the tracked ASCII corpus is untouched. Passes on both builds.
# --------------------------------------------------------------------------
CORPUS="$REPO_ROOT/Testing/Named/NamedColorTest.txt"
CORPUS_ICC="$OUTDIR/corpus.icc"
if [ -f "$CORPUS" ] && make_profile "Gray" "$CORPUS_ICC"; then
  timeout 60 "$APPLY" "$CORPUS" 0 0 "$CORPUS_ICC" 1 > "$OUTDIR/corpus.log" 2>&1
  missing=""
  for n in Black Gray Blue Red Green; do
    grep -Fq "{ \"$n\" }" "$OUTDIR/corpus.log" || missing="$missing $n"
  done
  if [ -z "$missing" ]; then
    pass_case ascii-corpus-unchanged "every tracked ASCII name still round-trips verbatim"
  else
    fail_case ascii-corpus-unchanged "missing name tokens:$missing"
  fi
else
  skip_case ascii-corpus-unchanged "$CORPUS or its profile unavailable"
fi

# --------------------------------------------------------------------------
# 7a. CONTROL -- an UNKNOWN escape must not start decoding. Passes on both builds.
#
# C:\temp contains \t, which the writer never emits, so the decoder keeps both
# characters and the name is untouched. This is the bulk of the acceptance
# surface: every escape except \\ and \xNN behaves exactly as it did before.
# --------------------------------------------------------------------------
make_legacy 'C:\temp' "$OUTDIR/bslash-in.txt"
timeout 60 "$APPLY" "$OUTDIR/bslash-in.txt" 0 0 "$OUTDIR/bslash.icc" 1 > "$OUTDIR/bslash.log" 2>&1
BS_TOKEN="$(grep -o '{ "[^"]*" }' "$OUTDIR/bslash.log" | head -1)"
if [ "$BS_TOKEN" = '{ "C:\temp" }' ] && ! grep -q "Profile application failed" "$OUTDIR/bslash.log"; then
  pass_case backslash-unknown-escape-preserved "an escape the writer never emits is kept verbatim"
else
  fail_case backslash-unknown-escape-preserved "token '$BS_TOKEN' -- \\t must not be decoded"
fi

# --------------------------------------------------------------------------
# 7b. ACCEPTANCE CHANGE, asserted rather than assumed.
#
# \\ and \xNN are the ONLY two spellings that move, and they DO move: a
# hand-written name C:\x41rt used to match a profile colour spelled literally
# C:\x41rt and now matches one spelled C:Art. An earlier draft of this suite
# claimed "a backslash reads exactly as it did before", which is false for these
# two -- the claim is narrowed here and pinned by measurement. Red pre-fix by
# design: this case documents the intended change, it does not guard against it.
# --------------------------------------------------------------------------
if make_profile 'C:Art' "$OUTDIR/decoded.icc"; then
  make_legacy 'C:\x41rt' "$OUTDIR/esc-in.txt"
  timeout 60 "$APPLY" "$OUTDIR/esc-in.txt" 0 0 "$OUTDIR/decoded.icc" 1 > "$OUTDIR/esc.log" 2>&1
  if grep -q "Profile application failed" "$OUTDIR/esc.log"; then
    fail_case hex-escape-decodes "C:\\x41rt did not resolve to the colour named C:Art"
  else
    pass_case hex-escape-decodes "\\xNN decodes -- the acceptance change is exactly this and \\\\"
  fi
  DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
else
  skip_case hex-escape-decodes "could not build the C:Art fixture"
fi

# --------------------------------------------------------------------------
# 8. A name that BEGINS with the terminator's tail
#
# ParseName() used to look for '" }' from the start of the line, where the '{ "'
# prefix supplies a quote of its own. A colour named ' }B' therefore "ended"
# before it began -- ptr < p, the decode loop never ran, and fromLegacy() dropped
# the row with no diagnostic at all. Red on 764fc176, where the row vanishes.
# --------------------------------------------------------------------------
if make_profile ' }B' "$OUTDIR/brace.icc"; then
  make_legacy ' }B' "$OUTDIR/brace-in.txt"
  timeout 60 "$APPLY" "$OUTDIR/brace-in.txt" 0 0 "$OUTDIR/brace.icc" 1 > "$OUTDIR/brace.log" 2>&1
  BR_TOKEN="$(grep -o '{ "[^"]*" }' "$OUTDIR/brace.log" | head -1)"
  if [ "$BR_TOKEN" = '{ " }B" }' ]; then
    pass_case leading-terminator-name "a name beginning with ' }' is parsed rather than dropped"
  else
    fail_case leading-terminator-name "token '$BR_TOKEN', expected '{ \" }B\" }' -- the row was dropped silently"
  fi
  DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
else
  skip_case leading-terminator-name "could not build the ' }B' fixture"
fi

# --------------------------------------------------------------------------
# 9. A line longer than the reader's own buffer must END the read, not spin
#
# getline() sets failbit without reaching eof when the line does not fit, and
# failbit is sticky: every later getline() returns immediately having extracted
# nothing, so `while (!eof())` never terminates. Measured on 764fc176 as exit 124
# under any timeout (CWE-835) -- the same shape as #2414 and #2442 in the sibling
# tools. 30000 characters against a 20000-byte buffer.
#
# The 15s timeout is deliberately well under the 60s used elsewhere: this case
# distinguishes "terminated" from "spinning", and a slow lane still finishes a
# single refused parse in far less.
# --------------------------------------------------------------------------
make_legacy "$(printf 'A%.0s' $(seq 1 30000))" "$OUTDIR/huge.txt"
timeout 15 "$APPLY" "$OUTDIR/huge.txt" 0 0 "$OUTDIR/utf8.icc" 1 > "$OUTDIR/huge.log" 2>&1
rc=$?
if [ "$rc" -eq 124 ]; then
  fail_case overlong-line-terminates "a 30000-character line did not terminate the read (rc=124)"
else
  pass_case overlong-line-terminates "a line longer than the read buffer ends the parse (rc=$rc)"
fi
DEFECT_MEASURED=$((DEFECT_MEASURED + 1))

# --------------------------------------------------------------------------
# 10. CONTROL -- the break added for case 9 must not eat a final row.
#
# A last line with no trailing newline leaves eofbit set but NOT failbit, because
# getline() did extract characters. Passes on both builds; this is the off-by-one
# that a naive `if (!InputData) break;` would introduce.
# --------------------------------------------------------------------------
printf "'nmcl'\t; Data Format\nicEncodeValue\t; Encoding\n\n{ \"Black\" } 1.0\n{ \"Gray\" } 1.0" \
  > "$OUTDIR/nonl.txt"
# corpus.icc is the unmodified NamedColor.xml, so it carries both Black and Gray.
# Case 6 builds it; guard rather than assume, or a skip there turns this into a
# failure that says nothing about the break.
if [ ! -f "$OUTDIR/corpus.icc" ]; then
  make_profile "Gray" "$OUTDIR/corpus.icc" || true
fi
timeout 60 "$APPLY" "$OUTDIR/nonl.txt" 0 0 "$OUTDIR/corpus.icc" 1 > "$OUTDIR/nonl.log" 2>&1
NONL_ROWS="$(grep -c '{ "' "$OUTDIR/nonl.log")"
if [ ! -f "$OUTDIR/corpus.icc" ]; then
  skip_case final-row-without-newline "corpus profile unavailable"
elif [ "$NONL_ROWS" -eq 2 ]; then
  pass_case final-row-without-newline "a last line with no trailing newline is still read"
else
  fail_case final-row-without-newline "expected 2 rows, got $NONL_ROWS -- the overlong-line break ate the final row"
fi

# --------------------------------------------------------------------------
echo
echo "  pass=$PASS fail=$FAIL skip=$SKIP"

if [ "$FAIL" -ne 0 ]; then
  echo "  [FAIL] iccdev-issue-2439-config-name-roundtrip"
  exit 1
fi

# A green run that measured none of the defect cases is the failure mode this
# accounting exists to prevent.
if [ "$DEFECT_MEASURED" -eq 0 ]; then
  echo "  [SKIP] no defect case was measured"
  exit 77
fi

echo "  [PASS] iccdev-issue-2439-config-name-roundtrip ($DEFECT_MEASURED defect cases measured)"
exit 0
