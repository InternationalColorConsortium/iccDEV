#!/bin/bash
###############################################################################
# Build iccDEV with an instrumented libc++ and validate focused MSan coverage.
###############################################################################

set -euo pipefail

usage()
{
  echo "Usage: $0 --source-dir DIR --build-dir DIR --runtime-dir DIR [--out-dir DIR]"
}

source_dir=""
build_dir=""
runtime_dir=""
out_dir=""

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
    --runtime-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      runtime_dir="$2"
      shift 2
      ;;
    --out-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      out_dir="$2"
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

fixture_dir="$source_dir/.github/ci/test-data"
short_fixture="$fixture_dir/json-parametric-short-params.json"
control_fixture="$fixture_dir/json-parametric-complete-params.json"
out_dir="${out_dir:-$build_dir/msan-taint-logs}"

for required_tool in clang clang++ cmake grep ldd nm; do
  if ! command -v "$required_tool" >/dev/null 2>&1; then
    echo "[FAIL] required tool is unavailable: $required_tool" >&2
    exit 127
  fi
done
if [ ! -f "$source_dir/Build/Cmake/CMakeLists.txt" ] ||
   [ ! -f "$short_fixture" ] || [ ! -f "$control_fixture" ]; then
  echo "[FAIL] --source-dir is not a complete taint-trace checkout" >&2
  exit 2
fi
if [ ! -f "$runtime_dir/include/c++/v1/string" ] ||
   [ ! -f "$runtime_dir/lib/libc++.so.1" ] ||
   [ ! -f "$runtime_dir/lib/libc++abi.so.1" ]; then
  echo "[FAIL] --runtime-dir is not a complete libc++ runtime: $runtime_dir" >&2
  exit 2
fi

mkdir -p "$build_dir" "$out_dir"
for runtime_library in libc++.so.1 libc++abi.so.1; do
  nm -D "$runtime_dir/lib/$runtime_library" \
    > "$out_dir/$runtime_library.symbols"
  if ! grep -Fq '__msan_' "$out_dir/$runtime_library.symbols"; then
    echo "[FAIL] $runtime_library is not MemorySanitizer-instrumented" >&2
    exit 2
  fi
done

compile_flags="-fsanitize=memory -fsanitize-memory-track-origins"
compile_flags+=" -nostdinc++ -isystem $runtime_dir/include/c++/v1"
link_flags="-fsanitize=memory -fsanitize-memory-track-origins -nostdlib++"
link_flags+=" -L$runtime_dir/lib -Wl,-rpath,$runtime_dir/lib -lc++ -lc++abi"

CC=clang CXX=clang++ cmake -G Ninja \
  -S "$source_dir/Build/Cmake" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="$compile_flags" \
  -DCMAKE_EXE_LINKER_FLAGS="$link_flags" \
  -DCMAKE_SHARED_LINKER_FLAGS="$link_flags" \
  -DENABLE_TOOLS=ON \
  -DENABLE_TESTS=ON \
  -DENABLE_WXWIDGETS=OFF \
  -DENABLE_IMAGE_TOOLS=OFF \
  -DENABLE_MSAN=ON \
  -DICCDEV_ENABLE_TAINT_TRACE=ON \
  -DENABLE_SHARED_LIBS=ON \
  -DENABLE_STATIC_LIBS=OFF
cmake --build "$build_dir" --target iccFromJson iccConnectThreadTest \
  --parallel "${BUILD_JOBS:-$(nproc)}"

from_json="$build_dir/Tools/IccFromJson/iccFromJson"
thread_test="$build_dir/Testing/iccConnectThreadTest"
msan_options="halt_on_error=1:exit_code=86:origin_history_size=7"

for binary in "$from_json" "$thread_test"; do
  ldd "$binary" > "$out_dir/$(basename "$binary").ldd"
  if grep -Fq 'libstdc++.so' "$out_dir/$(basename "$binary").ldd" ||
     ! grep -Fq "$runtime_dir/lib/libc++.so.1" \
       "$out_dir/$(basename "$binary").ldd"; then
    echo "[FAIL] binary crossed an uninstrumented C++ runtime boundary: $binary" >&2
    cat "$out_dir/$(basename "$binary").ldd" >&2
    exit 2
  fi
done

run_from_json()
{
  local name="$1"
  local fixture="$2"
  local status=0

  MSAN_OPTIONS="$msan_options" ICC_TAINT_TRACE=1 \
    "$from_json" "$fixture" "$out_dir/$name.icc" \
    -noid >"$out_dir/$name.stdout" 2>"$out_dir/$name.stderr" || status=$?
  echo "$status"
}

short_status="$(run_from_json short "$short_fixture")"
control_status="$(run_from_json control "$control_fixture")"
thread_status=0
MSAN_OPTIONS="$msan_options" \
  "$thread_test" "$source_dir/Testing/sRGB_v4_ICC_preference.icc" \
  >"$out_dir/thread.stdout" 2>"$out_dir/thread.stderr" || thread_status=$?

if [ "$short_status" -ne 86 ] ||
   ! grep -Fq 'MemorySanitizer: use-of-uninitialized-value' "$out_dir/short.stderr" ||
   ! grep -Fq 'stage=curve.apply access=source-read' "$out_dir/short.stderr" ||
   ! grep -Fq 'state=poisoned first_bad=' "$out_dir/short.stderr" ||
   ! grep -Fq 'CIccTagParametricCurve::Apply' "$out_dir/short.stderr" ||
   ! grep -Fq 'CIccTagParametricCurve::SetFunctionType' "$out_dir/short.stderr"; then
  echo "[FAIL] short parameter case did not reach the application MSan finding" >&2
  sed -n '1,160p' "$out_dir/short.stderr" >&2
  exit 2
fi
if [ "$control_status" -ne 1 ] || grep -Fq 'MemorySanitizer:' "$out_dir/control.stderr"; then
  echo "[FAIL] complete parameter control was not MSan-clean" >&2
  sed -n '1,160p' "$out_dir/control.stderr" >&2
  exit 2
fi
if [ "$thread_status" -ne 0 ] || grep -Fq 'MemorySanitizer:' "$out_dir/thread.stderr"; then
  echo "[FAIL] threaded control was not MSan-clean" >&2
  sed -n '1,160p' "$out_dir/thread.stderr" >&2
  exit 2
fi

echo "[PASS] MSan reached CIccTagParametricCurve::Apply with allocation origin"
echo "[PASS] complete JSON and threaded controls are MSan-clean"
echo "[EVIDENCE] $out_dir"
