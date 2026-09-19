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
colorant_reject_fixture="$fixture_dir/json-colorant-table-nonnumeric-pcs.json"
colorant_control_fixture="$fixture_dir/json-colorant-table-complete-pcs.json"
xml_control_fixture="$source_dir/Testing/V2/v2GrayTRC.xml"
out_dir="${out_dir:-$build_dir/msan-taint-logs}"

for required_tool in clang clang++ cmake grep ldd ninja nm; do
  if ! command -v "$required_tool" >/dev/null 2>&1; then
    echo "[FAIL] required tool is unavailable: $required_tool" >&2
    exit 127
  fi
done
if [ ! -f "$source_dir/Build/Cmake/CMakeLists.txt" ] ||
   [ ! -f "$short_fixture" ] || [ ! -f "$control_fixture" ] ||
   [ ! -f "$colorant_reject_fixture" ] ||
   [ ! -f "$colorant_control_fixture" ] ||
   [ ! -f "$xml_control_fixture" ]; then
  echo "[FAIL] --source-dir is not a complete taint-trace checkout" >&2
  exit 2
fi
if [ ! -f "$runtime_dir/include/c++/v1/string" ] ||
   [ ! -f "$runtime_dir/lib/libc++.so.1" ] ||
   [ ! -f "$runtime_dir/lib/libc++abi.so.1" ] ||
   [ ! -f "$runtime_dir/include/libxml2/libxml/parser.h" ] ||
   [ ! -f "$runtime_dir/lib/libxml2.so" ]; then
  echo "[FAIL] --runtime-dir is not a complete MSan runtime: $runtime_dir" >&2
  exit 2
fi

mkdir -p "$build_dir" "$out_dir"
for runtime_library in libc++.so.1 libc++abi.so.1 libxml2.so; do
  nm -D "$runtime_dir/lib/$runtime_library" \
    > "$out_dir/$runtime_library.symbols"
  if ! grep -Fq '__msan_' "$out_dir/$runtime_library.symbols"; then
    echo "[FAIL] $runtime_library is not MemorySanitizer-instrumented" >&2
    exit 2
  fi
done

ICCDEV_MSAN_LIBCXX_DIR="$runtime_dir" cmake \
  --preset linux-clang-msan \
  -S "$source_dir/Build/Cmake" -B "$build_dir" \
  -DICCDEV_ENABLE_TAINT_TRACE=ON \
  -DENABLE_SHARED_LIBS=ON \
  -DENABLE_STATIC_LIBS=OFF
cmake --build "$build_dir" \
  --target iccFromJson iccFromXml iccConnectThreadTest iccTaintTraceMemoryStateProbe \
  --parallel "${BUILD_JOBS:-$(nproc)}"

from_json="$build_dir/Tools/IccFromJson/iccFromJson"
from_xml="$build_dir/Tools/IccFromXml/iccFromXml"
thread_test="$build_dir/Testing/iccConnectThreadTest"
memory_probe="$build_dir/Testing/iccTaintTraceMemoryStateProbe"
msan_options="halt_on_error=1:exit_code=86:origin_history_size=7"

for binary in "$from_json" "$from_xml" "$thread_test"; do
  ldd "$binary" > "$out_dir/$(basename "$binary").ldd"
  if grep -Fq 'libstdc++.so' "$out_dir/$(basename "$binary").ldd" ||
     ! grep -Fq "$runtime_dir/lib/libc++.so.1" \
       "$out_dir/$(basename "$binary").ldd"; then
    echo "[FAIL] binary crossed an uninstrumented C++ runtime boundary: $binary" >&2
    cat "$out_dir/$(basename "$binary").ldd" >&2
    exit 2
  fi
done
if ! grep -Fq "$runtime_dir/lib/libxml2.so" "$out_dir/iccFromXml.ldd"; then
  echo "[FAIL] iccFromXml crossed an uninstrumented libxml2 boundary" >&2
  cat "$out_dir/iccFromXml.ldd" >&2
  exit 2
fi

run_from_json()
{
  local name="$1"
  local fixture="$2"
  local status=0
  local profile="$out_dir/$name.icc"

  rm -f -- "$profile" "$out_dir/$name.stdout" "$out_dir/$name.stderr"

  MSAN_OPTIONS="$msan_options" ICC_TAINT_TRACE=1 \
    "$from_json" "$fixture" "$profile" \
    -noid >"$out_dir/$name.stdout" 2>"$out_dir/$name.stderr" || status=$?
  echo "$status"
}

# Every fixture below is fail-closed or clean, so none of them proves the tracer
# can see poison. The probe poisons bytes 5..15 of a written buffer itself;
# __msan_test_shadow reports without reading, so MSan stays silent and exit is 0.
probe_status=0
MSAN_OPTIONS="$msan_options" ICC_TAINT_TRACE=1 "$memory_probe" \
  >"$out_dir/memory-probe.stdout" 2>"$out_dir/memory-probe.stderr" || probe_status=$?
