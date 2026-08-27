# Spectral PCS white-point conversion

**Status:** approved for implementation
**Date:** 2026-08-26
**Branch:** `refactor/pcs-adjust-in-pcsxform`
**Precedes:** Task 5 of [the PCS adjustment refactor](2026-08-26-pcs-adjust-in-pcsxform.md)

## Why this exists

The PCS adjustment refactor found that a v5 spectral profile at absolute
colorimetric intent, at a chain edge, has samples 0-2 of its spectral vector
scaled by the *XYZ* media-white adjustment — `AdjustPCS()` treats the first
three samples as X, Y, Z and leaves the rest untouched. At an interior spectral
connection the same adjustment is dropped instead. Pinned by
`spectralTrailingEdgeStillAdjustsInsideApply()` in
`.github/ci/regression/pcs-adjust-placement.cpp`.

Both behaviours predate the refactor. Task 5 (deleting the in-`Apply()`
machinery) was halted rather than silently removing the edge case.

## Decision

The repository owner ruled: implement the conversion properly.

> Relative spectra should be absolute spectra divided by the spectral white
> point, or absolute spectra should be relative spectra multiplied by spectral
> white point. Absolute tags with absolute intent have no conversion, relative
> tags with relative intent have no conversion.

Scope: **reflectance, transmission and radiant** spectral PCS only. Bidirectional
reflectance and sparse-matrix PCS are excluded — their sample vectors are not a
plain spectrum, so element-wise scaling is not dimensionally meaningful.

This lands as its own commit, before Task 5's deletions, so a behaviour change is
not buried inside a mechanical refactor.

## Requirements

**S1 — the conversion.** Element-wise against the spectral white point:
`relative = absolute / white`, `absolute = relative × white`.

**S2 — when it applies.** Convert if and only if the tag's absolute-ness differs
from the rendering intent's:

| `m_nIntent` | `m_nTagIntent` | Conversion |
|---|---|---|
| absolute | absolute | none |
| relative | relative | none |
| absolute | relative | relative → absolute |
| relative | absolute | absolute → relative |

i.e. `(m_nIntent == icAbsoluteColorimetric) != (m_nTagIntent == icAbsoluteColorimetric)`.

**S3 — direction per port.** Mirrors how `CheckDstAbs()`/`CheckSrcAbs()` divide
the work today:

- **Destination port of an input xform** (device → PCS): the tag emits values in
  the tag's absolute-ness; the CMM's PCS must carry the intent's. Convert
  tag-space → intent-space. Tag relative + intent absolute ⇒ **multiply**.
- **Source port of an output xform** (PCS → device): the CMM's PCS carries the
  intent's absolute-ness; the tag expects its own. Convert intent-space →
  tag-space. Intent absolute + tag relative ⇒ **divide**.

The source port is the exact inverse of the destination port, exactly as the
colorimetric path inverts `m_PCSScale` for an output xform.

**S4 — applicable spectral PCS types.** `icSigReflectanceSpectralPcsData`,
`icSigTransmissionSpectralPcsData`, `icSigRadiantSpectralPcsData`. Not
`icSigBiDirReflectanceSpectralPcsData`, not `icSigSparseMatrixSpectralPcsData` —
those take no conversion, and that exclusion must be asserted by a test so it
cannot drift.

**S5 — the XYZ path must stop firing at a spectral port.** `CIccXform`'s base
`NeedsSrcPcsAdjust()` / `NeedsDstPcsAdjust()` must return false when the
corresponding port space is a spectral PCS. This is what stops `AdjustPCS()`
mangling samples 0-2, and it is the behaviour change this document authorises.

**S6 — where the scale is pushed.** All three connection sites, both sides:

