#!/bin/bash
###############################################################################
# #2262 -- the BRDF rendering-intent lines printed by Usage()
###############################################################################
#
# Three tools publish the same decimal rendering-intent table -- iccApplyNamedCmm,
# iccApplyProfiles and iccApplyToLink -- and all three carried the same two
# defects on its BRDF rows.
#
#   1. The acronym was transposed, B-D-R-F.  ICC.2-2023 9.2.14-17 and 9.2.26-29
#      spell the tags brdfAToB0Tag..brdfDToB3Tag, and the library's own
#      identifiers have always agreed (icXformLutBRDFParam, icSigBRDFDToB0Tag,
#      CIccStructBRDF).  Only the three Usage() screens disagreed.
#
#   2. Codes 50/60/70/80 were printed as flat values, while the structurally
#      identical Preview row above them is printed as "20 + Intent".  All four
#      do take an intent in their units digit: CIccXform::Create() indexes a
#      four-entry tag array with nTagIntent in each of the
#      icXformLutBRDFParam..icXformLutMCS cases, and the decode that feeds it
#      falls through its type switch for 5..8 so the units digit reaches the
#      intent unchanged.  The omission is not cosmetic -- it is what produced
#      the "51/61/71/81 are bogus codes" finding that had to be refuted on
#      #2261, i.e. it had already cost a review round before it was fixed.
#
# The two halves are asserted differently, and deliberately so:
#
#   - The Usage() assertions below are what red/green this change.  Every one
#     of them fails against master, where all three tools print the four rows as
#     flat values with the acronym transposed, and iccApplyNamedCmm additionally
#     names the three transforms Model/Light/Output.
#   - The decode assertions do NOT fail against master: the decoder was always
#     right, which is precisely why the printed table was wrong rather than the
#     behaviour.  They are here as the evidence that the corrected text is
#     true, and as the guard against the other repair someone could reach for
#     -- making the tool reject 51/61/71/81 to match the old wording.  That is
#     the regression this half bites on, and it would remove reachable function.
#
# Registered as a script test, so it does not run on Windows (script tests are
# not registered there at all).  The text under test is emitted by a printf
# block with no platform-conditional code, so a Linux/macOS lane covers it.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for generated logs/configs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2262-brdf-usage}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

# Tested before the cd rather than after it.  Under "set -e" a command
# substitution whose cd fails aborts the script right here with no output at
# all, which makes the "-n" guard below dead code and hides require_tool's
# "missing executable" diagnostic behind a bare exit 1 -- in CI that reads as
# "failed, no reason".  Guarding first leaves BUILD_ROOT empty and lets the
# script reach a failure it can name.  (Written as a test rather than
# "cd ... || true", which is the A && B || C shape shellcheck flags as SC2015 --
# and the pre-flight gate runs shellcheck at default severity, so info-level
# findings fail it.)
BUILD_ROOT=""
if [ -d "$TOOLS_DIR" ]; then
  BUILD_ROOT="$(cd "$TOOLS_DIR/.." && pwd -P)"
fi
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

# The tracked sRGB v4 profile.  It carries none of the BRDF or MCS tags these
# codes select, which is exactly what makes it the right fixture here: the
# decode is asserted from the exported configuration, written before any
# transform is built, so the test needs no BRDF profile and takes no dependency
# on the generated-profile fixture.
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
DATA="$TESTING_DIR/ApplyDataFiles/rgb8bit.txt"

# errexit is lifted across the three lookups for the same reason it is lifted
# around every tool invocation below.  "find | head" is a pipeline, and under
# "set -o pipefail" find's non-zero exit for an unreadable or absent TOOLS_DIR
# becomes the substitution's exit, which under "set -e" aborts the script here
# with no output whatsoever.  That is what made require_tool's "missing
# executable" diagnostic unreachable: the failure CI actually saw was a bare
# exit 1 with an empty log.  Letting the variables come back empty routes the
# same condition through require_tool, which names it.
set +e
APPLYNCM="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyNamedCmm -type f 2>/dev/null | head -1)"
APPLYPRF="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyProfiles -type f 2>/dev/null | head -1)"
APPLYLNK="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
set -e

PASS=0

fail() {
  echo "  [FAIL] issue-2262-brdf-usage -- $1"
  exit 1
}

require_file() {
  if [ ! -f "$1" ]; then
    fail "missing fixture: $1"
  fi
}

require_tool() {
  local what="$1" path="$2"

  if [ -z "$path" ]; then
    fail "$what not found under $TOOLS_DIR"
  fi
  if [ ! -x "$path" ]; then
    fail "$what is not executable: $path"
  fi
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null; then
    sed -n '1,120p' "$1"
    return 1
  fi

  return 0
}

require_tool iccApplyNamedCmm "$APPLYNCM"
require_tool iccApplyProfiles "$APPLYPRF"
require_tool iccApplyToLink   "$APPLYLNK"
require_file "$SRGB"
require_file "$DATA"

