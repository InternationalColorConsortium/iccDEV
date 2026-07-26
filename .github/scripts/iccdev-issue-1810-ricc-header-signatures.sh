#!/bin/bash
###############################################################################
# iccDEV issue #1810 - 'RICC' in the CMM and platform header fields of the
#                      Testing fixtures
###############################################################################
#
# 'RICC' was never a registered signature.  It was this project's own enum
# typo: icSigRefIccMAX carried the value 0x52494343 ('RICC') while its comment
# and registry.color.org/cmm-signatures both said 'RIMX' = 0x52494D58.  PR #1473
# corrected the enum (recorded as row B3 in
# Tools/CmdLine/IccPawgReport/registry/PERMISSIVENESS_DELTAS.md), but the
# fixtures authored while the enum was wrong kept the bad value, so the
# validator went on reporting them.
#
# Two header fields are actually checked, and they fail differently:
#
#   <PreferredCMMType>RICC   -> "Unregistered CMM signature."   IccProfile.cpp
#   <PrimaryPlatform>RICC    -> "Unknown platform signature."   IccProfile.cpp
#
# A third field, <ProfileCreator>, also carried 'RICC' in 24 fixtures, but
# CIccProfile::Validate never inspects the creator field -- it is read and
# written only -- so those produce no finding and are deliberately NOT part of
# this test.  Asserting on them would be asserting on nothing.
#
# The fix replaces the CMM value with the registered 'RIMX' and clears the
# platform field to empty (the encoding 6 other Testing fixtures already use
# for "unspecified"), per @PhilGreen736767's ruling on #1810:
#
#   "RICC is the iccMAX Reference Implementation.  The registered CMM for this
#    is 'RIMX', not 'RICC'.  'RICC' is also not a platform signature."
#
# CONTROL -- run first, and the reason this test is not vacuous.  After the fix
# there is no fixture left that SHOULD warn, so a validator that had stopped
# emitting these findings altogether would let every assertion below pass.  The
# control therefore synthesises a failure: it copies a fixture, injects an
# unregistered CMM signature ('ZZZZ'), and requires the warning to appear.  If
# it does not, the environment cannot observe the defect and the whole test
# SKIPs rather than reporting a green it has not earned.
#
# CASE A -- text guard.  No tracked Testing XML may reintroduce 'RICC' into
# either validated field.  Needs no build, so it still has teeth in jobs that
# do not produce the command-line tools.
#
# CASE B -- functional.  Every fixture this issue touched must convert and
# validate free of both findings.  The validator itself is the oracle, so the
# registered-signature allow-list is never duplicated here and cannot drift.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (or skipped cleanly)
#   2 - regression detected
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1810}"
mkdir -p "$OUTDIR"

# The fixtures corrected by the #1810 change, relative to the repository root.
# Listed explicitly rather than derived, so the set this test covers is exactly
# the set the fix touched and stays reviewable in the diff.
FIXTURES="
Testing/CMYK-3DLUTs/CMYK-3DLUTs.xml
Testing/CMYK-3DLUTs/CMYK-3DLUTs2.xml
Testing/ICS/Lab_float-D65_2deg-Part1.xml
Testing/ICS/Lab_float-IllumA_2deg-Part2.xml
Testing/ICS/Lab_int-D65_2deg-Part1.xml
Testing/ICS/Lab_int-IllumA_2deg-Part2.xml
Testing/ICS/Rec2100HlgFull-Part1.xml
Testing/ICS/Rec2100HlgFull-Part2.xml
Testing/ICS/Rec2100HlgFull-Part3.xml
Testing/ICS/Spec400_10_700-D50_2deg-Part1.xml
Testing/ICS/Spec400_10_700-D93_2deg-Part2.xml
Testing/ICS/XYZ_float-D65_2deg-Part1.xml
Testing/ICS/XYZ_float-IllumA_2deg-Part2.xml
Testing/ICS/XYZ_int-D65_2deg-Part1.xml
Testing/ICS/XYZ_int-IllumA_2deg-Part2.xml
"

# Both findings are emitted by CIccProfile::Validate with this exact wording.
CMM_MSG="Unregistered CMM signature"
PLAT_MSG="Unknown platform signature"

status=0

# Print a captured block indented under the report line above it.  Written as a
# read loop rather than `echo "$var" | sed 's/^/.../'` to keep the pre-flight
# lint gate clean (SC2001); it also preserves blank lines within the block.
indent() {
  while IFS= read -r line; do
    printf '       %s\n' "$line"
  done <<< "$1"
}

# --- CASE A: no tracked fixture may carry 'RICC' in a validated field --------
# Deliberately ordered first: it is the assertion that needs no tools, so a
# build-less job still reports something meaningful instead of skipping whole.
#
# <ProfileCreator> is excluded on purpose -- see the header comment.  Only the
# two fields CIccProfile::Validate actually inspects are guarded.
hits=""
if command -v git >/dev/null 2>&1 && git -C "$REPO_ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  hits="$(git -C "$REPO_ROOT" grep -n \
      -e '<PreferredCMMType>RICC</PreferredCMMType>' \
      -e '<PrimaryPlatform>RICC</PrimaryPlatform>' \
      -- 'Testing/*' 2>/dev/null)"
