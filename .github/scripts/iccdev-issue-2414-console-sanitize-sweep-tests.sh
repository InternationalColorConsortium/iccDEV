#!/bin/bash
###############################################################################
# #2414 -- the ANSI-injection half: CLI tools that echo a user-supplied path
#          to the console without sanitizing it
###############################################################################
#
# #2406 established the mechanism and #2419 fixed the four iccApply* tools;
# #2437 changed the helper to escape by CODEPOINT rather than by byte.  The
# helper, icSanitizeConsoleText(), lives in IccProfLib/IccFileUtil.h.
#
# #2414's QA task is the rest of the tooling.  Measured on 764fc176, EIGHT more
# tools printed an operand verbatim, so a profile named with a CSI sequence
# repainted the terminal of anyone who ran them:
#
#   iccDumpProfile iccPngDump iccFromCube iccRoundTrip
#   iccPawgReport  iccToXml   iccBenchApply iccFromXml
#
#   iccProfilePlot  iccV5DspObsToV4Dsp
#
# The count in the issue thread said six, and that was wrong in both directions:
# it counted only tools that ALSO call fopen(), which dropped iccToXml,
# iccFromXml, iccRoundTrip and iccPawgReport.
#
# It also cleared iccProfilePlot, and an earlier draft of THIS header repeated
# that.  Both were wrong for the same reason: the probe invocation was rejected
# by argument validation before any path was printed, and "no escape in the
# output" was read as "no path is printed here".  iccProfilePlot needs its
# `list` subcommand to reach iccProfilePlot.cpp:248, and iccV5DspObsToV4Dsp
# needs all three operands to reach its twelve diagnostics; both leak once you
# get there.  An early reject is not evidence of a clean tool -- which is why
# every case below asserts the ESCAPED form is PRESENT, not merely that the raw
# one is absent.  iccJpegDump and iccProfileVisualize did check out:
# iccJpegDump prints no operand on any accepted invocation, and
# iccProfileVisualize substitutes "_".
#
# Twelve tools have no sanitization at all; TEN of them were measured to leak.
#
# What each defect case asserts, and why it is two assertions rather than one:
# a raw ESC must not survive to the console, AND the escaped spelling must be
# present.  "No raw ESC" alone is satisfied by a tool that fails earlier for an
# unrelated reason and prints nothing at all -- which is exactly what the first
# draft of this suite did for iccJpegDump and iccBenchApply, whose argument
# validation rejected the invocation before any path was printed.  Requiring
# the \x1B spelling pins that the path really was echoed, through the helper.
#
# The controls carry as much weight as the defect cases.  An over-eager fix --
# sanitizing a string that is not a path, or double-escaping -- would be
# invisible to every case above, so ascii-path-unchanged asserts that an
# ordinary ASCII path still prints with NO backslash escapes at all, and
# valid-conversion-still-works asserts iccToXml still produces its profile.
# Both pass on the unpatched build; that is what they are for.
#
# Red/green against 764fc176: all eight defect cases fail (raw ESC present,
# escaped form absent).  Both controls pass on BOTH builds.
#
# NOT labelled "slow": ci-pr-action.yml pins ctest_mode=fast, which drops the
# slow label, and this suite needs to run on the pull_request path (#2412).
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# An explicit ICCDEV_TOOLS_DIR is never second-guessed: resolving past it would
# measure a DIFFERENT build than the caller meant, which is how a stale tree
# fakes a result.
if [ -n "${ICCDEV_TOOLS_DIR:-}" ]; then
  TOOLS_DIR="$ICCDEV_TOOLS_DIR"
  TOOLS_DIR_EXPLICIT=1
else
  TOOLS_DIR="$REPO_ROOT/Build/Tools"
  TOOLS_DIR_EXPLICIT=0
fi
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2414-console-sanitize-sweep}"
mkdir -p "$OUTDIR"

