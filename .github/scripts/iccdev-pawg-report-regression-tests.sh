#!/bin/bash
###############################################################################
# iccPawgReport PAWG assessment regression and security tests
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to Testing
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-pawg-report-regressions}"
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
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"
export LLVM_PROFILE_FILE="${LLVM_PROFILE_FILE:-/dev/null}"

PAWG="$TOOLS_DIR/IccPawgReport/iccPawgReport"
GOOD_PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
TRUNCATED="$OUTDIR/truncated.icc"
MALWARE="$OUTDIR/private-pe-signature.icc"
GZIP_FALSE_POSITIVE="$OUTDIR/private-invalid-gzip-signature.icc"
GZIP_TRUE_POSITIVE="$OUTDIR/private-valid-gzip-signature.icc"
STD_GZIP_FALSE_POSITIVE="$OUTDIR/standard-tag-invalid-gzip-signature.icc"
STD_GZIP_TRUE_POSITIVE="$OUTDIR/standard-tag-valid-gzip-signature.icc"
STD_ELF_FALSE_POSITIVE="$OUTDIR/standard-tag-invalid-elf-signature.icc"
STD_ELF_TRUE_POSITIVE="$OUTDIR/standard-tag-valid-elf-signature.icc"
STD_SHEBANG_FALSE_POSITIVE="$OUTDIR/standard-tag-invalid-shebang-signature.icc"
STD_SHEBANG_TRUE_POSITIVE="$OUTDIR/standard-tag-valid-shebang-signature.icc"
STD_TEXTSIG_FALSE_POSITIVE="$OUTDIR/standard-tag-invalid-textsig-signature.icc"
STD_TEXTSIG_TRUE_POSITIVE="$OUTDIR/standard-tag-valid-textsig-signature.icc"
REGISTERED_PRIVATE="$OUTDIR/registered-private-tag.icc"

PASS=0
FAIL=0
TOTAL=0

fail_case() {
  local name="$1"
  local reason="$2"
  echo "  [FAIL] $name -- $reason"
  FAIL=$((FAIL + 1))
}

pass_case() {
  local name="$1"
  local reason="$2"
  echo "  [PASS] $name -- $reason"
  PASS=$((PASS + 1))
}

summary_value() {
  local logfile="$1"
  local key="$2"
  awk -F: -v key="$key" '
    $0 ~ "^  " key ":" {
      gsub(/[[:space:]]/, "", $2)
      print $2
      exit
    }
  ' "$logfile"
}

rendered_count() {
  local logfile="$1"
  local label="$2"
  grep -F -c "  [$label]" "$logfile" 2>/dev/null || true
}

total_item_count() {
  local logfile="$1"
  grep -E -c '^  \[(OK  |WARN|FAIL|N/A |GAP |--  )\] (S|C|Q)[0-9]+' "$logfile" 2>/dev/null || true
}

check_sanitizers() {
  local name="$1"
  local logfile="$2"

  if grep -qE "ERROR: AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL" "$logfile" 2>/dev/null; then
    echo "    sanitizer finding in $logfile"
    sed -n '1,80p' "$logfile"
    return 1
  fi

  return 0
}

