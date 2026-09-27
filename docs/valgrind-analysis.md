# Valgrind-Family Analysis

iccDEV provides registered Memcheck, Helgrind, DRD, Massif, and Callgrind
lanes for command-line tools and threaded CMM paths. These lanes always use a
separate non-sanitized Debug build. Never wrap the sanitizer-instrumented
binaries shipped in the unified image.

The executable component lives in
<a href="../.github/ci/valgrind/README.md">`.github/ci/valgrind/`</a>.
It records stdout, stderr, analyzer logs, generated files, and `summary.tsv`
under the ignored `out/valgrind-evidence/` directory.

## Native quick start

```bash
.github/ci/valgrind/build.sh
.github/ci/valgrind/self-test.sh
.github/ci/valgrind/run.sh --tool memcheck dump fromxml fromjson
.github/ci/valgrind/run.sh --tool helgrind connect-thread applyprofiles-row benchapply
.github/ci/valgrind/status.sh
```

Use `all` for the complete 14-target registry. A finding or timeout fails the
runner. `--allow-findings` is available only for deliberate evidence
collection where Valgrind reports findings through exit 86 or a parsed error
count. Target failures, loader errors, signals, and timeouts always fail.

The build explicitly disables ASAN, UBSAN, IntegerSanitizer, float sanitizer,
TSan, MSan, LSan, fuzzing, coverage, profiling, and LTO. It also rejects
sanitizer flags preserved in the CMake cache and rejects dynamically or
statically linked sanitizer runtimes before either building or running a lane.

## Unified image

The Docker image installs the same component on `PATH`:

```bash
iccdev-valgrind-build --target dump --target connect-thread
iccdev-valgrind-self-test --sanitized-binary "$ICCDEV_TOOLS_DIR/IccDumpProfile/iccDumpProfile"
iccdev-valgrind-run --tool memcheck dump
iccdev-valgrind-run --tool helgrind connect-thread
iccdev-valgrind-status
```

The image uses `/workspace/valgrind/build` and
`/workspace/valgrind/output`, keeping this build isolated from the configured
sanitizer tree at `/workspace/build`. Override
`ICCDEV_VALGRIND_SOURCE_DIR`, `ICCDEV_VALGRIND_BUILD_DIR`,
`ICCDEV_VALGRIND_OUTPUT_DIR`, or `ICCDEV_VALGRIND_JOBS` when evidence must live
on a mounted volume.

## Analyzer selection

| Analyzer | Primary use |
| --- | --- |
| Memcheck | Invalid access, use of uninitialized data, and leak analysis. |
| Helgrind | Lock ordering and data-race analysis for concurrent targets. |
| DRD | Independent thread-synchronization analysis; compare with Helgrind. |
| Massif | Heap and stack growth measurement. |
| Callgrind | Instruction and call-graph profiling. |

## ICC corpus sweeps

Build the `pawg` or `dump` lane, then use the corpus runner instead of a shell
word-splitting loop. It safely handles spaces in profile paths, bounds every
invocation, and preserves one evidence directory per profile.

```bash
.github/ci/valgrind/build.sh --target pawg --target dump
.github/ci/valgrind/corpus.sh --tool memcheck --icc-tool pawg --output-dir "$PWD/out/pawg-memcheck" /path/to/corpus
.github/ci/valgrind/corpus.sh --tool memcheck --icc-tool dump --output-dir "$PWD/out/dump-memcheck" /path/to/corpus
```

The accepted `--tool` values are `memcheck`, `helgrind`, `drd`, `massif`, and
`callgrind`. PAWG and dump are single-process report tools, so prefer Memcheck,
Massif, and Callgrind for their corpus sweeps. Use registered threaded targets
such as `applyprofiles-row` when investigating synchronization with Helgrind or
DRD.

## Docker reproduction

The unified image contains the repository script and the same non-sanitized
build helper. This one-liner follows the pull-and-run pattern used by maintainer
issue reproductions while keeping evidence on the host:

