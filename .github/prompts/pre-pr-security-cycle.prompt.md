# Pre-PR Security Cycle

Use this prompt to plan or audit the minimum secure loop before opening,
updating, or merging an iccDEV PR.

## Inputs

- Pre-PR branch and head SHA:
- PR head SHA and explicit opening authorization, if applicable:
- Issue or PR:
- Changed surface:
- Security-sensitive paths:
- Maintainer-owned infrastructure touched:
- Expected merge signal:

## Loop Checklist

1. Scope the smallest complete change.
2. Build and run nearest deterministic tests.
3. Run SAST:
   - workflow changes: governance audit, YAML parse, `actionlint`, CodeQL
     Actions analysis, expression injection scan, and trusted-base helper
     boundary review;
   - Python script changes: syntax check and CodeQL Python analysis;
   - shell script changes: ShellCheck; CodeQL Actions covers inline workflow
     `run:` blocks but not standalone shell scripts;
   - C/C++ or CMake changes: CodeQL local script or hosted CodeQL workflow;
   - parser/profile/tool changes: code-review hunting prompt.
   - Dockerfile or container changes: `hadolint`, Trivy config, image
     vulnerability/secret scan, and container smoke or healthcheck validation.
4. Run dynamic checks:
   - CTest, sanitizer, CLI smoke, Docker runtime/image scan or healthcheck,
     WASM parity, release assets, or vcpkg consumer smoke as appropriate.
   - MCP runtime changes: validate every affected Docker image variant. Compare
     discovered CLI tools against `TOOL_BINARIES` and inspect capability flags;
     never assert one fixed health-tool total across variants.
5. Fix confirmed findings and repeat only affected checks.
6. Use the current maintainer image and local tools for the applicable checks;
   record the image digest and revision. On the pushed pre-PR branch, run
   `ci-qa-pr` with `ctest-mode=full` and `ctest-recent-limit=0`, plus
   `ci-codeql-security` on the same head. Wait for terminal results and
   repeat affected checks after any head change.
7. Freeze the head and record the readiness evidence and `base...HEAD` contract
   matrix required by `docs/governance/UPSTREAM_PR_READINESS.md` before
   requesting review. The matrix maps each changed cross-cutting surface to its
   producer, consumer, build/runtime behavior, toolchain or platform boundary,
   CI trigger, dependency owner, and evidence. If a re-review finds any new
   blocker, stop serial automated review and return to branch-only grooming.
8. Open the PR only with explicit user authorization after readiness passes.
   Run `ci-pr-action` with `ci_scope=full` on the PR head for final
   cross-platform builds, profile checks, and representative CTest coverage.
   This gate does not replace local or pre-PR screening.
9. Prepare a golfed handoff with commands, run IDs, sentinels, and known skips.

## Handoff Format

- Branch and commit:
- Files or surfaces changed:
- Local validation (image digest and revision where used):
- Pre-PR `ci-qa-pr` and `ci-codeql-security` (same head SHA, run IDs, results):
- PR-head `ci-pr-action` (run ID, result, applicable skips):
- SAST/CodeQL status:
- Dynamic/sanitizer status:
- Known skips, suppressions, or deferred follow-ups:
- Reviewed exceptions and visual trust-boundary notes:
- Merge-ready decision:

## References

- `../../docs/pre-pr-security-cycle.md`
- `../../docs/workflow-security-trust-boundaries.md`
- `../skills/pre-pr-security-cycle/SKILL.md`
- `audit-workflow-governance.prompt.md`
- `build-and-test.prompt.md`
- `code-review-hunting.prompt.md`
