#!/usr/bin/env bash
# Download ICC registry profiles and run headless CLI QA scans.

# shellcheck disable=SC2016
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

URL_LIST="$REPO_ROOT/.github/ci/profile-registry/profile-source-list.txt"
OUTDIR="${ICCDEV_REGISTRY_QA_OUTDIR:-$HOME/work/copilot/iccdev-registry-qa-$(date -u +%Y%m%dT%H%M%SZ)}"
PROFILE_DIR=""
TIMEOUT_SECONDS="${ICCDEV_REGISTRY_QA_TIMEOUT:-20}"
MAX_PROFILES=0
FULL_MATRIX=0
SKIP_DOWNLOAD=0
LOG_TAIL_LINES="${ICCDEV_REGISTRY_QA_LOG_TAIL_LINES:-2000}"
FAIL_ON="${ICCDEV_REGISTRY_QA_FAIL_ON:-CRASH,TIMEOUT}"

usage() {
  cat <<'USAGE'
Usage: .github/scripts/iccdev-registry-profile-qa.sh [options]

Options:
  --url-list PATH      URL list to download, default: .github/ci/profile-registry/profile-source-list.txt
  --out-dir PATH       output root for downloads, logs, TSVs, and Markdown
  --profile-dir PATH   existing directory of downloaded files; implies --skip-download
  --timeout SEC        per-tool timeout, default: 20
  --max-profiles N     scan only first N .icc files after download
  --full-matrix        run every variant for each tool; default runs CI smoke variants
  --log-tail-lines N   bound archived per-run logs after classification; 0 keeps full logs
  --fail-on LIST       statuses that fail the script, default: CRASH,TIMEOUT
  --skip-download      use --profile-dir or <out-dir>/profiles without network
  -h, --help           show this help

Outputs:
  <out-dir>/download-manifest.tsv
  <out-dir>/summary.md
  <out-dir>/{pawg,dumpprofile,roundtrip}/results.tsv
  <out-dir>/specsep/{logs,tiffs}/
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --url-list) URL_LIST="$2"; shift 2 ;;
    --out-dir) OUTDIR="$2"; shift 2 ;;
    --profile-dir) PROFILE_DIR="$2"; SKIP_DOWNLOAD=1; shift 2 ;;
    --timeout) TIMEOUT_SECONDS="$2"; shift 2 ;;
    --max-profiles) MAX_PROFILES="$2"; shift 2 ;;
    --full-matrix) FULL_MATRIX=1; shift ;;
    --log-tail-lines) LOG_TAIL_LINES="$2"; shift 2 ;;
    --fail-on) FAIL_ON="$2"; shift 2 ;;
    --skip-download) SKIP_DOWNLOAD=1; shift ;;
    -h|--help) usage; exit 0 ;;
    -*) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    *) echo "ERROR: unexpected argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if ! [[ "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]] || [[ "$TIMEOUT_SECONDS" -lt 1 ]]; then
  echo "ERROR: --timeout must be a positive integer" >&2
  exit 2
fi

if ! [[ "$MAX_PROFILES" =~ ^[0-9]+$ ]]; then
  echo "ERROR: --max-profiles must be a non-negative integer" >&2
  exit 2
fi

if ! [[ "$LOG_TAIL_LINES" =~ ^[0-9]+$ ]]; then
  echo "ERROR: --log-tail-lines must be a non-negative integer" >&2
  exit 2
fi

if [[ ! -f "$URL_LIST" ]]; then
  echo "ERROR: URL list not found: $URL_LIST" >&2
  exit 2
fi

mkdir -p "$OUTDIR"
PROFILE_DIR="${PROFILE_DIR:-$OUTDIR/profiles}"
mkdir -p "$PROFILE_DIR"
MANIFEST="$OUTDIR/download-manifest.tsv"
SUMMARY="$OUTDIR/summary.md"

