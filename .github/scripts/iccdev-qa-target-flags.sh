#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

tools_dir="${ICCDEV_TOOLS_DIR:-${repo_root}/build/Tools}"
testing_dir="${ICCDEV_TESTING_DIR:-${repo_root}/Testing}"
out_dir="${ICCDEV_TEST_OUTDIR:-${repo_root}/build/Testing/ctest-output/iccdev-qa-target-flags}"
marker_file="${repo_root}/.github/ci/qa-flags/qa-target-marker.txt"
base_profile="${testing_dir}/sRGB_v4_ICC_preference.icc"
transform_input="${testing_dir}/Display/RgbTest.txt"
transform_profile="${base_profile}"
transform_output="${out_dir}/iccApplyNamedCmm.transform-output.txt"
tiff_input="${out_dir}/qa-write-embedded-profile.tif"
write_output_profile="${out_dir}/iccTiffDump.extracted.icc"
qa_profile="${out_dir}/qa-target-flags.icc"
negative_profile="${out_dir}/qa-target-flags-negative-control.icc"
malformed_profile="${out_dir}/qa-target-flags-malformed.icc"
evidence_ndjson="${out_dir}/evidence.ndjson"
summary_json="${out_dir}/summary.json"
summary_tsv="${out_dir}/summary.tsv"
classifier_sample="${repo_root}/.github/ci/qa-flags/sanitizer-stack-smash-sample.txt"
tools_root="$(dirname "$tools_dir")"
cmake_cache="${ICCDEV_CMAKE_CACHE:-${tools_root}/CMakeCache.txt}"

if [ -f "${repo_root}/.github/scripts/sanitize-sed.sh" ]; then
  # shellcheck disable=SC1091
  source "${repo_root}/.github/scripts/sanitize-sed.sh"
else
  sanitize_line() {
    printf '%s' "$1" | tr -d '\000-\011\013\014\016-\037\177'
  }
fi

fail() {
  printf '[FAIL] %s\n' "$(sanitize_line "$1")" >&2
  exit 1
}

find_tool() {
  local name="$1"
  local subdir="$2"
  local candidate="${tools_dir}/${subdir}/${name}"

  if [ -x "$candidate" ]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi

  fail "Unable to find ${name}; set ICCDEV_TOOLS_DIR or add tool directories to PATH"
}

zlib_toggle_enabled() {
  [ -f "$cmake_cache" ] && grep -q '^ICC_USE_ZLIB:BOOL=ON$' "$cmake_cache"
}

assert_runtime_zlib_dependency() {
  local tool_name="$1"
  local tool_path="$2"
  local ldd_out="${out_dir}/${tool_name}.ldd.txt"

  zlib_toggle_enabled || return 0

  if ! command -v ldd >/dev/null 2>&1; then
    printf '[SKIP] %s zlib runtime dependency check requires ldd\n' "$(sanitize_line "$tool_name")"
    return 0
  fi

  set +e
  ldd "$tool_path" >"$ldd_out" 2>&1
  local ldd_status
  ldd_status=$?
  set -e

  if [ "$ldd_status" -ne 0 ]; then
    printf '[SKIP] %s zlib runtime dependency check unavailable: ldd exit %s\n' \
      "$(sanitize_line "$tool_name")" "$ldd_status"
    return 0
  fi

  # The compression toggle links zlib (libz).  Do not key this check on
  # "lzma": liblzma is a transitive LibXml2 dependency for XML tools only.
  if grep -Eq 'libz(\.so|[^[:alpha:]])|zlib' "$ldd_out"; then
    printf '[OK] %s runtime links zlib when ICC_USE_ZLIB=ON\n' "$(sanitize_line "$tool_name")"
    return 0
  fi

  fail "${tool_name} does not show libz/zlib in ldd output while ICC_USE_ZLIB=ON"
}

require_file() {
  local path="$1"
  [ -f "$path" ] || fail "Required file not found: ${path}"
}

