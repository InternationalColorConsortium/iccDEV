#!/bin/bash
# Issue #2964 file-backed sampled-curve SetSize() failure regression coverage.
#
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
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

set -uo pipefail

TOOLS_DIR="${ICCDEV_TOOLS_DIR:-Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-xml-sampled-curve-setsize}"
PYTHON="${ICCDEV_PYTHON:-python3}"
mkdir -p "$OUTDIR"

if [ -d "$TOOLS_DIR" ]; then
  TOOLS_DIR="$(cd "$TOOLS_DIR" && pwd -P)"
fi

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

PASS=0
FAIL=0
SKIP=0

pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1 -- $2"; FAIL=$((FAIL + 1)); }
skip() { echo "  [SKIP] $1 -- $2"; SKIP=$((SKIP + 1)); }

has_sanitizer_finding() {
  grep -qE 'ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL' "$1" 2>/dev/null
}

was_signal_death() {
  [ "$1" -ge 129 ] && [ "$1" -le 192 ]
}

if [ ! -x "$FROMXML" ]; then
  echo "  [SKIP] iccFromXml not built at $FROMXML"
  exit 0
fi
if ! command -v "$PYTHON" >/dev/null 2>&1; then
  echo "  [SKIP] Python interpreter '$PYTHON' is unavailable"
  exit 0
fi

"$PYTHON" - "$OUTDIR" <<'PY'
import pathlib
import sys

out = pathlib.Path(sys.argv[1])

def profile(first_curve):
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <ProfileVersion>5.10</ProfileVersion>
    <ProfileSubVersion>1.0</ProfileSubVersion>
    <ProfileDeviceClass>mid </ProfileDeviceClass>
    <ProfileSubClass>osel</ProfileSubClass>
    <DataColourSpace>nc0002</DataColourSpace>
    <PCS></PCS>
    <RenderingIntent>Relative Colorimetric</RenderingIntent>
    <PCSIlluminant><XYZNumber X="0" Y="0" Z="0"/></PCSIlluminant>
    <MCS>mc0002</MCS>
  </Header>
  <Tags>
    <AToM0Tag><multiProcessElementType>
      <MultiProcessElements InputChannels="2" OutputChannels="2">
        <CurveSetElement InputChannels="2" OutputChannels="2">
          {first_curve}
          <SingleSampledCurve Start="-infinity" End="+infinity"
              FirstEntry="0" LastEntry="1" StorageType="3" ExtensionType="0">
            0 255
          </SingleSampledCurve>
        </CurveSetElement>
      </MultiProcessElements>
    </multiProcessElementType></AToM0Tag>
  </Tags>
