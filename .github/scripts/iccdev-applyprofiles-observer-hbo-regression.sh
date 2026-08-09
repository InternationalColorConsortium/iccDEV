#!/bin/bash
###############################################################################
# iccDEV iccApplyProfiles spectral observer heap-buffer-overflow regression
###############################################################################
#
# Replays AFL applyprofiles-fast profiles that drove ReflectanceCLUT and
# ReflectanceObserver Begin() through getEmissiveObserver() with illuminant
# ranges wider than the caller-allocated observer matrix.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-$REPO_ROOT/Build/Testing/ctest-output/applyprofiles-observer-hbo}"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

APPLY="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
if [ ! -x "$APPLY" ]; then
  echo "[FAIL] missing executable: $APPLY"
  exit 1
fi

BUILD_DIR="$(cd "$TOOLS_DIR/.." && pwd)"
export LD_LIBRARY_PATH="$BUILD_DIR/IccProfLib:$BUILD_DIR/IccXML:$BUILD_DIR/IccJSON:$BUILD_DIR/IccConnect:${LD_LIBRARY_PATH:-}"

mkdir -p "$OUTDIR"
ASAN_PROBE="$OUTDIR/applyprofiles-observer-hbo-asan-probe.txt"
asan_instrumented=0
if command -v ldd >/dev/null 2>&1 &&
   ldd "$APPLY" > "$ASAN_PROBE" 2>/dev/null &&
   grep -q 'libasan' "$ASAN_PROBE"; then
  asan_instrumented=1
elif command -v nm >/dev/null 2>&1 &&
     nm "$APPLY" > "$ASAN_PROBE" 2>/dev/null &&
     grep -q '__asan' "$ASAN_PROBE"; then
  asan_instrumented=1
elif command -v strings >/dev/null 2>&1 &&
     strings "$APPLY" > "$ASAN_PROBE" 2>/dev/null &&
     grep -q '__asan' "$ASAN_PROBE"; then
  asan_instrumented=1
fi

if [ "$asan_instrumented" -ne 1 ]; then
  echo "[SKIP] iccApplyProfiles is not ASAN-instrumented"
  exit 77
fi

for tool in base64 sha256sum timeout; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "[FAIL] $tool is required"
    exit 1
  fi
done

# Both replay profiles declare a 6CLR device space, so the source image has to
# carry six samples per pixel or iccApplyProfiles refuses the pair up front with
# "Number of samples 3 ... doesn't match device samples 6 in first profile" and
# never calls Begin() -- the very path these profiles exist to drive.  The
# seed-tiff-none-rgb-8x8.tif this used to name is 8x8 RGB, three samples, so the
# replay had been bailing before it reached getEmissiveObserver().  This is the
# only six-sample TIFF in the tracked corpus and is what the sibling spectral-PCS
# replay harness already uses for the same profiles (#2052).
SRC_TIF="$TESTING_DIR/mcs/CMYKSS-Numbered-Overprint.tif"
if [ ! -f "$SRC_TIF" ]; then
  echo "[FAIL] missing TIFF fixture: $SRC_TIF"
  exit 1
fi

TMPDIR="$(mktemp -d "$OUTDIR/applyprofiles-observer.XXXXXX")"
trap 'rm -rf "$TMPDIR"' EXIT

decode_profile() {
  local name="$1"
  local expected="$2"
  local path="$TMPDIR/$name.icc"

  case "$name" in
    reflectance-clut)
      base64 -d > "$path" <<'EOF_REFLECTANCE_CLUT'
