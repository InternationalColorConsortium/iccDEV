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

HADOLINT_IMAGE="${PREFLIGHT_HADOLINT_IMAGE:-hadolint/hadolint@sha256:30a8fd2e785ab6176eed53f74769e04f125afb2f74a6c52aef7d463583b6d45e}"
TRIVY_IMAGE="${PREFLIGHT_TRIVY_IMAGE:-aquasec/trivy@sha256:5c59e08f980b5d4d503329773480fcea2c9bdad7e381d846fbf9f2ecb8050f6b}"
TRIVY_CACHE_DIR="${PREFLIGHT_TRIVY_CACHE_DIR:-${TMPDIR:-/tmp}/iccdev-trivy-cache-$$}"

run_hadolint() {
  if command -v hadolint >/dev/null 2>&1; then
    hadolint "$@"
    return
  fi
  if command -v docker >/dev/null 2>&1; then
    docker run --rm \
      -v "$REPO_ROOT":/repo:ro \
      -w /repo \
      "$HADOLINT_IMAGE" \
      hadolint "$@"
    return
  fi
  return 127
}

run_trivy_config() {
  if command -v trivy >/dev/null 2>&1; then
    trivy config --skip-check-update --severity LOW,MEDIUM,HIGH,CRITICAL --exit-code 1 .
    return
  fi
  if command -v docker >/dev/null 2>&1; then
    rm -rf "$TRIVY_CACHE_DIR"
    mkdir -p "$TRIVY_CACHE_DIR"
    docker run --rm \
      -v "$REPO_ROOT":/repo:ro \
      -v "$TRIVY_CACHE_DIR":/root/.cache/ \
      -w /repo \
      "$TRIVY_IMAGE" \
      config --skip-check-update --severity LOW,MEDIUM,HIGH,CRITICAL --exit-code 1 /repo
    return
  fi
  return 127
}

