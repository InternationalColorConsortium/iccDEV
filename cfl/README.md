# CFL Smoke Fuzzing

This directory contains the maintainer-owned CFL smoke entry point for the
current AFL/CFL onboarding branch. It intentionally starts with the core
iccDEV command-line surfaces:

The CFL smoke harnesses are experimental maintainer validation scaffolding.
They are registered for manual or reusable workflow use from `master` and
integration branches; they are not a mandatory CI quality gate and they do not
replace normal source-level regression tests. Findings from these harnesses
should be promoted only after replay with an existing iccDEV tool and a durable
input artifact.

- `dump`: `iccDumpProfile input.icc ALL`
- `toxml`: `iccToXml input.icc output.xml`
- `fromxml`: `iccFromXml input.xml output.icc`
- `tojson`: `iccToJson input.icc output.json`
- `fromjson`: `iccFromJson input.json output.icc`
- `roundtrip`: `iccRoundTrip input.icc 1 0`

Run the local smoke with:

```bash
cfl/build.sh --runs 1
```

Apply the local CFL patch stack before configuring iccDEV with `--patches`.
The
`ci-cfl-smoke` workflow applies `.github/ci/fuzz-patches/cfl` by default so
patch branches test local fixes before they are promoted to source PRs:

```bash
cfl/build.sh --patches --runs 1
```

The default patch directory is `.github/ci/fuzz-patches/cfl`. The harnesses are
CLI-fidelity wrappers: each libFuzzer input is written to a temporary file and
replayed through the matching sanitized iccDEV tool. This keeps onboarding
stable while deeper in-process harnesses mature separately.

Do not commit generated `cfl/bin`, `.cfl-smoke`, build trees, crash artifacts,
coverage output, or profiler data.
