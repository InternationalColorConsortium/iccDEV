#!/bin/bash
###############################################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Pin ci-pr-action native and external-tooling path classification.
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
CLASSIFIER="$SCRIPT_DIR/../scripts/classify-pr-paths.sh"
failures=0
cases=0

assert_case() {
  local name="$1"
  local paths="$2"
  local expected="$3"
  local actual

  cases=$((cases + 1))
  actual="$(printf '%s\n' "$paths" | "$CLASSIFIER")"
  if [ "$actual" != "$expected" ]; then
    echo "[FAIL] $name" >&2
    echo "Expected:" >&2
    printf '%s\n' "$expected" >&2
    echo "Actual:" >&2
    printf '%s\n' "$actual" >&2
    failures=$((failures + 1))
  else
    echo "[PASS] $name"
  fi
}

expected_all_false='source_changed=false
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "OpenImageIO wrapper CMake is external only" \
  ".github/ci/tooling/openimageio/qa/CMakeLists.txt" \
  'source_changed=false
build_config_changed=false
testing_changed=false
external_tooling_changed=true
openimageio_tooling_changed=true
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "HEIF helper source is external only" \
  ".github/ci/tooling/heif/IccHeifDump.cpp" \
  'source_changed=false
build_config_changed=false
testing_changed=false
external_tooling_changed=true
openimageio_tooling_changed=false
heif_tooling_changed=true
libpng_tooling_changed=false'

assert_case "native CMake testing entry point" \
  "Build/Cmake/Testing/CMakeLists.txt" \
  'source_changed=false
build_config_changed=true
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "native library source" \
  "IccProfLib/IccProfile.cpp" \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "native generated header template" \
  "IccProfLib/IccProfLibVer.h.in" \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "native tool resource" \
  "Tools/wxWidget/wxProfileDump/bitmaps/open.xpm" \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "IIS site runtime inputs are native" \
  $'Tools/Winnt/IccIisIsapi/index.html\nTools/Winnt/IccIisIsapi/site.js\nTools/Winnt/IccIisIsapi/site.css\nTools/Winnt/IccIisIsapi/web.config' \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "IIS PowerShell helper is native" \
  "Tools/Winnt/IccIisIsapi/Test-IccIisIsapiEndpoints.ps1" \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "IIS image asset is native" \
  "Tools/Winnt/IccIisIsapi/assets/logo.png" \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "IIS documentation remains non-native" \
  "Tools/Winnt/IccIisIsapi/Readme.md" "$expected_all_false"

assert_case "workflow is governance only" \
  ".github/workflows/example.yml" "$expected_all_false"

assert_case "documentation is non-native" \
  "docs/ctest.md" "$expected_all_false"

assert_case "example source is non-native" \
  "examples/ios-apply-preview/ApplyPreviewHost.h" "$expected_all_false"

assert_case "port CMake is non-native" \
  "ports/example/CMakeLists.txt" "$expected_all_false"

assert_case "Python binding source is non-native" \
  "python/iccdev/profile.pyx" "$expected_all_false"

assert_case "MATLAB binding source is non-native" \
  "matlab/mex/icc_profile_mex.cpp" "$expected_all_false"

assert_case "Apple CMake entry point is native" \
  "Build/AppleMobile/CMakeLists.txt" \
  'source_changed=false
build_config_changed=true
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "root dependency manifest is native build configuration" \
  "vcpkg.json" \
  'source_changed=false
build_config_changed=true
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "registered native regression source" \
  ".github/ci/regression/example.cpp" \
  'source_changed=true
build_config_changed=false
testing_changed=true
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "registered native regression fixtures" \
  $'.github/ci/regression/issue-2086-noop-scnt-operators.xml\n.github/ci/regression/gamma-1.0000000000.icc\n.github/ci/regression/issue-2086-table102-operands.txt' \
  'source_changed=false
build_config_changed=false
testing_changed=true
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "libpng maintainer QA" \
  ".github/ci/tooling/libpng/qa/libpng_iccp_qa.py" \
  'source_changed=false
build_config_changed=false
testing_changed=false
external_tooling_changed=true
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=true'

assert_case "external tooling plus native source" \
  $'.github/ci/tooling/openimageio/qa/CMakeLists.txt\nIccProfLib/IccProfile.cpp' \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=true
openimageio_tooling_changed=true
heif_tooling_changed=false
libpng_tooling_changed=false'

assert_case "native-to-excluded rename retains source classification" \
  $'IccProfLib/IccProfile.cpp\nexamples/IccProfile.cpp' \
  'source_changed=true
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false'

if [ "$failures" -ne 0 ]; then
  echo "[FAIL] $failures of $cases classifier cases failed" >&2
  exit 1
fi

echo "[PASS] all $cases classifier cases passed"
