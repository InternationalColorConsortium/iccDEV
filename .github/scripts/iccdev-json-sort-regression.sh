#!/bin/bash
# Issue #1920 -- regression coverage for iccToJson -sort.
#
# -sort had no test of any kind, which is how a 2.5x-3.4x cost in its scalar
# path went unnoticed. The performance is not asserted here (timings are not a
# stable gate); what is asserted is every correctness property the sort pass
# has to preserve, so that a future rewrite of it cannot silently change data:
#
#   1. keys really are in ascending order, recursively        (the -sort contract)
#   2. sorted output carries the SAME data as unsorted output (no value corruption)
#   3. the same input twice gives byte-identical output        (determinism)
#   4. sorted output is still readable by iccFromJson          (round-trip)
#   5. all of the above hold at -indent=0, 2 and 4
#
# Property 2 is the load-bearing one. The sort pass has to copy every scalar
# from one nlohmann instantiation into another, and that copy is exactly where
# a float can lose precision or an integer can change signedness without any
# structural difference to show for it.
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -d "$SCRIPT_DIR/../../IccProfLib" ]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
elif [ -d "$SCRIPT_DIR/../../../IccProfLib" ]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
else
  REPO_ROOT="$(pwd)"
fi
cd "$REPO_ROOT" || exit 1

TOOLS_DIR="${ICCDEV_TOOLS_DIR:-build/Tools}"
BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1

TOJSON="$TOOLS_DIR/IccToJson/iccToJson"
FROMJSON="$TOOLS_DIR/IccFromJson/iccFromJson"

PASS=0
FAIL=0

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

ok()   { PASS=$((PASS+1)); echo "[PASS] $1"; }
bad()  { FAIL=$((FAIL+1)); echo "[FAIL] $1"; }

if [ ! -x "$TOJSON" ]; then
  echo "iccToJson not found at $TOJSON -- nothing to verify"
  exit 1
fi

# Profiles chosen for shape, not size: a CLUT-heavy CMYK profile (mixed integer
# and float scalars), a 100%-float display profile, and a smaller profile that
# puts proportionally more weight on the object path than the array path.
#
# The indent list is per profile on purpose. Indentation is handled by the final
# dump() and is orthogonal to document shape, so the full 0/2/4 sweep only needs
# the cheapest profile; the large ones would just buy the same coverage at many
# times the cost. Running the whole matrix on all three took 97s of the 300s
# timeout under ASAN on a developer machine, which is too little headroom for a
# loaded runner, and held 175MB of intermediates at once.
# Entries are <profile>|<comma separated indents>; commas rather than spaces so
# that each entry survives word splitting as a single token.
PROFILE_MATRIX="
Testing/SpecRef/srgbRef.icc|0,2,4
Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc|2
Testing/Display/LaserProjector.icc|2
"

# Recursively assert ascending key order, and compare sorted against unsorted
# for data equality. Python is already a hard dependency of the JSON test group.
check_json() {
  python3 - "$1" "$2" <<'PYEOF'
import json, sys

def keys_ordered(node, path="$"):
    if isinstance(node, dict):
        ks = list(node.keys())
        if ks != sorted(ks):
            for a, b in zip(ks, sorted(ks)):
                if a != b:
                    return "%s: keys not ascending (saw '%s', expected '%s')" % (path, a, b)
        for k, v in node.items():
            err = keys_ordered(v, "%s.%s" % (path, k))
            if err:
                return err
    elif isinstance(node, list):
        for i, v in enumerate(node):
            err = keys_ordered(v, "%s[%d]" % (path, i))
            if err:
                return err
    return None

sorted_path, plain_path = sys.argv[1], sys.argv[2]

with open(sorted_path) as f:
    s = json.load(f)
with open(plain_path) as f:
    p = json.load(f)

err = keys_ordered(s)
if err:
    print("ORDER " + err)
    sys.exit(1)

# dict == dict ignores insertion order, so this compares content alone. Floats
# must match exactly: both sides originate from the same double, so any
# difference here means the sort pass altered a value rather than moved it.
if s != p:
    print("DATA sorted and unsorted documents differ in content")
    sys.exit(1)

print("OK")
PYEOF
}

echo "=================================================================="
echo " iccToJson -sort regression (issue #1920)"
echo "=================================================================="

for entry in $PROFILE_MATRIX; do
  icc="${entry%%|*}"
  indents="$(echo "${entry#*|}" | tr ',' ' ')"
  if [ ! -f "$icc" ]; then
    echo "[SKIP] $icc not present"
    continue
  fi
  name="$(basename "$icc" .icc)"

  for indent in $indents; do
    s1="$WORK/$name.$indent.sorted.json"
    s2="$WORK/$name.$indent.sorted2.json"
    pl="$WORK/$name.$indent.plain.json"

    if ! "$TOJSON" "$icc" "$s1" -sort "-indent=$indent" >/dev/null 2>&1; then
      bad "$name indent=$indent: iccToJson -sort exited non-zero"
      continue
    fi
    if [ ! -s "$s1" ]; then
      bad "$name indent=$indent: -sort produced an empty file"
      continue
    fi
    if ! "$TOJSON" "$icc" "$pl" "-indent=$indent" >/dev/null 2>&1; then
      bad "$name indent=$indent: iccToJson without -sort exited non-zero"
      continue
    fi

    result="$(check_json "$s1" "$pl" 2>&1)"
    if [ "$result" = "OK" ]; then
      ok "$name indent=$indent: keys ascending, content identical to unsorted"
    else
      bad "$name indent=$indent: $result"
    fi

    # Determinism: a second run of the same input must be byte-identical.
    "$TOJSON" "$icc" "$s2" -sort "-indent=$indent" >/dev/null 2>&1
    if cmp -s "$s1" "$s2"; then
      ok "$name indent=$indent: -sort is deterministic"
    else
      bad "$name indent=$indent: -sort output differs between runs"
    fi

    rm -f "$s2" "$pl"

    # Round-trip the sorted document back to a profile, at the default indent.
    # Sorting moves the keys out of the writer's emission order, so this is the
    # check that the reader is not quietly depending on that order.
    if [ "$indent" = "2" ] && [ -x "$FROMJSON" ]; then
      if "$FROMJSON" "$s1" "$WORK/$name.roundtrip.icc" >/dev/null 2>&1; then
        ok "$name: sorted JSON round-trips through iccFromJson"
      else
        bad "$name: iccFromJson rejected the sorted document"
      fi
      rm -f "$WORK/$name.roundtrip.icc"
    fi

    # These documents run to tens of MB each. Release every intermediate as soon
    # as it has been checked rather than accumulating the whole matrix on disk.
    rm -f "$s1"
  done
done

echo "------------------------------------------------------------------"
echo "PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
if [ "$PASS" -eq 0 ]; then
  echo "no profiles were exercised"
  exit 1
fi
exit 0
