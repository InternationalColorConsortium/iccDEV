# ICC Apply Tool Quality Assurance

This directory contains local quality-assurance drivers and generated command
corpora for the main `iccApply*` command-line tools:

- `iccApplyNamedCmm`
- `iccApplyProfiles`
- `iccApplySearch`
- `iccApplyToLink`
- `iccBenchApply`

The scripts are intended for maintainer QA on a local checkout. They exercise
documented command-line argument shapes, optional config export/replay paths,
environment-variable arguments, profile connection condition arguments, and
sanitizer-visible failures.

## Prerequisites

Build the command-line tools and generate the hybrid test profiles first:

```sh
cd Build
cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build . --parallel
cd ../Testing/hybrid
./BuildAndTest.sh
```

Run the suite and mutation drivers below from `Testing/hybrid`. The scripts
use build-tree tool paths when they are available.

## Quick smoke

```sh
../../.github/ci/quality-assurance/scripts/icc_apply_qa_suite.sh --mutations 12
```

The suite runs both focused argument contracts and mutation smoke cases across
all four apply tools, then scans logs for sanitizer signatures. The focused
contracts include V5 BRDF and spectral NamedCmm transforms, deep and row
ApplyProfiles options, fast/no-init/weighted ApplySearch paths, and V5/CUBE
ApplyToLink output. It exits nonzero if any tool command fails or sanitizer
output is detected.

## Focused quick checks

Run these checks from the repository root after building the tools. Set
`ICCDEV_BUILD_DIR` when the build directory is not `Build/`; set
`ICCDEV_ROOT` when invoking a copied script outside the checkout. Set
`ICCDEV_TOOLS_DIR` to override tool discovery.

```sh
.github/ci/quality-assurance/scripts/iccApplyProfiles-quick-check.sh
.github/ci/quality-assurance/scripts/iccApplyNamedCmm-quick-check.sh
.github/ci/quality-assurance/scripts/iccApplySearch-quick-check.sh
.github/ci/quality-assurance/scripts/iccApplyToLink-quick-check.sh
.github/ci/quality-assurance/scripts/iccBenchApply-quick-check.sh
```

The focused checks use checked-in fixtures and validate representative success,
configuration export/replay, and argument-rejection paths. They write logs and
temporary outputs under a fresh `QA_OUTDIR` unless that environment variable is
set. Every `qa_run` also writes a sibling `.cmd` file with the exact tool argc
and shell-escaped argv. Set `QA_TIMEOUT_SECONDS` to override the 30-second
per-command limit.

The ApplyProfiles quick check also verifies the headless receipt contract. It
requires JSONL telemetry with monotonic event sequence numbers, one terminal
`run_completed` event, a final JSON evidence receipt with the output digest,
rejection of a pre-existing sidecar path, and byte-identical TIFF output with
telemetry enabled. Tool-owned JSONL and evidence remain separate from the raw
stdout/stderr logs so sanitizer scanners keep their existing authority.

The tracked `manifests/iccApplyProfiles-smoke-matrix.json` adds conversion-level
evidence. Its wrapper records exact argv, revision and file hashes, structured
outcome classifications, semantic TIFF metadata, `iccTiffDump` output, PAWG
JSON for each available source and destination profile, and measurement-only
channel error. Run it directly with:

```sh
ICCDEV_BUILD_DIR=/path/to/build \
  .github/ci/quality-assurance/scripts/iccApplyProfiles-matrix.sh \
  --out-dir /tmp/iccapply-evidence
python3 .github/ci/quality-assurance/scripts/test_icc_apply_profiles_matrix.py
```

The first policy phase is `framework_only`: command failures, timeouts,
sanitizer findings, crashes, TIFF contract failures, and unexpected PAWG findings
block. The smoke profile's known checklist warnings are pinned by profile digest,
owner, purpose, and warning IDs in the manifest. These RGB conversions use the
embedded source profile and a destination profile; the TIFF contract checks RGB
photometric interpretation, alpha absence, embedding, and exact decoded pixel
identity for this same-profile fixture. The channel-error measurement is
code-value drift, not a colorimetric
conformance result. Profile-aware color thresholds remain measurement-only until
profile owners approve them.

This matrix uses `iccPawgReport --json` for checklist verdicts and IDs.
`ICCDEV_ENABLE_QA_FLAGS=ON` exposes a separate load/validation receipt through
`--qa-flags --evidence-json`; that receipt does not contain the checklist IDs
needed by this manifest, so it is not required for the smoke matrix.

## Per-tool drivers

```sh
../../.github/ci/quality-assurance/scripts/iccApplyNamedCmm_ci_path_exercise.sh --mutations 200
../../.github/ci/quality-assurance/scripts/iccApplyProfiles_ci_path_exercise.sh --mutations 200 --no-replay-cfg
../../.github/ci/quality-assurance/scripts/icc_ci_tool_path_exercise.sh --tool search --mutations 200 --no-replay-cfg
../../.github/ci/quality-assurance/scripts/tolink-script-random-001.sh --tests 200
```

Use `--generate FILE` to regenerate the command corpora under `commands/`.
Use `--count` to show each generator's mutation space without requiring a built
test tree.

## Command corpora

The files under `commands/` are generated replay corpora. They intentionally
contain raw commands only so they can be executed with standard shell tooling:

```sh
while IFS= read -r cmd; do
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 bash -lc "$cmd"
done < ../../.github/ci/quality-assurance/commands/iccApplyToLink_QA_200.txt
```

Generated corpora are ASCII text and should remain line-ending clean for Linux
and Windows checkout compatibility.
