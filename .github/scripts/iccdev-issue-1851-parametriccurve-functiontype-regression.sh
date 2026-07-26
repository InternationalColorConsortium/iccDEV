#!/bin/bash
###############################################################################
# iccDEV issue #1851 - unvalidated ParametricCurve FunctionType in the XML
#                      parametricCurveType reader
###############################################################################
#
# CIccTagXmlParametricCurve::ParseXml read the attribute with atoi():
#
#   if (!SetFunctionType(atoi(functionType))){      // IccTagXml.cpp
#     return false;
#   }
#
# ICC.1 defines the parametric curve function type as 0-4, and
# CIccTagParametricCurve::SetFunctionType takes an icUInt16Number.  atoi()
# returns a signed int, so every value outside 0..65535 narrowed on the way in,
# and the guard could not catch it: SetFunctionType is documented
# "Return: always true!!" and ends in an unconditional `return true`, so
# `if (!SetFunctionType(...))` has never rejected anything.
#
# Both PoCs are param-functiontype-control-1851.xml with ONLY the redTRCTag
# FunctionType attribute changed, so anything else they exercise is a
# known-good path.  The control is converted first and the whole test skips if
# it does not convert, which keeps an unrelated environment problem from being
# reported as a #1851 regression.
#
# CASE A -- value laundering.  This is the case that matters on an ordinary
# build.  FunctionType="65540" is not a legal type, but 65540 narrowed to
# icUInt16Number is 4, which IS legal and takes 7 parameters -- exactly how many
# the PoC supplies.  So the malformed attribute was laundered into a valid one:
#
#   $ iccFromXml ub-parametriccurve-functiontype-launder-1851.xml out.icc
#   Profile parsed and saved correctly
#   $ cmp out.icc control.icc && echo identical
#   identical
#
# iccFromXml wrote a profile whose red TRC claims function type 4 while the
# source document said 65540 - a value the file never contained.  The fixed
# reader must reject it.  No sanitizer required.
#
# CASE B -- the breadcrumb exactly as filed.  FunctionType="96948242 1.23
# 0.44994"; atoi() stops at the first non-digit and returns 96948242, which
# narrows to 20498:
#
#   IccTagXml.cpp:3538:30: runtime error: implicit conversion from type 'int'
#   of value 96948242 (32-bit, signed) to type 'icUInt16Number' (aka 'unsigned
#   short') changed the value to 20498 (16-bit, unsigned)
#
# 20498 is not a legal type, so the parameter-count check below already rejected
# the tag pre-fix.  Case B therefore has NO functional tell and is meaningful
# only under an integer/implicit-conversion sanitizer build; it is skipped
# cleanly elsewhere rather than reported as a vacuous pass.
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
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1851}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
if [ -z "$FROMXML" ] || [ ! -x "$FROMXML" ]; then
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
  exit 0
fi

# Locate the build's CMakeCache.txt (walk up from the tool) to decide whether
# case B's check is actually compiled in.
#
# The gate deliberately does NOT accept ENABLE_UBSAN / a bare "undefined" in the
# flags.  Build/Cmake/CMakeLists.txt maps ENABLE_UBSAN to -fsanitize=undefined,
# and implicit-integer-truncation lives in the integer/implicit-conversion
# groups, not in undefined.  A looser test reports a confident PASS on an
# -fsanitize=address,undefined build that cannot observe the defect at all.
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
  # $1 = PoC basename (without .xml).
  #
  # Sets the globals TOOL_LOG / TOOL_ICC / TOOL_RC rather than echoing the log
  # path: a caller writing LOG=$(run_tool ...) would run this in a command
  # substitution subshell, and TOOL_RC would never reach the caller.
  local poc="$DATA_DIR/$1.xml"
  TOOL_LOG="$OUTDIR/$1.log"
  TOOL_ICC="$OUTDIR/$1.icc"
  rm -f "$TOOL_ICC"
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=0:exitcode=0:allocator_may_return_null=1" \
  UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
    "$FROMXML" "$poc" "$TOOL_ICC" > "$TOOL_LOG" 2>&1
  TOOL_RC=$?
}

