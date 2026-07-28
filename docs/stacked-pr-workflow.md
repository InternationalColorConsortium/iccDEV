# Stacked PR Workflow

Use this workflow when a maintainer wants several focused pull requests reviewed
as one ordered stack. A stack is a chain of PRs in the same repository where the
bottom PR targets `master` and each PR above it targets the branch immediately
below it.

## When to Use a Stack

- A change naturally splits into independently reviewable layers.
- A follow-up change depends on an earlier branch but should keep a focused diff.
- Maintainers want the whole series to be visible and mergeable from the GitHub
  stack UI.

Use the existing [linear stack workflow](linear-stack-workflow.md) when the goal
is only to rewrite one branch into a linear history. Use this workflow when the
output should be multiple linked pull requests.

## Requirements

- GitHub CLI authenticated with push and PR permissions.
- `gh stack` installed: `gh extension install github/gh-stack`.
- A clean clone or disposable worktree. Do not create stacks from a dirty local
  development checkout.
- A fully linear branch chain. The top branch must contain the bottom branch in
  its history.

Check local enablement:

```bash
gh extension list | grep 'github/gh-stack'
gh stack --help
```

## Build the Branch Chain

Start from a clean clone:

```bash
repo="$HOME/work/copilot/iccdev-stacked-pr"
remote=git@github.com:InternationalColorConsortium/iccDEV.git
mkdir -p "$(dirname "$repo")"
git clone "$remote" "$repo"
cd "$repo"
git fetch --prune origin master branch-one branch-two
```

Create the bottom branch from `origin/master` and cherry-pick its commit:

```bash
git checkout -B branch-one origin/master
git cherry-pick <branch-one-commit>
```

Create the next layer from the bottom branch and cherry-pick only the unique
commit for that layer:

```bash
git checkout -B branch-two branch-one
git cherry-pick <branch-two-commit>
```

Repeat for each additional layer. Do not merge branches into the stack.

## Review and Validate

Before submitting the stack:

```bash
git status --short --branch
git log --oneline --decorate --graph --boundary origin/master..HEAD
git diff --check origin/master..HEAD
```

For C/C++ or workflow changes, run a fresh local build and CTest:

```bash
build_dir=/tmp/iccdev-stacked-pr-build
rm -rf "$build_dir"
cmake -S Build/Cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build "$build_dir" -j"$(nproc)"
cmake --build "$build_dir" --target build-test-binaries --parallel "$(nproc)"
ctest --test-dir "$build_dir" --output-on-failure
```

Restore generated profile artifacts if CTest rewrites tracked examples.

## Submit Draft Stacked PRs

Adopt the local branch chain and submit draft PRs:

```bash
gh stack init --base master branch-one branch-two
gh stack view --short
gh stack submit --auto
```

`gh stack submit --auto` creates new PRs as drafts unless `--open` is passed.
For existing PRs, use `gh pr ready --undo <number>` if they need to remain
drafts after stack submission.

Expected PR bases:

```text
master <- branch-one <- branch-two <- branch-three
```

The GitHub UI should show a stack map on each PR. CI and branch protection are
evaluated against the stack base, even for PRs whose direct base is another
stack branch.

## Update or Rebase a Stack

Use the stack CLI for cascading updates:

```bash
gh stack sync
gh stack rebase
gh stack push
```

`gh stack push` uses force-with-lease and atomic push behavior for tracked stack
branches. If the stack diverges or a branch moved remotely, stop and inspect the
range before pushing.

## Reporting Checklist

- Stack base and branch order.
- PR URLs and draft/open state.
- Local `gh stack` version or extension version.
- Local validation commands and results.
- Hosted workflow run URLs, if triggered.
- Known skips or generated artifacts restored.
