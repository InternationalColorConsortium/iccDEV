---
name: stacked-pr-fast-lane
description: >
  Create and synchronize focused gh-stack pull request layers, then prepare
  guarded maintainer fast-lane validation through the GitHub CLI.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(gh:*)
---

# Stacked PR and Fast-Lane Workflow

Use this skill when related work will create multiple pull requests in a
rolling 24-hour period, when a PR stack is already checked out, or when a
maintainer asks for `ci-pr-action` fast-lane validation.

## Stack Policy

- Prefer a single linear stack for related changes. Each layer must contain one
  independently reviewable concern and target the branch immediately below it.
- Use separate stacks for unrelated or parallel work. Do not use a stack to
  combine opportunistic cleanup with the requested change.
- Before stack work, update the plugin and inspect the tracked state:

  ```bash
  gh extension upgrade gh-stack
  gh stack view --json
  ```

- Use `gh stack init <bottom-branch>` before writing a new multi-part change
  and `gh stack add <next-branch>` for each dependent layer.
- Use `gh stack sync --remote origin` before handoff and after remote/base
  changes. Use `gh stack submit --auto --remote origin` to create or update
  the PRs. Do not manually rebase or push one stack layer.
- For noninteractive agents, never invoke the TUI-only `gh stack modify` or
  `gh stack switch`. Use explicit branch/PR arguments and JSON output.

## Agent Responsibilities

1. Check the current stack with `gh stack view --json`.
2. Keep the active branch limited to its owning layer. If the owner is unclear,
   inspect history before editing.
3. Run the smallest validation for that layer; do not run the full CTest suite
   merely because the work is stacked.
4. Synchronize through `gh stack sync --remote origin` before reporting branch
   or PR state.
5. Report the stack order, layer branch, head SHA, focused validation, and
   dependency on the layer below.

Do not dispatch fast lane for a fork, a closed PR, or an untrusted contributor.
An agent can prepare commands and inspect runs, but cannot treat fast-lane
success as merge approval.

## Maintainer Fast Lane

Fast lane applies only to an open same-repository PR and runs exact GCC 15.2
Release LTO plus GCC 15.2 ASAN+UBSAN Debug tool validation. It defaults to
the latest CTest, with Windows opt-in. Docker verification is scheduled whenever
the selected PR changes the container surface. It is an accelerator, not a
replacement for required PR gates or the long-cycle `ci_scope=full` gate.

Confirm the target before dispatch:

```bash
gh pr view <pr-number> --repo InternationalColorConsortium/iccDEV \
  --json number,state,isCrossRepository,headRefName,headRefOid
```

Only a maintainer with current repository permission may dispatch:

```bash
gh workflow run ci-pr-action --repo InternationalColorConsortium/iccDEV \
  --ref <branch> \
  -f ci_scope=fast-lane \
  -f pr_number=<open-same-repository-pr-number> \
  -f ctest_recent_limit=1 \
  -f include_windows=false \
  -f warning_policy=fail
```

Find and watch the resulting run:

```bash
gh run list --repo InternationalColorConsortium/iccDEV \
  --workflow ci-pr-action --branch <branch> --event workflow_dispatch \
  --json databaseId,headSha,status,conclusion --limit 1
gh run watch <run-id> --repo InternationalColorConsortium/iccDEV --exit-status
```

Trigger shared-concurrency runs sequentially. Add Windows only when the change
needs that surface. Docker is scheduled for container changes and is advisory on
`ci-qa-pr-docker-testing`. Escalate to `ci_scope=full` when the changed surface
or merge policy requires the long cycle.

## References

- `../../copilot-instructions.md`
- `../../../AGENTS.md`
- `../maintainer-ci-ctest/SKILL.md`
- `../../../docs/pre-pr-security-cycle.md`
- `../../../docs/label-system.md`
- `../../../docs/regression-container.md`