</IccProfile>
'''

def single_curve(filename):
    return f'''<SingleSampledCurve Start="-infinity" End="+infinity"
              FirstEntry="0" LastEntry="1" StorageType="3" ExtensionType="0"
              Filename="{filename}" Format="binary" Endian="little"/>'''

segment = '''<SegmentedCurve>
            <FormulaSegment Start="-infinity" End="0" FunctionType="0">1 0 0 0</FormulaSegment>
            <SampledSegment Start="0" End="1" StorageType="3"
                Filename="sampled-segment-257.bin" Format="binary" Endian="little"/>
            <FormulaSegment Start="1" End="+infinity" FunctionType="0">1 0 0 1</FormulaSegment>
          </SegmentedCurve>'''

(out / 'single-65536.bin').write_bytes(bytes(65536))
(out / 'single-65535.bin').write_bytes(bytes(65535))
(out / 'sampled-segment-257.bin').write_bytes(bytes(257))
(out / 'single-over-cap.xml').write_text(profile(single_curve('single-65536.bin')))
(out / 'single-at-cap.xml').write_text(profile(single_curve('single-65535.bin')))
(out / 'sampled-segment.xml').write_text(profile(segment))
PY

echo "=== SingleSampledCurve deterministic SetSize refusal ==="

single_log="$OUTDIR/single-over-cap.log"
single_icc="$OUTDIR/single-over-cap.icc"
rm -f "$single_icc"
(cd "$OUTDIR" && "$FROMXML" single-over-cap.xml "$single_icc") >"$single_log" 2>&1
single_rc=$?

if was_signal_death "$single_rc"; then
  fail "xml-single-sampled-curve-over-cap" "iccFromXml died on signal $((single_rc - 128))"
elif has_sanitizer_finding "$single_log"; then
  fail "xml-single-sampled-curve-over-cap" "sanitizer finding after SetSize() refusal"
elif [ "$single_rc" -eq 0 ]; then
  fail "xml-single-sampled-curve-over-cap" "65,536 samples were accepted"
elif [ -e "$single_icc" ]; then
  fail "xml-single-sampled-curve-over-cap" "a profile was written after the curve failed"
elif ! grep -q "Unable to store 65536 samples.*Single Sampled Curve" "$single_log"; then
  fail "xml-single-sampled-curve-over-cap" "missing curve-size diagnostic"
else
  pass "xml-single-sampled-curve-over-cap"
fi

single_control_log="$OUTDIR/single-at-cap.log"
single_control_icc="$OUTDIR/single-at-cap.icc"
rm -f "$single_control_icc"
(cd "$OUTDIR" && "$FROMXML" single-at-cap.xml "$single_control_icc") >"$single_control_log" 2>&1
single_control_rc=$?

if has_sanitizer_finding "$single_control_log"; then
  fail "xml-single-sampled-curve-at-cap" "sanitizer finding at 65,535 samples"
elif [ ! -s "$single_control_icc" ] || ! grep -q "Profile parsed" "$single_control_log"; then
  fail "xml-single-sampled-curve-at-cap" "the exact boundary did not parse (rc=$single_control_rc)"
else
  pass "xml-single-sampled-curve-at-cap"
fi

echo "=== SampledSegment allocation failure ==="

segment_control_log="$OUTDIR/sampled-segment-control.log"
segment_control_icc="$OUTDIR/sampled-segment-control.icc"
rm -f "$segment_control_icc"
(cd "$OUTDIR" && "$FROMXML" sampled-segment.xml "$segment_control_icc") >"$segment_control_log" 2>&1
segment_control_rc=$?

if has_sanitizer_finding "$segment_control_log"; then
  fail "xml-sampled-segment-control" "sanitizer finding without injection"
elif [ ! -s "$segment_control_icc" ] || ! grep -q "Profile parsed" "$segment_control_log"; then
  fail "xml-sampled-segment-control" "the 257-sample control did not parse (rc=$segment_control_rc)"
else
  pass "xml-sampled-segment-control"
fi

if [ "$(uname -s)" != "Linux" ]; then
  skip "xml-sampled-segment-allocation-failure" "LD_PRELOAD fault injection is Linux-only"
else
  cc_bin="${CC:-cc}"
  shim_src="$OUTDIR/fail-sampled-segment-calloc.c"
  shim_so="$OUTDIR/fail-sampled-segment-calloc.so"
  shim_build_log="$OUTDIR/fail-sampled-segment-calloc-build.log"
  sentinel="$OUTDIR/calloc-shim-loaded"

  cat >"$shim_src" <<'SHIM'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void *(*calloc_fn)(size_t, size_t);

void *calloc(size_t count, size_t size)
{
  static calloc_fn real_calloc;
  static int announced;
  const char *sentinel;
  const char *enabled;

  if (!real_calloc) {
    real_calloc = (calloc_fn)dlsym(RTLD_NEXT, "calloc");
    if (!real_calloc)
      _exit(126);
  }

  sentinel = getenv("ICCDEV_SHIM_SENTINEL");
  if (!announced && sentinel) {
    int fd;
    announced = 1;
    fd = open(sentinel, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0)
      close(fd);
  }

  enabled = getenv("ICCDEV_FAIL_SAMPLE_CALLOC");
  if (enabled && !strcmp(enabled, "1") && count == 257 && size == 4) {
    static const char message[] = "iccdev fault injection: calloc(257, 4) refused\n";
    ssize_t ignored = write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)ignored;
    return NULL;
  }

  return real_calloc(count, size);
}
SHIM

  if ! command -v "$cc_bin" >/dev/null 2>&1; then
    skip "xml-sampled-segment-allocation-failure" "no C compiler ($cc_bin)"
  elif ! "$cc_bin" -shared -fPIC -O2 -Wall -Wextra -o "$shim_so" "$shim_src" -ldl >"$shim_build_log" 2>&1; then
    skip "xml-sampled-segment-allocation-failure" "fault-injection DSO did not build"
  else
    shim_control_log="$OUTDIR/calloc-shim-control.log"
    shim_control_icc="$OUTDIR/calloc-shim-control.icc"
    rm -f "$sentinel" "$shim_control_icc"
    (cd "$OUTDIR" && ICCDEV_SHIM_SENTINEL="$sentinel" \
      ICCDEV_FAIL_SAMPLE_CALLOC=0 LD_PRELOAD="$shim_so" \
      "$FROMXML" sampled-segment.xml "$shim_control_icc") >"$shim_control_log" 2>&1
    shim_control_rc=$?

    if [ ! -f "$sentinel" ]; then
      skip "xml-sampled-segment-allocation-failure" "calloc interposition unavailable (sanitizer/static runtime)"
    elif [ ! -s "$shim_control_icc" ] || ! grep -q "Profile parsed" "$shim_control_log"; then
      fail "xml-sampled-segment-allocation-failure" "DSO control did not parse (rc=$shim_control_rc)"
    else
      injected_log="$OUTDIR/sampled-segment-injected.log"
      injected_icc="$OUTDIR/sampled-segment-injected.icc"
      rm -f "$injected_icc"
      (cd "$OUTDIR" && ICCDEV_FAIL_SAMPLE_CALLOC=1 LD_PRELOAD="$shim_so" \
        "$FROMXML" sampled-segment.xml "$injected_icc") >"$injected_log" 2>&1
      injected_rc=$?

      if ! grep -q "iccdev fault injection: calloc(257, 4) refused" "$injected_log"; then
        fail "xml-sampled-segment-allocation-failure" "the targeted allocation was not refused"
      elif was_signal_death "$injected_rc"; then
        fail "xml-sampled-segment-allocation-failure" "iccFromXml died on signal $((injected_rc - 128))"
      elif has_sanitizer_finding "$injected_log"; then
        fail "xml-sampled-segment-allocation-failure" "sanitizer finding after allocation failure"
      elif [ "$injected_rc" -eq 0 ]; then
        fail "xml-sampled-segment-allocation-failure" "allocation failure was accepted"
      elif [ -e "$injected_icc" ]; then
        fail "xml-sampled-segment-allocation-failure" "a profile was written after allocation failure"
      elif ! grep -q "Unable to store 257 samples.*Sampled Curve Segment" "$injected_log"; then
        fail "xml-sampled-segment-allocation-failure" "missing segment-size diagnostic"
      else
        pass "xml-sampled-segment-allocation-failure"
      fi
    fi
  fi
fi

echo "=== Summary ==="
echo "  Passed:  $PASS"
echo "  Failed:  $FAIL"
echo "  Skipped: $SKIP"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
exit 0
