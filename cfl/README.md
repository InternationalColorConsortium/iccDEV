# CFL Smoke Fuzzing

This directory contains the maintainer-owned CFL smoke entry point for the
current AFL/CFL onboarding branch. It covers the core iccDEV command-line
surfaces and the public profile-visualization model:

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
- `profilevisualize`: parses an in-memory ICC profile, enumerates public
  `IccVizModel.hpp` descriptors, and renders every graph or raster descriptor
- `writerserialize`: renders the same descriptors and then serializes them
  through `Mini{PDF,SVG,TIFF}` — the IFD layout and offset arithmetic, the PDF
  object graph and xref table, and `SVGOut` (#2116)

Run the local smoke with:

```bash
cfl/build.sh --seconds 30
```

Apply the local CFL patch stack before configuring iccDEV with `--patches`.
The
`ci-cfl-smoke` workflow applies `.github/ci/fuzz-patches/cfl` by default so
patch branches test local fixes before they are promoted to source PRs:

```bash
cfl/build.sh --patches --seconds 30
```

The default patch directory is `.github/ci/fuzz-patches/cfl`. The six command-
line harnesses are CLI-fidelity wrappers: each libFuzzer input is written to a
temporary file and replayed through the matching sanitized iccDEV tool.

`profilevisualize` and `writerserialize` are in-process. They compile the engine
sources as separate translation units and consume only public headers; do not
include a CLI implementation file or call `processLuts()` from either.
`writerserialize` links `Mini{PDF,SVG,TIFF}.cpp` in addition, because the
serialization entries it drives are not reachable from `IccVizModel` alone.

Build and run only the in-process harnesses with:

```bash
cfl/build.sh --targets profilevisualize,writerserialize --seconds 30
```

A CLUT-bearing seed has to survive into the corpus for either one to render a
raster, and two independent gates used to remove the only such seed (#2120):
`max_seed_bytes` deleted it from the corpus, and libFuzzer's `max_len`
truncated whatever survived. Both defaults are now above it, and pruning a seed
committed under `.github/ci/test-data` is a hard error rather than a silent
`rm`, so a future oversized regression seed forces a decision about the cap
instead of quietly costing coverage.

Note the `.options` files are not read by `build.sh` — libFuzzer binaries do not
consume them; they are the ClusterFuzz/OSS-Fuzz runner convention. `build.sh`
passes `-max_len`, `-timeout`, `-rss_limit_mb` and `-use_value_profile`
explicitly, so changing a value means changing it in both places.

Do not commit generated `cfl/bin`, `.cfl-smoke`, build trees, crash artifacts,
coverage output, or profiler data.
