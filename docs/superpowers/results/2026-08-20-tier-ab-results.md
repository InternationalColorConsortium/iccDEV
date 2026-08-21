# Tier A/B results: retiring the hardening-era per-pixel guards

Date: 2026-08-20
Branch: `perf/hoist-hardening-guards`
Audit: the 15 tier-A and tier-B findings
Prior results: `2026-08-20-tier-c-results.md`

This is the branch that answers the original question directly: **did the
stability and vulnerability work cost measurable throughput?**

## Answer

**Almost none of it did.** Of the guards the hardening commits added to per-pixel
code, the measurable cost is close to zero. The wins on this branch come from two
things that are not guards at all: a gamma decode that was being redone per call,
and a zero-fill that was mostly dead.

That is worth stating plainly because it was the premise of the whole review. The
guards are individually a compare against a member or a null test, and on this
hardware those do not show up.

## Results

| finding | what it was | outcome |
|---|---|---|
| A3 | NDLut per-pixel min-with-16 clamps | done, also a correctness fix |
| A4 | three unreachable `isfinite(pos)` guards | done |
| A5 | `Interp2d` offset ceiling recomputed per pixel | done |
| A6 | `CIccTagCurve::Apply` ordering + gamma decode | **done, the win** |
| A7 | `CIccToneMapFunc::Apply` re-testing `Begin()`'s contract | done |
| A8 | CAM elements re-testing pointers and channel counts | done, check moved to `Begin()` |
| A9 | emission matrices re-testing an allocation | done |
| A10 | `CIccXform3DLut` scratch zero-fill | done, gated |
| A1, A2 | `Interp3dTetra` / `InterpND` channel bounds | **not done, and must not be** |
| B2, B3, B4 | interpreter loop overhead | done, measured nothing |
| B1 | Calc if/select offset re-validation | **not done** |
| B5 | Calc operand stack representation | **not done** |

### Measured

Two runs of best-of-8 interleaved A/B (method in the tier-C results):

| case | run 1 | run 2 | re-verified | why |
|---|---|---|---|---|
| `monochrome` | +6.2% | +16.6% | **+10.5%** | single-entry gamma curve |
| `mono-lab` | +3.1% | +9.9% | **+11.1%** | same, plus a CLUT |
| `matrix-trc` | +3.6% | +4.4% | **+3.0%** | TRC curves |
| all others | within noise | | within noise | |

The third column is a re-measurement taken after this work was rebased onto a
master roughly forty commits newer, against a reference built from that same
master. It matters because the harness had a defect in the interval --
`iccBenchApply`'s profile-root resolution was fixed in #2256, which had made the
benchmark vacuous on Windows, the platform all of these numbers were taken on.
The re-measurement was done specifically to check whether that invalidated them.
It did not: the same three cases move, in the same direction, by comparable
amounts, and every checksum is identical to the master reference.

The wins land exactly on the curve-dominated cases, which is what A6 predicts:
those fixtures use `curveType` with a single u8Fixed8 entry, so every call took
the gamma path that was re-decoding `m_Curve[0]` *and* computing an `nIndex` it
then discarded. Nothing else on this branch is individually resolvable.

## A1 and A2 were not done, and must not be

An earlier draft of this document described these as bounds "duplicated from
`Begin()`", and justified leaving them on cost grounds: removing them needs
`CIccCLUT::Begin()` to be able to refuse, it returns `void`, and widening it to
`bool` means touching ten call sites of a public API for two comparisons that
tier C showed measure nothing.

**That framing is wrong. They are not duplicates -- they are the only guard that
works,** and removing them would open a hole rather than tidy one. The chain
above them leaks at every step:

- `CIccMpeSpectralCLUT::Read` reads `m_nInputChannels` as a 16-bit value from the
  profile and validates only `< 1`. There is no upper bound, and the value is then
  narrowed to `icUInt8Number` when the CLUT is constructed, so a declared 17 stays
  17 and a declared 256 becomes 0. `CIccMpeCLUT::Read` in `IccMpeBasic.cpp` does
  bound it; the spectral path appears not to.
- `CIccCLUT::Init` rejects `m_nInput > 16` and returns false, but six of its eight
  call sites discard the return. Only the two lut8/lut16 read paths check it.
- `CIccCLUT::Begin` bails with a bare `return;` on the same condition, before
  filling `m_MaxGridPoint`, setting `m_nNodes`, or allocating `m_nOffset`, leaving
  the object unusable with no way for any of its nine callers to know.

So the per-pixel `if (m_nInput > 16) return;` in `InterpND`, and the matching
`m_nOutput` guard in `Interp3dTetra`, are what actually stop a malformed CLUT
reaching the interpolation. They are load-bearing.

They can only be retired after the read-path bound and the discarded `Init()`
returns are fixed, and that is a correctness change that does not belong in a
performance branch. It is being handled separately.

## Tier B has essentially nothing to offer

B2/B3/B4 were done: the debugger hook is read once instead of twice per opcode
(it is a file-static, so the compiler had to reload it after every `Exec`), and
two unreachable arithmetic guards are gone. **`mpe-calc` moved −1.4%** -- nothing,
on the case wholly dominated by that loop.

Attribution explains why, and retires B1 and B5 with it:

- The first `Mpe` is **98%** of the `mpe-calc` chain.
- That calculator has **zero branch ops**. B1 -- re-validating if/select offsets
  per pixel, which the audit called the highest-value target in tier B -- has
  nothing to act on.
- Its sub-elements are CurveSets of FormulaSegments with exponents 1.0 and 2.4,
  so `CIccFormulaCurveSegment::Apply` calls `pow()` per channel per pixel. At
  0.14 Mpx/s the cost is irreducible transcendental math.

That also explains a result from the previous branch: the CLUT clamp work moved
`mpe-calc` 0.0%. It is not CLUT-bound or interpreter-bound. It is `pow`-bound.

B5's operand-stack traffic is noise beside that, and replacing `std::vector` with
a raw buffer would mean removing underflow checks that a security reviewer would
reasonably want kept, for no measurable return.

**The audit called tier B the highest-value target in the review. That was wrong,
and it was wrong for a measurable reason: it counted checked-arithmetic calls
without asking what fraction of the runtime they represent.**

## A3 is also a correctness fix

`CIccXformNDLut::Begin()` bounded the input channel count at 256 while `Apply()`
copies into a fixed `Pixel[16]`, so a tag declaring 17 to 255 channels was
admitted and then silently truncated -- quietly wrong colour rather than an error.
The bound is now 16, with a matching output check, which also lets both per-pixel
clamps go.

Nothing in the corpus is affected: the 11-, 17- and 18-channel profiles all
resolve to `Mpe` xforms. `CIccXformNDLut` appears unreachable from the corpus
entirely, so the clamp removal is unmeasured and the correctness change untested.

## Process note: rebuild everything after a header change

Adding a member to `CIccCLUT` changes `sizeof`, and building only the
`iccBenchApply` target left sixty-odd regression binaries with object code
compiled against the old header but linking the new DLL. The result was
**16 failures, mostly SEGFAULTs and one heap corruption, across unrelated tests**
-- and a wasted bisect concluding that A3 was at fault when it was not. A full
rebuild gave 102/102 with the identical source.

The failure mode is bad in both directions: it can invent failures, and it can
just as easily hide real ones behind a spurious pass. Any CTest run that follows
a header change and a partial build is not evidence of anything.
