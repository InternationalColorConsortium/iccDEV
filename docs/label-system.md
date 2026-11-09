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
| `.github/workflows/ci-maintainer-pr-dispatch.yml` | Dispatches exact-head validation from maintainer CI labels. |
| `.github/workflows/ci-codeql-security.yml` | Runs full CodeQL when `ci:codeql` or `codeql-ready` is applied. |

## Label Classes

| Class | Labels | Owner |
|-------|--------|-------|
| Issue triage | `needs-triage`, `needs-repro`, `requires:more-information` | Maintainers |
| Issue kind | `bug`, `feature`, `question`, `security` | Maintainers |
| PR status | `passed`, `failed`, `pending`, `Merge Ready` | CI |
| PR CI control | `ci:run-auto`, `ci:full`, `ci:fast-lane`, `ci:ctest-last-1` through `ci:ctest-last-10`, `ci:no-windows`, `ci:warnings-fail`, `ci:skip`, `ci:codeql`, `codeql-ready` | Maintainers |
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

`update-labels.yml` evaluates open PRs on PR events, completion of the
PR validation workflows, schedule, and manual dispatch. It keeps `passed`,
`failed`, and `pending` mutually exclusive. A completed successful
`PR Summary` check run or verified Actions commit status on the current PR
head is required. The status fallback supports validation dispatched from the
trusted `master` workflow: its check run belongs to `master`, while its
explicit status belongs to the validated PR SHA. Lightweight checks, a stale
status, or an unverified status cannot substitute. Missing, skipped, or
neutral validation remains `pending`; failed validation remains `failed`.
The labeler reads the current-head check runs and statuses without launching
validation, skips unchanged writes, and rechecks the PR head before mutation.
Failed API reads or writes do not produce a success label.
It adds `Merge Ready` only when CI is successful, the PR is not draft, and the
merge state is clean. A review or approval is not required. It does not
synchronize the label manifest; the dedicated taxonomy-sync workflow
provisions managed labels.

Do not use these status labels as the only merge gate. Branch protection and
required checks remain authoritative.

### PR CI Scope

`ci-maintainer-pr-dispatch.yml` handles the one-shot maintainer-applied CI
control labels on an open same-repository PR targeting `master`. The separate
`ci-codeql-security.yml` handles `ci:codeql` and `codeql-ready`. Each workflow
checks the label actor's
current write, maintain, or admin permission and rechecks the event head.
Validation dispatches use the trusted `master` workflow definition with the
PR number and exact expected head SHA; the downstream workflow resolves that
PR through GitHub's API before testing its source. A stale request fails
without publishing success for the new head.

| Applied label | Action |
|---------------|--------|
| `ci:run-auto` | Dispatch path-scoped `ci-pr-action` validation. |
| `ci:full` | Dispatch the long-cycle `ci-pr-action` matrix. |
| `ci:fast-lane`, `ci:ctest-last-1` through `ci:ctest-last-10`, `ci:no-windows`, `ci:warnings-fail` | Dispatch bounded fast-lane validation with the selected settings. |
| `ci:skip` | Record a pending `PR Summary` and dispatch no validation. It never satisfies the required success context or `Merge Ready`. |
| `ci:codeql`, `codeql-ready` | Trigger the separate full CodeQL workflow on the open PR under the maintainer label guard. Keep the applied label until its current-head check succeeds or the maintainer removes the request. |

The dispatcher records pending status, then dispatches a focused trusted
`master`-ref status-label refresh for the exact PR head, including for
`ci:skip`. This promptly clears stale `passed` or `Merge Ready` labels while
validation is pending. It clears a one-shot control label only after its
dispatch succeeds. Path and derived status labels never dispatch CI.

Opened pull requests and manual dispatches default to `ci_scope=auto`;
`synchronize` runs the bounded fast lane. In auto scope,
the native matrix runs only for owned library/tool source and resources,
`Testing/`, registered native regression sources and fixtures, or supported
build entry points under `Build/Cmake`, `Build/AppleMobile`, and `Build/XCode`;
documentation-only changes use the constrained fast-lane settings, and
workflow-only changes receive the preflight and workflow-security gates.
Container-only changes use workflow-security gates and local container
validation.

