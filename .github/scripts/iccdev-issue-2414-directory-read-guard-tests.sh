#!/bin/bash
###############################################################################
# #2414 -- fopen() succeeds on a directory, so a directory argument is not
#          rejected where the tool believes it opened a file
###############################################################################
#
# #2411 found this in iccFromXml: -v=<some-directory> slipped past an openability
# guard, because fopen(dir, "r") succeeds on glibc, and produced a four-line
# libxml2 cascade naming the XML file instead of the one-line schema error.  The
# fix was a read-a-byte discriminator -- a directory fails the read with EISDIR,
# a legitimately empty file reports EOF with no error.  It was static in
# IccFromXml.cpp, so no other tool could reach it; it now lives in
# IccProfLib/IccFileUtil.h as icIsReadableFile(), beside the write-side
# icOpenRegularWriteFile().
#
# Sweeping the other tools for the same shape turned up a worse instance than
# the one that started it.  iccFromCube HUNG on a directory:
#
#   isEOF() tested feof(m_f) alone.  A read that FAILS -- EISDIR here -- makes
#   fgetc() return EOF while leaving feof() FALSE and setting ferror() instead,
#   so getNextLine() returned an empty string forever and the three
#   `while (!isEOF())` loops never advanced.  Measured on ee14d031, exit 124
#   under any timeout, spinning with no allocation growth (CWE-835).
#
# The fix is the ferror() arm alone.  The tidier-looking version -- probe the
# path with icIsReadableFile() before open() -- is a REGRESSION, which is why
# case (6) exists: the probe opens, reads and closes, so the second fopen() of a
# single-reader FIFO blocks on a writer that has already been consumed.  That
# guard took a FIFO from 254 to 124, trading one hang for another.  Validate the
# stream you are holding, not the path you are about to open again.
#
# Anti-vacuity, in two places.  "Did not hang" is trivially satisfied by a tool
# that fails for an unrelated reason, so the directory cases also require the
# diagnostic that names the path.  And a guard that is too eager would be
# invisible to every defect case, so the controls carry as much weight: a real
# .cube must still convert, and /dev/null must still be ACCEPTED and then
# refused by the PARSER, which is the case an S_ISREG guard would have broken.
# Nothing else pins that: iccdev.issue-1178-fromcube-devnull passes /dev/null as
# the OUTPUT profile path (`iccFromCube "$POC" /dev/null`), never as the input
# .cube, so case (5) here is the only cover for the input side.
#
# The suite ALSO has to be able to report that it measured nothing.  Resolving
# TOOLS_DIR on the directory rather than on the binaries let a configured tree
# with no executables under Build/Tools skip all five iccFromCube cases and
# still exit 0, carried by the single iccFromXml case.  Hence tools_dir_has_both
# below, and the FROMCUBE_MEASURED gate at the end.
#
# Red/green against ee14d031: fromcube-directory-terminates and
# fromxml-schema-fifo fail with rc=124, and fromcube-directory-names-path finds
# no diagnostic.  The other five cases pass on BOTH builds -- that is what they
# are for.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# Whether the caller named the tree.  An explicit ICCDEV_TOOLS_DIR is never
# second-guessed below: silently resolving past it would measure a DIFFERENT
# build than the one the caller meant, which is how a stale tree fakes a result.
if [ -n "${ICCDEV_TOOLS_DIR:-}" ]; then
  TOOLS_DIR="$ICCDEV_TOOLS_DIR"
  TOOLS_DIR_EXPLICIT=1
else
  TOOLS_DIR="$REPO_ROOT/Build/Tools"
  TOOLS_DIR_EXPLICIT=0
fi
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2414-directory-read-guard}"
mkdir -p "$OUTDIR"

# Resolve on the BINARIES, not on the directory.  Build/Tools exists in a
# configured tree as CMake scaffolding with no executables under it, so a
# directory test resolved TOOLS_DIR to a path where nothing is built and every
# case below skipped -- and the suite still exited 0, because the "nothing was
# measured" guard was satisfied by the one iccFromXml case.  A green run that
# skipped every case covering the defect is the failure mode this whole file
# exists to prevent, so the probe has to be for the thing it will execute.
tools_dir_has_both() {
  [ -x "$1/IccFromCube/iccFromCube" ] && [ -x "$1/IccFromXml/iccFromXml" ]
}
tools_dir_has_any() {
  [ -x "$1/IccFromCube/iccFromCube" ] || [ -x "$1/IccFromXml/iccFromXml" ]
}

