#!/bin/bash
###############################################################################
# iccApplySearch command-line argument regression tests
###############################################################################
#
# Sibling of iccdev-applynamedcmm-cli-args-regression.sh. Both tools publish a
# "-cfg config_file_path" usage, and #1674 established for iccApplyNamedCmm that
# an argument the tool silently drops is worse than one it rejects: the run still
# reports success, so the caller cannot tell an honoured argument list from an
# ignored one. iccApplySearch shares that usage but never got the guard, so
# "cfg-extra" below is the case that fails against an unfixed tool. The remaining
# rejects already hold on master and are pinned here so the argument surface has
# a home of its own rather than living only inside the tool-coverage baseline.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-applysearch-cli-args}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplySearch -type f 2>/dev/null | head -1)"
DATA="$TESTING_DIR/ApplyDataFiles/rgb8bit.txt"
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
CFG="$OUTDIR/applysearch-cfg.json"
THREADED_DATA="$OUTDIR/rgb8bit-threaded.txt"
THREAD_IMPL="$REPO_ROOT/IccProfLib/IccCmmThread.cpp"

fail() {
  echo "  [FAIL] iccApplySearch-cli-args -- $1"
  exit 1
}

check_sanitizers() {
  local logfile="$1"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    sed -n '1,120p' "$logfile"
    return 1
  fi

  return 0
}

require_file() {
  local path="$1"

  if [ ! -f "$path" ]; then
    fail "missing fixture: $path"
  fi
}

require_tool() {
  local path="$1"

  if [ ! -x "$path" ]; then
    fail "missing executable: $path"
  fi
}

run_expect_success() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  if [ "$rc" -ne 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name failed with exit=$rc"
  fi
}

