#!/bin/bash
###############################################################################
# iccFromCube CLI argument and DOMAIN_MIN/MAX parsing regression.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-fromcube-cli-args}"
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
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

FROMCUBE="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromCube -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"
CUBE="$OUTDIR/domain.cube"
OUT_ICC="$OUTDIR/domain.icc"
EXTRA_ICC="$OUTDIR/extra.icc"
ONE_D_CUBE="$OUTDIR/one-d.cube"
TRUNCATED_CUBE="$OUTDIR/truncated.cube"
EXTRA_ROW_CUBE="$OUTDIR/extra-row.cube"
JUNK_SUFFIX_CUBE="$OUTDIR/junk-suffix.cube"
NAN_INF_CUBE="$OUTDIR/nan-inf.cube"
DOMAIN_LOG="$OUTDIR/domain.log"
EXTRA_LOG="$OUTDIR/extra-args.log"
ONED_LOG="$OUTDIR/one-d.log"
TRUNCATED_LOG="$OUTDIR/truncated.log"
EXTRA_ROW_LOG="$OUTDIR/extra-row.log"
JUNK_SUFFIX_LOG="$OUTDIR/junk-suffix.log"
NAN_INF_LOG="$OUTDIR/nan-inf.log"
DUMP_LOG="$OUTDIR/domain-dump.log"

