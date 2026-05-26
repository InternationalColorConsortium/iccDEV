#!/bin/bash
# Regression tests for iccTiffDump attacker-controlled console output.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tiffdump-output-hardening}"
TIFFDUMP="$TOOLS/IccTiffDump/iccTiffDump"

mkdir -p "$OUTDIR"

if [ ! -x "$TIFFDUMP" ]; then
  echo "[FAIL] missing executable: $TIFFDUMP"
  exit 1
fi

SAMPLE_TIFF="$ICCDEV_TESTING/hybrid/Data/TShirtDesignKW.tif"
if [ ! -f "$SAMPLE_TIFF" ]; then
  echo "[FAIL] missing sample TIFF: $SAMPLE_TIFF"
  exit 1
fi

pass=0
fail=0

run_ok() {
  local name="$1"
  shift

  if "$@"; then
    echo "  [PASS] $name"
    pass=$((pass + 1))
  else
    echo "  [FAIL] $name"
    fail=$((fail + 1))
  fi
}

test_profile_description_escaping() {
  local mutated="$OUTDIR/tiffdump-mutated-desc.tif"
  local log="$OUTDIR/tiffdump-mutated-desc.log"

  cp "$SAMPLE_TIFF" "$mutated"
  perl -0pi -e 's/Dot Gain 20%/bad\n\e[31mX!!/g' "$mutated"
  "$TIFFDUMP" "$mutated" > "$log" 2>&1

  grep -Fq 'Description:      bad\n\x1B[31mX!!' "$log" || return 1
  ! grep -q "$(printf '\033')" "$log"
}

test_input_path_escaping() {
  local odd="$OUTDIR/name_with_
newline.tif"
  local log="$OUTDIR/tiffdump-path-escape.log"

  cp "$SAMPLE_TIFF" "$odd"
  "$TIFFDUMP" "$odd" > "$log" 2>&1

  grep -Fq 'Filename:          '"$OUTDIR"'/name_with_\nnewline.tif' "$log" || return 1
  ! grep -q "$(printf '\033')" "$log"
}

test_export_path_escaping() {
  local dst="$OUTDIR/export_with_
newline.icc"
  local log="$OUTDIR/tiffdump-export-escape.log"

  "$TIFFDUMP" "$SAMPLE_TIFF" "$dst" > "$log" 2>&1

  [ -s "$dst" ] || return 1
  grep -Fq 'Profile extracted to: '"$OUTDIR"'/export_with_\nnewline.icc' "$log"
}

echo "=== iccTiffDump output hardening regression ==="
run_ok "tiffdump-profile-description-escape" test_profile_description_escaping
run_ok "tiffdump-input-path-escape" test_input_path_escaping
run_ok "tiffdump-export-path-escape" test_export_path_escaping

echo "iccTiffDump output hardening regression: $pass passed, $fail failed, $((pass + fail)) total"

if [ "$fail" -ne 0 ]; then
  exit 1
fi
