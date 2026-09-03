# Maintainer Label System

Use this guide when changing iccDEV labels, issue triage, pull request path
labels, or CI status labels. Labels are maintainer-owned infrastructure because
they affect review routing, security handling, CI visibility, and merge
readiness.

## Canonical Files

| File | Purpose |
|------|---------|
| `.github/labels.yml` | Managed label names, colors, and descriptions. |
| `.github/labeler.yml` | Deterministic pull request path-to-label rules. |
| `.github/scripts/sync-labels.sh` | Local and CI label synchronization helper. |
| `docs/label-inventory-audit.md` | Managed-versus-legacy inventory and API-write record. |
| `.github/agents/maintainer-label-triage.agent.md` | Read-only label-system audit. |
| `.github/workflows/sync-labels.yml` | Applies `.github/labels.yml` on `master` or manual dispatch. |
| `.github/workflows/pr-labeler.yml` | Applies path labels to pull requests. |
| `.github/workflows/ci-fork-automation-gate.yml` | Fails protected automation changes from forks and applies `Governance`. |
| `.github/workflows/label.yml` | Adds first-pass issue triage labels and welcome guidance. |
| `.github/workflows/update-labels.yml` | Adds PR CI status labels: `passed`, `failed`, `pending`, `Merge Ready`. |
| `.github/workflows/ci-codeql-security.yml` | Runs full CodeQL when the `codeql-ready` label is applied. |

## Label Classes

| Class | Labels | Owner |
|-------|--------|-------|
| Issue triage | `needs-triage`, `needs-repro`, `requires:more-information` | Maintainers |
| Issue kind | `bug`, `feature`, `question`, `security` | Maintainers |
| PR status | `passed`, `failed`, `pending`, `Merge Ready`, `codeql-ready` | CI and maintainers |
| Dependency maintenance | `bump-sha-pins` | Maintainers |
| Scope | `Source`, `Documentation`, `Configuration`, `CI`, `Build`, `Testing`, `Tools`, `JSON`, `Python`, `WASM`, `vcpkg`, and related area labels | Path labeler and maintainers |
| Governance | `Governance`, `Labels`, `security`, `SAST`, `CodeQL`, `Sanitizers`, `Release` | Maintainers |

Keep existing public label names stable unless maintainers intentionally migrate
them. New workflow or status labels should be lower-case hyphenated, except
when matching an established GitHub convention or existing repository label.
New area labels should match the current Title Case scope style.

## Inventory and API Budget

`.github/labels.yml` is the managed manifest, not a destructive declaration of
every live GitHub label. The repository retains legacy labels while open issues,
open PRs, workflows, prompts, or documentation still use them. Do not attach an
undeclared legacy label to new automation; either add it to the managed manifest
or complete an intentional migration.

Record a live-versus-managed inventory before a taxonomy change using
`docs/label-inventory-audit.md`. Label synchronization first fetches the live
inventory and updates only missing or drifted managed labels. Issue triage does
not synchronize the taxonomy: it only applies labels that the taxonomy-sync
workflow has already provisioned. Batch taxonomy edits in one change to avoid
repeated label API writes.

## Workflow Behavior

### Label Sync

`sync-labels.yml` runs on pushes to `master` that touch label taxonomy files,
and it can be started manually with `workflow_dispatch`. The workflow grants
only `issues: write` and `contents: read`, checks out only the taxonomy and sync
script, and calls:

```bash
.github/scripts/sync-labels.sh
```

The script creates missing labels and updates only changed color or description
metadata after reading the live inventory. It does not delete labels. Retire
labels manually only after checking open issues, open PRs, workflow references,
prompts, and documentation.

For local validation without mutating GitHub:

```bash
GH_REPOSITORY=InternationalColorConsortium/iccDEV \
  .github/scripts/sync-labels.sh --dry-run
```

### Pull Request Path Labels