run_expect_reject() {
  local name="$1"
  shift
  local logfile="$OUTDIR/$name.log"
  local rc=0

  rm -f "$logfile"
  set +e
  timeout 60 "$@" > "$logfile" 2>&1
  rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$name emitted sanitizer diagnostics"
  if [ "$rc" -eq 124 ]; then
    fail "$name timed out"
  fi
  # A signal is never an acceptable refusal. Checking it separately from the
  # non-zero test below keeps a crash from reading as a rejection, which is the
  # failure mode a "did it refuse?" assertion is otherwise blind to.
  if [ "$rc" -ge 128 ] && [ "$rc" -le 192 ]; then
    fail "$name crashed with signal $((rc - 128))"
  fi
  if [ "$rc" -eq 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$name accepted malformed arguments"
  fi
}

require_tool "$APPLY"
require_file "$DATA"
require_file "$SRGB"

echo "=== iccApplySearch CLI argument regression ==="

# Let the tool write its own configuration rather than hand-authoring JSON here:
# the schema then cannot drift out from under this test, and the export doubles as
# a success case for the argument list that "cfg-extra" only extends by one token.
run_expect_success export-cfg \
  "$APPLY" -exportcfganddata "$CFG" "$DATA" 3 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
require_file "$CFG"

run_expect_success valid-cfg "$APPLY" -cfg "$CFG"
run_expect_success valid-legacy "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1

# Derive the fixture size from the production dispatch threshold. Four workers
# require more than three chunks; coupling the copy count to the constant keeps
# this test from silently becoming scalar if the threshold changes.
# Require the bare default, explicit scalar, automatic, and four-worker CLI
# output to remain byte-identical.
require_file "$THREAD_IMPL"
pixels_per_thread="$(awk \
  '$1 == "static" && $2 == "const" && $3 == "icUInt32Number" && \
   $4 == "kIccSmallApplyPixelsPerThread" && $5 == "=" { \
     sub(/;$/, "", $6); print $6; exit \
   }' "$THREAD_IMPL")"
data_rows="$(sed -n '3,$p' "$DATA" | awk 'NF { count++ } END { print count + 0 }')"
if [[ ! "$pixels_per_thread" =~ ^[1-9][0-9]*$ ]] || (( data_rows < 1 )); then
  fail "unable to derive the four-worker fixture size"
fi
threaded_copies=$(( (3 * pixels_per_thread + 1 + data_rows - 1) / data_rows ))
sed -n '1,2p' "$DATA" > "$THREADED_DATA"
for ((i = 0; i < threaded_copies; ++i)); do
  sed -n '3,$p' "$DATA" >> "$THREADED_DATA"
done
run_expect_success valid-default \
  "$APPLY" "$THREADED_DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
run_expect_success valid-threads-1 \
  "$APPLY" -threads 1 "$THREADED_DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
run_expect_success valid-threads-auto \
  "$APPLY" -threads 0 "$THREADED_DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
run_expect_success valid-threads-4 \
  "$APPLY" -threads 4 "$THREADED_DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
for output in valid-threads-1 valid-threads-auto valid-threads-4; do
  cmp "$OUTDIR/valid-default.log" "$OUTDIR/$output.log" ||
    fail "$output output differs from the default scalar output"
done
run_expect_success valid-threads-cfg "$APPLY" -threads 4 -cfg "$CFG"

# The #1674 guard this script exists for: everything past argv[2] used to be read
# and dropped, leaving a zero exit behind.
run_expect_reject cfg-extra "$APPLY" -cfg "$CFG" ignored-extra
run_expect_reject threads-negative "$APPLY" -threads -1 -cfg "$CFG"
run_expect_reject threads-too-large "$APPLY" -threads 257 -cfg "$CFG"
run_expect_reject threads-not-a-number "$APPLY" -threads nope -cfg "$CFG"
run_expect_reject threads-empty "$APPLY" -threads "" -cfg "$CFG"
run_expect_reject threads-bare "$APPLY" -threads
run_expect_reject threads-missing-command "$APPLY" -threads 4
run_expect_reject threads-debugcalc "$APPLY" -threads 4 -debugcalc "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1

# Already rejected on master; pinned so the surface cannot regress quietly.
run_expect_reject init-missing-value "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT
run_expect_reject legacy-extra "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1 ignored-extra
run_expect_reject env-short-name "$APPLY" "$DATA" 0 0 -ENV:abc 1 "$SRGB" 1 "$SRGB" 1 -INIT 1


###############################################################################
# #2190: the intent-code columns, in both positions iccApplySearch offers
#
# iccApplySearch is the only tool that reaches CIccCfgSearchApply::fromArgs and
# the -INIT initializer, so it is the only place the two decodes can be pinned
# against each other. They must answer the same code the same way: while the
# profile decode refused an unrecognised overprint column and -INIT masked it
# off, "3000000" was rejected in one position and silently accepted in the
# other within a single command line.
#
# -INIT has no overprint and no HToS field, so any code reaching those columns
# is refused there rather than truncated to the part it can represent.
###############################################################################

assert_rejected_sequence() {
  grep -Fqx "Unable to parse profile sequence arguments" "$OUTDIR/$1.log" ||
    fail "$1 was refused, but not by the profile-sequence parser"
}

# Same value, both positions, same verdict -- the pairing is the assertion.
run_expect_reject intent-overprint-3-profile \
  "$APPLY" "$DATA" 0 0 "$SRGB" 3000000 "$SRGB" 1 -INIT 1
assert_rejected_sequence intent-overprint-3-profile
run_expect_reject intent-overprint-3-init \
  "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 3000000
assert_rejected_sequence intent-overprint-3-init
run_expect_reject intent-overprint-9-init \
  "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 9000000
assert_rejected_sequence intent-overprint-9-init
run_expect_reject intent-htos-init \
  "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 100000
assert_rejected_sequence intent-htos-init

# Negative codes, both positions. "-110" used to decode exactly like "110"
# because the type digit takes abs() and the intent digit does not.
#
# These two need the diagnostic asserted, not just a non-zero exit. Before the
# fix "-110" was accepted by the parser and the run died later in Begin() with
# "Invalid Look-Up Table type" -- also a non-zero exit, so run_expect_reject
# alone passes against the defect and pins nothing. The parse-time message is
# what separates "refused the argument" from "ran with it and failed downstream".
run_expect_reject intent-negative-profile \
  "$APPLY" "$DATA" 0 0 "$SRGB" -110 "$SRGB" 1 -INIT 1
assert_rejected_sequence intent-negative-profile
run_expect_reject intent-negative-init \
  "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT -110
assert_rejected_sequence intent-negative-init

# Controls. Without these the rejections above would also be satisfied by a
# decode that had stopped accepting the columns altogether: +1000000 is a valid
# overprint in the profile position, and +10000 is the widest column -INIT can
# still represent, so both must continue to run.
run_expect_success intent-overprint-1-profile \
  "$APPLY" "$DATA" 0 0 "$SRGB" 1000000 "$SRGB" 1 -INIT 1
run_expect_success intent-v5-init \
  "$APPLY" "$DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 10000

echo "  [PASS] iccApplySearch-cli-args"
exit 0