echo "=== #2262 BRDF rendering-intent Usage() regression ==="

###############################################################################
# Part 1 -- the three Usage() screens
#
# Each tool prints its table with its own leading indent, so every row here is
# matched against a copy of the screen with leading whitespace stripped, as a
# whole-line FIXED string (grep -Fqx).  Fixed rather than a regex on purpose:
# the MCS row carries "(", ")" and "/", which are ERE metacharacters, so an -E
# pattern would quietly stop meaning what it reads as.  Whole-line rather than a
# substring for the same reason it matters at all -- an unanchored "50 + Intent"
# is satisfied by a row that appended anything to the transform name, and the
# point of this test is that each row says exactly one thing.
###############################################################################

# The transposed spelling, split across two adjacent literals so that the
# repository-wide grep this issue was reopened on finds nothing anywhere in the
# tree, this test included.  Bash concatenates adjacent quoted strings within a
# word, so the variable holds the four letters while no line contains them.
TRANSPOSED="BD""RF"

assert_usage_row() {
  local name="$1" row="$2"

  grep -Fqx "$row" "$OUTDIR/$name.stripped" ||
    fail "$name does not print \"$row\""
}

check_usage() {
  local name="$1" tool="$2"
  local log="$OUTDIR/$name.log"
  local rc=0

  rm -f "$log"
  set +e
  timeout 60 "$tool" > "$log" 2>&1
  rc=$?
  set -e

  check_sanitizers "$log" || fail "$name usage emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out printing usage"
  fi
  if [ "$rc" -ne 0 ]; then
    fail "$name did not exit 0 when printing usage (exit=$rc)"
  fi

  sed 's/^[[:space:]]*//' "$log" > "$OUTDIR/$name.stripped"

  # Anti-vacuity: an empty or truncated capture satisfies the negative
  # assertion below and none of the positive ones, so pin a row this change
  # does not touch as evidence the screen was captured whole.  The gamut row is
  # the one all three tools spell identically -- their Preview rows do not
  # agree, which is the pre-existing divergence noted on the issue and left
  # alone here.
  grep -Fqx "30 - Gamut" "$OUTDIR/$name.stripped" ||
    fail "$name usage output does not carry the untouched gamut row"

  assert_usage_row "$name" "50 + Intent - BRDF Parameters"
  assert_usage_row "$name" "60 + Intent - BRDF Direct"
  assert_usage_row "$name" "70 + Intent - BRDF MCS Parameters"
  # 80 is the one row carrying a qualifier.  icXformLutMCS offsets by
  # nTagIntent only on its MVIS/Output branch (MToS0..3, MToB0..3); the
  # MultiplexIdentification/Input branch reads a single AToM0Tag and the
  # MultiplexLink branch a single MToA0Tag, so 80..83 are indistinguishable in
  # the to-MCS direction.  Leaving the row unqualified would have over-claimed
  # by exactly the criterion that keeps gamut in the flat form.
  assert_usage_row "$name" "80 + Intent - MCS connection (Intent applies to MToS/MToB only)"

  if grep -Fq "$TRANSPOSED" "$log"; then
    fail "$name usage output still spells the acronym with the letters transposed"
  fi

  # ...and the superseded flat form is gone.  Asserting only that the new rows
  # are present would still pass on a screen that printed both, which is the
  # shape a careless merge produces.  Scoped to these four codes: 30 and 33
  # print in the flat form legitimately, because the gamut transform reads
  # icSigGamutTag with no intent offset.
  if grep -Eq "^[[:space:]]*(50|60|70|80) - " "$log"; then
    fail "$name usage output still prints a BRDF/MCS row in the flat form"
  fi

  PASS=$((PASS + 1))
}

check_usage applynamedcmm-usage "$APPLYNCM"
check_usage applyprofiles-usage "$APPLYPRF"
check_usage applytolink-usage   "$APPLYLNK"

# The three tables must agree with each other.  Each tool prints these rows at
# its own indent, so compare the stripped rows: the whole reason the wording
# diverged -- the same three transforms named "Model"/"Light"/"Output" in one
# tool and "Parameters"/"Direct"/"MCS Parameters" in the other two -- is that
# nothing ever compared them.
#
# The four per-row assertions above already pin the exact wording, so what this
# adds is narrower than it looks: it catches a row order or a duplicate that
# every per-row match would still be satisfied by.  Verified live rather than
# assumed -- swapping the 60 and 70 rows in one tool passes all four row
# assertions and fails here.
extract_rows() {
  grep -E '^(50|60|70|80) \+ Intent - ' "$OUTDIR/$1.stripped"
}

extract_rows applynamedcmm-usage > "$OUTDIR/rows-namedcmm.txt"
extract_rows applyprofiles-usage > "$OUTDIR/rows-profiles.txt"
extract_rows applytolink-usage   > "$OUTDIR/rows-tolink.txt"