make_marker_profile() {
  local base="$1"
  local marker="$2"
  local output="$3"
  local tag_sig="$4"
  local nonce="${5:-}"

  python3 - "$base" "$marker" "$output" "$tag_sig" "$nonce" <<'PY'
import struct
import sys
from pathlib import Path

base = Path(sys.argv[1])
marker_path = Path(sys.argv[2])
output = Path(sys.argv[3])
tag_sig = sys.argv[4].encode("ascii")
nonce = sys.argv[5].encode("ascii")

if len(tag_sig) != 4:
    raise SystemExit("tag signature must be exactly 4 ASCII bytes")
if nonce and not nonce.startswith(b"ICCDEV_QA_NONCE_"):
    raise SystemExit("nonce must use ICCDEV_QA_NONCE_ prefix")

data = bytearray(base.read_bytes())
marker = marker_path.read_text(encoding="ascii").strip().encode("ascii")
controlled = b"A" * 16
payload = tag_sig + b"\x00\x00\x00\x00" + marker + b"\x00"
if nonce:
    payload += nonce + b"\x00"
payload += controlled
payload += b"\x00" * ((4 - (len(payload) % 4)) % 4)

if len(data) < 132:
    raise SystemExit("base profile too small")

tag_count = struct.unpack(">I", data[128:132])[0]
table_end = 132 + tag_count * 12
if table_end > len(data):
    raise SystemExit("base profile tag table extends beyond file")

shift = 12
new_data = bytearray(data[:132])
for i in range(tag_count):
    entry = data[132 + i * 12:132 + (i + 1) * 12]
    sig, offset, size = struct.unpack(">III", entry)
    if offset >= table_end:
        offset += shift
    new_data.extend(struct.pack(">III", sig, offset, size))

payload_offset = (len(data) + shift + 3) & ~3
new_data.extend(struct.pack(">4sII", tag_sig, payload_offset, len(payload)))
new_data.extend(data[table_end:])
while len(new_data) < payload_offset:
    new_data.append(0)
new_data.extend(payload)

struct.pack_into(">I", new_data, 0, len(new_data))
struct.pack_into(">I", new_data, 128, tag_count + 1)
output.write_bytes(new_data)
PY
}

make_malformed_profile() {
  local marker="$1"
  local output="$2"

  python3 - "$marker" "$output" <<'PY'
import sys
from pathlib import Path

marker_path = Path(sys.argv[1])
output = Path(sys.argv[2])
marker = marker_path.read_text(encoding="ascii").strip().encode("ascii")
payload = b"not-an-icc-profile\n" + marker + b"\n" + (b"A" * 16) + b"\n"
output.write_bytes(payload)
PY
}

make_tiff_with_embedded_profile() {
  local profile="$1"
  local output="$2"

  python3 - "$profile" "$output" <<'PY'
import struct
import sys
from pathlib import Path

profile = Path(sys.argv[1]).read_bytes()
output = Path(sys.argv[2])

entries = []

def entry(tag, typ, count, value=None, data=None):
    entries.append((tag, typ, count, value, data))

entry(256, 4, 1, 1)                                  # ImageWidth
entry(257, 4, 1, 1)                                  # ImageLength
entry(258, 3, 1, 8)                                  # BitsPerSample
entry(259, 3, 1, 1)                                  # Compression: none
entry(262, 3, 1, 1)                                  # Photometric: min-is-black
entry(273, 4, 1, "IMAGE")                            # StripOffsets
entry(277, 3, 1, 1)                                  # SamplesPerPixel
entry(278, 4, 1, 1)                                  # RowsPerStrip
entry(279, 4, 1, 1)                                  # StripByteCounts
entry(282, 5, 1, data=struct.pack("<II", 72, 1))     # XResolution
entry(283, 5, 1, data=struct.pack("<II", 72, 1))     # YResolution
entry(296, 3, 1, 2)                                  # ResolutionUnit: inch
entry(34675, 7, len(profile), data=profile)           # ICC profile tag

ifd_size = 2 + len(entries) * 12 + 4
current = 8 + ifd_size
records = []
for tag, typ, count, value, data in entries:
    if data is not None:
        offset = current
        current += len(data)
        if current % 2:
            current += 1
        records.append((tag, typ, count, offset, data))
    else:
        records.append((tag, typ, count, value, None))

image_offset = current
buf = bytearray()
buf.extend(b"II")
buf.extend(struct.pack("<H", 42))
buf.extend(struct.pack("<I", 8))
buf.extend(struct.pack("<H", len(records)))

for tag, typ, count, value, data in records:
    value = image_offset if value == "IMAGE" else value
    buf.extend(struct.pack("<HHI", tag, typ, count))
    if data is not None:
        buf.extend(struct.pack("<I", value))
    elif typ == 3 and count == 1:
        buf.extend(struct.pack("<H", value) + b"\0\0")
    else:
        buf.extend(struct.pack("<I", value))

buf.extend(struct.pack("<I", 0))
for tag, typ, count, value, data in records:
    if data is None:
        continue
    while len(buf) < value:
        buf.append(0)
    buf.extend(data)
    if len(buf) % 2:
        buf.append(0)

while len(buf) < image_offset:
    buf.append(0)
buf.append(0)
output.write_bytes(buf)
PY
}

