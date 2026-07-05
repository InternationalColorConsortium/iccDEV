# Maintainer Regression Container Task

Use this prompt to run a repeatable maintainer investigation in
`iccdev-ci-regression`.

Canonical guide: `docs/regression-container.md`

## Inputs

- Operation: basic smoke / PR validation / issue reproduction
- Image tag (`latest` for initial interactive work, immutable SHA for reproducibility):
- PR number:
- Issue number:
- Branch or commit:
- Affected tool or component:
- Focused regression script:
- Focused CTest name:
- Input or invariant:
- Expected pass signal:
- Expected failure signal:
- Evidence directory:

## Required Workflow

1. Pull the selected image and record its digest and source revision. If using
   `latest`, verify both before starting.
2. Start with a clean container Git worktree. Stop and report if it is dirty.
3. For a PR, fetch `pull/<number>/head` and check out the fetched ref detached.
4. For an issue, use the smallest existing project input and project tool.
5. Rebuild the affected target or the full configured build.
6. Run the focused regression first and its CTest wrapper when registered.
7. For local PR proof, mount the reviewed worktree read-only into the published
   image and run the Docker PR verification build: strict Clang sanitizer flags,
   `iccDumpProfile`, and zero compiler warnings.
8. If the changed behavior is outside `iccDumpProfile`, build the affected tool
   and `build-test-binaries` inside the same image, then run the focused CTest.
9. Scan output for compiler warnings, ASAN, UBSAN, and signal termination.
10. Classify exit `1-127` as graceful and `128+` as signal termination.
11. Save evidence outside the disposable container.
12. If CI is requested, use the PR trigger or explicitly dispatch
    `ci-pr-action.yml`; do not assume a branch push triggers it.
13. Diff all `Dockerfile*` files between `ci-qa-pr-docker-testing` and
    `ci-qa-flags`, then carry applicable fixes and validation to both branches.
14. Include the `ci-qa-flags` commit and hosted run in the handoff.
15. Promote the verified immutable digest to `latest` only after all image smoke
    tests and regression CTest checks succeed: from `master`, or with the
    explicit `publish-regression-latest=true` dispatch on the protected
    Docker-testing branch.

## Safety

- Do not mount the Docker socket, SSH directory, or GitHub credentials into
  untrusted PR code.
- Do not create custom C++ reproducers.
- Do not use `git clean` to conceal a dirty published image.
- Do not classify by profile filename; use sanitizer stack-frame paths.
- Do not claim success without the exact tested SHA and command result.

## Handoff

Report:

- Image tag, digest, and revision.
- Tested PR, issue, branch, and commit.
- Build and test commands.
- Focused and CTest results.
- Exit status and sanitizer classification.
- Evidence paths.
- Workflow URLs and conclusions.
- Container defects or missing maintainer capabilities.
