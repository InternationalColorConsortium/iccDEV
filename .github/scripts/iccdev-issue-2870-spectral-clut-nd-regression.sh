#!/usr/bin/env bash
#
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     1. Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#
#     2. Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#
#     3. Neither the name of International Color Consortium (ICC) nor the names of
#        its contributors may be used to endorse or promote products derived from
#        this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Regression for #2870: a spectral CLUT element (EmissionCLutElement or
# ReflectanceCLutElement) with seven or more inputs wrote past a zero-byte
# heap block on its first Apply().
#
# CIccMpeSpectralCLUT::Apply() interpolates the colorimetric CLUT Begin()
# builds, but GetNewApply() sized the N-dimensional interpolation scratch from
# the spectral CLUT as read, which is never begun and has an empty node table.
# Up to six inputs the dedicated interpolators need no scratch, so only the
# InterpND() path -- seven inputs and up -- wrote through it: AddressSanitizer
# reports a heap-buffer-overflow, a plain build aborts in malloc or crashes.
#
# The fixture is a v5 seven-colour display profile whose AToB0 is a 2^7-node
# emission CLUT. The PCC is built from the tracked Testing/PCC XML.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2870}"
mkdir -p "$OUTDIR"

TAG="issue-2870-spectral-clut-nd"

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

APPLY="$(find_tool iccApplyNamedCmm)"
FROMXML="$(find_tool iccFromXml)"
for b in "$APPLY" "$FROMXML"; do
  if [ -z "$b" ] || [ ! -x "$b" ]; then
    echo "[SKIP] iccApplyNamedCmm and iccFromXml are required under $TOOLS_DIR"
    exit 0
  fi
done

FIXTURE="$REPO_ROOT/.github/ci/test-data/spectral-clut-7d-emission-2870.xml"
PCC_XML="$REPO_ROOT/Testing/PCC/Spec400_10_700-N_2deg-Abs.xml"
for f in "$FIXTURE" "$PCC_XML"; do
  if [ ! -f "$f" ]; then
    echo "[SKIP] required tracked input missing: $f"
    exit 0
  fi
done

build() {
  if ! ( cd "$(dirname "$1")" && "$FROMXML" "$(basename "$1")" "$2" ) \
       > "$2.fromxml.log" 2>&1 || [ ! -f "$2" ]; then
    echo "[FAIL] $TAG -- could not build $2 from $1"
    sed 's/^/    /' "$2.fromxml.log"
    exit 1
  fi
}

PROFILE="$OUTDIR/spectral-clut-7d.icc"
PCC="$OUTDIR/pcc.icc"
build "$FIXTURE" "$PROFILE"
build "$PCC_XML" "$PCC"

DATA="$OUTDIR/seven-colour.txt"
printf "'7CLR' ; Data Format\nicEncodeFloat ; Encoding\n\n0 0 0 0 0 0 0\n1 1 1 1 1 1 1\n" > "$DATA"

LOG="$OUTDIR/apply.log"
timeout 120s "$APPLY" "$DATA" 3 0 "$PROFILE" 0 -PCC "$PCC" > "$LOG" 2>&1
RC=$?

FAILURES=0
if [ "$RC" -ne 0 ]; then
  echo "  [FAIL] $TAG -- iccApplyNamedCmm exited $RC"
  FAILURES=$((FAILURES + 1))
fi
if grep -qE 'ERROR: (Address|Memory)Sanitizer|runtime error:' "$LOG"; then
  echo "  [FAIL] $TAG -- sanitizer report"
  FAILURES=$((FAILURES + 1))
fi
if [ "$FAILURES" -ne 0 ]; then
  sed 's/^/    /' "$LOG" | head -40
  echo "[FAIL] $TAG: $FAILURES failure(s)"
  exit 1
fi
echo "[PASS] $TAG"
exit 0
