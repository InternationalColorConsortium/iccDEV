# Linear Stack Workflow

This workflow is for maintainers who need to refresh a feature branch on top of
current `master`, then stack a squash commit or another feature commit above it
without creating a merge commit.

Use it for branch updates such as:

- rebase `pip-install-iccdev` on `origin/master`;
- cherry-pick a reviewed feature commit on top;
- resolve overlaps with a stated preference;
- build and test locally;
- update the remote branch with an exact force-with-lease.

## Principles

1. Keep history linear. Use `git rebase` and `git cherry-pick`, not merge.
2. Work in a clean clone or disposable worktree. Do not rewrite from a dirty
   development checkout.
3. Fetch immediately before rewriting and immediately before pushing.
4. Treat conflict preference as explicit. If a maintainer says to prefer the
   stacked commit, `git cherry-pick -X theirs <commit>` makes `theirs` mean the
   commit being picked.
5. Review the rewritten stack with `git range-diff`.
6. Build and run CTest locally.
7. Push with an exact `--force-with-lease`; never use plain `--force`.

## Example: Rebase and Stack One Commit

Set variables:

```bash
repo=/home/h02332/work/copilot/iccdev-linear-stack
remote=git@github.com:InternationalColorConsortium/iccDEV.git
target=pip-install-iccdev
extra_commit=7fb0b2087e66946173cd49ad3289d435ba13a8e1
build_dir=/tmp/iccdev-linear-stack-build
```

Prepare the clean clone:

```bash
mkdir -p "$(dirname "$repo")"
if [ -d "$repo/.git" ]; then
  cd "$repo"
  git remote set-url origin "$remote"
  git fetch --prune origin master "$target"
else
  git clone "$remote" "$repo"
  cd "$repo"
  git fetch --prune origin master "$target"
fi
git checkout -B "$target" "origin/$target"
git status --short --branch
```

Rebase the target branch:

```bash
git rebase origin/master
```

If conflicts occur:

```bash
git status --short --branch
git diff --cc
git add <resolved-files>
git -c core.editor=true rebase --continue
```

Stack the extra commit without a merge commit:

```bash
git fetch origin add-icc-connect
git cherry-pick -X theirs "$extra_commit"
```

If conflicts remain after the strategy option, inspect and resolve them
manually:

```bash
git status --short --branch
git diff --cc
git add <resolved-files>
git -c core.editor=true cherry-pick --continue
```

## Review Before Testing

Check the stack and whitespace:

```bash
git status --short --branch
git log --oneline --decorate --graph --boundary "origin/master..HEAD"
git range-diff origin/master "origin/$target" HEAD
git diff --check origin/master..HEAD
```

The expected shape is:

```text
<extra commit>    Refactor: add IccConnect library
<target commit>   ci: tighten XML and package guardrails
<target commit>   feat: add installable iccDEV bindings and MCP tools
<master commit>   origin/master
```

## Local Validation

Use a fresh build directory:

```bash
rm -rf "$build_dir"
cmake -S Build/Cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build "$build_dir" -j"$(nproc)"
ctest --test-dir "$build_dir" --output-on-failure
```

If CTest regenerates tracked profile files, restore only those generated
artifacts after tests pass:

```bash
git status --short
git restore -- Testing/mcs/Flexo-CMYKOGP/*.icc
git status --short --branch
```

## Push Safely

Fetch and use the exact remote SHA as the force-with-lease value:

```bash
git fetch --prune origin master "$target"
lease=$(git rev-parse "origin/$target")
git push --force-with-lease="refs/heads/$target:$lease" origin "$target"
git fetch origin "$target"
git log --oneline --decorate --graph --boundary "origin/master..origin/$target"
```

If the branch moved after your last review, the lease protects the remote from
being overwritten. Stop, fetch, run `git range-diff` again, and re-test if the
stack changed.

## Troubleshooting

| Symptom | Cause | Action |
|---------|-------|--------|
| Git prompts for an HTTPS username | Remote URL is HTTPS but only SSH auth is configured | `git remote set-url origin git@github.com:InternationalColorConsortium/iccDEV.git` |
| Rebase creates a merge commit | `git pull` or merge was used | Abort, reset the disposable clone, and use `git rebase origin/master` |
| `-X theirs` keeps the wrong side | The command was not a cherry-pick | Remember: in cherry-pick, `theirs` is the picked commit; in other operations, inspect before assuming |
| CMake sees stale generated headers | Reused build directory | Delete the build directory and configure again |
| CTest changes tracked `.icc` files | Profile generation rewrote committed examples | Restore generated artifacts after tests pass |
