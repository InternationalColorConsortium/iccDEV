#!/bin/bash
###############################################################################
# iccFromCube issue-1843 CLUT axis-order and long-line regression
###############################################################################
#
# Two defects in iccFromCube, both reachable from a legal .cube file:
#
#   1. CLUT axis order.  A .cube table runs the red index fastest and blue
#      slowest; an ICC CLUT is laid out the other way round, with input channel
#      0 carrying the largest stride.  iccFromCube streamed .cube rows into the
#      CLUT in file order, which transposed the first and last input axes -- an
#      identity .cube produced a profile that swapped red and blue, mapping
#      source (1,0,0) to (0,0,1).  This test converts identity cubes and asserts
#      that applying them is a no-op, plus an asymmetric red-only cube that
#      pins which axis is which (an identity alone cannot distinguish a correct
#      grid from one whose axes are transposed twice).
#
#   2. Long lines.  getNextLine() stopped after MAX_LINE_LEN characters without
#      consuming the rest of the physical line, so the tail was returned as the
#      next "line" and parsed as a keyword in its own right.  A 300-character
#      comment -- legal, the format sets no line length limit -- was enough to
#      make a valid .cube fail with "Unknown keyword".
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1843-fromcube-clut-order}"
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

FROMCUBE="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromCube -type f 2>/dev/null | head -1)"
APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyNamedCmm -type f 2>/dev/null | head -1)"

fail() {
  echo "  [FAIL] issue-1843-fromcube-clut-order -- $1"
  exit 1
}

echo "=== iccFromCube issue-1843 CLUT axis-order and long-line regression ==="

if [ -z "$FROMCUBE" ] || [ ! -x "$FROMCUBE" ]; then
  echo "  [SKIP] iccFromCube not built under $TOOLS_DIR"
  exit 0
fi
if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyNamedCmm not built under $TOOLS_DIR"
  exit 0
fi

# ---------------------------------------------------------------------------
# Compare iccApplyNamedCmm output against an expectation.
#
# Result rows are "<out r> <out g> <out b>\t; <src r> <src g> <src b>".  MODE
# "identity" asserts the output equals the source; MODE "red-only" asserts the
# output is (src r, 0, 0).  Both use a tolerance because the values come back
# through a trilinear CLUT interpolation.
# ---------------------------------------------------------------------------
check_apply() {
  local label="$1" profile="$2" data="$3" mode="$4"
  local log="$OUTDIR/apply-$label.log"

  if ! "$APPLY" "$data" 2 0 "$profile" 1 > "$log" 2>&1; then
    sed -n '1,20p' "$log"
    fail "iccApplyNamedCmm failed on $label"
  fi

  awk -v mode="$mode" -v label="$label" '
    /;/ && !/^;/ {
      split($0, halves, ";")
      n = split(halves[1], out, "[ \t]+")
      m = split(halves[2], src, "[ \t]+")
      # split() on leading whitespace yields an empty first field.
      oi = (out[1] == "" ? 1 : 0); si = (src[1] == "" ? 1 : 0)
      if (n - oi < 3 || m - si < 3) next
      for (i = 1; i <= 3; i++) { o[i] = out[i+oi] + 0; s[i] = src[i+si] + 0 }
      if (mode == "red-only") { s[2] = 0; s[3] = 0 }
      rows++
      for (i = 1; i <= 3; i++) {
        d = o[i] - s[i]; if (d < 0) d = -d
        if (d > 0.001) {
          printf("    row %d: got (%.4f %.4f %.4f) want (%.4f %.4f %.4f)\n",
                 rows, o[1], o[2], o[3], s[1], s[2], s[3])
          bad++
          next
        }
      }
    }
    END {
      if (rows == 0) { print "    no result rows parsed"; exit 2 }
      if (bad > 0) { printf("    %d of %d rows wrong\n", bad, rows); exit 1 }
      printf("    %s: %d/%d rows correct\n", label, rows, rows)
    }
  ' "$log"

  local rc=$?
  [ "$rc" -eq 0 ] || fail "$label did not survive the round trip through the CLUT (#1843)"
}

