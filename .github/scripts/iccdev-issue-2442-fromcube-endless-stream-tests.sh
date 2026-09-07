#!/bin/bash
###############################################################################
# #2442 -- iccFromCube spins forever on an endless readable stream
###############################################################################
#
# CubeFile::getNextLine() read with
#
#   while ((c = fgetc(m_f)) != EOF && c != '\n')
#
# which ends on EOF or a newline and consults nothing else.  On a stream that is
# endlessly readable and never yields a newline, neither ever arrives, so the
# loop spins inside a SINGLE getNextLine() call.  Measured on 5bbe773f: exit 124
# under any timeout at a flat RSS -- CPU exhaustion, not an allocation blow-up
# (CWE-835).
#
# This is the sibling of #2414, not a duplicate of it.  #2414 repaired isEOF()
# so the three `while (!isEOF())` loops terminate on a FAILED read; this loop
# never reaches isEOF() at all, so /dev/zero measured 124 both before and after
# that fix.  MAX_LINE_LEN does not bound it either: that caps what is STORED,
# and characters past it are deliberately still consumed so the tail of an
# over-long line is not returned as the next call's "line" (#1843).
#
# The fix bounds what is CONSUMED at MAX_LINE_CONSUME_LEN (8 * MAX_LINE_LEN =
# 65536) and marks the stream unusable, which routes the case into the same
# `while (!isEOF())` loops #2414 already terminates.
#
# WHAT IS REACHABLE, measured rather than assumed.  The stream must be SEEKABLE
# as well as endless: parseHeader() calls ftell() at the top of every iteration
# and refuses with "header parsing error" when it returns -1, so a pipe never
# gets as far as this loop.  An endlessly-fed FIFO (`tr -dc a < /dev/zero > p`)
# exits 254 on the UNFIXED build.  That leaves seekable character devices --
# /dev/zero and /dev/full, both covered below.  /dev/urandom is deliberately NOT
# a case: random bytes contain 0x0A, so it terminates on its own and would be a
# fixture that passes against the unfixed build.
#
# Anti-vacuity, which is most of this file.  A cap is trivially satisfied by a
# tool that refuses more than it should, and that failure would be INVISIBLE to
# the two defect cases -- so eight of the eleven cases are controls that must
# pass on BOTH builds, and three of them pin the cap's boundary from the other
# side:
#
#   * a 20000-character comment -- over MAX_LINE_LEN, far under the consume cap
#   * a line of 65535 consumed characters -- one below the cap
#   * an over-long TABLE ROW, which must still be rejected by parse3DTable() as
#     "Invalid 3DLUT entry" and NOT silently misread (#1843)
#
# The trailing-junk pair is there because the cap DID cost a validation once:
# the "Too many 3DLUT entries" loop accepts empty lines, so a discarded
# over-long line read as a blank one and the parse returned success on a file
# master rejects.  Neither build may accept it.
#
# The /dev/null case is here for the same reason it is in the #2414 suite, and
# it is the argument against "just require a regular file": /dev/null is a
# character device, so an S_ISREG guard would refuse it before the parser saw
# it, and the contract is that it reaches the parser and is refused THERE.
#
# Red/green against 5bbe773f: devzero-terminates and devfull-terminates fail
# with rc=124, devzero-names-cause finds no diagnostic, and over-cap-refused
# fails because the unfixed build ACCEPTS the file (rc=0).  The other five cases
# pass on both builds -- that is what they are for.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# An explicit ICCDEV_TOOLS_DIR is never second-guessed: resolving past it would
# measure a DIFFERENT build than the caller meant, which is how a stale tree
# fakes a result.
if [ -n "${ICCDEV_TOOLS_DIR:-}" ]; then
  TOOLS_DIR="$ICCDEV_TOOLS_DIR"
  TOOLS_DIR_EXPLICIT=1
else
  TOOLS_DIR="$REPO_ROOT/Build/Tools"
  TOOLS_DIR_EXPLICIT=0
