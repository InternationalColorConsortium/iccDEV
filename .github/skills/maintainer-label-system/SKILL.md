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
2. Update `.github/labels.yml` before changing workflow or labeler behavior.
3. Add `.github/labeler.yml` rules only for deterministic file paths or branch
   names. Do not classify severity, exploitability, or maintainer judgment from
   PR text.
4. Keep privileged label workflows on trusted metadata:
   - no checkout of PR head code in `pull_request_target`;
   - explicit `zizmor` rationale for any retained `pull_request_target` labeler;
   - least-privilege job permissions;
   - pinned third-party actions;
   - no direct `${{ }}` expressions inside shell.
5. If issue triage logic changes, keep it conservative and make labels easy for
   maintainers to override.
6. If PR status labels change, preserve mutual exclusion among `passed`,
   `failed`, and `pending`.
7. Update docs and prompts when policy or maintainer workflow changes.

## Validation

```bash
bash -n .github/scripts/sync-labels.sh
GH_REPOSITORY=InternationalColorConsortium/iccDEV \
  .github/scripts/sync-labels.sh --dry-run
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' .github/labels.yml .github/labeler.yml
actionlint -no-color .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
zizmor .github/workflows/pr-labeler.yml .github/workflows/sync-labels.yml .github/workflows/label.yml .github/workflows/update-labels.yml
git diff --check
```

For workflow governance changes, also run:

```bash
.github/scripts/preflight-safety-checks.sh
```

## Review Checklist

- `.github/labels.yml` contains every automated label.
- `.github/labeler.yml` paths are specific enough to avoid noisy labels.
- Large PR safeguards still exist.
- Label workflows do not execute untrusted PR content.
- Status labels remain machine-managed and mutually exclusive.
- `codeql-ready` still routes to the full CodeQL workflow.
- Documentation, skills, and prompts point to the same canonical files.

## References

- `../../../docs/label-system.md`
- `../../labeler.yml`
- `../../labels.yml`
- `../../workflows/pr-labeler.yml`
- `../../workflows/sync-labels.yml`
- `../../workflows/label.yml`
- `../../workflows/update-labels.yml`
- `../../instructions/workflow-governance.instructions.md`
- `../../prompts/maintainer-label-triage.prompt.md`
