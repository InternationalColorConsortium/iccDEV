#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Regression tests for maintainer-local AFL/CFL fuzz patch validation.
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
tmp=""

cleanup() {
  if [ -n "$tmp" ]; then
    rm -rf "$tmp"
  fi
}
trap cleanup EXIT

tmp="$(mktemp -d /tmp/iccdev-fuzz-patch-tests.XXXXXX)"
patch_dir="$tmp/patches"
output="$tmp/strict.out"
dirty_repo="$tmp/dirty-repo"
dirty_output="$tmp/dirty.out"
nonblocking_repo="$tmp/nonblocking-repo"
nonblocking_patch_dir="$tmp/nonblocking-patches"
nonblocking_output="$tmp/nonblocking.out"
empty_patch_dir="$tmp/empty-patches"
empty_output="$tmp/empty.out"

mkdir -p "$patch_dir"

{
    printf '%s\n' 'diff --git a/README.md b/README.md'
    printf '%s\n' '--- a/README.md'
    printf '%s\n' '+++ b/README.md'
    printf '%s\n' '@@ -1 +1 @@'
    printf '%s\n' '-this line is intentionally absent from README'
    printf '%s\n' '+this replacement must not apply'
} > "$patch_dir/001-drifted.patch"

if "$REPO_ROOT/.github/scripts/iccdev-apply-fuzz-patches.sh" \
    --mode afl --patch-dir "$patch_dir" --dry-run --strict >"$output" 2>&1; then
    echo "FAIL: strict dry-run accepted a drifted patch" >&2
    cat "$output" >&2
    exit 1
fi

grep -q '\[ERROR\] dry-run: patch did not apply' "$output"
echo "PASS: strict dry-run rejects drifted patches"

git clone --quiet "$REPO_ROOT" "$nonblocking_repo"
git -C "$nonblocking_repo" reset --quiet --hard HEAD
cp "$REPO_ROOT/.github/scripts/iccdev-apply-fuzz-patches.sh" \
  "$nonblocking_repo/.github/scripts/iccdev-apply-fuzz-patches.sh"
mkdir -p "$nonblocking_patch_dir" "$empty_patch_dir"
printf '%s\n' 'before' > "$nonblocking_repo/nonblocking-target.txt"

{
    printf '%s\n' 'diff --git a/nonblocking-target.txt b/nonblocking-target.txt'
    printf '%s\n' '--- a/nonblocking-target.txt'
    printf '%s\n' '+++ b/nonblocking-target.txt'
    printf '%s\n' '@@ -1 +1 @@'
    printf '%s\n' '-missing'
    printf '%s\n' '+never-applied'
} > "$nonblocking_patch_dir/001-drifted.patch"

{
    printf '%s\n' 'diff --git a/nonblocking-target.txt b/nonblocking-target.txt'
    printf '%s\n' '--- a/nonblocking-target.txt'
    printf '%s\n' '+++ b/nonblocking-target.txt'
    printf '%s\n' '@@ -1 +1 @@'
    printf '%s\n' '-before'
    printf '%s\n' '+after'
} > "$nonblocking_patch_dir/002-applicable.patch"

"$nonblocking_repo/.github/scripts/iccdev-apply-fuzz-patches.sh" \
  --mode cfl --patch-dir "$nonblocking_patch_dir" >"$nonblocking_output" 2>&1
grep -q '\[WARN\] patch did not apply; skipping' "$nonblocking_output"
grep -qx 'after' "$nonblocking_repo/nonblocking-target.txt"
grep -q 'Patch summary: applied=1 integrated=0 skipped=1' "$nonblocking_output"
echo "PASS: non-strict application skips drift and continues to later patches"

"$nonblocking_repo/.github/scripts/iccdev-apply-fuzz-patches.sh" \
  --mode cfl --patch-dir "$empty_patch_dir" >"$empty_output" 2>&1
grep -q '\[WARN\] no cfl fuzz patches found; continuing' "$empty_output"
if "$nonblocking_repo/.github/scripts/iccdev-apply-fuzz-patches.sh" \
    --mode cfl --patch-dir "$empty_patch_dir" --strict >>"$empty_output" 2>&1; then
    echo "FAIL: strict application accepted an empty patch directory" >&2
    cat "$empty_output" >&2
    exit 1
fi
echo "PASS: empty stacks continue by default and fail in strict maintenance mode"

git clone --quiet "$REPO_ROOT" "$dirty_repo"
git -C "$dirty_repo" reset --quiet --hard HEAD
cp "$REPO_ROOT/.github/scripts/check-fuzz-patches.sh" "$dirty_repo/.github/scripts/check-fuzz-patches.sh"
cp "$REPO_ROOT/.github/scripts/iccdev-apply-fuzz-patches.sh" "$dirty_repo/.github/scripts/iccdev-apply-fuzz-patches.sh"
dirty_patch="$(find "$dirty_repo/.github/ci/fuzz-patches/cfl" -maxdepth 1 \
  -type f -name '*.patch' | sort | head -n 1)"
if [ -z "$dirty_patch" ]; then
  echo "FAIL: dirty-input test could not find a tracked CFL patch" >&2
  exit 1
fi
printf '%s\n' '# dirty local edit' >> "$dirty_patch"

if "$dirty_repo/.github/scripts/check-fuzz-patches.sh" >"$dirty_output" 2>&1; then
    echo "FAIL: clone-backed check accepted dirty fuzz patch inputs" >&2
    cat "$dirty_output" >&2
    exit 1
fi

grep -q 'refusing to validate dirty fuzz patch inputs' "$dirty_output"
echo "PASS: clone-backed check rejects dirty fuzz patch inputs"
