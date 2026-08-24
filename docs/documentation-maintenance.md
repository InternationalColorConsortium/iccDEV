# Documentation Maintenance

Use this guide when editing iccDEV documentation. The goal is high signal:
accurate commands, one canonical source per topic, and short routing docs that
point to deeper references.

## Canonical Sources

| Topic | Canonical source |
|-------|------------------|
| User install and packaging | `docs/install.md` |
| User build instructions | `docs/build.md` |
| MATLAB bindings and Windows PowerShell setup | `docs/matlab-bindings.md` |
| MATLAB PAWG Q1 runtime, native contract, and release support files | `docs/matlab-bindings.md` and `.github/skills/matlab-bindings-test/SKILL.md` |
| CTest tool suites and add-test process | `docs/ctest.md` |
| CLI tools and shared option tables | `docs/tools-cli-reference.md` |
| JSON workflow | `docs/iccjson.md` |
| JSON tag examples | `docs/iccjson-tag-types.md` |
| IccConnect library and `CIccConnectCmm` factory | `docs/icc-connect.md` |
| IccConnect JSON config schema | `docs/icc-connect-config.schema.json` |
| Threaded CMM apply (`CIccThreadedCmm`) | `docs/icc-cmm-threading.md` |
| Bisect workflow | `docs/bisect.md` |
| Pre-PR security cycle | `docs/pre-pr-security-cycle.md` and `.github/skills/pre-pr-security-cycle/SKILL.md` |
| Maintainer regression container operations | `docs/regression-container.md`, `.github/skills/regression-container-maintainer/SKILL.md`, and `.github/prompts/regression-container-maintainer.prompt.md` |
| Regression workflow updates | `docs/regression-workflow-governance.md` |
| Maintainer label system | `docs/label-system.md`, `.github/labels.yml`, `.github/labeler.yml`, and `.github/skills/maintainer-label-system/SKILL.md` |
| CodeQL queries | `.github/codeql-queries/README.md` |
| Security issue format | `.github/prompts/SECURITY_ISSUE_FORMAT.md` |
| Agent routing | `.github/copilot-instructions.md` |
| Agent instructions | `AGENTS.md` (canonical); root `CLAUDE.md` and `GEMINI.md` mirror it |
| Build and sanitizer policy | `.github/instructions/build-system.instructions.md` |
| Test and regression policy | `.github/instructions/testing.instructions.md` |
| Workflow hardening | `.github/instructions/workflow-governance.instructions.md` |
| vcpkg port policy | `.github/instructions/vcpkg-port.instructions.md` |
| Agent skills | `.github/skills/README.md` |
| Maintainer CI and CTest workflow | `.github/skills/maintainer-ci-ctest/SKILL.md` and `.github/prompts/maintainer-ci-ctest.prompt.md` |
| Maintainer label workflow | `.github/skills/maintainer-label-system/SKILL.md` and `.github/prompts/maintainer-label-triage.prompt.md` |
| IIS sample setup | `Tools/Winnt/IccIisIsapi/isapi-instructions.md` |
| IIS API reference | `Tools/Winnt/IccIisIsapi/api.md` |

## Editing Checklist

- Keep quickstart docs short and link to the canonical source for detail.
- Verify command names against CMake targets or existing scripts.
- Keep exact paths reproducible from the repository root.
- Keep Windows MATLAB commands in PowerShell, use `repo\msvc` as the documented
  build root, use MATLAB `setenv` for MATLAB environment changes, and label
  shell `export` examples as Unix-only.
- Runnable MATLAB smoke examples must use existing `Testing/...` profiles
  rather than placeholder filenames.
- Route workflow, CTest, CPack, sanitizer, release packaging, and security
  automation changes through iccDEV maintainers.
- Prefer tables for indexes and terse prose for workflows.
- Remove duplicate explanations after preserving any unique details.
- Keep prompts operational; move long reference material into a named reference
  file when it is reused by templates or prompts.
- Keep Doxygen-friendly Markdown links within the documented INPUT tree. When a
  reference points outside Doxygen's scanned sources (for example a repo-root
  `python/README.md` or an `examples/.../README.md` page), prefer an explicit
  HTML anchor like `<a href="../python/README.md">python/README.md</a>` to keep
  the generated docs warning-free without changing the rendered link target.
- Do not add generated artifacts, logs, crash files, or local environment paths.
- New or relocated C/C++ sources and headers must retain the complete ICC
  Software License block from an adjacent established file. Do not use
  abbreviated or placeholder license text; review this before handoff.
- Follow `.gitattributes`: use LF for documentation, prompts, skills, MATLAB,
  CMake, YAML/JSON, Dockerfiles, and Unix scripts; preserve CRLF for
  Windows-native PowerShell, batch, and command scripts.
- When editing `AGENTS.md`, keep the root `CLAUDE.md` and `GEMINI.md` mirrors in
  sync; they should only route back to `AGENTS.md`, never duplicate its rules.

## Copilot Instruction and Review Model

GitHub supports three complementary repository instruction surfaces:

- `.github/copilot-instructions.md` for concise repository-wide rules.
- `.github/instructions/**/*.instructions.md` for path-specific rules.
- the nearest `AGENTS.md` for agent context; the root `CLAUDE.md` and
  `GEMINI.md` remain thin compatibility mirrors in this repository.

Copilot code review reads these instructions and relevant `.github/skills` from
the pull request head branch, so instruction changes can be reviewed in the
same PR. Keep review guidance actionable and scoped to the owning surface.
Copilot submits advisory `Comment` reviews rather than approvals or requests
for changes, so it does not replace required human review or branch protection.

References:

- <https://docs.github.com/en/copilot/how-tos/copilot-on-github/customize-copilot/add-custom-instructions/add-repository-instructions>
- <https://docs.github.com/en/copilot/how-tos/use-copilot-agents/request-a-code-review/use-code-review>

## Validation

Before handing off documentation changes, run:

```bash
git diff --check
```

Also check local Markdown links, ASCII-only output, and `.gitattributes`
line-ending compliance for changed or new files. If a file is deleted, search
for stale references to that path.