download_profiles() {
  local url base dest bytes sha status
  printf "url\tfile\tstatus\tbytes\tsha256\n" > "$MANIFEST"
  while IFS= read -r url; do
    [[ -n "$url" ]] || continue
    [[ "$url" != \#* ]] || continue
    base="${url##*/}"
    dest="$PROFILE_DIR/$base"
    status="ok"
    if ! curl -fsSL --retry 3 --connect-timeout 15 --max-time 120 "$url" -o "$dest"; then
      status="download-failed"
    fi
    if [[ -f "$dest" ]]; then
      bytes="$(wc -c < "$dest" | tr -d '[:space:]')"
      sha="$(sha256sum "$dest" | awk '{print $1}')"
    else
      bytes=0
      sha="-"
    fi
    printf "%s\t%s\t%s\t%s\t%s\n" "$url" "$base" "$status" "$bytes" "$sha" >> "$MANIFEST"
  done < "$URL_LIST"
}

if [[ "$SKIP_DOWNLOAD" -eq 0 ]]; then
  download_profiles
else
  printf "url\tfile\tstatus\tbytes\tsha256\n" > "$MANIFEST"
  find "$PROFILE_DIR" -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort |
    while IFS= read -r base; do
      bytes="$(wc -c < "$PROFILE_DIR/$base" | tr -d '[:space:]')"
      sha="$(sha256sum "$PROFILE_DIR/$base" | awk '{print $1}')"
      printf "local\t%s\tok\t%s\t%s\n" "$base" "$bytes" "$sha" >> "$MANIFEST"
    done
fi

ICC_COUNT="$(find "$PROFILE_DIR" -maxdepth 1 -type f -name '*.icc' | wc -l | tr -d '[:space:]')"
if [[ "$ICC_COUNT" -eq 0 ]]; then
  echo "ERROR: no .icc profiles available in $PROFILE_DIR" >&2
  exit 1
fi

run_scan() {
  local name="$1" script="$2" variant="$3"
  local -a args=()
  args+=(--timeout "$TIMEOUT_SECONDS" --out-dir "$OUTDIR/$name")
  args+=(--log-tail-lines "$LOG_TAIL_LINES")
  args+=(--fail-on "$FAIL_ON")
  if [[ "$MAX_PROFILES" -gt 0 ]]; then
    args+=(--max-files "$MAX_PROFILES")
  fi
  if [[ "$FULL_MATRIX" -eq 0 ]]; then
    args+=(--variant "$variant")
  fi
  "$script" "${args[@]}" "$PROFILE_DIR"
}

status=0
run_scan pawg "$SCRIPT_DIR/icc-pawg-qa-scan.sh" text || status=1
run_scan dumpprofile "$SCRIPT_DIR/icc-dumpprofile-qa-scan.sh" validate-all || status=1
run_scan roundtrip "$SCRIPT_DIR/icc-roundtrip-qa-scan.sh" intent-1 || status=1

specsep_args=(--profile-dir "$PROFILE_DIR")
if [[ "$MAX_PROFILES" -gt 0 ]]; then
  specsep_args+=(--max-profiles "$MAX_PROFILES")
fi
ICCDEV_TEST_TIMEOUT="$TIMEOUT_SECONDS" \
ICCDEV_TEST_OUTDIR="$OUTDIR/specsep" \
  bash "$SCRIPT_DIR/iccdev-specsep-profile-sweep.sh" "${specsep_args[@]}" || status=1

{
  printf '# ICC registry profile QA\n\n'
  printf '| Field | Value |\n'
  printf '|-------|-------|\n'
  printf '| URL list | `%s` |\n' "$URL_LIST"
  printf '| Profile directory | `%s` |\n' "$PROFILE_DIR"
  printf '| ICC profiles | %s |\n' "$ICC_COUNT"
  printf '| Max profiles | %s |\n' "$MAX_PROFILES"
  printf '| Timeout | %s seconds |\n' "$TIMEOUT_SECONDS"
  printf '| Log tail lines | %s |\n' "$LOG_TAIL_LINES"
  printf '| Full matrix | %s |\n' "$FULL_MATRIX"
  printf '| Fail-on | `%s` |\n' "$FAIL_ON"
  printf '| Manifest | `%s` |\n\n' "$MANIFEST"
  printf '## Tool summaries\n\n'
  for name in pawg dumpprofile roundtrip; do
    printf '### %s\n\n' "$name"
    if [[ -f "$OUTDIR/$name/summary.md" ]]; then
      sed -n '1,40p' "$OUTDIR/$name/summary.md"
      printf '\n'
    else
      printf 'No summary produced.\n\n'
    fi
  done
  printf '### specsep\n\n'
  printf 'The optional profile argument was swept with fixed eight-channel inputs.\n'
  printf 'Per-profile logs and accepted TIFFs are under `%s`.\n\n' "$OUTDIR/specsep"
} > "$SUMMARY"

printf '[SUMMARY] registry_profiles=%s\n' "$ICC_COUNT"
printf '[SUMMARY] manifest=%s\n' "$MANIFEST"
printf '[SUMMARY] markdown=%s\n' "$SUMMARY"
exit "$status"
