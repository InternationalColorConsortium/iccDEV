# Regression Workflow Governance

Use this guide when adding or changing regression gates, tool-test workflows, or
focused validation scripts.

These workflows and gates are maintainer-owned infrastructure. General
contributors should not edit `.github/**`, CTest registration, sanitizer helper
scripts, CPack or release packaging, workflow policy, or security automation
unless an iccDEV maintainer explicitly requests that work. Contributors should
coordinate needed coverage with a maintainer.

## Screening Sequence

1. Run directly applicable local checks for the changed behavior. Confirm
   that positive and negative cases exercise the intended path.
2. When the maintainer authorizes hosted pre-PR screening, run `ci-qa-pr` with
   `ctest-mode=full` and `ctest-recent-limit=0`, plus
   `ci-codeql-security` on the same head. Investigate failures and repeat the
   affected checks after changes.
3. Once screening succeeds and the user authorizes publication, open the PR.
4. On initial PR actions, `ci-pr-action` runs the selected Debug ASAN/UBSAN
   lane. Run its full scope on the current PR head for final cross-platform
   builds, profile generation, and representative CTest coverage. Its
   `pr-extended` exclusion and path-scoped lanes mean it does not replace
   directly applicable local checks or authorized pre-PR screening. Verify
   the exact head and terminal results.

Each workflow still follows its declared scope: for
example, `ci-infrastructure` CTests require direct selection or a dedicated
workflow. Run specialized security and packaging gates when their contracts
apply.

## Canonical Locations

| Item | Location | Purpose |
|------|----------|---------|
| Focused reusable regression scripts | `.github/scripts/` | Scripted checks shared by one or more workflows. |
| Regression PoC inventory | `.github/ci/regression/README.md` | Maps regression inputs and scripts to issues. |
| Tool test gate | `.github/workflows/ci-iccdev-tool-tests.yml` | Reusable/manual core or ASAN/UBSAN regression coverage. |
| PR Debug sanitizer gate | `.github/workflows/ci-pr-action.yml` | Path-scoped Debug ASAN/UBSAN tool suite on initial PR validation; manual `ci-qa-pr` full/0 covers later heads. |
| Risk analysis | `.github/workflows/ci-pr-risk-security-analysis.yml` | Optional manual/reusable full-repository Linux and Windows risk inventory; shares preflight workflow policy. |
| Rebase safety gate | `.github/workflows/ci-rebase-safety.yml` | Lightweight ancestry validation for same-repository PR synchronize events. |
| PR CI label dispatcher | `.github/workflows/ci-maintainer-pr-dispatch.yml` | Maintainer-label requests dispatched from the trusted `master` workflow with exact PR head input. |
| PR status labeler | `.github/workflows/update-labels.yml` | API-only current-head validation on PR events and trusted workflow completions. |
| MATLAB native gate | `.github/workflows/ci-matlab.yml` | Manual Windows MSVC, Linux x86_64, and Apple Silicon macOS MEX QA; separate Docker interoperability. |
| MATLAB release bundles | `.github/workflows/ci-latest-release.yml` | Linux x86_64, macOS arm64, and Windows x64 MEX builds with staged MATLAB QA and payload verification. |
| Apple archive and runtime gates | `.github/workflows/ci-apple-mobile-core.yml`, `.github/workflows/ci-pr-action.yml` | Mobile static-core artifacts, central PR Xcode Release/Debug CTest runtime layouts, and [local simulator commands](build.md). |
| CTest registration | `Build/Cmake/Testing/CMakeLists.txt` | CTest names, labels, fixtures, timeouts, and check target. |
| CTest process guide | `docs/ctest.md` | Local commands, registered suites, and add-test workflow. |
| Coverage process guide | `docs/code-coverage.md` | Deterministic profraw merging, module matching, and configuration timing. |
| Maintainer CI skill | `.github/skills/maintainer-ci-ctest/SKILL.md` | Repeatable maintainer workflow for CI, CTest, CPack, sanitizer, and release gates. |
| Maintainer CI prompt | `.github/prompts/maintainer-ci-ctest.prompt.md` | Structured planning prompt for maintainer-owned infrastructure changes. |
| Workflow rules | `.github/instructions/workflow-governance.instructions.md` | Shell hardening, output sanitization, and injection prevention. |
| Workflow trust boundaries | `docs/workflow-security-trust-boundaries.md` | Trusted-base helper model, PR workflow canaries, and visual review aids. |
| Testing rules | `.github/instructions/testing.instructions.md` | Test directories, script expectations, and regression flow. |
| Unified Dockerfile | `Dockerfile` | Published runtime, MCP, and pinned CI dependency image. |

