---
name: pre-pr-security-cycle
description: >
  Maintainer workflow for the pre-PR secure loop: code, build/test,
  SAST/CodeQL, dynamic sanitizer checks, fixes, and concise handoff.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(gh:*)
---

# Pre-PR Security Cycle

Use this skill before opening, updating, or finalizing an iccDEV PR that touches
C/C++, CMake, CI, release packaging, sanitizer policy, CodeQL, or other
security automation.

## Scope

- Keep the branch focused on one feature, fix, or governance change.
- Do not bundle opportunistic cleanup unless it is required for the check to
  pass.
- Identify whether the change is contributor code, maintainer infrastructure,
  or both.

## Loop

1. Implement the smallest complete change.
2. Build and run the nearest deterministic tests.
3. Run SAST for the changed surface.
4. Run dynamic, sanitizer, packaging, or artifact checks for the changed
   surface.
5. Fix confirmed findings.
6. Repeat only the checks affected by the fix.
7. Prepare a concise handoff.

## SAST Selection

| Change | Required static checks |
|--------|------------------------|
| Workflow YAML | Workflow governance prompt, YAML parse, `actionlint`, CodeQL Actions analysis, expression-in-run scan |
| Python script | Python syntax check and CodeQL Python analysis |
| Shell script | ShellCheck; CodeQL Actions covers inline workflow `run:` blocks, not standalone shell scripts |
| C/C++ or CMake security path | CodeQL local script or hosted `ci-codeql-security` |
| Parser/profile/tool behavior | Code review hunting prompt plus sanitizer build where practical |
| Release, WASM, vcpkg | Governance prompt plus package/runtime smoke logs |
| Dockerfile or container policy | `hadolint`, Trivy config, image scan, and Docker runtime smoke |

CodeQL does not replace YAML, shell, or permissions review.

For workflow changes, also review
`../../../docs/workflow-security-trust-boundaries.md`. PR workflows that build
PR code must use trusted-base `.github/scripts` helpers for sanitizers,
summaries, and reusable workflow logic; any PR-controlled helper execution must
be test-only and explicitly visible in preflight output.

## Dynamic Checks

Choose the smallest dynamic check that proves the changed behavior:

- CTest suite or focused regression for profile/tool changes.
- ASAN/UBSAN/IntSan command for parser or untrusted input changes.
- Docker runtime smoke and image vulnerability/secret scan for container
  changes.
- Dockerfile checks must not be advisory-only when container files changed:
  run `hadolint` and Trivy config, then build, scan, or smoke the affected image
  when practical.
- WASM validation and parity for Emscripten changes.
- Release ZIP/checksum/artifact shape check for release packaging changes.
- vcpkg consumer smoke for port/export changes.

## Handoff

Report only merge-relevant evidence:

- branch and commit SHA;
- changed surface;
- commands or workflow run IDs;
- pass/fail conclusion and key sentinel counts;
- known skips, suppressions, warnings, or deferred follow-ups;
- reviewed cache/artifact/token/script exceptions and trust-boundary notes;
- whether the PR is merge-ready.

Prefer a short human-golfed report over raw logs.

## References

- `../../../docs/pre-pr-security-cycle.md`
- `../../../docs/workflow-security-trust-boundaries.md`
- `../../../docs/build.md`
- `../../../docs/ctest.md`
- `../../../docs/codeql.md`
- `../../../docs/regression-workflow-governance.md`
- `../../prompts/pre-pr-security-cycle.prompt.md`
- `../../prompts/audit-workflow-governance.prompt.md`
- `../../prompts/build-and-test.prompt.md`
- `../../prompts/code-review-hunting.prompt.md`