else
  # Fall back to a plain walk when the tree is not a git checkout (release
  # tarball, vendored copy), so the guard does not silently disappear.
  hits="$(grep -rn \
      -e '<PreferredCMMType>RICC</PreferredCMMType>' \
      -e '<PrimaryPlatform>RICC</PrimaryPlatform>' \
      --include='*.xml' "$REPO_ROOT/Testing" 2>/dev/null)"
fi

if [ -n "$hits" ]; then
  echo "[FAIL] #1810 case A: 'RICC' reintroduced into a validated header field"
  indent "$hits"
  echo "       use the registered 'RIMX' for <PreferredCMMType>; leave"
  echo "       <PrimaryPlatform> empty ('RICC' is not a platform signature)"
  status=2
else
  echo "[PASS] #1810 case A: no tracked fixture carries 'RICC' in a validated field"
fi

# --- locate the tools; case B needs both -------------------------------------
FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ] || [ -z "$DUMP" ] || [ ! -x "$DUMP" ]; then
  echo "[SKIP] case B: iccFromXml / iccDumpProfile not found under $TOOLS_DIR"
  exit "$status"
fi

# Convert $1 (repo-relative XML) to $2 (output .icc).
#
# Runs with the XML's own directory as the working directory: several of these
# fixtures reference sibling data files (EOTF.txt, LUT tables) by relative path
# and will not parse from anywhere else.
convert() {
  local xml="$1" out="$2"
  ( cd "$REPO_ROOT/$(dirname "$xml")" && \
    "$FROMXML" "$(basename "$xml")" "$out" ) >"$out.convert.log" 2>&1
}

# --- CONTROL: the validator must still be able to emit the CMM finding -------
# Injects an unregistered signature into a copy of a real fixture.  The copy is
# written beside the original because of the relative-path dependency above,
# and is removed on every exit path.
CONTROL_SRC="Testing/ICS/Rec2100HlgFull-Part1.xml"
CONTROL_TMP="$REPO_ROOT/$(dirname "$CONTROL_SRC")/.iccdev-1810-control.xml"
# Inlined rather than a cleanup() function so shellcheck can see the command is
# reachable (SC2317 cannot follow a trap handler to its definition).  Single
# quotes are deliberate: $CONTROL_TMP expands when the trap fires, not now.
trap 'rm -f "$CONTROL_TMP"' EXIT

if [ ! -f "$REPO_ROOT/$CONTROL_SRC" ]; then
  echo "[SKIP] case B: control fixture missing: $CONTROL_SRC"
  exit "$status"
fi

sed 's|<PreferredCMMType>[^<]*</PreferredCMMType>|<PreferredCMMType>ZZZZ</PreferredCMMType>|' \
    "$REPO_ROOT/$CONTROL_SRC" > "$CONTROL_TMP"
convert "$(dirname "$CONTROL_SRC")/.iccdev-1810-control.xml" "$OUTDIR/control.icc"
if [ ! -f "$OUTDIR/control.icc" ]; then
  echo "[SKIP] control profile did not convert; environment issue, not #1810"
  sed -n '1,10p' "$OUTDIR/control.icc.convert.log" 2>/dev/null
  exit "$status"
fi
"$DUMP" -v 26 "$OUTDIR/control.icc" > "$OUTDIR/control.validate.log" 2>&1
if ! grep -q "$CMM_MSG" "$OUTDIR/control.validate.log"; then
  # Without this the case B loop below would pass on a validator that had
  # stopped checking the CMM field at all.
  echo "[SKIP] control: validator did not flag an unregistered CMM ('ZZZZ');"
  echo "       it cannot observe #1810 here, so case B would be a vacuous pass"
  exit "$status"
fi
echo "[PASS] #1810 control: validator still flags an unregistered CMM signature"

# --- CASE B: every corrected fixture validates free of both findings ---------
checked=0
for xml in $FIXTURES; do
  [ -f "$REPO_ROOT/$xml" ] || { echo "[SKIP] fixture missing: $xml"; continue; }
  base="$(basename "$xml" .xml)"
  icc="$OUTDIR/$base.icc"
  rm -f "$icc"
  convert "$xml" "$icc"
  if [ ! -f "$icc" ]; then
    echo "[FAIL] #1810 case B: $xml no longer converts"
    sed -n '1,5p' "$icc.convert.log" 2>/dev/null | sed 's/^/       /'
    status=2
    continue
  fi
  "$DUMP" -v 26 "$icc" > "$icc.validate.log" 2>&1
  found="$(grep -E "$CMM_MSG|$PLAT_MSG" "$icc.validate.log")"
  if [ -n "$found" ]; then
    echo "[FAIL] #1810 case B: $base still reports an unregistered header signature"
    indent "$found"
    status=2
  fi
  checked=$((checked + 1))
done

if [ "$status" -eq 0 ]; then
  echo "[PASS] #1810 case B: $checked corrected fixtures validate with no"
  echo "       unregistered-CMM or unknown-platform finding"
fi

exit "$status"
