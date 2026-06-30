#!/bin/bash
###############################################################################
# iccApplyToLink NaN/SixChanInputRef sanitizer debug helper
###############################################################################
#
# Replays the CI debugging PoC:
#   iccApplyToLink bar.foo 0 2 1 foo.bar nan 1 1 0 SpecRef/SixChanInputRef.icc 13
#
# This helper replays the original master-equivalent iccApplyToLink crash path
# with added signature logging.  The NaN range must now be rejected before CMM
# setup or LUT/vector construction, with no sanitizer output.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR              -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR            -- path to Testing
#   ICCDEV_TEST_OUTDIR            -- output directory for logs and reports
#   ICCDEV_APPLYTOLINK_NAN_PROFILE -- optional alternate profile path
#   ICCDEV_APPLYTOLINK_FLAMEGRAPH -- set to 1 to collect perf/flamegraph data
#   ICCDEV_APPLYTOLINK_PROFILE_PROBE -- set to 1 to run a second finite-range probe
#
# Exit codes:
#   0 - expected graceful rejection captured or skipped on non-sanitizer build
#   2 - timeout, sanitizer finding, unexpected success, or wrong failure mode
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-$REPO_ROOT/Build/Testing/iccdev-applytolink-nan-sixchan-debug}"
RUN_FLAMEGRAPH="${ICCDEV_APPLYTOLINK_FLAMEGRAPH:-0}"
QA_MARKER_FILE="${ICCDEV_QA_TARGET_MARKER:-$REPO_ROOT/.github/ci/qa-flags/qa-target-marker.txt}"

if [ -f "$SCRIPT_DIR/sanitize-sed.sh" ]; then
  # shellcheck disable=SC1091
  source "$SCRIPT_DIR/sanitize-sed.sh"
fi

sanitize_for_summary()
{
  if declare -F sanitize_line >/dev/null 2>&1; then
    sanitize_line "$1"
  else
    printf "%s" "$1" | tr -d "\000-\011\013\014\016-\037\177"
  fi
}

git_value()
{
  git -C "$REPO_ROOT" "$@" 2>/dev/null || printf "unknown"
}

ci_value()
{
  local value="$1"
  if [ -n "$value" ]; then
    printf "%s" "$value"
  else
    printf "local"
  fi
}

