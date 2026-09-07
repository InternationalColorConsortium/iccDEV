#!/bin/bash
###############################################################################
# #2453 -- iccDumpProfile reported success for every failure unless -v was given
###############################################################################
#
# nValid was assigned in exactly one place: the switch on nStatus inside the
# "if (bDumpValidation)" block, which only -v opens.  Without -v the tool
# returned the initialiser 0 for every outcome, so a caller checking $? could
# not tell a successful dump from a file that does not exist:
#
#   iccDumpProfile /nonexistent.icc ; echo $?   -> 0   (prints "Unable to parse")
#   iccDumpProfile <a directory>    ; echo $?   -> 0
#   iccDumpProfile <200 random bytes>; echo $?  -> 0
#   iccDumpProfile <a valid profile>; echo $?   -> 0   (indistinguishable)
#
# This is the #2405 family -- "an incomplete or failed invocation returns
# success" -- reaching a tool #2421 did not cover, by a different route: a
# discarded status rather than a "Usage(); return 0".
#
# SCOPE, and the reason case 8 asserts exit 0 rather than a failure.
# Without -v no validation pass ever runs: the plain branch only opens the
# profile.  So the fix reports PARSE failure, not validity, and a profile that
# parses but is critically invalid still exits 0 on the plain path.  Case 8
# pins that deliberately, using a tracked fixture that exits 0 plain and 255
# under -v.  Making the plain path genuinely validate is the larger option in
# #2453 and needs a maintainer ruling; if it ever lands, case 8 is the
# assertion that must be consciously flipped rather than silently broken.
#
# Note for anyone extending this: nStatus is NOT left at icValidateOK on the
# plain path.  ONE icMaxStatus site runs without -v -- the duplicate-tag-signature
# check at iccDumpProfile.cpp:863 -- and can raise it to icValidateWarning.  The
# six others (:896 through :981) sit inside the bDumpValidation block opened at
# :883 and run only with -v.  The switch maps Warning and NonCompliant alike to
# 0, which is why the fix keys off the parse failure instead: it changes exactly
# the invocations that printed "Unable to parse".
#
# WHY THE FIX RETURNS 1 AND NOT -1, pinned by cases 9-12 below.  A -1 return is
# shell status 255, and iccdev-fuzz-triage.sh classify_log(), the classify_exit()
# in icc-tool-qa-scan-common.sh, and the sweep in _build-test-unix.yml all read
# >= 128 as "died on a signal".  The fuzz triage replays AFL findings as
# `<tool> <input> ALL`, and those inputs are unparseable by construction, so 255
# would reclassify every one from "clean" to a fabricated "signal" and redden the
# lane.  1 is graceful-fail / FAIL, which neither harness counts as a failure.
#
# Red/green, measured against master bb172dd9: cases 1-4 FAIL (each exits 0);
# cases 5-8 PASS on both builds and exist to pin the no-regression edges.
#
# Corpus: with the fix applied, 269 profiles in a freshly generated tree (105
# tracked + 164 produced by iccdev.create-profiles and iccdev.hybrid-pipeline)
# were dumped on the plain path.  Exit status changed for none of them.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-dumpprofile-exit-code-regression}"
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

DUMP="$TOOLS_DIR/IccDumpProfile/iccDumpProfile"
if [ ! -x "$DUMP" ]; then
  echo "iccDumpProfile not found at $DUMP; skipping" >&2
  exit 77
fi

VALID_FIXTURE="$REPO_ROOT/.github/ci/regression/gamma-1.0000000000.icc"
PARSES_BUT_INVALID="$REPO_ROOT/.github/ci/regression/issue-1103-calculator-window-underflow.icc"
for f in "$VALID_FIXTURE" "$PARSES_BUT_INVALID"; do
  if [ ! -f "$f" ]; then
    echo "fixture missing: $f" >&2
    exit 77
  fi
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
head -c 200 /dev/urandom > "$WORK/random.bin"
printf 'not a profile\n' > "$WORK/text.txt"
mkdir -p "$WORK/adirectory"

PASS=0
FAIL=0
RAN=0

