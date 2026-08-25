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
   For Windows MATLAB documentation, preserve PowerShell syntax, the
   `repo\msvc` build root, MATLAB `setenv`, real `Testing/...` smoke profiles,
   and the PAWG tool, native contract target, and staged support-file list.
5. Replace repeated detail with links to canonical sources.
6. Keep Doxygen-friendly links within the scanned INPUT tree; for repo-root or
   example links that do not exist under Doxygen's configured input, use an
   explicit HTML anchor (`<a href="...">...</a>`) instead of a Markdown link that
   triggers warning-only docs failures.
7. Delete duplicate files only after moving unique content elsewhere.
8. Run whitespace, local-link, ASCII, line-ending, and stale-reference checks.
   Run `doxygen .github/ci/doxygen/Doxyfile` and require
   `docs/generated/doxygen-warnings.log` to be empty. Do not add generated
   Doxygen HTML or warning logs to the change.
   Require LF for documentation, prompts, skills, MATLAB, CMake, YAML/JSON,
   Dockerfiles, and Unix scripts; preserve CRLF for `.ps1`, `.bat`, and `.cmd`.

## Harvest Rules

- Import reusable process, not repository-specific paths or private scratch
  locations.
- Prefer a short prompt or maintenance doc over another long reference file.
- Keep user-facing docs task-oriented; keep agent rules in `.github/`.

## References

- `../../../docs/documentation-maintenance.md`
- `../../prompts/reduce-doc-noise.prompt.md`
- `../../copilot-instructions.md`