## PR Path Classification

`ci-matlab` is manual-dispatch only. It retains the R2026a Windows native
regression lane plus Ubuntu x86_64 and Apple Silicon macOS MEX gateways with
the R2023b compatibility floor. All three call `matlab/run_ci_qa.m` for the
shared wrapper, runtime, plotting, and example contract. Only unavailable
Docker interoperability may skip there; a separate pinned-container job tests
that contract. These jobs do not package or upload release artifacts.

`ci-pr-action.yml` reports factual path classes separately from the
`run_native_matrix` execution decision. Native source ownership is limited to
`IccProfLib/`, `IccXML/`, `IccJSON/`, `IccConnect/`, `Tools/`, and registered
C/C++ sources under `.github/ci/regression/`. Generated header templates and
native tool resources under the owned source directories are included. Native
regression fixtures under `.github/ci/regression/` and HEIF carriers under
`.github/ci/test-data/heif/` select the testing surface.
Native build configuration is
limited to `Build/Cmake/`, `Build/AppleMobile/`, `Build/XCode/BuildAll.sh`, and
the root `vcpkg.json`. `Testing/` changes also select the native matrix.

Examples, ports, bindings, documentation, and unrelated CMake wrappers do not
select the native matrix merely because of a file extension or basename.
Keep this contract table-driven in
`.github/tests/test-ci-pr-path-classifier.sh` and source the classifier from the
trusted base checkout for pull requests. Path discovery disables Git rename
detection so a move is classified against both its removed source path and its
added destination path.

`ci-pr-action` owns deterministic core builds and tests, plus a reusable MCP
package and Dockerfile check. The MCP job runs when the native matrix is
selected or when MCP package, Dockerfile, or MCP workflow files change. It
checks out the exact PR head selected by the caller and contributes to the
protected `PR Summary` result. Its `mcp-server-` concurrency group is distinct
from the caller's `ci-pr-` group, so a called job cannot cancel its caller.
The core reusable tool suite passes `instrumentation=core` and always excludes
`ci-infrastructure` and `clusterfuzzlite` CTests.
The macOS Clang Release/LTO and Debug jobs run a representative native CTest
selection after profile generation. They exclude `slow`, `clusterfuzzlite`,
`pr-extended`, and `ci-infrastructure`, so Darwin shared-library and optimizer
boundaries are checked without duplicating the comprehensive pre-PR matrix.
Its fast mode runs a fixed core smoke selection plus the requested recent
registrations; the recent limit is registration order, not change impact.
Full mode checks the image's spectral-preview Python package pins and decoder
capabilities before reusing its environment, and rebuilds that environment
from the checkout when the image contract differs. Fast mode excludes the sole
spectral-preview CTest and skips that dependency setup.
Windows PR jobs report CTest helper compilation and CTest execution as separate
steps. The MinGW helper build uses bounded two-way parallelism; MSVC and
ClangCL keep serial staging for vcpkg runtime DLLs.
On initial PR opening, reopening, or ready-for-review, `ci-pr-action` selects
its Debug ASAN/UBSAN tool-suite job when the sanitizer path classifier matches
native source, build, testing, regression data, scripts, or relevant workflows.
The reusable tool workflow remains manually dispatchable. Pre-flight is a
standalone path-scoped PR workflow, while risk analysis is an explicit final
security workflow, so scanner or infrastructure failures cannot be mistaken
for core build failures. Dynamic-analysis tooling and ClusterFuzzLite remain manual or
scheduled lanes.

## Rebase and Final Validation

