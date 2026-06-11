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
  echo "Usage: check-workflow-cache-policy.sh WORKFLOW.yml|WORKFLOW_DIR [...]" >&2
  exit 2
fi

failures=0
workflow_files=()

for item in "$@"; do
  if [ -d "$item" ]; then
    while IFS= read -r workflow; do
      workflow_files+=("$workflow")
    done < <(find "$item" -maxdepth 1 -type f \( -name '*.yml' -o -name '*.yaml' \) | sort)
  else
    workflow_files+=("$item")
  fi
done

if [ "${#workflow_files[@]}" -eq 0 ]; then
  echo "[FAIL] No workflow files found" >&2
  exit 1
fi

for workflow in "${workflow_files[@]}"; do
  workflow_failures=0
  if [ ! -f "$workflow" ]; then
    echo "[FAIL] Workflow file not found: $workflow" >&2
    failures=$((failures + 1))
    continue
  fi

  while IFS= read -r match; do
    [ -n "$match" ] || continue
    echo "[FAIL] Cache primitive found in $workflow: $match" >&2
    workflow_failures=$((workflow_failures + 1))
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
    /^[[:space:]]*-?[[:space:]]*uses:[[:space:]]*actions\/cache@/ ||
    /^[[:space:]]*cache-(from|to):[[:space:]]*type=gha/ ||
    /^[[:space:]]*cache-dependency-path:[[:space:]]*/ ||
    (/^[[:space:]]*cache:[[:space:]]*/ && $0 !~ /^[[:space:]]*cache:[[:space:]]*false([[:space:]]*(#.*)?)?$/) ||
    (/^[[:space:]]*cache-binary:[[:space:]]*/ && $0 !~ /^[[:space:]]*cache-binary:[[:space:]]*false([[:space:]]*(#.*)?)?$/) ||
    /^[[:space:]]*no-cache:[[:space:]]*false([[:space:]]*(#.*)?)?$/ {
      gsub(/^[[:space:]]+/, "")
      print
    }
  ' "$workflow")

  if [ "$workflow_failures" -eq 0 ]; then
    echo "[OK] Cache policy: $workflow"
  fi
done

if [ "$failures" -ne 0 ]; then
  exit 1
fi