AAAErAAAAAAFAAAAc3BhYzZDTFJYWVogB+oABgAVABQAHwAdYWNzcAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAPbWAAEAAAAA0y1JQ0MgxGcoQMfZ47bdNyIS2VO2HgAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAADZGVzYwAAAKgAAABSc3ZjbgAAAPwAAAA8QTJCMAAAATgAAAN0bWx1
YwAAAAAAAAABAAAADGVuVVMAAAA2AAAAHABJAG4AdABlAHIAcAA2AGQAIABzAHAAZQBjAHQAcgBh
AGwAIABVAEIAUwBBAE4AIABQAG8AQwAAc3ZjbgAAAAAAAAABAAAAAAAAAAAAAAABRZxAAAAAAAAA
AAAAP3bWAD+AAAA/Uy0AP3bWAD+AAAA/Uy0AbXBldAAAAAAABgADAAAAAgAAACAAAAM0AAADVAAA
ACByY2x0AAAAAAAGAAMAAAAAXkBekAADAAACAgICAgIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAA/gAAAAAAAAD+AAAAAAAAAAAAAAD+AAAA/gAAAP4AAAAAAAAAAAAAAP4AAAAAAAAA/gAAA
P4AAAD+AAAAAAAAAP4AAAD+AAAA/gAAAPczMzQAAAAAAAAAAPczMzQAAAAA/gAAAPczMzT+AAAAA
AAAAPczMzT+AAAA/gAAAP4AAAD3MzM0AAAAAP4AAAD3MzM0/gAAAP4AAAD+AAAA9zMzNP4AAAD+A
AAA/gAAAPkzMzQAAAAAAAAAAPkzMzQAAAAA/gAAAPkzMzT+AAAAAAAAAPkzMzT+AAAA/gAAAP4AA
AD5MzM0AAAAAP4AAAD5MzM0/gAAAP4AAAD+AAAA+TMzNP4AAAD+AAAA/gAAAPpmZmgAAAAAAAAAA
PpmZmgAAAAA/gAAAPpmZmj+AAAAAAAAAPpmZmj+AAAA/gAAAP4AAAD6ZmZoAAAAAP4AAAD6ZmZo/
gAAAP4AAAD+AAAA+mZmaP4AAAD+AAAA/gAAAPszMzQAAAAAAAAAAPszMzQAAAAA/gAAAPszMzT+A
AAAAAAAAPszMzT+AAAA/gAAAP4AAAD7MzM0AAAAAP4AAAD7MzM0/gAAAP4AAAD+AAAA+zMzNP4AA
AD+AAAA/gAAAPwAAAAAAAAAAAAAAPwAAAAAAAAA/gAAAPwAAAD+AAAAAAAAAPwAAAD+AAAA/gAAA
P4AAAD8AAAAAAAAAP4AAAD8AAAA/gAAAP4AAAD+AAAA/AAAAP4AAAD+AAAA/gAAAPxmZmgAAAAAA
AAAAPxmZmgAAAAA/gAAAPxmZmj+AAAAAAAAAPxmZmj+AAAA/gAAAP4AAAD8ZmZoAAAAAP4AAAD8Z
mZo/gAAAP4AAAD+AAAA/GZmaP4AAAD+AAAA/gAAAPzMzMwAAAAAAAAAAPzMzMwAAAAA/gAAAPzMz
Mz+AAAAAAAAAPzMzMz+AAAA/gAAAP4AAAD8zMzMAAAAAP4AAAD8zMzM/gAAAP4AAAD+AAAA/MzMz
P4AAAD+AAAA/gAAAP4AAAD+AAAA/gAAAZW9icwAAAAAAAwADXkBekAADAAA/gAAAP4AAAD+AAAA=
EOF_REFLECTANCE_CLUT
      ;;
    reflectance-observer)
      base64 -d > "$path" <<'EOF_REFLECTANCE_OBSERVER'
AAAErAAAAAAFAAAAc3BhYzZDTFJYWVogB+oABgAVABQAHwAdYWNzcAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAPbWAAEAAAAA0y1JQ0MgxGcoQMfZ47bdNyIS2VO2HgAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAADZGVzYwAAAKgAAABSc3ZjbgAAAPwAAAA8QTJCMAAAATgAAAN0bWx1
YwAAAAAAAAABAAAADGVuVVMAAAA2AAAAHABJAG4AdABlAHIAcAA2AGQAIABzAHAAZQBjAHQAcgBh
AGwAIABVAEIAUwBBAE4AIABQAG8AQwAAc3ZjbgAAAAAAAAABAAAAAAAAAAAAAAABRZxAAAAAAAAA
AAAAP3bWAD+AAAA/Uy0AP3bWAD+AAAA/Uy0AbXBldAAAAAAABgADAAAAAgAAACAAAAM0AAADVAAA
ACBlY2x0AAAAAAAGAAMAAAAAXkBekAADAAACAgICAgIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAA/gAAAAAAAAD+AAAAAAAAAAAAAAD+AAAA/gAAAP4AAAAAAAAAAAAAAP4AAAAAAAAA/gAAA
P4AAAD+AAAAAAAAAP4AAAD+AAAA/gAAAPczMzQAAAAAAAAAAPczMzQAAAAA/gAAAPczMzT+AAAAA
AAAAPczMzT+AAAA/gAAAP4AAAD3MzM0AAAAAP4AAAD3MzM0/gAAAP4AAAD+AAAA9zMzNP4AAAD+A
AAA/gAAAPkzMzQAAAAAAAAAAPkzMzQAAAAA/gAAAPkzMzT+AAAAAAAAAPkzMzT+AAAA/gAAAP4AA
AD5MzM0AAAAAP4AAAD5MzM0/gAAAP4AAAD+AAAA+TMzNP4AAAD+AAAA/gAAAPpmZmgAAAAAAAAAA
PpmZmgAAAAA/gAAAPpmZmj+AAAAAAAAAPpmZmj+AAAA/gAAAP4AAAD6ZmZoAAAAAP4AAAD6ZmZo/
gAAAP4AAAD+AAAA+mZmaP4AAAD+AAAA/gAAAPszMzQAAAAAAAAAAPszMzQAAAAA/gAAAPszMzT+A
AAAAAAAAPszMzT+AAAA/gAAAP4AAAD7MzM0AAAAAP4AAAD7MzM0/gAAAP4AAAD+AAAA+zMzNP4AA
AD+AAAA/gAAAPwAAAAAAAAAAAAAAPwAAAAAAAAA/gAAAPwAAAD+AAAAAAAAAPwAAAD+AAAA/gAAA
P4AAAD8AAAAAAAAAP4AAAD8AAAA/gAAAP4AAAD+AAAA/AAAAP4AAAD+AAAA/gAAAPxmZmgAAAAAA
AAAAPxmZmgAAAAA/gAAAPxmZmj+AAAAAAAAAPxmZmj+AAAA/gAAAP4AAAD8ZmZoAAAAAP4AAAD8Z
mZo/gAAAP4AAAD+AAAA/GZmaP4AAAD+AAAA/gAAAPzMzMwAAAAAAAAAAPzMzMwAAAAA/gAAAPzMz
Mz+AAAAAAAAAPzMzMz+AAAA/gAAAP4AAAD8zMzMAAAAAP4AAAD8zMzM/gAAAP4AAAD+AAAA/MzMz
P4AAAD+AAAA/gAAAP4AAAD+AAAA/gAAAcm9icwAAAAAAAwADXkBekAADAAA/gAAAP4AAAD+AAAA=
EOF_REFLECTANCE_OBSERVER
      ;;
    *)
      echo "[FAIL] unknown profile fixture: $name"
      exit 1
      ;;
  esac

  actual="$(sha256sum "$path" | awk '{print $1}')"
  if [ "$actual" != "$expected" ]; then
    echo "[FAIL] $name digest mismatch: $actual"
    exit 1
  fi

  printf '%s\n' "$path"
}

