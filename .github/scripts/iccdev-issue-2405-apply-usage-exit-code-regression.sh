#!/bin/bash
###############################################################################
# #2405 -- the four iccApply* tools reported success for invocations that
#          applied nothing
###############################################################################
#
# All four tools guarded their operand count with "argc < minargs -> Usage();
# return 0".  Usage() printed on stdout, so an incomplete invocation was
# indistinguishable from a successful one to any caller that checks $?: the
# wrapper saw exit 0, an empty stdout was never produced, and no output file
# appeared.  Measured on master f186948c, thirteen distinct incomplete forms
# exited 0:
#
#   iccApplyProfiles   0 operands                      (minargs 2)
#   iccApplyNamedCmm   0 operands                      (minargs 2)
#   iccApplySearch     0 and 1 operands                (minargs 3)
#   iccApplyToLink     0,1,2,3,4,5,6,7,8 operands      (minargs 10)
#
# The contract applied here is the one #1514 settled for iccSpecSepToTiff (see
# iccdev-issue-1514-specsep-usage-exit-code-regression-tests.sh), which
# iccRoundTrip, iccPawgReport and now iccFromXml (#2387) also follow:
#
#   malformed / too few operands -> Usage on STDERR, non-zero, stdout empty
#   -h / --help                  -> Usage on STDOUT, exit 0,   stderr empty
#
# The STREAM is the discriminator, not the status: once a help flag exists,
# both paths print the same screen and only the stream tells them apart.  That
# is why every case below asserts the quiet stream is EMPTY -- an implementation
# that printed usage to both would satisfy a status-only test.
#
# Red/green, measured against master f186948c: 24 of the 28 cases below fail.
# All thirteen too-few-operand forms fail, each reporting exit=0.  The five
# "clears minargs but still malformed" cases fail because master answers them on
# stdout.  Of the eight help cases, the four on iccApplyProfiles and
# iccApplyNamedCmm fail (exit 255 and 1 respectively -- minargs is 2 there, so
# "-h" is a complete operand list and reaches the parser, which rejects it).
#
# The other four PASS against master, and the reason is worth stating rather
# than papering over: iccApplySearch and iccApplyToLink have minargs 3 and 10,
# so "-h" is *below* their minimum and lands in the very "Usage(); return 0"
# path this change removes.  Master satisfies the help contract there by
# accident of the defect, not by supporting a help flag -- which is exactly why
# the malformed cases, not the help cases, are what red/green this change.
# Those four assertions still earn their place: they pin the help path against
# a future regression once it is the only route to Usage() on stdout.
#
# The two "threads-then-*" cases were added later, with code-scanning alert
# 2365 (cpp/constant-comparison), and they fail
# against f186948c too -- which is why the count above is 24 of 28 rather than
# 22 of 26.  There the post-"-threads" guard is a bare "Usage(); return
# EXIT_FAILURE;" and Usage() takes no stream argument, so the screen lands on
# stdout and there is no "Missing arguments after -threads" line to match.
# They fail on the stdout assertion first, and would fail on the message one
# too.  That alert itself is not what they red/green -- see the note on
# run_threads_operand_count() below.
#
# Anti-vacuity: assert_usage_screen() pins a row that this change does not
# touch, so a truncated or empty capture cannot satisfy the stream assertions
# on its own; and the run exits 77 rather than 0 if no case was measured.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-apply-usage-exit-code-regression}"
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

STDOUT_LOG="$OUTDIR/apply-usage.stdout.log"
STDERR_LOG="$OUTDIR/apply-usage.stderr.log"

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

# Pin the LAST row of the tool's screen.  Proving the final row arrived proves
# the capture holds a whole usage screen, not a truncated fragment that would
# satisfy the "other stream is empty" assertions by being nearly empty itself.
# The four screens do not end alike -- iccApplySearch has no gamut row at all,
# and its intent table stops at 100 -- so the anchor is per tool rather than one
# row assumed common to all, which is what a first version of this test assumed
# and got wrong.
#
# Passed in by each caller rather than read from a global: as a global it was
# assigned by the per-tool table loop and any case running outside that loop
# silently inherited the last entry's row (found while fixing alert 2365).
#
# Matched as a whole line with -F against a leading-whitespace-stripped copy:
# every anchor below contains "+", "(", ")" or "/", all ERE metacharacters, so a
# regex would quietly stop meaning what it reads as (the same reasoning as the
# #2262 row assertions).  The tools indent these rows differently.
assert_usage_screen() {
  local name="$1" log="$2" where="$3" anchor="$4"
  sed 's/^[[:space:]]*//' "$log" > "$log.stripped"
  if ! grep -Fqx "$anchor" "$log.stripped" 2>/dev/null; then
    fail_case "$name" "no complete usage screen on $where (last row \"$anchor\" absent)"
    sed -n '1,10p' "$log"
    return 1
  fi
  return 0
}