# Two passes, and the order matters.  A tree can hold one tool and not the other
# -- Build/Tools in a working checkout often has stale binaries for some tools
# only -- and settling for it would skip the cases that cover the defect while
# another tree next to it has everything.  Prefer a complete tree, and fall back
# to a partial one only so the skip accounting at the end can report what was
# missing.
if [ "$TOOLS_DIR_EXPLICIT" -eq 0 ] && ! tools_dir_has_both "$TOOLS_DIR"; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/out/build/Tools"; do
    if tools_dir_has_both "$candidate"; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

if [ "$TOOLS_DIR_EXPLICIT" -eq 0 ] && ! tools_dir_has_any "$TOOLS_DIR"; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/out/build/Tools"; do
    if tools_dir_has_any "$candidate"; then
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
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"

CUBE_OK="$REPO_ROOT/.github/ci/test-data/test-identity.cube"
XML_OK="$REPO_ROOT/Testing/Calc/CameraModel.xml"

# The directory every case is pointed at.  Named so it cannot be mistaken for a
# fixture that was meant to exist.
ADIR="$OUTDIR/a-directory-not-a-file"
rm -rf "$ADIR"
mkdir -p "$ADIR"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Counted separately from TOTAL: the iccFromCube cases are the ones that cover
# the defect, so the suite must not be able to report success without them.
FROMCUBE_MEASURED=0

fail_case() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
pass_case() { echo "  [PASS] $1 -- $2"; PASS=$((PASS + 1)); }
skip_case() { echo "  [SKIP] $1 -- $2"; SKIP=$((SKIP + 1)); }

# ---------------------------------------------------------------------------
# iccFromCube
# ---------------------------------------------------------------------------
if [ ! -x "$FROMCUBE" ]; then
  skip_case fromcube-directory-terminates "not built at $FROMCUBE"
  skip_case fromcube-directory-names-path "not built"
  skip_case fromcube-regular-file-converts "not built"
  skip_case fromcube-missing-file-unchanged "not built"
  skip_case fromcube-devnull-unchanged "not built"
