# Valgrind QA Component

This directory is the executable source of truth for iccDEV Memcheck,
Helgrind, DRD, Massif, and Callgrind lanes. It mirrors the repository-owned
AFL++ and CFL helpers while keeping dynamic analysis in a separate,
non-sanitized Debug build. Shared cache, dependency, symbol, and string probes
fail closed when sanitizer instrumentation is present or cannot be ruled out.

Use `build.sh` to configure the build, `run.sh` to execute named lanes,
`status.sh` to summarize evidence, `validate.sh` to check the registry, and
`self-test.sh` to verify build isolation and failure classification after the
`dump` target is built. User and maintainer commands live in
`docs/valgrind-analysis.md`.

The older `.github/scripts/iccdev-valgrind-qa.sh` remains the focused
before/after regression helper for the historical `GetNewApplyCmm()` race.
