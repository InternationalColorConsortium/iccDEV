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

SRC_TIF="$TESTING_DIR/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
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

  set +e
  ASAN_OPTIONS="symbolize=0:halt_on_error=1:abort_on_error=1:detect_leaks=0" \
  UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
    timeout 30 "$APPLY" "$SRC_TIF" "$out_tif" 4 1 1 1 1 "$profile" 40 > "$log" 2>&1
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

  echo "[PASS] $name completed without sanitizer findings"
}

run_profile reflectance-clut 3ae8248544e08837bef71f66df02139cbbc8007fe1969ce7fcfe3b7aa473dd5a
run_profile reflectance-observer 483c6b366df494b6caf762b11d77cb08228be91cc03df59955266bfcb5930f02

echo "[PASS] iccApplyProfiles spectral observer HBO regression completed"
