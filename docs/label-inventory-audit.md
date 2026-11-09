# Label Inventory Audit

Use this artifact before changing the managed label manifest.

```text
branch or pull request:
managed manifest source: .github/labels.yml
live inventory command: gh label list --repo OWNER/REPOSITORY --limit 200
managed labels missing live:
legacy live labels:
automated consumers checked:
manual migration or deletion plan:
expected label API writes:
```

## Issue #3046 snapshot, 2026-11-08

```text
branch: ci-qa-pr-docker-testing (local staging branch ci-qa-pr-3045-scan-build-labels)
managed manifest source: .github/labels.yml
live inventory command: gh label list --repo InternationalColorConsortium/iccDEV --limit 500 --json name
managed labels: 58
live labels: 145
managed labels missing live: CI, ci:codeql, question
legacy live labels: 90 (retained; no deletion requested)
automated consumers checked: ci-maintainer-pr-dispatch.yml, ci-pr-action.yml,
  ci-codeql-security.yml, ci-docker-pr.yml, update-labels.yml, sync-labels.yml
manual migration or deletion plan: none; keep codeql-ready as an existing alias
expected label API writes after merge: create the three missing managed labels;
  sync-labels.sh may also update any color or description drift it detects
```

The count is a live-versus-branch snapshot, not a request to delete legacy
labels. No label synchronization or remote CI was started for this audit.

## PR #3050 Docker-label retirement

The maintainer chose to retain the existing manual `ci-docker.yml` workflow
and retire the duplicate PR Docker workflow and `ci:docker` control label.
The managed manifest count changes from 58 to 57. The earlier snapshot above
remains a historical record. `ci:docker` already exists in the live repository;
label sync does not delete legacy labels, so a maintainer should remove that
live label after this PR merges and the default-branch manifest no longer
declares it.
