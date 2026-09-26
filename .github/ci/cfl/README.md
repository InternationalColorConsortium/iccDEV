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
  applies one bounded profile-derived pixel. It always covers both
  interpolation modes using the profile's declared rendering intent; when the
  input contains bytes beyond its declared ICC size, the first three trailing
  bytes independently select direction, intent, and interpolation
- `xmlparse`: parses an XML buffer with network and file includes disabled,
  then drives `CIccProfileXml::ParseXml()` and validation
- `jsonparse`: parses a JSON buffer and drives
  `CIccProfileJson::ParseJson()` and validation
- `connectconfig`: drives the public IccConnect `fromJson()`/`toJson()`
  round-trip paths for top-level and nested configuration objects without
  launching a tool or opening a configured path
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
dictionary and options file. `connectconfig` has a schema-shaped seed and a
dedicated configuration dictionary rather than relying on profile-JSON tokens.
The address-sanitizer prune job uses `!cancelled()` after configuration so one
real finding does not indefinitely starve corpus maintenance while cancellation
still stops cleanup. An optional manual input builds all targets with the coverage
sanitizer and uploads the ClusterFuzzLite coverage report artifact.
The official GitHub artifact backend emits one identically named corpus per
sanitizer. After pruning, only a separate cleanup job receives `actions: write`;
it preserves the newest same-run corpus for each of the eight exact target
names and deletes only the superseded duplicates from that run.

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
use 2 minutes per group. Corpus pruning has a 2-minute minimum per sequential
target, for a 16-minute total budget across all eight targets.

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
stack in `.github/ci/fuzz-patches/cfl` by default. Each open libFuzzer MSan
issue has an individual patch, including a duplicate atomic patch for the
shared #2699/#2703 root cause. Every patch is attempted independently: stale
or drifted patches warn and are skipped while later patches and the build
continue. This lets a normal source fix land without stopping unrelated fuzz
lanes. A manual
`ci-clusterfuzzlite` dispatch can select `unpatched` to reconfirm the original
signals. Local OSS-Fuzz builds can do the same by changing
`.clusterfuzzlite/known-bug-patch-mode` to `unpatched`, or by exporting
`ICCDEV_CFL_KNOWN_BUG_PATCH_MODE=unpatched` when invoking the adapter directly.
Remove only the corresponding issue patch after its source fix lands on
`master`. The strict patch dry-run remains a local maintenance gate, while the
default workflow path is deliberately non-blocking. See
`.github/ci/fuzz-patches/cfl/README.md` for the issue inventory and retirement
rules.

Every successful adapter build writes its source SHA, target group, and patch
mode to `build-out/iccdev-cfl-build-provenance.txt`. The workflow forwards the
checked-out SHA through `CFL_EXTRA_ICCDEV_CFL_SOURCE_SHA` because the source
snapshot inside the build container has no Git metadata. It verifies the
record against `GITHUB_SHA` before fuzzing, pruning, or coverage. This records
the exact revision requested from the official builder without pretending the
containerized source snapshot can independently recover Git provenance.

Successful MSan builds print the resolved libc++, libc++abi, and, for the XML
target, libxml2 paths before reporting the result of each pinned #2687 replay.
The XML harness installs process-local libxml error callbacks so routine
malformed-input diagnostics do not bury sanitizer output; sanitizer reports and
iccDEV validation behavior remain unchanged.

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

## Expansion Order

Improve an existing target before adding another target that reaches the same
library surface. The next additions, in priority order, are:

1. A separately attributable multi-profile CMM chain target covering hints,
   NamedColor transforms, search weights, and boundary pixels.
2. Separate XML and JSON serialization/round-trip targets so parser and writer
   failures remain distinguishable.
3. An in-process V5 display-observer conversion target using a fixed observer
   profile and a fuzzed display profile.
4. TIFF apply-row and TIFF/PNG/JPEG carrier targets only after every external
   dependency has an instrumented MSan boundary.

Do not port local research harnesses wholesale. Every official target needs a
public in-process seam, a useful structured seed, sanitizer-compatible runtime
dependencies, and a distinct attribution boundary.
