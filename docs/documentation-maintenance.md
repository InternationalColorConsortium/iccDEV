# Documentation Maintenance

Use this guide when editing iccDEV documentation. The goal is high signal:
accurate commands, one canonical source per topic, and short routing docs that
point to deeper references.

## Canonical Sources

| Topic | Canonical source |
|-------|------------------|
| User install and packaging | `docs/install.md` |
| User build instructions | `docs/build.md` |
| CTest tool suites and add-test process | `docs/ctest.md` |
| CLI tools and shared option tables | `docs/tools-cli-reference.md` |
| JSON workflow | `docs/iccjson.md` |
| JSON tag examples | `docs/iccjson-tag-types.md` |
| IccConnect library and `CIccConnectCmm` factory | `docs/icc-connect.md` |
| IccConnect JSON config schema | `docs/icc-connect-config.schema.json` |
| Threaded CMM apply (`CIccThreadedCmm`) | `docs/icc-cmm-threading.md` |
| Bisect workflow | `docs/bisect.md` |
| Pre-PR security cycle | `docs/pre-pr-security-cycle.md` and `.github/skills/pre-pr-security-cycle/SKILL.md` |
| Regression workflow updates | `docs/regression-workflow-governance.md` |
| Maintainer label system | `docs/label-system.md`, `.github/labels.yml`, `.github/labeler.yml`, and `.github/skills/maintainer-label-system/SKILL.md` |
| CodeQL queries | `.github/codeql-queries/README.md` |
| Security issue format | `.github/prompts/SECURITY_ISSUE_FORMAT.md` |
| Agent routing | `.github/copilot-instructions.md` |
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
- Route workflow, CTest, CPack, sanitizer, release packaging, and security
  automation changes through iccDEV maintainers.
- Prefer tables for indexes and terse prose for workflows.
- Remove duplicate explanations after preserving any unique details.
- Keep prompts operational; move long reference material into a named reference
  file when it is reused by templates or prompts.
- Do not add generated artifacts, logs, crash files, or local environment paths.

## Validation

Before handing off documentation changes, run:

```bash
git diff --check
```

Also check local Markdown links and ASCII-only output for changed or new files.
If a file is deleted, search for stale references to that path.