# A malformed invocation: non-zero status, usage on stderr, stdout untouched.
run_malformed() {
  local name="$1" tool="$2" anchor="$3"; shift 3
  TOTAL=$((TOTAL + 1))
  local exit_code=0

  : > "$STDOUT_LOG"; : > "$STDERR_LOG"
  timeout 60 "$tool" "$@" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  # A crash also produces a non-zero status, so exclude the signal range rather
  # than accepting any non-zero as proof the guard fired.
  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "incomplete invocation reported success (exit=0)"
    return
  fi
  if [ -s "$STDOUT_LOG" ]; then
    fail_case "$name" "malformed invocation wrote $(wc -c < "$STDOUT_LOG") bytes to stdout"
    sed -n '1,5p' "$STDOUT_LOG"
    return
  fi
  if ! grep -q "Missing arguments" "$STDERR_LOG" 2>/dev/null; then
    fail_case "$name" "no 'Missing arguments' diagnostic on stderr"
    sed -n '1,5p' "$STDERR_LOG"
    return
  fi
  assert_usage_screen "$name" "$STDERR_LOG" "stderr" "$anchor" || return

  pass_case "$name" "rejected with exit=$exit_code, usage on stderr, stdout empty"
}

# A malformed invocation that is NOT short of minargs, so it is rejected by the
# argument parser rather than the operand-count guard and prints no usage screen.
# These exist because the operand-count sweep above cannot reach them, and
# because this is where the four tools most easily drift apart: the same
# diagnostic is emitted by all of them, and before #2405 completed the contract
# iccApplyProfiles printed it on stderr while its two siblings used stdout.
# Asserts the stream and the status, not the wording.
run_malformed_no_usage() {
  local name="$1" tool="$2"; shift 2
  TOTAL=$((TOTAL + 1))
  local exit_code=0

  : > "$STDOUT_LOG"; : > "$STDERR_LOG"
  timeout 60 "$tool" "$@" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "malformed invocation reported success (exit=0)"
    return
  fi
  if [ -s "$STDOUT_LOG" ]; then
    fail_case "$name" "malformed invocation wrote $(wc -c < "$STDOUT_LOG") bytes to stdout"
    sed -n '1,5p' "$STDOUT_LOG"
    return
  fi
  if [ ! -s "$STDERR_LOG" ]; then
    fail_case "$name" "malformed invocation produced no diagnostic on stderr"
    return
  fi

  pass_case "$name" "rejected with exit=$exit_code, diagnostic on stderr, stdout empty"
}

# The operand-count guard that runs AFTER "-threads N" has been consumed.
# iccApplySearch shifts its vector ("argv += 2; argc -= 2") and then repeats the
# minargs test, so the count in that second message comes from a different
# expression than the one the sweep above covers -- and nothing reached it: the
# "threads-no-count" case below stops at the earlier "Missing thread count"
# guard, which returns before the shift.
#
# These cases do not red/green alert 2365.  That change removed an "argc > 0 ?" clamp
# whose else branch was already unreachable -- the minargs test has established
# argc >= 3 before the shift, so argc >= 1 after it -- and the output is
# identical before and after it (verified on all five reachable forms).  What
# they pin is the arithmetic the clamp was standing in front of: "-threads 4"
# leaves argc == 1, the smallest value that can reach the line, so a rewrite
# that dropped the "- 1" or clamped on the wrong side would print 1 or 0 here
# rather than 0 and 1.  Asserts the whole line with -F: the message has no
# regex metacharacters but the count is the point, and a substring match on
# "received" would accept any number.
run_threads_operand_count() {
  local name="$1" tool="$2" anchor="$3" expected="$4"; shift 4
  TOTAL=$((TOTAL + 1))
  local exit_code=0

  : > "$STDOUT_LOG"; : > "$STDERR_LOG"
  timeout 60 "$tool" "$@" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -le 192 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "incomplete invocation reported success (exit=0)"
    return
  fi
  if [ -s "$STDOUT_LOG" ]; then
    fail_case "$name" "malformed invocation wrote $(wc -c < "$STDOUT_LOG") bytes to stdout"
    sed -n '1,5p' "$STDOUT_LOG"
    return
  fi

  local want="Missing arguments after -threads: expected at least 2, received ${expected}."
  if ! grep -Fqx "$want" "$STDERR_LOG" 2>/dev/null; then
    fail_case "$name" "stderr does not carry \"$want\""
    sed -n '1,5p' "$STDERR_LOG"
    return
  fi
  assert_usage_screen "$name" "$STDERR_LOG" "stderr" "$anchor" || return

  pass_case "$name" "reported received=$expected, usage on stderr, stdout empty"
}

