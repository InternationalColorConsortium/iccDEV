# AGENTS.md -- iccDEV

This file is a short navigation aid for automated coding agents and maintainers.
Detailed rules live in `.github/copilot-instructions.md` and
`.github/instructions/*.instructions.md`.

## Ground Rules

- Check `git --no-pager status --short --branch` before edits.
- Prefer focused changes with exact repros and regression coverage.
- Use 2-space C++ indentation, K&R braces, `m_` members, and return-value errors.
- Exit 1-127 is graceful failure. Exit 128+ is signal termination.
- Use sanitizer builds for bug hunting; see `.github/instructions/build-system.instructions.md`.
- Add the nearest regression test for behavior fixes.

## Navigation

| Need | File |
|------|------|
| Build, test, style, CI | `.github/copilot-instructions.md` |
| Regression bisect workflow | `.github/prompts/bisect-regression.prompt.md` |
| Security repro | `.github/prompts/reproduce-security-issue.prompt.md` |
| Issue filing format | `.github/prompts/file-security-issue.prompt.md` |
| Library hardening | `.github/instructions/icc-library-code.instructions.md` |
| Workflow hardening | `.github/instructions/workflow-governance.instructions.md` |
| Maintainer label system | `docs/label-system.md` |
| Label triage prompt | `.github/prompts/maintainer-label-triage.prompt.md` |
| Testing details | `.github/instructions/testing.instructions.md` |
