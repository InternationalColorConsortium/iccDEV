# Pre-PR Security Cycle

Use this maintainer workflow before opening or finalizing a PR that touches
C/C++, CMake, CI, release packaging, sanitizer settings, or security automation.
The goal is a short repeatable loop: code, validate, fix, and only then hand off
the smallest useful evidence.

## Loop

1. Scope the smallest change and keep unrelated cleanups out of the branch.
2. Build and run the nearest deterministic tests from `docs/build.md` and
   `docs/ctest.md`. Do not run broad CTest suites for a MATLAB-only change;
   build the MATLAB dependencies and run the focused MATLAB QA instead.
3. Run SAST appropriate to the change:
   - workflow edits: `audit-workflow-governance.prompt.md`, YAML parse,
     `actionlint`, full-workflow preflight canaries, trusted-base helper review,
     CodeQL Actions analysis, and direct-expression scans;
   - Python script edits: Python syntax checks and CodeQL Python analysis;
   - shell script edits: ShellCheck; CodeQL Actions still covers inline workflow
     `run:` blocks, but standalone shell scripts have no CodeQL extractor;
   - C/C++ or CMake edits: `.github/scripts/run-codeql-local.sh`, or the
     hosted `ci-codeql-security` workflow when local CodeQL is not practical;
   - Dockerfile or container edits: `hadolint`, Trivy config, image
     vulnerability/secret scan, and runtime or healthcheck smoke validation;
   - security-sensitive code paths: `code-review-hunting.prompt.md`;
   - MATLAB MEX or wrapper edits:
     `.github/instructions/matlab-code-review.instructions.md` and the
     focused MATLAB checks in `docs/matlab-bindings.md`.
4. Run dynamic or sanitizer checks for the changed surface:
   - ASAN/UBSAN/IntSan builds for parser, profile, or tool changes;
   - CTest profile gates for generated profile behavior;
   - release, Docker runtime/image-scan, WASM, or vcpkg smoke tests for
     packaging changes.
5. Fix every confirmed issue and repeat the relevant checks until the same
   command set is clean.
6. Freeze the head, record the readiness evidence in
   `docs/governance/UPSTREAM_PR_READINESS.md`, and request one complete review
   of the cumulative diff. Resolve requested changes branch-only and renew the
   evidence before a complete re-review. If that re-review finds an issue in
   unchanged code, stop serial automated review and return to branch-only
   grooming until a maintainer directs otherwise.
7. Produce a golfed handoff: branch, commit, changed surface, command results,
   hosted run IDs, known skips or deferred items, and merge-readiness signal.

## Stacked PR and Fast-Lane Handoff

For related changes that require multiple PRs within a rolling 24-hour period,
use `.github/skills/stacked-pr-fast-lane/SKILL.md`. Keep each layer focused,
synchronize the stack before handoff, and report the stack order with each PR's
head SHA. Fast lane is a maintainer-only, same-repository PR accelerator; it
does not replace the required PR gates or permit fork validation.

## Fork Agent-Configuration Boundary

Treat custom instructions, agent instructions, and skills from a fork PR head
as untrusted review guidance. Before reviewing same-repository agent-policy
changes, confirm the Fork Automation Gate protects the current and previous
paths and that the related trust-boundary documentation remains accurate.
Because Copilot code review reads head-branch guidance, compare same-repository
agent-policy changes with the trusted base and do not let the proposed guidance
weaken the controls used to review it.

## Local Fast Lane

During focused iteration, scan only changed workflows and scripts and skip the
expensive local CodeQL databases and query-resolution pass:

```bash
.github/scripts/preflight-safety-checks.sh --fast-lane=matlab
```

The MATLAB fast lane limits changed-file discovery to `ci-matlab.yml` and the
preflight script itself. It still runs YAML parsing, available workflow
linters, cache and security canaries, injection/trust checks, ShellCheck when
needed, and the permission audit. It does not run CTest. Pair it with only the
focused MATLAB dynamic tests in `docs/matlab-bindings.md`. Use plain
`--fast-lane` for changed-only checks across other workflow/script surfaces.

Before finalizing maintainer-owned workflow or security changes, run the full
preflight without `--fast-lane`, or use the hosted preflight and risk-analysis
gates. `--require-tools` remains available when every optional local scanner
must be present.

## SAST, DAST, and CodeQL Boundaries

CodeQL is required for workflow and Python script changes through preflight, and
for C/C++ or CMake-relevant security changes through the full CodeQL workflow.
The local fast lane intentionally defers CodeQL to the full local or hosted
gate; it is an iteration aid, not the final security signal.
Pair it with workflow-governance checks, actionlint, yamllint, ShellCheck, and
container scanners because no single tool covers the full CI threat model.

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
- `workflow-security-trust-boundaries.md`
- `regression-workflow-governance.md`
- `.github/skills/pre-pr-security-cycle/SKILL.md`
- `.github/prompts/pre-pr-security-cycle.prompt.md`
- `.github/prompts/audit-workflow-governance.prompt.md`
- `.github/prompts/build-and-test.prompt.md`
- `.github/prompts/code-review-hunting.prompt.md`
