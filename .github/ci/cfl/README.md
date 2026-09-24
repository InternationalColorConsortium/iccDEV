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
  through `Mini{PDF,SVG,TIFF}` -- the IFD layout and offset arithmetic, the PDF
  object graph and xref table, and `SVGOut` (#2116)

Run the local smoke with:

```bash
.github/ci/cfl/build.sh --seconds 30
```

## ClusterFuzzLite

The official ClusterFuzzLite integration lives in `.clusterfuzzlite/` and
builds the two in-process targets, `icc_profilevisualize_fuzzer` and
`icc_writerserialize_fuzzer`. These targets provide useful coverage feedback
inside one libFuzzer process; the six CLI-fidelity wrappers remain local smoke
targets because coverage from their child processes is not visible to the
parent libFuzzer process.

The dedicated `ci-clusterfuzzlite` workflow runs on manual dispatch and pushes
to `ci-qa-clusterfuzz`. Its matrix builds and fuzzes with `address`,
`undefined`, and `memory`; libFuzzer is the engine for every matrix entry, not
a fourth sanitizer. Each build packages the tracked ICC files from
`.github/ci/test-data/` as the seed corpus for both in-process targets.
After the sanitizer matrix succeeds, one bounded address-sanitizer job prunes
the persistent corpus so redundant inputs do not accumulate across batch runs.

Local validation uses an OSS-Fuzz checkout:

```bash
python3 infra/helper.py build_image --external --pull /path/to/iccDEV
python3 infra/helper.py build_fuzzers --external --clean \
  --engine libfuzzer --sanitizer address /path/to/iccDEV
python3 infra/helper.py check_build --external \
  --engine libfuzzer --sanitizer address /path/to/iccDEV
python3 infra/helper.py run_fuzzer --external \
  --engine libfuzzer --sanitizer address \
  /path/to/iccDEV icc_profilevisualize_fuzzer -- -max_total_time=30
```

Repeat the last three commands with `undefined` and `memory`. The
ClusterFuzzLite build disables XML, JSON, tools, and zlib so the MSan binary
does not mix the in-process target with uninstrumented system libraries. The
memory adapter also builds the repository-pinned MSan libc++ and libc++abi,
rejects either fuzzer if `ldd` finds libstdc++ or cannot resolve that runtime,
and replays the exact #2687 input before accepting the targets. This prevents
uninstrumented standard-library writes from leaving poisoned shadow state and
being misattributed to `CIccProfile`. The lane covers the public `IccVizModel`
and writer APIs; the existing local CFL smoke retains the broader tool and
compressed-tag coverage. See `docs/issue-2687-msan-runtime.md` for the report
and producer-consumer contract.

Manual workflow dispatch accepts a whole-number `fuzz_minutes` input from 2
through 45 as the total fuzzing budget in every sanitizer matrix entry. The
ClusterFuzzLite runner divides that total budget across the sequential targets.
Branch-push runs use a 2-minute total budget. Corpus pruning remains fixed at 2
minutes.

All CFL modes require a matching Clang C/C++ pair at major version 21 or 22.
The local builder prefers 22, falls back to 21, and rejects older or mismatched
compilers. The pinned ClusterFuzzLite builder currently supplies Clang 22.

Apply the local CFL patch stack before configuring iccDEV with `--patches`.
The
`ci-cfl-smoke` workflow applies `.github/ci/fuzz-patches/cfl` by default so
patch branches test local fixes before they are promoted to source PRs:

```bash
.github/ci/cfl/build.sh --patches --seconds 30
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
.github/ci/cfl/build.sh --targets profilevisualize,writerserialize --seconds 30
```

A CLUT-bearing seed has to survive into the corpus for either one to render a
raster, and two independent gates used to remove the only such seed (#2120):
`max_seed_bytes` deleted it from the corpus, and libFuzzer's `max_len`
truncated whatever survived. Both defaults are now above it, and pruning a seed
committed under `.github/ci/test-data` is a hard error rather than a silent
`rm`, so a future oversized regression seed forces a decision about the cap
instead of quietly costing coverage.

Note the `.options` files are not read by `build.sh` -- libFuzzer binaries do not
consume them; they are the ClusterFuzz/OSS-Fuzz runner convention. `build.sh`
passes `-max_len`, `-timeout`, `-rss_limit_mb` and `-use_value_profile`
explicitly, so changing a value means changing it in both places.

Do not commit generated `.github/ci/cfl/bin`, `.cfl-smoke`, build trees, crash artifacts,
coverage output, or profiler data.
