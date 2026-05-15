# Pre-PR Security Cycle

Use this maintainer workflow before opening or finalizing a PR that touches
C/C++, CMake, CI, release packaging, sanitizer settings, or security automation.
The goal is a short repeatable loop: code, validate, fix, and only then hand off
the smallest useful evidence.

## Loop

1. Scope the smallest change and keep unrelated cleanups out of the branch.
2. Build and run the nearest deterministic tests from `docs/build.md` and
   `docs/ctest.md`.
3. Run SAST appropriate to the change:
   - workflow edits: `audit-workflow-governance.prompt.md`, YAML parse,
     `actionlint`, and direct-expression scans;
   - C/C++ or CMake edits: `.github/scripts/run-codeql-local.sh`, or the
     hosted `ci-codeql-security` workflow when local CodeQL is not practical;
   - Dockerfile or container edits: `hadolint`, Trivy config, image
     vulnerability/secret scan, and runtime or healthcheck smoke validation;
   - security-sensitive code paths: `code-review-hunting.prompt.md`.
4. Run dynamic or sanitizer checks for the changed surface:
   - ASAN/UBSAN/IntSan builds for parser, profile, or tool changes;
   - CTest profile gates for generated profile behavior;
   - release, Docker runtime/image-scan, WASM, or vcpkg smoke tests for
     packaging changes.
5. Fix every confirmed issue and repeat the relevant checks until the same
   command set is clean.
6. Produce a golfed handoff: branch, commit, changed surface, command results,
   hosted run IDs, known skips or deferred items, and merge-readiness signal.

## SAST, DAST, and CodeQL Boundaries

CodeQL is required for C/C++ or CMake-relevant security changes, but it is not a
workflow linter. Pair CodeQL with workflow-governance checks for YAML and shell
changes.

DAST in this repository usually means exercising built binaries and packaged
artifacts with hostile or representative profiles, not a web scanner. Use
sanitizer runs, CTest, CLI smoke tests, Docker runtime checks, WASM parity, and
release-asset validation as the dynamic phase.

## Minimal Evidence

Record only evidence that changes the merge decision:

- exact command or workflow name;
- exit status or GitHub conclusion;
- key sentinel count, artifact name, checksum, or alert status;
- known skip, suppression, or post-merge follow-up.

Avoid long logs, copied stack traces, or generated file listings unless they are
the defect or the proof.

## References

- `build.md`
- `ctest.md`
- `codeql.md`
- `regression-workflow-governance.md`
- `.github/skills/pre-pr-security-cycle/SKILL.md`
- `.github/prompts/pre-pr-security-cycle.prompt.md`
- `.github/prompts/audit-workflow-governance.prompt.md`
- `.github/prompts/build-and-test.prompt.md`
- `.github/prompts/code-review-hunting.prompt.md`
