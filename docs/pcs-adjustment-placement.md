# PCS adjustment placement

On a colorimetric PCS, every PCS adjustment -- absolute-colorimetric
media-white scaling, the v2-perceptual black point shift, and
`IIccAdjustPCSXform` hints such as BPC -- is performed by `CIccPcsXform`.
`CIccXform::Apply()` performs none: the guarded `CheckSrcAbs()`/`CheckDstAbs()`
call sites are gone, along with the `m_bSrcPcsConversion`/`m_bDstPcsConversion`
flags that used to route work through them. The three helpers themselves are
retained but deprecated (see "Deprecated" below).

On a spectral PCS the XYZ media-white adjustment does not apply at all;
`NeedsSrcPcsAdjust()`/`NeedsDstPcsAdjust()` answer false there and
`CIccPcsXform` pushes an element-wise spectral white point conversion instead.
See `docs/superpowers/plans/2026-08-26-spectral-pcs-white-point-conversion.md`.

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

`CIccXform::CheckSrcAbs()`, `CheckDstAbs()` and `AdjustPCS()` are no longer
called from anywhere in `IccProfLib`. They are `protected` rather than private,
so a third-party `CIccXform` subclass may call them from its own `Apply()`;
they are kept for that reason alone. Such a subclass now applies the adjustment
**twice** -- `CIccPcsXform` has already performed it at the connection -- and
should stop calling them.

`bUsePCSConversions` on `CIccCmm::Begin()`, `CIccNamedColorCmm::Begin()` and
`CheckPCSConnections()` is ignored. It used to select the in-xform path at an
interior PCS connection; there is no such path any more, so the interior
`CIccPcsXform` is now built unconditionally for colorimetric PCS links. The
parameter is retained for source compatibility. No caller in this tree ever
passed `true`.

## Known gap: spectral PCS at a chain edge

A spectral **interior** connection is handled: `CIccPcsXform::Connect()` pushes
the element-wise spectral white point conversion on whichever side owes it.

A spectral **chain edge** is not. `CIccCmm::CheckPCSConnections()`'s two edge
blocks gate on `IsSpaceColorimetricPCS()`, and a spectral signature never
satisfies that, so no `CIccPcsXform` is built at a spectral edge and
`ConnectFirst()`/`ConnectLast()`'s own spectral branches never get to run. A
chain that begins or ends on a spectral PCS therefore gets no white point
conversion at that edge.

Nothing silently mangles the pixel there any more: the XYZ media-white
adjustment that used to fire inside `Apply()` at such an edge is gone with the
rest of the in-xform path, and `NeedsSrcPcsAdjust()`/`NeedsDstPcsAdjust()`
answer false at a spectral port regardless. The chain simply carries the tag's
own absolute-ness through unchanged. `spectralTrailingEdgeNoLongerAdjustsInsideApply()`
in `.github/ci/regression/pcs-adjust-placement.cpp` pins that, and
`CmmProbe::inertAdjustCount()` records the residue: the xform still reports
`NeedAdjustPCS()`, but neither port asks for the adjustment, so no
`CIccPcsXform` will ever pick it up.

Whether the edge blocks should be widened to cover spectral ports is an open
decision for the repository owner.
