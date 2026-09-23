#!/bin/bash
###############################################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Verify the OpenImageIO configure-warning allowlist fails closed.
#
###############################################################################
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

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AUDITOR="$SCRIPT_DIR/../scripts/audit-openimageio-configure-warnings.sh"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-oiio-warning-audit.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT

printf '%s\n' \
  'CMake Warning at src/cmake/dependency_utils.cmake:469 (find_package):' \
  '  Could not find a package configuration file provided by "Imath".' \
  '' \
  'CMake Warning (dev) at src/cmake/dependency_utils.cmake:417 (set):' \
  '  Cannot set "ENABLE_OpenVDB": current scope has no parent.' \
  '' \
  'CMake Warning at src/heif.imageio/CMakeLists.txt:29 (message):' \
  '  heif plugin will not be built' \
  > "$work_dir/expected.log"

"$AUDITOR" "$work_dir/expected.log"

printf '%s\n' \
  'CMake Warning at src/cmake/new_probe.cmake:12 (message):' \
  '  A new warning must fail the audit.' \
  > "$work_dir/unexpected.log"

if "$AUDITOR" "$work_dir/unexpected.log" > "$work_dir/unexpected.out" 2>&1; then
  echo "[FAIL] unexpected configure warning passed the audit" >&2
  exit 1
fi

grep -Fq '[FAIL] unexpected configure warning' "$work_dir/unexpected.out"
echo "[PASS] unexpected configure warning failed closed"
