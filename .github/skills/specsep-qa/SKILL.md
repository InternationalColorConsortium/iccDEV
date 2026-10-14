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
   For tile coverage, also build `iccApplyProfiles` and `iccTiffTilesTest`.
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

Run the wrapper, then build the regression helpers before a direct filtered
CTest run. Helpers such as `iccTiffTilesTest` are intentionally excluded from
the default product build; `check` builds them automatically, but `ctest` does
not:

```bash
cmake --build Build --target build-test-binaries
ctest --test-dir Build -R '^iccdev\.specsep-' --output-on-failure --no-tests=error
```

For tiled input/output, BigTIFF, IFD inspection/extraction, spectral sample
preservation, and scalar/threaded image parity, run on Unix or Windows:

```bash
ctest --test-dir Build -C Release -R '^iccdev\.tiff-(tiles|tile-budget-cli|directory-listing-cli)' --output-on-failure --no-tests=error
```

The `iccdev.tiff-tile-budget-cli` test checks 16 mixed tiled/stripped channels
for pixel parity and verifies that the shared 256 MiB input tile-band limit
rejects before creating output. The output tile band has its own cap; do not
describe either as a process-wide memory limit.

The `iccdev.tiff-directory-listing-cli` test checks plain and JSON inventory
of 4096 main IFDs and clean rejection of 4097 main IFDs. Compare elapsed
times separately when investigating listing performance; avoid a timing gate.

For multi-configuration generators such as Visual Studio, set `-C` to the
configuration that was built (`Release` above, or `Debug`).

The native helper generates fixtures from libtiff independently of `CTiffImg`;
the CLI runner needs only Python's standard library. Do not claim SVS decoding,
ROI or pyramid rewriting from these generic TIFF tests.

Verify changed text files are ASCII and run `git diff --check`.

## References

- `../../prompts/specsep-qa.prompt.md`
- `../../../docs/ctest.md`
- `../../instructions/testing.instructions.md`
