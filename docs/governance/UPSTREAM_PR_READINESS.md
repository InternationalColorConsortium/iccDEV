# Upstream Pull Request Authorization and Readiness {#upstream-pr-readiness}

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
8. A `base...HEAD` contract matrix covers every changed cross-cutting surface:
   producer, consumer, build/runtime behavior, supported toolchain or platform,
   CI trigger, dependency owner, and local evidence. Compiler-specific defaults
   and explicit overrides are both validated; runtime suppression syntax is
   verified by the runtime rather than inferred from compile-time categories.
9. Applicable local checks use the current maintainer image and tools, with
   its digest and source revision recorded. The pushed pre-PR head has
   terminal, passing `ci-qa-pr` results with `ctest-mode=full` and
   `ctest-recent-limit=0`, plus `ci-codeql-security` results on the same SHA.
   Any head change after screening requires renewed applicable evidence.
10. Generated artifacts are removed or intentionally documented.
11. Active and suppressed automated-review findings are inspected together,
    including review summaries because suppressed findings may have no thread.
12. Documentation and reported evidence describe actual runtime behavior, not
    only compile-time eligibility.
13. The user explicitly authorizes PR creation.

Any incomplete item means the branch remains branch-only. After an authorized
PR opens, run `ci-pr-action` with `ci_scope=full` on its head and record its
cross-platform build, profile, and representative CTest results. This final
PR gate does not replace the local or pre-PR screening above.

## Review Cycle Stop Rule

Automated review is a final verification gate, not the implementation loop.

- Request one complete review of the frozen head and cumulative diff.
- Resolve its findings branch-only, repeat the readiness gate and complete
  contract matrix, then request one complete re-review only when a maintainer
  needs it.
- If that re-review identifies any new blocker, including one in the repair,
  stop.
- Return to branch-only grooming and perform a complete diff/configuration
  audit. Do not request another automated review without maintainer direction.

## Required Evidence

Record:

```text
authorization: exact user quotation
base: target branch and fetched SHA
range-diff: reviewed
merge commits: none
maintainer image: digest and source revision, if used
build: command and result
tests: command, pass count, skip count
negative tests: cases and results
pre-PR hosted: exact head SHA; ci-qa-pr full/0 and ci-codeql-security run IDs and results
PR-head final gate: ci-pr-action run ID and result, if PR is open
contract matrix: changed surface, producer, consumer, boundary, and evidence
review inventory: review IDs plus active and suppressed findings from threads and summaries
scope documentation: checked
readiness: PASS or FAIL
```

Use `.github/skills/upstream-pr-readiness/SKILL.md` for the executable workflow.
