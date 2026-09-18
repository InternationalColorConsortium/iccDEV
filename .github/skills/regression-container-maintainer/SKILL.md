---
name: regression-container-maintainer
description: >
  Use the published unified iccDEV container for maintainer smoke testing,
  pull request validation, issue reproduction, sanitizer review, and CI handoff.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(docker:*)
  - shell(gh:*)
---

# Unified Container Maintainer

Use this skill for repeatable maintainer operations in
`ghcr.io/internationalcolorconsortium/iccdev`.

## Required Inputs

1. Operation: basic smoke, PR validation, or issue reproduction.
2. Image selector: use `latest`; select a full-SHA tag only for an explicit
   reproducibility need. Resolve it at run time and do not embed a particular
   SHA as a reusable default.
3. PR, issue, branch, or commit reference.
4. Affected tool and smallest focused regression.
5. Expected pass and failure signals.
6. Host evidence directory.

## Workflow

1. Read `../../../docs/regression-container.md`.
2. Pull `latest`, resolve it to a digest, record the digest and source
   revision, and execute the digest rather than the mutable tag. Select a
   full-SHA tag only to reproduce or compare a specific image.
3. Mount only an evidence directory and start a disposable container.
4. Require a clean initial Git worktree.
5. For a PR, fetch `pull/<number>/head` and check out the ref detached.
6. For an issue, reproduce with existing project tools and saved inputs.
7. Rebuild the affected target, then run the focused regression and registered
   CTest wrapper.
8. For local PR proof, pull `latest`, record its resolved digest, mount the
   reviewed worktree read-only, and copy it to container-local scratch space.
   Run the local canonical-image build, reject compiler warnings, and run
   CTest excluding only the `slow` and `calculator` labels.
9. When the PR-specific behavior is in an excluded suite, run its focused CTest
   wrapper in addition to the local container envelope.
10. For AFL/CFL work, run `iccdev-fuzz-env`, record the patch-stack counts, and
    run the smallest matching smoke (`iccdev-afl-smoke.sh --patches` or
    `.github/ci/cfl/build.sh --patches`) before broader validation.
11. Run broader CTest or GCC 15.2 strict parity only after the focused check
    passes.
12. Scan logs for compiler warnings, ASAN, UBSAN, and signal termination.
13. Trigger `ci-pr-action.yml` explicitly for a pre-PR branch; a push alone does
    not trigger that workflow.
14. Use `latest` unless a full-SHA tag is required for a specific
    reproducibility need. Resolve the selector at run time and never hardcode
    a SHA tag as a long-lived workflow default. Integration, release, legacy,
    branch, run, and image-variant tags are not validation selectors.
15. Confirm the canonical image digest and hosted validation explicitly.
16. Report exact image tag, digest, source revision, commands, results, evidence,
    and workflow URLs.

For container/MCP runtime changes, run
`python3 .github/scripts/iccdev-container-smoke.py "$IMAGE"` before dispatch.
It checks real MCP and REST operations and requires native validation in the
unified image. Keep stdin open through the last response and report the dynamic
inventory. Use `python3 -m pytest iccdev-mcp/tests -q` for the focused scanner,
entrypoint, server, and REST contracts; do not use CI as the first validation.

## Safety Gates

- Do not mount the Docker socket, SSH keys, or GitHub token into untrusted code.
- Do not use custom C++ reproducers.
- Do not use `git clean` to mask a dirty image.
- Exit `1-127` is graceful; exit `128+` is signal termination.
- Attribute sanitizer findings by stack-frame paths.
- Preserve evidence outside the disposable container.

## References

- `../../../docs/regression-container.md`
- `../../../docs/regression-workflow-governance.md`
- `../../prompts/regression-container-maintainer.prompt.md`
- `../../instructions/build-system.instructions.md`
- `../../instructions/testing.instructions.md`
- `../../instructions/workflow-governance.instructions.md`
