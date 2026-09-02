# iccBenchApply: a throughput harness for the apply paths

Date: 2026-08-20
Component: `Tools/CmdLine/IccBenchApply` (new)

## Problem

A review of every `Apply`, `ApplyN`, `ApplySequence`, and `Interp*` definition in
`IccProfLib/` found 23 places where the per-pixel path re-decides something
`Begin()` already establishes. Ten arrived with the hardening work — `c0609ecc`
("Fix: Integer Overflow & OOM", #494) and the CWE-400/834 bounds pass are the
two main sources — and the rest predate it.

None of that can currently be acted on with confidence, because **there is no
throughput measurement anywhere in the tree**. `grep -ri "benchmark\|throughput\|
pixels per second"` over `.github/`, `Testing/`, and `Tools/` returns nothing.
So neither the suspected regression nor any proposed fix can be demonstrated
rather than argued, and — more dangerous — a hoist that changes colour output
would not be caught by a timing-blind test suite.

This spec covers the harness only. The optimisation work it enables is scoped
separately (see "Branch plan").

## Goals

1. Report pixels/second for a profile chain, per xform within that chain, and
   for individual hot leaf functions.
2. Emit a stable output checksum per case, so a later change can be shown to be
   numerically equivalent rather than merely fast.
3. Sweep thread counts, and assert the checksum is invariant across them.
4. Run in CI without ever reddening a build.

## Non-goals

- No performance gate. GitHub's shared runners vary 20–30% between runs on the
  same commit; a threshold would flake. Record only.
- No changes to `IccProfLib/` on this branch. The harness must be a clean
  baseline to measure against, which it is not if it also edits the library.
- No new profile-generation machinery. The repo already has a CTest fixture for
  that (see "Library and repository constraints", item 2).

## Library and repository constraints

Six facts shape the design. Each was verified against `029dc30c`.

1. **The chain is enumerable through public API after `Begin()`.**
   `CIccCmm::IterateXforms(IXformIterator*)` (`IccProfLib/IccCmm.h:1814`) hands
   out a `const CIccXform*` for each xform in the resolved chain — including the
   `CIccPcsXform` entries that `Begin()` inserts between profiles.
   `CIccXform::GetXformType()` (`:393`) classifies each, and
   `CIccXform::GetNewApply(icStatusCMM&)` (`:446`) is public — the `protected:`
   block in `CIccXform` does not begin until roughly `:516`. Per-xform timing
   inside a live chain therefore needs no friend declarations and no changes to
   the library.

2. **Most test profiles do not exist until generated, and a fixture already
   generates them.** Only 80 `.icc` files under `Testing/` are tracked, and
   nearly all are tiny `CalcTest/calcUnderStack_*` POCs rather than workloads.
   The real sources are 191 tracked `.xml` files converted by `iccFromXml`.
   `Build/Cmake/Testing/CMakeLists.txt:5298` already registers
   `iccdev.create-profiles` with `FIXTURES_SETUP iccdev_profiles` (and
   `:4783` registers `iccdev.windows-create-profiles` likewise), so a test needs
   only `FIXTURES_REQUIRED iccdev_profiles`.

3. **Generated profiles land in different trees per platform.** The comment at
   `Build/Cmake/Testing/CMakeLists.txt:1746` records that the POSIX fixture
   writes into the source tree while the Windows fixture writes into the build
   tree. Profile paths must therefore be resolved against both roots at runtime,
   not hardcoded to either.

4. **The chain-parsing helper that `iccApplyNamedCmm` uses is not available to a
   library-only tool.** `CIccCfgProfileSequence` comes from `IccConnect.h`
   (`Tools/CmdLine/IccApplyNamedCmm/iccApplyNamedCmm.cpp:80,280`), so reusing it
   would pull IccConnect and IccJSON into the binary. `iccApplyToLink` parses the
   same encoded-intent chain syntax while linking `${TARGET_LIB_ICCPROFLIB}`
   alone (`Build/Cmake/Tools/IccApplyToLink/CMakeLists.txt`), and is the model to
   follow.

5. **Multi-pixel apply is chunked, and that matters for interpretation.**
   `CIccApplyCmm::Apply(dst, src, nPixels)` (`IccProfLib/IccCmm.cpp:8671`)
   processes `kCmmChunkPixels` through each xform in turn before advancing, to
   keep each xform's CLUT warm in cache. Timing xforms individually defeats that
   locality, so per-xform times will sum to more than the whole-chain time.

6. **`Begin()` is contractually idempotent.** `CIccCmm::Begin()` returns early
   when `m_pApply` is set — the contract `e4e05c3a` restored for #1940. Any
   Begin-time precomputation added later must be idempotent, so the harness
   should exercise a second `Begin()`.

## Design

### Files

```
Tools/CmdLine/IccBenchApply/iccBenchApply.cpp    new
Tools/CmdLine/IccBenchApply/Readme.md            new
Testing/V2/v2GrayTRC.xml                         new (see "New test asset")
Build/Cmake/Tools/IccBenchApply/CMakeLists.txt   new
Build/Cmake/CMakeLists.txt                       +1 ADD_SUBDIRECTORY near :1749
Build/Cmake/Testing/CMakeLists.txt               + case registration
Testing/CreateAllProfiles.sh                     +1 iccFromXml line
Testing/CreateAllProfiles.bat                    +1 iccFromXml line
```

`Build/Cmake/Tools/IccBenchApply/CMakeLists.txt` is a copy of
`IccApplyToLink`'s, including its `IF(NOT TARGET ...)` guard, linking
`${TARGET_LIB_ICCPROFLIB}` only.

### CLI contract

```
iccBenchApply {options} interpolation {profile_path rendering_intent {-PCC pcc_path}}...

  interpolation      0 = Linear, 1 = Tetrahedral   (as iccApplyToLink)
  rendering_intent   0..3 plus the +1000 / +10000 modifiers iccApplyToLink parses

  -pixels N          pixels per buffer         (default 1048576)
  -repeats N         timed repeats per case    (default 7)
  -threads L         comma list, e.g. 1,2,8    (default 1)
  -perxform          per-xform breakdown, including PCS xforms
  -leaf              tier 3, isolated leaf functions
  -csv               machine-readable output
  -suite             run the built-in case table, ignoring any chain arguments
```

The trailing repeating `profile intent` group is the chain. This is what makes
non-round-tripping profiles usable: `SixChanCameraRef` is `scnr` class and can
only appear first, as can the new `v2GrayTRC`.

### Tier 1 — whole chain

Build a `CIccCmm`, `AddXform` each chain element, `Begin()`, then time
`Apply(dst, src, nPixels)` over the whole buffer. This is the headline number
and the only one that reflects the chunked path in constraint 5.

### Tier 2 — per xform, including PCS

With `-perxform`: after `Begin()`, collect the chain via `IterateXforms`, then
for each xform obtain a `CIccApplyXform` from `GetNewApply()` and time it over
the same buffer, feeding it the output of the previous stage. Report type,
Mpx/s, and share of the summed total.

The report must carry the constraint-5 caveat inline, so nobody adds the column
up and compares it to tier 1:

```
chain: v2RgbLut8 [rel] -> v2CmykLut16 [rel]
  #  xform type                Mpx/s   share
  0  3DLut  (RGB->Lab)          14.2     38%
  1  PCS    (LabToXYZ, Scale,
             XYZToLab)          31.8     17%
  2  4DLut  (Lab->CMYK)          9.6     45%
  note: per-xform timing defeats the chunked-apply cache locality that tier 1
        measures; shares are for attribution, not a decomposition of tier 1.
```

The PCS row is the point of this tier. It is where the sparse-matrix and
`icXYZtoLab` findings live, and it is invisible in tier 1.

### Tier 3 — isolated leaf functions

With `-leaf`: pull tags from the chain's profiles and drive the hot leaves
directly through public accessors — `CIccMBB::GetCLUT()` and `GetCurvesA()` are
public (`IccProfLib/IccTagLut.h:495-496`). Covers `CIccTagCurve::Apply`,
`Interp3d`, `Interp3dTetra`, `InterpND`, and a Calc `Apply`. Reported in
Mvalues/s, clearly separated from the Mpx/s tiers.

### Threading axis

For each thread count in `-threads`, attach via
`CIccThreadedCmm::Attach(pCmm, n)` (`IccProfLib/IccCmmThread.h:153`) and repeat
tier 1. Report throughput and scaling factor.

**The checksum must be identical across every thread count.** This is the
assertion that catches a later hoist parked on the shared object when it should
have been in the apply object. A mismatch fails the case.

### Checksum oracle

Every case hashes its destination buffer (FNV-1a over the raw bytes) and prints
the result beside the timing. Two purposes:

- It consumes the output, so the compiler cannot elide the apply loop.
- It is the equivalence gate for the optimisation branches. `perf/hoist-invariants`
  must reproduce every checksum bit-identically. `perf/hoist-hardening-guards`
  must too, except for the one deliberate `+inf` semantic change in the
  `m_UnitClipFunc` work, which then shows up as exactly one moved checksum
  instead of hiding among many.

Float bit patterns are hashed, not decimal renderings, so the oracle is exact.
This means it is also sensitive to legitimate FP reassociation from compiler flag
changes; the Readme states that a checksum diff means "investigate", not
"regression", and that comparisons are only valid between builds with matching
flags.

### Input buffer

Deterministic LCG with a fixed seed, so runs are comparable. The buffer is
partitioned:

- rows 0..n-2: in-gamut values in [0,1]
- final row: pathological — negative, >1, +inf, -inf, NaN

The pathological row is deliberate. Several findings concern clamp and
non-finite handling (`ClutUnitClip`, the `isfinite` tests in the interpolators
and sampled curves); a happy-path-only buffer would leave them unexercised and
would let a wrong hoist pass the oracle.

### Methodology

`std::chrono::steady_clock`. One warm-up pass, then `-repeats` timed passes.
Report the **median**, with min and max. Median because runner noise is
one-sided — interrupts and contention only ever slow a pass down — so a mean is
biased low and a min is optimistic.

C++17 is already the project standard (`Build/Cmake/CMakeLists.txt:226`), and
`<chrono>` is not currently included anywhere in `IccProfLib/` or `Tools/`, so
this introduces it.

### New test asset

`Testing/V2/v2GrayTRC.xml` — a minimal v2 Gray→XYZ profile with a `grayTRC`.

Required because **no tracked source produces a `CIccXformMonochrome`**. The one
Gray profile in the tree, `Testing/Display/GrayGSDF.xml`, is `link` class with
`DataColourSpace` and `PCS` both `GRAY`, so it never instantiates that xform.
`CIccXformMonochrome::Apply` (`IccProfLib/IccCmm.cpp:5630`) is the largest single
per-pixel win in the audit, and it currently has no coverage of any kind.

Follows the existing three-file `Testing/V2/` pattern
(`v2RgbMatrixTRC.xml`, `v2RgbLut8.xml`, `v2CmykLut16.xml`) and is added to both
`CreateAllProfiles.sh` and `CreateAllProfiles.bat`.

### Built-in case table

`-suite` runs these. Every source below was verified present and of the stated
class in the tracked tree.

| case | chain | covers |
|---|---|---|
| `matrix-trc` | `v2RgbMatrixTRC` 1 → `sRGB_D65_MAT` 1 | C5, C7 |
| `lut-3d-tetra` | `v2RgbLut8` 1 → `v2CmykLut16` 1 | A1, A5, A6, A10, C6 |
| `lut-nd-6ch` | `SpecRef/SixChanCameraRef` 1 → `v2CmykLut16` 1 | A2, A3, C6, C1 |
| `monochrome` | `v2GrayTRC` 1 → `v2RgbMatrixTRC` 1 | C2 |
| `mpe-calc` | `Calc/srgbCalcTest` 1 → `sRGB_D65_MAT` 1 | all of tier B |
| `mpe-tonemap` | `Display/Rec2100HlgFull` 1 → `sRGB_D65_MAT` 1 | A7 |
| `pcs-rel` | `v2RgbLut8` 1 → `v2CmykLut16` 1 | PCS baseline |
| `pcs-abs` | `v2RgbLut8` 3 → `v2CmykLut16` 3 | C5; delta vs `pcs-rel` isolates PCS adjust |

`sRGB_D65_MAT.icc` is tracked, so it needs no generation. The rest come from the
`iccdev_profiles` fixture.

Case identity is the chain, not the profile, so a case whose profile is missing
reports `SKIP` with the reason. A build configured without `ENABLE_ICCXML` still
runs the tracked-`.icc` cases rather than failing.

### CTest integration

One test, `iccdev.apply-throughput`:

- `FIXTURES_REQUIRED iccdev_profiles`
- `LABELS "iccdev;iccprofLib;benchmark;throughput;slow"`
- runs `-suite -csv` with a reduced `-pixels` for CI wall-clock
- passes if every case either produced a checksum or reported `SKIP`; **no time
  threshold is asserted**

`iccBenchApply` is a normal tool target, not an `iccdev_add_regression_executable`
helper, so the test invokes the built binary. `add_dependencies(build-test-binaries
iccBenchApply)` makes `check` build it.

The `slow` label puts it on the existing opt-out path rather than inventing a new
one: `check-fast` (`Build/Cmake/Testing/CMakeLists.txt:88`) already excludes
`slow`, while `check` (`:29`) excludes only `known-red` and so still runs it.
That is the correct pairing — the test is record-only and cannot fail on timing,
so there is no reason to hide it from `check`, only from the fast loop.

Also asserts constraint 6: one case runs `Begin()` twice and requires the second
call to report `icCmmStatOk` and leave the checksum unchanged.

### Error handling

- Unreadable or missing profile: `SKIP` for that case with the path and reason;
  exit 0 in `-suite` mode. A chain given explicitly on the command line is an
  error, exit non-zero — an explicit request that cannot be honoured should fail
  loudly.
- `AddXform` or `Begin()` failure: report the `icStatusCMM` name and skip the
  case. `iccApplyToLink.cpp:1114-1132` documents that `AddXform` owns and frees
  the profile on failure (#1327); the same ownership rule applies here.
- Allocation failure for the pixel buffer: error and exit non-zero.
- Thread count exceeding `hardware_concurrency`: honoured but annotated, since
  oversubscribed scaling numbers are not meaningful.

### Testing the harness itself

The harness is a measuring instrument, so its own correctness matters:

- Checksum determinism: the same case run twice in one process, and across two
  processes, must produce the same checksum. Asserted in `-suite`.
- Thread invariance: covered by the threading assertion above.
- A known-answer case: the `matrix-trc` chain applied to a handful of hand-checked
  values, verified against the same values pushed through `iccApplyToLink`, so a
  future refactor cannot silently change what the harness measures.

## Branch plan

Three branches, in order. Each merges before the next starts, so every
optimisation is measured against a stable baseline.

1. `bench/throughput-harness` — this spec. No `IccProfLib/` changes.
2. `perf/hoist-invariants` — the 8 tier-C findings. No hardening guard touched.
3. `perf/hoist-hardening-guards` — the 15 tier-A and tier-B findings. Isolated
   so a security reviewer can read the guard removals without pure-perf edits
   interleaved.

### Conventions binding branches 2 and 3

**Every removed check leaves a comment.** Not "removed for perf" — the next
reader must be able to re-derive the argument, including which commit introduced
the guard:

```cpp
// Bound moved to CIccCLUT::Begin() (#2210): m_nOutput is fixed before any
// Apply() and Begin() now refuses >16, so the per-pixel test could only
// re-confirm what Begin() proved. Original guard added by c0609ecc (#494).
```

(`#2210` stands in for the issue number of the branch-2 or branch-3 work; each
comment cites its own.)

For a check that was never reachable, the comment states why rather than citing
a move.

**Precomputed state is placed by mutability, for thread safety.** `CIccXform`
and `CIccPcsStep` objects are shared across threads; `CIccApplyXform`,
`CIccApplyPcsStep`, `CIccApplyMpe`, and `CIccApplyCmm` are per-thread.

- *Immutable after `Begin()`* → the shared object. Covers the monochrome white
  point, curve gamma, white-point reciprocals, cached src/dst spaces, the
  PCS-conversion enum, flag decodes, and the scratch-init flag.
- *Written during `Apply()`* → the per-apply object, allocated in
  `GetNewApply()`. Only two findings need this: `CIccPcsStepSrcSparseMatrix`,
  whose matrix is re-pointed per pixel, and the Calc operand stack, which already
  lives in `CIccApplyMpeCalculator` and changes representation only.
- `CIccPcsStepSparseMatrix` — the non-`Src` half — is immutable once built and
  belongs on the step, *not* the apply object. Stated explicitly because the two
  are near-identical at the call site and only one is shareable.

All Begin-time precomputation must be idempotent, per constraint 6.

## Risks

- **Runner noise makes small wins unmeasurable in CI.** Accepted: CI records,
  and the numbers that decide anything are taken locally on a quiet machine.
- **`-perxform` shares mislead if read as a decomposition.** Mitigated by the
  inline note; the alternative — omitting the tier — loses the PCS visibility
  that motivated it.
- **The oracle is sensitive to FP reassociation.** Documented in the Readme;
  comparisons are only valid across builds with matching compiler flags.
- **The oracle cannot catch a wrong hoist in code no case reaches.** The
  pathological input row narrows this, but coverage is bounded by the case table;
  see the accepted gap below.

## Accepted gaps

**`InterpND` has no coverage, and will not get any in this harness.**
`CIccCLUT` dispatches 5- and 6-channel input to the dedicated `Interp5d` and
`Interp6d` routines, so `lut-nd-6ch` exercises `Interp6d` and never reaches the
`InterpND` default path. A true `InterpND` case needs a ≥7-channel source. The
only 7CLR profile in the tree, `private/Turquoise-Magenta-Yellow-Violet-Green-
Blue-Orange_output.icc`, is untracked, and sourcing or authoring a tracked
7-channel profile is deferred to a separate future effort.

Two consequences to carry forward, rather than rediscover:

1. `lut-nd-6ch` is named for what it measures — `Interp6d` — and the Readme and
   the case table must not imply `InterpND` coverage. Findings A2 and A3 concern
   `InterpND` specifically.
2. Branch 3 therefore lands A2 and A3 **without throughput evidence**. Both are
   still supportable: A2 is a textual duplicate of bounds `CIccCLUT::Begin()`
   already asserts (`IccProfLib/IccTagLut.cpp:2430,2450` versus `:3480,3487`),
   and A3 is primarily a correctness fix — it converts silent truncation of a
   >16-channel N-D LUT into an explicit `icCmmStatInvalidLut` — with the removed
   per-pixel clamps a secondary benefit. The checksum oracle still covers them
   for equivalence via `Interp5d`/`Interp6d` and the shared `CIccXformNDLut`
   entry path. This should be stated in those PRs rather than left implicit.

## Open items

None blocking.