assert_report_truth() {
  local name="$1"
  local logfile="$2"
  local rendered_total
  local summary_total
  local ok_count
  local warn_count
  local fail_count
  local na_count
  local gap_count
  local not_run_count
  local summary_sum

  rendered_total="$(total_item_count "$logfile")"
  summary_total="$(summary_value "$logfile" "Total checklist items")"
  ok_count="$(summary_value "$logfile" "PASS")"
  warn_count="$(summary_value "$logfile" "WARN")"
  fail_count="$(summary_value "$logfile" "FAIL")"
  na_count="$(summary_value "$logfile" "N/A")"
  gap_count="$(summary_value "$logfile" "GAP")"
  not_run_count="$(summary_value "$logfile" "NOT RUN")"

  if [ "$rendered_total" != "31" ] || [ "$summary_total" != "31" ]; then
    echo "    $name rendered $rendered_total item(s), summary reports $summary_total"
    return 1
  fi

  if [ "$ok_count" != "$(rendered_count "$logfile" "OK  ")" ]; then
    echo "    $name PASS summary does not match rendered OK items"
    return 1
  fi
  if [ "$warn_count" != "$(rendered_count "$logfile" "WARN")" ]; then
    echo "    $name WARN summary does not match rendered WARN items"
    return 1
  fi
  if [ "$fail_count" != "$(rendered_count "$logfile" "FAIL")" ]; then
    echo "    $name FAIL summary does not match rendered FAIL items"
    return 1
  fi
  if [ "$na_count" != "$(rendered_count "$logfile" "N/A ")" ]; then
    echo "    $name N/A summary does not match rendered N/A items"
    return 1
  fi
  if [ "$gap_count" != "$(rendered_count "$logfile" "GAP ")" ]; then
    echo "    $name GAP summary does not match rendered GAP items"
    return 1
  fi
  if [ "$not_run_count" != "$(rendered_count "$logfile" "--  ")" ]; then
    echo "    $name NOT RUN summary does not match rendered NOT RUN items"
    return 1
  fi

  summary_sum=$((ok_count + warn_count + fail_count + na_count + gap_count + not_run_count))
  if [ "$summary_sum" -ne 31 ]; then
    echo "    $name summary counts add to $summary_sum"
    return 1
  fi

  for section in "[ SECURITY ]" "[ CONFORMANCE ]" "[ QUALITY ]" "[ ASSESSMENT SUMMARY ]"; do
    if ! grep -F -q "$section" "$logfile"; then
      echo "    $name missing report section: $section"
      return 1
    fi
  done

  if ! grep -F -q "https://www.color.org/profiles/assessment/index.xalter" "$logfile"; then
    echo "    $name missing PAWG assessment URL"
    return 1
  fi

  return 0
}

run_static_source_audit() {
  local name="pawg-static-source-audit"
  local audit_log="$OUTDIR/static-source-audit.log"
  local banned_pattern='system[[:space:]]*\(|popen[[:space:]]*\(|fork[[:space:]]*\(|exec[lvpe]*[[:space:]]*\(|CreateProcess'

  TOTAL=$((TOTAL + 1))
  rm -f "$audit_log"

  if grep -RInE "$banned_pattern" "$REPO_ROOT/Tools/CmdLine/IccPawgReport" > "$audit_log" 2>&1; then
    fail_case "$name" "source contains process-spawning API usage"
    sed -n '1,40p' "$audit_log"
    return
  fi

  pass_case "$name" "no process-spawning APIs in PAWG tool source"
}

run_binary_size_guard() {
  local name="pawg-binary-size-guard"
  local size_bytes

  TOTAL=$((TOTAL + 1))

  if [ ! -x "$PAWG" ]; then
    fail_case "$name" "missing executable: $PAWG"
    return
  fi

  size_bytes="$(wc -c < "$PAWG" | tr -d '[:space:]')"
  if [ -z "$size_bytes" ] || [ "$size_bytes" -le 0 ]; then
    fail_case "$name" "unable to determine executable size"
    return
  fi

  if [ "$size_bytes" -gt 209715200 ]; then
    fail_case "$name" "executable is unexpectedly large: $size_bytes bytes"
    return
  fi

  pass_case "$name" "executable size is bounded: $size_bytes bytes"
}

run_good_profile() {
  local name="$1"
  local flag="$2"
  local logfile="$OUTDIR/$name.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile"

  if [ ! -x "$PAWG" ]; then
    fail_case "$name" "missing executable: $PAWG"
    return
  fi
  if [ ! -f "$GOOD_PROFILE" ]; then
    fail_case "$name" "missing test profile: $GOOD_PROFILE"
    return
  fi

  if [ -n "$flag" ]; then
    timeout 60 "$PAWG" "$flag" "$GOOD_PROFILE" > "$logfile" 2>&1 || exit_code=$?
  else
    timeout 60 "$PAWG" "$GOOD_PROFILE" > "$logfile" 2>&1 || exit_code=$?
  fi

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -E -q '^  \[(OK  |WARN|FAIL|N/A |GAP |--  )\] Q1[[:space:]]' "$logfile"; then
    fail_case "$name" "missing Q1 quality item"
    return
  fi
  if ! grep -F -q "Overall:" "$logfile"; then
    fail_case "$name" "missing overall assessment"
    return
  fi

  pass_case "$name" "31-item PAWG report is internally consistent"
}

