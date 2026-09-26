# iccDEV Valgrind Analysis Task

Use the registered Valgrind component for a bounded dynamic-analysis run.

Canonical guide: `docs/valgrind-analysis.md`

## Inputs

- Source branch and commit:
- Analyzer: Memcheck / Helgrind / DRD / Massif / Callgrind
- Registered target or targets:
- Optional ICC corpus and report tool (`pawg` or `dump`):
- Native host or unified image:
- Per-target timeout:
- Expected clean or finding state:
- Evidence directory:

## Required workflow

1. Validate the target registry.
2. Build only the selected lanes in a separate non-sanitized Debug tree.
3. Verify the cache has no sanitizer flags and the target binary has no
   sanitizer instrumentation, including statically linked runtimes.
4. For runner or image QA, build `dump` and run the Valgrind self-test.
5. Run the selected analyzer with a bounded timeout.
6. Preserve `summary.tsv`, analyzer logs, stdout, stderr, and generated files.
7. Treat a timeout or analyzer error as a failure unless the task explicitly
   requests known-finding evidence.
8. For a race investigation, compare Helgrind and DRD and state when their
   results differ.
9. For issue #2592 or threaded row-application notification changes, run the
   focused `issue-2592-regression.sh` wrapper and compare its generated TIFFs.
10. Report exact commands, source revision, exit codes, error counts, and paths.
11. For a corpus task, use `corpus.sh`, verify paths containing spaces, and
    preserve the per-profile analyzer logs and summary.
12. For a Docker task, preserve evidence outside the container and record the
    image digest and source revision. Prefer tar streaming when host and image
    user IDs differ.

Do not create a standalone C++ reproducer. Use registered project tools,
regression executables, and durable ICC/XML/JSON/TIFF inputs.
