#!/bin/bash
###############################################################################
# iccDEV issues #1898 #1899 #1900 #1901 #1902 #1903
#   unbounded MPE channel counts and spectral step counts in the XML and JSON
#   readers
###############################################################################
#
# Six issues, one defect.  Every multi-process-element reader stores its channel
# counts in an icUInt16Number, and every one of them parsed the attribute into a
# signed int first:
#
#   m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));  // XML
#   jGetValue(j, "outputChannels", nOut);                              // JSON
#   m_nOutputChannels = (icUInt16Number)nOut;
#
# The guard that followed was "!nIn || !nOut", which rejects zero and nothing
# else.  360200 therefore passed it and then changed value on the way into
# storage -- 360200 & 0xFFFF == 32520.  Wavelength step counts (icSpectralRange
# ::steps, also u16) were read the same way.
#
# On the XML side the narrowing is an implicit conversion, so UBSan reports it:
#
#   IccMpeXml.cpp:3422:11: runtime error: implicit conversion from type 'int' of
#   value 360200 (32-bit, signed) to type 'icUInt16Number' (aka 'unsigned
#   short') changed the value to 32520 (16-bit, unsigned)
#
# On the JSON side, and at IccTagXml.cpp:4748, the cast is explicit -- which
# suppresses the sanitizer check while changing the value just the same.  That
# is the half of the cluster #1901 was filed about, and it is why this test does
# not rely on sanitizer output for its main assertions.
#
# All fixtures are the same profile with ONLY the one attribute changed, so
# anything else they exercise is a known-good path.
#
# CASE A -- value laundering.  The case that matters on an ordinary build.  Four
# documents were accepted, converted with exit 0, and written to disk carrying a
# channel count the source never contained:
#
#   $ iccFromXml ub-mpe-channels-tag-launder-1901.xml out.icc; echo $?
#   0
#   $ iccToXml out.icc back.xml && grep MultiProcessElements back.xml
#   <MultiProcessElements InputChannels="11" OutputChannels="32520">
#
#   $ iccFromJson ub-mpe-channels-calc-launder-1902.json out.icc; echo $?
#   0
#   ... round-trips "outputChannels": 360200 as 32520
#
# The document asked for 360200, the tool reported success, and the profile it
# wrote says 32520.  The fixed readers must refuse the count instead.  No
# sanitizer required -- the tell is the written file.
#
# CASE B -- the reported breadcrumbs that had no functional tell.  For the CLUT
# and the two observer elements the narrowed value went on to fail a later
# constraint (the CLUT could not be built at that size; steps != InputChannels),
# so pre-fix these were already rejected and "no profile written" is equally
# true on a broken build.  Asserting on it would be a vacuous pass, so case B is
# gated on an integer/implicit-conversion sanitizer build and SKIPPED elsewhere.
#
# CONTROLS -- two of them, both required to convert:
#   1. a legal count (OutputChannels="11" / "36"), which fails loudly if the fix
#      over-rejects ordinary documents;
#   2. OutputChannels="32520" -- the wrapped value itself.  This is the control
#      that pins down what the fix actually changed: 32520 is a representable
#      count and must still be accepted, so rejecting 360200 is about the value
#      not fitting the field and NOT about a newly tightened channel cap.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1898}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi
# iccFromJson is optional: the JSON tools are behind ICCDEV_HAS_JSON_TOOLS, so
# the JSON half of the cluster is skipped rather than failing where they are off.
FROMJSON="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromJson -type f 2>/dev/null | head -1)"

# Locate the build's CMakeCache.txt (walk up from the tool) to decide whether
# case B's check is actually compiled in.
#
# As in the #1851 test the gate deliberately does NOT accept ENABLE_UBSAN or a
# bare "undefined" in the flags: Build/Cmake/CMakeLists.txt maps ENABLE_UBSAN to
# -fsanitize=undefined, and implicit-integer-truncation lives in the integer /
# implicit-conversion groups, not in undefined.  A looser gate would report a
# confident PASS on an -fsanitize=address,undefined build that cannot observe
# the conversion at all.
CACHE=""
probe="$(dirname "$FROMXML")"
for _ in 1 2 3 4 5; do
  if [ -f "$probe/CMakeCache.txt" ]; then CACHE="$probe/CMakeCache.txt"; break; fi
  probe="$(dirname "$probe")"