generate_truncated_profile() {
  rm -f "$TRUNCATED"
  head -c 64 "$GOOD_PROFILE" > "$TRUNCATED"
}

run_truncated_profile() {
  local name="pawg-truncated-profile-dynamic-test"
  local logfile="$OUTDIR/truncated.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile"

  if ! generate_truncated_profile; then
    fail_case "$name" "failed to generate truncated profile"
    return
  fi

  timeout 60 "$PAWG" "$TRUNCATED" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "malformed input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "raw checks only; IccProfLib parse failed" "$logfile"; then
    fail_case "$name" "missing raw-check fallback disclosure"
    return
  fi

  pass_case "$name" "truncated input rejected without crash and with truthful report counts"
}

generate_private_pe_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$MALWARE" <<'PY'
import pathlib
import struct
import sys

dst = pathlib.Path(sys.argv[1])
size = 244
data = bytearray(size)
data[0:4] = struct.pack(">I", size)
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[64:68] = struct.pack(">I", 0)
data[68:72] = struct.pack(">I", 0x0000F6D6)
data[72:76] = struct.pack(">I", 0x00010000)
data[76:80] = struct.pack(">I", 0x0000D32D)
data[128:132] = struct.pack(">I", 1)
data[132:136] = b"zzzz"
data[136:140] = struct.pack(">I", 144)
data[140:144] = struct.pack(">I", 100)
payload = bytearray(100)
payload[0:2] = b"MZ"
payload[60:64] = struct.pack("<I", 64)
payload[64:68] = b"PE\0\0"
data[144:244] = payload
dst.write_bytes(data)
PY
}

run_private_malware_profile() {
  local name="pawg-private-malware-signature-dynamic-test"
  local logfile="$OUTDIR/private-malware.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$MALWARE"

  if ! generate_private_pe_signature_profile; then
    fail_case "$name" "failed to generate private PE signature profile"
    return
  fi

  timeout 60 "$PAWG" "$MALWARE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "malware-signature input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[FAIL] S8" "$logfile"; then
    fail_case "$name" "known malware signature was not reported in S8"
    return
  fi
  if ! grep -F -q "[WARN] S11" "$logfile"; then
    fail_case "$name" "private tag presence was not reported in S11"
    return
  fi

  pass_case "$name" "private malware signature detected without crash"
}

generate_private_invalid_gzip_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$GZIP_FALSE_POSITIVE" <<'PY'
import pathlib
import struct
import sys

dst = pathlib.Path(sys.argv[1])
size = 180
data = bytearray(size)
data[0:4] = struct.pack(">I", size)
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[64:68] = struct.pack(">I", 0)
data[68:72] = struct.pack(">I", 0x0000F6D6)
data[72:76] = struct.pack(">I", 0x00010000)
data[76:80] = struct.pack(">I", 0x0000D32D)
data[128:132] = struct.pack(">I", 1)
data[132:136] = b"zzzz"
data[136:140] = struct.pack(">I", 144)
data[140:144] = struct.pack(">I", 36)
payload = bytearray(36)
payload[1:11] = bytes([0x1f, 0x8b, 0x08, 0x86, 0x97, 0x0b, 0x75, 0x87, 0x7e, 0x87])
data[144:180] = payload
dst.write_bytes(data)
PY
}

run_invalid_gzip_signature_profile() {
  local name="pawg-invalid-gzip-signature-true-negative"
  local logfile="$OUTDIR/private-invalid-gzip.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$GZIP_FALSE_POSITIVE"

  if ! generate_private_invalid_gzip_signature_profile; then
    fail_case "$name" "failed to generate invalid gzip signature profile"
    return
  fi

  timeout 60 "$PAWG" "$GZIP_FALSE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[OK  ] S8" "$logfile"; then
    fail_case "$name" "invalid gzip header was incorrectly reported in S8"
    return
  fi
  if grep -F -q "embedded gzip stream" "$logfile"; then
    fail_case "$name" "invalid gzip header produced gzip detail text"
    return
  fi

  pass_case "$name" "invalid gzip-like bytes did not trigger S8"
}

