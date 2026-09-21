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
6. For issues labeled `CodeQL`, distinguish an alert-only report from completed
   technical triage. Keep `needs-triage` when there is no parser-to-tool
   reachability evidence or false-positive rationale. Accept a false-positive
   classification only when it names the guarding setup method, shows failure
   propagation, and retains an unguarded query-test control.
7. For SBO/HBO alerts, require the allocation count, access count, and boundary
   equality to be recorded together. For accuracy or precision alerts, require
   a named ICC encoding or formula plus an executable expected-value oracle;
   generic floating-point suspicion is not completed technical triage.

Return evidence and recommendations only. Do not edit labels or workflows.
