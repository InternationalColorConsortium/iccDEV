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
| `IccProfLib/**`, `IccXML/**`, `Tools/**`, `IccJSON/**` | `instructions/icc-library-code.instructions.md` | Parser hardening and C++ safety patterns |
| `Testing/**` | `instructions/testing.instructions.md` | Test scripts, profile directories, regression flow |
| `IccProfLib/icProfileHeader.h` | `instructions/icc-specification.instructions.md` | ICC header, tag, and color-space rules |
| `ports/**` | `instructions/vcpkg-port.instructions.md` | vcpkg overlay port and CI |
| `Tools/Winnt/IccIisIsapi/**` | `Tools/Winnt/IccIisIsapi/isapi-instructions.md` | IIS ISAPI setup and hardening |

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

## Build, Test, and Safety

- User build instructions: `docs/build.md`
- Maintainer build and sanitizer policy: `.github/instructions/build-system.instructions.md`
- Test profile workflow: `.github/instructions/testing.instructions.md`
- Workflow governance: `.github/instructions/workflow-governance.instructions.md`

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
| Workflow governance audit | `.github/prompts/audit-workflow-governance.prompt.md` |
| Maintainer label triage | `.github/prompts/maintainer-label-triage.prompt.md` |
| vcpkg debug | `.github/prompts/vcpkg-port-debug.prompt.md` |

## Skills

| Task | Skill |
|------|-------|
| Documentation maintenance | `.github/skills/docs-maintenance/SKILL.md` |
| Sanitizer reproduction | `.github/skills/sanitizer-repro/SKILL.md` |
| JSON/config regressions | `.github/skills/json-config-regression/SKILL.md` |
| Regression workflow governance | `.github/skills/regression-workflow-governance/SKILL.md` |
| Maintainer label system | `.github/skills/maintainer-label-system/SKILL.md` |
| Version bump | `.github/skills/version-bump/SKILL.md` |