generate_private_valid_gzip_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$GZIP_TRUE_POSITIVE" <<'PY'
import gzip
import pathlib
import struct
import sys

dst = pathlib.Path(sys.argv[1])
payload = gzip.compress(b"icc-pawg\n", mtime=0)
size = 144 + len(payload)
data = bytearray(size)
data[0:4] = struct.pack(">I", size)
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[64:68] = struct.pack(">I", 0)
data[68:72] = struct.pack(">I", 0x0000F6D6)
data[72:76] = struct.pack(">I", 0x00010000)
data[76:80] = struct.pack(">I", 0x0000D32D)
data[128:132] = struct.pack(">I", 1)
data[132:136] = b"zzzz"
data[136:140] = struct.pack(">I", 144)
data[140:144] = struct.pack(">I", len(payload))
data[144:size] = payload
dst.write_bytes(data)
PY
}

run_valid_gzip_signature_profile() {
  local name="pawg-valid-gzip-signature-true-positive"
  local logfile="$OUTDIR/private-valid-gzip.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$GZIP_TRUE_POSITIVE"

  if ! generate_private_valid_gzip_signature_profile; then
    fail_case "$name" "failed to generate valid gzip signature profile"
    return
  fi

  timeout 60 "$PAWG" "$GZIP_TRUE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "gzip-signature input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[FAIL] S8" "$logfile"; then
    fail_case "$name" "valid gzip signature was not reported in S8"
    return
  fi
  if ! grep -F -q "embedded gzip stream signature at offset" "$logfile"; then
    fail_case "$name" "gzip detail text missing expected offset"
    return
  fi
  if ! grep -F -q "[FAIL] S12" "$logfile"; then
    fail_case "$name" "private gzip signature was not reported in S12"
    return
  fi

  pass_case "$name" "valid gzip member still triggers S8 and S12"
}

generate_standard_tag_invalid_gzip_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$STD_GZIP_FALSE_POSITIVE" <<'PY'
import pathlib
import struct
import sys

dst = pathlib.Path(sys.argv[1])
# 256-byte high-entropy CLUT-like blob carried by a STANDARD 'A2B0' tag, with the
# exact Fogra51_CF false-positive bytes (1f 8b 08 then invalid FLG 0x86 ...)
# embedded in the middle -- mirrors a coincidental gzip magic inside lutAToBType
# CLUT data, as opposed to the private-tag false positive above.
blob = bytearray((i * 73 + 11) & 0xff for i in range(256))
blob[120:130] = bytes([0x1f, 0x8b, 0x08, 0x86, 0x97, 0x0b, 0x75, 0x87, 0x7e, 0x87])
size = 144 + len(blob)
data = bytearray(size)
data[0:4] = struct.pack(">I", size)
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[64:68] = struct.pack(">I", 0)
data[68:72] = struct.pack(">I", 0x0000F6D6)
data[72:76] = struct.pack(">I", 0x00010000)
data[76:80] = struct.pack(">I", 0x0000D32D)
data[128:132] = struct.pack(">I", 1)
data[132:136] = b"A2B0"
data[136:140] = struct.pack(">I", 144)
data[140:144] = struct.pack(">I", len(blob))
data[144:size] = blob
dst.write_bytes(data)
PY
}

run_standard_tag_invalid_gzip_signature_profile() {
  local name="pawg-standard-tag-invalid-gzip-true-negative"
  local logfile="$OUTDIR/standard-tag-invalid-gzip.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_GZIP_FALSE_POSITIVE"

  if ! generate_standard_tag_invalid_gzip_signature_profile; then
    fail_case "$name" "failed to generate standard-tag invalid gzip signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_GZIP_FALSE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[OK  ] S8" "$logfile"; then
    fail_case "$name" "invalid gzip header in standard tag was incorrectly reported in S8"
    return
  fi
  if grep -F -q "embedded gzip stream" "$logfile"; then
    fail_case "$name" "invalid gzip header produced gzip detail text"
    return
  fi
  if ! grep -F -q "[OK  ] S11" "$logfile"; then
    fail_case "$name" "standard tag was misreported as a private tag in S11"
    return
  fi

  pass_case "$name" "invalid gzip-like bytes in a standard CLUT tag did not trigger S8"
}

