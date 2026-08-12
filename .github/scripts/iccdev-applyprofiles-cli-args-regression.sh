#!/bin/bash
###############################################################################
# iccApplyProfiles command-line argument regression tests
###############################################################################
#
# Third member of the #1674 argument-contract family, after
# iccdev-applynamedcmm-cli-args-regression.sh (#1906) and
# iccdev-applysearch-cli-args-regression.sh (#2075). The shared finding is that an
# argument the tool silently drops is worse than one it rejects: the run still
# reports success, so the caller cannot tell an honoured argument list from an
# ignored one.
#
# iccApplyProfiles carried the defect in both of its usages, but not to the same
# extent, and the two reject cases below are deliberately split to record that:
#
#   "cfg-extra"    -- the "-cfg <path>" usage ignored *any* number of trailing
#                     tokens.
#   "legacy-extra" -- the positional usage ignored exactly an *odd* leftover
#                     token. CIccCfgProfileSequence::fromArgs() consumes the
#                     profile group in pairs and stops when fewer than two
#                     arguments remain, so two or more leftovers are already read
#                     as a further profile/intent pair and rejected. That is what
#                     "legacy-extra-pair" pins, and it is why the fix guards the
#                     odd remainder only.
#
# "threads-cfg-extra" and "threads-cfg-valid" exist because the -cfg guard tests
# argc *after* the optional "-threads N" pair has been consumed; a guard written
# against the process-entry argc would wrongly reject "-threads 1 -cfg cfg.json".
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-applyprofiles-cli-args}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

# Both substitutions below must not be allowed to abort the script: under
# "set -euo pipefail" a failing cd, or a find over a missing directory whose
# non-zero status pipefail propagates, kills the run at the assignment itself --
# before require_tool can report which executable was missing. CTest would then
# show a bare non-zero exit with no output, making "the tool was not built in this
# configuration" indistinguishable from "the argument contract regressed".
BUILD_ROOT=""
if [ -d "$TOOLS_DIR/.." ]; then
  BUILD_ROOT="$(cd "$TOOLS_DIR/.." && pwd -P)"
fi
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyProfiles -type f 2>/dev/null | head -1 || true)"
SRC="$TESTING_DIR/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
SRGB="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
CFG="$OUTDIR/applyprofiles-cfg.json"

fail() {
  echo "  [FAIL] iccApplyProfiles-cli-args -- $1"
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

# A refused command line must not leave a converted image behind: the whole point
# of the guard is that the caller can tell a honoured argument list from an
# ignored one, and an output file written anyway would re-create that ambiguity
# for anyone reading the directory rather than the exit status.
require_absent() {
  local path="$1"
  local name="$2"

  if [ -e "$path" ]; then
    fail "$name wrote '$path' despite refusing the command line"
  fi
}

require_tool "$APPLY"
require_file "$SRC"
require_file "$SRGB"

echo "=== iccApplyProfiles CLI argument regression ==="

# Let the tool write its own configuration rather than hand-authoring JSON here:
# the schema then cannot drift out from under this test, and the export doubles as
# a success case for the legacy argument list the reject cases only extend.
run_expect_success export-cfg \
  "$APPLY" -exportcfg "$CFG" "$SRC" "$OUTDIR/export.tif" 0 0 0 0 1 "$SRGB" 1
require_file "$CFG"

run_expect_success valid-cfg "$APPLY" -cfg "$CFG"
run_expect_success valid-legacy \
  "$APPLY" "$SRC" "$OUTDIR/legacy.tif" 0 0 0 0 1 "$SRGB" 1
run_expect_success threads-cfg-valid "$APPLY" -threads 1 -cfg "$CFG"

# The two cases that fail against an unfixed tool: each ran the transform, wrote
# its output and exited 0 with the trailing token discarded.
rm -f "$OUTDIR/cfg-extra.tif" "$OUTDIR/legacy-extra.tif"
run_expect_reject cfg-extra "$APPLY" -cfg "$CFG" ignored-extra
run_expect_reject legacy-extra \
  "$APPLY" "$SRC" "$OUTDIR/legacy-extra.tif" 0 0 0 0 1 "$SRGB" 1 ignored-extra
require_absent "$OUTDIR/legacy-extra.tif" legacy-extra

run_expect_reject cfg-extra-many "$APPLY" -cfg "$CFG" a b c
run_expect_reject threads-cfg-extra "$APPLY" -threads 1 -cfg "$CFG" ignored-extra

# Already rejected before the fix, pinned so the surrounding surface cannot
# regress quietly while the odd-remainder guard above is in place.
run_expect_reject legacy-extra-pair \
  "$APPLY" "$SRC" "$OUTDIR/legacy-pair.tif" 0 0 0 0 1 "$SRGB" 1 extra1 extra2
run_expect_reject legacy-bad-encoding \
  "$APPLY" "$SRC" "$OUTDIR/legacy-enc.tif" 9 0 0 0 1 "$SRGB" 1

echo "  [PASS] iccApplyProfiles-cli-args"
exit 0
