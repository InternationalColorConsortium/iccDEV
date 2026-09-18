---
name: valgrind-analysis
description: >
  Build and run iccDEV tool lanes under Memcheck, Helgrind, DRD, Massif, or
  Callgrind while preserving analyzer evidence.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(docker:*)
---

# Valgrind Analysis

Use this skill for uninitialized-memory, invalid-access, leak, synchronization,
heap-growth, or call-profile investigations against iccDEV tools.

## Workflow

1. Read `../../../docs/valgrind-analysis.md`.
2. Select the smallest registered target that reaches the affected path.
3. Run `.github/ci/valgrind/build.sh --target TARGET` to produce a separate
   non-sanitized Debug build.
4. Confirm the build rejects cached sanitizer flags and sanitizer-instrumented
   binaries, including statically linked runtimes.
5. For runner or image QA, run `.github/ci/valgrind/self-test.sh` after building
   the `dump` target.
6. Run `.github/ci/valgrind/run.sh --tool TOOL TARGET` with a bounded timeout.
7. Preserve the generated `summary.tsv`, analyzer log, stdout, stderr, and
   generated work files.
8. For concurrency, compare Helgrind and DRD rather than treating either as a
   substitute for TSan.
9. Attribute findings to stack frames and observed synchronization behavior,
   not input filenames.
10. Report the exact source revision, command, exit code, error count, and
   evidence directory.

Use `--allow-findings` only when collecting a known or deliberately compared
finding. Never use it to turn an unexplained result into a clean gate.

## Container path

The unified image exposes `iccdev-valgrind-build`, `iccdev-valgrind-run`,
`iccdev-valgrind-status`, `iccdev-valgrind-validate`, and
`iccdev-valgrind-self-test`. These commands write to `/workspace/valgrind` by
default and must not wrap `/workspace/build`.

## References

- `../../../docs/valgrind-analysis.md`
- `../../prompts/valgrind-analysis.prompt.md`
- `../sanitizer-repro/SKILL.md`
- `../regression-container-maintainer/SKILL.md`
