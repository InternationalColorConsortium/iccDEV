# iccApplyProfiles Processing QA

`iccApplyProfiles` image QA records more than a successful tool exit. Each
matrix row identifies its source TIFF, destination profile, transform options,
command, output, log, expected result, actual result, and elapsed time.

## Output contract

A positive conversion is a pass only when its destination TIFF is nonempty and
structurally valid. The validator must verify that it has one page; preserves
the source geometry, orientation, and intended resolution; uses the requested
sample encoding, compression, and planar layout; has the channel count required
by the destination profile; and carries the requested embedding state.

Use both `iccTiffDump` and a machine-readable TIFF parser. File existence,
`file`, and a zero exit status alone do not establish a valid output.

The alpha contract is explicit. Release-quality ordinary conversions preserve
alpha. A workflow that deliberately drops alpha must name that lossy mode in
its result and must not report it as an ordinary conversion pass.

## Matrix outcome vocabulary

| Outcome | Required evidence |
|---|---|
| `PASS` | Successful command, valid nonempty TIFF, and satisfied output contract |
| `EXPECTED_REJECTION` | Declared absent precondition, expected failure, no output TIFF |
| `PROFILE_QUALITY_ISSUE` | Transform observed, but PAWG policy quarantines the profile |
| `OUTPUT_CONTRACT_FAILURE` | Tool exit 0 but TIFF or declared output metadata is invalid |
| `TOOL_FAILURE` | Unexpected ordinary nonzero exit |
| `TIMEOUT` | Per-case timeout |
| `SANITIZER_FINDING` | ASan or UBSan diagnostic |
| `MEMORY_TOOL_FINDING` | Memcheck or Helgrind diagnostic |
| `CRASH` | Signal termination |

The outer runner classifies timeout, sanitizer, memory-tool, and signal
outcomes. A tool receipt never claims that an interrupted process crashed.

## Headless evidence

Use the opt-in receipt interface to retain machine-readable evidence without
contaminating image progress or sanitizer stderr:

```sh
iccApplyProfiles --telemetry=jsonl --telemetry-file run.jsonl \
  --evidence-json receipt.json --quiet -cfg conversion.json
```

The JSONL sidecar records validated lifecycle events. The evidence JSON is
written only after the destination closes and records its SHA-256 digest. A
sidecar path is a new file and cannot alias any source, destination, profile,
PCC, or configuration file.

Human lifecycle and progress receipts use UTC timestamps. Progress reports
completed and total rows/pixels, percentage, elapsed time, measured throughput,
estimated remaining and total time, execution mode, and requested/effective
thread counts. Estimates are withheld until both elapsed time and completed
work are nonzero; use `--telemetry-interval-ms` to tune the rate without changing
the final update. JSONL progress uses the same counters and estimate rules.
Progress is emitted after a completed batch, so the interval is a minimum
spacing rather than a promise of updates while one batch is running. Inverse
search defaults to at most 16 rows per batch. Use `--debug` in a build configured
with `ICCDEV_ENABLE_APPLY_BATCH_TIMING=ON` to see batch start and completion
timings when diagnosing a slow call.

## Profile policy

PAWG hard-pass profiles are eligible for tagged positive output and
colorimetric baselines. Known warnings require an explicit manifest entry with
the profile digest, owner, purpose, and expected checklist warning IDs. An
unexpected warning or gap blocks the smoke matrix as `PROFILE_QUALITY_ISSUE`.
Hard-fail profiles remain useful quarantine inputs, but cannot satisfy a
conformance-positive result or produce distributable tagged output. A missing
or malformed embedded source profile belongs only in an expected-negative lane.

The compatibility matrix may compare equivalent profile pairs, but it must not
collapse them before recording the evidence that proves their equivalence.

## Validation layers

Run a small trusted and quarantined profile subset in pull-request smoke QA,
including a declared missing-embedded-profile rejection and tagged plus
untagged output modes. Keep the existing bounded sanitizer runner as a hard
gate. Run the broad external-profile matrix, PAWG inventory, semantic TIFF
validation, and profile-aware round-trip metrics in scheduled or manually
dispatched QA. Memcheck and Helgrind always use a separate non-sanitized Debug
build.

Raw RGB byte equality is not a colorimetric assertion for an RGB-to-CMYK or
seven-channel conversion. For those cases, convert back through the declared
profile chain and record DeltaE2000 and channel-error baselines for the
fixture, profile, intent, and encoding.

The checked-in smoke manifest is
<a href="../../.github/ci/quality-assurance/manifests/iccApplyProfiles-smoke-matrix.json">iccApplyProfiles-smoke-matrix.json</a>.
Run it through `iccApplyProfiles-matrix.sh`; each invocation writes
`results.jsonl` plus `summary.json` and preserves row-specific logs, telemetry,
receipts, profile assessments, TIFF dumps, and output artifacts under the
selected evidence directory. Artifact references may consume an earlier row's
output, which lets the expected-negative lane prove missing embedded-profile
handling without adding a second binary TIFF fixture.

## See also

- <a href="../../Tools/CmdLine/IccApplyProfiles/Readme.md">iccApplyProfiles command reference</a>
- <a href="../../.github/ci/quality-assurance/README.md">Apply tool QA drivers</a>
- [CLI tool reference](../tools-cli-reference.md)
