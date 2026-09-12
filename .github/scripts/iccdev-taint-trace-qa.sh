#!/bin/bash
###############################################################################
# Validate runtime-gated tainted read/write diagnostics with Valgrind Memcheck.
###############################################################################

set -uo pipefail

usage()
{
  echo "Usage: $0 --source-dir DIR --tools-dir DIR [--out-dir DIR]"
}

source_dir=""
tools_dir=""
out_dir=""

while [ "$#" -gt 0 ]; do
  case "$1" in
    --source-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      source_dir="$2"
      shift 2
      ;;
    --tools-dir)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      tools_dir="$2"
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
missing_fixture="$fixture_dir/json-parametric-missing-params.json"
nonarray_fixture="$fixture_dir/json-parametric-nonarray-params.json"
long_fixture="$fixture_dir/json-parametric-long-params.json"
unknown_fixture="$fixture_dir/json-parametric-unknown-function.json"
colorant_finding_fixture="$fixture_dir/json-colorant-table-nonnumeric-pcs.json"
colorant_control_fixture="$fixture_dir/json-colorant-table-complete-pcs.json"
from_json="$tools_dir/IccFromJson/iccFromJson"
build_dir="$(dirname "$tools_dir")"
out_dir="${out_dir:-${TMPDIR:-/tmp}/iccdev-taint-trace-qa}"

for required_tool in valgrind grep; do
  if ! command -v "$required_tool" >/dev/null 2>&1; then
    echo "[FAIL] required tool is unavailable: $required_tool" >&2
    exit 127
  fi
done
if [ ! -x "$from_json" ]; then
  echo "[FAIL] iccFromJson is unavailable: $from_json" >&2
  exit 2
fi
if [ ! -f "$build_dir/CMakeCache.txt" ] ||
   ! grep -Fqx 'CMAKE_BUILD_TYPE:STRING=Debug' "$build_dir/CMakeCache.txt"; then
  echo "[FAIL] taint-trace Memcheck QA requires a Debug CMake build" >&2
  exit 2
fi
if [ ! -f "$short_fixture" ] || [ ! -f "$control_fixture" ] ||
   [ ! -f "$missing_fixture" ] || [ ! -f "$nonarray_fixture" ] ||
   [ ! -f "$long_fixture" ] || [ ! -f "$unknown_fixture" ] ||
   [ ! -f "$colorant_finding_fixture" ] ||
   [ ! -f "$colorant_control_fixture" ]; then
  echo "[FAIL] taint-trace fixtures are unavailable" >&2
  exit 2
fi

mkdir -p "$out_dir"

run_case()
{
  local name="$1"
  local fixture="$2"
  local expected_status="$3"
  local log="$out_dir/$name.log"
  local profile="$out_dir/$name.icc"
  local status=0

  rm -f -- "$log" "$profile"
  ICC_TAINT_TRACE=1 valgrind --quiet --tool=memcheck --track-origins=yes \
    --error-exitcode=86 --num-callers=20 \
    "$from_json" "$fixture" "$profile" -noid >"$log" 2>&1 || status=$?

  # These intentionally minimal profiles fail whole-profile validation after
  # serializing the tag, so iccFromJson returns 1 in both the finding and
  # control cases. Memcheck changes the finding case to the configured 86.
  if [ "$status" -ne "$expected_status" ]; then
    echo "[FAIL] $name case returned $status instead of $expected_status: $log" >&2
    return 1
  fi
  if [ ! -s "$profile" ]; then
    echo "[FAIL] $name case did not reach profile serialization: $log" >&2
    return 1
  fi
}

run_rejected_case()
{
  local name="$1"
  local fixture="$2"
  local expected_message="$3"
  local log="$out_dir/$name.log"
  local profile="$out_dir/$name.icc"
  local status=0

  rm -f -- "$log" "$profile"
  ICC_TAINT_TRACE=1 valgrind --quiet --tool=memcheck --track-origins=yes \
    --error-exitcode=86 --num-callers=20 \
    "$from_json" "$fixture" "$profile" -noid >"$log" 2>&1 || status=$?

  if [ "$status" -ne 1 ] || [ -e "$profile" ] ||
     ! grep -Fq "$expected_message" "$log" ||
     grep -Fq 'state=poisoned' "$log" ||
     grep -Fq 'Uninitialised byte(s)' "$log" ||
     grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$log"; then
    echo "[FAIL] $name parameter case was not rejected cleanly: $log" >&2
    sed -n '1,160p' "$log" >&2
    return 1
  fi
}

if ! run_rejected_case short "$short_fixture" \
       'parametricCurveType params count does not match functionType' ||
   ! run_rejected_case missing "$missing_fixture" \
       'parametricCurveType params must be an array' ||
   ! run_rejected_case nonarray "$nonarray_fixture" \
       'parametricCurveType params must be an array' ||
   ! run_rejected_case long "$long_fixture" \
       'parametricCurveType params count does not match functionType'; then
  exit 2
fi

if ! run_case control "$control_fixture" 1; then
  exit 2
fi
if grep -Fq 'state=poisoned' "$out_dir/control.log" ||
   grep -Fq 'Uninitialised byte(s)' "$out_dir/control.log" ||
   grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$out_dir/control.log"; then
  echo "[FAIL] complete parameter control reported poisoned memory" >&2
  sed -n '1,160p' "$out_dir/control.log" >&2
  exit 2
fi

if ! run_case unknown "$unknown_fixture" 1; then
  exit 2
fi
if grep -Fq 'state=poisoned' "$out_dir/unknown.log" ||
   grep -Fq 'Uninitialised byte(s)' "$out_dir/unknown.log"; then
  echo "[FAIL] unknown function type did not reach accepted serialization" >&2
  sed -n '1,160p' "$out_dir/unknown.log" >&2
  exit 2
fi

if ! run_case colorant-finding "$colorant_finding_fixture" 86 ||
   ! run_case colorant-control "$colorant_control_fixture" 1; then
  exit 2
fi
if ! grep -Fq 'Uninitialised value was created by a stack allocation' "$out_dir/colorant-finding.log" ||
   ! grep -Fq 'CIccTagJsonColorantTable::ParseJson' "$out_dir/colorant-finding.log" ||
   ! grep -Fq 'CIccTagColorantTable::Write' "$out_dir/colorant-finding.log" ||
   ! grep -Fq 'stage=io.write16 access=source-read' "$out_dir/colorant-finding.log" ||
   ! grep -Fq 'state=poisoned first_bad=' "$out_dir/colorant-finding.log" ||
   ! grep -Fq 'Syscall param write(buf) points to uninitialised byte(s)' "$out_dir/colorant-finding.log"; then
  echo "[FAIL] non-numeric colorant PCS did not expose the expected stack-to-file poison chain" >&2
  sed -n '1,220p' "$out_dir/colorant-finding.log" >&2
  exit 2
fi
if grep -Fq 'state=poisoned' "$out_dir/colorant-control.log" ||
   grep -Fq 'Uninitialised byte(s)' "$out_dir/colorant-control.log" ||
   grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$out_dir/colorant-control.log"; then
  echo "[FAIL] complete colorant PCS control reported poisoned memory" >&2
  sed -n '1,220p' "$out_dir/colorant-control.log" >&2
  exit 2
fi

echo "[PASS] short, missing, non-array, and long parameters were rejected cleanly"
echo "[PASS] complete parameter control serialized initialized values"
echo "[PASS] unknown function type was accepted and serialized"
echo "[PASS] non-numeric colorant PCS traced stack poison to the output file"
echo "[PASS] complete colorant PCS control serialized initialized values"
echo "[EVIDENCE] $out_dir"
