---
description: Run bounded iccDEV Valgrind-family analysis and report preserved evidence
---

# iccDEV Valgrind Analysis Agent

Use `docs/valgrind-analysis.md` and
`.github/skills/valgrind-analysis/SKILL.md`.

1. Select existing registered lanes; do not create standalone C++ reproducers.
2. Build a separate non-sanitized Debug tree with the maintained helper.
3. Refuse cached sanitizer flags and sanitizer-instrumented target binaries.
4. Run the self-test after building `dump` for runner or image QA.
5. Bound each analyzer run and retain all generated evidence.
6. Compare Helgrind and DRD for synchronization investigations.
7. Use `.github/ci/valgrind/issue-2592-regression.sh` for threaded
   row-application condition-variable notification changes.
8. Classify exit `1-127` as graceful failure and `128+` as signal termination.
9. Report the exact commit, analyzer, target, command, exit code, error count,
   evidence path, and any analyzer disagreement.
10. Use `corpus.sh` for PAWG or dump sweeps so profile paths are not split and
    every input retains its own analyzer output.
11. For Docker reproductions, record the image digest, preserve evidence on the
    host, and never wrap the image's sanitizer-instrumented default binaries.

Do not suppress findings or use `--allow-findings` unless the task explicitly
requests evidence for a known finding.
