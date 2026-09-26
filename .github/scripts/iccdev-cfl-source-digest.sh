#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Compute a deterministic digest for a clean ClusterFuzzLite source snapshot.
###############################################################################

set -euo pipefail

repo_root="${1:-.}"

if [ ! -d "$repo_root/.clusterfuzzlite" ]; then
  echo "ERROR: not an iccDEV ClusterFuzzLite source tree: $repo_root" >&2
  exit 2
fi

(
  cd "$repo_root"
  find . \
    -path './.git' -prune -o \
    -path './Build/AppleMobile' -prune -o \
    -path './docs/generated' -prune -o \
    -path './.clusterfuzzlite/known-bug-patch-mode' -prune -o \
    -path './.clusterfuzzlite/target-group' -prune -o \
    -type f -print0 |
    LC_ALL=C sort -z |
    while IFS= read -r -d '' path; do
      digest="$(sha256sum "$path" | cut -d' ' -f1)"
      printf '%s\0%s\0' "$path" "$digest"
    done |
    sha256sum |
    cut -d' ' -f1
)