Shared HEIF carriers under `.github/ci/test-data/heif/` select the native
testing surface. The table-driven contract is
`.github/tests/test-ci-pr-path-classifier.sh`.
Dispatch `ci_scope=full` explicitly for a long-cycle matrix. For the shortest
same-repository PR lane, provide an open `pr_number`, choose
`ci_scope=fast-lane`, and set `ctest_recent_limit`, `include_windows`,
and `warning_policy` on that dispatch. Fast lane defaults to the latest
registered CTest and strict warning failure. Windows is opt-in. The PR
orchestrator does not run a Docker verification job.

An opened same-repository PR targeting `master` uses path-scoped `auto`.
Each later push or rebase emits `synchronize` and starts `ci-pr-action` with
bounded `fast-lane` settings alongside `ci-rebase-safety`. The fast lane
publishes `PR Summary` for the exact PR head. A completed validation run
triggers the trusted status labeler automatically. The labeler runs from the
default branch, reads API metadata only, and never checks out or executes PR
head content. It also refreshes after CodeQL and PR Docker validation.

For a missing automatic run or an explicit retry, dispatch the trusted
`master` workflow using the PR number and exact current head SHA. The
workflow resolves that SHA through GitHub's API and rejects a changed head:

```bash
gh pr view <pr-number> --repo InternationalColorConsortium/iccDEV \
  --json state,isCrossRepository,baseRefName,headRefName,headRefOid
gh workflow run ci-pr-action --repo InternationalColorConsortium/iccDEV \
  --ref master -f ci_scope=auto -f pr_number=<pr-number> \
  -f expected_head_sha=<40-character-head-sha>
gh run watch <run-id> --repo InternationalColorConsortium/iccDEV --exit-status
```

The dispatch run's `headSha` is `master` because that ref selects the trusted
workflow definition. Its finalizer publishes an explicit `PR Summary` commit
status on the resolved PR head. Check that status and the PR's current head;
do not substitute the run's `headSha` for the validated source SHA.

If the completion-triggered labeler run is absent, dispatch it manually on
`master`, watch it, and inspect the derived labels:

```bash
gh workflow run update-labels.yml --repo InternationalColorConsortium/iccDEV \
  --ref master
gh run list --repo InternationalColorConsortium/iccDEV \
  --workflow update-labels.yml --branch master --event workflow_dispatch \
  --json databaseId,createdAt,status,conclusion,url --limit 5
gh run watch <labeler-run-id> --repo InternationalColorConsortium/iccDEV \
  --exit-status
gh pr view <pr-number> --repo InternationalColorConsortium/iccDEV \
  --json headRefOid,isDraft,mergeStateStatus,labels \
  --jq '{headRefOid,isDraft,mergeStateStatus,labels:[.labels[].name]}'
```

Capture the labeler run ID from the dispatch URL, or identify that dispatch
by URL and creation time in `gh run list`. Confirm terminal success and that
`headRefOid` still matches the validated SHA.
`Merge Ready` is derived only when validation is `passed`, the PR is not a
draft, and `mergeStateStatus` is `CLEAN`; report the actual labels and merge
state if those conditions are not met. A successful CI run alone does not
guarantee `Merge Ready`.

The labeler never launches validation. Automatic completion refresh and the
manual fallback avoid waiting for the four-hour schedule. The final
`ci_scope=full` gate remains the readiness check described in
`pre-pr-security-cycle.md`.

For an upper stacked PR whose base is another branch, trusted PR-number
dispatch rejects the non-`master` base. Branch-only screening can provide
head evidence, but it does not API-resolve the upper PR/base and must not be
reported as PR API-synced merge readiness. After the bottom PR merges, run
`gh stack sync --remote origin`, confirm the upper PR's new base and head
through `gh pr view`, then validate its newly `master`-based current head.

