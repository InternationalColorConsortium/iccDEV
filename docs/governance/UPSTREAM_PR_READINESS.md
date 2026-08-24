# Upstream Pull Request Authorization and Readiness

Use this policy before creating, reopening, or requesting review on an
upstream pull request.

## Authorization Boundary

Only explicit user language such as "open a PR", "create the pull request", or
"reopen PR N" authorizes PR publication.

The following requests do not authorize PR creation:

- push the branch;
- trigger CI;
- run a workflow whose name contains `pr`;
- request or monitor a review;
- prepare a PR description;
- synchronize related branches.

If a requested workflow requires an open PR, report the prerequisite and ask
for authorization. Do not create the PR under the user's authenticated account.

## Readiness Gate

All items must pass before an authorized PR is opened or a review is requested:

1. Scope is frozen and the PR description covers every feature, ISA path,
   toolchain, workflow, and behavior change.
2. The branch is rebased on the current target branch in a clean worktree.
3. History is linear: no merge commits; `git range-diff` is reviewed.
4. `git diff --check` passes and the complete diff is manually reviewed.
5. Normal build targets and project-specific excluded test helpers are built.
6. The complete applicable test suite passes locally.
7. Positive and negative configuration cases pass, including Debug/Release,
   compiler, sanitizer, diagnostics, runtime CPU dispatch, and fallback paths.
8. Generated artifacts are removed or intentionally documented.
9. Active and suppressed automated-review findings are inspected together.
10. Documentation and reported evidence describe actual runtime behavior, not
    only compile-time eligibility.
11. The user explicitly authorizes PR creation.

Any incomplete item means the branch remains branch-only.

## Review Cycle Stop Rule

Automated review is a final verification gate, not the implementation loop.

- Request one complete review of the frozen head and cumulative diff.
- Resolve its findings branch-only, repeat the readiness gate, then request one
  complete re-review only when a maintainer needs it.
- If that re-review identifies a missed issue in unchanged code, stop.
- Return to branch-only grooming and perform a complete diff/configuration
  audit. Do not request another automated review without maintainer direction.

## Required Evidence

Record:

```text
authorization: exact user quotation
base: target branch and fetched SHA
range-diff: reviewed
merge commits: none
build: command and result
tests: command, pass count, skip count
negative tests: cases and results
active and suppressed findings: checked
scope documentation: checked
readiness: PASS or FAIL
```

Use `.github/skills/upstream-pr-readiness/SKILL.md` for the executable workflow.
