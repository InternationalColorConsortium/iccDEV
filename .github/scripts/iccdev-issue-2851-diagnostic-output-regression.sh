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
# Regression for #2851 and #2852: iccFromXml and iccFromJson printed the
# library's parse and validation reports to the console as-is. Those reports
# quote document text -- here a calculator variable name -- so a name holding
# CR, LF or an escape sequence wrote them to the terminal or log verbatim.
# Four XML calculator diagnostics also ended with a stray quote after their
# line feed, which began the next line with "'".
#
# Both tools now escape each line of a report (icSanitizeConsoleLines) and keep
# the line feeds between messages.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2851}"
mkdir -p "$OUTDIR"

TAG="issue-2851-diagnostic-output"
FAILURES=0

fail() {
  echo "  [FAIL] $TAG -- $1"
  FAILURES=$((FAILURES + 1))
}

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

DATA="$REPO_ROOT/.github/ci/test-data"

# check_report <name> <tool> <input> <expected message>
check_report() {
  local name="$1" tool="$2" input="$3" expect="$4"
  local log="$OUTDIR/$name.stderr"
  if [ ! -f "$input" ]; then
    fail "$name: missing fixture $input"
    return
  fi
  timeout 60s "$tool" "$input" "$OUTDIR/$name.icc" > "$OUTDIR/$name.stdout" 2> "$log"
  local rc=$?
  if [ "$rc" -eq 0 ]; then
    fail "$name: the malformed variable name was accepted"
  fi
  if ! grep -qF "$expect" "$log"; then
    fail "$name: expected '$expect' in the report"
  fi
  # A raw CR or ESC byte anywhere in the report is the defect.
  if LC_ALL=C grep -q "$(printf '\r')" "$log"; then
    fail "$name: the report carries a raw carriage return"
  fi
  if LC_ALL=C grep -q "$(printf '\033')" "$log"; then
    fail "$name: the report carries a raw escape character"
  fi
  # The injected text must not begin a line of its own.
  if grep -q '^X-Injected' "$log"; then
    fail "$name: document text started a new line of the report"
  fi
  if grep -q "^'" "$log"; then
    fail "$name: a report line begins with a stray quote"
  fi
  if grep -qE 'ERROR: (Address|Leak|Memory)Sanitizer|runtime error:' "$log"; then
    fail "$name: sanitizer report"
  fi
  if [ "$FAILURES" -ne 0 ]; then
    sed 's/^/    /' "$log" | cat -A | head -20
  fi
}

FROMXML="$(find_tool iccFromXml)"
if [ -n "$FROMXML" ] && [ -x "$FROMXML" ]; then
  check_report xml "$FROMXML" "$DATA/diag-calc-varname-2851.xml" \
    "Invalid calc element variable name 'lhs\"\\r\\nX-Injected: yes'"
else
  echo "[SKIP] iccFromXml not found under $TOOLS_DIR"
fi

FROMJSON="$(find_tool iccFromJson)"
if [ -n "$FROMJSON" ] && [ -x "$FROMJSON" ]; then
  check_report json "$FROMJSON" "$DATA/diag-calc-varname-2852.json" \
    "Invalid variable name 'lhs\"\\r\\nX-Injected: yes\\x1B[31m'"
else
  echo "[SKIP] iccFromJson not found under $TOOLS_DIR"
fi

if [ "$FAILURES" -ne 0 ]; then
  echo "[FAIL] $TAG: $FAILURES failure(s)"
  exit 1
fi
echo "[PASS] $TAG"
exit 0
