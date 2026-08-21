# Copilot Instructions for iccDEV

Use this as the short cross-cutting guide. Path-specific details auto-load from
`.github/instructions/*.instructions.md`; prefer those files over duplicating
commands here.

## Path-Specific Instructions

| Pattern | File | Purpose |
|---------|------|---------|
| `.github/workflows/**` | `instructions/workflow-governance.instructions.md` | Shell hardening, injection prevention, output sanitization |
| `.github/labels.yml`, `.github/labeler.yml` | `instructions/workflow-governance.instructions.md` | Maintainer-owned label automation and trusted write workflows |
| `.github/scripts/**` | `instructions/sanitizer-scripts.instructions.md` | `sanitize-sed.sh` and `sanitize.ps1` APIs |
| `Build/**` | `instructions/build-system.instructions.md` | CMake, platform notes, sanitizer options, WASM/LTO |
| `IccProfLib/**`, `IccXML/**`, `Tools/**`, `IccJSON/**`, `IccConnect/**` | `instructions/icc-library-code.instructions.md` | Parser hardening and C++ safety patterns |
| `Testing/**` | `instructions/testing.instructions.md` | Test scripts, profile directories, regression flow |
| `IccProfLib/icProfileHeader.h` | `instructions/icc-specification.instructions.md` | ICC header, tag, and color-space rules |
| `ports/**` | `instructions/vcpkg-port.instructions.md` | vcpkg overlay port and CI |
| `python/**` | `instructions/python-bindings.instructions.md` | Cython build, tests, and `ICCDEV_BUILD_DIR` |
| `matlab/**` | `instructions/matlab-mex.instructions.md` | MEX gateway, OOP layer, and `build_mex.m` |
| `Tools/Winnt/IccIisIsapi/**` | `Tools/Winnt/IccIisIsapi/isapi-instructions.md` | IIS ISAPI setup and hardening |

## Agent Instructions

This repository uses all three GitHub custom-instruction types:

- Repository-wide: this file (`.github/copilot-instructions.md`).
- Path-specific: `.github/instructions/*.instructions.md`, auto-loaded by
  `applyTo` globs.
- Agent: `AGENTS.md` is canonical, and the nearest `AGENTS.md` in the tree
  wins. Root `CLAUDE.md` (Anthropic Claude) and `GEMINI.md` (Google Gemini) are
  thin mirrors that route back to `AGENTS.md` so every agent shares one source
  of truth.

Keep agent rules in `AGENTS.md`, not in the mirrors. See
https://docs.github.com/en/copilot/how-tos/copilot-on-github/customize-copilot/add-custom-instructions/add-repository-instructions

## Current JSON/Config Regression Gate

JSON/config parser fixes must fail closed: reject short arrays, non-numeric
required values, failed nested `ParseJson()`, bad struct members, malformed
fixed-size arrays, and stale reset/fromJson state.

Canonical regression scripts:

- `.github/scripts/iccdev-json-parser-regression-tests.sh`
- `.github/scripts/iccdev-json-cfg-tests.sh`
- `.github/scripts/iccdev-stdobserver-regression-tests.sh`
- `.github/scripts/iccdev-mluc-setter-regression-tests.sh`
- `.github/scripts/iccdev-mluc-read-utf16-regression-tests.sh`
- `.github/scripts/iccdev-cam-degenerate-regression-tests.sh`
- `.github/scripts/iccdev-namedcolor-apply-regression-tests.sh`
- `.github/scripts/iccdev-v5-namedcmm-regression-tests.sh`

For regression workflow updates, use
`.github/skills/regression-workflow-governance/SKILL.md` and
`docs/regression-workflow-governance.md`.

For maintainer regression-container use, PR validation, and issue reproduction,
use `.github/skills/regression-container-maintainer/SKILL.md` and
`docs/regression-container.md`.

## Build, Test, and Safety

- User build instructions: `docs/build.md`
- Maintainer build and sanitizer policy: `.github/instructions/build-system.instructions.md`
- Test profile workflow: `.github/instructions/testing.instructions.md`
- Workflow governance: `.github/instructions/workflow-governance.instructions.md`
- Windows session helper: dot-source `.\.github\scripts\icc-session.ps1`, then
  run `icc-session` to create the next `DD-mmm-YYYY-NNN` workspace directory.

## Pull Requests

- Follow `CONTRIBUTING.md`: discuss the change in an issue first, keep the PR
  focused, and do not modify maintainer-owned infrastructure unless a
  maintainer requested it.
- Before opening, updating, or finalizing a PR, use the pre-PR security cycle:
  `docs/pre-pr-security-cycle.md`,
  `.github/skills/pre-pr-security-cycle/SKILL.md`, and
  `.github/prompts/pre-pr-security-cycle.prompt.md`.
- Run the smallest complete validation for the changed surface. For workflow,
  Docker, release, CMake, parser, or security automation changes, follow the
  additional checks required by the path-specific instructions and pre-PR
  security cycle.