run_evidence() {
  local name="$1"
  local expected_status="$2"
  local stdout_path="$3"
  local stderr_path="$4"
  shift 4

  set +e
  "$@" >"$stdout_path" 2>"$stderr_path"
  local status=$?
  set -e

  if [ "$status" -eq "$expected_status" ]; then
    printf '[OK] %s evidence command completed\n' "$(sanitize_line "$name")"
  else
    printf '[FAIL] %s evidence command exited %s, expected %s\n' \
      "$(sanitize_line "$name")" "$status" "$expected_status" >&2
    if [ -s "$stderr_path" ]; then
      while IFS= read -r line; do
        printf '%s\n' "$(sanitize_line "$line")" >&2
      done <"$stderr_path"
    fi
    # Not `exit "$status"`.  The negative controls expect a NONZERO status, so
    # exiting with the observed one turned the case they exist to catch -- the
    # tool starting to report success for an unloadable profile -- into
    # `exit 0`, and CTest read the whole run as PASS.
    exit 1
  fi

  python3 -m json.tool "$stdout_path" >/dev/null
  cat "$stdout_path" >>"$evidence_ndjson"
  printf '\n' >>"$evidence_ndjson"
}

run_command() {
  local name="$1"
  local expected_status="$2"
  local stdout_path="$3"
  local stderr_path="$4"
  shift 4

  set +e
  "$@" >"$stdout_path" 2>"$stderr_path"
  local status=$?
  set -e

  if [ "$status" -eq "$expected_status" ]; then
    printf '[OK] %s command completed\n' "$(sanitize_line "$name")"
  else
    printf '[FAIL] %s command exited %s, expected %s\n' \
      "$(sanitize_line "$name")" "$status" "$expected_status" >&2
    if [ -s "$stderr_path" ]; then
      while IFS= read -r line; do
        printf '%s\n' "$(sanitize_line "$line")" >&2
      done <"$stderr_path"
    fi
    # Not `exit "$status"`.  The negative controls expect a NONZERO status, so
    # exiting with the observed one turned the case they exist to catch -- the
    # tool starting to report success for an unloadable profile -- into
    # `exit 0`, and CTest read the whole run as PASS.
    exit 1
  fi
}

assert_json_flag() {
  local file="$1"
  local flag="$2"

  python3 - "$file" "$flag" <<'PY'
import json
import sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding="ascii"))
if sys.argv[2] not in data.get("qaFlags", []):
    raise SystemExit(f"missing flag {sys.argv[2]}")
PY
}

assert_json_no_flag() {
  local file="$1"
  local flag="$2"

  python3 - "$file" "$flag" <<'PY'
import json
import sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding="ascii"))
if sys.argv[2] in data.get("qaFlags", []):
    raise SystemExit(f"unexpected flag {sys.argv[2]}")
PY
}

assert_json_fields() {
  local file="$1"
  local flag="$2"
  shift 2

  python3 - "$file" "$flag" "$@" <<'PY'
import json
import sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding="ascii"))
flag = sys.argv[2]
if flag and flag not in data.get("qaFlags", []):
    raise SystemExit(f"missing flag {flag}")

