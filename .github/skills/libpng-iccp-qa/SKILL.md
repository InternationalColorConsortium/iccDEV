---
name: libpng-iccp-qa
description: Reproduce and test pinned libpng iCCP stream-completion, Adler-32, trailing-output, and PNG CRC-policy faults with durable ICC fixtures.
---

# libpng iCCP Fault QA

Use this skill for libpng iCCP fault reproduction, the tracked libpng candidate
patch, or `.github/workflows/ci-libpng-iccp-smoke.yml`.

## Workflow

1. Keep libpng clones, build trees, generated PNG carriers, and logs outside
   the iccDEV worktree.
2. Read `.github/ci/tooling/libpng/Readme.md`. Use the exact pinned tag and
   revision; do not substitute the system libpng.
3. Configure `.github/ci/tooling/libpng/qa/CMakeLists.txt` with ASan+UBSan and
   build the `pngtest` target.
4. Before patching, run `libpng_iccp_qa.py --mode vulnerable` and require all
   four invalid profiles to survive. This is the negative proof.
5. Run `git apply --check`, apply every patch in lexical order, rebuild, and
   use `ctest -N --no-tests=error` to verify `iccdev.libpng-iccp-qa` exists.
6. Run that focused CTest with `--output-on-failure --no-tests=error`. Require
   invalid profiles to be absent, the valid control to match byte-for-byte,
   and the explicit relaxed bad-CRC policy to retain the profile.
7. Treat missing fixtures, hash drift, sanitizer diagnostics, signals, new
   compiler warnings, patch drift, or a missing CTest as failures.
8. Report iccDEV and libpng revisions, patch hash, compiler and sanitizer
   envelope, the before/after matrix, fixture hash, and all local changes.

Use the manually dispatched workflow for hosted validation. Do not add this
external dependency build to general iccDEV pull-request or tool-test jobs.
Upstream issue assignment is pending; do not conflate this fault with PAWG
profile parsing or claim that pnggroup/libpng #211 covers the full contract.
