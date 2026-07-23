# AFL++ CI Seeds

This directory holds tiny, reusable seeds for the manual AFL++ smoke workflow.
The workflow also reuses ICC fixtures from `.github/ci/test-data/`.

Keep these files small and deterministic. Do not store AFL output queues,
crashes, hangs, coverage reports, or profiler data here.

Current target seed sources:

- `dump`: `.github/ci/test-data/*.icc`
- `toxml`: `.github/ci/test-data/*.icc`
- `fromcube`: `.github/ci/test-data/test-identity.cube` plus `cube/*.cube`
