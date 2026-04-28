---
name: docs-maintenance
description: >
  Review and edit iccDEV documentation for signal, accuracy, canonical
  ownership, and low-noise handoff.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# Documentation Maintenance

Use this skill when reducing duplicated docs, adding routing docs, or reviewing
documentation for accuracy.

## Workflow

1. Confirm the target branch and whether changes are local-only.
2. Read `../../../docs/documentation-maintenance.md` for canonical sources.
3. Inventory duplicated or stale content before editing.
4. Preserve exact commands, paths, CMake options, executable names, and safety
   requirements.
5. Replace repeated detail with links to canonical sources.
6. Delete duplicate files only after moving unique content elsewhere.
7. Run whitespace, local-link, ASCII, and stale-reference checks.

## Harvest Rules

- Import reusable process, not repository-specific paths or private scratch
  locations.
- Prefer a short prompt or maintenance doc over another long reference file.
- Keep user-facing docs task-oriented; keep agent rules in `.github/`.

## References

- `../../../docs/documentation-maintenance.md`
- `../../prompts/reduce-doc-noise.prompt.md`
- `../../copilot-instructions.md`
