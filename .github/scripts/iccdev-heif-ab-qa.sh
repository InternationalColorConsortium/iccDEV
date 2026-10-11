#!/usr/bin/env bash
###############################################################################
# iccDEV HEIF carrier and IccProfLib A/B regression test
###############################################################################
#
# Copyright (c) International Color Consortium.
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
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
###############################################################################

set -euo pipefail

carrier_tool="${ICCDEV_HEIF_CARRIER_TOOL:-}"
native_tool="${ICCDEV_HEIF_NATIVE_TOOL:-}"
corpus="${ICCDEV_HEIF_CORPUS:-}"
outdir="${ICCDEV_TEST_OUTDIR:-}"

if [[ ! -x "$carrier_tool" || ! -x "$native_tool" || ! -d "$corpus" || -z "$outdir" ]]; then
  echo "[FAIL] missing HEIF A/B tool, corpus, or output directory"
  exit 1
fi
mkdir -p "$outdir"

check_runtime_findings() {
  local logfile="$1"
  ! grep -Eq 'ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:' "$logfile"
}

run_tool() {
  local tool="$1"
  local fixture="$2"
  local directory="$3"
  local logfile="$4"

  rm -rf -- "$directory"
  mkdir -p "$directory"
  if ! "$tool" "$corpus/$fixture" "$directory" >"$logfile" 2>&1; then
    echo "[FAIL] $fixture: $(basename "$tool") exited nonzero"
    sed -n '1,80p' "$logfile"
    return 1
  fi
  if ! check_runtime_findings "$logfile"; then
    echo "[FAIL] $fixture: sanitizer finding from $(basename "$tool")"
    sed -n '1,80p' "$logfile"
    return 1
  fi
}

for fixture in valid-prof.heic valid-rICC.heic duplicate-prof.heic conflicting-prof.heic conflicting-nclx.heic malformed-prof.heic short-prof.hif; do
  carrier_dir="$outdir/carrier/$fixture"
  native_dir="$outdir/native/$fixture"
  carrier_log="$outdir/carrier-$fixture.log"
  native_log="$outdir/native-$fixture.log"

  run_tool "$carrier_tool" "$fixture" "$carrier_dir" "$carrier_log"
  run_tool "$native_tool" "$fixture" "$native_dir" "$native_log"

  if ! diff -u <(cd "$carrier_dir" && find . -type f -name '*.icc' -printf '%P\n' | sort) \
      <(cd "$native_dir" && find . -type f -name '*.icc' -printf '%P\n' | sort); then
    echo "[FAIL] $fixture: extracted profile names differ"
    exit 1
  fi

  while IFS= read -r profile; do
    if ! cmp -s "$carrier_dir/$profile" "$native_dir/$profile"; then
      echo "[FAIL] $fixture: profile bytes differ for $profile"
      exit 1
    fi
  done < <(cd "$carrier_dir" && find . -type f -name '*.icc' -printf '%P\n' | sort)

  if ! grep -Fq 'icc_profiles=' "$native_log" || ! grep -Fq 'proflib_parse=' "$native_log"; then
    echo "[FAIL] $fixture: native parse report is incomplete"
    exit 1
  fi

  echo "[PASS] $fixture: raw profiles match and IccProfLib parse status reported"
done

oversized="$outdir/oversized-valid-prof.heic"
oversized_log="$outdir/oversized-valid-prof.log"
cp "$corpus/valid-prof.heic" "$oversized"
truncate -s 268435457 "$oversized"
oversized_status=0
"$native_tool" "$oversized" >"$oversized_log" 2>&1 || oversized_status=$?
if (( oversized_status == 0 || oversized_status >= 128 )) ||
   ! grep -Fq 'Error: unable to read HEIF colour properties' "$oversized_log" ||
   ! check_runtime_findings "$oversized_log"; then
  echo "[FAIL] oversized carrier was not rejected safely"
  sed -n '1,80p' "$oversized_log"
  exit 1
fi
rm -f -- "$oversized"
echo "[PASS] oversized carrier rejected before buffering"

amplified="$outdir/amplified-associations.heic"
amplified_log="$outdir/amplified-associations.log"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
generator="$script_dir/../ci/tooling/heif/qa/make-association-stress.py"
python3 "$generator" "$amplified" 65
amplified_status=0
"$native_tool" "$amplified" >"$amplified_log" 2>&1 || amplified_status=$?
if (( amplified_status == 0 || amplified_status >= 128 )) ||
   ! grep -Fq 'Error: unable to read HEIF colour properties' "$amplified_log" ||
   ! check_runtime_findings "$amplified_log"; then
  echo "[FAIL] repeated ICC associations were not bounded safely"
  sed -n '1,80p' "$amplified_log"
  exit 1
fi
rm -f -- "$amplified"
echo "[PASS] repeated ICC associations rejected before extraction"

python3 "$generator" "$amplified" 64
within_status=0
"$native_tool" "$amplified" >"$amplified_log" 2>&1 || within_status=$?
if (( within_status == 0 || within_status >= 128 )) ||
   ! grep -Fq 'Error: unable to read HEIF file:' "$amplified_log" ||
   ! check_runtime_findings "$amplified_log"; then
  echo "[FAIL] parser rejected associations at the 256 MiB extraction limit"
  sed -n '1,80p' "$amplified_log"
  exit 1
fi
rm -f -- "$amplified"
echo "[PASS] associations at the extraction limit reached libheif validation"

echo "[PASS] HEIF carrier and IccProfLib A/B QA"
