# Tier C results: hoisting pre-existing per-pixel invariants

Date: 2026-08-20
Branch: `perf/hoist-invariants`
Plan: `docs/superpowers/plans/2026-08-20-hoist-invariants-tier-c.md`

Running record. Updated as each task lands; findings are recorded whether the
result was positive, negative, or unmeasurable.

## Measurement method

The workstation this was measured on shows about **30% run-to-run variance** on
the same binary for a single median run, which is far more than most of these
changes are worth. Three corrections were needed before any number could be
trusted:

1. **Interleaved A/B, best-of-N.** Two builds run alternately across N rounds,
   comparing the fastest observed pass per case. Interference only ever makes a
   run slower, so the fastest sample is the least polluted.
2. **Reference snapshots must capture the DLL, not just the executable.**
   `iccBenchApply` links `IccProfLib2.dll` dynamically, so copying only the `.exe`
   silently measures the *new* library. Mixing a new executable with an old DLL is
   not a valid shortcut either: Task 1 added a virtual to `CIccPcsStep`, changing
   vtable layout.
3. **Alternate which build runs first each round.** Running ref-then-new every
   round makes load drift inside a round bias systematically against whichever
   build runs second. Before this was fixed, several cases read 3-9% "slower" for
   a change that could not execute in them at all.

With all three in place, a null test of two identical builds gives **±1.5%, one
case at 3.2%**. Changes above about 4% are signal; below that is not
distinguishable on this hardware.

**The `-perxform` tier cannot be used for A/B comparison.** Its variance is 41 to
73 Mpx/s across five runs of the same binary -- each stage runs a short burst
rather than a sustained loop, so it never reaches steady state. It is sound for
attribution (which stage dominates a chain) and useless for deciding whether a
change helped.

**Chain numbers understate stage-level wins.** A hoist inside one xform is
diluted by the rest of its chain, so the percentages below are lower bounds on
the value of each change, not measures of it.

## Results

| finding | outcome | measured |
|---|---|---|
| C1 sparse PCS matrix built once | kept | **no coverage exists** |
| C2 monochrome reference white | kept | **+5.7% / +6.5%** |
| C3 MPE PCS encodings cached | **reverted** | **−4.9%, consistent** |

### C1 -- build the sparse PCS matrix once instead of per pixel

`CIccPcsStepSparseMatrix::Apply` constructed a `CIccSparseMatrix` per pixel, and
`CIccSparseMatrix::Init` unconditionally deletes and re-news its accessor -- a
heap allocation and free on every pixel. Moved into a new
`CIccPcsStep::BeginStep()` hook called once from `CIccPcsXform::Begin()`.

**No measurement is possible.** `CIccPcsStepMatrix::reduce()` only produces a
sparse step when the PCS matrix is more than 25% zeros, and instrumenting
`BeginStep()` showed **zero** constructions across the whole benchmark suite,
every spectral chain the corpus can form, and the one bispectral sparse-matrix
profile in the tree (`Named/SparseMatrixNamedColor` is `nmcl` class, so its sparse
matrix lives in the named-colour tag, not a PCS step).

This corrects the audit, which rated the finding "hot" on the strength of it
being a per-pixel allocation without checking whether the path is reached. Kept
because a latent per-pixel allocation will bite whoever does reach it in a real
bispectral workflow, but no speed claim is attached to it.

### C2 -- precompute the monochrome reference white in Begin()

`CIccXformMonochrome::Apply` rebuilt the PCS-encoded perceptual reference white on
every pixel from compile-time constants. Moved to `Begin()`, held on the xform
because it is immutable afterwards.

| case | before | after | change |
|---|---|---|---|
| `monochrome` (XYZ PCS) | 32.37 | 34.22 | **+5.7%** |
| `mono-lab` (Lab PCS) | 21.25 | 22.64 | **+6.5%** |

All checksums bit-identical: the output branch keeps its division rather than
multiplying by a precomputed reciprocal, because `x/w` and `x*(1/w)` are not
bit-identical and one divide is negligible beside what was hoisted. Keeping the
oracle a strict equality check across the whole branch was worth more than the
last few percent.

`mono-lab` required a new fixture. `v2GrayTRC` has an XYZ PCS, so the Lab branch
-- the only one that reaches `XyzToLab` and its three cube roots -- was never
executed. `Testing/V2/v2GrayTRCLab.xml` closes that gap, and is worth having
independently: it gives the repo its first coverage of that branch.

### C3 -- cache the MPE PCS encodings (REVERTED)

`CIccXformMpe::Apply` calls `GetSrcSpace()`/`GetDstSpace()` per pixel, each
branching four ways through the profile header, plus an invariant intent
comparison. Caching all of it in `Begin()` looked like a clear win.

**It measured consistently slower and was reverted.**

| run | rounds | `mpe-calc` |
|---|---|---|
| 1 | 4 | −4.4% |
| 2 | 4 | −4.4% |
| 3 | 6 | −5.7% |
| 4 | 12 | −4.9% |
| null test, identical builds | 4 | −1.2% |

Every other case flipped sign between runs, which is the signature of noise.
`mpe-calc` never did -- and `mpe-calc` is the case dominated by
`CIccXformMpe::Apply`, the only function the change touched.

The likely mechanism is that the hoist was not trading computation for a cheaper
load. The three new members sit past a large `CIccXform` base, so `Apply()` reads
them from a high offset and plausibly a colder cache line, whereas what it read
before -- `m_nIntent`, `m_nTagIntent`, and `m_pProfile->m_Header.pcs` -- is
already hot, because the surrounding apply path touches the header constantly.
Removing redundant work is a reason to *expect* a win, not evidence of one.

**A correctness bug was also found here, and it is the more important finding.**
The first attempt folded `m_bSrcPcsConversion`/`m_bDstPcsConversion` into the
cached decision. Those are mutated *after* the xform's `Begin()` has run:
`CIccCmm::Begin()` runs the per-xform `Begin()` loop at `IccCmm.cpp:9797` and only
then calls `CheckPCSConnections()` at `:9803`, which calls
`SetDstPCSConversion(false)`/`SetSrcPCSConversion(false)` on the xforms either
side of an inserted PCS xform. The cached value went stale and the
absolute-colorimetric output changed: `pcs-abs` moved
`0xd0a37c30` → `0x5c36a0b6`.

That would have shipped silently. `pcs-abs` was the *only* case that moved, and
it exists only because an earlier commit deliberately repointed the PCS pair at
profiles with differing white points -- with the original D50/D50 pair, absolute
and relative produced byte-identical output and this bug would have passed the
gate unnoticed.

**Consequence for the remaining tasks:** "immutable after `Begin()`" cannot be
assumed from a member's type or name. `CIccCmm::Begin()` does substantial work
both before and after the per-xform loop, so any candidate for caching needs its
write-ordering checked against that sequence specifically. This applies directly
to the remaining tasks that plan to cache flags on `CIccXform`.
