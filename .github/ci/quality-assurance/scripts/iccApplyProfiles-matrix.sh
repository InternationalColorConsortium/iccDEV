#!/usr/bin/env bash
################################################################################
# .github/ci/quality-assurance/scripts/iccApplyProfiles-matrix.sh
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
################################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ICCDEV_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
ICCDEV_BUILD_DIR="${ICCDEV_BUILD_DIR:-$ICCDEV_ROOT/Build}"
ICCDEV_TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$ICCDEV_BUILD_DIR/Tools}"
MANIFEST="$ICCDEV_ROOT/.github/ci/quality-assurance/manifests/iccApplyProfiles-smoke-matrix.json"
OUT_DIR="${ICCDEV_TEST_OUTDIR:-}"
TIMEOUT_SECONDS="${QA_TIMEOUT_SECONDS:-30}"

usage() {
  cat <<'EOF'
Usage: iccApplyProfiles-matrix.sh [--out-dir DIR] [--timeout SECONDS]

Run the tracked ApplyProfiles smoke matrix and retain JSONL results, semantic
TIFF evidence, tool receipts, exact commands, profile assessments, and logs.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir) OUT_DIR="$2"; shift 2 ;;
    --out-dir=*) OUT_DIR="${1#--out-dir=}"; shift ;;
    --timeout) TIMEOUT_SECONDS="$2"; shift 2 ;;
    --timeout=*) TIMEOUT_SECONDS="${1#--timeout=}"; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if ! [[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]]; then
  echo "error: --timeout must be a positive integer" >&2
  exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "error: python3 is required" >&2
  exit 2
fi

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$(mktemp -d /tmp/iccApplyProfiles-matrix.XXXXXX)"
elif [[ -d "$OUT_DIR" ]] && [[ -n "$(find "$OUT_DIR" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
  OUT_DIR="$(mktemp -d "$OUT_DIR/run.XXXXXX")"
  echo "Evidence directory already contained a prior run; using $OUT_DIR"
fi

APPLY_TOOL="$ICCDEV_TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"
TIFFDUMP_TOOL="$ICCDEV_TOOLS_DIR/IccTiffDump/iccTiffDump"
PAWG_TOOL="$ICCDEV_TOOLS_DIR/IccPawgReport/iccPawgReport"
for tool in "$APPLY_TOOL" "$TIFFDUMP_TOOL" "$PAWG_TOOL"; do
  if [[ ! -x "$tool" ]]; then
    echo "error: missing executable: $tool" >&2
    exit 2
  fi
done

exec python3 "$SCRIPT_DIR/icc_apply_profiles_matrix.py" \
  --repo-root "$ICCDEV_ROOT" \
  --manifest "$MANIFEST" \
  --out-dir "$OUT_DIR" \
  --apply-tool "$APPLY_TOOL" \
  --tiffdump-tool "$TIFFDUMP_TOOL" \
  --pawg-tool "$PAWG_TOOL" \
  --timeout "$TIMEOUT_SECONDS"
