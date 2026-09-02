# iccSpecSepToTiff QA

Use this prompt to validate or diagnose `iccSpecSepToTiff` behavior with the
repository-owned fixtures and scripts.

## Inputs

- Branch or worktree:
- Build directory:
- Failure or behavior to prove:
- Sanitizer configuration, if any:

## Procedure

1. Confirm the worktree and tool build match the target commit.
2. Read `Tools/CmdLine/IccSpecSepToTiff/Readme.md` for the current CLI contract.
3. Run `.github/scripts/iccdev-specsep-qa.sh` with `ICCDEV_TOOLS_DIR` set to the
   matching `Build/Tools` directory.
4. Rerun only the failing child suite while diagnosing it.
5. When optional-profile coverage is relevant, run
   `.github/scripts/iccdev-specsep-profile-sweep.sh --profile-dir DIR` against
   the downloaded or external corpus, with `--channels N` set to a sample count
   that corpus can actually match -- a mismatched value makes every profile an
   expected rejection and the sweep passes without embedding anything. Classify
   clean profile incompatibility separately from crashes, timeouts, sanitizer
   findings, partial output, and unexplained exits.
6. For registered-test changes, run
   `ctest --test-dir Build -R '^iccdev\.specsep-' --output-on-failure --no-tests=error`.
7. Report the command, exit status, per-suite totals, sanitizer findings, and
   any missing fixture or tool identified by preflight.

For a timed option/output review, run
`.github/scripts/iccdev-specsep-campaign.sh --seconds N`. Review
`matrix-summary.txt`, the exact generated TIFF paths, and the representative
`iccTiffDump` and `tiffinfo` reports. Distinguish exact pixel equality from
metadata/specification conformance and report any divergence separately.

Keep outputs under `ICCDEV_TEST_OUTDIR`. Do not depend on `~/scripts`, private
scratch paths, or untracked fixtures.
