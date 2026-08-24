# Reduce Documentation Noise

Use this prompt when reviewing iccDEV documentation for signal, accuracy, and
maintainability.

## Inputs

- Target branch and commit
- Intended audience: user, contributor, maintainer, agent, or CI reviewer
- Files or directories in scope
- Any non-negotiable content that must remain

## Review Rules

- Keep one canonical source for each command, table, or policy.
- Replace duplicated details with links to the canonical source.
- Preserve exact executable names, CMake options, paths, and reproduction steps.
- For Windows MATLAB content, keep PowerShell syntax, the `repo\msvc` build
  root, MATLAB `setenv`, existing `Testing/...` smoke profiles, and required
  PAWG targets and packaged support files.
- Prefer short task-oriented docs over broad background summaries.
- Delete stale or duplicate files when their useful content is merged elsewhere.
- Do not remove safety requirements, sanitizer flags, legal requirements, or
  regression gates unless an equivalent canonical reference remains.
- Preserve `.gitattributes` line endings: LF for documentation, MATLAB, and
  Unix-facing configuration; CRLF for Windows-native PowerShell, batch, and
  command files.

## Output

1. List canonical sources created or preserved.
2. List files reduced, split, or deleted.
3. Call out behavior-affecting or accuracy-sensitive edits.
4. Run link, whitespace, ASCII, and line-ending checks before reporting
   completion.

See `docs/documentation-maintenance.md` for the repository documentation map and
review checklist.