if [ "$(wc -l < "$OUTDIR/rows-namedcmm.txt")" -ne 4 ]; then
  fail "expected four BRDF/MCS rows from iccApplyNamedCmm, found $(wc -l < "$OUTDIR/rows-namedcmm.txt")"
fi

cmp -s "$OUTDIR/rows-namedcmm.txt" "$OUTDIR/rows-profiles.txt" ||
  fail "iccApplyNamedCmm and iccApplyProfiles disagree on the BRDF/MCS rows"
cmp -s "$OUTDIR/rows-namedcmm.txt" "$OUTDIR/rows-tolink.txt" ||
  fail "iccApplyNamedCmm and iccApplyToLink disagree on the BRDF/MCS rows"

###############################################################################
# Part 2 -- the "+ Intent" claim is true, not merely printed
#
# Asserted through -exportcfg rather than exit status.  Every invocation here
# exits non-zero, because the tracked sRGB profile carries none of these tags
# and CIccCmm::AddXform() fails once the transform is actually built -- so exit
# status cannot tell "the parser refused the code" from "the parser accepted it
# and the profile had no such tag".  The exported configuration can: it is
# written after the decode and before the transform, so its presence and its
# contents are the decode, and a code the parser refuses produces no file at
# all.  (Reading a downstream failure as a rejection is the vacuity that made
# an earlier run_expect_reject arm on #2190 pass against the defect.)
###############################################################################

decode_cfg() {
  local code="$1" cfg="$OUTDIR/decode-$1.cfg.json" log="$OUTDIR/decode-$1.log"

  local rc=0

  rm -f "$cfg" "$log"
  set +e
  timeout 60 "$APPLYNCM" -exportcfg "$cfg" "$DATA" 0 1 "$SRGB" "$code" > "$log" 2>&1
  rc=$?
  set -e

  check_sanitizers "$log" || fail "decode-$code emitted sanitizer diagnostics"
  # Exit status is not the assertion here -- see the block comment above -- but
  # a timeout and a crash are still worth naming, or they would surface below as
  # the much less informative "produced no exported configuration".
  if [ "$rc" -eq 124 ]; then
    fail "intent code $code timed out"
  fi
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "intent code $code crashed with signal $((rc - 128))"
  fi

  # A refused code never reaches the export, so this is the discriminator
  # between "accepted and decoded" and "rejected".  It is proved non-vacuous by
  # the rejection control below, which asserts this same line does appear.
  if grep -Fqx "Unable to parse profile sequence arguments" "$log"; then
    fail "intent code $code was refused by the profile-sequence parser"
  fi
  if [ ! -f "$cfg" ]; then
    fail "intent code $code produced no exported configuration"
  fi
}

assert_decoded() {
  local code="$1" transform="$2" intent="$3"
  local cfg="$OUTDIR/decode-$code.cfg.json"

  grep -Fq "\"transform\": \"$transform\"" "$cfg" ||
    fail "intent code $code did not decode to transform \"$transform\""
  grep -Fq "\"intent\": \"$intent\"" "$cfg" ||
    fail "intent code $code did not decode to intent \"$intent\""
}

# Each family is walked across all four intent digits.  Walking all four is
# what makes this an assertion about the units digit rather than about one
# code: a decode that ignored the digit would give the same intent four times
# and fail on three of them, and one that had been changed to refuse the
# non-zero digits would fail in decode_cfg.
INTENTS=(perceptual relative saturation absolute)

for base_and_name in "50:brdfParam" "60:brdfDirect" "70:brdfMcsParam" "80:MCS"; do
  base="${base_and_name%%:*}"
  transform="${base_and_name##*:}"

  for digit in 0 1 2 3; do
    code=$((base + digit))
    decode_cfg "$code"
    assert_decoded "$code" "$transform" "${INTENTS[$digit]}"
    PASS=$((PASS + 1))
  done
done

# Rejection control.  3000000 is the unrecognised-overprint code from #2190 and
# is genuinely refused, so it proves the "Unable to parse profile sequence
# arguments" check above can fire -- without it, a build that had stopped
# emitting that diagnostic would let every decode arm pass on a tool that
# accepted nothing.
REJECT_CFG="$OUTDIR/decode-reject.cfg.json"
REJECT_LOG="$OUTDIR/decode-reject.log"
rm -f "$REJECT_CFG" "$REJECT_LOG"
set +e
timeout 60 "$APPLYNCM" -exportcfg "$REJECT_CFG" "$DATA" 0 1 "$SRGB" 3000000 > "$REJECT_LOG" 2>&1
set -e
check_sanitizers "$REJECT_LOG" || fail "decode-reject emitted sanitizer diagnostics"
grep -Fqx "Unable to parse profile sequence arguments" "$REJECT_LOG" ||
  fail "the rejection control was not refused by the profile-sequence parser"
if [ -f "$REJECT_CFG" ]; then
  fail "the rejection control exported a configuration for a refused code"
fi
PASS=$((PASS + 1))

echo "  [PASS] issue-2262-brdf-usage -- $PASS assertions"
exit 0