fi
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2442-fromcube-endless-stream}"
mkdir -p "$OUTDIR"

# Resolve on the BINARY, not on the directory.  Build/Tools exists in a
# configured tree as CMake scaffolding with nothing built under it, so a
# directory test picks a path where every case skips -- and the suite would
# still exit 0.  A green run that skipped every case covering the defect is the
# failure mode this accounting exists to prevent (#2414 hit exactly that).
if [ "$TOOLS_DIR_EXPLICIT" -eq 0 ] && [ ! -x "$TOOLS_DIR/IccFromCube/iccFromCube" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/out/build/Tools"; do
    if [ -x "$candidate/IccFromCube/iccFromCube" ]; then
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

FROMCUBE="$TOOLS_DIR/IccFromCube/iccFromCube"
CUBE_OK="$REPO_ROOT/.github/ci/test-data/test-identity.cube"

# Must track MAX_LINE_CONSUME_LEN in iccFromCube.cpp.  Asserted rather than
# assumed: the over/under pair below is only a boundary test if this is the
# boundary, so a change to the constant that is not mirrored here turns two
# cases into noise.
CAP=65536

PASS=0
FAIL=0
SKIP=0
TOTAL=0
# Counted separately: the endless-stream cases are the ones that cover the
# defect, so the suite must not be able to report success without them.
DEFECT_MEASURED=0

fail_case() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
pass_case() { echo "  [PASS] $1 -- $2"; PASS=$((PASS + 1)); }
skip_case() { echo "  [SKIP] $1 -- $2"; SKIP=$((SKIP + 1)); }

if [ ! -x "$FROMCUBE" ]; then
  echo "iccFromCube not built at $FROMCUBE -- nothing measured"
  echo "=== summary: 0 passed, 0 failed, 11 skipped, 0 total ==="
  exit 77
fi
if [ ! -f "$CUBE_OK" ]; then
  echo "fixture missing: $CUBE_OK -- nothing measured"
  echo "=== summary: 0 passed, 0 failed, 11 skipped, 0 total ==="
  exit 77
fi

# --- fixtures -------------------------------------------------------------
# Generated rather than tracked: three of them are 64 KB of one repeated
# character, which is corpus bloat for no reading benefit, and generating them
# keeps the sizes visibly tied to $CAP above.
#
# awk rather than reading /dev/zero, deliberately.  This suite uses that device
# as INPUT UNDER TEST, and iccdev_add_script_test has no WIN32 guard, so on a
# lane where it is absent the fixtures would come out short and the cap cases
# would fail for a reason that has nothing to do with the fix.  The device cases
# skip cleanly there; the fixture cases must not depend on it.
# Doubling rather than appending one character at a time: 70000 single-character
# concatenations is quadratic in some awks, ~17 doublings is not.
repeat_char() { # $1 = count, $2 = character
  awk -v n="$1" -v c="$2" 'BEGIN { s = c; while (length(s) < n) s = s s; printf "%s", substr(s, 1, n) }'
}

make_long_line_cube() { # $1 = consumed chars on the first line, $2 = output
  {
    printf '# '
    repeat_char "$(( $1 - 2 ))" 'y'
    printf '\n'
    cat "$CUBE_OK"
  } > "$2"
}

make_long_line_cube 20000        "$OUTDIR/long-comment.cube"
make_long_line_cube "$((CAP-1))" "$OUTDIR/just-under.cube"
make_long_line_cube "$((CAP+1))" "$OUTDIR/just-over.cube"

# Trailing junk AFTER a complete table, in two lengths.  The long one is the
# case that caught a regression in this very fix: the trailing
# "Too many 3DLUT entries" loop treats an empty line as legal, so a DISCARDED
# over-long line looked identical to a blank one and the parse returned success
# -- printing the cap diagnostic and then "successfully created" at rc=0, on a
# file master rejects.  The short one pins that the ordinary trailing-garbage
# check still works, so the long case cannot be satisfied by breaking both.
{ cat "$CUBE_OK"; repeat_char 100 'z'; printf '\n'; } \
  > "$OUTDIR/trailing-short.cube"
{ cat "$CUBE_OK"; repeat_char 70000 'z'; printf '\n'; } \
  > "$OUTDIR/trailing-long.cube"

# An over-long TABLE ROW rather than a comment: this is the #1843 shape, where
# the tail past MAX_LINE_LEN is dropped from the returned text and the row is
# then short of its three floats and rejected explicitly.
{
  head -4 "$CUBE_OK"
  printf '0.0 0.0 0.0 '
  repeat_char 20000 '9'
  printf '\n'
  tail -n +6 "$CUBE_OK"
} > "$OUTDIR/long-row.cube"

# --- (1) the defect: /dev/zero --------------------------------------------
# 15s against a ~0.1s real conversion, so a timeout here is the loop, not a
# slow machine.
if [ ! -c /dev/zero ]; then
  skip_case devzero-terminates "/dev/zero is not a character device here"
  skip_case devzero-names-cause "/dev/zero is not a character device here"
else
  LOG="$OUTDIR/devzero.log"
  rc=0
  timeout 15 "$FROMCUBE" /dev/zero "$OUTDIR/devzero.icc" > "$LOG" 2>&1 || rc=$?
  TOTAL=$((TOTAL + 1)); DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
  if [ "$rc" -eq 124 ]; then
    fail_case devzero-terminates "timed out (rc=124) -- the getNextLine() spin is back"
  elif [ "$rc" -eq 0 ]; then
    fail_case devzero-terminates "reported success on /dev/zero (rc=0)"
  else
    pass_case devzero-terminates "refused with rc=$rc"
  fi

  # (2) Anti-vacuity for (1).  "Did not hang" is satisfied by a tool that fails
  # for some unrelated reason, so the refusal has to be THIS refusal: the cap
  # naming itself, and the parse failure naming the file.
  TOTAL=$((TOTAL + 1))
  if ! grep -q "Line longer than $CAP characters" "$LOG" 2>/dev/null; then
    fail_case devzero-names-cause "no cap diagnostic -- terminated for some other reason"
    sed -n '1,10p' "$LOG"
  elif ! grep -q "Unable to parse .*/dev/zero" "$LOG" 2>/dev/null; then
    fail_case devzero-names-cause "cap diagnostic present but the parse did not fail naming the file"
    sed -n '1,10p' "$LOG"
  else
    pass_case devzero-names-cause "named the cap and failed the parse"
  fi
fi

# --- (3) the same defect through a second device --------------------------
# /dev/full is not a duplicate of /dev/zero here: it is the check that the fix
# is a property of the READ loop rather than a special case for one path.
if [ ! -c /dev/full ]; then
  skip_case devfull-terminates "/dev/full not present on this platform"
else
  rc=0
  timeout 15 "$FROMCUBE" /dev/full "$OUTDIR/devfull.icc" > "$OUTDIR/devfull.log" 2>&1 || rc=$?
  TOTAL=$((TOTAL + 1)); DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
  if [ "$rc" -eq 124 ]; then
    fail_case devfull-terminates "timed out (rc=124)"
  elif [ "$rc" -eq 0 ]; then
    fail_case devfull-terminates "reported success on /dev/full (rc=0)"
  else
    pass_case devfull-terminates "refused with rc=$rc"
  fi
fi

# --- (4) the cap's upper side ---------------------------------------------
TOTAL=$((TOTAL + 1)); DEFECT_MEASURED=$((DEFECT_MEASURED + 1))
rc=0
timeout 30 "$FROMCUBE" "$OUTDIR/just-over.cube" "$OUTDIR/just-over.icc" \
  > "$OUTDIR/just-over.log" 2>&1 || rc=$?
if [ "$rc" -eq 0 ]; then
  fail_case over-cap-refused "accepted a $((CAP+1))-character line (rc=0) -- the cap is not in effect"
elif [ "$rc" -eq 124 ]; then
  fail_case over-cap-refused "timed out (rc=124)"
elif grep -q "Line longer than $CAP characters" "$OUTDIR/just-over.log" 2>/dev/null; then
  pass_case over-cap-refused "refused at the cap with rc=$rc"
else
  fail_case over-cap-refused "rc=$rc but not the cap's diagnostic"
  sed -n '1,10p' "$OUTDIR/just-over.log"
fi

# --- (5) the cap's lower side: ONE character below it must still convert ---
# This is the case that would catch an off-by-one or a cap applied to the
# stored text rather than the consumed text.
TOTAL=$((TOTAL + 1))
rm -f "$OUTDIR/just-under.icc"
rc=0
timeout 30 "$FROMCUBE" "$OUTDIR/just-under.cube" "$OUTDIR/just-under.icc" \
  > "$OUTDIR/just-under.log" 2>&1 || rc=$?
if [ "$rc" -eq 0 ] && [ -s "$OUTDIR/just-under.icc" ]; then
  pass_case under-cap-converts "$((CAP-1))-character line converted, $(wc -c < "$OUTDIR/just-under.icc") bytes"
else
  fail_case under-cap-converts "rc=$rc -- the cap bites one character early"
  sed -n '1,10p' "$OUTDIR/just-under.log"
fi

# --- (6) control: a long comment, over MAX_LINE_LEN but far under the cap --
TOTAL=$((TOTAL + 1))
rm -f "$OUTDIR/long-comment.icc"
rc=0
timeout 30 "$FROMCUBE" "$OUTDIR/long-comment.cube" "$OUTDIR/long-comment.icc" \
  > "$OUTDIR/long-comment.log" 2>&1 || rc=$?
if [ "$rc" -eq 0 ] && [ -s "$OUTDIR/long-comment.icc" ]; then
  pass_case long-comment-converts "20000-character comment converted"
else
  fail_case long-comment-converts "rc=$rc -- a legal over-MAX_LINE_LEN line was refused"
  sed -n '1,10p' "$OUTDIR/long-comment.log"
fi

# --- (7) control: #1843 must stay fixed -----------------------------------
# An over-long table row is short of its three floats once the tail is dropped,
# and must be rejected AS a bad entry rather than resumed mid-line and misread
# as a row of its own.
TOTAL=$((TOTAL + 1))
rc=0
timeout 30 "$FROMCUBE" "$OUTDIR/long-row.cube" "$OUTDIR/long-row.icc" \
  > "$OUTDIR/long-row.log" 2>&1 || rc=$?
if [ "$rc" -eq 0 ]; then
  fail_case long-row-rejected "accepted a truncated 3DLUT row (rc=0)"
elif grep -q "Invalid 3DLUT entry" "$OUTDIR/long-row.log" 2>/dev/null; then
  pass_case long-row-rejected "rejected as a bad entry, rc=$rc (#1843 intact)"
else
  fail_case long-row-rejected "rc=$rc but not the parser's entry error -- the line was resumed or misread"
  sed -n '1,10p' "$OUTDIR/long-row.log"
fi

# --- (7b) the regression this fix introduced once: trailing junk ----------
# Both are controls in the sense that they must pass on master too -- the point
# is that the cap must not COST a validation.  rc alone is the assertion for the
# long one: which message comes back legitimately differs between builds (master
# still has the line's text and says "Too many 3DLUT entries"; with the cap the
# text is gone, so the honest answer is that the file was not read), but neither
# build may accept the file.
TOTAL=$((TOTAL + 1))
rm -f "$OUTDIR/trailing-long.icc"
rc=0
timeout 30 "$FROMCUBE" "$OUTDIR/trailing-long.cube" "$OUTDIR/trailing-long.icc" \
  > "$OUTDIR/trailing-long.log" 2>&1 || rc=$?
if [ "$rc" -eq 0 ]; then
  fail_case trailing-long-rejected "accepted a $((CAP+4464))-character line after a complete table (rc=0) -- the cap dropped a validation"
  sed -n '1,10p' "$OUTDIR/trailing-long.log"
elif [ -s "$OUTDIR/trailing-long.icc" ]; then
  fail_case trailing-long-rejected "rc=$rc but a profile was still written"
else
  pass_case trailing-long-rejected "refused with rc=$rc and wrote nothing"
fi

TOTAL=$((TOTAL + 1))
rc=0
timeout 30 "$FROMCUBE" "$OUTDIR/trailing-short.cube" "$OUTDIR/trailing-short.icc" \
  > "$OUTDIR/trailing-short.log" 2>&1 || rc=$?
if grep -q "Too many 3DLUT entries" "$OUTDIR/trailing-short.log" 2>/dev/null && [ "$rc" -ne 0 ]; then
  pass_case trailing-short-rejected "ordinary trailing garbage still refused, rc=$rc"
else
  fail_case trailing-short-rejected "rc=$rc -- the ordinary trailing-garbage check is gone"
  sed -n '1,10p' "$OUTDIR/trailing-short.log"
fi

# --- (8) control: /dev/null must still reach the PARSER --------------------
# The argument against "require a regular file": /dev/null is a character
# device, so an S_ISREG guard refuses it before the parse.  rc alone cannot
# tell the two apart -- both are non-zero -- so the discriminator is WHICH
# message comes back.
if [ ! -c /dev/null ]; then
  skip_case devnull-reaches-parser "/dev/null is not a character device here"
else
  TOTAL=$((TOTAL + 1))
  rc=0
  timeout 20 "$FROMCUBE" /dev/null "$OUTDIR/devnull.icc" > "$OUTDIR/devnull.log" 2>&1 || rc=$?
  if [ "$rc" -eq 0 ] || [ "$rc" -eq 124 ]; then
    fail_case devnull-reaches-parser "rc=$rc"
    sed -n '1,10p' "$OUTDIR/devnull.log"
  elif grep -q "Line longer than" "$OUTDIR/devnull.log" 2>/dev/null; then
    fail_case devnull-reaches-parser "the consume cap fired on an EMPTY stream"
    sed -n '1,10p' "$OUTDIR/devnull.log"
  elif grep -q "Unable to parse .*/dev/null" "$OUTDIR/devnull.log" 2>/dev/null; then
    pass_case devnull-reaches-parser "reached the parser and was refused there, rc=$rc"
  else
    fail_case devnull-reaches-parser "rc=$rc but not the parser's refusal -- a guard rejected it first"
    sed -n '1,10p' "$OUTDIR/devnull.log"
  fi
fi

# --- (9) control: a real .cube still converts ------------------------------
TOTAL=$((TOTAL + 1))
rm -f "$OUTDIR/identity.icc"
rc=0
timeout 60 "$FROMCUBE" "$CUBE_OK" "$OUTDIR/identity.icc" > "$OUTDIR/identity.log" 2>&1 || rc=$?
if [ "$rc" -eq 0 ] && [ -s "$OUTDIR/identity.icc" ]; then
  pass_case regular-file-converts "converted, $(wc -c < "$OUTDIR/identity.icc") bytes"
else
  fail_case regular-file-converts "rc=$rc"
  sed -n '1,10p' "$OUTDIR/identity.log"
fi

echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
if [ "$PASS" -eq 0 ]; then
  echo "no case was measured -- treating as skipped rather than passed"
  exit 77
fi
# The endless-stream and over-cap cases are the ones that cover the defect.
# Passing on the controls alone is not evidence about this fix.
if [ "$DEFECT_MEASURED" -eq 0 ]; then
  echo "no endless-stream case ran (tools dir: $TOOLS_DIR) -- treating as skipped rather than passed"
  exit 77
fi
exit 0
