#!/bin/bash
###############################################################################
# iccDEV issue #1677 - iccApplyProfiles CIccPcsXform::pushBiRef2Rad replay
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1677}"
REPORT="${ICCDEV_REPRO_REPORT:-$OUTDIR/issue-1677-reproduction.md}"
EXPECT_SANITIZER="${ICCDEV_EXPECT_ISSUE_1677_ASAN:-1}"

SOURCE_TIFF_URL="https://raw.githubusercontent.com/xsscx/fuzz/master/graphics/icc/hbo-CIccPcsXform-pushBiRef2Rad-IccCmm_cpp-Line3597.icc"
SOURCE_TIFF_SHA256="29318205aea6770be4de61232a51e43eb58201d06b9a7d6f40d884b0e84ed3f2"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

APPLY="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
if [ ! -x "$APPLY" ]; then
  echo "[FAIL] missing executable: $APPLY" >&2
  exit 1
fi
if [ ! -x "$FROMXML" ]; then
  echo "[FAIL] missing executable: $FROMXML" >&2
  exit 1
fi

for required in curl sha256sum timeout; do
  if ! command -v "$required" >/dev/null 2>&1; then
    echo "[FAIL] missing required tool: $required" >&2
    exit 1
  fi
done

case "$EXPECT_SANITIZER" in
  0|1) ;;
  *)
    echo "[FAIL] ICCDEV_EXPECT_ISSUE_1677_ASAN must be 0 or 1" >&2
    exit 2
    ;;
esac

BUILD_DIR="$(cd "$TOOLS_DIR/.." && pwd)"
export LD_LIBRARY_PATH="$BUILD_DIR/IccProfLib:$BUILD_DIR/IccXML:$BUILD_DIR/IccConnect:${LD_LIBRARY_PATH:-}"
export DYLD_LIBRARY_PATH="$BUILD_DIR/IccProfLib:$BUILD_DIR/IccXML:$BUILD_DIR/IccConnect:${DYLD_LIBRARY_PATH:-}"

mkdir -p "$OUTDIR"
SOURCE_TIFF="$OUTDIR/hbo-CIccPcsXform-pushBiRef2Rad-IccCmm_cpp-Line3597.icc"
OUT_TIFF="$OUTDIR/iccapplyprofiles-pcsxform-replay.tif"
LOG="$OUTDIR/issue-1677-applyprofiles-pcsxform.log"
PCC="$OUTDIR/hybrid/ICC/Spec400_10_700-F11_2deg-Abs.icc"
PCC_XML="$REPO_ROOT/Testing/hybrid/Data/Spec400_10_700-F11_2deg-Abs.xml"
SRGB="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"

download_checked() {
  local url="$1"
  local dest="$2"
  local expected="$3"
  local actual

  rm -f "$dest"
  curl -fsSL --retry 3 --max-time 60 -o "$dest" "$url"
  actual="$(sha256sum "$dest" | awk '{print $1}')"
  if [ "$actual" != "$expected" ]; then
    echo "[FAIL] SHA-256 mismatch for $dest" >&2
    echo "       expected: $expected" >&2
    echo "       actual:   $actual" >&2
    exit 1
  fi
}

download_checked "$SOURCE_TIFF_URL" "$SOURCE_TIFF" "$SOURCE_TIFF_SHA256"

if [ ! -r "$PCC_XML" ]; then
  echo "[FAIL] missing PCC XML fixture: $PCC_XML" >&2
  exit 1
fi
if [ ! -r "$SRGB" ]; then
  echo "[FAIL] missing sRGB fixture: $SRGB" >&2
  exit 1
fi

rm -f "$OUT_TIFF" "$LOG"
mkdir -p "$(dirname "$PCC")"
"$FROMXML" "$PCC_XML" "$PCC" > "$OUTDIR/issue-1677-pcc-fromxml.log" 2>&1
if [ ! -r "$PCC" ]; then
  echo "[FAIL] unable to generate PCC fixture: $PCC" >&2
  sed -n '1,120p' "$OUTDIR/issue-1677-pcc-fromxml.log" >&2
  exit 1
fi

cmd=(
  "$APPLY"
  "$SOURCE_TIFF"
  "$OUT_TIFF"
  1
  1
  0
  1
  1
  -embedded
  10003
  -pcc
  "$PCC"
  "$SRGB"
  1
)

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:abort_on_error=0:exitcode=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"
set +e
timeout 60 "${cmd[@]}" > "$LOG" 2>&1
exit_code=$?
set -e

has_sanitizer=0
if grep -qE 'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|heap-buffer-overflow|CIccPcsXform::pushBiRef2Rad|IccCmm.cpp' "$LOG"; then
  has_sanitizer=1
fi

{
  echo "# Issue #1677 iccApplyProfiles reproduction"
  echo ""
  echo "- Issue: https://github.com/InternationalColorConsortium/iccDEV/issues/1677"
  echo "- Tool: \`$APPLY\`"
  echo "- PCC XML: \`Testing/hybrid/Data/Spec400_10_700-F11_2deg-Abs.xml\`"
  echo "- Source TIFF/embedded profile fixture SHA-256: \`$SOURCE_TIFF_SHA256\`"
  echo "- Expect sanitizer: \`$EXPECT_SANITIZER\`"
  echo "- Observed sanitizer: \`$has_sanitizer\`"
  echo "- Exit code: \`$exit_code\`"
  if [ "$exit_code" -eq 124 ]; then
    echo "- Timeout: \`1\`"
  else
    echo "- Timeout: \`0\`"
  fi
  echo ""
  echo "## Replay command"
  echo ""
  echo '```bash'
  printf '%q ' "${cmd[@]}"
  echo ""
  echo '```'
  echo ""
  echo "## Replay log excerpt"
  echo ""
  echo '```text'
  sed -n '1,160p' "$LOG"
  echo '```'
} > "$REPORT"

if [ "$EXPECT_SANITIZER" -eq 1 ]; then
  if [ "$has_sanitizer" -ne 1 ]; then
    echo "[FAIL] issue #1677 did not reproduce the expected ASan breadcrumb" >&2
    sed -n '1,120p' "$LOG" >&2
    exit 1
  fi
  if [ "$exit_code" -eq 124 ]; then
    echo "[PASS] issue #1677 reproduces the ASan heap-buffer-overflow report before timeout"
  else
    echo "[PASS] issue #1677 reproduces the ASan heap-buffer-overflow report"
  fi
else
  if [ "$exit_code" -eq 124 ]; then
    echo "[FAIL] issue #1677 replay timed out without sanitizer findings" >&2
    sed -n '1,120p' "$LOG" >&2
    exit 1
  fi
  if [ "$has_sanitizer" -ne 0 ]; then
    echo "[FAIL] issue #1677 replay still reports sanitizer findings" >&2
    sed -n '1,120p' "$LOG" >&2
    exit 1
  fi
  echo "[PASS] issue #1677 replay completed without sanitizer findings"
fi

echo "[INFO] Reproduction report: $REPORT"
