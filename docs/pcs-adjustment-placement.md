# PCS adjustment placement

`CIccXform::Apply()` performs no PCS adjustment at all. That part is
unconditional: the guarded `CheckSrcAbs()`/`CheckDstAbs()` call sites are gone,
along with the `m_bSrcPcsConversion`/`m_bDstPcsConversion` flags that used to
route work through them. The three helpers themselves are retained but
deprecated (see "Deprecated" below).

Every adjustment that *is* performed -- absolute-colorimetric media-white
scaling, the v2-perceptual black point shift, and `IIccAdjustPCSXform` hints
such as BPC -- is performed by `CIccPcsXform`. But the **handover** to it
happens only at a colorimetric PCS port. `CIccCmm::CheckPCSConnections()` builds
a `CIccPcsXform` only where a port satisfies `IsSpaceColorimetricPCS()` or
`IsSpaceSpectralPCS()`, and its two chain-edge blocks test only the former. The
port kinds are not treated alike, and the differences are not all deliberate:

| Port | What performs the adjustment |
|---|---|
| colorimetric PCS (XYZ, Lab) | `CIccPcsXform`, at the interior connection or at either chain edge |
| spectral PCS | not the XYZ adjustment at all -- `NeedsSrcPcsAdjust()`/`NeedsDstPcsAdjust()` answer false, and `CIccPcsXform` pushes an element-wise spectral white point conversion instead. Interior connections only; a spectral chain *edge* gets nothing (see "Known gaps") |
| MCS | nothing (see "Known gaps") |

See `docs/superpowers/plans/2026-08-26-spectral-pcs-white-point-conversion.md`
for the spectral conversion.

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

## Known gaps: ports that are not a colorimetric PCS

**One root cause, several symptoms.** `m_bAdjustPCS` can be set on an xform with
no guarantee that the port the adjustment would apply to is XYZ or Lab. Two of
the three setters in `CIccXform::Begin()` do test `IsSpacePCS(m_Header.pcs)` --
but `m_Header.pcs` is not the same thing as the *port*, and the third setter,
the `IIccAdjustPCSXform` hint path at `IccProfLib/IccCmm.cpp:1708-1723`, applies
no port test whatsoever: it sets the flag purely on `CalcFactors()` returning
true. Meanwhile `AdjustPCS()` unconditionally treats `pixel[0..2]` as X, Y and Z.

Every gap below is a consequence of that one mismatch. So was the spectral
interior connection, which used to drop the adjustment and now takes the white
point conversion -- the one instance fully closed so far. The other two are only
half closed: neither mangles a pixel any more, because the in-`Apply()` path that
did the mangling is gone, but neither has been given the conversion it actually
owes, and both are awaiting the repository owner.

A reader who holds the root cause in mind will predict the next instance rather
than discover it: look for any `GetSrcSpace()`/`GetDstSpace()` return that is
neither XYZ nor Lab, and ask what can set `m_bAdjustPCS` for it.

### Spectral PCS at a chain edge

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

### MCS ports, via an `IIccAdjustPCSXform` hint

`CIccXform::GetDstSpace()` returns `m_Header.mcs` for an `icToMCS` xform, with
no colorimetric test. `NeedsDstPcsAdjust()` excludes only spectral ports, so an
MCS port reaches it unexcluded; `CIccXformMpe`'s override adds only an intent
test, which is *satisfied* at a non-absolute intent rather than violated. And
`CheckPCSConnections()` builds no `CIccPcsXform` at an MCS port -- the edge
blocks require a colorimetric PCS, and the interior MCS clause requires the
*next* xform to be MCS too.

What reaches it: neither `IsSpacePCS(m_Header.pcs)` setter can fire on such an
xform except at absolute intent -- the v2-perceptual branch needs `IsVersion2()`
or `!HasPerceptualHandling()`, and an MCS xform is a v5 `CIccXformMpe`, which is
neither. The hint path can, and does.

`CIccApplyBPC` is the only in-tree `IIccAdjustPCSXform`; its `CalcFactors()` rejects
`icAbsoluteColorimetric` and the `icSigLinkClass` / `icSigAbstractClass` /
`icSigNamedColorClass` device classes, but it constrains the *profile*, never
the port. An `icSigInputClass` profile carrying both `AToB0` (which the BPC
black-point probe needs) and `AToM0`, added with `icXformLutMCS` at
`icPerceptual` with a `CIccApplyBPCHint`, satisfies every one of those checks
and reaches `Apply()` with `NeedsDstPcsAdjust()` true on an MCS port.

Before the `CheckSrcAbs()`/`CheckDstAbs()` retirement the guard flag at that
port was still set, so `CheckDstAbs()` ran and applied the BPC XYZ black-point
affine to MCS channels 0..2, leaving the rest alone. Through an identity
`AToM0` fed `0.20 0.40 0.60 0.80` that produced
`0.213048 0.409379 0.587843 0.800000`. Today nothing performs it and the
channels pass through unmodified -- so the retirement changed behaviour here.

It is the same defect as the spectral one, in the same direction as the ruling
already made there: MCS channels are not X, Y and Z, so the affine had no
meaning. Restoring it would restore the corruption. But it is a behaviour change
under a refactor whose contract was "changes no behaviour", so the ruling for
MCS is the repository owner's. The options are the spectral three: leave it
(the current state), build a `CIccPcsXform` at MCS ports, or reject the
configuration at `Begin()`.

`pcsAdjustHintReachesANonPcsPort()` in
`.github/ci/regression/pcs-adjust-placement.cpp` pins the current state and is
falsifiable in both directions: excluding MCS in the predicates flips
`CmmProbe::inertAdjustCount()`, and building a `CIccPcsXform` there flips
`pcsXformCount()`.