fail() {
  echo "  [FAIL] fromcube-cli-args -- $1"
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

expect_fromcube_failure() {
  local cube_file="$1"
  local output_file="$2"
  local logfile="$3"
  local label="$4"

  rm -f "$output_file" "$logfile"
  set +e
  "$FROMCUBE" "$cube_file" "$output_file" > "$logfile" 2>&1
  local rc=$?
  set -e

  check_sanitizers "$logfile" || fail "$label emitted sanitizer diagnostics"
  if [ "$rc" -eq 0 ]; then
    sed -n '1,80p' "$logfile"
    fail "$label unexpectedly succeeded"
  fi
  if [ -e "$output_file" ]; then
    fail "$label created an output profile"
  fi
}

echo "=== iccFromCube CLI argument and DOMAIN regression ==="

if [ -z "$FROMCUBE" ] || [ ! -x "$FROMCUBE" ]; then
  echo "  [SKIP] iccFromCube not built under $TOOLS_DIR"
  exit 0
fi
if [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  echo "  [SKIP] iccDumpProfile not built under $TOOLS_DIR"
  exit 0
fi

cat > "$CUBE" <<'EOF'
TITLE "domain parse audit"
LUT_3D_SIZE 2
DOMAIN_MIN 0.0 1.0 2.0
DOMAIN_MAX 3.0 4.0 5.0
0.0 0.0 0.0
1.0 0.0 0.0
0.0 1.0 0.0
1.0 1.0 0.0
0.0 0.0 1.0
1.0 0.0 1.0
0.0 1.0 1.0
1.0 1.0 1.0
EOF

rm -f "$OUT_ICC" "$EXTRA_ICC" "$DOMAIN_LOG" "$EXTRA_LOG" "$DUMP_LOG"
"$FROMCUBE" "$CUBE" "$OUT_ICC" > "$DOMAIN_LOG" 2>&1 || {
  sed -n '1,80p' "$DOMAIN_LOG"
  fail "regular domain cube conversion failed"
}
check_sanitizers "$DOMAIN_LOG" || fail "domain conversion emitted sanitizer diagnostics"
[ -s "$OUT_ICC" ] || fail "domain conversion did not produce output"

"$DUMP" -v 100 "$OUT_ICC" A2B0 > "$DUMP_LOG" 2>&1 || true
check_sanitizers "$DUMP_LOG" || fail "domain dump emitted sanitizer diagnostics"
grep -Fq "Single Sampled Curve [0.00000000, 3.00000000]" "$DUMP_LOG" || fail "red DOMAIN range was not preserved"
grep -Fq "Single Sampled Curve [1.00000000, 4.00000000]" "$DUMP_LOG" || fail "green DOMAIN range was not preserved"
grep -Fq "Single Sampled Curve [2.00000000, 5.00000000]" "$DUMP_LOG" || fail "blue DOMAIN range was not preserved"

set +e
"$FROMCUBE" "$CUBE" "$EXTRA_ICC" ignored-extra-arg > "$EXTRA_LOG" 2>&1
extra_rc=$?
set -e
check_sanitizers "$EXTRA_LOG" || fail "extra-argument run emitted sanitizer diagnostics"
if [ "$extra_rc" -eq 0 ]; then
  fail "extra argument run unexpectedly succeeded"
fi
if [ -e "$EXTRA_ICC" ]; then
  fail "extra argument run created an output profile"
fi

cat > "$ONE_D_CUBE" <<'EOF'
TITLE "unsupported one dimensional LUT"
LUT_1D_SIZE 2
0.0 0.0 0.0
1.0 1.0 1.0
EOF
set +e
"$FROMCUBE" "$ONE_D_CUBE" "$OUTDIR/one-d.icc" > "$ONED_LOG" 2>&1
oned_rc=$?
set -e
check_sanitizers "$ONED_LOG" || fail "1D run emitted sanitizer diagnostics"
if [ "$oned_rc" -eq 0 ]; then
  fail "1D cube unexpectedly succeeded"
fi
grep -Fq "1DLUTs are not supported" "$ONED_LOG" || fail "1D cube did not report unsupported 1DLUT"

cat > "$TRUNCATED_CUBE" <<'EOF'
TITLE "truncated table"
LUT_3D_SIZE 2
0 0 0
1 0 0
0 1 0
1 1 0
EOF
expect_fromcube_failure "$TRUNCATED_CUBE" "$OUTDIR/truncated.icc" "$TRUNCATED_LOG" "truncated 3DLUT table"
grep -Fq "Incomplete 3DLUT table" "$TRUNCATED_LOG" || fail "truncated table did not report incomplete 3DLUT"

cat > "$EXTRA_ROW_CUBE" <<'EOF'
TITLE "extra row table"
LUT_3D_SIZE 2
0 0 0
1 0 0
0 1 0
1 1 0
0 0 1
1 0 1
0 1 1
1 1 1
0.5 0.5 0.5
EOF
expect_fromcube_failure "$EXTRA_ROW_CUBE" "$OUTDIR/extra-row.icc" "$EXTRA_ROW_LOG" "extra-row 3DLUT table"
grep -Fq "Too many 3DLUT entries" "$EXTRA_ROW_LOG" || fail "extra-row table did not report surplus 3DLUT entries"

cat > "$JUNK_SUFFIX_CUBE" <<'EOF'
TITLE "junk suffix numbers"
LUT_3D_SIZE 2
DOMAIN_MIN 0red 0green 0blue
DOMAIN_MAX 1red 1green 1blue
0red 0green 0blue
1red 0green 0blue
0red 1green 0blue
1red 1green 0blue
0red 0green 1blue
1red 0green 1blue
0red 1green 1blue
1red 1green 1blue
EOF
expect_fromcube_failure "$JUNK_SUFFIX_CUBE" "$OUTDIR/junk-suffix.icc" "$JUNK_SUFFIX_LOG" "junk-suffix numeric input"
grep -Fq "Invalid DOMAIN_MIN value" "$JUNK_SUFFIX_LOG" || fail "junk suffix numeric input did not report invalid DOMAIN_MIN"

cat > "$NAN_INF_CUBE" <<'EOF'
TITLE "nan inf numbers"
LUT_3D_SIZE 2
0 0 0
1 0 0
0 1 0
1 1 0
0 0 1
1 0 1
0 1 1
nan inf -inf
EOF
expect_fromcube_failure "$NAN_INF_CUBE" "$OUTDIR/nan-inf.icc" "$NAN_INF_LOG" "non-finite 3DLUT input"
grep -Fq "Invalid 3DLUT entry" "$NAN_INF_LOG" || fail "non-finite 3DLUT input did not report invalid entry"

echo "  [PASS] fromcube-cli-args -- exact arity, DOMAIN ranges, strict numeric parsing, and 1D rejection verified"