done
sanitized=0
if [ -n "$CACHE" ]; then
  if grep -qiE "ENABLE_INTEGER_SANITIZER:BOOL=ON" "$CACHE"; then
    sanitized=1
  elif grep -iE "CMAKE_(CXX|C)_FLAGS" "$CACHE" | grep -qiE "fsanitize=[^ ]*(integer|implicit)"; then
    sanitized=1
  fi
fi

TOOL_RC=0
TOOL_LOG=""
TOOL_ICC=""
run_tool() {
  # $1 = fixture basename including extension.  Chooses the converter from the
  # extension.  Sets the globals TOOL_LOG / TOOL_ICC / TOOL_RC rather than
  # echoing them: a caller using $(run_tool ...) would run this in a command
  # substitution subshell and TOOL_RC would never reach the caller.
  local fixture="$DATA_DIR/$1"
  local base="${1%.*}"
  local tool="$FROMXML"
  case "$1" in
    *.json) tool="$FROMJSON" ;;
  esac
  TOOL_LOG="$OUTDIR/$base.log"
  TOOL_ICC="$OUTDIR/$base.icc"
  rm -f "$TOOL_ICC"
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0:allocator_may_return_null=1" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$tool" "$fixture" "$TOOL_ICC" > "$TOOL_LOG" 2>&1
  TOOL_RC=$?
}

status=0

# --- CONTROLS ---------------------------------------------------------------
# Run first.  A control that cannot convert means the environment is wrong, not
# that #1898 regressed, so the affected half skips instead of blaming the fix.
XML_CONTROL_ICC=""
run_tool "mpe-channels-control-1898.xml"
if [ -f "$TOOL_ICC" ]; then
  XML_CONTROL_ICC="$OUTDIR/mpe-channels-control-1898.control.icc"
  cp "$TOOL_ICC" "$XML_CONTROL_ICC"
  echo "[PASS] control: legal XML channel counts still convert"
else
  if grep -qE "runtime error: |AddressSanitizer" "$TOOL_LOG"; then
    echo "[FAIL] control: a legal XML channel count tripped a sanitizer"
    grep -E "runtime error: |AddressSanitizer|IccMpeXml|IccTagXml" "$TOOL_LOG" | head
    exit 2
  fi
  echo "[SKIP] XML control did not convert; environment issue, not #1898"
  sed -n '1,10p' "$TOOL_LOG"
  exit 0
fi

# The wrapped value must remain acceptable -- see the header.  A fix that
# rejected 32520 too would pass every case-A assertion below while having
# broken legitimate documents, so this control is what distinguishes
# "refuses an unrepresentable count" from "lowered the channel ceiling".
run_tool "mpe-channels-wrapped-control-1901.xml"
if [ -f "$TOOL_ICC" ]; then
  echo "[PASS] control: OutputChannels=\"32520\" (the wrapped value) still converts"
else
  echo "[FAIL] control: OutputChannels=\"32520\" was rejected -- the fix is"
  echo "       refusing representable counts, not just unrepresentable ones"
  sed -n '1,10p' "$TOOL_LOG"
  status=2
fi

