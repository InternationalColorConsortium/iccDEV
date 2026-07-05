---
name: regression-container-maintainer
description: >
  Use the published iccDEV regression container for maintainer smoke testing,
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

# Regression Container Maintainer

Use this skill for repeatable maintainer operations in
`ghcr.io/internationalcolorconsortium/iccdev-ci-regression`.

## Required Inputs

1. Operation: basic smoke, PR validation, or issue reproduction.
2. Image tag: `latest` for initial interactive work or an immutable SHA tag for
   reproducible validation.
3. PR, issue, branch, or commit reference.
4. Affected tool and smallest focused regression.
5. Expected pass and failure signals.
6. Host evidence directory.

## Workflow

1. Read `../../../docs/regression-container.md`.
2. Pull the image and record its digest and source revision. For `latest`, verify
   both values before using the image.
3. Mount only an evidence directory and start a disposable container.
4. Require a clean initial Git worktree.
5. For a PR, fetch `pull/<number>/head` and check out the ref detached.
6. For an issue, reproduce with existing project tools and saved inputs.
7. Rebuild the affected target, then run the focused regression and registered
   CTest wrapper.
8. For local PR proof, mount the reviewed worktree read-only into the published
   image and run the Docker PR verification build (`iccDumpProfile` with the
   strict Clang sanitizer flags) before handoff.
9. Build the affected tool and `build-test-binaries` inside the same image when
   the PR-specific behavior is outside `iccDumpProfile`, then run the focused
   CTest wrapper.
10. Run broader CTest or GCC 15.2 strict parity only after the focused check
    passes.
11. Scan logs for compiler warnings, ASAN, UBSAN, and signal termination.
12. Trigger `ci-pr-action.yml` explicitly for a pre-PR branch; a push alone does
    not trigger that workflow.
13. Compare all `Dockerfile*` files between `ci-qa-pr-docker-testing` and
    `ci-qa-flags`; carry every applicable container fix to both branches.
14. Confirm the `ci-qa-flags` branch update and hosted validation explicitly.
15. Promote the verified immutable digest to `latest` only after all image smoke
    tests and regression CTest checks succeed: automatically from `master`, or
    explicitly from the protected Docker-testing branch with
    `publish-regression-latest=true`. Never promote it from other branches.
16. Report exact image tag, digest, source revision, commands, results, evidence,
    and workflow URLs.

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
