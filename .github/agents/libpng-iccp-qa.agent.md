---
description: Run pinned libpng iCCP fault reproduction and candidate-patch QA
---

# libpng iCCP QA Agent

Use `.github/ci/tooling/libpng/Readme.md` and
`.github/skills/libpng-iccp-qa/SKILL.md`.

1. Use the exact pinned upstream tag and revision.
2. Keep dependency and build state outside the source worktree.
3. Prove the vulnerable contract before applying the tracked patch.
4. Apply the patch only after `git apply --check` succeeds.
5. Rebuild under ASan+UBSan and reject new compiler warnings.
6. Discover tests with `ctest -N --no-tests=error`, then run the focused test.
7. Require invalid profiles to be discarded and valid or explicitly relaxed
   profiles to be preserved byte-for-byte.
8. Report revisions, hashes, environment, before/after matrix, CTest evidence,
   and every local change.

Do not create standalone C++ reproducers, modify libpng outside the temporary
checkout, or represent the candidate patch as an iccDEV runtime change.