Opening, reopening, or marking a PR ready for review runs its path-scoped
automatic gates. A subsequent `synchronize` event runs `ci-rebase-safety`
for ancestry and `ci-pr-action` in its bounded fast lane for the new head.
The fast lane does not restart the full matrix. `ci-pr-action` publishes an
exact-head `PR Summary` result; the trusted status labeler refreshes after
validation completes. Check that result, the current head, labels, draft
state, and merge state before reporting `Merge Ready`.

The Debug ASAN/UBSAN job runs only on initial PR actions; `synchronize` and
manual full-scope dispatches do not rerun it.

If automatic validation is absent on an open same-repository PR targeting
`master`, dispatch `ci-pr-action` from the trusted `master` workflow with
the PR number and exact expected head SHA, then manually refresh
`update-labels.yml` if its completion-triggered refresh is absent. See
`label-system.md` for commands and the non-`master`-base stacked PR limit.
This automatic fast lane does not replace the final full-scope lanes below.

Before merging a rebased source PR, explicitly dispatch the applicable final
lanes on the PR head. Use the full core matrix for source, build, or test
changes; run manual `ci-qa-pr` with `ctest-mode=full` and
`ctest-recent-limit=0` for comprehensive same-head sanitizer coverage when its
contract applies. Dispatch JSON and MATLAB workflows only when their
documented paths or contracts apply. `ci-pr-action` includes MCP validation
for full core runs; `ci-mcp-server` remains available for an isolated rerun.

```bash
gh workflow run ci-pr-action.yml --ref master -f ci_scope=full \
  -f pr_number=<open-master-target-pr-number> \
  -f expected_head_sha=<40-character-head-sha>
gh workflow run longbuildctest.yml --ref <head-branch> \
  -f ctest-mode=full -f ctest-recent-limit=0
gh workflow run ci-json-roundtrip.yml --ref <head-branch>
gh workflow run ci-mcp-server.yml --ref <head-branch>
gh workflow run ci-matlab.yml --ref <head-branch>
gh workflow run ci-pr-risk-security-analysis.yml --ref <head-branch>
```

`Rebase Safety` proves ancestry only; it does not satisfy the protected
`PR Summary` context. Check the automatic fast lane and final full-scope
results on the current PR head before merge. Rebases and
ordinary pushes both emit `synchronize`; neither automatically restarts the
full matrix. Manual risk analysis checks every workflow's Bash prologue,
even when the target and audit base SHA coincide.

## When to Add a Script

Extend an existing regression script when practical. Add a new
`.github/scripts/*.sh` check only when it needs reuse across workflows, parses
generated or binary output, or makes an otherwise long workflow step easier to
review. Short, single-purpose checks can stay inline.

## Regression Gate Requirements

Every new regression gate must exercise a known defect or named invariant.
Verify that it performs the intended work and fails when the invariant breaks.
Follow `AGENTS.md` for CTest selection and coverage requirements.

Prefer deterministic invariants over broad diffs. For generated ICC profiles,
normalize only documented volatile fields when comparing whole files; otherwise
assert specific tag sizes, offsets, record lengths, or validation messages.

The Apple mobile-core gate extends #2445's static-core SDK builds. It builds
both dependency-free `apple-*-core` presets and extended
`apple-*-extended-core` presets where each SDK is installed. A standalone
manual dispatch builds and validates the static archives, generated version
headers, and manifest before uploading them, then runs the bounded Xcode gate.
The release workflow reuses the archive build and validation without rerunning
Xcode. Normal PR CI separately requires fresh Release and Debug CLI output from
`iccdev-xcode-ctest-smoke.sh` in the central PR workflow, without constructing
or uploading the mobile archive.

Hosted workflows do not run `iccdev-apple-simulator-smoke.sh`. Its fresh-report,
console-sentinel, missing-fixture, extended-library, invalid-profile, and mobile
gap checks remain an explicit local gate documented in `docs/build.md`. The
combined manual iOS example sanitizer helper is local-only as well. The Apple
workflow does not run automatically on a push, and a nonzero helper exit fails
the selected gate.

