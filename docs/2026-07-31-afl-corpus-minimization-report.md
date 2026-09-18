# AFL Corpus Minimization Report - 2026-07-31

## Scope

This pass continued the stopped AFL campaign cleanup after the largest pending
queues were identified. It minimized actionable crash inputs first, then ran
per-target queue minimization for the highest-pending lanes, and finally
validated the lanes that need adjustment before any long run is resumed.

Generated AFL outputs remain local and ignored. Do not promote any file from
the cmin directories below unless it is tied to a documented regression or new
parser feature.

## Crash Minimization

Crash filenames were preserved in the AFL-generated minimized corpus outputs.
The original crash manifest for this pass was written locally to
`/tmp/afl-crash-manifest-20260731T145145Z.tsv`.

| Target | Source | Minimized output | Result |
| --- | --- | --- | --- |
| `applyprofiles-hybrid-embedded/default` | `crashes` | `afl/afl-applyprofiles-hybrid-embedded/cmin-crashes-sanitizer-default-20260731T145237Z` | 2 kept from 2 |
| `fromxml-noid/default` | `crashes` | `afl/afl-fromxml-noid/cmin-crashes-sanitizer-default-20260731T145237Z` | 1 kept from 1 |

Notes:

- Normal coverage-mode crash minimization produced README-only output for these
  crash sets, so the sanitizer replay pass was rerun with `afl-cmin.bash -C`.
- Keep these minimized crash inputs local until each is replayed, bucketed, and
  attached to a tracked issue or regression test.

## Queue Minimization

All requested high-pending queues completed per-target `afl-cmin.bash` passes.
Counts below were verified from disk with symlink-aware file counting.

| Target instance | Input queue | Minimized output | Kept |
| --- | ---: | --- | ---: |
| `applyprofiles-deep/main` | 4210 | `afl/afl-applyprofiles-deep/cmin-queue-main-20260731T145237Z` | 1877 |
| `applyprofiles-fast/main` | 4104 | `afl/afl-applyprofiles-fast/cmin-queue-main-20260731T145237Z` | 1919 |
| `pawgreport-read/default` | 4450 | `afl/afl-pawgreport-read/cmin-queue-default-20260731T145320Z` | 2418 |
| `specseptotiff-compress/default` | 3109 | `afl/afl-specseptotiff-compress/cmin-queue-default-20260731T145320Z` | 1669 |
| `pawgreport/default` | 3164 | `afl/afl-pawgreport/cmin-queue-default-20260731T145320Z` | 1790 |
| `dump/default` | 2683 | `afl/afl-dump/cmin-queue-default-20260731T145402Z` | 1523 |
| `applysearch/main` | 1510 | `afl/afl-applysearch/cmin-queue-main-20260731T145403Z` | 837 |
| `profilevisualize/main` | 1769 | `afl/afl-profilevisualize/cmin-queue-main-20260731T145403Z` | 802 |

Use the minimized outputs as the next resume input only after current crashes are
triaged. Do not merge them into checked-in corpora wholesale.

## XML Hang Pruning

The XML hang queues were minimized non-destructively into local cmin directories.
The result shows these should not be bulk-deleted without replay triage.

| Target instance | Hang inputs | Minimized output | Kept |
| --- | ---: | --- | ---: |
| `fromxml/default` | 77 | `afl/afl-fromxml/cmin-hangs-default-20260731T145844Z` | 74 |
| `fromxml-noid/default` | 59 | `afl/afl-fromxml-noid/cmin-hangs-default-20260731T145844Z` | 59 |

Next action: replay these minimized hangs with the same timeout and sanitizer
environment, bucket those that still hang, and delete or archive only the ones
that now replay as normal parse failures.

## Lane Adjustments

`afl/targets.sh` was adjusted for the low-yield lanes before long runs:

- `specseptotiff-tiff`: requires zero-exit seed dry runs, disables trimming, and
  keeps the wrapper lane in no-forkserver/bin-check-skip mode. Seed-only staging
  produced 282 seeds and rejected 161 crashing or hanging seeds.
- `pngdump`: now screens PNG seeds by `file(1)`, pulls from `fuzz/graphics/png`,
  `test-profiles`, and `extended-test-profiles`, caps generated inputs at 256 KiB,
  and keeps it as a bounded lane. Seed-only staging produced 181 PNG seeds.
- `pngdump-inject`: remains a fixed-PNG, fuzzed-ICC smoke lane and is capped at
  64 KiB generated inputs. Seed-only staging produced 350 ICC seeds.
- `applysearch-hybrid-pcc`: caps generated inputs at 64 KiB and keeps the small
  screened hybrid seed set. Seed-only staging produced 8 valid seeds.
- ApplySearch weight lanes: generated inputs are capped at 8 KiB, with the
  positive-fast lane capped at 2 KiB. Seed-only staging produced 78 positive,
  51 positive-fast, 3 zero, 3 negative, and 69 finite-max seeds.
- `fromxml` and `fromxml-noid`: explicitly use text input mode, the existing XML
  dictionary, bounded 256 KiB generated inputs, and seed dry runs. Seed-only
  staging produced 437 `fromxml` seeds and 431 `fromxml-noid` seeds.

## Ignore And Artifact Policy

`git check-ignore` confirmed these remain out of git:

- AFL output trees, including `fastresume.bin` and local `.profraw` under output.
- AFL cmin output directories.
- Generated report artifacts under `afl/reports/generated/`.
- Generic `*.log` files.

Local seed-only validation removed stale `fastresume.bin` files from several AFL
output trees and wrote ignored `pruned-seeds-*` directories for oversized or
incompatible seeds.

## Recommended Next Corpus Actions

1. Replay and bucket minimized crash inputs first. Preserve original AFL
   filenames in issue text or metadata.
2. Resume high-pending lanes from the minimized cmin outputs, not the full
   original queues.
3. Keep XML/JSON/config re-seeding until after the minimized queue resumes are
   stable, so the next run does not inflate pending counts again.
4. Promote only minimized, durable reproducer inputs into checked-in corpora,
   and only when they document a regression or a new parser feature.
5. Treat `specseptotiff-tiff`, `pngdump`, `pngdump-inject`,
   `applysearch-hybrid-pcc`, and the ApplySearch weight lanes as bounded smoke
   or short-cycle discovery lanes until new coverage growth justifies longer
   CPU allocation.