generate_standard_tag_valid_gzip_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$STD_GZIP_TRUE_POSITIVE" <<'PY'
import gzip
import pathlib
import struct
import sys

dst = pathlib.Path(sys.argv[1])
# Genuine gzip member embedded in the middle of a 256-byte CLUT-like blob carried
# by a STANDARD 'A2B0' tag. The whole-file S8 scan must still flag it, while the
# private-tag checks S11/S12 stay clear because no private tag is present -- this
# is the standard-tag counterpart to the private-tag true positive above.
member = gzip.compress(b"icc-pawg-clut\n", mtime=0)
blob = bytearray((i * 73 + 11) & 0xff for i in range(256))
blob[120:120 + len(member)] = member
size = 144 + len(blob)
data = bytearray(size)
data[0:4] = struct.pack(">I", size)
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[64:68] = struct.pack(">I", 0)
data[68:72] = struct.pack(">I", 0x0000F6D6)
data[72:76] = struct.pack(">I", 0x00010000)
data[76:80] = struct.pack(">I", 0x0000D32D)
data[128:132] = struct.pack(">I", 1)
data[132:136] = b"A2B0"
data[136:140] = struct.pack(">I", 144)
data[140:144] = struct.pack(">I", len(blob))
data[144:size] = blob
dst.write_bytes(data)
PY
}

run_standard_tag_valid_gzip_signature_profile() {
  local name="pawg-standard-tag-valid-gzip-true-positive"
  local logfile="$OUTDIR/standard-tag-valid-gzip.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_GZIP_TRUE_POSITIVE"

  if ! generate_standard_tag_valid_gzip_signature_profile; then
    fail_case "$name" "failed to generate standard-tag valid gzip signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_GZIP_TRUE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "gzip-signature input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[FAIL] S8" "$logfile"; then
    fail_case "$name" "valid gzip signature in standard tag was not reported in S8"
    return
  fi
  if ! grep -F -q "embedded gzip stream signature at offset" "$logfile"; then
    fail_case "$name" "gzip detail text missing expected offset"
    return
  fi
  if ! grep -F -q "[OK  ] S12" "$logfile"; then
    fail_case "$name" "standard-tag gzip must leave private-tag check S12 clear"
    return
  fi

  pass_case "$name" "valid gzip member in a standard CLUT tag triggers S8 while S12 stays clear"
}

# Shared builder: writes a minimal RGB profile carrying a single STANDARD 'A2B0'
# tag whose 256-byte CLUT-like blob has `embed` spliced in at offset 120.
emit_standard_tag_blob_profile() {
  python3 - "$1" "$2" <<'PY'
import pathlib
import sys

dst = pathlib.Path(sys.argv[1])
embed = bytes.fromhex(sys.argv[2])
blob = bytearray((i * 73 + 11) & 0xff for i in range(256))
blob[120:120 + len(embed)] = embed
size = 144 + len(blob)
data = bytearray(size)
data[0:4] = size.to_bytes(4, "big")
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[68:72] = (0x0000F6D6).to_bytes(4, "big")
data[72:76] = (0x00010000).to_bytes(4, "big")
data[76:80] = (0x0000D32D).to_bytes(4, "big")
data[128:132] = (1).to_bytes(4, "big")
data[132:136] = b"A2B0"
data[136:140] = (144).to_bytes(4, "big")
data[140:144] = len(blob).to_bytes(4, "big")
data[144:size] = blob
dst.write_bytes(data)
PY
}

generate_standard_tag_invalid_elf_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  # "\x7fELF" magic followed by an invalid e_ident (EI_CLASS/EI_VERSION zero) --
  # a coincidental 4-byte hit in CLUT data, not a real ELF object.
  emit_standard_tag_blob_profile "$STD_ELF_FALSE_POSITIVE" \
    "7f454c4600000000a1b2c3d4e5f60718293a4b5c"
}

