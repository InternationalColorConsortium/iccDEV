# Maintainer Label Triage

Use this prompt when reviewing or changing the iccDEV label system.

## Inputs

- Target branch or pull request:
- Label change requested:
- Affected files:
- Labels that must remain stable:
- Workflows that must keep working:

## Review Rules

- Treat labels as maintainer-owned infrastructure.
- Treat `.github/labels.yml` as the managed manifest; inventory legacy live
  labels before any migration and do not delete them implicitly.
- Keep `.github/labeler.yml` limited to deterministic path and branch rules.
- Add scope labels to `.github/labels.yml` before issue-text or path automation;
  retain manual deletion after checking open label consumers.
- Do not infer severity or exploitability from issue or PR wording without
  maintainer review.
- Preserve `passed`, `failed`, and `pending` as mutually exclusive CI status
  labels.
- Preserve `Merge Ready` as a derived status, not a manual approval substitute.
- A pushed or rebased same-repository `master`-target PR starts a bounded
  `ci-pr-action` fast lane; completion refreshes status labels. Verify the
  current head, `PR Summary`, labels, and merge state before reporting
  readiness. Use trusted-`master` exact-head dispatch and manual label
  refresh only when an automatic run is absent. `Rebase Safety` alone is not
  PR validation.
- Keep `ci:codeql` and `codeql-ready` connected to the full CodeQL workflow.
- Keep all `ci:*` control labels maintainer-only and restricted to open
  same-repository `master`-target PRs. A trusted metadata-only dispatcher
  must use the exact event head and trusted `master` workflow. `ci:skip`
  must never publish successful validation.
- Do not checkout or execute PR-controlled content from `pull_request_target`.
- Require an explicit `zizmor` rationale when retaining `pull_request_target`
  for metadata-only label automation.
- Keep issue triage and PR-status labeling independent of taxonomy
  synchronization. Sync must fetch the live inventory once and update only
  missing or drifted managed labels.

## Output

1. Label taxonomy changes and why each label is needed.
2. Path labeler changes and the exact files or branch patterns they match.
3. Workflow permission or trigger changes.
4. Validation commands run and their results.
5. Any labels that need manual migration or deletion after merge.
6. Managed-versus-legacy inventory and expected label API writes.

## Validation Commands

```bash
bash -n .github/scripts/sync-labels.sh
GH_REPOSITORY=InternationalColorConsortium/iccDEV \
  .github/scripts/sync-labels.sh --dry-run
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' .github/labels.yml .github/labeler.yml
jactionlint --profile correctness --no-color .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
zizmor .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
git diff --check
```

See `docs/label-system.md` and
`docs/label-inventory-audit.md` and
`.github/skills/maintainer-label-system/SKILL.md` for the maintainer workflow.
