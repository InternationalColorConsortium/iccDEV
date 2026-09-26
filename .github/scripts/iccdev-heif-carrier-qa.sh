#!/bin/bash
###############################################################################
# iccDEV HEIF ICC carrier regression tests (#2647 / #2654)
###############################################################################
#
# Copyright (c) International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. In the absence of prior written permission, the names "ICC" and "The
#    International Color Consortium" must not be used to imply that the ICC
#    organization endorses or promotes products derived from this software.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# This software consists of voluntary contributions made by many individuals on
# behalf of the International Color Consortium. Membership in the ICC is
# encouraged when this software is used for commercial purposes. For more
# information, see <http://www.color.org/>.
#
# Environment variables:
#   ICCDEV_HEIF_TOOL    -- path to the iccHeifDump executable
#   ICCDEV_HEIF_CORPUS  -- path to the tracked HEIF carrier corpus
#   ICCDEV_TEST_OUTDIR  -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="${ICCDEV_HEIF_TOOL:-}"
CORPUS="${ICCDEV_HEIF_CORPUS:-$REPO_ROOT/.github/ci/tooling/heif/img}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-heif-carrier-qa}"
EXTRACT_DIR="$OUTDIR/extracted"

PASS=0
FAIL=0
TOTAL=0

mkdir -p "$OUTDIR" "$EXTRACT_DIR"

