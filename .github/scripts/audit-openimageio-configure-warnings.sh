#!/bin/bash
###############################################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Retain OpenImageIO CMake configure diagnostics while allowing only
#         the dependency probes and disabled plugins in the pinned QA build.
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

if [ "$#" -ne 1 ] || [ ! -f "$1" ]; then
  echo "Usage: audit-openimageio-configure-warnings.sh CONFIGURE_LOG" >&2
  exit 2
fi

configure_log="$1"
expected_count=0
unexpected_count=0

is_expected_warning() {
  local header="$1"
  local block="$2"

  case "$header" in
    *'src/cmake/dependency_utils.cmake:469 (find_package):'*)
      grep -Eq '"(Imath|libjpeg-turbo|OpenColorIO|yaml-cpp|minizip-ng|openjph|fmt)"' <<< "$block"
      ;;
    *'CMake Warning (dev) at src/cmake/dependency_utils.cmake:417 (set):'*)
      grep -Fq 'Cannot set "ENABLE_OpenVDB": current scope has no parent.' <<< "$block"
      ;;
    *'src/dicom.imageio/CMakeLists.txt:10 (message):'*)
      grep -Fq 'DICOM plugin will not be built, no DCMTK' <<< "$block"
      ;;
    *'src/gif.imageio/CMakeLists.txt:10 (message):'*)
      grep -Fq 'GIF plugin will not be built' <<< "$block"
      ;;
    *'src/heif.imageio/CMakeLists.txt:29 (message):'*)
      grep -Fq 'heif plugin will not be built' <<< "$block"
      ;;
    *'src/jpegxl.imageio/CMakeLists.txt:11 (message):'*)
      grep -Fq 'JPEG XL plugin will not be built' <<< "$block"
      ;;
    *'src/raw.imageio/CMakeLists.txt:12 (message):'*)
      grep -Fq 'Raw plugin will not be built' <<< "$block"
      ;;
    *)
      return 1
      ;;
  esac
}

mapfile -t warning_entries < <(
  grep -nE '^CMake (Deprecation )?Warning' "$configure_log" || true
)

for warning_index in "${!warning_entries[@]}"; do
  warning_entry="${warning_entries[$warning_index]}"
  line_number="${warning_entry%%:*}"
  header="${warning_entry#*:}"
  if [ "$warning_index" -lt "$((${#warning_entries[@]} - 1))" ]; then
    next_entry="${warning_entries[$((warning_index + 1))]}"
    block_end="$((${next_entry%%:*} - 1))"
  else
    block_end='$'
  fi
  block="$(sed -n "${line_number},${block_end}p" "$configure_log")"
  if is_expected_warning "$header" "$block"; then
    expected_count=$((expected_count + 1))
    printf '[OK] expected configure warning: %s\n' "$header"
  else
    unexpected_count=$((unexpected_count + 1))
    printf '[FAIL] unexpected configure warning at line %s:\n%s\n' \
      "$line_number" "$block" >&2
  fi
done

if [ "$unexpected_count" -ne 0 ]; then
  echo "[FAIL] $unexpected_count unexpected OpenImageIO configure warning(s)" >&2
  exit 1
fi

echo "[PASS] OpenImageIO configure warnings audited; expected=$expected_count unexpected=0"