`pr-labeler.yml` runs on `pull_request_target` so it can label forked pull
requests. It must not check out or execute PR-controlled code. The workflow
checks out only `.github/labeler.yml` from the trusted base commit and runs the
pinned `actions/labeler` action with `sync-labels: true`. The trigger carries
an explicit `zizmor` exception because the workflow needs repository write
permission for label updates; keep the exception only while the workflow remains
metadata-only.

Path labels are deterministic. Do not add sentiment, severity, or natural
language classification to `.github/labeler.yml`; those decisions belong to
maintainers during review.

The labeler skips file-based labels on very large PRs through:

```yaml
max-files-changed: 350
changed-files-labels-limit: 14
```

Maintainers can add labels manually when a large tree-wide change intentionally
spans many components.

### Fork Automation Review

`ci-fork-automation-gate.yml` uses `pull_request_target` only to inspect fork
PR file names through the GitHub API. It checks out trusted base helpers, never
checks out or executes fork content, and applies the existing `Governance`
label before failing when a fork changes workflows, scripts, hooks, Docker
files, or build configuration. The workflow does not close pull requests.

Normal PR build jobs are restricted to same-repository heads. Repository
Actions settings must require approval for all external contributors, and
maintainers must not approve a fork workflow after the `Governance` label is
applied until the protected-path change has been reviewed.

### Issue Triage

`label.yml` runs on issue open, edit, and reopen events. It adds
`needs-triage` and lightweight issue-kind and scope labels from the issue title
and body, including `Python` for Python, Cython, PyPI, pip, setuptools, or
wheel reports. It may also add `needs-repro` or
`requires:more-information` when a report is too short, contains a placeholder,
or describes a crash without a reproducible input or command.

The triage workflow is a routing aid, not a maintainer decision. Maintainers may
adjust or remove labels after reviewing reproductions, security impact, and
required test coverage. Label sync creates missing labels and updates the color
or description of managed labels declared in `.github/labels.yml`; retiring a
label is manual and must follow the deletion review in the maintainer change
process.

### PR CI Status Labels

`update-labels.yml` evaluates open PRs on schedule, PR events, and manual
dispatch. It keeps `passed`, `failed`, and `pending` mutually exclusive. It adds
`Merge Ready` only when CI is successful, the PR is not draft, the merge state is
clean, and the review decision is approved. It does not synchronize the label
manifest; the dedicated taxonomy-sync workflow provisions managed labels.

Do not use these status labels as the only merge gate. Branch protection and
required checks remain authoritative.

### PR CI Scope

`ci-pr-action.yml` does not trigger on label changes. This prevents path and
status label automation from starting another full CI run. Use labels for
classification and review routing, not as CI controls.

Pull requests and manual dispatches default to `ci_scope=auto`. In auto scope,
the full matrix runs only for source, build, or test changes;
documentation-only changes use the constrained fast-lane settings, and
workflow-only changes receive the preflight and workflow-security gates.
Container-only changes use workflow-security gates and local container
validation.
Dispatch `ci_scope=full` explicitly for a long-cycle matrix. For the shortest
same-repository PR lane, provide an open `pr_number`, choose
`ci_scope=fast-lane`, and set `ctest_recent_limit`, `include_windows`,
and `warning_policy` on that dispatch. Fast lane defaults to the latest
registered CTest and strict warning failure. Windows is opt-in. The PR
orchestrator does not run a Docker verification job.

Manual dispatches use an event-qualified concurrency group. A dispatch on an
open PR therefore does not cancel that PR's `pull_request` run; inspect the
dispatched run by its run ID rather than treating `gh pr checks` as its status.
For an owner-visible long-cycle build inventory, dispatch either
`CI Comprehensive Build and Test` (including Windows) or `CI Build Test (No
Fuzzers)` (without Windows). Both callers use `_build-matrix.yml`, which
centralizes the current reusable Unix and Windows gates plus focused sanitizer,
option, version-header, and clean-rebuild lanes.