run_standard_tag_invalid_elf_signature_profile() {
  local name="pawg-standard-tag-invalid-elf-true-negative"
  local logfile="$OUTDIR/standard-tag-invalid-elf.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_ELF_FALSE_POSITIVE"

  if ! generate_standard_tag_invalid_elf_signature_profile; then
    fail_case "$name" "failed to generate standard-tag invalid ELF signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_ELF_FALSE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[OK  ] S8" "$logfile"; then
    fail_case "$name" "invalid ELF ident in standard tag was incorrectly reported in S8"
    return
  fi
  if grep -F -q "ELF executable" "$logfile"; then
    fail_case "$name" "invalid ELF ident produced ELF detail text"
    return
  fi

  pass_case "$name" "ELF magic with invalid ident in a standard CLUT tag did not trigger S8"
}

generate_standard_tag_valid_elf_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  # Valid ELF identification: \x7fELF, ELFCLASS64, ELFDATA2LSB, EV_CURRENT,
  # e_type=ET_EXEC, e_machine=EM_X86_64, e_version=EV_CURRENT.
  emit_standard_tag_blob_profile "$STD_ELF_TRUE_POSITIVE" \
    "7f454c4602010100000000000000000002003e0001000000"
}

run_standard_tag_valid_elf_signature_profile() {
  local name="pawg-standard-tag-valid-elf-true-positive"
  local logfile="$OUTDIR/standard-tag-valid-elf.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_ELF_TRUE_POSITIVE"

  if ! generate_standard_tag_valid_elf_signature_profile; then
    fail_case "$name" "failed to generate standard-tag valid ELF signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_ELF_TRUE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "ELF-signature input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[FAIL] S8" "$logfile"; then
    fail_case "$name" "valid ELF signature in standard tag was not reported in S8"
    return
  fi
  if ! grep -F -q "ELF executable signature at offset" "$logfile"; then
    fail_case "$name" "ELF detail text missing expected offset"
    return
  fi
  if ! grep -F -q "[OK  ] S12" "$logfile"; then
    fail_case "$name" "standard-tag ELF must leave private-tag check S12 clear"
    return
  fi

  pass_case "$name" "valid ELF header in a standard CLUT tag triggers S8 while S12 stays clear"
}

generate_standard_tag_invalid_shebang_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  # "#!/" followed by binary (non-printable) bytes -- a coincidental hit in CLUT
  # data, not an interpreter line.
  emit_standard_tag_blob_profile "$STD_SHEBANG_FALSE_POSITIVE" \
    "23212f00ff01801090a0b0c0d0e0f000"
}

run_standard_tag_invalid_shebang_signature_profile() {
  local name="pawg-standard-tag-invalid-shebang-true-negative"
  local logfile="$OUTDIR/standard-tag-invalid-shebang.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_SHEBANG_FALSE_POSITIVE"

  if ! generate_standard_tag_invalid_shebang_signature_profile; then
    fail_case "$name" "failed to generate standard-tag invalid shebang signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_SHEBANG_FALSE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[OK  ] S8" "$logfile"; then
    fail_case "$name" "binary bytes after #!/ in standard tag were incorrectly reported in S8"
    return
  fi
  if grep -F -q "script shebang" "$logfile"; then
    fail_case "$name" "non-script #!/ produced shebang detail text"
    return
  fi

  pass_case "$name" "#!/ followed by binary in a standard CLUT tag did not trigger S8"
}

generate_standard_tag_valid_shebang_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  # A genuine interpreter line: #!/bin/sh\n
  emit_standard_tag_blob_profile "$STD_SHEBANG_TRUE_POSITIVE" \
    "23212f62696e2f73680a"
}