pass_case() {
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

fail_case() {
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

check_runtime_findings() {
  local name="$1"
  local logfile="$2"

  if grep -Eq 'ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:' "$logfile" 2>/dev/null; then
    fail_case "$name" "sanitizer finding"
    sed -n '1,80p' "$logfile"
    return 1
  fi
  return 0
}

require_text() {
  local name="$1"
  local logfile="$2"
  local expected="$3"

  if ! grep -Fq "$expected" "$logfile" 2>/dev/null; then
    fail_case "$name" "missing expected text: $expected"
    sed -n '1,80p' "$logfile"
    return 1
  fi
  return 0
}

run_case() {
  local name="$1"
  local expected_exit="$2"
  local expected_text="$3"
  shift 3

  local logfile="$OUTDIR/$name.log"
  local exit_code=0
  TOTAL=$((TOTAL + 1))
  rm -f "$logfile"

  "$TOOL" "$@" >"$logfile" 2>&1 || exit_code=$?

  if ! check_runtime_findings "$name" "$logfile"; then
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "terminated by signal (exit $exit_code)"
    sed -n '1,80p' "$logfile"
    return
  fi
  if [ "$exit_code" -ne "$expected_exit" ]; then
    fail_case "$name" "expected exit $expected_exit, got $exit_code"
    sed -n '1,80p' "$logfile"
    return
  fi
  require_text "$name" "$logfile" "$expected_text" || return
  pass_case "$name" "exit $exit_code and expected output"
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

if [ -z "$TOOL" ] || [ ! -x "$TOOL" ]; then
  echo "[FAIL] missing iccHeifDump executable: ${TOOL:-ICCDEV_HEIF_TOOL is unset}"
  exit 1
fi

for fixture in \
  conflicting-nclx.heic conflicting-prof.heic duplicate-prof.heic \
  empty-prof.heic invalid-ipma-index.heic malformed-prof.heic nclx-only.heif \
  overflow-meta-largesize.heic oversized-colr.heic short-prof.hif \
  truncated-colr.heic truncated-nclx.heic truncated-sps.heic valid-prof.avif \
  valid-prof.heic valid-rICC.heic; do
  if [ ! -f "$CORPUS/$fixture" ]; then
    echo "[FAIL] missing HEIF fixture: $CORPUS/$fixture"
    exit 1
  fi
done

VALID_SHA=de9b36ccc513fdc79860d5dff523aed9cf67411789e4c20ae866146cbaf75a4e
MALFORMED_SHA=015748e9d8583e2a9ed81b131f39e25c5cbfb79cecaf474e63c27be547f51506
EMPTY_SHA=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

echo "HEIF ICC carrier corpus"
run_case valid-prof-heic 0 "profile_bytes=540 sha256=$VALID_SHA" "$CORPUS/valid-prof.heic"
run_case valid-ricc-heic 0 "colour_type=rICC profile_bytes=540 sha256=$VALID_SHA" "$CORPUS/valid-rICC.heic"
run_case valid-prof-avif 0 "profile_bytes=540 sha256=$VALID_SHA" "$CORPUS/valid-prof.avif"
run_case nclx-only 0 "icc_profiles=0" "$CORPUS/nclx-only.heif"
run_case duplicate-prof 0 "icc_profiles=2" "$CORPUS/duplicate-prof.heic"
run_case conflicting-prof 0 "profile_bytes=540 sha256=$MALFORMED_SHA" "$CORPUS/conflicting-prof.heic"
run_case conflicting-nclx 0 "colour_type=nclx profile_bytes=0" "$CORPUS/conflicting-nclx.heic"
run_case malformed-prof 0 "profile_bytes=540 sha256=$MALFORMED_SHA" "$CORPUS/malformed-prof.heic"
run_case empty-prof 0 "profile_bytes=0 sha256=$EMPTY_SHA" "$CORPUS/empty-prof.heic"
run_case short-prof 0 "profile_bytes=7 sha256=fdb8f562726e1e10f0a275347266df425d50b8d668af4b65a83c9021d8222279" "$CORPUS/short-prof.hif"
run_case truncated-sps 0 "icc_profiles=1" "$CORPUS/truncated-sps.heic"
run_case invalid-ipma-index 2 "unable to initialize HEIF reader: 8" "$CORPUS/invalid-ipma-index.heic"
run_case overflow-meta-largesize 2 "unable to initialize HEIF reader: 8" "$CORPUS/overflow-meta-largesize.heic"
run_case oversized-colr 2 "unable to initialize HEIF reader: 8" "$CORPUS/oversized-colr.heic"
run_case truncated-colr 2 "unable to initialize HEIF reader: 8" "$CORPUS/truncated-colr.heic"
run_case truncated-nclx 2 "unable to initialize HEIF reader: 8" "$CORPUS/truncated-nclx.heic"

echo "HEIF property and CLI controls"
run_case property-valid 0 "property=1 result=0" --colr-property 1 "$CORPUS/valid-prof.heic"
run_case property-missing 2 "property=99 result=17" --colr-property 99 "$CORPUS/valid-prof.heic"
run_case property-non-colr 2 "property=2 result=17" --colr-property 2 "$CORPUS/valid-prof.heic"
run_case property-invalid-text 3 "invalid property index: nope" --colr-property nope "$CORPUS/valid-prof.heic"
rm -f "$OUTDIR/missing.heic"
run_case missing-input 2 "unable to initialize HEIF reader: 7" "$OUTDIR/missing.heic"
run_case bare-invocation 3 "Usage:"

echo "HEIF ICC extraction"
rm -f "$EXTRACT_DIR/item-1-property-1-prof.icc"
run_case extract-valid-prof 0 "output=$EXTRACT_DIR/item-1-property-1-prof.icc" \
  "$CORPUS/valid-prof.heic" "$EXTRACT_DIR"
TOTAL=$((TOTAL + 1))
EXTRACTED="$EXTRACT_DIR/item-1-property-1-prof.icc"
if [ ! -f "$EXTRACTED" ]; then
  fail_case extracted-profile "output file was not created"
else
  extracted_size="$(wc -c <"$EXTRACTED" | tr -d ' ')"
  extracted_sha="$(sha256_file "$EXTRACTED")"
  if [ "$extracted_size" != "540" ]; then
    fail_case extracted-profile "expected 540 bytes, got $extracted_size"
  elif [ "$extracted_sha" != "$VALID_SHA" ]; then
    fail_case extracted-profile "unexpected SHA-256 $extracted_sha"
  else
    pass_case extracted-profile "540 bytes and SHA-256 $VALID_SHA"
  fi
fi

echo
echo "HEIF QA summary: total=$TOTAL pass=$PASS fail=$FAIL"
if [ "$FAIL" -ne 0 ] || [ "$PASS" -ne "$TOTAL" ]; then
  exit 1
fi
exit 0