# Resolve on a BINARY, not on the directory: Build/Tools exists in a configured
# tree as CMake scaffolding with no executables under it, and a directory test
# would resolve to a path where nothing is built (see the sibling
# directory-read-guard script, where that produced a green run that measured
# nothing).
tools_dir_has_any() {
  [ -x "$1/IccDumpProfile/iccDumpProfile" ] || [ -x "$1/IccToXml/iccToXml" ]
}
if [ "$TOOLS_DIR_EXPLICIT" -eq 0 ] && ! tools_dir_has_any "$TOOLS_DIR"; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/out/build/Tools"; do
    if tools_dir_has_any "$candidate"; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

PASS=0; FAIL=0; SKIP=0; MEASURED=0

# A CSI colour sequence is the payload: it is what a terminal ACTS on, and it
# is the shape CVE-2021-42574-style attacks use to hide text.  Built with
# printf rather than written literally so the escape cannot be lost by an
# editor or a diff tool that strips control characters.
ESC=$(printf '\033')
CSI="${ESC}[31m"

VALID_PROFILE="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"

# Cases are driven through a shared runner so every one of them applies the
# SAME pair of assertions; a per-case hand-rolled grep is how one case ends up
# weaker than the others without anyone noticing.
run_case() {
  local name="$1"; shift
  local bin="$1"; shift
  if [ ! -x "$bin" ]; then
    echo "[SKIP] $name (not built: $bin)"
    SKIP=$((SKIP+1))
    return
  fi
  MEASURED=$((MEASURED+1))
  local out
  # 20s, not 60: the CTest TIMEOUT is 300s and this suite now makes 13
  # spawns.  At 60s each, three hung tools would exhaust the budget and CTest
  # would kill the script before it printed a single [FAIL] line -- the failure
  # mode the sibling directory-guard suite sized its own budget to avoid.
  out=$(timeout 20 "$bin" "$@" 2>&1)
  local raw=0 escaped=0
  case "$out" in *"$CSI"*) raw=1 ;; esac
  case "$out" in *'\x1B[31m'*) escaped=1 ;; esac
  if [ "$raw" -eq 0 ] && [ "$escaped" -eq 1 ]; then
    echo "[PASS] $name"
    PASS=$((PASS+1))
  else
    echo "[FAIL] $name (raw_esc=$raw escaped_present=$escaped)"
    printf '%s\n' "$out" | head -5 | cat -v | sed 's/^/         /'
    FAIL=$((FAIL+1))
  fi
}

echo "=== #2414 console-sanitize sweep: TOOLS_DIR=$TOOLS_DIR ==="

# --- defect cases: a path whose NAME carries a CSI sequence ------------------
EVIL_ICC="$OUTDIR/ev${CSI}il.icc"
EVIL_XML="$OUTDIR/ev${CSI}il.xml"
EVIL_BAD="$OUTDIR/bad-ev${CSI}il.icc"
rm -f "$EVIL_ICC" "$EVIL_XML" "$EVIL_BAD"
if [ -r "$VALID_PROFILE" ]; then
  cp "$VALID_PROFILE" "$EVIL_ICC"
fi
printf 'not a profile\n' > "$EVIL_BAD"
printf '<not-icc/>\n'    > "$EVIL_XML"

run_case "dumpprofile-escapes-path"  "$TOOLS_DIR/IccDumpProfile/iccDumpProfile"   "$EVIL_ICC"
run_case "pngdump-escapes-path"      "$TOOLS_DIR/IccPngDump/iccPngDump"           "$EVIL_ICC"
run_case "roundtrip-escapes-path"    "$TOOLS_DIR/IccRoundTrip/iccRoundTrip"       "$EVIL_ICC"
run_case "pawgreport-escapes-path"   "$TOOLS_DIR/IccPawgReport/iccPawgReport"     "$EVIL_ICC"
run_case "fromcube-escapes-path"     "$TOOLS_DIR/IccFromCube/iccFromCube"         "$EVIL_BAD" "$OUTDIR/out.icc"
run_case "toxml-escapes-path"        "$TOOLS_DIR/IccToXml/iccToXml"               "$EVIL_BAD" "$OUTDIR/out.xml"
run_case "fromxml-escapes-path"      "$TOOLS_DIR/IccFromXml/iccFromXml"           "$EVIL_XML" "$OUTDIR/out2.icc"
run_case "benchapply-escapes-path"   "$TOOLS_DIR/IccBenchApply/iccBenchApply"     1 "$EVIL_BAD" 0

