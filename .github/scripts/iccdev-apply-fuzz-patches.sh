#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Apply maintainer-local AFL/CFL fuzzing patches to the current iccDEV checkout.
###############################################################################

set -euo pipefail

usage() {
  echo "Usage: $0 --mode afl|cfl [--patch-dir DIR] [--dry-run] [--strict]"
}

run_patch_check() {
  patch --batch --forward --dry-run --no-backup-if-mismatch -p1 -d "$repo_root" < "$1"
}

run_reverse_check() {
  patch --batch --reverse --dry-run --no-backup-if-mismatch -p1 -d "$repo_root" < "$1"
}

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
mode=""
patch_dir=""
dry_run=0
strict=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --mode)
      [ "$#" -ge 2 ] || { echo "ERROR: --mode requires afl or cfl" >&2; exit 2; }
      mode="$2"
      shift 2
      ;;
    --patch-dir)
      [ "$#" -ge 2 ] || { echo "ERROR: --patch-dir requires a directory" >&2; exit 2; }
      patch_dir="$2"
      shift 2
      ;;
    --dry-run)
      dry_run=1
      shift
      ;;
    --strict)
      strict=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "$mode" in
  afl|cfl)
    ;;
  *)
    echo "ERROR: --mode must be afl or cfl" >&2
    usage >&2
    exit 2
    ;;
esac

if [ -z "$patch_dir" ]; then
  patch_dir="$repo_root/.github/ci/fuzz-patches/$mode"
fi

if [ ! -d "$patch_dir" ]; then
  echo "ERROR: fuzz patch directory not present: $patch_dir" >&2
  exit 2
fi

patches=()
while IFS= read -r patch_file; do
  patches+=("$patch_file")
done < <(find "$patch_dir" -maxdepth 1 -type f -name '*.patch' | sort)
if [ "${#patches[@]}" -eq 0 ]; then
  echo "ERROR: no $mode fuzz patches found in $patch_dir" >&2
  exit 2
fi

for patch_file in "${patches[@]}"; do
  echo "Applying $mode fuzz patch: $(basename "$patch_file")"
  if [ "$dry_run" -eq 1 ]; then
    if run_patch_check "$patch_file" >/dev/null; then
      echo "  dry-run: applies cleanly"
    elif run_reverse_check "$patch_file" >/dev/null; then
      echo "  dry-run: already applied"
    else
      if [ "$strict" -eq 1 ]; then
        echo "  [ERROR] dry-run: patch did not apply"
        run_patch_check "$patch_file"
      fi
      echo "  [WARN] dry-run: patch did not apply; skipping"
      run_patch_check "$patch_file" || true
    fi
  else
    if run_patch_check "$patch_file" >/dev/null; then
      patch --batch --forward --no-backup-if-mismatch -p1 -d "$repo_root" < "$patch_file"
    elif run_reverse_check "$patch_file" >/dev/null; then
      echo "  already applied"
    else
      if [ "$strict" -eq 1 ]; then
        echo "  [ERROR] patch did not apply"
        run_patch_check "$patch_file"
      fi
      echo "  [WARN] patch did not apply; skipping"
      run_patch_check "$patch_file" || true
    fi
  fi
done