run_profile() {
  local name="$1"
  local expected="$2"
  local profile
  local out_tif="$TMPDIR/$name-out.tif"
  local log="$OUTDIR/$name.log"
  local exit_code

  profile="$(decode_profile "$name" "$expected")"
  rm -f "$out_tif" "$log"

  # dst_sample_encoding 3 is icEncodeFloat.  This used to pass 4, which the
  # iccApplyProfiles usage text advertised as float from 1f0a9dd2 until #1996
  # corrected it; the selector switch answered every out-of-range value with
  # 8 bit and exit 0, so following the shipped documentation looked like it
  # worked.  #1996 made the switch reject what it does not define, turning the
  # stale 4 into "Unable to parse configuration arguments" and Usage() (#2052).
  # The replays do real work now that they are fed a source they can pair with:
  # about 4 s each in a local Debug sanitizer build, against the 30 s this used to
  # allow while it was exiting immediately.  45 s keeps a full order of magnitude
  # of headroom for a slower CI container and still leaves both replays inside the
  # 120 s the CTest budgets for the script.
  set +e
  ASAN_OPTIONS="symbolize=0:halt_on_error=1:abort_on_error=1:detect_leaks=0" \
  UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
    timeout 45 "$APPLY" "$SRC_TIF" "$out_tif" 3 1 1 1 1 "$profile" 40 > "$log" 2>&1
  exit_code=$?
  set -e

  if grep -qE 'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|MemorySanitizer' "$log"; then
    echo "[FAIL] sanitizer finding while replaying $name applyprofiles observer PoC"
    sed -n '1,120p' "$log"
    exit 1
  fi

  if [ "$exit_code" -eq 124 ]; then
    echo "[FAIL] iccApplyProfiles timed out while replaying $name"
    sed -n '1,80p' "$log"
    exit 1
  fi

  if [ "$exit_code" -ge 128 ] && [ "$exit_code" -le 192 ]; then
    echo "[FAIL] iccApplyProfiles crashed with signal $((exit_code - 128)) while replaying $name"
    sed -n '1,80p' "$log"
    exit 1
  fi

  # Everything above only rejects a crash, so any orderly refusal -- a bad
  # selector, a device-space mismatch, an unreadable profile -- used to reach the
  # [PASS] below having replayed nothing at all.  A regression guard that cannot
  # distinguish "ran clean" from "declined to run" is what let both defects
  # #2052 records sit here undetected, so assert the replay actually happened:
  # a zero status, the writer's completion marker, and a non-empty output image.
  if [ "$exit_code" -ne 0 ]; then
    echo "[FAIL] iccApplyProfiles refused to replay $name (exit $exit_code)"
    sed -n '1,80p' "$log"
    exit 1
  fi

  if ! grep -q '100%' "$log"; then
    echo "[FAIL] $name did not run to completion: no progress marker in $log"
    sed -n '1,80p' "$log"
    exit 1
  fi

  if [ ! -s "$out_tif" ]; then
    echo "[FAIL] $name produced no output image at $out_tif"
    sed -n '1,80p' "$log"
    exit 1
  fi

  echo "[PASS] $name replayed to completion without sanitizer findings"
}

run_profile reflectance-clut 3ae8248544e08837bef71f66df02139cbbc8007fe1969ce7fcfe3b7aa473dd5a
run_profile reflectance-observer 483c6b366df494b6caf762b11d77cb08228be91cc03df59955266bfcb5930f02

echo "[PASS] iccApplyProfiles spectral observer HBO regression completed"
