# Unified iccDEV Container Task

Use this prompt to run a repeatable maintainer investigation in
`iccdev`.

Canonical guide: `docs/regression-container.md`

## Inputs

- Operation: basic smoke / PR validation / issue reproduction
- Image selector (`latest`, full-SHA tag, or release tag; resolved at run time):
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
- AFL/CFL scope: none / AFL smoke / CFL smoke / both

## Required Workflow

1. Pull the selected tag, resolve and record its digest and source revision,
   then execute the digest. Do not hardcode a particular SHA as a reusable
   workflow default.
2. Start with a clean container Git worktree. Stop and report if it is dirty.
3. For a PR, fetch `pull/<number>/head` and check out the fetched ref detached.
4. For an issue, use the smallest existing project input and project tool.
5. Rebuild the affected target or the full configured build.
6. Run the focused regression first and its CTest wrapper when registered.
7. For local PR proof, pull the published `latest` image, record its resolved
   digest, mount the reviewed worktree read-only, and copy it to container-local
   scratch space. Run the local canonical-image build with the configured tool
   and test target set, zero compiler warnings, and CTest excluding only the
   `slow` and `calculator` labels.
8. If the changed behavior is in an excluded suite, run its focused CTest in
   addition to the local container envelope.
9. Scan output for compiler warnings, ASAN, UBSAN, and signal termination.
10. Classify exit `1-127` as graceful and `128+` as signal termination.
11. For AFL/CFL work, run `iccdev-fuzz-env`, verify patch-stack counts, and run
    the smallest patched or unpatched smoke that matches the requested scope.
12. Save evidence outside the disposable container.
13. If CI is requested, use the PR trigger or explicitly dispatch
    `ci-pr-action.yml`; do not assume a branch push triggers it.
14. Use only `latest`, full-SHA, or release tags as selectors and resolve them
    at run time. Existing legacy tags are continuity-only; do not create,
    recommend, or depend on branch, run, or image-variant tags.
15. Include the canonical image digest and hosted run in the handoff.

For MCP/REST container changes, run the reusable runtime gate before dispatch:
`python3 .github/scripts/iccdev-container-smoke.py "$IMAGE" --report-dir out/container-smoke`.
Run `python3 -m pytest iccdev-mcp/tests -q` for the focused scanner, native
override, entrypoint, and REST contracts. Report exact discovered inventories,
not a fixed healthy-tool total. Do not close MCP stdin before tool responses.

For AFL/CFL work, keep the review scope narrow: these helpers are experimental
maintainer workflows and local validation patch stacks. They should support
manual fuzz runs from `master` or an integration branch, not redefine required
project CI or merge fuzz patches as source hardening.

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
- `iccdev-fuzz-env` output summary when AFL/CFL is in scope.
- Focused and CTest results.
- Exit status and sanitizer classification.
- Evidence paths.
- Workflow URLs and conclusions.
- Container defects or missing maintainer capabilities.
