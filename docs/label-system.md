# Maintainer Label System

Use this guide when changing iccDEV labels, issue triage, pull request path
labels, or CI status labels. Labels are maintainer-owned infrastructure because
they affect review routing, security handling, CI visibility, and merge
readiness.

## Canonical Files

| File | Purpose |
|------|---------|
| `.github/labels.yml` | Canonical label names, colors, and descriptions. |
| `.github/labeler.yml` | Deterministic pull request path-to-label rules. |
| `.github/scripts/sync-labels.sh` | Local and CI label synchronization helper. |
| `.github/workflows/sync-labels.yml` | Applies `.github/labels.yml` on `master` or manual dispatch. |
| `.github/workflows/pr-labeler.yml` | Applies path labels to pull requests. |
| `.github/workflows/label.yml` | Adds first-pass issue triage labels and welcome guidance. |
| `.github/workflows/update-labels.yml` | Adds PR CI status labels: `passed`, `failed`, `pending`, `Merge Ready`. |
| `.github/workflows/ci-codeql-security.yml` | Runs full CodeQL when the `codeql-ready` label is applied. |

## Label Classes

| Class | Labels | Owner |
|-------|--------|-------|
| Issue triage | `needs-triage`, `needs-repro`, `requires:more-information` | Maintainers |
| Issue kind | `bug`, `feature`, `question`, `security` | Maintainers |
| PR status | `passed`, `failed`, `pending`, `Merge Ready`, `codeql-ready` | CI and maintainers |
| Scope | `Source`, `Documentation`, `Configuration`, `CI`, `Build`, `Testing`, `Tools`, `JSON`, `WASM`, `vcpkg`, and related area labels | Path labeler and maintainers |
| Governance | `Governance`, `Labels`, `security`, `SAST`, `CodeQL`, `Sanitizers`, `Release` | Maintainers |

Keep existing public label names stable unless maintainers intentionally migrate
them. New workflow or status labels should be lower-case hyphenated, except
when matching an established GitHub convention or existing repository label.
New area labels should match the current Title Case scope style.

## Workflow Behavior

### Label Sync

`sync-labels.yml` runs on pushes to `master` that touch label taxonomy files,
and it can be started manually with `workflow_dispatch`. The workflow grants
only `issues: write` and `contents: read`, checks out only the taxonomy and sync
script, and calls:

```bash
.github/scripts/sync-labels.sh
```

The script creates missing labels and updates existing color or description
metadata. It does not delete labels. Retire labels manually only after checking
open issues, open PRs, workflow references, prompts, and documentation.

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

### Issue Triage

`label.yml` runs on issue open, edit, and reopen events. It syncs the repository
label inventory, then adds `needs-triage` and lightweight issue-kind labels from
the issue title and body. It may also add `needs-repro` or
`requires:more-information` when a report is too short, contains a placeholder,
or describes a crash without a reproducible input or command.

The triage workflow is a routing aid, not a maintainer decision. Maintainers may
adjust labels after reviewing reproductions, security impact, and required test
coverage.

### PR CI Status Labels

`update-labels.yml` evaluates open PRs on schedule, PR events, and manual
dispatch. It keeps `passed`, `failed`, and `pending` mutually exclusive. It adds
`Merge Ready` only when CI is successful, the PR is not draft, the merge state is
clean, and the review decision is approved.

Do not use these status labels as the only merge gate. Branch protection and
required checks remain authoritative.

### CodeQL Ready

`ci-codeql-security.yml` uses the `codeql-ready` label to trigger the full
CodeQL security workflow for a PR. Use this label when a change touches C/C++,
CMake, CodeQL query logic, parser hardening, or security-sensitive automation
and the fast preflight checks are not enough.

## Maintainer Change Process

1. Add or edit labels in `.github/labels.yml` first.
2. Add `.github/labeler.yml` path rules only for deterministic scope labels.
3. Update this guide, `.github/skills/maintainer-label-system/SKILL.md`, or
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
