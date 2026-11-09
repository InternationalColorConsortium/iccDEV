---
name: maintainer-label-system
description: >
  Maintain iccDEV repository labels, path labeler rules, issue triage labels,
  PR CI status labels, and label workflow governance.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# Maintainer Label System

Use this skill when adding, removing, or auditing labels and label automation.

## Required Inputs

1. The branch or PR being changed.
2. Whether the change is taxonomy-only, path-label automation, issue triage,
   PR status labeling, or CodeQL label routing.
3. Any labels that must remain stable for existing issues, PRs, or workflows.

## Workflow

1. Read `../../../docs/label-system.md`.
2. Audit the managed manifest against live labels before changing taxonomy.
   `.github/labels.yml` is not destructive: classify undeclared live labels as
   legacy or migrate them deliberately.
3. Update `.github/labels.yml` before changing workflow or labeler behavior.
4. Add `.github/labeler.yml` rules only for deterministic file paths or branch
   names. Do not classify severity, exploitability, or maintainer judgment from
   PR text.
5. Keep privileged label workflows on trusted metadata:
   - no checkout of PR head code in `pull_request_target`;
   - explicit `zizmor` rationale for any retained `pull_request_target` labeler;
   - least-privilege job permissions;
   - pinned third-party actions;
   - no direct `${{ }}` expressions inside shell.
6. Keep issue triage and PR-status labeling independent of taxonomy
   synchronization; neither may issue manifest-wide label writes for every
   issue or PR event.
7. If issue triage logic changes, keep it conservative and make labels easy for
   maintainers to override.
8. If PR status labels change, preserve mutual exclusion among `passed`,
   `failed`, and `pending`. Require a completed successful `PR Summary` check
   run on the current head or a verified explicit commit status from a
   successful trusted `master` dispatch; lightweight checks are not validation.
   Skip unchanged labels, recheck the head before writes, and expose write
   failures. Run `python3 .github/tests/test_ci_rebase_policy.py`.
9. If PR CI control labels change, keep them maintainer-only and restricted
   to open same-repository PRs targeting `master`. Recheck the actor's current
   repository permission and event head, then dispatch `ci-pr-action` from
   the trusted `master` workflow with PR number and expected head SHA.
   `ci:run-auto`, `ci:full`, `ci:fast-lane`, and bounded option labels select
   their documented scopes. The separate `ci-codeql-security` workflow handles
   `ci:codeql` and `codeql-ready` and runs full CodeQL;
   `ci:skip` must not publish successful `PR Summary`. Clear one-shot labels
   only after their authorized dispatch or skip action succeeds.
   A same-repository `synchronize` event starts the bounded fast lane; a
   trusted metadata-only `workflow_run` refreshes status labels after
   validation. It must never check out or execute PR head content.
10. Update docs, prompts, agents, and the inventory artifact when policy or
    maintainer workflow changes.
11. For a new scope label, add its canonical `.github/labels.yml` entry before
   issue-text or path automation. Keep deletion manual after checking all active
   label consumers.

## Validation

```bash
bash -n .github/scripts/sync-labels.sh
GH_REPOSITORY=InternationalColorConsortium/iccDEV \
  .github/scripts/sync-labels.sh --dry-run
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' .github/labels.yml .github/labeler.yml
jactionlint --profile correctness --no-color .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml .github/workflows/ci-maintainer-pr-dispatch.yml .github/workflows/ci-pr-action.yml .github/workflows/ci-codeql-security.yml
zizmor .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml .github/workflows/ci-maintainer-pr-dispatch.yml .github/workflows/ci-pr-action.yml .github/workflows/ci-codeql-security.yml
python3 -m pytest .github/tests/test_ci_maintainer_label_dispatch.py
python3 .github/tests/test_ci_rebase_policy.py
python3 -m pytest .github/tests/test_ci_status_labeler_dispatch.py
git diff --check
```

For workflow governance changes, also run:

```bash
.github/scripts/preflight-safety-checks.sh
```

## Review Checklist

- `.github/labels.yml` contains every automated label.
- Managed labels are reconciled with the live inventory without deleting legacy
  labels or rewriting unchanged metadata.
- `.github/labeler.yml` paths are specific enough to avoid noisy labels.
- Large PR safeguards still exist.
- Label workflows do not execute untrusted PR content.
- Completion-triggered status labeling uses only default-branch metadata and
  verifies current-head validation before `Merge Ready`.
- Status labels remain machine-managed and mutually exclusive.
- PR CI control labels are documented, canonical in `.github/labels.yml`, and
  unavailable to fork PR fast lanes.
- `bump-sha-pins` is documented as a maintainer follow-up label for pinned
  GitHub Action, Docker, or container SHA refreshes.
- `ci:codeql` and `codeql-ready` route to the full CodeQL workflow.
- `ci:skip` cannot satisfy the protected `PR Summary` context.
- Documentation, skills, and prompts point to the same canonical files.
- The label-triage agent and inventory artifact match the managed manifest.

## References

- `../../../docs/label-system.md`
- `../../labeler.yml`
- `../../labels.yml`
- `../../workflows/pr-labeler.yml`
- `../../workflows/sync-labels.yml`
- `../../workflows/label.yml`
- `../../workflows/update-labels.yml`
- `../../workflows/ci-maintainer-pr-dispatch.yml`
- `../../workflows/ci-pr-action.yml`
- `../../workflows/ci-codeql-security.yml`
- `../../instructions/workflow-governance.instructions.md`
- `../../prompts/maintainer-label-triage.prompt.md`
