#!/bin/bash
###############################################################################
# iccJpegDump issue-2162 unchecked read error regression
###############################################################################
# InjectIccIntoJpeg() copies the JPEG tail with
#
#   while ((n = fread(buf, 1, sizeof(buf), in)) > 0) { ... }
#
# and fread reports n == 0 for BOTH end-of-file and a read error, so the return
# value alone cannot tell them apart.  Before the fix ferror(in) was never
# consulted -- zero occurrences in the file, against six checked fwrite results
# -- so an I/O error part-way through the image data ended the copy silently:
# the output JPEG was truncated with no EOI marker, yet the function returned
# true and the tool printed "successfully injected" and exited 0.
#
# There is no portable way to make a real read fail on demand, so this injects
# the failure with an LD_PRELOAD shim that returns 0 from fread after N calls --
# which is exactly what the loop sees when a read fails.  The test asserts the
# tool now exits non-zero and says so, instead of reporting success.
#
# The shim is validated before it is trusted, and the validation has to come
# from inside the shim: a pass-through shim behaves identically to no shim, and
# an LD_PRELOAD the loader cannot open is only a warning on stderr -- the
# program still runs normally.  So the shim touches a sentinel file on its first
# call and the control run requires it.  Without that, a build where preloading
# is impossible (a noexec $OUTDIR, static linking, non-glibc) would pass the
# control, produce a full-size "injected" output, and red a correct binary.
# Missing sentinel means SKIP, not FAIL.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2162-jpegdump-read-error}"
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

JPEGDUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccJpegDump -type f 2>/dev/null | head -1)"
INPUT_ICC="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
INPUT_JPEG="$REPO_ROOT/Tools/Winnt/IccIisIsapi/assets/phil.jpg"
SHIM_SRC="$OUTDIR/failread.c"
SHIM_SO="$OUTDIR/failread.so"
LOGFILE="$OUTDIR/issue-2162-jpegdump-read-error.log"

fail() {
  echo "  [FAIL] issue-2162-jpegdump-read-error -- $1"
  exit 1
}

skip() {
  echo "  [SKIP] issue-2162-jpegdump-read-error -- $1"
  exit 0
}

echo "=== iccJpegDump issue-2162 unchecked read error regression ==="

# --- preconditions -----------------------------------------------------------

if [ "$(uname -s)" != "Linux" ]; then
  skip "LD_PRELOAD fault injection is Linux-only (found $(uname -s))"
fi
if [ -z "$JPEGDUMP" ] || [ ! -x "$JPEGDUMP" ]; then
  skip "iccJpegDump not built under $TOOLS_DIR"
fi
if [ ! -f "$INPUT_ICC" ]; then
  skip "missing $INPUT_ICC"
fi
if [ ! -f "$INPUT_JPEG" ]; then
  skip "missing $INPUT_JPEG"
fi

CC_BIN="${CC:-cc}"
if ! command -v "$CC_BIN" >/dev/null 2>&1; then
  skip "no C compiler ($CC_BIN) to build the fault-injection shim"
fi

# --- the shim ----------------------------------------------------------------
# Passes the first FAIL_AFTER fread calls through, then returns 0.  The tool's
# first fread reads the 2-byte SOI marker, so FAIL_AFTER=2 lets the SOI and one
# 4096-byte tail chunk through before the simulated failure.

cat > "$SHIM_SRC" <<'SHIM'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int calls = 0;
static int closed = 0;

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  static size_t (*real_fread)(void *, size_t, size_t, FILE *);
  const char *env = getenv("ICCDEV_FAIL_AFTER");
  const char *sentinel = getenv("ICCDEV_SHIM_SENTINEL");
  int limit = env ? atoi(env) : 0;

  if (!real_fread)
    real_fread = dlsym(RTLD_NEXT, "fread");

  /* Prove the shim is actually loaded.  An LD_PRELOAD the loader cannot open is
     only a warning on stderr -- the program then runs normally -- so the caller
     has no way to distinguish "interposed and passed through" from "never
     loaded" without a positive signal from inside the shim. */
  if (calls == 0 && sentinel)
    fclose(fopen(sentinel, "w"));

  calls++;

  /* Close the descriptor out from under the stream so the read that follows
     fails for real (EBADF) and glibc sets the stream's error indicator.  Simply
     returning 0 here would reproduce the truncation but NOT the error state,
     and ferror() -- the thing under test -- would stay false, so the test would
     fail against the fixed binary for the wrong reason.

     Guarded: glibc may satisfy one more call from its buffer before reaching
     the syscall, so this runs twice otherwise -- and the second close() would
     act on a descriptor number that is free again by then. */
  if (limit > 0 && calls > limit && !closed) {
    closed = 1;
    close(fileno(stream));
  }

  return real_fread(ptr, size, nmemb, stream);
}
SHIM