write_replay_context()
{
  local utc branch commit short_commit describe run_url

  utc="$(date -u "+%Y-%m-%dT%H:%M:%SZ")"
  branch="$(git_value rev-parse --abbrev-ref HEAD)"
  commit="$(git_value rev-parse HEAD)"
  short_commit="$(git_value rev-parse --short HEAD)"
  describe="$(git_value describe --always --dirty --tags)"
  run_url="local"

  if [ -n "${GITHUB_SERVER_URL:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] && [ -n "${GITHUB_RUN_ID:-}" ]; then
    run_url="${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
  fi

  printf "ICC_DIAG: replay.utc=%s\n" "$utc"
  printf "ICC_DIAG: replay.script=%s\n" "$0"
  printf "ICC_DIAG: replay.summary=SixChanInputRef NaN range rejected before CMM/LUT generation\n"
  printf "ICC_DIAG: git.branch=%s commit=%s short=%s describe=%s\n" "$branch" "$commit" "$short_commit" "$describe"
  printf "ICC_DIAG: ci.github_actions=%s workflow=%s job=%s run_id=%s attempt=%s run_url=%s\n" \
    "$(ci_value "${GITHUB_ACTIONS:-}")" \
    "$(ci_value "${GITHUB_WORKFLOW:-}")" \
    "$(ci_value "${GITHUB_JOB:-}")" \
    "$(ci_value "${GITHUB_RUN_ID:-}")" \
    "$(ci_value "${GITHUB_RUN_ATTEMPT:-}")" \
    "$run_url"
  printf "ICC_DIAG: ci.repository=%s ref=%s ref_name=%s sha=%s actor=%s event=%s\n" \
    "$(ci_value "${GITHUB_REPOSITORY:-}")" \
    "$(ci_value "${GITHUB_REF:-}")" \
    "$(ci_value "${GITHUB_REF_NAME:-}")" \
    "$(ci_value "${GITHUB_SHA:-}")" \
    "$(ci_value "${GITHUB_ACTOR:-}")" \
    "$(ci_value "${GITHUB_EVENT_NAME:-}")"
  printf "ICC_DIAG: qa.controlled_pattern=0x41414141 reg16=0x4141,0x4141 bytes=0x41,0x41,0x41,0x41\n"
}

write_qa_marker_registers()
{
  if [ ! -f "$QA_MARKER_FILE" ]; then
    printf "qa_marker_file: missing (%s)\n" "$QA_MARKER_FILE"
    return 0
  fi

  python3 - "$QA_MARKER_FILE" <<'PY'
import sys
from pathlib import Path

marker_path = Path(sys.argv[1])
marker = marker_path.read_text(encoding="ascii").strip()
marker_bytes = marker.encode("ascii")
controlled = b"A" * 4

def hex_bytes(data):
    return ",".join(f"0x{byte:02x}" for byte in data)

def reg32(data):
    value = int.from_bytes(data[:4], "big")
    return f"0x{value:08x}"

print(f"qa_marker_file: {marker_path}")
print(f"qa_marker: {marker}")
print(f"qa_marker_prefix_reg32: {reg32(marker_bytes)}")
print(f"qa_marker_prefix_bytes: {hex_bytes(marker_bytes[:16])}")
print("qa_controlled_pattern: 0x41414141")
print("qa_controlled_pattern_reg16: 0x4141,0x4141")
print(f"qa_controlled_pattern_bytes: {hex_bytes(controlled)}")
PY
}

mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools" "$REPO_ROOT/build-dbg/Tools"; do
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

APPLY="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
PROFILE="${ICCDEV_APPLYTOLINK_NAN_PROFILE:-$TESTING_DIR/SpecRef/SixChanInputRef.icc}"
PROFILE_XML="$TESTING_DIR/SpecRef/SixChanInputRef.xml"
LOGFILE="$OUTDIR/iccApplyToLink-nan-sixchan.log"
PROFILE_LOG="$OUTDIR/profile-diagnostics.log"
REPORT="$OUTDIR/stack-analysis.txt"
CONTEXT_LOG="$OUTDIR/replay-context.txt"
OUT_PROFILE="$OUTDIR/bar.foo"
RUN_PROFILE_PROBE="${ICCDEV_APPLYTOLINK_PROFILE_PROBE:-0}"

echo "=== iccApplyToLink NaN/SixChanInputRef sanitizer debug ==="

if [ -z "$APPLY" ] || [ ! -x "$APPLY" ]; then
  echo "  [SKIP] iccApplyToLink not built under $TOOLS_DIR"
  exit 0
fi

cache=""
probe="$(dirname "$APPLY")"
for _ in 1 2 3 4 5; do
  if [ -f "$probe/CMakeCache.txt" ]; then
    cache="$probe/CMakeCache.txt"
    break
  fi
  probe="$(dirname "$probe")"
done

sanitized=0
if [ -n "$cache" ]; then
  if grep -qiE "ENABLE_ASAN:BOOL=ON|ENABLE_SANITIZERS:BOOL=ON" "$cache"; then
    sanitized=1
  elif grep -iE "CMAKE_(CXX|C)_FLAGS" "$cache" | grep -qiE "fsanitize=[^ ]*address"; then
    sanitized=1
  fi
fi

if [ "$sanitized" -ne 1 ]; then
  echo "  [SKIP] iccApplyToLink not built with AddressSanitizer (CMakeCache: ${cache:-none})"
  exit 0
fi

if [ ! -f "$PROFILE" ] && [ -n "$FROMXML" ] && [ -x "$FROMXML" ] && [ -f "$PROFILE_XML" ]; then
  PROFILE="$OUTDIR/SixChanInputRef.icc"
  ( cd "$TESTING_DIR/SpecRef" && "$FROMXML" "SixChanInputRef.xml" "$PROFILE" ) > "$OUTDIR/fromxml.log" 2>&1
fi

if [ ! -f "$PROFILE" ]; then
  echo "  [SKIP] PoC profile missing: $PROFILE"
  exit 0
fi

export ICC_APPLYTOLINK_DIAGNOSTICS="${ICC_APPLYTOLINK_DIAGNOSTICS:-1}"
export ICC_CMM_LEGACY_SPECTRAL_REPLAY="${ICC_CMM_LEGACY_SPECTRAL_REPLAY:-1}"
export ASAN_OPTIONS="${ICCDEV_APPLYTOLINK_ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:abort_on_error=0:detect_leaks=0:symbolize=1:print_stacktrace=1}"
export UBSAN_OPTIONS="${ICCDEV_APPLYTOLINK_UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"

rm -f "$LOGFILE" "$PROFILE_LOG" "$REPORT" "$CONTEXT_LOG" "$OUT_PROFILE" "$OUTDIR/profile-probe.foo"

cmd=("$APPLY" "$OUT_PROFILE" 0 2 1 foo.bar nan 1 1 0 "$PROFILE" 13)

printf "  [INFO] command:"
printf " %q" "${cmd[@]}"
printf "\n"

status=0
write_replay_context > "$CONTEXT_LOG"
cat "$CONTEXT_LOG" > "$LOGFILE"
timeout 30 "${cmd[@]}" >> "$LOGFILE" 2>&1 || status=$?

profile_status=0
if [ "$RUN_PROFILE_PROBE" = "1" ]; then
  ICC_APPLYTOLINK_DIAGNOSTICS=1 \
    timeout 30 "$APPLY" "$OUTDIR/profile-probe.foo" 0 2 1 foo.bar 0 1 1 0 "$PROFILE" 13 > "$PROFILE_LOG" 2>&1 || profile_status=$?
else
  printf "ICC_DIAG: profile_probe=skipped reason=crash-focused-replay\n" > "$PROFILE_LOG"
fi

has_asan=0
has_ubsan=0
if grep -qE "ERROR: AddressSanitizer|SUMMARY: AddressSanitizer" "$LOGFILE" "$PROFILE_LOG"; then
  has_asan=1
fi
if grep -q "runtime error:" "$LOGFILE" "$PROFILE_LOG"; then
  has_ubsan=1
fi

classification="UNKNOWN"
result="[FAIL]"
detail="expected graceful invalid-range rejection was not captured"
if [ "$has_asan" -eq 1 ] || [ "$has_ubsan" -eq 1 ]; then
  classification="SANITIZER"
  detail="unexpected sanitizer finding captured"
elif [ "$status" -eq 124 ]; then
  classification="TIMEOUT"
  detail="timeout after 30s"
elif [ "$status" -eq 134 ] || [ "$status" -eq 136 ] || [ "$status" -eq 137 ] || [ "$status" -eq 139 ]; then
  classification="SIGNAL"
  detail="signal exit $((status - 128)) without sanitizer report"
elif [ "$status" -eq 0 ]; then
  classification="UNEXPECTED_PASS"
  detail="PoC unexpectedly produced a link"
elif grep -q "Invalid input range" "$LOGFILE"; then
  classification="GRACEFUL_REJECT"
  result="[PASS]"
  detail="invalid NaN range rejected before sanitizer-sensitive paths"
elif [ "$status" -ge 1 ]; then
  classification="WRONG_FAILURE"
  detail="tool exited $status without expected invalid-range message"
fi

{
  printf "iccApplyToLink NaN/SixChanInputRef stack analysis\n"
  sed "s/^ICC_DIAG: //" "$CONTEXT_LOG"
  printf "classification: %s\n" "$classification"
  printf "exit_code: %s\n" "$status"
  printf "has_asan: %s\n" "$has_asan"
  printf "has_ubsan: %s\n" "$has_ubsan"
  printf "profile_probe_exit_code: %s\n" "$profile_status"
  printf "tool: %s\n" "$APPLY"
  printf "profile: %s\n" "$PROFILE"
  printf "asan_options: %s\n" "$ASAN_OPTIONS"
  printf "ubsan_options: %s\n" "$UBSAN_OPTIONS"
  printf "legacy_spectral_replay: %s\n" "$ICC_CMM_LEGACY_SPECTRAL_REPLAY"
  printf "profile_probe: %s\n" "$RUN_PROFILE_PROBE"
  printf "command:"
  printf " %q" "${cmd[@]}"
  printf "\n\n"
  printf "qa marker/register representation:\n"
  write_qa_marker_registers
  printf "\n"
  printf "diagnostics:\n"
  grep -E "^(ICC_DIAG: (range|profile|cmm\\.)|SCARINESS:|==[0-9]+==ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|.*runtime error:)" "$LOGFILE" || true
  printf "\nprofile probe diagnostics:\n"
  grep -E "^(ICC_DIAG|SCARINESS:|==[0-9]+==ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|.*runtime error:)" "$PROFILE_LOG" || true
  printf "\nstack frames:\n"
  grep -E "^[[:space:]]*#[0-9]+|Icc[A-Za-z0-9_./-]+\\.(cpp|h)" "$LOGFILE" "$PROFILE_LOG" | sed -n "1,80p" || true
  printf "\nfirst 40 log lines:\n"
  sed -n "1,40p" "$LOGFILE"
} > "$REPORT"

if [ "$RUN_FLAMEGRAPH" = "1" ] && [ -x "$SCRIPT_DIR/iccdev-tool-flamegraphs.sh" ]; then
  echo "  [INFO] collecting focused flamegraph data"
  ICCDEV_FLAMEGRAPH_ONLY=iccApplyToLink-nan-sixchan \
  ICCDEV_FLAMEGRAPH_APPLYTOLINK_NAN_POC=1 \
  ICCDEV_FLAMEGRAPH_OUTDIR="$OUTDIR/flamegraph" \
  ICCDEV_FLAMEGRAPH_REPEAT="${ICCDEV_FLAMEGRAPH_REPEAT:-3}" \
  ICCDEV_FLAMEGRAPH_TIMEOUT="${ICCDEV_FLAMEGRAPH_TIMEOUT:-30}" \
  ICCDEV_TOOLS_DIR="$TOOLS_DIR" \
  ICCDEV_TESTING_DIR="$TESTING_DIR" \
    "$SCRIPT_DIR/iccdev-tool-flamegraphs.sh" > "$OUTDIR/flamegraph.log" 2>&1 || true
fi

echo "  $result iccApplyToLink-nan-sixchan -- $detail (exit $status)"
echo "  [INFO] log: $LOGFILE"
echo "  [INFO] report: $REPORT"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    printf "### iccApplyToLink NaN/SixChanInputRef debug\n\n"
    printf "* Result: %s\n" "$(sanitize_for_summary "$result $detail")"
    printf "* Exit code: %s\n" "$(sanitize_for_summary "$status")"
    printf "* Classification: %s\n" "$(sanitize_for_summary "$classification")"
    printf "* UTC: %s\n" "$(sanitize_for_summary "$(sed -n 's/^ICC_DIAG: replay.utc=//p' "$CONTEXT_LOG")")"
    printf "* Git: %s\n" "$(sanitize_for_summary "$(sed -n 's/^ICC_DIAG: git.//p' "$CONTEXT_LOG")")"
    printf "* Run: %s\n" "$(sanitize_for_summary "$(sed -n 's/^ICC_DIAG: ci.github_actions=//p' "$CONTEXT_LOG")")"
    printf "* Report: %s\n" "$(sanitize_for_summary "$REPORT")"
    printf "* Log: %s\n" "$(sanitize_for_summary "$LOGFILE")"
  } >> "$GITHUB_STEP_SUMMARY"
fi

if [ "$result" = "[FAIL]" ]; then
  exit 2
fi
exit 0
