#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
###############################################################################
# Build and run the registered threaded CMM regression under Valgrind.
#
# Exit codes:
#   0 - observed the requested clean/finding state
#   2 - invalid arguments or the requested state was not observed
#   127 - a required local tool is unavailable
###############################################################################

set -uo pipefail

usage()
{
  echo "Usage: $0 --source-dir DIR --build-dir DIR --tool TOOL --expect STATE [--runs N] [--out-dir DIR] [--label LABEL]"
  echo "  TOOL: helgrind or memcheck"
  echo "  STATE: clean or finding (finding is valid only for helgrind)"
}

source_dir=""
build_dir=""
tool=""
expect=""
runs=1
out_dir=""
label="selected"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --source-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      source_dir="$2"
      shift 2
      ;;
    --build-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      build_dir="$2"
      shift 2
      ;;
    --tool)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      tool="$2"
      shift 2
      ;;
    --expect)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      expect="$2"
      shift 2
      ;;
    --runs)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      runs="$2"
      shift 2
      ;;
    --out-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      out_dir="$2"
      shift 2
      ;;
    --label)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      label="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[FAIL] unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "$tool" in
  helgrind|memcheck) ;;
  *) echo "[FAIL] --tool must be helgrind or memcheck" >&2; exit 2 ;;
esac
case "$expect" in
  clean|finding) ;;
  *) echo "[FAIL] --expect must be clean or finding" >&2; exit 2 ;;
esac
if [ "$expect" = "finding" ] && [ "$tool" != "helgrind" ]; then
  echo "[FAIL] finding expectation is supported only for helgrind" >&2
  exit 2
fi
case "$runs" in
  ''|0*|*[!0-9]*) echo "[FAIL] --runs must be an integer from 1 through 10" >&2; exit 2 ;;
esac
runs=$((10#$runs))
if [ "$runs" -lt 1 ] || [ "$runs" -gt 10 ]; then
  echo "[FAIL] --runs must be an integer from 1 through 10" >&2
  exit 2
fi
case "$label" in
  ''|*[!A-Za-z0-9._-]*)
    echo "[FAIL] --label must contain only ASCII letters, digits, dot, dash, or underscore" >&2
    exit 2
    ;;
esac
if [ ! -f "$source_dir/Build/Cmake/CMakeLists.txt" ] ||
   [ ! -f "$source_dir/Testing/sRGB_v4_ICC_preference.icc" ]; then
  echo "[FAIL] --source-dir is not a complete iccDEV checkout: $source_dir" >&2
  exit 2
fi
for required_tool in cmake valgrind; do
  if ! command -v "$required_tool" >/dev/null 2>&1; then
    echo "[FAIL] required tool is unavailable: $required_tool" >&2
    exit 127
  fi
done

out_dir="${out_dir:-$build_dir/valgrind-logs}"
mkdir -p "$build_dir" "$out_dir"

if ! cmake -S "$source_dir/Build/Cmake" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TESTS=ON \
  -DENABLE_TOOLS=ON \
  -DENABLE_SANITIZERS=OFF \
  -DENABLE_ASAN=OFF \
  -DENABLE_UBSAN=OFF \
  -DENABLE_TSAN=OFF \
  -DENABLE_LTO=OFF; then
  echo "[FAIL] CMake configuration failed" >&2
  exit 2
fi
if ! cmake --build "$build_dir" --target iccConnectThreadTest --parallel "$(nproc)"; then
  echo "[FAIL] threaded CMM regression build failed" >&2
  exit 2
fi

test_binary="$build_dir/Testing/iccConnectThreadTest"
if [ ! -x "$test_binary" ]; then
  echo "[FAIL] regression executable is unavailable: $test_binary" >&2
  exit 2
fi

common_args=(--error-exitcode=99)
if [ "$tool" = "helgrind" ]; then
  tool_args=(--tool=helgrind --fair-sched=yes --history-level=full)
else
  tool_args=(--tool=memcheck --leak-check=full --show-leak-kinds=all
             "--errors-for-leak-kinds=definite,indirect,possible"
             --track-origins=yes)
fi

finding_seen=0
for run_no in $(seq 1 "$runs"); do
  log="$out_dir/$tool-$run_no.log"
  set +e
  valgrind "${tool_args[@]}" "${common_args[@]}" \
    "$test_binary" "$source_dir/Testing/sRGB_v4_ICC_preference.icc" \
    >"$log" 2>&1
  status=$?
  set -e

  if [ "$run_no" -eq 1 ]; then
    echo "[EVIDENCE] BEGIN $label $tool run $run_no (exit $status)"
    LC_ALL=C tr -d '\000-\011\013\014\016-\037\177' < "$log" |
      sed -n '1,200p'
    echo "[EVIDENCE] END $label $tool run $run_no"
  fi

  if [ "$expect" = "clean" ]; then
    if [ "$status" -ne 0 ] || ! grep -Eq 'ERROR SUMMARY: 0 errors' "$log"; then
      echo "[FAIL] $tool run $run_no was not clean (status $status): $log" >&2
      tail -80 "$log" >&2
      exit 2
    fi
    echo "[PASS] $tool run $run_no clean"
    continue
  fi

  if [ "$status" -eq 99 ] &&
     grep -Fq 'CIccCmm::GetNewApplyCmm' "$log" &&
     grep -Fq 'Possible data race' "$log"; then
    finding_seen=1
    echo "[PASS] helgrind run $run_no reproduced the GetNewApplyCmm race"
  elif [ "$status" -eq 0 ]; then
    echo "[WARN] helgrind run $run_no did not schedule the expected race"
  else
    echo "[FAIL] helgrind run $run_no returned unexpected status $status: $log" >&2
    tail -80 "$log" >&2
    exit 2
  fi
done

if [ "$expect" = "finding" ] && [ "$finding_seen" -ne 1 ]; then
  echo "[FAIL] expected GetNewApplyCmm race was not observed in $runs run(s)" >&2
  exit 2
fi

echo "[PASS] Valgrind $tool expectation '$expect' satisfied"
