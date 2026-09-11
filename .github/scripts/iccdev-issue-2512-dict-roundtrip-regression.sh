#!/bin/bash
###############################################################################
# dictType XML/JSON round-trip coverage (issue #2512)
###############################################################################
#
# Before this test, no tracked XML fixture contained a DictEntry and no CTest
# named dict, so none of the dictType text code ran under test:
#
#   CIccTagXmlDict::ToXml     IccTagXml.cpp   -- name/value via icWCharToUtf8,
#                                                localized text via icUtf16ToUtf8
#   CIccTagXmlDict::ParseXml  IccTagXml.cpp
#   CIccTagJsonDict::ToJson   IccTagJson.cpp  -- the same two converters
#   CIccTagJsonDict::ParseJson IccTagJson.cpp
#
# The gap was measured rather than inferred (on a9daa06a): corrupting
# icWCharToUtf8's output length by one byte left all 264 tests green, while the
# same corruption in its sibling icUtf16ToUtf8 turned four red.
#
# The fixture, .github/ci/test-data/dict-entries-2512.xml, has one entry per
# record shape the binary format can hold; its header lists them.  The test
# builds a profile from it and then checks each direction separately:
#
#   1. The profile carries all seven entries and validates.  Without this, a
#      reader that dropped the tag would make every later comparison pass on
#      an empty dict.
#   2. ICC -> XML writes back exactly the DictEntry lines the fixture holds.
#   3. XML -> ICC reproduces the profile byte for byte.
#   4. ICC -> JSON writes each entry's fields with the right values, and keeps
#      "no value" apart from "empty value".
#   5. JSON -> ICC reproduces the profile byte for byte.
#
# Steps 2 and 4 check the text as well as the bytes, because a byte comparison
# alone passes when writer and reader are wrong in matching ways -- for
# example, both using the same misspelt key.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 1 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-2512-dict-roundtrip}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

find_tool() {
  find "$TOOLS_DIR" -maxdepth 2 -name "$1" -type f 2>/dev/null | head -1
}

TOXML="$(find_tool iccToXml)"
FROMXML="$(find_tool iccFromXml)"
TOJSON="$(find_tool iccToJson)"
FROMJSON="$(find_tool iccFromJson)"
DUMP="$(find_tool iccDumpProfile)"

fail() {
  echo "  [FAIL] issue-2512-dict-roundtrip -- $1"
  exit 1
}

echo "=== dictType XML/JSON round-trip coverage (issue #2512) ==="

for tool in "$TOXML" "$FROMXML" "$TOJSON" "$FROMJSON" "$DUMP"; do
  if [ -z "$tool" ] || [ ! -x "$tool" ]; then
    echo "  [SKIP] XML/JSON round-trip tools not all built under $TOOLS_DIR"
    exit 0
  fi
done

FIXTURE="$REPO_ROOT/.github/ci/test-data/dict-entries-2512.xml"
[ -f "$FIXTURE" ] || fail "missing fixture $FIXTURE"

# The fixture's entry count, for steps 1 and 2.  Those follow the fixture on
# their own; step 4 keeps its own copy of the entries and needs editing too.
EXPECTED_ENTRIES="$(grep -c '<DictEntry ' "$FIXTURE")"
[ "$EXPECTED_ENTRIES" -gt 0 ] || fail "the fixture has no DictEntry elements"

# ---------------------------------------------------------------------------
# run <label> <tool> <args...> -- run a tool, showing its log if it fails.
# ---------------------------------------------------------------------------
run() {
  local label="$1"; shift
  local log="$OUTDIR/$label.log"

  if ! "$@" > "$log" 2>&1; then
    sed -n '1,20p' "$log"
    fail "$label failed"
  fi
}

# ---------------------------------------------------------------------------
# same_bytes <reference> <candidate> <what> -- byte-exact comparison.
# ---------------------------------------------------------------------------
same_bytes() {
  if ! cmp -s "$1" "$2"; then
    cmp -l "$1" "$2" 2>/dev/null | sed -n '1,6p' | sed 's/^/    /'
    fail "$3 changed the bytes of the dict profile (#2512)"
  fi
}

# ---------------------------------------------------------------------------
# dict_lines <xml> -- the lines between <dictType> and </dictType>, with the
# indentation removed.  The writer indents differently from the fixture, and
# indentation carries no data here: every value is in an attribute or a CDATA
# section.
# ---------------------------------------------------------------------------
dict_lines() {
  sed -n '/<dictType>/,/<\/dictType>/p' "$1" \
    | sed -e 's/^[[:space:]]*//' -e '/<dictType>/d' -e '/<\/dictType>/d'
}

# ===========================================================================
# 1. Build the profile, and prove the dict is in it.
# ===========================================================================
run build "$FROMXML" "$FIXTURE" "$OUTDIR/dict.icc"

"$DUMP" -v "$OUTDIR/dict.icc" ALL > "$OUTDIR/dump.log" 2>&1
got="$(grep -c '^BEGIN DICT_ENTRY' "$OUTDIR/dump.log")"
if [ "$got" -ne "$EXPECTED_ENTRIES" ]; then
  sed -n '/BEGIN DICT_TAG/,/END DICT_TAG/p' "$OUTDIR/dump.log" | sed -n '1,20p' | sed 's/^/    /'
  fail "the built profile holds $got dict entries, the fixture has $EXPECTED_ENTRIES"
