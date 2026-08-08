# AGENTS.md -- iccDEV

This file is a short navigation aid for automated coding agents and maintainers.
Detailed rules live in `.github/copilot-instructions.md` and
`.github/instructions/*.instructions.md`.

`AGENTS.md` is the canonical agent-instruction file. GitHub Copilot also
recognizes `CLAUDE.md` (Anthropic Claude) and `GEMINI.md` (Google Gemini) at the
repository root; both are thin mirrors that route back here so every agent
shares one source of truth. Update rules here, not in the mirrors.

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
| Pull request preparation and handoff | `docs/pre-pr-security-cycle.md` |
| Pre-PR security skill | `.github/skills/pre-pr-security-cycle/SKILL.md` |
| Pre-PR security prompt | `.github/prompts/pre-pr-security-cycle.prompt.md` |
| Regression bisect workflow | `.github/prompts/bisect-regression.prompt.md` |
| Maintainer regression container | `docs/regression-container.md` |
| Regression container prompt | `.github/prompts/regression-container-maintainer.prompt.md` |
| Security repro | `.github/prompts/reproduce-security-issue.prompt.md` |
| Issue filing format | `.github/prompts/file-security-issue.prompt.md` |
| Library hardening | `.github/instructions/icc-library-code.instructions.md` |
| Workflow hardening | `.github/instructions/workflow-governance.instructions.md` |
| Maintainer label system | `docs/label-system.md` |
| Label triage prompt | `.github/prompts/maintainer-label-triage.prompt.md` |
| Testing details | `.github/instructions/testing.instructions.md` |
| Python bindings | `.github/instructions/python-bindings.instructions.md` |
| MATLAB MEX bindings | `.github/instructions/matlab-mex.instructions.md` |
| MATLAB build and QA | `docs/matlab-bindings.md` |
| MATLAB binding tests | `.github/skills/matlab-bindings-test/SKILL.md` |
| MATLAB binding debug | `.github/prompts/debug-matlab-bindings.prompt.md` |
| Python binding tests | `.github/skills/python-bindings-test/SKILL.md` |
| WASM build tests | `.github/skills/wasm-build-test/SKILL.md` |
| MCP subprocess debug | `.github/prompts/debug-mcp-subprocess.prompt.md` |
| Python/Cython debug | `.github/prompts/debug-python-bindings.prompt.md` |
| Documentation maintenance | `docs/documentation-maintenance.md` |

## WASM Scope

WASM now ships as a staged Node/npm-style module package. Do not expect or
restore legacy `wasm/*.html`, `wasm/*.css`, or `wasm/*.js` browser UI assets.
Use `.github/skills/wasm-build-test/SKILL.md` for the current module smoke and
profile parity checks.