if [ "$probe_status" -ne 0 ] ||
   ! grep -Eq 'stage=probe\.initialized .* state=initialized$' "$out_dir/memory-probe.stderr" ||
   ! grep -Eq 'stage=probe\.poisoned .* state=poisoned first_bad=5$' "$out_dir/memory-probe.stderr" ||
   grep -Fq 'MemorySanitizer:' "$out_dir/memory-probe.stderr"; then
  echo "[FAIL] memory-state probe did not report initialized then poisoned at offset 5 (exit $probe_status)" >&2
  sed -n '1,80p' "$out_dir/memory-probe.stderr" >&2
  exit 2
fi

short_status="$(run_from_json short "$short_fixture")"
control_status="$(run_from_json control "$control_fixture")"
colorant_reject_status="$(run_from_json colorant-nonnumeric "$colorant_reject_fixture")"
colorant_control_status="$(run_from_json colorant-control "$colorant_control_fixture")"
thread_status=0
MSAN_OPTIONS="$msan_options" \
  "$thread_test" "$source_dir/Testing/sRGB_v4_ICC_preference.icc" \
  >"$out_dir/thread.stdout" 2>"$out_dir/thread.stderr" || thread_status=$?
xml_status=0
MSAN_OPTIONS="$msan_options" \
  "$from_xml" "$xml_control_fixture" "$out_dir/v2GrayTRC.icc" \
  >"$out_dir/xml.stdout" 2>"$out_dir/xml.stderr" || xml_status=$?

if [ "$short_status" -ne 1 ] || [ -e "$out_dir/short.icc" ] ||
   ! grep -Fq 'parametricCurveType params count does not match functionType' \
     "$out_dir/short.stderr" ||
   grep -Fq 'MemorySanitizer:' "$out_dir/short.stderr"; then
  echo "[FAIL] short parameter case was not rejected cleanly" >&2
  sed -n '1,160p' "$out_dir/short.stderr" >&2
  exit 2
fi
if [ "$control_status" -ne 1 ] || [ ! -s "$out_dir/control.icc" ] ||
   grep -Fq 'MemorySanitizer:' "$out_dir/control.stderr"; then
  echo "[FAIL] complete parameter control was not MSan-clean" >&2
  sed -n '1,160p' "$out_dir/control.stderr" >&2
  exit 2
fi
if [ "$colorant_reject_status" -ne 1 ] ||
   [ -e "$out_dir/colorant-nonnumeric.icc" ] ||
   ! grep -Fq 'colorantTableType pcs must contain three numeric values' \
     "$out_dir/colorant-nonnumeric.stderr" ||
   grep -Fq 'MemorySanitizer:' "$out_dir/colorant-nonnumeric.stderr"; then
  echo "[FAIL] non-numeric colorant PCS was not rejected cleanly" >&2
  sed -n '1,200p' "$out_dir/colorant-nonnumeric.stderr" >&2
  exit 2
fi
if [ "$colorant_control_status" -ne 1 ] ||
   [ ! -s "$out_dir/colorant-control.icc" ] ||
   ! grep -Fq 'stage=io.write16 access=source-read' \
     "$out_dir/colorant-control.stderr" ||
   ! grep -Fq 'state=initialized' "$out_dir/colorant-control.stderr" ||
   grep -Fq 'state=unknown' "$out_dir/colorant-control.stderr" ||
   grep -Fq 'MemorySanitizer:' "$out_dir/colorant-control.stderr"; then
  echo "[FAIL] complete colorant PCS control was not MSan-clean" >&2
  sed -n '1,160p' "$out_dir/colorant-control.stderr" >&2
  exit 2
fi
if [ "$thread_status" -ne 0 ] || grep -Fq 'MemorySanitizer:' "$out_dir/thread.stderr"; then
  echo "[FAIL] threaded control was not MSan-clean" >&2
  sed -n '1,160p' "$out_dir/thread.stderr" >&2
  exit 2
fi
if [ "$xml_status" -ne 0 ] || [ ! -s "$out_dir/v2GrayTRC.icc" ] ||
   grep -Fq 'MemorySanitizer:' "$out_dir/xml.stderr"; then
  echo "[FAIL] v2GrayTRC XML control was not MSan-clean" >&2
  sed -n '1,160p' "$out_dir/xml.stderr" >&2
  exit 2
fi

echo "[PASS] memory-state probe reported initialized, then poisoned at offset 5"
echo "[PASS] short parametric parameters were rejected before serialization"
echo "[PASS] non-numeric colorant PCS was rejected before serialization"
echo "[PASS] complete JSON, colorant PCS, and threaded controls are MSan-clean"
echo "[PASS] v2GrayTRC XML control is clean with instrumented libxml2"
echo "[EVIDENCE] $out_dir"