def lookup(obj, path):
    cur = obj
    for part in path.split("."):
        if isinstance(cur, list):
            try:
                cur = cur[int(part)]
            except (ValueError, IndexError):
                raise KeyError(path)
        elif isinstance(cur, dict) and part in cur:
            cur = cur[part]
        else:
            raise KeyError(path)
    return cur

for path in sys.argv[3:]:
    value = lookup(data, path)
    if value is None or value == "" or value == []:
        raise SystemExit(f"empty evidence field {path}")
PY
}

assert_json_loaded() {
  local file="$1"
  local expected="$2"

  python3 - "$file" "$expected" <<'PY'
import json
import sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding="ascii"))
expected = sys.argv[2] == "true"
if data.get("loaded") is not expected:
    raise SystemExit(f"loaded={data.get('loaded')} expected {expected}")
PY
}

assert_json_equals() {
  local file="$1"
  local path="$2"
  local expected="$3"

  python3 - "$file" "$path" "$expected" <<'PY'
import json
import sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding="ascii"))
path = sys.argv[2]
expected = sys.argv[3]
cur = data
for part in path.split("."):
    if isinstance(cur, dict) and part in cur:
        cur = cur[part]
    else:
        raise SystemExit(f"missing evidence field {path}")
if cur != expected:
    raise SystemExit(f"{path}={cur!r} expected {expected!r}")
PY
}

assert_json_key_count() {
  local file="$1"
  local key="$2"
  local expected_count="$3"

  python3 - "$file" "$key" "$expected_count" <<'PY'
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="ascii")
key = '"' + sys.argv[2] + '"'
expected = int(sys.argv[3])
actual = text.count(key)
if actual != expected:
    raise SystemExit(f"{key} count {actual} != {expected}")
PY
}

write_case_summary() {
  local case_name="$1"
  local expected="$2"
  local observed="$3"
  local status="$4"
  local notes="$5"

  printf '%s\t%s\t%s\t%s\t%s\n' \
    "$case_name" "$expected" "$observed" "$status" "$notes" >>"$summary_tsv"
}

classify_sanitizer_sample() {
  local sample="$1"
  local output="$2"

  python3 - "$sample" "$output" <<'PY'
import json
import sys
from pathlib import Path

sample = Path(sys.argv[1])
output = Path(sys.argv[2])
text = sample.read_text(encoding="ascii")
flags = []
if "AddressSanitizer" in text:
    flags.append("ICCDEV_FLAG_SANITIZER")
if "stack smashing detected" in text or "__stack_chk_fail" in text or "stack-buffer-overflow" in text:
    flags.append("ICCDEV_FLAG_STACK_SMASH")
if "0x41414141" in text or "41414141" in text:
    flags.append("ICCDEV_FLAG_CONTROLLED_PATTERN")
summary = ""
stack_frame_file = ""
for line in text.splitlines():
    if line.startswith("SUMMARY:"):
        summary = line
    stripped = line.strip()
    if stripped.startswith("#") and " in " in stripped:
        candidate = stripped.rsplit(" ", 1)[-1]
        if ":" in candidate and "/" in candidate:
            stack_frame_file = candidate
evidence = {
    "schema": "iccdev-qa-log-evidence/v1",
    "tool": "iccdev-qa-log-classifier",
    "source": str(sample),
    "fixtureOnly": True,
    "exitCode": 134,
    "exitClassification": "sanitizer-abort",
    "qaFlags": flags,
    "sanitizer": {
        "summary": summary,
        "stackFrameFilePath": stack_frame_file,
    },
    "indicators": {
        "addressSanitizer": "AddressSanitizer" in text,
        "stackSmash": "stack smashing detected" in text or "__stack_chk_fail" in text,
        "stackBufferOverflow": "stack-buffer-overflow" in text,
        "controlledPattern": "0x41414141" in text or "41414141" in text,
    },
}
output.write_text(json.dumps(evidence, separators=(",", ":")) + "\n", encoding="ascii")
PY
}

require_file "$base_profile"
require_file "$transform_input"
require_file "$transform_profile"
require_file "$marker_file"
require_file "$classifier_sample"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"

mkdir -p "$out_dir"
: >"$evidence_ndjson"
printf 'case\texpected\tobserved\tstatus\tnotes\n' >"$summary_tsv"