run_standard_tag_valid_shebang_signature_profile() {
  local name="pawg-standard-tag-valid-shebang-true-positive"
  local logfile="$OUTDIR/standard-tag-valid-shebang.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_SHEBANG_TRUE_POSITIVE"

  if ! generate_standard_tag_valid_shebang_signature_profile; then
    fail_case "$name" "failed to generate standard-tag valid shebang signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_SHEBANG_TRUE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "shebang-signature input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[FAIL] S8" "$logfile"; then
    fail_case "$name" "valid shebang in standard tag was not reported in S8"
    return
  fi
  if ! grep -F -q "script shebang signature at offset" "$logfile"; then
    fail_case "$name" "shebang detail text missing expected offset"
    return
  fi
  if ! grep -F -q "[OK  ] S12" "$logfile"; then
    fail_case "$name" "standard-tag shebang must leave private-tag check S12 clear"
    return
  fi

  pass_case "$name" "valid #!/ interpreter line in a standard CLUT tag triggers S8 while S12 stays clear"
}

generate_standard_tag_invalid_textsig_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  # Case-insensitive text signature "iex(" surrounded by binary CLUT bytes -- a
  # coincidental match with no printable context, as seen on SWOP2013C3_CRPC5.icc.
  emit_standard_tag_blob_profile "$STD_TEXTSIG_FALSE_POSITIVE" "69657828"
}

run_standard_tag_invalid_textsig_signature_profile() {
  local name="pawg-standard-tag-invalid-textsig-true-negative"
  local logfile="$OUTDIR/standard-tag-invalid-textsig.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_TEXTSIG_FALSE_POSITIVE"

  if ! generate_standard_tag_invalid_textsig_signature_profile; then
    fail_case "$name" "failed to generate standard-tag invalid text-signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_TEXTSIG_FALSE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[OK  ] S8" "$logfile"; then
    fail_case "$name" "text signature without printable context was incorrectly reported in S8"
    return
  fi
  if grep -F -q "PowerShell expression invocation" "$logfile"; then
    fail_case "$name" "binary-context text match produced text-signature detail"
    return
  fi

  pass_case "$name" "text signature with no printable context in a standard CLUT tag did not trigger S8"
}

generate_standard_tag_valid_textsig_signature_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi
  # A genuine embedded script blob (printable context on both sides):
  # <html><body><script>invoke-expression('calc.exe');</script></body></html>
  emit_standard_tag_blob_profile "$STD_TEXTSIG_TRUE_POSITIVE" \
    "3c68746d6c3e3c626f64793e3c7363726970743e696e766f6b652d65787072657373696f6e282763616c632e65786527293b3c2f7363726970743e3c2f626f64793e3c2f68746d6c3e"
}

run_standard_tag_valid_textsig_signature_profile() {
  local name="pawg-standard-tag-valid-textsig-true-positive"
  local logfile="$OUTDIR/standard-tag-valid-textsig.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$STD_TEXTSIG_TRUE_POSITIVE"

  if ! generate_standard_tag_valid_textsig_signature_profile; then
    fail_case "$name" "failed to generate standard-tag valid text-signature profile"
    return
  fi

  timeout 60 "$PAWG" "$STD_TEXTSIG_TRUE_POSITIVE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 0 ]; then
    fail_case "$name" "text-signature input unexpectedly returned success"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[FAIL] S8" "$logfile"; then
    fail_case "$name" "embedded script markup in standard tag was not reported in S8"
    return
  fi
  if ! grep -F -q "signature at offset" "$logfile"; then
    fail_case "$name" "text-signature detail text missing expected offset"
    return
  fi
  if ! grep -F -q "[OK  ] S12" "$logfile"; then
    fail_case "$name" "standard-tag text signature must leave private-tag check S12 clear"
    return
  fi

  pass_case "$name" "embedded script markup in a standard CLUT tag triggers S8 while S12 stays clear"
}

run_json_report() {
  local name="pawg-json-evidence-output"
  local logfile="$OUTDIR/json-report.log"
  local jsonfile="$OUTDIR/good-profile.json"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$jsonfile"

  timeout 60 "$PAWG" --json "$GOOD_PROFILE" > "$jsonfile" 2> "$logfile" || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! python3 - "$jsonfile" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    report = json.load(handle)

summary = report.get("summary", {})
items = report.get("items", [])
refs = report.get("references", [])
assert summary.get("total") == 31
assert len(items) == 31
assert items[0]["id"] == "S1"
assert items[-1]["id"] == "Q4"
assert any("registry.color.org/tag-signatures" in ref for ref in refs)
assert sum(summary[k] for k in ("pass", "warn", "fail", "notApplicable", "gap", "notRun")) == 31
PY
  then
    fail_case "$name" "JSON evidence shape is invalid"
    sed -n '1,80p' "$jsonfile"
    return
  fi

  pass_case "$name" "JSON evidence output is parseable and complete"
}

