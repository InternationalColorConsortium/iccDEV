#!/usr/bin/env bash
###############################################################
#
# Copyright (c) 2025-2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Run local workflow, script, and security pre-flight checks.
#
###############################################################
set -euo pipefail

REQUIRE_TOOLS=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --require-tools) REQUIRE_TOOLS=1; shift ;;
    --help|-h)
      echo "Usage: preflight-safety-checks.sh [--require-tools]"
      exit 0
      ;;
    *) echo "[FAIL] Unknown option: $1" >&2; exit 1 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="${PREFLIGHT_SCAN_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
cd "$REPO_ROOT"

failures=0
skips=0

run_check() {
  local name="$1"
  shift
  echo "=== $name ==="
  if "$@"; then
    echo "[OK] $name"
  else
    echo "[FAIL] $name" >&2
    failures=$((failures + 1))
  fi
  echo ""
}

skip_or_fail() {
  local tool="$1"
  if [ "$REQUIRE_TOOLS" -eq 1 ]; then
    echo "[FAIL] Required tool not found: $tool" >&2
    failures=$((failures + 1))
  else
    echo "[SKIP] Tool not found: $tool"
    skips=$((skips + 1))
  fi
}

workflow_trigger_names() {
  local wf="$1"
  python3 - "$wf" <<'PY'
import sys

import yaml

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    workflow = yaml.safe_load(handle) or {}

if not isinstance(workflow, dict):
    raise SystemExit(0)

triggers = workflow.get("on")
if triggers is None:
    # PyYAML follows YAML 1.1 and parses an unquoted "on" key as True.
    triggers = workflow.get(True)

if isinstance(triggers, str):
    print(triggers)
elif isinstance(triggers, list):
    for trigger in triggers:
        print(trigger)
elif isinstance(triggers, dict):
    for trigger in triggers:
        print(trigger)
PY
}

workflow_checkout_credential_issues() {
  local wf="$1"
  python3 - "$wf" <<'PY'
import sys

import yaml

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    workflow = yaml.safe_load(handle) or {}

jobs = workflow.get("jobs", {}) if isinstance(workflow, dict) else {}
if not isinstance(jobs, dict):
    raise SystemExit(0)

for job_name, job in jobs.items():
    if not isinstance(job, dict):
        continue
    steps = job.get("steps", [])
    if not isinstance(steps, list):
        continue
    for index, step in enumerate(steps, start=1):
        if not isinstance(step, dict):
            continue
        uses = str(step.get("uses", ""))
        if not uses.startswith("actions/checkout@"):
            continue
        with_block = step.get("with", {})
        if not isinstance(with_block, dict):
            with_block = {}
        persist = with_block.get("persist-credentials")
        if persist is False or str(persist).lower() == "false":
            continue
        step_name = step.get("name", f"step {index}")
        print(f"{sys.argv[1]}: job {job_name}, {step_name}")
PY
}