icc_dump_profile="$(find_tool iccDumpProfile IccDumpProfile)"
icc_pawg_report="$(find_tool iccPawgReport IccPawgReport)"
icc_apply_named="$(find_tool iccApplyNamedCmm IccApplyNamedCmm)"
icc_tiff_dump="$(find_tool iccTiffDump IccTiffDump)"
icc_to_json="$(find_tool iccToJson IccToJson)"
icc_from_json="$(find_tool iccFromJson IccFromJson)"
icc_to_xml="$(find_tool iccToXml IccToXml)"
icc_from_xml="$(find_tool iccFromXml IccFromXml)"
icc_round_trip="$(find_tool iccRoundTrip IccRoundTrip)"

assert_runtime_zlib_dependency "iccDumpProfile" "$icc_dump_profile"
assert_runtime_zlib_dependency "iccPawgReport" "$icc_pawg_report"
assert_runtime_zlib_dependency "iccApplyNamedCmm" "$icc_apply_named"
assert_runtime_zlib_dependency "iccTiffDump" "$icc_tiff_dump"
assert_runtime_zlib_dependency "iccToJson" "$icc_to_json"
assert_runtime_zlib_dependency "iccFromJson" "$icc_from_json"
assert_runtime_zlib_dependency "iccToXml" "$icc_to_xml"
assert_runtime_zlib_dependency "iccFromXml" "$icc_from_xml"
assert_runtime_zlib_dependency "iccRoundTrip" "$icc_round_trip"

qa_nonce="ICCDEV_QA_NONCE_$(sha256sum "$base_profile" "$marker_file" | sha256sum | awk '{print toupper(substr($1, 1, 16))}')"
make_marker_profile "$base_profile" "$marker_file" "$qa_profile" "qaFL" "$qa_nonce"
make_marker_profile "$base_profile" "$marker_file" "$negative_profile" "fpFL"
make_malformed_profile "$marker_file" "$malformed_profile"
make_tiff_with_embedded_profile "$base_profile" "$tiff_input"
require_file "$tiff_input"

dump_json="${out_dir}/iccDumpProfile.evidence.json"
dump_err="${out_dir}/iccDumpProfile.stderr.txt"
negative_json="${out_dir}/iccDumpProfile.negative-control.evidence.json"
negative_err="${out_dir}/iccDumpProfile.negative-control.stderr.txt"
malformed_json="${out_dir}/iccDumpProfile.malformed.evidence.json"
malformed_err="${out_dir}/iccDumpProfile.malformed.stderr.txt"
pawg_json="${out_dir}/iccPawgReport.evidence.json"
pawg_err="${out_dir}/iccPawgReport.stderr.txt"
transform_json="${out_dir}/iccApplyNamedCmm.transform.evidence.json"
transform_config="${out_dir}/iccApplyNamedCmm.transform.config.json"
transform_err="${out_dir}/iccApplyNamedCmm.transform.stderr.txt"
write_json="${out_dir}/iccTiffDump.write.evidence.json"
write_err="${out_dir}/iccTiffDump.write.stderr.txt"
classifier_json="${out_dir}/sanitizer-stack-smash-classifier.evidence.json"

python3 - "$transform_input" "$transform_output" "$transform_profile" "$transform_config" <<'PY'
import json
import sys
from pathlib import Path

input_data = Path(sys.argv[1])
output_data = Path(sys.argv[2])
profile = Path(sys.argv[3])
config = Path(sys.argv[4])
payload = {
    "dataFiles": {
        "srcType": "legacy",
        "srcFile": str(input_data),
        "dstType": "legacy",
        "dstFile": str(output_data),
        "dstEncoding": "float",
    },
    "profileSequence": [
        {
            "iccFile": str(profile),
            "intent": "absolute",
            "interpolation": "linear",
        }
    ],
}
config.write_text(json.dumps(payload, separators=(",", ":")) + "\n", encoding="ascii")
PY

run_evidence \
  "iccDumpProfile" \
  0 \
  "$dump_json" \
  "$dump_err" \
  "$icc_dump_profile" --qa-flags --evidence-json --diag -v 100 "$qa_profile"

