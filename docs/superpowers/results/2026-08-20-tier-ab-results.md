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

## A1 and A2 were not done, and were correct not to be here

Two drafts of this section have now been wrong in opposite directions, and the
reason is worth recording, because both errors came from reasoning about
reachability without measuring it.

The first draft called these bounds "duplicated from `Begin()`" and justified
leaving them on cost grounds. The second called them **"the only guard that
works"** on the strength of a chain of unchecked returns leading from
`CIccMpeSpectralCLUT::Read` down to the interpolators.

The chain was real, and every link in it was described accurately. Its
*conclusion* was not. That was established by probing the library rather than
reading it, in the branch that fixed the underlying defect
(`fix/clut-channel-count-validation`):

- **`m_nInput > 16` never reached a CLUT through any read path.** Both element
  readers follow the discarded `Init()` with `pData = m_pCLUT->GetData(0); if
  (!pData) return false;`. `Init()` returns before the `delete[]`/reallocation
  when it refuses, the constructor had already set `m_pData` to NULL, and
  `GetData(0)` is `&m_pData[0]`. So the dropped return was caught by accident.
  Declared counts of 17, 255, 256 and 272 all made `Read()` return false.
- Which means the per-pixel `if (m_nInput > 16) return;` in `InterpND` and the
  `m_nOutput` guard in `Interp3dTetra` were **not reachable from a malformed
  profile either**. They were neither duplicates nor load-bearing. They were
  unreachable, and the honest reason to leave them alone was that no one had
  established which.
- The defect that *was* reachable is the narrowing cast's **truncation**, not its
  overflow: a declared 257 became a one-dimensional CLUT under an element still
  reporting 257 input channels, and 259/260/272 did the same onto 3, 4 and 16.
- `CIccCLUT::Init` had six of **ten** call sites discarding its return, not six of
  eight. `CIccMpeExtCLUT::Read` and `CIccCLUT::Read` check it too.

The fix landed on that branch: the count is bounded before the cast in every
parser (`Read`, `icCLutFromXml`, `icCLUTFromJson`), all ten `Init()` returns are
acted on, `CIccCLUT::Begin()` returns `bool` and refuses a CLUT that `Init()`
never made usable, and the MPE `Begin()` overrides check the element's declared
count against its CLUT's dimensionality.

**With that in place the per-pixel guards became retireable** -- not because they
were duplicates, but because the invariant they were the last expression of is
now established once, at `Begin()`, on every construction path. They have since
been retired there, as a correctness change rather than a performance one: tier C
measured them at nothing, and one of the two turned out to be a defect. The
`m_nOutput > 16` test in `Interp3dTetra` was justified on the claim that "valid
LUT profiles cap output channels at <=16", which is false -- `m_nOutput` is an
`icUInt16Number` precisely because MPE CLUT elements need more, and every sibling
interpolator writes `m_nOutput` values unbounded. For a legal 3-input CLUT with
>=17 outputs it returned without writing anything at all.

The lasting point for this document: A1/A2 were correctly left out of a
performance branch, but the reasoning offered for that in both earlier drafts was
guesswork. "This guard is load-bearing" and "this guard is redundant" are the
same claim about reachability, and neither is knowable by reading the call chain
alone.

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
