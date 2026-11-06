#!/usr/bin/env bash
###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Allow verified archives and digest-pinned validated-image build caches.
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
    directory_workflows=$(find "$item" -maxdepth 1 -type f \( -name '*.yml' -o -name '*.yaml' \) | sort)
    while IFS= read -r workflow; do
      [ -n "$workflow" ] || continue
      workflow_files+=("$workflow")
    done <<< "$directory_workflows"
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

  # Capture the scanner result before reading it. The process-substitution
  # redirection fails on macOS Bash 3.2, allowing a false [OK] result.
  matches=$(python3 - "$workflow" <<'PY'
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


def step_window(lines, action_index):
    action_indent = len(lines[action_index]) - len(lines[action_index].lstrip(" "))
    step_indent = max(0, action_indent - 2)
    start = action_index
    while start > 0:
        candidate = lines[start - 1]
        indent = len(candidate) - len(candidate.lstrip(" "))
        if candidate.strip() and indent == step_indent and candidate.lstrip().startswith("- "):
            start -= 1
            break
        start -= 1
    end = action_index + 1
    while end < len(lines):
        candidate = lines[end]
        indent = len(candidate) - len(candidate.lstrip(" "))
        if candidate.strip() and indent == step_indent and candidate.lstrip().startswith("- "):
            break
        end += 1
    return "\n".join(lines[start:end])


def scalar(window, name):
    lines = window.splitlines()
    for index, line in enumerate(lines):
        block = re.match(
            rf"^(\s+){re.escape(name)}:\s*[|>][-+]?\s*$",
            line,
        )
        if not block:
            continue
        base_indent = len(block.group(1))
        values = []
        for candidate in lines[index + 1:]:
            indent = len(candidate) - len(candidate.lstrip(" "))
            if candidate.strip() and indent <= base_indent:
                break
            if candidate.strip():
                values.append(candidate.strip())
        return " ".join(values)

    match = re.search(rf"(?m)^\s+{re.escape(name)}:\s*(\S[^\r\n]*)$", window)
    if match:
        value = match.group(1).strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        return value
    return ""


def cache_action_findings(window, action, workflow_text, restore_ids):
    findings = []
    action_match = re.fullmatch(
        r"actions/cache/(restore|save)@([0-9a-f]{40})",
        action,
    )
    if not action_match:
        return ["actions/cache must use restore or save pinned to a lowercase commit SHA"]

    operation = action_match.group(1)
    path = scalar(window, "path")
    key = scalar(window, "key")
    if not re.fullmatch(r"(?:/|\$\{\{ runner\.temp \}\}/)[^\r\n]+\.(?:zip|tar\.gz|tgz|tar\.xz)", path):
        findings.append("cache path must name one immutable archive file")
    digest_match = re.search(r"(?:^|[-_])([0-9a-f]{64})(?:$|[-_])", key)
    if "${{" in key or not digest_match:
        findings.append("cache key must be literal and contain the archive SHA-256")
    elif (
        workflow_text.count(digest_match.group(1)) < 2
        or not re.search(r"\b(sha256sum|Get-FileHash)\b", workflow_text, re.I)
    ):
        findings.append("cached archive SHA-256 must be re-verified before extraction")

    if operation == "save":
        condition = scalar(window, "if")
        trusted_events = {"push", "schedule", "workflow_dispatch"}
        trusted_term = (
            r"github\.event_name\s*==\s*['\"]"
            r"(?:push|schedule|workflow_dispatch)['\"]"
        )
        named_events = set(re.findall(
            r"github\.event_name\s*==\s*['\"]([a-z_]+)['\"]",
            condition,
        ))
        trusted_expression = rf"{trusted_term}(?:\s*\|\|\s*{trusted_term})*"
        restore_miss = (
            r"steps\.([A-Za-z0-9_-]+)\.outputs\.cache-hit\s*!=\s*['\"]true['\"]"
        )
        safe_save_condition = (
            rf"(?:\({trusted_expression}\)|{trusted_expression})"
            rf"\s*&&\s*{restore_miss}"
        )
        if "github.event_name" not in condition or not named_events:
            findings.append("cache save requires an explicit trusted-event condition")
        elif not named_events <= trusted_events:
            findings.append("cache save condition includes an untrusted event")
        elif not re.fullmatch(safe_save_condition, condition):
            findings.append(
                "cache save condition must contain only trusted-event equality checks "
                "and a restore miss check"
            )
        else:
            restore_id = re.search(restore_miss, condition).group(1)
            if restore_id not in restore_ids:
                findings.append(
                    "cache save miss check must reference an actions/cache/restore step id"
                )
    return findings


workflow = Path(sys.argv[1])
workflow_text = workflow.read_text(encoding="utf-8")
outside_run = list(code_lines(workflow))
outside_text = "\n".join(code for _, code, _ in outside_run)
restore_ids = set()
for number, code, lines in outside_run:
    if re.match(r"(?:-\s*)?uses:\s*actions/cache/restore@", code.strip(), re.I):
        restore_id = scalar(step_window(lines, number - 1), "id")
        if restore_id:
            restore_ids.add(restore_id)
low_trust_trigger = re.search(
    r"(?m)^\s{0,4}(pull_request_target|workflow_run|issue_comment|issues):\s*$",
    outside_text,
)
write_boundary = re.search(
    r"(?m)^\s+(contents|packages|attestations|artifact-metadata|id-token|actions|"
    r"pull-requests|issues|statuses|checks|deployments|pages):\s*write\s*$",
    outside_text,
)
safe_docker_cache_contract = (
    not low_trust_trigger
    and re.search(r"(?m)^\s{2}workflow_dispatch:\s*$", outside_text)
    and "docker buildx imagetools inspect" in workflow_text
    and "source=type=registry,ref=%s@%s" in workflow_text
    and re.search(
        r'cache_digest"\s*=~\s*\^sha256:\[0-9a-f\]\{64\}\$',
        workflow_text,
    )
    and "no-cache: ${{ !inputs.use_build_cache }}" in workflow_text
)
for number, code, lines in outside_run:
    stripped = code.strip()
    action_match = re.match(r"(?:-\s*)?uses:\s*(actions/cache(?:/[^@]+)?@\S+)", stripped, re.I)
    if action_match:
        window = step_window(lines, number - 1)
        if low_trust_trigger:
            print(f"{number}: actions/cache is forbidden on low-trust triggers")
        if write_boundary:
            print(f"{number}: actions/cache is forbidden in a write-capable workflow")
        for finding in cache_action_findings(
            window,
            action_match.group(1),
            workflow_text,
            restore_ids,
        ):
            print(f"{number}: {finding}")
    if re.match(r"(?:-\s*)?uses:\s*msys2/setup-msys2@", stripped, re.I):
        window = step_window(lines, number - 1)
        if not re.search(r"(?m)^\s+cache:\s*(false|0|no|off)\s*$", window, re.I):
            print(f"{number}: msys2/setup-msys2 cache not disabled")
    docker_cache_match = re.match(r"cache-(from|to):\s*(\S.*)$", stripped, re.I)
    if docker_cache_match:
        direction = docker_cache_match.group(1).lower()
        value = docker_cache_match.group(2).strip()
        expected = {
            "from": "${{ inputs.use_build_cache && steps.cache.outputs.source || '' }}",
            "to": (
                "${{ inputs.use_build_cache && env.PUBLISH_IMAGE == 'true' "
                "&& 'type=inline' || '' }}"
            ),
        }[direction]
        if not safe_docker_cache_contract or value != expected:
            print(
                f"{number}: Docker cache-{direction} must use the "
                "digest-pinned validated-image inline-cache contract"
            )
    if re.match(r"(cache-dependency-path|restore-keys|save-always):\s*\S", stripped, re.I):
        print(f"{number}: {stripped}")
    if re.match(r"enableCrossOsArchive:\s*\S", stripped, re.I):
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
  while IFS= read -r match; do
    [ -n "$match" ] || continue
    echo "[FAIL] Cache policy violation in $workflow: $match" >&2
    workflow_failures=$((workflow_failures + 1))
    failures=$((failures + 1))
  done <<< "$matches"
  if [ "$workflow_failures" -eq 0 ]; then
    echo "[OK] Cache policy: $workflow"
  fi
done

if [ "$failures" -ne 0 ]; then
  exit 1
fi
