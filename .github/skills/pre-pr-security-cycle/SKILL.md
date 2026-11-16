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
- For maintainer infrastructure, prefer a conservative security-review loop:
  run the relevant local scanners, patch confirmed findings, retest, and record
  any accepted scanner noise with a clear rationale.
- Runner-reduction changes must retain trusted-base sanitizer sourcing,
  sanitization for every workflow output, and path-gated coverage for container
  changes.

## Loop

1. Implement the smallest complete change.
2. Build and run the nearest deterministic tests.
3. Run SAST for the changed surface.
4. Run dynamic, sanitizer, packaging, or artifact checks for the changed
   surface.
5. Fix confirmed findings.
6. Repeat only the checks affected by the fix.
7. On the pushed pre-PR branch, run `ci-qa-pr` with `ctest-mode=full` and
   `ctest-recent-limit=0`, plus `ci-codeql-security` on the same head. Wait
   for terminal results on that head.
8. Review the complete diff and directly affected consumers before pushing.
   Follow `../../../AGENTS.md` for scope and review limits.
9. Open the PR only after explicit user authorization and applicable checks.
    Run `ci-pr-action` with `ci_scope=full` on the PR head for final
    cross-platform builds, profile checks, and representative CTest coverage;
    it does not replace local or pre-PR screening. A push or rebase on an
    open same-repository PR targeting `master` starts the bounded fast lane
    automatically; completion refreshes status labels. Verify the current
    head, `PR Summary`, labels, and merge state. If the automatic run is
    absent, dispatch the trusted `master` workflow with `pr_number` and
    `expected_head_sha`, then refresh labels manually. Follow
    `../../../docs/label-system.md` for commands and stacked-PR limits.
10. Report the current head, checks and results, and unresolved issues
    concisely.

For focused local iteration, run
`.github/scripts/preflight-safety-checks.sh --fast-lane=matlab` for MATLAB-only
work, or plain `--fast-lane` for other changed workflow/script surfaces. These
skip local CodeQL database/query work. Do not add unrelated CTest coverage:
MATLAB-only changes use the focused MATLAB build and QA, while the full local
or hosted preflight remains the final workflow security signal.

Use the current maintainer image and its installed local tooling where
applicable. Keep local, pre-PR hosted, and PR-head results distinct when
reporting.

## SAST Selection

| Change | Required static checks |
|--------|------------------------|
| Workflow YAML | Workflow governance prompt, YAML parse, `jactionlint`, `zizmor`, CodeQL Actions analysis, expression-in-run scan |
| Python script | Python syntax check and CodeQL Python analysis |
| Shell script | ShellCheck and `zizmor`; CodeQL Actions covers inline workflow `run:` blocks, not standalone shell scripts |
| C/C++ or CMake security path | CodeQL local script or hosted `ci-codeql-security`; query changes also require a positive fixture and a safe negative control |
| Parser/profile/tool behavior | Code review hunting prompt plus sanitizer build where practical |
| Release, WASM, vcpkg | Governance prompt plus package/runtime smoke logs |
| Dockerfile or container image | `hadolint`, Trivy config/image scan, Dockle/Grype/Syft when practical |

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
- MCP runtime changes must validate every affected Docker image variant. Assert
  that discovered CLI tools match `TOOL_BINARIES` and inspect optional-capability
  flags; do not use a fixed health-tool count shared across variants.
- Dockerfile checks must not be advisory-only when container files changed:
  run `hadolint` and Trivy config, then build, scan, or smoke the affected image
  when practical.
- Confirm that container-surface (`image_definition_changed`) changes select
  workflow-security gates without adding a Docker verification job or full PR
  matrix to the aggregate status.
- For container changes, validate the same helper checks that the publishing
  workflow validates, including patch checkers, applicators, environment
  banners, and healthcheck semantics.
- WASM validation and parity for Emscripten changes.
- Release ZIP/checksum/artifact shape check for release packaging changes.
- vcpkg consumer smoke for port/export changes.

## References

- `../../../docs/pre-pr-security-cycle.md`
- `../../../docs/workflow-security-trust-boundaries.md`
- `../../../docs/build.md`
- `../../../docs/ctest.md`
- `../../../docs/codeql.md`
- `../../../docs/regression-workflow-governance.md`
- `../../prompts/audit-workflow-governance.prompt.md`
- `../../prompts/build-and-test.prompt.md`
- `../../prompts/code-review-hunting.prompt.md`
