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

Use `all` for the complete 13-target registry. A finding or timeout fails the
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

`connect-thread`, `applyprofiles-row`, `applysearch-row`, and `benchapply` are
the preferred concurrency lanes. Analyzer disagreement is evidence to retain,
not a reason to suppress one result.

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
.github/ci/valgrind/build.sh --target dump
.github/ci/valgrind/self-test.sh
```

Inspect Massif data with `ms_print` and Callgrind data with
`callgrind_annotate`. Both utilities are included in the unified image.
