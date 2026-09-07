# iccBenchApply

Measures apply-path throughput through an ICC profile chain, and emits an output
checksum per case so a change can be shown numerically equivalent as well as
fast.

Links `IccProfLib` only. `iccApplyNamedCmm`'s `CIccCfgProfileSequence` would have
supplied the chain parser for free, but it lives in `IccConnect.h`, and pulling
IccConnect and IccJSON into a binary whose purpose is measuring IccProfLib puts
their code in the thing being measured.

## Usage

```
iccBenchApply {options} interpolation {profile_path rendering_intent {-PCC pcc_path}}...
```

`interpolation` is `0` for linear or `1` for tetrahedral. `rendering_intent` is
decoded column by column, exactly as `iccApplyToLink` decodes it: the units digit
is the base intent `0`-`3`, the tens digit is the transform-lookup type
(`icXformLutType`, reachable values `0`-`9`; `+10` drops the D2Bx/B2Dx tags and
`+40` adds black-point compensation), `+100` requests luminance matching, and
`+1000` uses a V5 sub-profile if present. A negative code is rejected here exactly
as `iccApplyToLink` rejects it — the column arithmetic drops the sign, so `-10`
used to resolve as `10` in both tools (#2268).

Two columns used to diverge and no longer do (#2271). The hundreds column was
stripped without being read, so `0`, `100` and `200` built the identical chain;
and a tens digit of `4` reached `AddXform()` as the lookup type `icXformLutBPC`
rather than as a hint, so `iccBenchApply … profile.icc 40` failed with `Invalid
Look-Up Table type` where `iccApplyToLink … profile.icc 40` succeeded. Both are
closed, so this tool can now time a BPC or luminance-matched chain;
`iccdev.bench-apply-bpc-changes-result` and `iccdev.bench-apply-luminance-column`
hold it to that by comparing the applied checksum against the same chain without
the hint.

`+10000` is **not** a separate modifier here, despite what this file said before:
the sub-profile test is `(code / 1000) > 0`, so `10000` requests exactly what
`1000` requests. A `+10000` column does exist in
`CIccCfgProfileSequence::fromArgs()` — the decode behind `iccApplyNamedCmm`,
`iccApplyProfiles` and `iccApplySearch` — which is a genuinely different decode:
it strips `% 1000` rather than `% 100`, so there the hundreds column is part of
the transform type and `100`-`130` select
`icXformLutSpectral`-`icXformLutNamedDevice` instead of requesting luminance
matching. The same code means different things in the two families (#2270).

| option | effect |
|---|---|
| `-pixels N` | pixels per buffer (default 1048576) |
| `-repeats N` | timed repeats per case (default 7) |
| `-threads L` | comma list of thread counts, e.g. `1,2,8` (default `1`) |
| `-perxform` | per-xform breakdown, including PCS steps |
| `-leaf` | isolated hot leaf functions |
| `-metrics` | report the benchmark buffers and workload; with `-csv`, writes metrics to stderr |
| `-suite` | run the built-in case table; takes no chain arguments |
| `-csv` | machine-readable output |

Examples:

```sh
# one chain, per-xform breakdown
iccBenchApply -perxform 1 Testing/V2/v2RgbLut8.icc 1 Testing/V2/v2CmykLut16.icc 1

# the built-in table
iccBenchApply -suite

# the built-in table at three thread counts, as CSV
iccBenchApply -suite -csv -threads 1,2,8

# the built-in table, bounded for a quick check
iccBenchApply -suite -pixels 65536 -repeats 3
```

## Metrics report

`-metrics` reports the exact source and destination benchmark-buffer sizes,
their total allocation footprint, the logical input-plus-output buffer capacity
per scheduled apply, pixel count, and warm-up plus timed apply count. It reports
once for a command-line chain and once for each resolved `-suite` case.
`benchmark_bytes_per_apply` is therefore a logical capacity value, not a claim
about physical DRAM traffic: cache residency, write allocation, prefetching,
and transform-local storage are platform- and profile-dependent. These values
describe the tool's allocations and scheduled calls; they do not claim to
represent all memory touched inside a profile's resolved transform graph.

`thread_count_settings` is the number of requested `-threads` values, not the
number of workers in one apply. The per-thread-setting values describe one
whole-buffer warm-up plus timed sequence. The `total_*` values multiply those
scheduled calls across all requested settings, so `-threads 1,2,8 -repeats 3`
reports 3 thread-count settings, 3 timed and 4 scheduled applies per setting,
9 timed applies, and 12 scheduled applies total.

The report intentionally omits hardware instructions, stack operands, and
floating-point operations. They depend on the compiler, target ABI,
optimization level, resolved transforms, and scalar/SSE/AVX dispatch. Use the
opt-in Linux profiler, `.github/scripts/iccdev-clut-profile.sh`, for numeric
hardware instruction, memory-operation, and supported floating-point event
counts. Stack operands need architecture-specific disassembly or sampling, so
they are not portable benchmark output.

`-suite` and a chain are alternatives, not a chain with a default. Passing both
is refused rather than silently resolved in favour of the table, and `-perxform`
and `-leaf` are refused with `-suite` because the table reports whole-chain
throughput only. Anything the tool will not act on, it says so about.

The chain is a repeating trailing group, which is what makes non-round-tripping
profiles usable: an input-only profile such as `SpecRef/SixChanCameraRef.icc` or
`V2/v2GrayTRC.icc` can only appear first.

## What it measures

**Whole chain.** `CIccCmm::Apply(dst, src, nPixels)` over the whole buffer. This
is the headline number and the only one that reflects the chunked multi-pixel
path in `CIccCmm.cpp`.

**Per xform** (`-perxform`). After `Begin()`, the chain is enumerated through
`CIccCmm::IterateXforms` and each xform timed individually — including the
`CIccPcsXform` entries `Begin()` inserts between profiles, which are invisible in
the whole-chain figure.

**Isolated leaves** (`-leaf`). `CIccCurve::Apply` and the 3D CLUT interpolators,
driven directly through the public `CIccMBB::GetCLUT()` / `GetCurvesA()`
accessors. Reported in Mval/s.

## Four ways to misread the output

1. **Per-xform shares attribute cost; they do not decompose the chain figure.**
   Timing xforms one at a time defeats the chunked-apply cache locality the chain
   number benefits from, so the per-xform times sum to *more* than the chain
   time. Measured on a `v2RgbLut8 -> v2CmykLut16` chain, the summed per-xform
   times work out to 4.54 Mpx/s against the chain's 4.94.

2. **Checksums only compare across builds with matching compiler flags.** The
   hash is over raw float bit patterns, which is what makes it an exact
   equivalence oracle — and also what makes it sensitive to legitimate
   floating-point reassociation. A checksum difference between two different
   builds means *investigate*, not *regression*.

3. **`spectral-6ch` measures an MPE chain, not `InterpND`.** `SixChanCameraRef`
   is MPE-based, so it resolves to an `Mpe` xform and never reaches
   `CIccXformNDLut`. `CIccCLUT` also dispatches 5- and 6-channel input to
   dedicated `Interp5d` / `Interp6d` routines, so `Interp5d`, `Interp6d` and
   `InterpND` all have **no coverage** in this harness. Closing that needs a
   tracked LUT-based profile with at least 5 channels, and is a separate effort.

4. **No timing threshold is asserted anywhere.** CI records; it never gates. The
   numbers that decide anything should be taken locally on an otherwise idle
   machine, because shared runners vary 20-30% between runs on the same commit.

## The built-in case table

`-suite` runs these. Each name describes the chain that actually gets built,
which is not always what the profile names suggest.

| case | resolved chain | notes |
|---|---|---|
| `matrix-trc` | MatrixTRC → PCS → Mpe | |
| `lut-3d-tetra` | 3DLut → PCS → 3DLut | also carries the `Begin()` re-entry check |
| `spectral-6ch` | Mpe → PCS → 3DLut | see misreading 3 above |
| `monochrome` | Monochrome → MatrixTRC | no PCS step: XYZ PCS on both sides |
| `mpe-calc` | Mpe → PCS → Mpe | the Calculator element |
| `mpe-tonemap` | Mpe → Mpe | no PCS step |
| `pcs-rel` | 3DLut → PCS → Mpe | relative colorimetric |
| `pcs-abs` | 3DLut → PCS → Mpe | absolute; the delta against `pcs-rel` isolates PCS adjustment |

The `pcs` pair is `V2/v2RgbLut8.icc` (D50) → `sRGB_D65_MAT.icc` (D65)
deliberately. The obvious choice of reusing the `lut-3d-tetra` pair does not
work: both `Testing/V2` LUT profiles carry a `mediaWhitePointTag` of exactly D50,
which makes the absolute adjustment an identity and produces a checksum
byte-identical to the relative run. The delta only exists when the two profiles
disagree about the white point.

A case whose profile is missing reports `SKIP` with a reason and does not fail,
so a build configured without `ENABLE_ICCXML` still runs the cases whose profiles
are tracked. Most of the corpus is generated by `Testing/CreateAllProfiles.sh`
(or `.bat`) from tracked XML; the CTest registration declares
`FIXTURES_REQUIRED iccdev_profiles` so that runs first.

## What makes it fail

Never a slow number. Only:

- a **checksum that changed between thread counts**, which means state that
  should live in the per-thread apply object is being shared, or
- a **second `Begin()`** that changed the status or the output, which breaks the
  idempotency contract restored for #1940.

## Input data

Deterministic LCG fill with a fixed seed, so runs are comparable. The final row
of the buffer is deliberately pathological — negative, greater than one, ±inf,
and NaN — because several of the clamp and non-finite paths worth measuring would
otherwise never execute, and a happy-path-only buffer would let an incorrect
change pass the checksum oracle unnoticed.

## Runtime, and what looks like a hang

The suite gives every case the same pixel budget, and the cases do not cost the
same per pixel. On one Release host the spread across the table was `monochrome`
at 33.77 Mpx/s against `mpe-calc` at 0.18 -- a factor of about 190 -- so at the
default 1048576 pixels and 7 repeats, `mpe-calc` alone accounts for most of the
run. On a debug or sanitizer build, where every case is slower again by an order
of magnitude, that single case can run for tens of minutes.

That is slow, not stuck. Each row's case name and thread count are printed and
flushed before the measurement starts, so the case in progress is always the one
named last. Under `-csv` that notice goes to stderr instead -- `running <case>
t=<n>` -- because a record stream cannot carry a half-written row; stdout stays
exactly the CSV a parser expects. Use `-pixels` to bound a run; the registered
`iccdev.apply-throughput` test uses `-pixels 65536 -repeats 3`.

## Exit status

Zero when every measured case agreed with itself across thread counts and across
a second `Begin()`. Nonzero on a checksum that moved, on an argument the tool
will not act on, and -- since #2254 -- when the run measured no cases at all.
Individual `SKIP`s are tolerated, because a case that cannot resolve on one
platform is expected; nine of nine is not a benchmark, it is a misconfiguration,
and it used to exit zero.

## Profile path resolution

`-suite` resolves each Testing-relative path against two roots, in order:
`ICCDEV_BENCH_SOURCE_ROOT` then `ICCDEV_BENCH_BUILD_ROOT`, defaulting to `.`.
Both are needed because the POSIX profile fixture writes generated profiles into
the source tree while the Windows fixture writes them into its own work
directory.

Each root is tried two ways -- as a directory that *contains* `Testing/`, and as
a `Testing` tree itself -- so `iccBenchApply -suite` works from the repository
root and from inside `Testing/`, and a fixture directory that is already a
Testing tree can be named directly. The containing form is tried first, so a
repository root resolves exactly as it always did.

Note that all nine cases open a *generated* profile first. The one tracked
profile in the table, `ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc`, is never
first in its chain, so without the `create-profiles` fixture having run every
case reports `SKIP` and the run fails; see **Exit status** above. The two
registered tests that drive the suite are therefore only registered where a
profile-generating target exists.
