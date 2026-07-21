#!/bin/bash
###############################################################################
# iccApplyNamedCmm namedColor2 fixed-string termination regression
###############################################################################
#
# The ICC Software License, Version 0.2
#
# Copyright (c) 2003-2026 The International Color Consortium. All rights
# reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# 3. In the absence of prior written permission, the names "ICC" and "The
#    International Color Consortium" must not be used to imply that the
#    ICC organization endorses or promotes products derived from this
#    software.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
# ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
###############################################################################
#
# Legacy v2 namedColor2Type stores each rootName in a fixed 32-byte field. A
# malformed profile can fill all 32 bytes without a NUL terminator. Older XML
# and named-color output paths treated that field as an unbounded C string,
# producing sanitizer findings or terminal-dependent garbage output. This test
# builds a tiny nmcl profile with one all-ASCII, non-NUL rootName so the
# regression signal is deterministic and readable.
#
# The expected behavior is that the reader forces rootName[31] = 0. Therefore a
# 32-byte source name:
#
#   ASCII_ENTRY_000_NONUL_REPRO_XXXX
#
# is reported as the 31-byte:
#
#   ASCII_ENTRY_000_NONUL_REPRO_XXX
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 2 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-ncl2-nonul-ascii-regression}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect:$BUILD_ROOT/IccConnect/IccLibConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyNamedCmm -type f 2>/dev/null | head -1)"
LOGFILE="$OUTDIR/ncl2-nonul-ascii-regression.log"
PROFILE="$OUTDIR/ncl2-nonul-ascii.icc"
DATAFILE="$OUTDIR/rgb-one-red.txt"

regress() {
  echo "  [FAIL] ncl2-nonul-ascii-regression -- $1"
  exit 2
}

check_sanitizers() {
  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$1" 2>/dev/null; then
    sed -n '1,80p' "$1"
    return 1
  fi
  return 0
}

echo "=== iccApplyNamedCmm namedColor2 non-NUL ASCII rootName regression ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyNamedCmm not built under $TOOLS_DIR"
  exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "  [SKIP] python3 not available for fixture generation"
  exit 0
fi

rm -f "$PROFILE" "$DATAFILE"

if ! python3 - "$PROFILE" <<'PY'
from pathlib import Path
import struct
import sys

out = Path(sys.argv[1])
be16 = lambda v: struct.pack(">H", v)
be32 = lambda v: struct.pack(">I", v)

desc_text = b"nonul ncl2 repro\0"
desc = (
    b"desc" + be32(0) + be32(len(desc_text) - 1) + desc_text +
    b"\0\0\0" + be32(0) + be32(0)
)
wtpt = b"XYZ " + be32(0) + be32(0x0000F6D6) + be32(0x00010000) + be32(0x0000D32D)

name = b"ASCII_ENTRY_000_NONUL_REPRO_XXXX"
if len(name) != 32:
    raise SystemExit("fixture name must be exactly 32 bytes")

ncl2 = (
    b"ncl2" + be32(0) + be32(0) + be32(1) + be32(3) +
    b"#" + b"\0" * 31 + b"\0" * 32 +
    name +
    be16(0x8000) + be16(0x8000) + be16(0x8000) +
    be16(65535) + be16(0) + be16(0)
)

tags = [(b"desc", desc), (b"wtpt", wtpt), (b"ncl2", ncl2)]
data_offset = 128 + 4 + 12 * len(tags)
entries = []
data = b""
for sig, tag in tags:
    data += b"\0" * ((-(data_offset + len(data))) % 4)
    entries.append((sig, data_offset + len(data), len(tag)))
    data += tag

size = data_offset + len(data)
header = bytearray(128)
header[0:4] = be32(size)
header[4:8] = b"appl"
header[8:12] = bytes([2, 0x20, 0, 0])
header[12:16] = b"nmcl"
header[16:20] = b"RGB "
header[20:24] = b"Lab "
header[24:36] = be16(2026) + be16(7) + be16(4) + be16(0) + be16(0) + be16(0)
header[36:40] = b"acsp"
header[40:44] = b"APPL"
header[64:68] = be32(0)
header[68:72] = be32(0x0000F6D6)
header[72:76] = be32(0x00010000)
header[76:80] = be32(0x0000D32D)
header[80:84] = b"appl"

tag_table = be32(len(tags)) + b"".join(sig + be32(offset) + be32(length) for sig, offset, length in entries)
out.write_bytes(bytes(header) + tag_table + data)
PY
then
  regress "failed to generate ncl2 profile fixture"
fi

if ! cat > "$DATAFILE" <<'EOF'
'RGB '	; Data Format
icEncodeFloat	; Encoding

1 0 0
EOF
then
  regress "failed to write named-color input fixture"
fi

rm -f "$LOGFILE"
"$APPLY" "$DATAFILE" 6:0:5 1 "$PROFILE" 93 > "$LOGFILE" 2>&1
rc=$?
check_sanitizers "$LOGFILE" || regress "sanitizer diagnostic while applying generated ncl2 profile"
if [ "$rc" -ne 0 ]; then
  sed -n '1,80p' "$LOGFILE"
  regress "iccApplyNamedCmm exited $rc"
fi

if ! grep -F '{ "#ASCII_ENTRY_000_NONUL_REPRO_XXX" }' "$LOGFILE" >/dev/null; then
  sed -n '1,80l' "$LOGFILE"
  regress "expected 31-byte terminated name was not reported"
fi

if grep -F 'ASCII_ENTRY_000_NONUL_REPRO_XXXX' "$LOGFILE" >/dev/null; then
  sed -n '1,80l' "$LOGFILE"
  regress "unterminated 32-byte name leaked through without truncation"
fi

echo "  [PASS] ncl2-nonul-ascii-regression -- fixed 32-byte rootName is terminated before output"
exit 0