run_dockerfile_policy() {
  local policy_failures=0
  local dockerfile final_from

  for dockerfile in "$@"; do
    if grep -qiE '^[[:space:]]*FROM[[:space:]].*nixos/nix' "$dockerfile"; then
      final_from="$(awk 'BEGIN{IGNORECASE=1} /^[[:space:]]*FROM[[:space:]]/ { line=$0 } END { print line }' "$dockerfile")"
      if grep -qi 'nixos/nix' <<< "$final_from"; then
        echo "[FAIL] $dockerfile final stage inherits from nixos/nix; use a minimal runtime stage to avoid channel/source closure leakage" >&2
        policy_failures=$((policy_failures + 1))
      fi
    fi
  done

  [ "$policy_failures" -eq 0 ]
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

workflow_security_canaries() {
  python3 - "${workflow_files[@]}" <<'PY'
import re
import sys
from pathlib import Path

import yaml


WRITE_SCOPES = {
    "actions",
    "attestations",
    "checks",
    "contents",
    "deployments",
    "id-token",
    "issues",
    "packages",
    "pages",
    "pull-requests",
    "security-events",
    "statuses",
}


def as_bool(value):
    if isinstance(value, bool):
        return value
    return str(value).strip().lower() == "true"


def env_name(value):
    if isinstance(value, str):
        return value
    if isinstance(value, dict):
        return str(value.get("name", ""))
    return ""


def has_write_permission(value):
    if isinstance(value, str):
        return value == "write-all"
    if not isinstance(value, dict):
        return False
    for key, permission in value.items():
        if str(key) in WRITE_SCOPES and str(permission).lower() == "write":
            return True
    return False


def step_uses(step):
    return str(step.get("uses", ""))


def step_run(step):
    return str(step.get("run", ""))


def with_block(step):
    block = step.get("with", {})
    return block if isinstance(block, dict) else {}


def clean_text(value):
    return re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]", "", str(value))


def workflow_triggers(workflow):
    triggers = workflow.get("on")
    if triggers is None:
        triggers = workflow.get(True)
    if isinstance(triggers, str):
        return {triggers}
    if isinstance(triggers, list):
        return {str(trigger) for trigger in triggers}
    if isinstance(triggers, dict):
        return {str(trigger) for trigger in triggers}
    return set()


def reusable_callees(workflow):
    jobs = workflow.get("jobs", {})
    if not isinstance(jobs, dict):
        return set()
    callees = set()
    for job in jobs.values():
        if not isinstance(job, dict):
            continue
        uses = str(job.get("uses", ""))
        match = re.match(r"\./\.github/workflows/([^/]+\.ya?ml)$", uses)
        if match:
            callees.add(match.group(1))
    return callees


def artifact_allowlist_reason(raw):
    lines = raw.splitlines()
    for index, line in enumerate(lines):
        if "uses:" not in line or "actions/upload-artifact@" not in line:
            continue
        start = max(0, index - 8)
        context = "\n".join(lines[start:index])
        match = re.search(r"preflight:\s*allow-untrusted-artifact\s+reason=(\S+)", context)
        if match:
            return clean_text(match.group(1))
    return ""


def token_exposure_allowlist_reason(raw, step_name):
    lines = raw.splitlines()
    step_pattern = re.compile(rf"^\s*-\s+name:\s*[\"']?{re.escape(str(step_name))}[\"']?\s*$")
    for index, line in enumerate(lines):
        if not step_pattern.match(line):
            continue
        context = "\n".join(lines[max(0, index - 8):index])
        match = re.search(r"preflight:\s*allow-token-exposure\s+reason=(\S+)", context)
        if match:
            return clean_text(match.group(1))
    return ""


def pr_script_allowlist_reason(raw, step_name):
    lines = raw.splitlines()
    step_pattern = re.compile(rf"^\s*-\s+name:\s*[\"']?{re.escape(str(step_name))}[\"']?\s*$")
    for index, line in enumerate(lines):
        if not step_pattern.match(line):
            continue
        context = "\n".join(lines[max(0, index - 8):index])
        match = re.search(r"preflight:\s*allow-pr-script-execution\s+reason=(\S+)", context)
        if match:
            return clean_text(match.group(1))
    return ""


def package_install_allowlist_reason(raw, step_name):
    lines = raw.splitlines()
    step_pattern = re.compile(rf"^\s*-\s+name:\s*[\"']?{re.escape(str(step_name))}[\"']?\s*$")
    for index, line in enumerate(lines):
        if not step_pattern.match(line):
            continue
        context = "\n".join(lines[max(0, index - 8):index])
        match = re.search(r"preflight:\s*allow-package-install\s+reason=(\S+)", context)
        if match:
            return clean_text(match.group(1))
    return ""


def yaml_anchor_hits(raw):
    hits = []
    block_indent = None
    anchor_re = re.compile(r"(^|[\s\[{,:])(&[A-Za-z0-9_-]+|\*[A-Za-z0-9_-]+)(?=$|[\s\]},#])")
    merge_re = re.compile(r"^\s*<<\s*:")
    block_re = re.compile(r":\s*[|>][-+0-9]*\s*(#.*)?$")

    for number, line in enumerate(raw.splitlines(), start=1):
        if block_indent is not None:
            indent = len(line) - len(line.lstrip(" "))
            if not line.strip() or indent > block_indent:
                continue
            block_indent = None

        stripped = line.lstrip(" ")
        if not stripped or stripped.startswith("#"):
            continue

        code = line.split("#", 1)[0]
        if merge_re.search(code):
            hits.append((number, "<<:"))
        for match in anchor_re.finditer(code):
            hits.append((number, match.group(2)))

        if block_re.search(code):
            block_indent = len(line) - len(line.lstrip(" "))

    return hits


def has_package_lifecycle_command(run):
    command_re = re.compile(
        r"(^|[;&|()\s])("
        r"(?:\S*/)?python[0-9.]*\s+-m\s+pip|"
        r"pip|go|npm|pnpm|yarn|npx|cargo|twine|cibuildwheel"
        r")\s+(install|ci|publish|build)\b",
        re.I,
    )
    for line in run.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if re.match(r"^(echo|printf|cat)\b", stripped):
            continue
        if command_re.search(stripped):
            return True
    return False


def reviewed_exception(label, category, reason):
    print(f"[OK] {label}: reviewed {category} exception reason={reason}")


def step_label(path, job_name, index, step):
    step_name = clean_text(step.get("name", f"step {index}"))
    return f"{path}: job {job_name}, step {index} ({step_name})"


def publish_like_step(step):
    uses = step_uses(step)
    run = step_run(step)
    block = with_block(step)
    if uses.startswith("docker/build-push-action@") and as_bool(block.get("push", False)):
        return True
    if uses.startswith("actions/attest-build-provenance@"):
        return True
    if uses.startswith("actions/attest-sbom@"):
        return True
    if re.search(r"\bgh\s+release\s+(create|upload|edit|delete)\b", run):
        return True
    if re.search(r"\bgh\s+api\b.*\b/packages/\b", run, re.S):
        return True
    return False


def job_context(workflow, job):
    workflow_permissions = workflow.get("permissions", {})
    job_permissions = job.get("permissions", workflow_permissions)
    environment = env_name(job.get("environment", ""))
    steps = job.get("steps", [])
    if not isinstance(steps, list):
        steps = []

    write_permissions = has_write_permission(job_permissions)
    protected_environment = bool(re.search(
        r"(release|publish|deploy|prod|ghcr)",
        environment,
        re.I,
    ))
    publishes = any(isinstance(step, dict) and publish_like_step(step) for step in steps)
    return write_permissions or protected_environment or publishes


failures = 0
warnings = 0
cache_publish = 0
artifact_intake = 0
artifact_publish = 0
auth_canaries = 0
untrusted_artifacts = 0
reviewed_artifact_exceptions = 0
reviewed_token_exceptions = 0
pr_script_canaries = 0
reviewed_pr_script_exceptions = 0
yaml_anchor_canaries = 0
package_install_canaries = 0
reviewed_package_install_exceptions = 0

all_workflows = {}
for workflow_path in sorted(Path(".github/workflows").glob("*.yml")) + sorted(Path(".github/workflows").glob("*.yaml")):
    try:
        workflow_raw = workflow_path.read_text(encoding="utf-8")
        workflow_data = yaml.safe_load(workflow_raw) or {}
    except (OSError, UnicodeDecodeError, yaml.YAMLError):
        continue
    if isinstance(workflow_data, dict):
        all_workflows[workflow_path.name] = workflow_data

pr_context_workflows = {
    name for name, workflow in all_workflows.items()
    if workflow_triggers(workflow) & {"pull_request", "pull_request_target"}
}
changed = True
while changed:
    changed = False
    for name in list(pr_context_workflows):
        for callee in reusable_callees(all_workflows.get(name, {})):
            if callee not in pr_context_workflows:
                pr_context_workflows.add(callee)
                changed = True

for path in sys.argv[1:]:
    with open(path, "r", encoding="utf-8") as handle:
        raw = handle.read()
    workflow = yaml.safe_load(raw) or {}
    if not isinstance(workflow, dict):
        continue
    jobs = workflow.get("jobs", {})
    if not isinstance(jobs, dict):
        continue

    if not workflow.get("permissions"):
        print(f"[WARN] {path}: workflow root permissions are not explicit")
        warnings += 1

    for line_number, token in yaml_anchor_hits(raw):
        print(f"[FAIL] {path}:{line_number}: YAML anchor/alias/merge syntax is banned ({token})",
              file=sys.stderr)
        failures += 1
        yaml_anchor_canaries += 1

    for job_name, job in jobs.items():
        if not isinstance(job, dict):
            continue
        steps = job.get("steps", [])
        if not isinstance(steps, list):
            continue
        publish_context = job_context(workflow, job)

        if job.get("secrets") == "inherit":
            print(f"[WARN] {path}: job {job_name} uses secrets: inherit")
            warnings += 1
            auth_canaries += 1

        for index, step in enumerate(steps, start=1):
            if not isinstance(step, dict):
                continue
            uses = step_uses(step)
            run = step_run(step)
            block = with_block(step)
            label = step_label(path, job_name, index, step)
            pr_context = bool(workflow_triggers(workflow) & {"pull_request", "pull_request_target"})

            if uses.startswith("actions/cache@"):
                if publish_context:
                    print(f"[FAIL] {label}: actions/cache in release/publish/write context", file=sys.stderr)
                    failures += 1
                    cache_publish += 1
                else:
                    print(f"[WARN] {label}: actions/cache use requires cache-poisoning review")
                    warnings += 1

            if uses.startswith("docker/build-push-action@") and publish_context:
                for key in ("cache-from", "cache-to"):
                    if "type=gha" in str(block.get(key, "")):
                        print(f"[FAIL] {label}: Docker Buildx {key}=type=gha in publish context",
                              file=sys.stderr)
                        failures += 1
                        cache_publish += 1

            if uses.startswith("actions/download-artifact@") and publish_context:
                if "name" not in block and "pattern" not in block:
                    print(f"[FAIL] {label}: publish job downloads artifacts without an explicit name/pattern",
                          file=sys.stderr)
                    failures += 1
                    artifact_intake += 1

            if uses.startswith("actions/upload-artifact@"):
                if Path(path).name in pr_context_workflows:
                    artifact_reason = artifact_allowlist_reason(raw)
                    if artifact_reason:
                        reviewed_exception(label, "untrusted PR artifact", artifact_reason)
                        reviewed_artifact_exceptions += 1
                    else:
                        print(f"[FAIL] {label}: untrusted PR-context workflow uploads artifacts",
                              file=sys.stderr)
                        failures += 1
                        untrusted_artifacts += 1
                if_no_files = str(block.get("if-no-files-found", "")).lower()
                artifact_name = str(block.get("name", ""))
                release_artifact = path.endswith("ci-latest-release.yml") and artifact_name.startswith("iccdev-")
                if (publish_context or release_artifact) and if_no_files in {"warn", "ignore"}:
                    print(f"[FAIL] {label}: release/publish artifact upload allows missing files",
                          file=sys.stderr)
                    failures += 1
                    artifact_publish += 1

            env = step.get("env", {})
            if publish_context and isinstance(env, dict):
                env_text = "\n".join(f"{key}: {value}" for key, value in env.items())
                if "secrets.GITHUB_TOKEN" in env_text or "github.token" in env_text:
                    token_reason = token_exposure_allowlist_reason(raw, step.get("name", f"step {index}"))
                    if token_reason:
                        reviewed_exception(label, "token exposure", token_reason)
                        reviewed_token_exceptions += 1
                        continue
                    print(f"[WARN] {label}: token is exposed to publish/write step environment")
                    warnings += 1
                    auth_canaries += 1

            if pr_context and run:
                pr_script_pattern = re.compile(
                    r"(^|[\s\"'])("
                    r"(source|\.)\s+[\"']?(\$GITHUB_WORKSPACE/)?\.github/(scripts|tests)/"
                    r"|"
                    r"(bash|pwsh|powershell)\s+[\"']?(\$GITHUB_WORKSPACE/)?\.github/(scripts|tests)/"
                    r"|"
                    r"(\$GITHUB_WORKSPACE/)?\.github/(scripts|tests)/[^\s\"'|&;]+\.(sh|ps1)"
                    r")",
                    re.I,
                )
                if pr_script_pattern.search(run):
                    script_reason = pr_script_allowlist_reason(raw, step.get("name", f"step {index}"))
                    if script_reason:
                        reviewed_exception(label, "PR-controlled script execution", script_reason)
                        reviewed_pr_script_exceptions += 1
                    else:
                        print(f"[FAIL] {label}: pull_request workflow executes .github scripts/tests from PR checkout",
                              file=sys.stderr)
                        failures += 1
                        pr_script_canaries += 1

            if run and has_package_lifecycle_command(run):
                package_reason = package_install_allowlist_reason(raw, step.get("name", f"step {index}"))
                if package_reason:
                    reviewed_exception(label, "package lifecycle command", package_reason)
                    reviewed_package_install_exceptions += 1
                else:
                    print(f"[FAIL] {label}: package lifecycle command requires reviewed pinning/isolation rationale",
                          file=sys.stderr)
                    failures += 1
                    package_install_canaries += 1

print(f"cache publish canaries: {cache_publish}")
print(f"artifact intake canaries: {artifact_intake}")
print(f"artifact publish canaries: {artifact_publish}")
print(f"untrusted PR artifact canaries: {untrusted_artifacts}")
print(f"auth/accounting canaries: {auth_canaries}")
print(f"PR-controlled script execution canaries: {pr_script_canaries}")
print(f"YAML anchor canaries: {yaml_anchor_canaries}")
print(f"package lifecycle canaries: {package_install_canaries}")
print(f"reviewed untrusted PR artifact exceptions: {reviewed_artifact_exceptions}")
print(f"reviewed token exposure exceptions: {reviewed_token_exceptions}")
print(f"reviewed PR-controlled script exceptions: {reviewed_pr_script_exceptions}")
print(f"reviewed package lifecycle exceptions: {reviewed_package_install_exceptions}")
print(f"workflow canary warnings: {warnings}")

raise SystemExit(1 if failures else 0)
PY
}