# --- CONTROL: a legal FunctionType="4" curve must still convert --------------
# Run first.  If the fix ever over-rejects, this fails loudly instead of the
# two PoCs passing for the wrong reason; if the control cannot convert for an
# unrelated reason, the whole test skips rather than blaming #1851.
CONTROL="param-functiontype-control-1851"
if [ ! -f "$DATA_DIR/$CONTROL.xml" ]; then
  echo "[SKIP] control XML missing: $DATA_DIR/$CONTROL.xml"
  exit 0
fi
run_tool "$CONTROL"
CONTROL_ICC="$TOOL_ICC"
if [ ! -f "$CONTROL_ICC" ]; then
  if grep -qE "runtime error: |AddressSanitizer" "$TOOL_LOG"; then
    echo "[FAIL] #1851 control: legal FunctionType=\"4\" curve tripped a sanitizer"
    grep -E "runtime error: |AddressSanitizer|IccTagXml" "$TOOL_LOG" | head
    exit 2
  fi
  echo "[SKIP] control profile did not convert; environment issue, not #1851"
  sed -n '1,10p' "$TOOL_LOG"
  exit 0
fi
echo "[PASS] #1851 control: legal FunctionType=\"4\" curve still converts"

status=0

# --- CASE A: laundering (functional, valid on every build) -------------------
POC_A="ub-parametriccurve-functiontype-launder-1851"
if [ ! -f "$DATA_DIR/$POC_A.xml" ]; then
  echo "[SKIP] PoC XML missing: $DATA_DIR/$POC_A.xml"
else
  run_tool "$POC_A"
  if [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1851 case A: profile written for FunctionType=\"65540\" (rc=$TOOL_RC)"
    if cmp -s "$TOOL_ICC" "$CONTROL_ICC"; then
      echo "       output is byte-identical to the FunctionType=\"4\" control:"
      echo "       the out-of-range attribute was laundered into a legal type"
    fi
    status=2
  elif [ "$sanitized" -eq 1 ] && grep -qE "runtime error: implicit conversion" "$TOOL_LOG"; then
    echo "[FAIL] #1851 case A: implicit conversion of FunctionType reappeared"
    grep -E "runtime error: implicit conversion|IccTagXml" "$TOOL_LOG" | head
    status=2
  else
    echo "[PASS] #1851 case A: out-of-range FunctionType rejected, no profile written"
  fi
fi

# --- CASE B: the reported breadcrumb (sanitizer builds only) -----------------
POC_B="ub-parametriccurve-functiontype-overflow-1851"
if [ ! -f "$DATA_DIR/$POC_B.xml" ]; then
  echo "[SKIP] PoC XML missing: $DATA_DIR/$POC_B.xml"
elif [ "$sanitized" -ne 1 ]; then
  # Deliberately a SKIP, not a PASS.  Pre-fix this PoC was already rejected by
  # the parameter-count check, so "no profile written" is true on a broken
  # build too and would be a vacuous green.
  echo "[SKIP] case B: not an integer/implicit-conversion build (CMakeCache: ${CACHE:-none})"
else
  run_tool "$POC_B"
  if grep -qE "runtime error: implicit conversion" "$TOOL_LOG"; then
    echo "[FAIL] #1851 case B: ParametricCurve FunctionType sanitizer finding reappeared (rc=$TOOL_RC)"
    grep -E "runtime error: implicit conversion|IccTagXml.cpp" "$TOOL_LOG" | head
    status=2
  elif [ -f "$TOOL_ICC" ]; then
    echo "[FAIL] #1851 case B: profile written despite malformed FunctionType"
    status=2
  else
    echo "[PASS] #1851 case B: malformed FunctionType parsed without a narrowing conversion"
  fi
fi

exit "$status"