run_evidence \
  "iccDumpProfile negative control" \
  0 \
  "$negative_json" \
  "$negative_err" \
  "$icc_dump_profile" --qa-flags --evidence-json --diag -v 100 "$negative_profile"

run_evidence \
  "iccDumpProfile malformed control" \
  1 \
  "$malformed_json" \
  "$malformed_err" \
  "$icc_dump_profile" --qa-flags --evidence-json --diag -v 100 "$malformed_profile"

run_evidence \
  "iccPawgReport" \
  0 \
  "$pawg_json" \
  "$pawg_err" \
  "$icc_pawg_report" --qa-flags --evidence-json "$base_profile"

run_evidence \
  "iccApplyNamedCmm transform" \
  0 \
  "$transform_json" \
  "$transform_err" \
  "$icc_apply_named" --evidence-json -cfg "$transform_config"

run_evidence \
  "iccTiffDump write/extract" \
  0 \
  "$write_json" \
  "$write_err" \
  "$icc_tiff_dump" --evidence-json "$tiff_input" "$write_output_profile"

classify_sanitizer_sample "$classifier_sample" "$classifier_json"
python3 -m json.tool "$classifier_json" >/dev/null
cat "$classifier_json" >>"$evidence_ndjson"
printf '\n' >>"$evidence_ndjson"

grep -q '"schema":"iccdev-qa-evidence/v1"' "$dump_json" || fail "iccDumpProfile evidence schema missing"
assert_json_key_count "$dump_json" "schema" 1 || fail "iccDumpProfile evidence schema key duplicated"
assert_json_loaded "$dump_json" true || fail "iccDumpProfile did not load true-positive profile"
assert_json_flag "$dump_json" "ICCDEV_FLAG_LOAD" || fail "iccDumpProfile load flag missing"
assert_json_flag "$dump_json" "ICCDEV_FLAG_VALIDATE" || fail "iccDumpProfile validation flag missing"
assert_json_flag "$dump_json" "ICCDEV_FLAG_TAG_PAYLOAD" || fail "iccDumpProfile controlled tag payload flag missing"
assert_json_flag "$dump_json" "ICCDEV_FLAG_CONTROLLED_PATTERN" || fail "iccDumpProfile controlled 0x4141 pattern flag missing"
assert_json_fields "$dump_json" "ICCDEV_FLAG_LOAD" \
  "loadMode" "profileSize" "profileId"
assert_json_fields "$dump_json" "ICCDEV_FLAG_VALIDATE" \
  "validationStatus" "validationTags.0.signature" "validationTags.0.offset" "validationTags.0.size"
assert_json_fields "$dump_json" "ICCDEV_FLAG_TAG_PAYLOAD" \
  "qaPayload.tag" "qaPayload.offset" "qaPayload.size" "qaPayload.marker" "qaPayload.nonce" "qaPayload.controlledPattern"
assert_json_equals "$dump_json" "qaPayload.nonce" "$qa_nonce" || fail "iccDumpProfile nonce evidence mismatch"
write_case_summary \
  "true-positive qaFL" \
  "LOAD,VALIDATE,TAG_PAYLOAD,CONTROLLED_PATTERN" \
  "LOAD,VALIDATE,TAG_PAYLOAD,CONTROLLED_PATTERN" \
  "PASS" \
  "load mode/profile size/profile ID, validation tag table, private qaFL marker bytes, and generated nonce"

assert_json_loaded "$negative_json" true || fail "negative control did not load"
assert_json_flag "$negative_json" "ICCDEV_FLAG_LOAD" || fail "negative control load flag missing"
assert_json_flag "$negative_json" "ICCDEV_FLAG_VALIDATE" || fail "negative control validation flag missing"
assert_json_no_flag "$negative_json" "ICCDEV_FLAG_TAG_PAYLOAD" || fail "negative control incorrectly emitted tag payload flag"
assert_json_no_flag "$negative_json" "ICCDEV_FLAG_CONTROLLED_PATTERN" || fail "negative control incorrectly emitted controlled pattern flag"
write_case_summary \
  "negative-control fpFL" \
  "LOAD,VALIDATE only" \
  "LOAD,VALIDATE only" \
  "PASS" \
  "same marker bytes in non-qaFL tag do not trigger payload flags"