fi
# Ask for "valid" rather than only rejecting "NonCompliant": a critical error
# prints "Error! - ...", and a dump that crashed after listing the entries
# prints no verdict at all.  Either would pass a negative check.
if ! grep -Fq "Profile is valid" "$OUTDIR/dump.log"; then
  sed -n '/Validation Report/,$p' "$OUTDIR/dump.log" | sed -n '1,10p' | sed 's/^/    /'
  fail "the fixture profile does not validate"
fi
echo "    built: $got dict entries, profile validates"

# ===========================================================================
# 2 + 3. XML.
#
# The fixture is written in the writer's own form, so the DictEntry lines that
# come back out must equal the ones that went in.  This checks the writer
# directly: names and values through icWCharToUtf8 and icFixXml, the localized
# sub-elements through icUtf16ToUtf8, and whether Value is present at all.
# ===========================================================================
run toxml "$TOXML" "$OUTDIR/dict.icc" "$OUTDIR/dict.xml"

dict_lines "$FIXTURE"        > "$OUTDIR/dict-expected.txt"
dict_lines "$OUTDIR/dict.xml" > "$OUTDIR/dict-written.txt"
[ -s "$OUTDIR/dict-written.txt" ] || fail "iccToXml wrote no dictType element"
if ! diff -u "$OUTDIR/dict-expected.txt" "$OUTDIR/dict-written.txt" > "$OUTDIR/dict-xml.diff"; then
  sed -n '1,20p' "$OUTDIR/dict-xml.diff" | sed 's/^/    /'
  fail "iccToXml did not write back the fixture's DictEntry lines"
fi

run fromxml "$FROMXML" "$OUTDIR/dict.xml" "$OUTDIR/dict-rt-xml.icc"
same_bytes "$OUTDIR/dict.icc" "$OUTDIR/dict-rt-xml.icc" "an ICC -> XML -> ICC round trip"
echo "    XML: writer reproduces the fixture's entries; round trip is byte-exact"

# ===========================================================================
# 4 + 5. JSON.
#
# The expected entries are the fixture's, spelled out as JSON values.  Python
# compares whole entries, so a missing key, an extra key (a "value" on
# NameOnly) and a wrong value all fail, and key order does not matter.
# ===========================================================================
run tojson "$TOJSON" "$OUTDIR/dict.icc" "$OUTDIR/dict.json"

if command -v python3 >/dev/null 2>&1; then
  if ! python3 - "$OUTDIR/dict.json" > "$OUTDIR/dict-json-check.log" 2>&1 <<'PY'
import json
import sys


def loc(language, country, text):
    return {"language": language, "country": country, "text": text}


EXPECTED = [
    {"name": "ManufacturerName", "value": "iccDEV"},
    {"name": "NameOnly"},
    {"name": "EmptyValue", "value": ""},
    {"name": "Gr\u00fc\u00dfe", "value": "Caf\u00e9 \u8272"},
    {"name": "Escapes", "value": "a & b <c> \"q\" 's'"},
    {"name": "Localized", "value": "plain",
     "localizedNames": [loc("en", "US", "Localized"), loc("de", "DE", "Lokalisiert")],
     "localizedValues": [loc("en", "US", "plain")]},
    {"name": "ValueLocalizedOnly", "value": "v",
     "localizedValues": [loc("fr", "FR", "valeur")]},
]


def find_dict(node):
    if isinstance(node, dict):
        if node.get("type") == "dictType":
            return node
        for child in node.values():
            found = find_dict(child)
            if found is not None:
                return found
    elif isinstance(node, list):
        for child in node:
            found = find_dict(child)
            if found is not None:
                return found
    return None


with open(sys.argv[1], "r", encoding="utf-8") as handle:
    tag = find_dict(json.load(handle))

if tag is None:
    raise SystemExit("no dictType object in the JSON")

entries = tag.get("entries")
if entries != EXPECTED:
    print("expected:", json.dumps(EXPECTED, ensure_ascii=True, indent=1))
    print("written: ", json.dumps(entries, ensure_ascii=True, indent=1))
    if isinstance(entries, list) and len(entries) != len(EXPECTED):
        # EXPECTED is a second copy of the fixture, so a fixture that gained an
        # entry fails here too.  Say which it is likely to be.
        print(f"{len(entries)} entries written, {len(EXPECTED)} expected: if the "
              "fixture changed, update EXPECTED in this script to match")
    raise SystemExit("the JSON dict entries differ from the fixture")
PY
  then
    sed -n '1,40p' "$OUTDIR/dict-json-check.log" | sed 's/^/    /'
    fail "iccToJson did not write the fixture's dict entries"
  fi
  json_note=""
else
  json_note=" (python3 absent: JSON field check skipped)"
fi

run fromjson "$FROMJSON" "$OUTDIR/dict.json" "$OUTDIR/dict-rt-json.icc"
same_bytes "$OUTDIR/dict.icc" "$OUTDIR/dict-rt-json.icc" "an ICC -> JSON -> ICC round trip"
echo "    JSON: writer emits every entry's fields; round trip is byte-exact$json_note"

echo "  [PASS] issue-2512-dict-roundtrip -- all $EXPECTED_ENTRIES dict entries survive XML and JSON"
exit 0