Manual and label-triggered dispatches use an event- and PR-qualified
concurrency group. One PR's dispatch cannot cancel another PR's validation.
Inspect a dispatched run by its run ID; its check run is attached to the
trusted `master` ref, while its `PR Summary` status is on the PR head.
For an owner-visible long-cycle build inventory, dispatch either
`ci-qa-pr` (including Windows) or `CI Build Test (No
Fuzzers)` (without Windows). Both callers use `_build-matrix.yml`, which
centralizes the current reusable Unix and Windows gates plus focused sanitizer,
option, version-header, and clean-rebuild lanes.

Container-surface changes still need local canonical-image validation. The
existing `ci-docker.yml` workflow is a separate manual image validation and
release action; it publishes only from approved refs.

### Required Check Policy

Branch rules require stable aggregate contexts rather than mode-specific build
job names. This lets full, fast-lane, auto, governance, and docs runs use one
fail-closed policy:

| Required context | Purpose |
|------------------|---------|
| `PR Summary` | Aggregates selected validation lanes for the exact PR head; `synchronize` runs the bounded fast lane. |

Do not require individual Unix, GCC 15.2, tool-test, or Windows job names in
branch rules. The selected `ci-pr-action` jobs are aggregated by `PR Summary`.
A `synchronize` event runs `Rebase Safety` and the bounded `ci-pr-action` fast
lane, while final full validation remains explicit. A maintainer records the
applicable final-lane evidence for the rebased head before merge. The summary must treat
failed or cancelled detection, setup, and input-validation prerequisites as
failures; only intentionally skipped mode-specific jobs are acceptable.

`ci-pr-action` runs automatically only for pull requests targeting `master`.
Its PR-number workflow dispatch also accepts only open, same-repository pull
requests targeting `master`. Integration branches are outside this workflow's
required-check contract: their rulesets must not require these aggregate PR
contexts and should use branch-owned validation when needed. WASM parity
remains a `master`-only required context.

In the default `ci-pr-action` path, container-surface changes select
workflow-security gates without building an image in that job.

### CodeQL Ready

`ci-codeql-security.yml` uses `ci:codeql` or the existing `codeql-ready`
label to trigger full CodeQL for an open same-repository PR when a maintainer
applies it. The status labeler requires the selected CodeQL result on the
current head before `Merge Ready`. Labelling a merged PR
does not rerun CodeQL: the push trigger already analyzes `master`, and a
label-triggered run would use the PR base commit's trusted query set while
publishing its results against `master`. Closed unmerged PRs are also ignored:
they are not active review targets and must be reopened before CodeQL can run.
Use a CodeQL trigger label when a change touches
C/C++, CMake, CodeQL query logic, parser hardening, or security-sensitive
automation and the fast preflight checks are not enough.

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
jactionlint --profile correctness --no-color .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml \
  .github/workflows/label.yml .github/workflows/update-labels.yml \
  .github/workflows/ci-maintainer-pr-dispatch.yml .github/workflows/ci-pr-action.yml \
  .github/workflows/ci-codeql-security.yml
zizmor .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml \
  .github/workflows/label.yml .github/workflows/update-labels.yml \
  .github/workflows/ci-maintainer-pr-dispatch.yml .github/workflows/ci-pr-action.yml \
  .github/workflows/ci-codeql-security.yml
python3 -m pytest .github/tests/test_ci_maintainer_label_dispatch.py
python3 .github/tests/test_ci_rebase_policy.py
python3 -m pytest .github/tests/test_ci_status_labeler_dispatch.py
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
- Keep the `workflow_run` status refresh metadata-only on the trusted default
  branch. It may read Actions and PR API state but must not check out or run
  the completed workflow's PR head.
- Do not checkout PR head code in label workflows.
- Keep write permissions at the job that mutates labels.
- Prefer `GH_TOKEN` over exposing `GITHUB_TOKEN` in shell steps.
- Pass GitHub expressions through `env:` before using them in shell.
- Sanitize dynamic values before writing summaries or outputs.
- Pin third-party actions to full commit SHAs.
- Keep label deletion manual and deliberate.
