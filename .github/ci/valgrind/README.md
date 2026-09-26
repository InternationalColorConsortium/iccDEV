# Valgrind QA Component

This directory is the executable source of truth for iccDEV Memcheck,
Helgrind, DRD, Massif, and Callgrind lanes. It mirrors the repository-owned
AFL++ and CFL helpers while keeping dynamic analysis in a separate,
non-sanitized Debug build. Shared cache, dependency, symbol, and string probes
fail closed when sanitizer instrumentation is present or cannot be ruled out.

Use `build.sh` to configure the build, `run.sh` to execute named lanes,
`corpus.sh` to sweep `.icc` files with `iccPawgReport` or `iccDumpProfile`,
`status.sh` to summarize evidence, `validate.sh` to check the registry, and
`self-test.sh` to verify build isolation and failure classification after the
`dump` target is built. User and maintainer commands live in
`docs/valgrind-analysis.md`.

`issue-2592-regression.sh` builds `applyprofiles-row`, requires clean DRD and
Helgrind runs, and verifies that both analyzers produce the same output TIFF.

`corpus.sh` accepts paths containing spaces, applies a per-profile timeout, and
stores a summary, analyzer log, stdout, stderr, and profiler output for every
input. Build the matching `pawg` or `dump` target before invoking it.

The older `.github/scripts/iccdev-valgrind-qa.sh` remains the focused
before/after regression helper for the historical `GetNewApplyCmm()` race.
