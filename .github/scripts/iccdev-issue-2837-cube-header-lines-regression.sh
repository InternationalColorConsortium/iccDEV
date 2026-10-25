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
# Regression for #2837 and #2847: iccApplyToLink wrote the cube TITLE and each
# profile description comment with one fprintf, so a line break in either
# started a new line of the file.
#   #2847: Testing/Calc/RGBWProjector.xml's description runs over four lines;
#          iccFromCube refused the cube it produced ("Unknown keyword").
#   #2837: a title holding a line break and "LUT_3D_SIZE 999" put that keyword
#          on a line of its own.
# Each line of the text is now written as its own TITLE or comment line, which
# iccFromCube joins back together.
#
# Builds its own profile from the tracked XML, so no Testing/ profile
# generation is required.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2837}"
mkdir -p "$OUTDIR"

TAG="issue-2837-cube-header-lines"
FAILURES=0

fail() {
  echo "  [FAIL] $TAG -- $1"
  FAILURES=$((FAILURES + 1))
}

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

TOLINK="$(find_tool iccApplyToLink)"
FROMCUBE="$(find_tool iccFromCube)"
FROMXML="$(find_tool iccFromXml)"
for b in "$TOLINK" "$FROMCUBE" "$FROMXML"; do
  if [ -z "$b" ] || [ ! -x "$b" ]; then
    echo "[SKIP] iccApplyToLink, iccFromCube and iccFromXml are required under $TOOLS_DIR"
    exit 0
  fi
done

XML="$REPO_ROOT/Testing/Calc/RGBWProjector.xml"
if [ ! -f "$XML" ]; then
  echo "[SKIP] required tracked input missing: $XML"
  exit 0
fi

PROFILE="$OUTDIR/rgbw.icc"
if ! ( cd "$(dirname "$XML")" && "$FROMXML" "$(basename "$XML")" "$PROFILE" ) \
     > "$OUTDIR/fromxml.log" 2>&1 || [ ! -f "$PROFILE" ]; then
  echo "[SKIP] could not build the profile from $XML"
  sed 's/^/    /' "$OUTDIR/fromxml.log"
  exit 0
fi

sanitizer_clean() {
  ! grep -qE 'ERROR: (Address|Leak|Memory|Thread)Sanitizer|runtime error:' "$1"
}

# make_cube <name> <title>: writes $OUTDIR/<name>.cube from the RGBW profile.
make_cube() {
  local cube="$OUTDIR/$1.cube"
  rm -f "$cube"
  timeout 120s "$TOLINK" "$cube" 1 2 2 "$2" 0 1 0 0 "$PROFILE" 1 \
    > "$OUTDIR/$1.tolink.log" 2>&1
  local rc=$?
  if [ "$rc" -ne 0 ] || [ ! -f "$cube" ]; then
    fail "$1: iccApplyToLink exited $rc"
    sed 's/^/    /' "$OUTDIR/$1.tolink.log"
    return 1
  fi
  sanitizer_clean "$OUTDIR/$1.tolink.log" || fail "$1: sanitizer report from iccApplyToLink"
  return 0
}

# read_cube <name>: iccFromCube must accept the cube.
read_cube() {
  timeout 120s "$FROMCUBE" "$OUTDIR/$1.cube" "$OUTDIR/$1.icc" > "$OUTDIR/$1.fromcube.log" 2>&1
  local rc=$?
  if [ "$rc" -ne 0 ]; then
    fail "$1: iccFromCube refused the cube iccApplyToLink wrote (exit $rc)"
    sed 's/^/    /' "$OUTDIR/$1.fromcube.log"
  fi
  if grep -q "Unknown keyword" "$OUTDIR/$1.fromcube.log"; then
    fail "$1: iccFromCube reported an unknown keyword"
  fi
  sanitizer_clean "$OUTDIR/$1.fromcube.log" || fail "$1: sanitizer report from iccFromCube"
}

# #2847: a multi-line profile description in the header comments.
if make_cube description "title"; then
  CUBE="$OUTDIR/description.cube"
  if ! grep -qx '# - RGBW Projector' "$CUBE"; then
    fail "description: first description line not written as '# - RGBW Projector'"
  fi
  if ! grep -q '^#   This profile implements the RGBW projector model' "$CUBE"; then
    fail "description: second description line not written as a comment"
  fi
  if grep -q '^This profile implements' "$CUBE"; then
    fail "description: a description line was written without a comment marker"
  fi
  read_cube description
fi

# #2837: a title holding a line break and a keyword.
TITLE=$'foo\nLUT_3D_SIZE 999'
if make_cube title "$TITLE"; then
  CUBE="$OUTDIR/title.cube"
  if grep -q '^LUT_3D_SIZE 999' "$CUBE"; then
    fail "title: the text after the line break was written as a keyword line"
  fi
  if ! grep -qx 'TITLE "foo"' "$CUBE" || ! grep -qx 'TITLE "LUT_3D_SIZE 999"' "$CUBE"; then
    fail "title: each title line was not written as its own TITLE line"
  fi
  if [ "$(grep -c '^LUT_3D_SIZE ' "$CUBE")" -ne 1 ]; then
    fail "title: expected exactly one LUT_3D_SIZE keyword line"
  fi
  read_cube title
fi

if [ "$FAILURES" -ne 0 ]; then
  echo "[FAIL] $TAG: $FAILURES failure(s)"
  exit 1
fi
echo "[PASS] $TAG"
exit 0
