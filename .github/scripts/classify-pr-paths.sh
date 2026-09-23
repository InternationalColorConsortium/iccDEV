#!/bin/bash
###############################################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Classify changed paths for ci-pr-action without treating isolated
#         examples, ports, or maintainer tooling as native build changes.
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

changed_files="$(cat)"

has_changed_file() {
  local pattern="$1"
  grep -Eiq "$pattern" <<< "$changed_files"
}

source_changed=false
build_config_changed=false
testing_changed=false
external_tooling_changed=false
openimageio_tooling_changed=false
heif_tooling_changed=false
libpng_tooling_changed=false

native_source_pattern='^(IccProfLib|IccXML|IccJSON|IccConnect|Tools)/'
native_source_pattern+='.*\.(c|cc|cpp|cxx|h|hh|hpp|hxx|m|mm|rc|def|h\.in|xpm|ico|plist)$'
native_regression_source_pattern='^\.github/ci/regression/'
native_regression_source_pattern+='.*\.(c|cc|cpp|cxx|h|hh|hpp|hxx|m|mm|rc|def)$'
native_regression_input_pattern='^\.github/ci/regression/.*\.(icc|xml|txt)$'
iis_runtime_input_pattern='^Tools/Winnt/IccIisIsapi/'
iis_runtime_input_pattern+='(assets/.*|[^/]+\.(html|js|css|config|ps1))$'

if has_changed_file "${native_source_pattern}|${native_regression_source_pattern}|${iis_runtime_input_pattern}"; then
  source_changed=true
fi

if has_changed_file '^Build/Cmake/|^Build/AppleMobile/|^Build/XCode/BuildAll\.sh$|^vcpkg\.json$'; then
  build_config_changed=true
fi

if has_changed_file "^Testing/|${native_regression_source_pattern}|${native_regression_input_pattern}"; then
  testing_changed=true
fi

if has_changed_file '^\.github/ci/tooling/openimageio/|^\.github/workflows/ci-openimageio-icc-smoke\.yml$'; then
  openimageio_tooling_changed=true
fi

if has_changed_file '^\.github/ci/tooling/heif/|^\.github/workflows/ci-nokia-heif-icc-smoke\.yml$'; then
  heif_tooling_changed=true
fi

if has_changed_file '^\.github/ci/tooling/libpng/|^\.github/workflows/ci-libpng-iccp-smoke\.yml$'; then
  libpng_tooling_changed=true
fi

if [[ "$openimageio_tooling_changed" == "true" ||
      "$heif_tooling_changed" == "true" ||
      "$libpng_tooling_changed" == "true" ]]; then
  external_tooling_changed=true
fi

printf 'source_changed=%s\n' "$source_changed"
printf 'build_config_changed=%s\n' "$build_config_changed"
printf 'testing_changed=%s\n' "$testing_changed"
printf 'external_tooling_changed=%s\n' "$external_tooling_changed"
printf 'openimageio_tooling_changed=%s\n' "$openimageio_tooling_changed"
printf 'heif_tooling_changed=%s\n' "$heif_tooling_changed"
printf 'libpng_tooling_changed=%s\n' "$libpng_tooling_changed"