assert_json_loaded "$malformed_json" false || fail "malformed control unexpectedly loaded"
assert_json_no_flag "$malformed_json" "ICCDEV_FLAG_LOAD" || fail "malformed control incorrectly emitted load flag"
assert_json_no_flag "$malformed_json" "ICCDEV_FLAG_TAG_PAYLOAD" || fail "malformed control incorrectly emitted payload flag"
write_case_summary \
  "malformed marker file" \
  "loaded=false,no payload flags" \
  "loaded=false,no payload flags" \
  "PASS" \
  "marker bytes alone are not evidence without a loadable qaFL tag"

grep -q '"schema":"iccdev-qa-evidence/v1"' "$pawg_json" || fail "iccPawgReport evidence schema missing"
assert_json_loaded "$pawg_json" true || fail "iccPawgReport did not load base profile"
assert_json_flag "$pawg_json" "ICCDEV_FLAG_LOAD" || fail "iccPawgReport load flag missing"
assert_json_flag "$pawg_json" "ICCDEV_FLAG_VALIDATE" || fail "iccPawgReport validation flag missing"
write_case_summary \
  "pawg validation evidence" \
  "LOAD,VALIDATE" \
  "LOAD,VALIDATE" \
  "PASS" \
  "second CLI emits same schema for load and validation evidence"

assert_json_fields "$transform_json" "ICCDEV_FLAG_TRANSFORM" \
  "inputDigest" "profileId" "outputDigest" \
  "transform.inputDigest" "transform.profileId" "transform.outputDigest"
write_case_summary \
  "named CMM transform" \
  "TRANSFORM input digest, profile ID, output digest" \
  "TRANSFORM input digest, profile ID, output digest" \
  "PASS" \
  "iccApplyNamedCmm transformed checked-in RGB data through sRGB profile"

assert_json_fields "$write_json" "ICCDEV_FLAG_WRITE" \
  "outputDigest" "embeddedProfileId" "extractedProfileId" \
  "write.outputDigest" "write.embeddedProfileId" "write.extractedProfileId"
write_case_summary \
  "tiff embedded-profile extraction" \
  "WRITE output digest, embedded/extracted ICC profile ID" \
  "WRITE output digest, embedded/extracted ICC profile ID" \
  "PASS" \
  "iccTiffDump extracted the checked-in sRGB profile from a generated TIFF"

assert_json_flag "$classifier_json" "ICCDEV_FLAG_SANITIZER" || fail "sanitizer classifier flag missing"
assert_json_flag "$classifier_json" "ICCDEV_FLAG_STACK_SMASH" || fail "stack-smash classifier flag missing"
assert_json_flag "$classifier_json" "ICCDEV_FLAG_CONTROLLED_PATTERN" || fail "controlled-pattern classifier flag missing"
assert_json_fields "$classifier_json" "ICCDEV_FLAG_SANITIZER" \
  "exitClassification" "sanitizer.summary" "sanitizer.stackFrameFilePath"
write_case_summary \
  "sanitizer-log fixture" \
  "SANITIZER,STACK_SMASH,CONTROLLED_PATTERN" \
  "SANITIZER,STACK_SMASH,CONTROLLED_PATTERN" \
  "PASS" \
  "fixture-only sanitizer evidence includes exit classification, summary, and stack frame path"

python3 - "$qa_profile" "$negative_profile" "$malformed_profile" "$evidence_ndjson" "$summary_tsv" "$summary_json" <<'PY'
import json
import sys
from pathlib import Path

summary = {
    "schema": "iccdev-qa-target-flags-summary/v1",
    "status": "ok",
    "profile": sys.argv[1],
    "negativeControlProfile": sys.argv[2],
    "malformedControlProfile": sys.argv[3],
    "evidence": sys.argv[4],
    "summaryTsv": sys.argv[5],
}
Path(sys.argv[6]).write_text(json.dumps(summary, separators=(",", ":")) + "\n", encoding="ascii")
PY

printf '[OK] QA target flags evidence written to %s\n' "$(sanitize_line "$evidence_ndjson")"