token_format_canaries() {
  local scan_files=()
  local token_failures=0

  scan_files+=("${workflow_files[@]}")
  scan_files+=("${script_files[@]}")
  scan_files+=("${python_files[@]}")

  if [ "${#scan_files[@]}" -eq 0 ]; then
    echo "GitHub token format canaries: 0"
    return 0
  fi

  while IFS= read -r match; do
    [ -n "$match" ] || continue
    echo "[FAIL] GitHub token format canary: $match" >&2
    token_failures=$((token_failures + 1))
  done < <(python3 - "${scan_files[@]}" <<'PY'
import re
import sys


patterns = [
    re.compile(r"ghs_\\[A-Za-z0-9\\]\\{[0-9,]+\\}"),
    re.compile(r"ghs_\\[A-Za-z0-9_\\]\\{[0-9,]+\\}"),
    re.compile(r"ghs_\[A-Za-z0-9\][{][0-9,]+[}]"),
    re.compile(r"gh[opsur]_\[A-Za-z0-9\][{][0-9,]+[}]"),
    re.compile(r"ghs_[^\\s'\"]*\\{36\\}"),
    re.compile(r"(GITHUB_TOKEN|GH_TOKEN|installation token|app token).{0,80}(length|characters|chars).{0,40}(40|64|255)"),
    re.compile(r"(length|characters|chars).{0,40}(40|64|255).{0,80}(GITHUB_TOKEN|GH_TOKEN|installation token|app token)"),
    re.compile(r"(GITHUB_TOKEN|GH_TOKEN).{0,80}(cut -c|head -c|substr|substring)"),
    re.compile(r"(cut -c|head -c|substr|substring).{0,80}(GITHUB_TOKEN|GH_TOKEN)"),
]

for path in sys.argv[1:]:
    try:
        lines = open(path, "r", encoding="utf-8").read().splitlines()
    except UnicodeDecodeError:
        continue
    for number, line in enumerate(lines, start=1):
        if "GitHub token format canary" in line:
            continue
        if path.endswith("preflight-safety-checks.sh") and "re.compile(" in line:
            continue
        if path.endswith("ci-pr-risk-security-analysis.yml") and "re.compile(" in line:
            continue
        if any(pattern.search(line) for pattern in patterns):
            print(f"{path}:{number}: {line.strip()}")
PY
  )

  echo "GitHub token format canaries: $token_failures"
  [ "$token_failures" -eq 0 ]
}