if ! "$CC_BIN" -shared -fPIC -o "$SHIM_SO" "$SHIM_SRC" -ldl > "$LOGFILE" 2>&1; then
  sed -n '1,20p' "$LOGFILE"
  skip "could not build the fault-injection shim"
fi

# --- control: shim loaded, injection disabled --------------------------------
# Validates the instrument. If the tool cannot run normally with the shim
# loaded, interposition is not usable here and any red below would be noise.

BASE_JPEG="$OUTDIR/base.jpg"
SENTINEL="$OUTDIR/shim-loaded"
rm -f "$BASE_JPEG" "$SENTINEL"
ICCDEV_FAIL_AFTER=0 ICCDEV_SHIM_SENTINEL="$SENTINEL" LD_PRELOAD="$SHIM_SO" \
  "$JPEGDUMP" "$INPUT_JPEG" --write-icc "$INPUT_ICC" --output "$BASE_JPEG" \
  > "$LOGFILE" 2>&1
control_rc=$?

# The sentinel is the only proof that interposition happened.  A pass-through
# shim is byte-identical to no shim at all, and a preload the loader refuses is
# merely a warning on stderr -- so without this the script would sail past the
# control and then red on a correct binary at the "no effect" check below.
if [ ! -f "$SENTINEL" ]; then
  sed -n '1,20p' "$LOGFILE"
  skip "LD_PRELOAD did not interpose (no sentinel from the shim); nothing to test here"
fi

if [ "$control_rc" -ne 0 ] || [ ! -s "$BASE_JPEG" ]; then
  sed -n '1,20p' "$LOGFILE"
  skip "control run under LD_PRELOAD did not succeed (rc=$control_rc)"
fi

if [ "$(tail -c 2 "$BASE_JPEG" | od -An -tx1 | tr -d ' \n')" != "ffd9" ]; then
  fail "control output is not a complete JPEG (missing EOI marker)"
fi

base_size="$(wc -c < "$BASE_JPEG")"

# --- the regression ----------------------------------------------------------

TRUNC_JPEG="$OUTDIR/trunc.jpg"
rm -f "$TRUNC_JPEG"
ICCDEV_FAIL_AFTER=2 LD_PRELOAD="$SHIM_SO" \
  "$JPEGDUMP" "$INPUT_JPEG" --write-icc "$INPUT_ICC" --output "$TRUNC_JPEG" \
  > "$LOGFILE" 2>&1
inject_rc=$?

trunc_size=0
if [ -f "$TRUNC_JPEG" ]; then
  trunc_size="$(wc -c < "$TRUNC_JPEG")"
fi

echo "  control: rc=$control_rc size=$base_size"
echo "  injected: rc=$inject_rc size=$trunc_size"

# The read genuinely was cut short -- if it were not, this test would pass for
# the wrong reason and would keep passing if the injection ever stopped working.
if [ "$trunc_size" -ge "$base_size" ]; then
  fail "fault injection had no effect (injected size $trunc_size >= control size $base_size)"
fi

if [ "$inject_rc" -eq 0 ]; then
  sed -n '1,20p' "$LOGFILE"
  fail "read error was not detected: exited 0 with a $trunc_size-byte output where the control wrote $base_size (issue #2162)"
fi

if grep -qi "successfully injected" "$LOGFILE"; then
  sed -n '1,20p' "$LOGFILE"
  fail "reported success despite a failed read (issue #2162)"
fi

if ! grep -qi "Failed to read JPEG input" "$LOGFILE"; then
  sed -n '1,20p' "$LOGFILE"
  fail "no read-failure diagnostic was emitted"
fi

if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$LOGFILE" 2>/dev/null; then
  sed -n '1,80p' "$LOGFILE"
  fail "sanitizer diagnostic during the injected run"
fi

echo "  [PASS] issue-2162-jpegdump-read-error"
exit 0