# ---------------------------------------------------------------------------
# 1. Identity cube, 3x3x3, probed at every grid node.
#
# Rows are emitted in .cube order -- red fastest, blue slowest -- so this file
# is an identity only under that convention.  Probing all 27 nodes means any
# permutation of the input axes shows up, not just a red/blue swap.
# ---------------------------------------------------------------------------
IDENT_CUBE="$OUTDIR/identity3.cube"
PROBE="$OUTDIR/grid27.txt"
{
  echo 'TITLE "issue-1843 identity"'
  echo 'LUT_3D_SIZE 3'
  for b in 0.0 0.5 1.0; do
    for g in 0.0 0.5 1.0; do
      for r in 0.0 0.5 1.0; do echo "$r $g $b"; done
    done
  done
} > "$IDENT_CUBE"

{
  printf "'RGB '\t; Data Format\n"
  printf "icEncodeUnitFloat\t; Encoding\n\n"
  for r in 0.0 0.5 1.0; do
    for g in 0.0 0.5 1.0; do
      for b in 0.0 0.5 1.0; do echo "$r $g $b"; done
    done
  done
} > "$PROBE"

"$FROMCUBE" "$IDENT_CUBE" "$OUTDIR/identity3.icc" > "$OUTDIR/fromcube-identity.log" 2>&1 \
  || { sed -n '1,20p' "$OUTDIR/fromcube-identity.log"; fail "iccFromCube rejected an identity cube"; }
check_apply "identity-3x3x3" "$OUTDIR/identity3.icc" "$PROBE" "identity"

# The tracked CI fixture is an identity under the same convention, so it must
# behave identically -- this catches the fixture and the tool drifting apart.
if [ -f "$REPO_ROOT/.github/ci/test-data/test-identity.cube" ]; then
  "$FROMCUBE" "$REPO_ROOT/.github/ci/test-data/test-identity.cube" "$OUTDIR/identity-fixture.icc" \
    > "$OUTDIR/fromcube-fixture.log" 2>&1 \
    || { sed -n '1,20p' "$OUTDIR/fromcube-fixture.log"; fail "iccFromCube rejected test-identity.cube"; }
  check_apply "identity-fixture" "$OUTDIR/identity-fixture.icc" "$PROBE" "identity"
fi

# ---------------------------------------------------------------------------
# 2. Asymmetric cube: output depends on red alone, and only drives red.
#
# Under the old file-order copy this mapped input blue onto output red, so it
# fails loudly if the axes are transposed in either direction.
# ---------------------------------------------------------------------------
RED_CUBE="$OUTDIR/red-only.cube"
{
  echo 'LUT_3D_SIZE 2'
  for _b in 0 1; do
    for _g in 0 1; do
      for r in 0.0 1.0; do echo "$r 0.0 0.0"; done
    done
  done
} > "$RED_CUBE"

"$FROMCUBE" "$RED_CUBE" "$OUTDIR/red-only.icc" > "$OUTDIR/fromcube-red.log" 2>&1 \
  || { sed -n '1,20p' "$OUTDIR/fromcube-red.log"; fail "iccFromCube rejected the red-only cube"; }
check_apply "red-only" "$OUTDIR/red-only.icc" "$PROBE" "red-only"

# ---------------------------------------------------------------------------
# 3. Long lines must not be re-parsed as a following line.
# ---------------------------------------------------------------------------
LONG_CUBE="$OUTDIR/long-comment.cube"
LONG_COMMENT="$(awk 'BEGIN { s = ""; while (length(s) < 300) s = s "x"; print s }')"
{
  echo 'TITLE "issue-1843 long line"'
  echo "# $LONG_COMMENT"
  echo 'LUT_3D_SIZE 2'
  for _i in 1 2 3 4 5 6 7 8; do echo "0.0 0.0 0.0"; done
} > "$LONG_CUBE"

if ! "$FROMCUBE" "$LONG_CUBE" "$OUTDIR/long-comment.icc" > "$OUTDIR/fromcube-long.log" 2>&1; then
  sed -n '1,20p' "$OUTDIR/fromcube-long.log"
  fail "a 300-character comment line made a legal .cube fail to parse (#1843)"
fi
if grep -q "Unknown keyword" "$OUTDIR/fromcube-long.log"; then
  sed -n '1,20p' "$OUTDIR/fromcube-long.log"
  fail "the tail of a long comment line was parsed as a keyword (#1843)"
fi
[ -s "$OUTDIR/long-comment.icc" ] || fail "no profile written for the long-comment cube"
echo "    long-comment: 300-character comment line accepted"

echo "  [PASS] issue-1843-fromcube-clut-order -- CLUT axes preserved and long lines parse"
exit 0
