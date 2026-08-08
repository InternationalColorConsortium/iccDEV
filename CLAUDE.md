# CLAUDE.md -- iccDEV

Anthropic Claude tooling (Claude Code and other agents that load `CLAUDE.md`)
should treat `AGENTS.md` as the canonical agent-instruction file for this
repository. This file is a thin mirror so Claude engages the same unified
instructions as every other agent.

GitHub Copilot recognizes `AGENTS.md`, `CLAUDE.md`, and `GEMINI.md` as
equivalent agent-instruction files. To avoid drift, iccDEV keeps one source of
truth in `AGENTS.md`; do not duplicate rules here.

## Read First

1. `AGENTS.md` -- ground rules and the navigation map.
2. `.github/copilot-instructions.md` -- cross-cutting build, test, style, CI.
3. `.github/instructions/*.instructions.md` -- path-specific rules that apply to
   the files you touch (`applyTo` globs auto-load the right file).

Reference: https://docs.github.com/en/copilot/how-tos/copilot-on-github/customize-copilot/add-custom-instructions/add-repository-instructions