else
  # (1) The defect: a directory must not spin.  20s is far above the ~0.1s a
  # real conversion of this fixture takes, so a timeout here means the loop,
  # not a slow machine.
  LOG="$OUTDIR/fromcube-directory.log"
  rc=0
  timeout 20 "$FROMCUBE" "$ADIR" "$OUTDIR/from-directory.icc" > "$LOG" 2>&1 || rc=$?
  TOTAL=$((TOTAL + 1)); FROMCUBE_MEASURED=$((FROMCUBE_MEASURED + 1))
  if [ "$rc" -eq 124 ]; then
    fail_case fromcube-directory-terminates "timed out (rc=124) -- the isEOF() spin is back"
  elif [ "$rc" -eq 0 ]; then
    fail_case fromcube-directory-terminates "reported success on a directory (rc=0)"
  else
    pass_case fromcube-directory-terminates "refused with rc=$rc"
  fi

  # (2) Anti-vacuity for (1): the refusal must be the tool naming the path, not
  # some earlier unrelated failure.
  TOTAL=$((TOTAL + 1))
  if grep -q -- "$ADIR" "$LOG" 2>/dev/null; then
    pass_case fromcube-directory-names-path "diagnostic names the directory"
  else
    fail_case fromcube-directory-names-path "no diagnostic naming the path -- nothing was measured"
    sed -n '1,10p' "$LOG"
  fi

  # (3) Control: the guard must not refuse a real .cube.
  if [ -f "$CUBE_OK" ]; then
    TOTAL=$((TOTAL + 1))
    OUT_ICC="$OUTDIR/identity.icc"
    rm -f "$OUT_ICC"
    rc=0
    timeout 60 "$FROMCUBE" "$CUBE_OK" "$OUT_ICC" > "$OUTDIR/fromcube-ok.log" 2>&1 || rc=$?
    if [ "$rc" -eq 0 ] && [ -s "$OUT_ICC" ]; then
      pass_case fromcube-regular-file-converts "converted, $(wc -c < "$OUT_ICC") bytes"
    else
      fail_case fromcube-regular-file-converts "rc=$rc, output $( [ -s "$OUT_ICC" ] && echo present || echo "missing/empty")"
      sed -n '1,10p' "$OUTDIR/fromcube-ok.log"
    fi
  else
    skip_case fromcube-regular-file-converts "fixture missing: $CUBE_OK"
  fi

  # (4) Control: a missing file was already refused cleanly and must stay that
  # way -- the guard returns false for it too, by a different route.
  TOTAL=$((TOTAL + 1))
  MISSING="$OUTDIR/no-such-file.cube"
  rm -f "$MISSING"
  rc=0
  timeout 20 "$FROMCUBE" "$MISSING" "$OUTDIR/missing.icc" > "$OUTDIR/fromcube-missing.log" 2>&1 || rc=$?
  if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ] && grep -q -- "$MISSING" "$OUTDIR/fromcube-missing.log" 2>/dev/null; then
    pass_case fromcube-missing-file-unchanged "refused with rc=$rc and named the path"
  else
    fail_case fromcube-missing-file-unchanged "rc=$rc"
    sed -n '1,10p' "$OUTDIR/fromcube-missing.log"
  fi

  # (5) Control: /dev/null reads EOF with no error, so the guard must ACCEPT it
  # and leave it to the parser to refuse.  A guard written as S_ISREG would fail
  # this case, which is why it is here.
  if [ -c /dev/null ]; then
    TOTAL=$((TOTAL + 1))
    rc=0
    timeout 20 "$FROMCUBE" /dev/null "$OUTDIR/devnull.icc" > "$OUTDIR/fromcube-devnull.log" 2>&1 || rc=$?
    # rc alone cannot tell the two apart: a guard that refused /dev/null outright
    # would also exit non-zero, so this case would pass against exactly the
    # implementation it exists to rule out.  The discriminator is WHICH message
    # comes back -- the parser's, naming the file it tried to read.
    if [ "$rc" -eq 0 ] || [ "$rc" -eq 124 ]; then
      fail_case fromcube-devnull-unchanged "rc=$rc"
      sed -n '1,10p' "$OUTDIR/fromcube-devnull.log"
    elif grep -q "Unable to parse .*/dev/null" "$OUTDIR/fromcube-devnull.log" 2>/dev/null; then
      pass_case fromcube-devnull-unchanged "reached the parser and was refused there, rc=$rc"
    else
      fail_case fromcube-devnull-unchanged "rc=$rc but not the parser's refusal -- a guard rejected it before the parse"
      sed -n '1,10p' "$OUTDIR/fromcube-devnull.log"
    fi
  else
    skip_case fromcube-devnull-unchanged "/dev/null is not a character device here"
  fi

  # (6) Regression guard for the FIX ITSELF, not for the defect.
  #
  # The tidier-looking version of this fix probed the path with
  # icIsReadableFile() before open() -- open, read a byte, close, then fopen()
  # again.  On a single-reader FIFO the probe consumes the writer's rendezvous
  # and the SECOND open blocks forever, so that guard turned a directory fix
  # into a pipe hang: 254 before it, 124 with it.  A named pipe is the cheapest
  # input that tells a path probe apart from a handle check, which is why it is
  # a case here rather than a comment.
  # Remove any leftover first.  mkfifo FAILS when the path exists, and OUTDIR
  # persists across runs, so one interrupted run (a CTest TIMEOUT kill, Ctrl-C)
  # left a stale FIFO that skipped this case on every later run -- permanently
  # disabling the guard that protects the fix, while the suite still exited 0.
  rm -f "$OUTDIR/fifo-probe"
  if ! command -v mkfifo > /dev/null 2>&1; then
    skip_case fromcube-fifo-terminates "mkfifo not available on this platform"
  elif ! mkfifo "$OUTDIR/fifo-probe" 2>/dev/null; then
    skip_case fromcube-fifo-terminates "mkfifo failed in $OUTDIR (filesystem does not support FIFOs?)"
  else
    TOTAL=$((TOTAL + 1)); FROMCUBE_MEASURED=$((FROMCUBE_MEASURED + 1))
    if [ -f "$CUBE_OK" ]; then
      ( cat "$CUBE_OK" > "$OUTDIR/fifo-probe" 2>/dev/null ) &
      WRITER=$!
      rc=0
      timeout 20 "$FROMCUBE" "$OUTDIR/fifo-probe" "$OUTDIR/fifo.icc" \
        > "$OUTDIR/fromcube-fifo.log" 2>&1 || rc=$?
      kill "$WRITER" 2>/dev/null
      wait "$WRITER" 2>/dev/null
      if [ "$rc" -eq 124 ]; then
        fail_case fromcube-fifo-terminates "timed out (rc=124) -- a path probe was reintroduced before open()"
      else
        pass_case fromcube-fifo-terminates "terminated with rc=$rc"
      fi
    else
      skip_case fromcube-fifo-terminates "fixture missing: $CUBE_OK"
      TOTAL=$((TOTAL - 1)); FROMCUBE_MEASURED=$((FROMCUBE_MEASURED - 1))
    fi
    rm -f "$OUTDIR/fifo-probe"
  fi
fi