- PR handoffs must state the branch and commit, changed surface, local and
  hosted validation, known skips or deferrals, and merge-readiness. Cite
  workflow URLs or run IDs rather than copying raw logs.
- Do not call a PR ready or green while required `ci-pr-action`,
  `ci-preflight-safety`, or `ci-pr-risk-security-analysis` checks are pending,
  skipped unexpectedly, or failing.

## iccdev-mcp Review Notes

- Keep user-facing MCP setup docs task-oriented; put reviewer and agent guidance
  in `.github/` files or prompts.
- When reviewing repository MCP settings, compare the `mcpServers.iccdev.tools`
  allowlist against the MCP tool names from `iccdev_mcp/server.py`, not the REST
  route aliases from `/api/tools`. REST names such as `sig_to_str`,
  `list_profiles`, `to_xml`, `from_xml`, and `spec_sep` intentionally differ
  from MCP names such as `icc_sig_to_str`, `list_available_profiles`,
  `profile_to_xml`, `xml_to_profile`, and `spec_sep_to_tiff`.
- For `profile_summary`, keep raw ICC signature fields unchanged and expose
  printable `*_display` fields only for UI readability. ICC signature values may
  arrive as either 4-character strings or raw 32-bit integers, so both forms need
  test coverage.
- Dashboard changes should preserve stale-output reset when switching tools.

Key safety rules:

- 2-space C++ indentation, K&R braces, no tabs.
- Member prefix `m_`; match nearby naming and ownership patterns.
- Error handling uses return values, not exceptions.
- New files need ICC copyright and BSD 3-Clause header unless generated.
- Validate all file-controlled sizes, offsets, counts, and loop bounds.
- Bounds check pattern: `if (size > limit || offset > limit - size)`.
- Exit 1-127 is graceful failure, not a crash; exit 128+ is signal termination.

## Prompts

| Task | Prompt |
|------|--------|
| Bisect regression | `.github/prompts/bisect-regression.prompt.md` |
| Reproduce security issue | `.github/prompts/reproduce-security-issue.prompt.md` |
| File issue | `.github/prompts/file-security-issue.prompt.md` |
| Code review hunting | `.github/prompts/code-review-hunting.prompt.md` |
| Reduce documentation noise | `.github/prompts/reduce-doc-noise.prompt.md` |
| Build/test/coverage | `.github/prompts/build-and-test.prompt.md` |
| Regression workflow gate | `.github/prompts/add-regression-workflow.prompt.md` |
| Maintainer regression container | `.github/prompts/regression-container-maintainer.prompt.md` |
| Workflow governance audit | `.github/prompts/audit-workflow-governance.prompt.md` |
| Pre-PR security cycle | `.github/prompts/pre-pr-security-cycle.prompt.md` |
| Maintainer label triage | `.github/prompts/maintainer-label-triage.prompt.md` |
| vcpkg debug | `.github/prompts/vcpkg-port-debug.prompt.md` |
| Debug MCP subprocess | `.github/prompts/debug-mcp-subprocess.prompt.md` |
| Debug Python/Cython bindings | `.github/prompts/debug-python-bindings.prompt.md` |
| Debug MATLAB bindings | `.github/prompts/debug-matlab-bindings.prompt.md` |
| Debug WASM build | `.github/prompts/debug-wasm-build.prompt.md` |

## Skills

| Task | Skill |
|------|-------|
| Documentation maintenance | `.github/skills/docs-maintenance/SKILL.md` |
| Sanitizer reproduction | `.github/skills/sanitizer-repro/SKILL.md` |
| iccSpecSepToTiff QA | `.github/skills/specsep-qa/SKILL.md` |
| JSON/config regressions | `.github/skills/json-config-regression/SKILL.md` |
| Regression workflow governance | `.github/skills/regression-workflow-governance/SKILL.md` |
| Regression container maintainer | `.github/skills/regression-container-maintainer/SKILL.md` |
| Pre-PR security cycle | `.github/skills/pre-pr-security-cycle/SKILL.md` |
| Maintainer label system | `.github/skills/maintainer-label-system/SKILL.md` |
| Version bump | `.github/skills/version-bump/SKILL.md` |
| Python bindings tests | `.github/skills/python-bindings-test/SKILL.md` |
| MATLAB bindings tests | `.github/skills/matlab-bindings-test/SKILL.md` |
| WASM build tests | `.github/skills/wasm-build-test/SKILL.md` |

## WASM Notes

- The current WASM deliverable is the staged Node/npm-style module package from
  `Build/Cmake/wasm-package/stage.sh`, not a browser UI.
- Do not reintroduce or test legacy `wasm/*.html`, `wasm/*.css`, or
  `wasm/*.js` assets; validate with `wasm-stage/test_all.js` and
  `wasm-stage/regression.js`.