## Workflow Governance Requirements

Every edited workflow `run:` block must keep these properties:

- `shell: bash --noprofile --norc {0}` for bash steps.
- `BASH_ENV: /dev/null` for bash steps only.
- `set -euo pipefail` as the first bash command.
- `pwsh -NoProfile -NoLogo -NonInteractive -Command {0}` and
  `$ErrorActionPreference = 'Stop'` for PowerShell steps.
- `git config --global credential.helper ""`.
- `unset GITHUB_TOKEN || true`.
- No direct `${{ }}` expressions inside shell code; pass values through `env:`.
- Sanitized writes to `GITHUB_STEP_SUMMARY` and `GITHUB_OUTPUT`.
- No unbounded glob or `xargs` batching into repeated writes to one fixed report
  output; use an input list or unique staged outputs.
- Validate exact commit SHA inputs and reject malformed workflow inputs rather
  than silently coercing them. Bound runtime maxima against job timeouts,
  including setup and repeated targets.
- Least-privilege permissions.
- `pull_request_target` and `workflow_run` status automation must use trusted
  default-branch definitions and API metadata only. It must never checkout or
  run PR-controlled code before mutating trusted state such as labels or checks.
  A workflow completion is a refresh signal, not proof of PR success; resolve
  the current head and its required aggregate result before applying labels.
- `pull_request` workflows that build PR code must source `.github/scripts`
  helpers and sanitizers from a trusted base checkout, not from PR-controlled
  content, unless a test-only exception is explicitly marked for preflight.
- Runner-reduction changes must preserve trusted-base sanitizer sourcing,
  sanitize every `GITHUB_OUTPUT` value, and retain path-gated validation for
  container changes.
- Actions-service workflow cache is limited to exact-key, checksum-addressed
  immutable download archives. Pull requests restore only; trusted push,
  schedule, and manual jobs may save after verification and only on a restore
  miss. Keep release, build output, package tree, compiler, CTest, and artifact
  paths uncached. The
  manual Docker workflow may import inline cache metadata only from a previously
  validated canonical image resolved to an immutable digest, and may export it
  only inside the candidate image.
- Treat a cache as a measured pilot. Compare a cold trusted run with a warm
  rerun, record cache size and hit status, and remove a cache that adds service
  churn without a material elapsed-time reduction.

`ci-preflight-safety.yml` is the path-scoped automatic check for same-repository
PR open/reopen/ready events and `master` pushes; it can also be dispatched
manually. It runs the trusted-base preflight script and the applicable workflow
and script tools. The script's `--workflow-risk-only` mode owns the shared
deterministic policy check, including output-write annotations.

For an optional full-repository Linux and native Windows risk inventory,
dispatch `ci-pr-risk-security-analysis.yml` directly or reuse it. The Linux
job reports shared policy failures in its existing risk report; they block
when `fail_on_findings=true` and are advisory when false. The Windows
`Out-File` audit adds PowerShell-specific output coverage. Manual dispatch
defaults to both platforms and `fail_on_findings=true`; `workflow_call`
defaults to false. `severity_threshold=HIGH` is display-only, not a finding
filter. Risk analysis is not an automatic or required merge gate.

For maintainer-owned workflow, release, packaging, Docker, MCP, and security
automation changes, run directly applicable local security tools, patch
confirmed findings, and retest the affected path. Explain material accepted
scanner noise.

## Hosted Result Checks

Inspect the affected jobs and steps, including skipped checks and non-fatal
warnings. Confirm that the gate performed its intended work and produced the
expected output. For install or packaging changes, check relevant logs for
duplicate manifest entries, missing files, vcpkg mismatches, and skipped
smoke tests.

## Script Pattern

Regression scripts should:

- Use `#!/bin/bash` and `set -uo pipefail`.
- Accept `ICCDEV_TOOLS_DIR`, `ICCDEV_TESTING_DIR`, and `ICCDEV_TEST_OUTDIR`.
- Write temporary files under `ICCDEV_TEST_OUTDIR`.
- Print `[PASS]`, `[FAIL]`, `[WARN]`, or `[SKIP]` labels.
- Exit non-zero if any required check fails.
- Treat ASAN/UBSAN findings as failures unless a documented benign suppression
  is part of the test envelope.

