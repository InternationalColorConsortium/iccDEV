#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Validate in-tree AFL/CFL fuzz patch stacks with the applicator dry-run.
#
# The check runs from a fresh temporary clone of the current checkout so dirty
# worktrees and already-applied patches cannot hide malformed hunks or drift.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

reject_dirty_paths() {
  local path
  local relpath
  local dirty

  for path in "$@"; do
    case "$path" in
      "$REPO_ROOT"/*) relpath="${path#"$REPO_ROOT"/}" ;;
      /*)
        echo "ERROR: refusing to validate paths outside repo root: $path" >&2
        exit 1
        ;;
      *) relpath="$path" ;;
    esac

    dirty="$(git -C "$REPO_ROOT" status --porcelain --untracked-files=all -- "$relpath")"
    if [ -n "$dirty" ]; then
      echo "ERROR: refusing to validate dirty fuzz patch inputs:" >&2
      printf '%s\n' "$dirty" >&2
      echo "Commit or discard these changes before running the clone-backed check." >&2
      exit 1
    fi
  done
}

check_stack() {
  local label="$1"
  local patch_dir="$REPO_ROOT/.github/ci/fuzz-patches/$label"

  if ! compgen -G "$patch_dir/*.patch" >/dev/null; then
    echo "SKIP $label: no patches in $patch_dir"
    return 0
  fi

  reject_dirty_paths "$patch_dir" "$REPO_ROOT/.github/scripts/iccdev-apply-fuzz-patches.sh"

  (
    tmp="$(mktemp -d "/tmp/iccdev-${label}-patchcheck.XXXXXX")"
    trap 'test -n "${tmp:-}" && rm -rf "$tmp"' EXIT
    git clone --quiet "$REPO_ROOT" "$tmp"
    git -C "$tmp" reset --quiet --hard HEAD

    echo "CHECK $label patches"
    "$tmp/.github/scripts/iccdev-apply-fuzz-patches.sh" --mode "$label" --patch-dir "$tmp/.github/ci/fuzz-patches/$label" --dry-run --strict
  )
}

check_stack "afl"
check_stack "cfl"