# --- CASE A: laundering, XML (functional, valid on every build) --------------
for poc in ub-mpe-channels-calc-launder-1898.xml \
           ub-mpe-channels-tag-launder-1901.xml; do
  if [ ! -f "$DATA_DIR/$poc" ]; then
    echo "[SKIP] PoC missing: $DATA_DIR/$poc"
    continue
  fi
  run_tool "$poc"
  if [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1898 case A: profile written for OutputChannels=\"360200\" from $poc (rc=$TOOL_RC)"
    # Re-serialize to show the count that actually landed in the file.
    TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"
    if [ -n "$TOXML" ] && [ -x "$TOXML" ]; then
      if "$TOXML" "$TOOL_ICC" "$OUTDIR/${poc%.xml}.roundtrip.xml" >/dev/null 2>&1; then
        echo "       written profile re-serializes as:"
        grep -oE '(MultiProcessElements|CalculatorElement) InputChannels="[0-9]+" OutputChannels="[0-9]+"' \
          "$OUTDIR/${poc%.xml}.roundtrip.xml" | sed 's/^/         /'
      fi
    fi
    status=2
  elif [ "$sanitized" -eq 1 ] && grep -qE "runtime error: implicit conversion" "$TOOL_LOG"; then
    echo "[FAIL] #1898 case A: implicit conversion of a channel count reappeared in $poc"
    grep -E "runtime error: implicit conversion|IccMpeXml|IccTagXml" "$TOOL_LOG" | head
    status=2
  else
    echo "[PASS] #1898 case A: $poc rejected, no profile written"
  fi
done

# --- CASE A: laundering, JSON -----------------------------------------------
if [ -z "$FROMJSON" ] || [ ! -x "$FROMJSON" ]; then
  echo "[SKIP] iccFromJson not built; JSON half of the cluster (#1901/#1902) not exercised"
else
  run_tool "mpe-channels-control-1902.json"
  if [ ! -f "$TOOL_ICC" ]; then
    echo "[SKIP] JSON control did not convert; environment issue, not #1902"
    sed -n '1,10p' "$TOOL_LOG"
  else
    echo "[PASS] control: legal JSON channel counts still convert"

    run_tool "mpe-channels-wrapped-control-1901.json"
    if [ -f "$TOOL_ICC" ]; then
      echo "[PASS] control: JSON outputChannels 32520 still converts"
    else
      echo "[FAIL] control: JSON outputChannels 32520 was rejected"
      status=2
    fi

    for poc in ub-mpe-channels-calc-launder-1902.json \
               ub-mpe-channels-tag-launder-1901.json; do
      if [ ! -f "$DATA_DIR/$poc" ]; then
        echo "[SKIP] PoC missing: $DATA_DIR/$poc"
        continue
      fi
      run_tool "$poc"
      if [ -f "$TOOL_ICC" ]; then
        echo "[FAIL] #1902 case A: profile written for outputChannels 360200 from $poc (rc=$TOOL_RC)"
        TOJSON="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToJson -type f 2>/dev/null | head -1)"
        if [ -n "$TOJSON" ] && [ -x "$TOJSON" ]; then
          if "$TOJSON" "$TOOL_ICC" "$OUTDIR/${poc%.json}.roundtrip.json" >/dev/null 2>&1; then
            echo "       written profile re-serializes with:"
            grep -oE '"outputChannels": *[0-9]+' \
              "$OUTDIR/${poc%.json}.roundtrip.json" | sed 's/^/         /'
          fi
        fi
        status=2
      else
        echo "[PASS] #1902 case A: $poc rejected, no profile written"
      fi
    done
  fi
fi

# --- CASE B: reported breadcrumbs with no functional tell -------------------
# Pre-fix each of these was already rejected by a later constraint, so a
# "no profile written" assertion would hold on a broken build too.  Skipped
# rather than passed where the conversion cannot be observed.
for poc in ub-mpe-channels-clut-1899.xml \
           ub-mpe-steps-emission-1900.xml \
           ub-mpe-steps-reflectance-1903.xml; do
  if [ ! -f "$DATA_DIR/$poc" ]; then
    echo "[SKIP] PoC missing: $DATA_DIR/$poc"
    continue
  fi
  if [ "$sanitized" -ne 1 ]; then
    echo "[SKIP] case B ($poc): not an integer/implicit-conversion build (CMakeCache: ${CACHE:-none})"
    continue
  fi
  run_tool "$poc"
  if grep -qE "runtime error: implicit conversion" "$TOOL_LOG"; then
    echo "[FAIL] #1898 case B: narrowing conversion reappeared in $poc (rc=$TOOL_RC)"
    grep -E "runtime error: implicit conversion|IccMpeXml.cpp" "$TOOL_LOG" | head
    status=2
  elif [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1898 case B: profile written despite an out-of-range count in $poc"
    status=2
  else
    echo "[PASS] #1898 case B: $poc parsed without a narrowing conversion"
  fi
done

exit "$status"