parse_codeql_sarif() {
  local sarif="$1"
  local label="$2"
  local marker="$3"
  shift 3

  python3 - "$sarif" "$label" "$marker" "$@" <<'PY'
import json
import re
import sys
from pathlib import Path


sarif_path = Path(sys.argv[1])
label = sys.argv[2]
marker = re.escape(sys.argv[3])
included = {arg.replace("\\", "/") for arg in sys.argv[4:]}

with sarif_path.open("r", encoding="utf-8") as handle:
    sarif = json.load(handle)

raw_results = 0
unreviewed = 0
reviewed = 0

for run in sarif.get("runs", []):
    for result in run.get("results", []):
        locations = result.get("locations") or []
        if not locations:
            continue
        physical = locations[0].get("physicalLocation") or {}
        artifact = physical.get("artifactLocation") or {}
        path = str(artifact.get("uri", "")).replace("\\", "/")
        if included and path not in included:
            continue

        raw_results += 1
        region = physical.get("region") or {}
        line = int(region.get("startLine") or 1)
        rule = str(result.get("ruleId", "unknown"))
        message = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]", "", str(
            result.get("message", {}).get("text", "")
        )).replace("\n", " ")

        reason = ""
        try:
            lines = Path(path).read_text(encoding="utf-8").splitlines()
            start = max(0, line - 12)
            end = min(len(lines), line + 8)
            context = "\n".join(lines[start:end])
            match = re.search(rf"preflight:\s*allow-{marker}\s+reason=(\S+)", context)
            if match:
                reason = match.group(1)
        except (OSError, UnicodeDecodeError):
            reason = ""

        if reason:
            print(f"[OK] {path}:{line}: reviewed CodeQL {label} exception "
                  f"reason={reason} rule={rule}")
            reviewed += 1
        else:
            print(f"[FAIL] {path}:{line}: CodeQL {label} finding {rule}: {message}",
                  file=sys.stderr)
            unreviewed += 1

