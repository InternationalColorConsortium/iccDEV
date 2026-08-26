# PCS adjustment placement

On a colorimetric PCS, every PCS adjustment -- absolute-colorimetric
media-white scaling, the v2-perceptual black point shift, and
`IIccAdjustPCSXform` hints such as BPC -- is performed by `CIccPcsXform`;
`CheckPCSConnections()` clears the flags that used to route the same work
through `CIccXform::Apply()`. The old `CheckSrcAbs()`/`CheckDstAbs()` call
sites are still in the tree (see "Deprecated" below) and, on the one PCS
shape those flags don't reach, still run for real (see "Known gap" below).

## Where each adjustment lands

| Chain shape | Owner |
|---|---|
| interior PCS connection | `CIccPcsXform::Connect()` |
| leading PCS edge | `CIccPcsXform::ConnectFirst()` |
| trailing PCS edge | `CIccPcsXform::ConnectLast()` |

`CIccPcsXform::Optimize()` decides what survives. Two adjustments that are exact
inverses and adjacent in the PCS fold to `icCmmStatIdentityXform` and the whole
`CIccPcsXform` is dropped.

## What moved numerically

Moving the same affine math from `AdjustPCS()` into `CIccPcsStep`s
re-associates it, changing results by ~1e-7 relative. At a PCS edge the
adjustment now merges with the neighbouring `Lab<->XYZ` conversion, removing one
round trip per edge.

`AdjustPCS()` clamped negative components on an XYZ PCS via
`CIccPCSUtil::NegClip`. The step chain is pure affine and does not clamp, so a
negative XYZ component that used to be forced to zero now survives. A clip step
was considered and rejected: a non-affine step between the two adjustments would
block the cancellation this design exists to enable.

That clamp was reachable only through an `IIccAdjustPCSXform` hint whose `Scale`
exceeds 1, because only a negative offset can drive a non-negative input
negative. The built-in adjustments never produce one -- the absolute-intent path
uses a zero offset with a positive scale, and the v2-perceptual path uses a
positive offset. `CIccApplyBPC` is the hint that can, so BPC on an XYZ-PCS
profile is the whole of the exposure.

`xyzPcsChainStillAdjusts()` in
`.github/ci/regression/pcs-adjust-placement.cpp` pins that an XYZ-PCS chain
still applies its adjustment after the move -- it does not, and cannot,
demonstrate the clipping delta itself, since the built-in adjustments never
drive a negative offset.

## What did not change

A `PCS -> device -> PCS` round trip is not exact. The two adjustments bracket
the device LUTs, are never algebraically adjacent, and cannot cancel. This
affects `CIccApplyBPC`'s black-point probes, which build exactly that shape.

## Deprecated

`CIccXform::CheckSrcAbs()`, `CheckDstAbs()` and `AdjustPCS()` are still called
from `CIccXform::Apply()` -- every xform implementation in `IccCmm.cpp` guards
a `CheckSrcAbs()`/`CheckDstAbs()` call on its `m_bSrcPcsConversion`/
`m_bDstPcsConversion` flags. `CheckPCSConnections()` clears those flags on the
xform once it hands the same adjustment to a `CIccPcsXform`, so on a
colorimetric PCS the old call sites go quiet rather than double-applying.
Retiring the call sites outright is a separate, currently halted task (see
"Known gap" below for the one case where the flag is never cleared and the
old path still fires for real).

`bUsePCSConversions` on `CIccCmm::Begin()`, `CIccNamedColorCmm::Begin()` and
`CheckPCSConnections()` still has an effect, but only at interior PCS
connections -- the leading- and trailing-edge blocks don't consult it at all.
Passing `true` keeps an interior connection's old `CheckSrcAbs()`/
`CheckDstAbs()` path live instead of folding it into a `CIccPcsXform`. No
caller in this tree passes `true`, so in practice every chain gets the
`CIccPcsXform` path regardless.

## Known gap: spectral PCS at a chain edge

`CIccXform::GetDstSpace()` reports a spectral signature when
`m_bUseSpectralPCS` and the profile declares a `spectralPCS`, but
`CheckPCSConnections()`'s two edge blocks gate on `IsSpaceColorimetricPCS()`,
so they never hand the adjustment over at a spectral edge. Meanwhile
`CIccXform::Begin()` sets `m_bAdjustPCS` from `m_Header.pcs`, which stays Lab
or XYZ on a v5 spectral profile. A v5 spectral profile applied at absolute
colorimetric intent therefore still runs `AdjustPCS()` inside `Apply()`, which
applies an XYZ media-white scale and offset to the first three samples of the
spectral vector and leaves the rest untouched.

The interior connection path does not handle spectral correctly either --
it drops the adjustment instead of exposing it. All six spectral-source
branches of `CIccPcsXform::Connect()` (`IccProfLib/IccCmm.cpp:2469, 2490,
2538, 2559, 2604, 2625`) push only `pToXform->NeedsSrcPcsAdjust()`; there is
no `pFromXform->NeedsDstPcsAdjust()` push anywhere in the spectral region.
Meanwhile the interior loop in `CheckPCSConnections()` clears **both**
`m_bSrcPcsConversion` and `m_bDstPcsConversion` before calling `Connect()`,
for a spectral interior connection exactly as it does for a colorimetric
one (the loop's guard condition explicitly includes
`IsSpaceSpectralPCS(...)`). So at an interior spectral connection the
from-side adjustment is silenced in `Apply()` by the cleared flag and never
picked up by `Connect()` -- it is **dropped**, not applied.

The two spectral cases are therefore asymmetric: at a spectral chain edge
the adjustment **fires** (inside `Apply()`, because the flag there is never
cleared); at a spectral interior connection it is **dropped** (the flag is
cleared, but `Connect()` never pushes the from-side step). Neither is
"handling spectral correctly," and the asymmetry is itself evidence that
the edge behavior is accidental rather than intended: nothing in the design
motivates firing at one spectral shape and dropping the same adjustment at
another. This is pinned by
`spectralTrailingEdgeStillAdjustsInsideApply()` in
`.github/ci/regression/pcs-adjust-placement.cpp`; both the edge-fires and
interior-drops behavior are unchanged from `master` -- this is pre-existing,
not a regression introduced by this branch -- and are awaiting a decision
from the repository owner.