Container-surface changes, including the unified Dockerfile, packaged MCP
source, or `ci-docker-pr.yml`, select workflow-security gates only. The brittle
Docker Clang verification job is not part of `ci-pr-action`; validate a
container change locally before the canonical `ci-docker` publishing workflow.

### Required Check Policy

Branch rules require stable aggregate contexts rather than mode-specific build
job names. This lets full, fast-lane, auto, governance, and docs runs use one
fail-closed policy:

| Required context | Purpose |
|------------------|---------|
| `PR Summary` | Aggregates orchestration prerequisites and every selected build/test lane. |
| `Risk Analysis Gate / Workflow Security Audit` | Enforces Linux workflow and container security canaries. |
| `Risk Analysis Gate / Windows Security Audit (PowerShell)` | Enforces PowerShell and Windows workflow security canaries. |
| `WASM Release Build + Parity` | Required separately on `master` because it is outside `ci-pr-action`. |

Do not require individual Unix, GCC 15.2, tool-test, Windows, or Docker job
names in branch rules. Those jobs are conditional by selected mode and are
covered by `PR Summary`. The summary must treat failed or cancelled detection,
setup, and input-validation prerequisites as failures; only intentionally
skipped mode-specific jobs are acceptable.

The active `ci-qa-flags` ruleset requires the three `ci-pr-action` contexts.
`ci-pr-action` therefore runs for pull requests targeting either `master` or
`ci-qa-flags`. WASM parity remains a `master`-only required context.

The `ci-qa-pr-docker-testing` integrity ruleset does not require hosted status
contexts before a direct maintainer push. It requires signed commits, linear
fast-forward history, and deletion protection; maintainers dispatch
`ci-pr-action` and `ci-docker` immediately after pushing the testing branch.
Pull requests targeting `ci-qa-pr-docker-testing` also run `ci-pr-action`.
Container-surface changes select its workflow-security gates only; they do not
create a job-local Docker image.

### CodeQL Ready

`ci-codeql-security.yml` uses the `codeql-ready` label to trigger the full
CodeQL security workflow for a PR. Use this label when a change touches C/C++,
CMake, CodeQL query logic, parser hardening, or security-sensitive automation
and the fast preflight checks are not enough.

## Maintainer Change Process

1. Add or edit labels in `.github/labels.yml` first.
2. Add `.github/labeler.yml` path rules only for deterministic scope labels.
3. Update this guide, `docs/label-inventory-audit.md`,
   `.github/skills/maintainer-label-system/SKILL.md`, or
   `.github/prompts/maintainer-label-triage.prompt.md` when policy changes.
4. Validate locally:

```bash
bash -n .github/scripts/sync-labels.sh
GH_REPOSITORY=InternationalColorConsortium/iccDEV \
  .github/scripts/sync-labels.sh --dry-run
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' .github/labels.yml .github/labeler.yml
actionlint -no-color .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
zizmor .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
git diff --check
```

5. For workflow edits, also run the repository preflight gate when practical:

```bash
.github/scripts/preflight-safety-checks.sh
```

6. After merge, confirm `Sync Labels` succeeded or run it manually.
7. Before deleting a label, check open issues, open PRs, workflow references,
   prompts, and documentation; remove the label from `.github/labels.yml`, then
   delete it manually through GitHub.

## Security Guardrails

- Use `pull_request_target` only for metadata operations that do not execute PR
  content, and keep an explicit `zizmor` exception with a maintainer rationale.
- Do not checkout PR head code in label workflows.
- Keep write permissions at the job that mutates labels.
- Prefer `GH_TOKEN` over exposing `GITHUB_TOKEN` in shell steps.
- Pass GitHub expressions through `env:` before using them in shell.
- Sanitize dynamic values before writing summaries or outputs.
- Pin third-party actions to full commit SHAs.
- Keep label deletion manual and deliberate.