# iccProfilePlot needs its subcommand, and iccV5DspObsToV4Dsp needs all three
# operands, to reach a print site at all.  Both were wrongly cleared once; see
# the header.
run_case "profileplot-escapes-path"  "$TOOLS_DIR/IccProfilePlot/iccProfilePlot"   "$EVIL_BAD" list
run_case "v5dspobs-escapes-path"     "$TOOLS_DIR/IccV5DspObsToV4Dsp/iccV5DspObsToV4Dsp" \
                                     "$EVIL_BAD" "$EVIL_BAD" "$OUTDIR/v5out.icc"

# The stronger vector of the two: a line of the .cube FILE, not an operand the
# caller typed.  A downloaded LUT carries whatever its author put in it, and
# iccFromCube echoed the offending line verbatim.  Nothing else here covers
# content-derived output -- every case above is argv-derived.
EVIL_CUBE="$OUTDIR/evil-content.cube"
printf 'TITLE ok\nev%sil_keyword 1 2 3\n' "$CSI" > "$EVIL_CUBE"
run_case "fromcube-escapes-content"  "$TOOLS_DIR/IccFromCube/iccFromCube"         "$EVIL_CUBE" "$OUTDIR/out4.icc"

# --- controls: these must pass on the UNPATCHED build too --------------------
# An ordinary ASCII path must print with no backslash escaping at all.  This is
# what an over-eager or double-escaping fix would break, and no defect case
# above would notice.
ASCII_BAD="$OUTDIR/plain-ascii-bad.icc"
printf 'not a profile\n' > "$ASCII_BAD"
TOXML="$TOOLS_DIR/IccToXml/iccToXml"
if [ -x "$TOXML" ]; then
  MEASURED=$((MEASURED+1))
  out=$(timeout 20 "$TOXML" "$ASCII_BAD" "$OUTDIR/out3.xml" 2>&1)
  case "$out" in
    *'\x'*|*'\u'*)
      echo "[FAIL] ascii-path-unchanged (an ASCII path was escaped)"
      printf '%s\n' "$out" | head -3 | sed 's/^/         /'
      FAIL=$((FAIL+1)) ;;
    *"$ASCII_BAD"*)
      echo "[PASS] ascii-path-unchanged"
      PASS=$((PASS+1)) ;;
    *)
      echo "[FAIL] ascii-path-unchanged (path not echoed at all)"
      printf '%s\n' "$out" | head -3 | sed 's/^/         /'
      FAIL=$((FAIL+1)) ;;
  esac

  # And the tool must still do its job: sanitizing a diagnostic must not touch
  # the path actually handed to the profile reader.
  if [ -r "$VALID_PROFILE" ]; then
    MEASURED=$((MEASURED+1))
    if timeout 20 "$TOXML" "$VALID_PROFILE" "$OUTDIR/valid.xml" >/dev/null 2>&1 \
       && [ -s "$OUTDIR/valid.xml" ]; then
      echo "[PASS] valid-conversion-still-works"
      PASS=$((PASS+1))
    else
      echo "[FAIL] valid-conversion-still-works"
      FAIL=$((FAIL+1))
    fi
  else
    echo "[SKIP] valid-conversion-still-works (no $VALID_PROFILE)"
    SKIP=$((SKIP+1))
  fi
else
  echo "[SKIP] controls (iccToXml not built)"
  SKIP=$((SKIP+2))
fi

echo "=== Result: PASS=$PASS FAIL=$FAIL SKIP=$SKIP MEASURED=$MEASURED ==="

# A suite that measured nothing must SKIP, not pass.  Exit 0 with every case
# skipped is a green light for a defect that was never exercised.
if [ "$MEASURED" -eq 0 ]; then
  echo "No case was measured -- reporting SKIP rather than success."
  exit 77
fi
[ "$FAIL" -eq 0 ] || exit 1
exit 0