# A help request: exit 0, usage on stdout, stderr untouched.
run_help() {
  local name="$1" tool="$2" anchor="$3" flag="$4"
  TOTAL=$((TOTAL + 1))
  local exit_code=0

  : > "$STDOUT_LOG"; : > "$STDERR_LOG"
  timeout 60 "$tool" "$flag" > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -ne 0 ]; then
    fail_case "$name" "expected exit=0 for $flag, got exit=$exit_code"
    sed -n '1,5p' "$STDERR_LOG"
    return
  fi
  if [ -s "$STDERR_LOG" ]; then
    fail_case "$name" "$flag wrote $(wc -c < "$STDERR_LOG") bytes to stderr"
    sed -n '1,5p' "$STDERR_LOG"
    return
  fi
  assert_usage_screen "$name" "$STDOUT_LOG" "stdout" "$anchor" || return

  pass_case "$name" "$flag printed usage on stdout and exited 0"
}

echo "=== iccApply* usage-path exit-code regression (#2405) ==="

PROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
SEARCH="$TOOLS_DIR/IccApplySearch/iccApplySearch"
NAMEDCMM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
TOLINK="$TOOLS_DIR/IccApplyToLink/iccApplyToLink"

# Operands for the prefix sweep.  They are never consumed -- every form below is
# short of the tool's minimum -- so they only have to be present and inert.
ARGS=(out.icc 0 33 0 title 0 1 0 0)

# name | binary | highest incomplete operand count | last row of its screen
for entry in "iccApplyProfiles|$PROFILES|0|+10000 - Use V5 sub-profile if present" \
             "iccApplyNamedCmm|$NAMEDCMM|0|only one of +1000000 / +2000000 may be given)" \
             "iccApplySearch|$SEARCH|1|+10000 - Use V5 sub-profile if present" \
             "iccApplyToLink|$TOLINK|8|+1000 - Use V5 sub-profile if present"; do
  IFS='|' read -r name tool maxops anchor <<< "$entry"

  if [ ! -x "$tool" ]; then
    skip_case "$name" "not built at $tool"
    continue
  fi

  # Every incomplete prefix, not just the bare invocation: #2405 found that
  # iccApplyToLink accepted nine of them and iccApplySearch two, so a test that
  # only ran the tool with no operands would leave most of the defect uncovered.
  for n in $(seq 0 "$maxops"); do
    run_malformed "$name $n-operand" "$tool" "$anchor" "${ARGS[@]:0:$n}"
  done

  run_help "$name short help" "$tool" "$anchor" -h
  run_help "$name long help" "$tool" "$anchor" --help
done

# Forms that clear minargs but are still malformed.  One operand is a complete
# operand list for iccApplyProfiles and iccApplyNamedCmm (minargs 2), so these
# reach the configuration parser; "-threads" with no count is the guard whose
# twin lives in a different tool.  All three tools must answer on the same
# stream -- that they did not is what this half of the test pins.
DATA="$OUTDIR/incomplete-operand.txt"
printf 'data\n' > "$DATA"

if [ -x "$PROFILES" ]; then
  run_malformed_no_usage "iccApplyProfiles unparseable-args" "$PROFILES" "$DATA"
  run_malformed_no_usage "iccApplyProfiles threads-no-count" "$PROFILES" -threads
fi
if [ -x "$NAMEDCMM" ]; then
  run_malformed_no_usage "iccApplyNamedCmm unparseable-args" "$NAMEDCMM" "$DATA"
fi
if [ -x "$SEARCH" ]; then
  run_malformed_no_usage "iccApplySearch unparseable-args" "$SEARCH" "$DATA" 0
  run_malformed_no_usage "iccApplySearch threads-no-count" "$SEARCH" -threads
  # argc == 1 and argc == 2 after the shift: the only two values that reach the
  # post-"-threads" guard (a third operand clears minargs and goes to the parser).
  #
  # The anchor is passed in rather than inherited: it used to be a global that
  # the per-tool table loop above assigned, so out here it still held whichever
  # row the LAST entry set -- iccApplyToLink's "+1000", not iccApplySearch's
  # "+10000" -- and assert_usage_screen() looked for a row this tool never
  # prints and failed for the wrong reason.  It is now a parameter, so a case
  # added anywhere has to name the screen it expects.
  run_threads_operand_count "iccApplySearch threads-then-nothing" "$SEARCH" \
    "+10000 - Use V5 sub-profile if present" 0 -threads 4
  run_threads_operand_count "iccApplySearch threads-then-one-operand" "$SEARCH" \
    "+10000 - Use V5 sub-profile if present" 1 -threads 4 -cfg
fi

echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

# A run that measured nothing must not report green: with all four tools absent
# every case is skipped and the loops above would otherwise fall through to
# exit 0, which is the exact "reported success having done nothing" defect this
# suite exists to pin.
if [ "$PASS" -eq 0 ]; then
  echo "no case was measured -- treating as skipped rather than passed"
  exit 77
fi

exit 0
