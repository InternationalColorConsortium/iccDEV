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

`interpolation` is `0` for linear or `1` for tetrahedral. `rendering_intent`
takes `0..3` plus the `+1000` / `+10000` modifiers, decoded exactly as
`iccApplyToLink` decodes them, so a chain given to either tool resolves the same
way.

| option | effect |
|---|---|
| `-pixels N` | pixels per buffer (default 1048576) |
| `-repeats N` | timed repeats per case (default 7) |
| `-threads L` | comma list of thread counts, e.g. `1,2,8` (default `1`) |
| `-perxform` | per-xform breakdown, including PCS steps |
| `-leaf` | isolated hot leaf functions |
| `-suite` | run the built-in case table, ignoring any chain arguments |
| `-csv` | machine-readable output |

Examples:

```sh
# one chain, per-xform breakdown
iccBenchApply -perxform 1 Testing/V2/v2RgbLut8.icc 1 Testing/V2/v2CmykLut16.icc 1

# the built-in table at three thread counts, as CSV
iccBenchApply -suite -csv -threads 1,2,8
```

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

## Profile path resolution

`-suite` resolves each Testing-relative path against two roots, in order:
`ICCDEV_BENCH_SOURCE_ROOT` then `ICCDEV_BENCH_BUILD_ROOT`, defaulting to `.`.
Both are needed because the POSIX profile fixture writes generated profiles into
the source tree while the Windows fixture writes them into the build tree.
