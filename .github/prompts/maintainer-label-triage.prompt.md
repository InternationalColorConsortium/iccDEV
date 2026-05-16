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
- Keep `.github/labels.yml` as the canonical inventory.
- Keep `.github/labeler.yml` limited to deterministic path and branch rules.
- Do not infer severity or exploitability from issue or PR wording without
  maintainer review.
- Preserve `passed`, `failed`, and `pending` as mutually exclusive CI status
  labels.
- Preserve `Merge Ready` as a derived status, not a manual approval substitute.
- Keep `codeql-ready` connected to the full CodeQL workflow.
- Do not checkout or execute PR-controlled content from `pull_request_target`.
- Require an explicit `zizmor` rationale when retaining `pull_request_target`
  for metadata-only label automation.

## Output

1. Label taxonomy changes and why each label is needed.
2. Path labeler changes and the exact files or branch patterns they match.
3. Workflow permission or trigger changes.
4. Validation commands run and their results.
5. Any labels that need manual migration or deletion after merge.

## Validation Commands

```bash
bash -n .github/scripts/sync-labels.sh
GH_REPOSITORY=InternationalColorConsortium/iccDEV \
  .github/scripts/sync-labels.sh --dry-run
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' .github/labels.yml .github/labeler.yml
actionlint -no-color .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
zizmor .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
git diff --check
```

See `docs/label-system.md` and
`.github/skills/maintainer-label-system/SKILL.md` for the maintainer workflow.
