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
from_json="$tools_dir/IccFromJson/iccFromJson"
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
if [ ! -f "$short_fixture" ] || [ ! -f "$control_fixture" ]; then
  echo "[FAIL] parametric curve fixtures are unavailable" >&2
  exit 2
fi

mkdir -p "$out_dir"

run_case()
{
  local name="$1"
  local fixture="$2"
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
  if [ "$name" = "short" ]; then
    if [ "$status" -ne 86 ]; then
      echo "[FAIL] short parameter case returned $status instead of 86: $log" >&2
      return 1
    fi
  elif [ "$status" -ne 1 ]; then
    echo "[FAIL] complete parameter control returned $status instead of 1: $log" >&2
    return 1
  fi
  if [ ! -s "$profile" ]; then
    echo "[FAIL] $name case did not reach profile serialization: $log" >&2
    return 1
  fi
}

if ! run_case short "$short_fixture"; then
  exit 2
fi
if ! grep -Fq 'stage=curve.apply access=source-read' "$out_dir/short.log" ||
   ! grep -Fq 'stage=writer.read access=read' "$out_dir/short.log" ||
   ! grep -Fq 'stage=writer.read access=read index=1 count=7' "$out_dir/short.log" ||
   ! grep -Fq 'stage=io.write32 access=source-read' "$out_dir/short.log" ||
   ! grep -Fq 'stage=file.write8 access=source-read' "$out_dir/short.log" ||
   ! grep -Fq 'state=poisoned first_bad=0' "$out_dir/short.log"; then
  echo "[FAIL] short parameter trace did not expose the expected poison chain" >&2
  sed -n '1,160p' "$out_dir/short.log" >&2
  exit 2
fi

if ! run_case control "$control_fixture"; then
  exit 2
fi
if grep -Fq 'state=poisoned' "$out_dir/control.log" ||
   grep -Fq 'Uninitialised byte(s)' "$out_dir/control.log" ||
   grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$out_dir/control.log"; then
  echo "[FAIL] complete parameter control reported poisoned memory" >&2
  sed -n '1,160p' "$out_dir/control.log" >&2
  exit 2
fi

echo "[PASS] short parameters traced parser-to-file poisoned reads"
echo "[PASS] complete parameter control serialized initialized values"
echo "[EVIDENCE] $out_dir"
