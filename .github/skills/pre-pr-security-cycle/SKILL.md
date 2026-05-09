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
| Workflow YAML or shell | Workflow governance prompt, YAML parse, `actionlint`, expression-in-run scan |
| C/C++ or CMake security path | CodeQL local script or hosted `ci-codeql-security` |
| Parser/profile/tool behavior | Code review hunting prompt plus sanitizer build where practical |
| Release, Docker, WASM, vcpkg | Governance prompt plus package/runtime smoke logs |

CodeQL does not replace YAML, shell, or permissions review.

## Dynamic Checks

Choose the smallest dynamic check that proves the changed behavior:

- CTest suite or focused regression for profile/tool changes.
- ASAN/UBSAN/IntSan command for parser or untrusted input changes.
- Docker runtime smoke for container changes.
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
- whether the PR is merge-ready.

Prefer a short human-golfed report over raw logs.

## References

- `../../../docs/pre-pr-security-cycle.md`
- `../../../docs/build.md`
- `../../../docs/ctest.md`
- `../../../docs/codeql.md`
- `../../../docs/regression-workflow-governance.md`
- `../../prompts/pre-pr-security-cycle.prompt.md`
- `../../prompts/audit-workflow-governance.prompt.md`
- `../../prompts/build-and-test.prompt.md`
- `../../prompts/code-review-hunting.prompt.md`