Minimal local invocation:

```bash
ICCDEV_TOOLS_DIR=$PWD/Build/Tools \
ICCDEV_TESTING_DIR=$PWD/Testing \
ICCDEV_TEST_OUTDIR=/tmp/iccdev-regression \
  .github/scripts/<regression-script>.sh
```

## Documentation Updates

When adding a gate, update the smallest useful index:

- `docs/ctest.md` for CTest registrations, fixtures, labels, or scoped
  generated-profile count changes. CTest discovery is the authoritative suite
  inventory; do not add a global expected count.
- `docs/ctest.md` or the relevant tool README when adding cases inside an
  existing CTest-backed script without changing the CTest suite count.
- `.github/ci/regression/README.md` for PoC files or script-based gates.
- `.github/instructions/testing.instructions.md` when the gate becomes part of
  standard testing policy.
- `.github/copilot-instructions.md` only for cross-cutting agent routing.
- `docs/regression-workflow-governance.md` when the process itself changes.

Do not update root `README.md` or `CONTRIBUTING.md` for branch-specific
regression changes unless ICC approval explicitly covers those root documents.

## Maintainer Dockerfile Changes

All `Dockerfile*` changes are maintainer-owned because they affect release
artifacts, runner trust, or pinned dependency envelopes. Keep container changes
separate from general source changes when practical.

| File | Owner intent | Required local checks |
|------|--------------|-----------------------|
| `Dockerfile` | Unified image for runtime tools, MCP, ASAN/UBSAN CTest, fuzzing, review, and hybrid timing gates. Clang 22 is the default toolchain; the packaged AFL++ LLVM plugin is paired with Clang 21. | Use cached builds during development, then run the container maintainer preflight in \ref md_docs_2regression-container "the regression container guide", including a final no-cache build, policy/SAST checks, analyzer inventory, MCP/REST runtime smoke, health check, and image vulnerability/secret triage. AFL wrapper changes also need the container bootstrap probe in `docs/afl-fuzzing.md`. |

For unified `Dockerfile` publishing:

1. Build and smoke the target image locally; cached development builds are
   permitted, but the final pre-push proof must use no cache.
   The manual Docker workflow defaults to a digest-pinned inline cache from its
   last validated integration image, falls back to `latest`, and performs a
   cold build when neither exists. It exposes `use_build_cache=false` for an
   explicit clean hosted rebuild and does not use the GitHub Actions cache
   service or a separate mutable cache image.
2. Publish through the maintainer-controlled container release path.
3. Record the published immutable SHA tag, digest, and source revision from the
   release output.
4. Publish `latest` only from `master`; publish an integration tag only from
   `ci-qa-pr-docker-testing` or `ci-publish-colourbill-ctrl`, after all image
   smoke tests and regression CTest checks succeed, then confirm it resolves
   to the immutable digest.
5. Pass the immutable SHA tag to `ci-iccdev-tool-tests.yml` and rerun the
   regression gate.
6. Do not create branch-specific or run-specific image tags.

For day-to-day image use, PR checkout, issue reproduction, evidence handling,
and CI dispatch commands, use
[Maintainer regression container](regression-container.md).

## Local Validation

Before pushing:

```bash
bash -n .github/scripts/<regression-script>.sh
git diff --check
file .github/scripts/<regression-script>.sh
```

Run the focused regression and report its command and result.
For workflow edits, also inspect the changed `run:` blocks for the governance
rules above.
Run `.github/scripts/check-workflow-fixed-output-batching.py` for any workflow
that enumerates inputs for coverage, report, archive, or checksum generation.
For CI packaging edits, run the nearest local install/uninstall repro and grep
the captured logs for missing-file diagnostics. If the change touches CMake or
C++ code, run `.github/scripts/run-codeql-local.sh --custom-only`; CodeQL is not
a substitute for YAML linting.

Report changed files, focused results, and any unverified coverage to the
maintainer. A separate report artifact is unnecessary.