generate_registered_private_profile() {
  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - "$REGISTERED_PRIVATE" <<'PY'
import pathlib
import struct
import sys

dst = pathlib.Path(sys.argv[1])
size = 160
data = bytearray(size)
data[0:4] = struct.pack(">I", size)
data[8:12] = bytes.fromhex("04400000")
data[12:16] = b"mntr"
data[16:20] = b"RGB "
data[20:24] = b"XYZ "
data[36:40] = b"acsp"
data[68:72] = struct.pack(">I", 0x0000F6D6)
data[72:76] = struct.pack(">I", 0x00010000)
data[76:80] = struct.pack(">I", 0x0000D32D)
data[128:132] = struct.pack(">I", 1)
data[132:136] = b"arts"
data[136:140] = struct.pack(">I", 144)
data[140:144] = struct.pack(">I", 16)
data[144:148] = b"data"
dst.write_bytes(data)
PY
}

run_registered_private_profile() {
  local name="pawg-registered-private-tag-registry-test"
  local logfile="$OUTDIR/registered-private.log"
  local exit_code=0

  TOTAL=$((TOTAL + 1))
  rm -f "$logfile" "$REGISTERED_PRIVATE"

  if ! generate_registered_private_profile; then
    fail_case "$name" "failed to generate registered private tag profile"
    return
  fi

  timeout 60 "$PAWG" "$REGISTERED_PRIVATE" > "$logfile" 2>&1 || exit_code=$?

  if ! check_sanitizers "$name" "$logfile"; then
    fail_case "$name" "sanitizer finding"
    return
  fi
  if [ "$exit_code" -eq 124 ]; then
    fail_case "$name" "timed out"
    return
  fi
  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "crashed with signal $((exit_code - 128))"
    sed -n '1,80p' "$logfile"
    return
  fi
  if ! assert_report_truth "$name" "$logfile"; then
    fail_case "$name" "report count or section mismatch"
    return
  fi
  if ! grep -F -q "[WARN] S11" "$logfile"; then
    fail_case "$name" "private tag presence was not reported in S11"
    return
  fi
  if ! grep -F -q "[OK  ] C6" "$logfile"; then
    fail_case "$name" "registered private tag was not accepted by C6"
    return
  fi
  if ! grep -F -q "[OK  ] C7" "$logfile"; then
    fail_case "$name" "registered private tag documentation was not accepted by C7"
    return
  fi
  if ! grep -F -q "[OK  ] C8" "$logfile"; then
    fail_case "$name" "registered private tag was incorrectly marked undocumented"
    return
  fi
  if ! grep -F -q "'arts'=Graham Gill" "$logfile"; then
    fail_case "$name" "registered private tag owner evidence missing"
    return
  fi

  pass_case "$name" "registered private tag recognized from registry snapshot"
}

echo "=== iccPawgReport PAWG regression and security tests ==="
run_static_source_audit
run_binary_size_guard
run_good_profile "pawg-valid-profile-fidelity" ""
run_good_profile "pawg-read-option-fidelity" "--read"
run_json_report
run_truncated_profile
run_private_malware_profile
run_invalid_gzip_signature_profile
run_valid_gzip_signature_profile
run_standard_tag_invalid_gzip_signature_profile
run_standard_tag_valid_gzip_signature_profile
run_standard_tag_invalid_elf_signature_profile
run_standard_tag_valid_elf_signature_profile
run_standard_tag_invalid_shebang_signature_profile
run_standard_tag_valid_shebang_signature_profile
run_standard_tag_invalid_textsig_signature_profile
run_standard_tag_valid_textsig_signature_profile
run_registered_private_profile
echo "iccPawgReport PAWG regression and security tests: $PASS passed, $FAIL failed, $TOTAL total"

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi

exit 0
