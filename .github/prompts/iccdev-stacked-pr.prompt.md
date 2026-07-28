# iccDEV Stacked PR Prompt

Use this prompt when creating or reviewing a GitHub Stacked PR series for
iccDEV.

## Inputs

- Stack base:
- Branches, bottom to top:
- Source commits for each branch:
- Issue or PR references:
- Changed surfaces:
- Required local validation:

## Procedure

1. Work from a clean clone under `~/work/copilot/`.
2. Confirm `gh stack` is installed with `gh extension list` and `gh stack --help`.
3. Fetch `master` and every source branch.
4. Build a linear branch chain with `git checkout -B` and `git cherry-pick`.
5. Review `git log --oneline --graph --boundary origin/master..HEAD` and
   `git diff --check origin/master..HEAD`.
6. Run the smallest complete local build and CTest set for the changed surface.
7. Adopt the stack with `gh stack init --base master <branches...>`.
8. Submit with `gh stack submit --auto` to create draft PRs.
9. Confirm every PR is draft and has the expected base branch.

## Handoff

- Stack order:
- PR URLs:
- Draft state:
- Local validation:
- Hosted validation:
- Notes or skipped checks:

## References

- `../../docs/stacked-pr-workflow.md`
- `../skills/iccdev-stacked-pr/SKILL.md`
- https://github.github.com/gh-stack/