print(f"CodeQL {label} raw results: {raw_results}")
print(f"CodeQL {label} findings: {unreviewed}")
print(f"reviewed CodeQL {label} exceptions: {reviewed}")

raise SystemExit(1 if unreviewed else 0)
PY
}

run_codeql_actions_analysis() {
  local tmp_dir db_dir sarif
  tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-codeql-actions.XXXXXX")"
  db_dir="$tmp_dir/db"
  sarif="$tmp_dir/actions.sarif"

  codeql pack download codeql/actions-queries
  codeql database create "$db_dir" --language=actions --source-root "$REPO_ROOT" --overwrite
  codeql database analyze "$db_dir" \
    codeql/actions-queries:codeql-suites/actions-security-extended.qls \
    --format=sarif-latest \
    --output "$sarif"
  local status=0
  parse_codeql_sarif "$sarif" "Actions" "codeql-actions" "${workflow_files[@]}" || status=$?
  rm -rf "$tmp_dir"
  return "$status"
}

run_codeql_python_analysis() {
  local tmp_dir db_dir sarif
  tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-codeql-python.XXXXXX")"
  db_dir="$tmp_dir/db"
  sarif="$tmp_dir/python.sarif"

  codeql pack download codeql/python-queries
  codeql database create "$db_dir" --language=python --source-root "$REPO_ROOT" --overwrite
  codeql database analyze "$db_dir" \
    codeql/python-queries:codeql-suites/python-security-extended.qls \
    --format=sarif-latest \
    --output "$sarif"
  local status=0
  parse_codeql_sarif "$sarif" "Python" "codeql-python" "${python_files[@]}" || status=$?
  rm -rf "$tmp_dir"
  return "$status"
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
python_files=()
docker_files=()
changed_files=()
base_ref="${PREFLIGHT_BASE_REF:-origin/master}"
if git rev-parse --verify "$base_ref" >/dev/null 2>&1; then
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git diff --name-only --diff-filter=ACMRT "$base_ref"...HEAD -- \
    .github .githooks Dockerfile 'Dockerfile.*' .dockerignore | sort)
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git diff --cached --name-only --diff-filter=ACMRT -- \
    .github .githooks Dockerfile 'Dockerfile.*' .dockerignore | sort)
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git diff --name-only --diff-filter=ACMRT -- \
    .github .githooks Dockerfile 'Dockerfile.*' .dockerignore | sort)
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(git ls-files --others --exclude-standard -- \
    .github .githooks Dockerfile 'Dockerfile.*' .dockerignore | sort)
