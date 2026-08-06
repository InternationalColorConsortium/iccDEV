#!/bin/bash
###############################################################################
# iccDEV issue #1985 - heap use-after-free write in the encoding converter
###############################################################################
#
# CIccDefaultEncProfileConverter::ConvertFromParams read the luma/chroma matrix
# out of the profile's colorEncodingParams ("cept") struct with
#
#     pParams->FindElemOfType(icSigCeptLumaChromaMatrixMbr, icSigFloat32ArrayType)
#
# which returns pEntry->pTag borrowed from the struct, and then deleted it once
# the values had been copied out.  The entry stayed in the element list, so the
# struct was left holding a dangling pointer; icConvertEncodingProfile destroys
# that struct on every return path out of the converter, and CIccTagStruct::
# Cleanup walks the list writing a null parent into each element before deleting
# it.  The premature free therefore became a heap use-after-free write, a
# virtual-destructor dispatch through a vptr read out of freed memory, and a
# second free of the same chunk:
#
#   ERROR: AddressSanitizer: heap-use-after-free ... WRITE of size 8
#   SCARINESS: 52 (8-byte-write-heap-use-after-free)
#     #0 IIccObject::SetParentObject         IccProfLib/IccObject.h
#     #1 CIccTagStruct::Cleanup              IccProfLib/IccTagComposite.cpp
#     #2 CIccTagStruct::~CIccTagStruct
#     #4 icConvertEncodingProfile            IccProfLib/IccEncoding.cpp
#     #5 CIccXform::Create                   IccProfLib/IccCmm.cpp
#     #6 CIccCmm::AddXform
#   freed by:
#     #2 CIccDefaultEncProfileConverter::ConvertFromParams IccProfLib/IccEncoding.cpp
#
# The .github/ci/regression/encoding-lumamtx-double-free.cpp CTest covers the
# defect at the API, deterministically and in every build.  This test covers the
# other half of the report -- that a *profile* reaches it.  The PoC profile
# declares deviceClass 'cenc', so CIccXform::Create routes it through
# icConvertEncodingProfile while it is being added to a transform chain, before
# any LUT is written.  The tool then rejects the profile as an invalid space link
# regardless; the sanitizer report, not the exit code, is the signal.
#
# The PoC (uaf-encoding-lumamtx-1985.icc, built from the .xml of the same name
# next to it) is the reproducer supplied by @xsscx on issue #1985.  It carries
# the minimum that reaches the borrowed element: a white point, a media white
# point, a 9-value 'lmat', and the three primaries.
#
# Two independent signals, so this test is meaningful on every build:
#
#   1. A crash.  Measured against the unfixed library on an unsanitized clang
#      Debug build, the tool segfaults reproducibly (3/3 runs, exit 139), because
#      the dangling entry is dispatched through as a virtual destructor.  The
#      fixed library exits 255 on the same input -- the ordinary "invalid space
#      link" rejection -- so a crash is cleanly distinguishable from the expected
#      failure.  129..192 is this repo's crash band (see the #1781 QA matrix).
#   2. An AddressSanitizer report naming this converter or the struct teardown.
#
# Signal 1 is NOT guaranteed in principle: the two frees are ~400 lines apart with
# heavy allocation in between, which usually recycles the chunk, and that also
# defeats glibc's tcache double-free check.  A different allocator or platform may
# well limp on instead of dying.  That is why signal 2 exists and why the ASAN
# lanes remain the authoritative ones -- but it is not a reason to SKIP an
# unsanitized build, which would discard a signal that does fire in practice.
#
# Environment variables (set by the CTest harness):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/
#   ICCDEV_TEST_OUTDIR -- output directory for generated artifacts / logs
#
# Exit codes:
#   0 - pass (no use-after-free) or skipped cleanly
#   2 - AddressSanitizer use-after-free finding (regression)
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1985-encoding-lumamtx-uaf}"
DATA_DIR="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then TOOLS_DIR="$candidate"; break; fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
POC_ICC="$DATA_DIR/uaf-encoding-lumamtx-1985.icc"
LOGFILE="$OUTDIR/issue-1985-encoding-lumamtx-uaf.log"

echo "=== iccApplyToLink issue-1985 encoding luma-matrix use-after-free regression ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyToLink not built under $TOOLS_DIR"
  exit 0
fi
if [ ! -f "$POC_ICC" ]; then
  echo "  [SKIP] PoC profile missing: $POC_ICC"
  exit 0
fi

# Add the PoC profile to a transform link.  The dst/title file names are
# placeholders -- the cenc profile is converted (and, pre-fix, the borrowed
# element freed) while it is being added to the transform chain, so the tool
# exits non-zero on the invalid space link regardless.  The exit code and the
# sanitizer log, not that non-zero, are the signals.
#
# print_scariness records ASAN's own severity rating in the log for triage; it
# does not affect the verdict.  detect_leaks stays off because unrelated tool
# leaks are out of scope here.
rm -f "$LOGFILE"
ASAN_OPTIONS="detect_leaks=0:print_scariness=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
  "$APPLY" "$OUTDIR/issue-1985-link.bar" 0 2 1 issue-1985 0 1 1 0 "$POC_ICC" 1 > "$LOGFILE" 2>&1
rc=$?

# Signal 1 -- a crash.  129..192 is the repo's crash band (128 + signal); the
# expected rejection on a fixed library is 255, and an ASAN abort is 1, so
# neither collides with it.
if [ "$rc" -ge 129 ] && [ "$rc" -le 192 ]; then
  echo "  [FAIL] issue-1985 -- iccApplyToLink crashed on the PoC profile (exit $rc)"
  tail -20 "$LOGFILE"
  exit 2
fi

# Signal 2 -- match the defect, not merely any sanitizer noise: require a
# use-after-free or double-free report that names this converter or the struct
# teardown it corrupts.
if grep -qE "AddressSanitizer: (heap-use-after-free|attempting double-free)" "$LOGFILE" &&
   grep -qE "ConvertFromParams|CIccTagStruct::Cleanup|IccEncoding\.cpp" "$LOGFILE"; then
  echo "  [FAIL] issue-1985 -- encoding converter use-after-free reappeared"
  grep -E "AddressSanitizer|SCARINESS|ConvertFromParams|CIccTagStruct::Cleanup|IccEncoding\.cpp" \
    "$LOGFILE" | head -20
  exit 2
fi

echo "  [PASS] issue-1985-encoding-lumamtx-uaf -- borrowed cept element left to its owner (exit $rc)"
exit 0