- `CIccPcsXform::Connect()`'s spectral branches. **Note the existing asymmetry:**
  those branches push only `pToXform->NeedsSrcPcsAdjust()` today
  ([IccCmm.cpp:2469](../../../IccProfLib/IccCmm.cpp#L2469), :2490, :2538, :2559,
  :2604, :2625) — there is no `pFromXform` push anywhere in the spectral region,
  so the from-side is currently dropped. Both sides must be handled.
- `CIccPcsXform::ConnectFirst()` — no spectral adjustment handling exists.
- `CIccPcsXform::ConnectLast()` — no spectral adjustment handling exists.

**S7 — missing white point means no conversion.** If
`icSigSpectralWhitePointTag` is absent or unreadable, perform no conversion
rather than failing. This matches how the colorimetric path degrades when the
media white point tag is missing: `calcMediaWhiteXYZ()` falls back to the
illuminant, making media white and illuminant equal, so `bNeedAbsAdjust`'s
inequality test yields no adjustment.

**S8 — a zero or non-finite white point sample is a bad profile.** Division by it
must not produce an infinity. Reject with `icCmmStatInvalidProfile`, matching
`CheckForInvalidPCSScale()`'s treatment of a zero colorimetric scale.

## Design constraints already established

- **The spectrum:** `FindTag(icSigSpectralWhitePointTag)`, a `CIccTagNumArray`,
  read with `GetValues(buf, 0, samples)`. The single-argument `GetValues()`
  overload defaults to `nVectorSize = 1` and copies exactly one value — see the
  comment at [IccProfile.cpp:4080](../../../IccProfLib/IccProfile.cpp#L4080),
  which documents a real bug caused by exactly that. Always pass the count.
- **A profile can disagree with itself about its sample count.**
  `icGetSpaceSamples(spectralPCS)` and `m_Header.spectralRange.steps` are the
  same only when the range describes the vector actually stored.
  `Testing/Display/LaserProjector.icc` declares a 401-channel radiant signature
  in its `spectralDataInfo` tag and disagrees elsewhere. The scale vector must be
  dimensioned to match the pixel the step will operate on, and a mismatch must be
  rejected, never silently truncated or padded.
- **The primitive:** `CIccPcsXform::pushScale(icUInt16Number n, const icFloatNumber *vals)`
  ([IccCmm.h:1198](../../../IccProfLib/IccCmm.h#L1198)) pushes an N-channel
  `CIccPcsStepScale`. `m_PCSScale[3]` cannot hold a spectrum; do not try to widen it.
- **Access:** `CIccPcsXform` is a friend of `CIccXform`
  ([IccCmm.h:389](../../../IccProfLib/IccCmm.h#L389)), so the `Connect*` functions
  can read `m_nIntent`, `m_nTagIntent` and `m_pProfile` directly.
- **Reciprocals, not two code paths.** Pushing `1/white` for one direction and
  `white` for the other means an adjacent inverse pair folds to identity in
  `Optimize()` exactly as the colorimetric pair does — which is the whole point of
  the surrounding refactor. Compute one vector and invert it, rather than writing
  two independent loops.

## Non-goals

- Changing what the *colorimetric* adjustment computes. Only the spectral ports
  change.
- Bidirectional reflectance and sparse-matrix PCS (S4).
- Completing Task 5. That follows this work, separately.

## Test strategy

The same tolerance discipline as the parent refactor: **1e-5 relative**, never
bit-equality. Assertions must be able to fail — three assertions and one
measurement were removed from this branch for being unable to.

Required coverage:

1. **The existing pin flips.** `spectralTrailingEdgeStillAdjustsInsideApply()`
   currently asserts samples 0-2 ARE modified by the in-`Apply()` path. After S5
   that must no longer happen. Rewrite it to assert the correct new behaviour,
   and keep a comment recording what it used to pin and why that changed.
2. **Round-trip identity.** A relative tag applied at absolute intent, connected
   to a chain that converts back, must return the original spectrum to within the
   band — the multiply and the divide are exact inverses and should fold.
3. **The conversion actually fires**, with a white point that is not all ones, so
   a dropped conversion is detectable. Assert against an independently computed
   expected spectrum, not against a second run of the same path.
4. **All four rows of S2's table**, including both no-conversion rows.
5. **The S4 exclusions**: a bidirectional-reflectance and a sparse-matrix PCS
   port take no conversion.
6. **S7 and S8**: a missing white point converts nothing; a zero sample is
   rejected rather than producing an infinity.
