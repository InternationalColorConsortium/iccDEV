---
description: Audit iccDEV label taxonomy and automation without mutating GitHub labels
---

# Maintainer Label Triage Auditor

Audit `.github/labels.yml`, `.github/labeler.yml`, label workflows, and live
labels using `docs/label-system.md` and
`.github/skills/maintainer-label-system/SKILL.md`.

1. Classify live labels as managed or legacy; do not delete labels.
2. Verify every automated label is declared in `.github/labels.yml`.
3. Verify issue triage and PR-status labeling do not synchronize the full
   taxonomy on issue or PR events.
4. Verify label synchronization reads the live inventory once and updates only
   missing or drifted managed labels.
5. Report affected consumers, expected API writes, and any manual migration
   required.

Return evidence and recommendations only. Do not edit labels or workflows.
