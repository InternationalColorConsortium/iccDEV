#!/usr/bin/env bash
###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Verify workflow YAML does not use workflow cache primitives.
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
  done < <(python3 - "$workflow" <<'PY'
import re
import sys
from pathlib import Path


def code_lines(path):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    in_run = False
    run_indent = 0
    for number, line in enumerate(lines, start=1):
        indent = len(line) - len(line.lstrip(" "))
        if in_run:
            if line.strip() and indent <= run_indent:
                in_run = False
            else:
                continue
        if re.match(r"^\s*run:\s*[|>]", line):
            in_run = True
            run_indent = indent
            continue
        yield number, line.split("#", 1)[0].rstrip(), lines


def step_window(lines, start_index):
    start_indent = len(lines[start_index]) - len(lines[start_index].lstrip(" "))
    window = [lines[start_index]]
    for line in lines[start_index + 1:]:
        indent = len(line) - len(line.lstrip(" "))
        if line.strip() and indent <= start_indent and re.match(r"^\s*-?\s*(name|uses|run):", line):
            break
        window.append(line)
    return "\n".join(window)


workflow = Path(sys.argv[1])
for number, code, lines in code_lines(workflow):
    stripped = code.strip()
    if re.match(r"uses:\s*actions/cache@", stripped, re.I):
        print(f"{number}: actions/cache")
    if re.match(r"uses:\s*msys2/setup-msys2@", stripped, re.I):
        window = step_window(lines, number - 1)
        if not re.search(r"(?m)^\s+cache:\s*(false|0|no|off)\s*$", window, re.I):
            print(f"{number}: msys2/setup-msys2 cache not disabled")
    if (workflow.name == "ci-docker.yml"
            and re.match(r"cache-to:\s*type=inline\s*$", stripped, re.I)):
        continue
    if re.match(r"cache-(from|to):\s*\S", stripped, re.I):
        print(f"{number}: {stripped}")
    if re.match(r"(cache-dependency-path|restore-keys|save-always):\s*\S", stripped, re.I):
        print(f"{number}: {stripped}")
    match = re.match(r"cache:\s*(\S.*)$", stripped, re.I)
    if match and match.group(1).strip().lower().strip("'\"") not in {"false", "0", "no", "off"}:
        print(f"{number}: {stripped}")
    match = re.match(r"cache-binary:\s*(\S.*)$", stripped, re.I)
    if match and match.group(1).strip().lower().strip("'\"") not in {"false", "0", "no", "off"}:
        print(f"{number}: {stripped}")
    match = re.match(r"no-cache:\s*(\S.*)$", stripped, re.I)
    if match and match.group(1).strip().lower().strip("'\"") == "false":
        print(f"{number}: {stripped}")
PY
  )
  if [ "$workflow_failures" -eq 0 ]; then
    echo "[OK] Cache policy: $workflow"
  fi
done

if [ "$failures" -ne 0 ]; then
  exit 1
fi
