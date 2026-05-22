#!/usr/bin/env bash
###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Verify workflow YAML does not use GitHub Actions cache primitives.
#
###############################################################
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "Usage: check-workflow-cache-policy.sh WORKFLOW.yml [...]" >&2
  exit 2
fi

failures=0

for workflow in "$@"; do
  if [ ! -f "$workflow" ]; then
    echo "[FAIL] Workflow file not found: $workflow" >&2
    failures=$((failures + 1))
    continue
  fi

  while IFS= read -r match; do
    [ -n "$match" ] || continue
    echo "[FAIL] Cache primitive found in $workflow: $match" >&2
    failures=$((failures + 1))
  done < <(awk '
    /^[[:space:]]*run:[[:space:]]*\|/ {
      in_run=1
      match($0, /^[[:space:]]*/)
      run_indent=RLENGTH
      next
    }
    in_run {
      match($0, /^[[:space:]]*/)
      if ($0 !~ /^[[:space:]]*$/ && RLENGTH <= run_indent) {
        in_run=0
      } else {
        next
      }
    }
    /^[[:space:]]*uses:[[:space:]]*actions\/cache@/ ||
    /^[[:space:]]*cache-(from|to):[[:space:]]*type=gha/ {
      gsub(/^[[:space:]]+/, "")
      print
    }
  ' "$workflow")

  if [ "$failures" -eq 0 ]; then
    echo "[OK] Cache policy: $workflow"
  fi
done

if [ "$failures" -ne 0 ]; then
  exit 1
fi