# ---------------------------------------------------------------------------
# iccFromXml -- the #2411 case, now going through the promoted helper.  This is
# a no-behaviour-change assertion: it must still be the one-line schema error.
# ---------------------------------------------------------------------------
if [ ! -x "$FROMXML" ]; then
  skip_case fromxml-schema-directory "not built at $FROMXML"
elif [ ! -f "$XML_OK" ]; then
  skip_case fromxml-schema-directory "fixture missing: $XML_OK"
else
  TOTAL=$((TOTAL + 1))
  LOG="$OUTDIR/fromxml-schema-directory.log"
  rc=0
  timeout 60 "$FROMXML" "$XML_OK" "$OUTDIR/fromxml.icc" "-v=$ADIR" > "$LOG" 2>&1 || rc=$?

  # The libxml2 cascade this replaced was four lines naming the XML file; the
  # contract is one line naming the SCHEMA.  Counting lines is what tells the
  # two apart, so both halves are asserted.
  LINES=$(wc -l < "$LOG" | tr -d ' ')
  if [ "$rc" -eq 0 ]; then
    fail_case fromxml-schema-directory "accepted a directory as a schema (rc=0)"
  elif ! grep -q "schema" "$LOG" 2>/dev/null; then
    fail_case fromxml-schema-directory "refused with rc=$rc but did not name the schema"
    sed -n '1,10p' "$LOG"
  elif [ "$LINES" -gt 2 ]; then
    fail_case fromxml-schema-directory "$LINES lines of output -- the libxml2 cascade is back"
    sed -n '1,10p' "$LOG"
  else
    pass_case fromxml-schema-directory "one-line schema error, rc=$rc"
  fi

  # A FIFO as the schema, which is the other half of the same defect and the one
  # the promoted helper had to grow an S_ISREG arm for.  iccFromXml DOES probe
  # the path before handing it to libxml2, so it hung both ways: with no writer
  # the probe's own open blocks, and with a single writer the probe eats the
  # rendezvous and libxml2's reopen blocks.  Measured at 00b91b56: rc=124 for
  # both.  Only the writer-less case is exercised here -- it needs no background
  # process, so there is nothing to leak if the tool hangs and the case fails.
  #
  # The writer-less case is enough to pin the S_ISREG arm, which is the part of
  # icIsReadableFile() that is easiest to delete believing it redundant.  Deleting
  # it and keeping O_NONBLOCK does NOT make this case pass: read() on a
  # writer-less FIFO returns 0, not EAGAIN, so the guard accepts the path and
  # libxml2's own blocking reopen then hangs.  Measured: rc=1 with the arm, 124
  # without it.
  rm -f "$OUTDIR/schema-fifo"
  if ! command -v mkfifo > /dev/null 2>&1; then
    skip_case fromxml-schema-fifo "mkfifo not available on this platform"
  elif ! mkfifo "$OUTDIR/schema-fifo" 2>/dev/null; then
    skip_case fromxml-schema-fifo "mkfifo failed in $OUTDIR (filesystem does not support FIFOs?)"
  else
    TOTAL=$((TOTAL + 1))
    rc=0
    timeout 20 "$FROMXML" "$XML_OK" "$OUTDIR/fromxml-fifo.icc" "-v=$OUTDIR/schema-fifo" \
      > "$OUTDIR/fromxml-schema-fifo.log" 2>&1 || rc=$?
    if [ "$rc" -eq 124 ]; then
      fail_case fromxml-schema-fifo "timed out (rc=124) -- a writer-less FIFO reached a blocking open, in the guard or in libxml2 after it"
    elif [ "$rc" -eq 0 ]; then
      fail_case fromxml-schema-fifo "accepted a FIFO as a schema (rc=0)"
    elif grep -q "schema" "$OUTDIR/fromxml-schema-fifo.log" 2>/dev/null; then
      pass_case fromxml-schema-fifo "refused with rc=$rc, naming the schema"
    else
      fail_case fromxml-schema-fifo "refused with rc=$rc but did not name the schema"
      sed -n '1,10p' "$OUTDIR/fromxml-schema-fifo.log"
    fi
    rm -f "$OUTDIR/schema-fifo"
  fi
fi

echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
if [ "$PASS" -eq 0 ]; then
  echo "no case was measured -- treating as skipped rather than passed"
  exit 77
fi
# The iccFromCube cases are the ones that cover the defect.  Passing on the
# iccFromXml case alone is not evidence about this fix, so report a skip rather
# than a pass -- a green tick with every relevant case skipped is worse than no
# tick at all.
if [ "$FROMCUBE_MEASURED" -eq 0 ]; then
  echo "no iccFromCube case ran (tools dir: $TOOLS_DIR) -- treating as skipped rather than passed"
  exit 77
fi
exit 0
