---
name: heif-icc-qa
description: Build and test iccHeifDump against the pinned Nokia HEIF reader and the tracked valid, malformed, and overflow ICC carrier corpus.
---

# HEIF ICC Carrier QA

Use this skill for `iccHeifDump`, HEIF/AVIF `colr` carrier fixtures, the pinned
Nokia HEIF integration, or issue #2647 follow-up.

## Workflow

1. Check repository status and keep generated clones, builds, logs, and
   extracted profiles outside the worktree.
2. Read `.github/ci/tooling/heif/Readme.md` and use its pinned upstream commit.
   Homebrew `libheif` is not API-compatible with this tool.
3. Apply every tracked patch under `.github/ci/tooling/heif/patchs/` with
   `git apply --check` before configuring the wrapper project.
4. Build `iccHeifDump` through `.github/ci/tooling/heif/qa/CMakeLists.txt`.
5. Run `ctest -N --no-tests=error`, require exactly the documented HEIF test,
   then run `iccdev.heif-carrier-qa` with ASan+UBSan enabled.
   Use the manually dispatched `Nokia HEIF ICC carrier smoke` workflow for
   hosted validation; do not add this external build to the general iccDEV
   pull-request or tool-test workflows.
6. Treat exit values 128 and above, sanitizer diagnostics, missing fixtures,
   hash drift, or a missing CTest registration as failures. Exit 1-127 is a
   graceful tool failure only when the matrix expects it.
7. Report the iccDEV revision, Nokia revision, compiler/sanitizer envelope,
   carrier matrix result, extraction hash, known third-party warnings, and all
   local changes. New compiler warnings are failures in the dedicated smoke.

When changing the upstream pin or patch stack, test both `git apply --check`
and the complete sanitizer corpus. Do not silently regenerate or replace the
binary fixtures.