run_workflow_risk_subset() {
  local risk_failures=0
  local total_actions=0
  local pinned_actions=0
  local risky_triggers=0
  local injectable_expressions=0
  local checkout_without_persist_false=0
  local unsanitized_outputs=0

  for wf in "${workflow_files[@]}"; do
    echo "--- $wf ---"

    while IFS= read -r action_ref; do
      total_actions=$((total_actions + 1))
      if grep -qE '@[0-9a-f]{40}' <<< "$action_ref"; then
        pinned_actions=$((pinned_actions + 1))
      else
        echo "[FAIL] action is not SHA-pinned: $action_ref" >&2
        risk_failures=$((risk_failures + 1))
      fi
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
      /^[[:space:]]*-?[[:space:]]*uses:[[:space:]]/ {
        sub(/.*uses:[[:space:]]*/, "")
        gsub(/["'\''"]/, "")
        if ($0 !~ /^#/ && $0 !~ /docker:\/\// && $0 !~ /^\.\/\.github\/workflows\//) print
      }
    ' "$wf")

    trigger_names="$(workflow_trigger_names "$wf")"
    if grep -qx 'pull_request_target' <<< "$trigger_names" &&
       grep -q 'ref.*pull_request.*head' "$wf" 2>/dev/null; then
      echo "[FAIL] pull_request_target checks out PR head content" >&2
      risky_triggers=$((risky_triggers + 1))
      risk_failures=$((risk_failures + 1))
    fi
    if grep -qx 'workflow_run' <<< "$trigger_names" &&
       ! grep -q 'workflow_run.head_repository.full_name == github.repository' "$wf"; then
      echo "[FAIL] workflow_run trigger is missing same-repository guard" >&2
      risky_triggers=$((risky_triggers + 1))
      risk_failures=$((risk_failures + 1))
    fi
    if grep -qE '^(issue_comment|repository_dispatch)$' <<< "$trigger_names"; then
      echo "[FAIL] workflow uses a high-risk event trigger" >&2
      risky_triggers=$((risky_triggers + 1))
      risk_failures=$((risk_failures + 1))
    fi

    while IFS= read -r match; do
      echo "[FAIL] injectable expression in run block: $match" >&2
      injectable_expressions=$((injectable_expressions + 1))
      risk_failures=$((risk_failures + 1))
    done < <(awk '
      /^[[:space:]]*run:[[:space:]]*\|/ {
        in_run = 1
        match($0, /^[[:space:]]*/); run_indent = RLENGTH
        next
      }
      in_run {
        match($0, /^[[:space:]]*/); cur = RLENGTH
        if ($0 !~ /^[[:space:]]*$/ && cur <= run_indent) { in_run = 0 }
      }
      in_run && /\$\{\{/ {
        if (/\$\{\{[^}]* matrix\./ || /\$\{\{ *matrix\./ || /github\.event\.(issue|pull_request|discussion)\.(title|body)/ || /github\.event\.(comment|review|review_comment)\.body/ || /github\.event\.pages/ || /github\.event\.commits/ || /github\.head_ref/ || /inputs\./) {
          if (/needs\.[a-zA-Z0-9_-]*\.result/) next
          gsub(/^[[:space:]]+/, "")
          print
        }
      }
    ' "$wf")

    while IFS= read -r match; do
      [ -n "$match" ] || continue
      echo "[FAIL] actions/checkout is missing persist-credentials: false: $match" >&2
      checkout_without_persist_false=$((checkout_without_persist_false + 1))
      risk_failures=$((risk_failures + 1))
    done < <(workflow_checkout_credential_issues "$wf")

    while IFS= read -r match; do
      echo "[FAIL] unsanitized GITHUB_STEP_SUMMARY/GITHUB_OUTPUT write: $match" >&2
      unsanitized_outputs=$((unsanitized_outputs + 1))
      risk_failures=$((risk_failures + 1))
    done < <(awk '
      /GITHUB_(STEP_SUMMARY|OUTPUT)/ && />>/ {
        if ($0 !~ /sanitize_line|sanitize_print|sanitize_codeblock|sanitize_ref|sanitize_filename|escape_html|elements-sanitized|Sanitize-Line|Sanitize-Print|Safe-EchoForSummary|Escape-Html/) {
          gsub(/^[[:space:]]+/, "")
          print
        }
      }
    ' "$wf")
  done

  echo "workflow files: ${#workflow_files[@]}"
  echo "actions SHA-pinned: $pinned_actions/$total_actions"
  echo "risky triggers: $risky_triggers"
  echo "injectable run expressions: $injectable_expressions"
  echo "checkout credential issues: $checkout_without_persist_false"
  echo "unsanitized output writes: $unsanitized_outputs"

  if [ "$risk_failures" -ne 0 ]; then
    return 1
  fi
}

workflow_files=()
script_files=()
changed_files=()
base_ref="${PREFLIGHT_BASE_REF:-origin/master}"
if git rev-parse --verify "$base_ref" >/dev/null 2>&1; then
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git diff --name-only --diff-filter=ACMRT "$base_ref"...HEAD -- .github .githooks | sort)
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git diff --cached --name-only --diff-filter=ACMRT -- .github .githooks | sort)
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git diff --name-only --diff-filter=ACMRT -- .github .githooks | sort)
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git ls-files --others --exclude-standard -- .github .githooks | sort)
else
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(find .github .githooks -type f 2>/dev/null | sort)
fi

unique_changed_files=()
while IFS= read -r file; do
  unique_changed_files+=("$file")
done < <(printf '%s\n' "${changed_files[@]}" | awk 'NF && !seen[$0]++')

for file in "${unique_changed_files[@]}"; do
  case "$file" in
    .github/workflows/*.yml|.github/workflows/*.yaml)
      workflow_files+=("$file")
      ;;
    .github/scripts/*.sh|.githooks/pre-push)
      script_files+=("$file")
      ;;
  esac
done

if [ "${#workflow_files[@]}" -gt 0 ]; then
  run_check "YAML parse" python3 - "${workflow_files[@]}" <<'PY'
import sys

try:
    import yaml
except ImportError:
    print("PyYAML is required for YAML parse checks", file=sys.stderr)
    raise SystemExit(1)

for path in sys.argv[1:]:
    with open(path, "r", encoding="utf-8") as handle:
        yaml.safe_load(handle)
    print(f"[OK] {path}")
PY

  if command -v actionlint >/dev/null 2>&1; then
    run_check "actionlint" actionlint -no-color "${workflow_files[@]}"
  else
    skip_or_fail "actionlint"
  fi

  if command -v yamllint >/dev/null 2>&1; then
    run_check "yamllint" yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' "${workflow_files[@]}"
  else
    skip_or_fail "yamllint"
  fi

  if command -v zizmor >/dev/null 2>&1; then
    run_check "zizmor" zizmor "${workflow_files[@]}"
  else
    skip_or_fail "zizmor"
  fi

  run_check "local ci-risk-analysis subset" run_workflow_risk_subset
else
  echo "[SKIP] No changed workflow files"
  echo ""
fi

if [ "${#script_files[@]}" -gt 0 ]; then
  if command -v shellcheck >/dev/null 2>&1; then
    run_check "shellcheck" shellcheck "${script_files[@]}"
  else
    skip_or_fail "shellcheck"
  fi
else
  echo "[SKIP] No changed shell hook/script files"
  echo ""
fi

if command -v codeql >/dev/null 2>&1; then
  run_check "CodeQL query resolution" codeql resolve queries .github/codeql-queries/iccdev-security-suite.qls
elif command -v gh >/dev/null 2>&1 && gh codeql version >/dev/null 2>&1; then
  run_check "CodeQL query resolution" gh codeql resolve queries .github/codeql-queries/iccdev-security-suite.qls
else
  skip_or_fail "codeql or gh codeql"
fi

if [ -f .github/scripts/audit-workflow-permissions.py ]; then
  run_check "workflow permission audit" python3 .github/scripts/audit-workflow-permissions.py
else
  skip_or_fail "audit-workflow-permissions.py"
fi

echo "=== Pre-flight summary ==="
echo "failures: $failures"
echo "skips: $skips"

if [ "$failures" -ne 0 ]; then
  exit 1
fi
