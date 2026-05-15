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

Installs optional iccDEV git hooks into .git/hooks:

- pre-commit: blocks CI, hook, and Dockerfile changes that fail preflight.
- pre-push: warns on risky branch, history, and maintainer-path pushes.

The installer is idempotent and refuses to overwrite existing different hooks
unless --force is provided.
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
HOOK_NAMES=(pre-commit pre-push)

if ! git -C "$REPO_ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  echo "[FAIL] Expected a git checkout" >&2
  exit 1
fi

HOOK_DIR="$(git -C "$REPO_ROOT" rev-parse --git-path hooks)"
mkdir -p "$HOOK_DIR"

for hook_name in "${HOOK_NAMES[@]}"; do
  hook_src="$REPO_ROOT/.githooks/$hook_name"
  hook_dst="$HOOK_DIR/$hook_name"

  if [ ! -f "$hook_src" ]; then
    echo "[FAIL] Hook source not found: $hook_src" >&2
    exit 1
  fi

  if [ -e "$hook_dst" ] && ! cmp -s "$hook_src" "$hook_dst"; then
    if [ "$FORCE" -ne 1 ]; then
      echo "[FAIL] Existing .git/hooks/$hook_name differs; rerun with --force to replace it" >&2
      exit 1
    fi
  fi

  cp "$hook_src" "$hook_dst"
  chmod 0755 "$hook_dst"
  echo "[OK] Installed optional $hook_name hook at $hook_dst"
done