```bash
mkdir -p "$HOME/qa/iccdev-valgrind-docker-evidence" && docker pull ghcr.io/internationalcolorconsortium/iccdev:latest && set -o pipefail && docker run --rm --entrypoint bash ghcr.io/internationalcolorconsortium/iccdev:latest -lc 'set +e; { iccdev-valgrind-build --jobs 16 --target dump --target connect-thread --target applyprofiles-row && ICCDEV_VALGRIND_OUTPUT_DIR=/workspace/valgrind/output/memcheck iccdev-valgrind-run --tool memcheck dump && ICCDEV_VALGRIND_OUTPUT_DIR=/workspace/valgrind/output/helgrind iccdev-valgrind-run --tool helgrind connect-thread && ICCDEV_VALGRIND_OUTPUT_DIR=/workspace/valgrind/output/drd iccdev-valgrind-run --tool drd applyprofiles-row && ICCDEV_VALGRIND_OUTPUT_DIR=/workspace/valgrind/output/massif iccdev-valgrind-run --tool massif dump && ICCDEV_VALGRIND_OUTPUT_DIR=/workspace/valgrind/output/callgrind iccdev-valgrind-run --tool callgrind dump; } >&2; rc=$?; tar -C /workspace/valgrind/output -cf - .; exit "$rc"' | tar -C "$HOME/qa/iccdev-valgrind-docker-evidence" -xf -
```

Streaming a tar archive avoids host/container UID mismatches while making the
host user the owner of the extracted evidence. Use a new host directory for
each run because the analyzer runner refuses to overwrite evidence.

Mount an external corpus read-only and call the repository script for a Docker
corpus sweep:

```bash
mkdir -p "$HOME/qa/iccdev-valgrind-docker-corpus" && set -o pipefail && docker run --rm --entrypoint bash -v "/absolute/path/to/corpus:/corpus:ro" ghcr.io/internationalcolorconsortium/iccdev:latest -lc 'set +e; { iccdev-valgrind-build --jobs 16 --target pawg && .github/ci/valgrind/corpus.sh --tool memcheck --icc-tool pawg --output-dir /workspace/valgrind/output/pawg /corpus; } >&2; rc=$?; tar -C /workspace/valgrind/output -cf - .; exit "$rc"' | tar -C "$HOME/qa/iccdev-valgrind-docker-corpus" -xf -
```

`connect-thread`, `applyprofiles-row`, `applysearch-row`, and `benchapply` are
the preferred concurrency lanes. Analyzer disagreement is evidence to retain,
not a reason to suppress one result.

Issue #2592 records DRD's condition-variable notification check for the
threaded row-application pool. To keep this maintained DRD lane clean,
notifications for `m_jobReady` occur while holding the mutex associated with
its wait predicate. Run the focused native regression with:

```bash
ICCDEV_VALGRIND_BUILD_DIR="$PWD/out/issue-2592-valgrind" ICCDEV_VALGRIND_OUTPUT_DIR="$PWD/out/issue-2592-evidence" .github/ci/valgrind/issue-2592-regression.sh
```

The command requires zero DRD and Helgrind errors and byte-identical generated
TIFFs. Choose fresh build and evidence paths when retaining multiple runs.

The historical
<a href="../.github/scripts/iccdev-valgrind-qa.sh">`iccdev-valgrind-qa.sh`</a>
continues to provide the focused before/after expectation contract for the
`GetNewApplyCmm()` race. Use the registered component for broad tool-path
analysis and the historical helper for that exact regression.

## Validation

```bash
bash -n .github/ci/valgrind/*.sh .github/ci/valgrind/fixtures/*
shellcheck .github/ci/valgrind/*.sh .github/ci/valgrind/fixtures/*
.github/ci/valgrind/validate.sh
.github/ci/valgrind/build.sh --target dump --target pawg
.github/ci/valgrind/self-test.sh
.github/ci/valgrind/issue-2592-regression.sh
```

Inspect Massif data with `ms_print` and Callgrind data with
`callgrind_annotate`. Both utilities are included in the unified image.
