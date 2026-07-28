# iccDEV Stacked PR Workflow

Use this skill when an iccDEV maintainer asks for GitHub Stacked PR creation,
Stacked PR enablement testing, or `gh stack` workflow grooming.

## Rules

1. Use a clean clone or disposable worktree under `~/work/copilot/`.
2. Fetch `origin/master` and every branch or commit that will become a stack
   layer.
3. Keep the branch chain linear. Use `git cherry-pick`, not merge.
4. Put branches in dependency order: bottom branch targets `master`; each branch
   above targets the branch below it.
5. Run local build and CTest unless the maintainer explicitly says to skip them.
6. Use `gh stack init --base master <branches...>` to adopt the chain.
7. Use `gh stack submit --auto` for noninteractive draft PR creation. Do not pass
   `--open` unless the maintainer asks for ready-for-review PRs.
8. After submission, verify PR base branches and draft state with `gh pr view`.

## Workflow

```bash
repo="$HOME/work/copilot/iccdev-stacked-pr"
remote=git@github.com:InternationalColorConsortium/iccDEV.git
branches="branch-one branch-two"

rm -rf "$repo"
git clone "$remote" "$repo"
cd "$repo"
git fetch --prune origin master $branches

git checkout -B branch-one origin/master
git cherry-pick <branch-one-commit>

git checkout -B branch-two branch-one
git cherry-pick <branch-two-commit>

git status --short --branch
git log --oneline --decorate --graph --boundary origin/master..HEAD
git diff --check origin/master..HEAD
```

Validate:

```bash
build_dir=/tmp/iccdev-stacked-pr-build
rm -rf "$build_dir"
cmake -S Build/Cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build "$build_dir" -j"$(nproc)"
cmake --build "$build_dir" --target build-test-binaries --parallel "$(nproc)"
ctest --test-dir "$build_dir" --output-on-failure
```

Submit:

```bash
gh stack init --base master branch-one branch-two
gh stack view --short
gh stack submit --auto
```

Verify:

```bash
gh stack view --json
gh pr view branch-one --json number,url,isDraft,baseRefName,headRefName
gh pr view branch-two --json number,url,isDraft,baseRefName,headRefName
```

## Report

Report the stack base, branch order, PR URLs, draft state, validation commands,
and any known skips or generated artifacts.