# expect_rc <label> <mode> <expectation> <path>
#   mode: "zero" | "nonzero"
expect_rc() {
  local label="$1" mode="$2"; shift 2
  local rc slug logf
  # Keep each case's output under ICCDEV_TEST_OUTDIR.  A red case otherwise
  # reports only "expected non-zero, got 0" with nothing to triage from.
  slug="$(printf '%s' "$label" | tr -c 'A-Za-z0-9._-' '-')"
  logf="$OUTDIR/$slug.log"
  "$@" > "$logf" 2>&1
  rc=$?
  RAN=$((RAN + 1))
  if [ "$mode" = "nonzero" ]; then
    if [ "$rc" -ne 0 ]; then
      echo "PASS  $label (exit $rc)"
      PASS=$((PASS + 1))
    else
      echo "FAIL  $label -- expected non-zero, got 0"
      echo "        output: $logf"
      FAIL=$((FAIL + 1))
    fi
  else
    if [ "$rc" -eq 0 ]; then
      echo "PASS  $label (exit 0)"
      PASS=$((PASS + 1))
    else
      echo "FAIL  $label -- expected 0, got $rc"
      echo "        output: $logf"
      FAIL=$((FAIL + 1))
    fi
  fi
}

echo "== #2453: the plain path must report a failure it already printed =="
expect_rc "1. plain, missing file"            nonzero "$DUMP" "$WORK/does-not-exist.icc"
expect_rc "2. plain, a directory"             nonzero "$DUMP" "$WORK/adirectory"
expect_rc "3. plain, 200 random bytes"        nonzero "$DUMP" "$WORK/random.bin"
expect_rc "4. plain, a text file"             nonzero "$DUMP" "$WORK/text.txt"

echo "== edges that must NOT move =="
expect_rc "5. plain, valid profile"           zero    "$DUMP" "$VALID_FIXTURE"
expect_rc "6. -v, valid profile"              zero    "$DUMP" -v 100 "$VALID_FIXTURE"
expect_rc "7. -v, missing file"               nonzero "$DUMP" -v 100 "$WORK/does-not-exist.icc"

echo "== scope: parse failure is reported, validity still is not (see header) =="
expect_rc "8. plain, parses but -v says critical" zero "$DUMP" "$PARSES_BUT_INVALID"

# The CI callers all invoke `<tool> <input> ALL`, never a bare filename, and ALL
# reaches a different block (argc > nArg + 1) with its own return path.  These
# also pin the code AWAY from the >= 128 signal band the harnesses treat as a
# crash -- a fix returning -1 passes cases 1-4 and fails 9-11.
echo "== the 'ALL' shape every CI caller uses, and the value of the code =="
expect_rc "9. ALL, missing file"               nonzero "$DUMP" "$WORK/does-not-exist.icc" ALL
expect_rc "10. ALL, 200 random bytes"          nonzero "$DUMP" "$WORK/random.bin" ALL
expect_rc "11. ALL, valid profile"             zero    "$DUMP" "$VALID_FIXTURE" ALL

# The exit code must stay below the signal band on every malformed shape.
echo "== exit code must not land in the >= 128 signal band =="
for _case in "$WORK/does-not-exist.icc" "$WORK/adirectory" "$WORK/random.bin"; do
  "$DUMP" "$_case" ALL > "$OUTDIR/band.log" 2>&1
  _rc=$?
  RAN=$((RAN + 1))
  if [ "$_rc" -ne 0 ] && [ "$_rc" -lt 128 ] && [ "$_rc" -ne 124 ]; then
    echo "PASS  12. non-signal exit for $(basename "$_case") (exit $_rc)"
    PASS=$((PASS + 1))
  else
    echo "FAIL  12. $(basename "$_case") exited $_rc -- 0 is success, >=128 reads as a"
    echo "        signal to iccdev-fuzz-triage.sh and icc-tool-qa-scan-common.sh"
    FAIL=$((FAIL + 1))
  fi
done

echo
echo "cases run: $RAN  passed: $PASS  failed: $FAIL"
if [ "$RAN" -eq 0 ]; then
  echo "no case ran" >&2
  exit 77
fi
[ "$FAIL" -eq 0 ] || exit 1
exit 0
