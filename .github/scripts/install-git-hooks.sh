#!/usr/bin/env bash
###############################################################
#
# Copyright (c) 2025-2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Install optional local iccDEV git hooks.
#
###############################################################
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: install-git-hooks.sh [--force]

Installs the optional iccDEV pre-push sanity guard into .git/hooks/pre-push.
The installer is idempotent and refuses to overwrite an existing different
hook unless --force is provided.
EOF
}

FORCE=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --force) FORCE=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) echo "[FAIL] Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOOK_SRC="$REPO_ROOT/.githooks/pre-push"

if ! git -C "$REPO_ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  echo "[FAIL] Expected a git checkout" >&2
  exit 1
fi

if [ ! -f "$HOOK_SRC" ]; then
  echo "[FAIL] Hook source not found: $HOOK_SRC" >&2
  exit 1
fi

HOOK_DIR="$(git -C "$REPO_ROOT" rev-parse --git-path hooks)"
HOOK_DST="$HOOK_DIR/pre-push"
mkdir -p "$HOOK_DIR"

if [ -e "$HOOK_DST" ] && ! cmp -s "$HOOK_SRC" "$HOOK_DST"; then
  if [ "$FORCE" -ne 1 ]; then
    echo "[FAIL] Existing .git/hooks/pre-push differs; rerun with --force to replace it" >&2
    exit 1
  fi
fi

cp "$HOOK_SRC" "$HOOK_DST"
chmod 0755 "$HOOK_DST"
echo "[OK] Installed optional pre-push hook at $HOOK_DST"
