---
name: specsep-qa
description: >
  Build, run, or diagnose the repository-owned iccSpecSepToTiff QA suites and
  their checked-in spectral TIFF fixtures.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# iccSpecSepToTiff QA

Use this skill for `iccSpecSepToTiff` CLI contract, TIFF geometry, profile
alignment, output cleanup, or fixture work.

## Workflow

1. Read `../../../Tools/CmdLine/IccSpecSepToTiff/Readme.md` for the command
   contract and limits.
2. Build `iccSpecSepToTiff`, `iccTiffDump`, and `iccFromXml` in the same tree.
3. Run the repository wrapper from any directory:

```bash
ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools \
  /path/to/iccDEV/.github/scripts/iccdev-specsep-qa.sh
```

4. For a focused failure, rerun the named child script shown by the wrapper.
5. Keep durable fixtures under `.github/ci/test-data/`; keep generated TIFFs
   and logs under `ICCDEV_TEST_OUTDIR`.

For a bounded option/output campaign with exact pixel and TIFF-tag validation,
run:

```bash
ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools \
  /path/to/iccDEV/.github/scripts/iccdev-specsep-campaign.sh --seconds 300
```

The numbered-fixture child suite runs every checked-in valid TIFF as a
single-channel input with all four compression and planar combinations.

The profile sweep runs twice, because a profile is accepted only when its sample
count matches the generated TIFF: once at the default eight channels across
`Testing/CalcTest`, which can only produce rejections, and once at
`--channels 3` across `Testing/`, whose single depth-1 profile is the 3-channel
sRGB v4 preference. A run that accepts nothing says so, so a rejection-only
corpus cannot look like full coverage. For a local registry download, rerun with
`--profile-dir /path/to/profiles` and a `--channels` value the corpus can match;
accepted profiles must be embedded, while profile-specific clean rejections are
expected.

The extended campaign requires Python `numpy`, `tifffile`, and `imagecodecs` --
tifffile defers LZW decoding to imagecodecs and half the matrix writes LZW. It
preserves a
machine-readable text summary, representative `iccTiffDump`/`tiffinfo`
inspections, generated inputs, and output TIFFs under `ICCDEV_TEST_OUTDIR`.
It exits nonzero for either a functional failure or a specification-audit
finding; review both categories in `matrix-summary.txt`.

## Fixture Rules

- `spectral/spec_1` through `spec_10` are the tiny normal conversion sequence.
- `specsep-harvest/gray300/spec_1` through `spec_8` cover larger grayscale
  input.
- `specsep-truncated/spec_1` is the late-read-failure cleanup fixture.
- Do not replace these with local scratch paths or generated build artifacts.

## Validation

Run the wrapper, then the six registered CTests when CTest integration is in
scope:

```bash
ctest --test-dir Build -R '^iccdev\.specsep-' --output-on-failure --no-tests=error
```

Verify changed text files are ASCII and run `git diff --check`.

## References

- `../../prompts/specsep-qa.prompt.md`
- `../../../docs/ctest.md`
- `../../instructions/testing.instructions.md`
