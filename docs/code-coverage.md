# Code Coverage and Timing Analysis

Use Clang source-based coverage to answer which source paths executed. Keep it
separate from sanitizer reproduction and throughput benchmarking: coverage,
sanitizer, optimization, and profiling flags all change runtime behavior.

## Deterministic Coverage Contract

Each raw profile filename must retain LLVM's binary signature and use a bounded
pool so repeated short-lived tools do not create thousands of files:

```bash
export LLVM_PROFILE_FILE="$PWD/out/coverage-profraw/run-%4m.profraw"
```

Generate the report with the repository helper. It groups pooled files by base
signature, merges each group exactly once through an input file, and matches the
result to its owning executable or shared library before calling `llvm-cov`:

```bash
LLVM_COV=llvm-cov-22 LLVM_PROFDATA=llvm-profdata-22 \
  Build/Cmake/Testing/iccdev-matched-coverage-report.sh \
    "$PWD/out/coverage-build" \
    "$PWD/out/coverage-profraw" \
    "$PWD/out/coverage-report"
```

Do not pass an unbounded profile list through a shell glob or through `xargs`
to `llvm-profdata merge -o fixed-name.profdata`. A glob can exceed the process
argument limit. GNU `xargs` can split the list into multiple merge processes,
and every process then overwrites the same output. Use `--input-files=<file>`
or a deliberately staged merge with unique intermediate outputs.

The report's TOTAL row is a module-matched aggregate. Headers instantiated in
more than one module can contribute to more than one module total, so establish
a new baseline after changing report topology. Require zero mismatched function
records, every profile signature matched, nonempty HTML, and nonempty LCOV.

## CI Signal

`ci-code-coverage.yml` records environment metadata and millisecond phase
timings in the developer artifact. Required profile-generation scripts,
coverage instrumentation, and the registered hybrid CTest are gates. Expected
capability exits from `iccFromXml` and `iccRoundTrip` remain separately counted
review data. The artifact is uploaded with `always()` so a failed run preserves
partial logs and timing evidence.

Compare coverage only between runs using the same compiler, source revision,
test corpus, module-matching method, and build configuration. A material
coverage change with identical inputs is a correctness signal, not timing
jitter.

## Local Configuration Timing

The timing driver builds isolated trees and runs the focused hybrid CTest in
interleaved rounds:

```bash
.github/scripts/iccdev-build-config-timing.py \
  --output-dir out/build-config-timing \
  --jobs 4 \
  --test-iterations 10
```

Defaults cover Debug, Release, RelWithDebInfo, MinSizeRel, coverage, and ASan.
Add `sanitizers` explicitly only when its additional instrumentation is needed.
The output includes raw JSONL observations, logs, environment metadata, CSV,
and Markdown summaries with mean, median, sample standard deviation,
coefficient of variation, minimum, maximum, spread, and p95.

Configure and build currently have one observation per invocation; do not infer
their variance from that row. Three test rounds are a smoke measurement. Use at
least ten interleaved test rounds for a useful local variance comparison, keep
the machine otherwise idle, fix the build job count, and retain the raw record
when reporting an outlier.

## Interpretation

- Prefer median and p95 for runner-duration planning; the mean is sensitive to
  isolated host contention.
- Use coefficient of variation to compare relative jitter between build
  configurations with different absolute runtimes.
- Treat load, available-memory, compiler, kernel, and job-count changes as
  possible confounders.
- Do not compare sanitizer or coverage timing to Release throughput as a
  performance regression test.
- Reproduce an apparent CI timing regression locally before changing timeouts
  or parallelism.

Workflow changes must pass the fixed-output batching checker in
`.github/scripts/preflight-safety-checks.sh`. The policy intentionally permits
streaming checksum and lint commands whose child outputs are accumulated by one
outer redirection.
