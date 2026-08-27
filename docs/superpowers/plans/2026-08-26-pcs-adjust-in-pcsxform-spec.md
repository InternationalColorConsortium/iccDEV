# Spec: PCS adjustments belong to `CIccPcsXform`

**Status:** approved for implementation
**Date:** 2026-08-26
**Branch:** `refactor/pcs-adjust-in-pcsxform`

> **Halt note:** R5-R7 below are written in the imperative but were not all
> carried out. Task 5 (the deletions R5/R6 describe) is halted pending a
> repository-owner decision, and R7's "documented as ignored" claim is also
> not what shipped. See the contract matrix in
> `docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md` ("Corrected
> contract matrix") for what is actually true of the tree today.

## Problem

A PCS adjustment (absolute-colorimetric media-white scaling, the v2-perceptual
black-point shift, or a `IIccAdjustPCSXform` hint such as BPC) is applied in
*two different places* depending on where the transform sits in the chain:

- On an **interior** PCS connection, `CIccCmm::CheckPCSConnections()` clears
  `m_bDstPcsConversion` on the upstream xform and `m_bSrcPcsConversion` on the
  downstream one, then `CIccPcsXform::Connect()` pushes the adjustment as
  `CIccPcsStepScale` / `CIccPcsStepOffset` steps. `Optimize()` can then fold
  them away.
- On a **leading or trailing PCS edge**, nothing clears those flags, so the
  device xform performs the adjustment itself inside `Apply()` via
  `CheckSrcAbs()` / `CheckDstAbs()` / `AdjustPCS()`. `CIccPcsXform::ConnectFirst()`
  and `ConnectLast()` contain complete code for the adjustment guarded on
  `NeedAdjustSrcPCS()` / `NeedAdjustDstPCS()`, but those predicates are
  `m_bAdjustPCS && !m_bSrcPcsConversion` and the flags are only ever cleared by
  the interior loop — so **those branches are unreachable**.

Measured on `defaultcmyk.icc` (a v2 CMYK output profile) with an instrumented
build:

| Chain | Where the adjust runs |
|---|---|
| `CMYK -> PCS -> CMYK` (two profiles, perceptual both ways) | `CIccPcsXform`; steps `Lab2ToXYZ, Scale(0.99651527), Offset(+k), Offset(-k), Scale(1.00349689), XYZToLab2` fold to `icCmmStatIdentityXform` and the whole `CIccPcsXform` is deleted |
| `Lab -> CMYK` (leading PCS edge) | inside the xform; the edge `CIccPcsXform` carries only `CIccPcsStepLabToLab2` |
| `CMYK -> Lab` (trailing PCS edge) | inside the xform; the edge `CIccPcsXform` carries only `CIccPcsStepLab2ToLab` |
| `PCS -> CMYK -> PCS` (the shape `CIccApplyBPC` builds) | inside the xform, at **both** ends |

Forcing the interior case down the in-xform path changes results by ~2e-5 in
CMYK percent — the "small residual" that prompted this work.

`IccCmm.h:556` already labels `m_bSrcPcsConversion` / `m_bDstPcsConversion`
`//Temporary field`, and the trailing-edge condition in
`CheckPCSConnections()` already tests `NeedAdjustPCS()`. This refactor finishes
what was started.

## Goal

`CIccPcsXform` performs every PCS adjustment. `CIccXform::Apply()` performs
none. Whether a given adjustment survives is decided by
`CIccPcsXform::Optimize()`, not by a flag.

## Requirements

**R1.** Every PCS adjustment currently reachable through `CheckSrcAbs()` /
`CheckDstAbs()` must instead be pushed as `CIccPcsStep`s by `Connect()`,
`ConnectFirst()`, or `ConnectLast()`.

**R2.** The leading-edge condition in `CheckPCSConnections()` must build a
`CIccPcsXform` whenever the following xform needs a source-side adjustment,
mirroring the trailing-edge condition which already does.

**R3.** Per-subclass conditions that today live inside `Apply()` must be
answerable at `Begin()` time:

- `CIccXformMpe` skips the adjustment when
  `!(m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent)`
  (B2D3 / D2B3 tags carry absolute colorimetry already).
- `CIccXformNamedColor` skips the source-side adjustment when the source space
  is a spectral PCS (`IsSpaceSpectralPCS(m_nSrcSpace)`), and only performs
  either side at all when `IsSrcPCS()` / `IsDestPCS()`.

**R4.** Direction must be preserved exactly. `CheckSrcAbs()` acts only when
`!m_bInput`; `CheckDstAbs()` acts only when `m_bInput`. The replacement
predicates must carry the same restriction, so an abstract-class xform (PCS on
both sides) still adjusts on exactly one side.

**R5.** `m_bSrcPcsConversion`, `m_bDstPcsConversion`, `NeedAdjustSrcPCS()` and
`NeedAdjustDstPCS()` are removed.

**R6. Superseded.** Originally: `CheckSrcAbs()`, `CheckDstAbs()` and
`AdjustPCS()` are `protected`, so a third-party `CIccXform` subclass may call
them from its own `Apply()`. They are retained and marked deprecated, not
deleted. The repository owner overturned this after the fact: retaining them
converted a compile error at a third-party call site into a silent
double-application, because `CIccPcsXform` now performs the adjustment at the
connection unconditionally and the flags that used to guard against a second
application were already gone. The three functions, and the `m_AbsLab`
scratch buffer only `CheckSrcAbs()` used, have been deleted.

**R7.** `bUsePCSConversions` on `CIccCmm::Begin()` /
`CIccNamedColorCmm::Begin()` / `CheckPCSConnections()` keeps its signature for
source compatibility but is documented as ignored. A caller that passed `true`
previously got the in-xform path; after this change there is no such path.

**R8.** No clip step is introduced. `AdjustPCS()` ends with
`CIccPCSUtil::NegClip` on an XYZ PCS (guarded by `SAMPLEICC_NOCLIPLABTOXYZ`);
the pushed step chain is pure affine and does not clamp. This is an accepted
behavior change for negative XYZ values, pinned by a test so it is reviewed
rather than silent. See Risks.

## Non-goals

- Making a `PCS -> device -> PCS` round trip numerically exact. The two
  adjustments bracket the device LUTs, are never algebraically adjacent, and
  `Optimize()` cannot cancel them. This refactor removes one redundant
  `Lab<->XYZ` conversion per edge, which shrinks the residual; it does not
  zero it.
- Changing `CIccApplyBPC`'s black-point algorithm.
- Changing what any adjustment computes. Only *where* it is applied moves.

## Test strategy

The adjustment is large and the float-path noise is small, and that gap is the
oracle:

- v2-perceptual adjustment: scale `0.99651527`, i.e. **~3.5e-3** relative.
- absolute media-white adjustment: profile-dependent, typically **1e-3 to 1e-1**.
- reordering / re-association noise from moving the same affine math into
  `CIccPcsStep`s: **~1e-7** relative (measured).

So a tolerance band of `1e-5` relative cleanly separates "the adjustment is
still applied exactly once" from "it was dropped, doubled, or inverted", while
tolerating legitimate float reassociation. Every numeric assertion in this work
uses that band. Strict bit-equality is the wrong oracle here and must not be
used — it would fail on correct code.

## Risks

| Risk | Mitigation |
|---|---|
| Every PCS-edged chain moves numerically. That includes all absolute-colorimetric media-white handling, which is far more traffic than the v2-perceptual case that prompted this. | Tolerance-banded characterization tests written *before* any behavior change (Task 1), covering absolute and perceptual, Lab-PCS and XYZ-PCS. |
| `xform-abstorel-adjust.cpp`, `pcs-edge-metadata.cpp` and `v2-legacy-pcs.cpp` sit directly on the changed path. | Full CTest sweep in Task 8; each is inspected individually. |
| BPC black-point estimates shift slightly, moving anything downstream. | Task 7 measures and records the deltas explicitly. |
| Dropping the `NegClip` (R8) changes results for negative XYZ. | Task 6 pins the delta with a dedicated test. If review rejects the change, the documented fallback is: in `Optimize()`, after the fold, if the surviving chain still contains a `CIccPcsStepScale` or `CIccPcsStepOffset`, push a clip step and re-run the fold. Not implemented by default because a non-affine step in the middle would block the very cancellation this refactor exists to enable. |
