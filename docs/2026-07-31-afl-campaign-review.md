# AFL Campaign Review - 2026-07-31

## Scope

Reviewed the stopped AFL++ campaign under `afl/` for coverage, status,
corpus state, crash/hang triage, target configuration, and next-run fuzzing
options. Fuzzers were confirmed stopped with:

```sh
ps -C afl-fuzz -o pid=,pcpu=,pmem=,etime=,args=
```

No active `afl-fuzz` processes were reported.

## Commands Run

```sh
./afl/start.sh --list
./afl/triage.sh applyprofiles-hybrid-embedded --jobs 2
./afl/triage.sh fromxml-noid --jobs 2
AFL_REPORT_ROOT=/tmp/afl-report-20260731-3765969 ./afl/report.sh all --stats-only --target-jobs 2 --jobs 2
jq ... /tmp/afl-report-20260731-3765969/status.json
```

Generated management artifacts:

- `/tmp/afl-report-20260731-3765969/index.md`
- `/tmp/afl-report-20260731-3765969/status.json`
- `/tmp/afl-report-20260731-3765969/targets.tsv`

The stats-only report completed successfully in 1m21s after improving
`afl/status.sh` process discovery.

## High-Level Status

- Configured AFL targets: 52
- Status rows in generated JSON: 693, including historical parallel instances
- Running AFL processes: 0
- Aggregate execs across all status rows: 949,598,207
- Aggregate corpus entries across all status rows: 542,175
- Rows with crash files: 22
- Rows with hang files: 109

The current `default`/`main` rows are enough for campaign steering, but the
status JSON includes many historical parallel instances. Use the generated TSV
for full audit trails and the current-row summaries below for decisions.

## Triage Findings

### applyprofiles-hybrid-embedded

Current crash files: 2 in `afl/afl-applyprofiles-hybrid-embedded/output/default/crashes`.

Triage replayed 6 crash artifacts across default and synced lanes:

- 3 actionable sanitizer-backed crashes.
- 3 graceful tool failures.
- Actionable signature: AddressSanitizer heap-buffer-overflow.
- Hangs replayed as non-actionable: 1 clean, 1 graceful failure.

This target should be first for testcase minimization and bug filing.

### fromxml-noid

Current crash files: 1 in `afl/afl-fromxml-noid/output/default/crashes`.

Triage replayed 1 crash:

- 1 actionable timeout-with-sanitizer finding.
- Sanitizer detail: implicit conversion at
  `afl/iccDEV/IccXML/IccLibXML/IccMpeXml.cpp:313`.

Hang triage replayed 89 artifacts:

- 0 actionable hangs.
- 48 clean.
- 41 graceful tool failures.

Treat the crash as actionable and de-emphasize this lane's hang queue until new
evidence appears.

## Coverage And Queue Graphs

Highest current coverage by default/main row:

```text
jpegdump                23.62% | ########################
applytolink             16.69% | #################
pawgreport-read         16.59% | #################
pawgreport              16.51% | #################
pawgreport-fast         15.84% | ################
applyprofiles-deep      13.13% | #############
applyprofiles-fast      12.88% | #############
roundtrip-mpe           12.65% | #############
applytolink-cube        12.11% | ############
dump-read               11.84% | ############
```

Largest pending queues needing continuation or corpus minimization:

```text
applyprofiles-deep      4031 | ########################################
applyprofiles-fast      3960 | #######################################
pawgreport-read         2437 | ########################
specseptotiff-compress  2403 | ########################
pawgreport              2278 | #######################
v5dspobs                1961 | ####################
specseptotiff-desc      1752 | #################
dump                    1672 | ################
pawgreport-read-main    1664 | ################
pawgreport-fast-main    1574 | ################
```

Most noisy current finding queues:

```text
applynamedcmm           crashes=14 hangs=512
fromxml                 crashes=5  hangs=357
applyprofiles-hybrid    crashes=6  hangs=2
fromxml-noid            crashes=1  hangs=89
applysearch             crashes=0  hangs=309
fromjson                crashes=0  hangs=426
fromcube                crashes=0  hangs=303
```

## Target Assessment

Strong lanes to keep running:

- `applyprofiles-deep` and `applyprofiles-fast`: best ICC-profile growth lanes,
  high coverage, high pending work, stable enough to continue.
- `pawgreport`, `pawgreport-read`, `pawgreport-fast`: broad dump/report
  coverage and substantial pending queue.
- `applytolink` and `applytolink-cube`: high coverage and low pending, useful
  for regression sweeps and periodic corpus sync.
- `dump`, `dump-read`, `roundtrip`, `roundtrip-mpe`, `tojson`, `toxml`,
  `v5dspobs`: mature long-run lanes; prioritize minimization and differential
  replay over blind queue growth.

Lanes needing adjustment before long runs:

- `specseptotiff-tiff`: 0.06% coverage and 7.99% stability. Rework seed validity
  and wrapper determinism before spending more CPU.
- `pngdump` and `pngdump-inject`: less than 0.5% coverage. Add richer PNG seed
  classes or keep as short smoke lanes.
- `applysearch-hybrid-pcc` and weight lanes: low exec/sec or low density. Use
  small screened seeds and shorter bounded runs until new coverage improves.
- `fromxml` and `fromxml-noid`: keep CmpLog/dictionary support, but prune hang
  queues after crash minimization because many hangs replay clean or as normal
  parse failures.

## Corpus Management

Recommended next corpus actions:

1. Minimize actionable crash inputs first, preserving original AFL filenames in
   metadata or issue text.
2. Run per-target queue minimization for high-pending lanes before resuming
   them: `applyprofiles-deep`, `applyprofiles-fast`, `pawgreport-read`,
   `specseptotiff-compress`, `pawgreport`, `dump`, `applysearch`, and
   `profilevisualize`.
3. Keep generated AFL outputs, `fastresume.bin`, `.profraw`, coverage HTML,
   logs, screenshots, and local temp trees out of git.
4. Promote only minimized, durable reproducer inputs into checked-in corpora
   when they exercise a documented regression or new parser feature.
5. Re-seed XML/JSON/config lanes from current upstream regression artifacts
   after minimization, not before, to avoid inflating queue counts.

## Next Fuzzing Options

Use these as the next campaign defaults unless a target-specific experiment
needs different settings:

```sh
./afl/start.sh TARGET --parallel N --cmplog-auto --power-schedule rare --mopt-secs 300
```

Guidance:

- Use `--power-schedule rare` for mature coverage lanes with low recent finds.
- Use `--power-schedule explore` for short exploratory seed-screening runs.
- Keep `--mopt-secs 300` for long campaigns after smoke validation.
- Keep `--cmplog-auto` enabled for XML, JSON, config, CUBE, and ICC binary
  parser lanes where compare-heavy parsing dominates.
- Use `--seed-only --reseed --seed-order sorted` to stage deterministic corpus
  updates before long runs.
- Avoid spending long cycles on `specseptotiff-tiff` until stability improves.

## Script Management Note

`afl/status.sh` was updated to cache live AFL process discovery and use
`pgrep -af afl-fuzz` when available. This avoids repeated full `/proc` scans
when reporting stopped campaigns with many target instances.
