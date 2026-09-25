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
- `profileparse`: validates an ICC buffer, forces tag loading, and calls every
  loaded tag's public `Describe()` path
- `cmmapply`: builds a `CIccCmm` directly from a memory-backed profile and
  applies one bounded profile-derived pixel
- `xmlparse`: parses an XML buffer with network and file includes disabled,
  then drives `CIccProfileXml::ParseXml()` and validation
- `jsonparse`: parses a JSON buffer and drives
  `CIccProfileJson::ParseJson()` and validation
- `connectconfig`: drives the public IccConnect `fromJson()` configuration
  objects without launching a tool or opening a configured path
- `pawgreport`: calls the purpose-built `AssessPawgFromMemory()` and
  `PawgCompressionVerdict()` assessment seams
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
builds eight in-process targets in three independently budgeted groups:

- `core`: `profileparse`, `cmmapply`, `profilevisualize`, and
  `writerserialize`
- `formats`: `xmlparse`, `jsonparse`, and `connectconfig`
- `assessment`: `pawgreport`

The six CLI-fidelity wrappers remain local smoke targets because coverage from
their child processes is not visible to the parent libFuzzer process.

The dedicated `ci-clusterfuzzlite` workflow runs on manual dispatch and pushes
to `ci-qa-clusterfuzz`. Its matrix builds and fuzzes with `address`,
`undefined`, and `memory`; libFuzzer is the engine for every matrix entry, not
a fourth sanitizer. The nine group/sanitizer combinations run at most three at
a time to stay within hosted-runner and artifact API limits. ICC, XML, and JSON
targets receive only the matching
tracked seed family from `.github/ci/test-data/`, plus a format-specific
dictionary and options file. The address-sanitizer prune job uses `always()`
after configuration so one real finding does not indefinitely starve corpus
maintenance. An optional manual input builds all targets with the coverage
sanitizer and uploads the ClusterFuzzLite coverage report artifact.

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

Repeat the last three commands with every emitted target and with `undefined`
and `memory`. The memory adapter builds the repository-pinned MSan libc++ and
libc++abi. The `formats` group additionally builds pinned, instrumented
libxml2; the adapter rejects `icc_xmlparse_fuzzer` if `ldd` resolves a system
copy. Every MSan target is rejected if it resolves libstdc++ or cannot resolve
the bundled libc++ runtime, and the exact #2687 input is replayed before the
targets are accepted. This prevents uninstrumented standard-library or XML
dependency writes from leaving poisoned shadow state and being misattributed
to iccDEV.
See `docs/issue-2687-msan-runtime.md` for the report and producer-consumer
contract.

Manual workflow dispatch accepts a whole-number `fuzz_minutes` input from 2
through 45 as the budget for each target group and sanitizer pair. The runner
divides that group budget across only the sequential targets in that group, so
adding a format target does not dilute core-profile fuzzing. Branch-push runs
use 2 minutes per group. Corpus pruning remains fixed at 2 minutes.

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

Build and run all in-process harnesses with:

```bash
.github/ci/cfl/build.sh \
  --targets profileparse,cmmapply,profilevisualize,writerserialize,xmlparse,jsonparse,connectconfig,pawgreport \
  --seconds 30
```

The official adapter reads `.clusterfuzzlite/target-group`; set it to `core`,
`formats`, `assessment`, or `all`. `ICCDEV_CFL_TARGET_GROUP` provides the same
override for direct local adapter invocations. The GitHub workflow forwards its
matrix selection as `CFL_EXTRA_ICCDEV_CFL_TARGET_GROUP`, because the official
ClusterFuzzLite action builds a fresh clone of `GITHUB_SHA` and therefore does
not see files rewritten in the Actions checkout before the container step.

The official ClusterFuzzLite adapter applies the temporary source-only patch
stack in `.github/ci/fuzz-patches/cfl` by default. The current patch carries
the fixes under review for open MSan issues #2686 and #2688 so those known
findings do not stop exploration of the remaining surface. A manual
`ci-clusterfuzzlite` dispatch can select `unpatched` to reconfirm the original
signals. Local OSS-Fuzz builds can do the same by changing
`.clusterfuzzlite/known-bug-patch-mode` to `unpatched`, or by exporting
`ICCDEV_CFL_KNOWN_BUG_PATCH_MODE=unpatched` when invoking the adapter directly.
Remove the corresponding patch as soon as each source fix lands on `master`;
the strict patch dry-run in the configuration test makes stale patches fail
visibly.

A CLUT-bearing seed has to survive into the corpus for either visualization
target to render a raster, and two independent gates used to remove the only
such seed (#2120):
`max_seed_bytes` deleted it from the corpus, and libFuzzer's `max_len`
truncated whatever survived. Both defaults are now above it, and pruning a seed
committed under `.github/ci/test-data` is a hard error rather than a silent
`rm`, so a future oversized regression seed forces a decision about the cap
instead of quietly costing coverage.

The shared `icc_profile_fuzzer.options` and `icc_text_fuzzer.options` templates
are copied to target-specific names in the build output. They are not read by
`build.sh` -- libFuzzer binaries do not consume them; they are the
ClusterFuzz/OSS-Fuzz runner convention. `build.sh`
passes `-max_len`, `-timeout`, `-rss_limit_mb` and `-use_value_profile`
explicitly, so changing a value means changing it in both places.

Do not commit generated `.github/ci/cfl/bin`, `.cfl-smoke`, build trees, crash artifacts,
coverage output, or profiler data.