else
  while IFS= read -r file; do
    changed_files+=("$file")
  done < <(
    {
      find .github .githooks -type f 2>/dev/null
      find . -maxdepth 1 -type f \( -name 'Dockerfile' -o -name 'Dockerfile.*' -o -name '.dockerignore' \)
    } | sed 's#^\./##' | sort
  )
fi

unique_changed_files=()
while IFS= read -r file; do
  unique_changed_files+=("$file")
done < <(printf '%s\n' "${changed_files[@]}" | awk 'NF && !seen[$0]++')

while IFS= read -r file; do
  workflow_files+=("$file")
done < <(
  find .github/workflows -maxdepth 1 -type f \( -name '*.yml' -o -name '*.yaml' \) 2>/dev/null |
    sed 's#^\./##' |
    sort
)

for file in "${unique_changed_files[@]}"; do
  case "$file" in
    .github/scripts/*.sh|.githooks/pre-commit|.githooks/pre-push)
      script_files+=("$file")
      ;;
    .github/scripts/*.py)
      python_files+=("$file")
      ;;
    Dockerfile|Dockerfile.*)
      docker_files+=("$file")
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

  run_check "workflow security canaries" workflow_security_canaries
  run_check "GitHub token format canaries" token_format_canaries
  run_check "local ci-risk-analysis subset" run_workflow_risk_subset
  if command -v codeql >/dev/null 2>&1; then
    run_check "CodeQL Actions analysis" run_codeql_actions_analysis
  else
    skip_or_fail "codeql"
  fi
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

if [ "${#python_files[@]}" -gt 0 ]; then
  run_check "Python syntax" python3 -m py_compile "${python_files[@]}"
  if command -v codeql >/dev/null 2>&1; then
    run_check "CodeQL Python analysis" run_codeql_python_analysis
  else
    skip_or_fail "codeql"
  fi
else
  echo "[SKIP] No changed Python scripts"
  echo ""
fi

if [ "${#docker_files[@]}" -gt 0 ]; then
  if command -v hadolint >/dev/null 2>&1 || command -v docker >/dev/null 2>&1; then
    run_check "hadolint" run_hadolint "${docker_files[@]}"
  else
    skip_or_fail "hadolint or docker"
  fi

  if command -v trivy >/dev/null 2>&1 || command -v docker >/dev/null 2>&1; then
    run_check "Trivy config" run_trivy_config "${docker_files[@]}"
  else
    skip_or_fail "trivy or docker"
  fi

  run_check "Dockerfile runtime policy" run_dockerfile_policy "${docker_files[@]}"
else
  echo "[SKIP] No changed Dockerfile files"
  echo ""
fi

if [ -f .github/codeql-queries/iccdev-security-suite.qls ]; then
  if command -v codeql >/dev/null 2>&1; then
    run_check "CodeQL query resolution" codeql resolve queries .github/codeql-queries/iccdev-security-suite.qls
  elif command -v gh >/dev/null 2>&1 && gh codeql version >/dev/null 2>&1; then
    run_check "CodeQL query resolution" gh codeql resolve queries .github/codeql-queries/iccdev-security-suite.qls
  else
    skip_or_fail "codeql or gh codeql"
  fi
else
  echo "[SKIP] No CodeQL query suite found"
  echo ""
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
