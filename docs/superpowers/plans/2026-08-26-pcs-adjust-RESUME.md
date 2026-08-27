# Resume here — PCS adjustment refactor

**Branch:** `refactor/pcs-adjust-in-pcsxform`, forked from `master` at `68b1b6f2`
**Paused at:** `a60ea4e1`, 29 commits, every one reviewed and green
**Paused:** 2026-08-26
**Regression test:** `.github/ci/regression/pcs-adjust-placement.cpp`, 176 assertions

> **All eight plan tasks, the spectral conversion, and the helper deletion are
> complete.** Everything that remains needs a decision from the repository owner
> — see "Three open decisions" below. Nothing is half-finished in the tree.
>
> **`CheckSrcAbs()`, `CheckDstAbs()`, `AdjustPCS()` and `m_AbsLab` are deleted**
> (`bdbfa6a9`, `49c13bc9`). The spec's R6 originally said retain-and-deprecate;
> the owner overturned that, and R6 is marked superseded rather than rewritten.
> The reasoning: once `CIccPcsXform` performs the adjustment at the connection,
> a third-party subclass still calling those from its own `Apply()` would
> double-apply silently, because `m_bAdjustPCS` reads true regardless and the
> suppressing flags were already gone. Retention converted a compile error at
> the caller's own line into a silent wrong-colour bug. **This was the only new
> hazard the branch had introduced, and deleting them removed it.**
>
> **One root cause explains every gap this work found.** `m_bAdjustPCS` can be
> set on an xform with no guarantee its port is XYZ or Lab — the two
> `IsSpacePCS(m_Header.pcs)` setters test the profile *header*, not the port,
> and the `IIccAdjustPCSXform` hint path at
> [IccCmm.cpp:1708-1723](../../../IccProfLib/IccCmm.cpp#L1708-L1723) tests
> nothing at all — while `AdjustPCS()` unconditionally reads `pixel[0..2]` as
> X, Y, Z. A spectral chain edge, a spectral interior connection and an MCS port
> are three consequences of that single assumption. A reader who holds that in
> mind will predict the fourth instance rather than discover it.
> `docs/pcs-adjustment-placement.md` is the canonical statement of this.

## What is done

PCS adjustments (absolute-colorimetric media-white scaling, the v2-perceptual
black-point shift, `IIccAdjustPCSXform` hints such as BPC) now live in
`CIccPcsXform` for every **colorimetric** PCS port — interior connections and
both chain edges — so adjustments that cancel are folded away by `Optimize()`
instead of leaving a float residual.

Spectral PCS ports convert against the spectral white point
(`relative = absolute / white`, `absolute = relative × white`) at **interior**
connections. See [the spectral spec](2026-08-26-spectral-pcs-white-point-conversion.md).

Design record: [`docs/pcs-adjustment-placement.md`](../../pcs-adjustment-placement.md).
Contract matrix and BPC deltas: [here](2026-08-26-pcs-adjust-bpc-deltas.md).
Original spec and plan: [spec](2026-08-26-pcs-adjust-in-pcsxform-spec.md),
[plan](2026-08-26-pcs-adjust-in-pcsxform.md).

### Two real bugs this surfaced, both in code that had never executed

1. **`ConnectLast()` halved XYZ values.** Its adjustment branch assumed an
   actual-XYZ pixel where an XYZ-PCS xform emits *internal* XYZ, so the
   unconditional `pushXyzToXyzIn()` rescaled by 32768/65535 twice. Fixed in
   `823dc069`. Confirmed arithmetically: the XYZ-PCS absolute/relative ratio is
   now exactly `mediaX/illumX`.
2. **Spectral PCS ports were given the XYZ adjustment.** `AdjustPCS()` scaled
   samples 0-2 of a spectral vector as if they were X, Y, Z and left the rest
   untouched. The repository owner ruled on the correct behaviour; implemented
   in `21333dc5`.

Both branches were unreachable dead code until this work made them live.

## Three open decisions — all the repository owner's

**A. The MCS port.** An `icToMCS` xform is an *input* xform whose destination is
an MCS port. With a BPC hint it reaches `Apply()` with `NeedsDstPcsAdjust()`
true and no `CIccPcsXform` handover — because `GetDstSpace()` returns
`m_Header.mcs` with no colorimetric test, `NeedsDstPcsAdjust()` excludes only
spectral, `CIccXformMpe`'s override adds only an intent test (and is *satisfied*
at perceptual intent), and the edge blocks gate on `IsSpaceColorimetricPCS()`.
Before Task 5 that applied an XYZ black-point affine to MCS channels 0-2;
measured through an identity `AToM0` fed `0.20 0.40 0.60 0.80`, **before**
`0.213048 0.409379 0.587843 0.800000`, **after** `0.200000 0.400000 0.600000
0.800000`. Task 5's deletion stopped it, so **Task 5 did change behaviour**.
The new behaviour was left in place rather than reverting to restore the
corruption, and is pinned by `pcsAdjustHintReachesANonPcsPort()`.
*Is stopping that affine correct — the same reasoning already ruled on for
spectral — or should MCS ports get a defined adjustment of their own?* If the
former, the branch stands as-is. If the latter, that is another spec.

**B. The spectral chain edge.** A spectral chain *edge* gets no conversion:
interior connections have it, edges do not. `CheckPCSConnections()` gates both
edge blocks on `IsSpaceColorimetricPCS()`
([:9962](../../../IccProfLib/IccCmm.cpp#L9962),
[:10032](../../../IccProfLib/IccCmm.cpp#L10032)) and a spectral port never
qualifies, so `ConnectFirst()`/`ConnectLast()`'s spectral branches — implemented
and unit-tested — cannot run. Widening to `IsSpacePCS()` would complete the
ruling already given. Deliberately not done: it is a second behaviour change
beyond what the spectral spec authorises, and it makes two previously
unreachable functions live for a new port type, which is where every latent bug
on this branch was hiding. Wants its own commit and its own tests.

**C. Integration.** Merge to `master` locally / push and open a PR / keep the
branch. Not yet chosen.

## Completed work that this record previously listed as queued

### 1. Spectral fix round 1 — DONE (`1516fcce`, `769a34b3`, `0b8b5e9e`)

One Important finding plus six Minors from the review of `21333dc5`.

**Important.** `IccCmm.cpp:1856-1861` and `:1887` claim that dispatching
`NeedsSrcPcsAdjust()`/`NeedsDstPcsAdjust()` virtually from
`CheckSrcAbs()`/`CheckDstAbs()` "would apply those conditions twice". It would
not: `CIccXformMpe`'s override is character-identical to the guard already at
its `Apply()` call sites; `CIccXformNamedColor`'s call sites already sit inside
`if (IsSrcPCS())` / `if (IsDestPCS())`; the other five sites have no override.
Boolean AND is idempotent. The code is correct either way, but the comment
asserts a hazard that does not exist, and the qualification silently ignores any
*future* override that narrows without a mirrored call-site guard.

Fix it together with the related Minor: `CheckSrcAbs()`/`CheckDstAbs()` now make
a virtual `GetSrcSpace()`/`GetDstSpace()` call **once per pixel** whenever the
adjustment fires (`:1821`, `:1835`), and the answer is a `Begin()`-time
constant. Cache it in a member; keep `NeedAdjustPCS()` undisturbed, because
`CheckPCSConnections()`'s edge conditions still read it.

`stash@{0}` holds a partial attempt at exactly this — cache members and comments
added, the `Begin()` fill and predicate bodies not written. The header comment in
that stash describes behaviour the `.cpp` does not yet have, so it will not build
into a coherent state as-is. Redoing from scratch is cheap.

Minors: two copy-pasted "reciprocal of the from-side push" comments at
`IccCmm.cpp:2741-2743` and `:2759-2761` are false — they sit in the
bi-directional/sparse-matrix branch, which has no from-side push;
`IccCmm.cpp:3970` reimplements the in-scope file-static
`icSpectralPcsMatchesRange()` (`:2247-2267`); `:3969`'s `nSamples != nPortSamples`
clause is tautological at all nine call sites and should be labelled defensive;
`pcs-adjust-placement.cpp:826-831` misattributes the flipped pin's result to S7
when the operative reason is that no `CIccPcsXform` is built at a spectral edge
at all; `:1249` substitutes an `icCmmStatIdentityXform` assertion for the
spec's "returns the original spectrum" and should say so; and `IccCmm.cpp:1861`
and `:1888` keep legacy leading tabs on two rewritten lines.

### 2. Edge-gate widening — still open, see decision B above

Today a spectral chain **edge** no longer corrupts samples 0-2, but gets no
conversion either. Wrong became absent, not correct.

`CheckPCSConnections()` gates both edge blocks on `IsSpaceColorimetricPCS()`
([`IccCmm.cpp:9911`](../../../IccProfLib/IccCmm.cpp#L9911),
[`:9986`](../../../IccProfLib/IccCmm.cpp#L9986)), and a spectral xform's
`GetSrcSpace()`/`GetDstSpace()` returns the reduced spectral type, so
`ConnectFirst()`/`ConnectLast()` cannot run for a spectral port. Their spectral
handling is implemented and unit-tested but dormant, and pinned as dormant by
`pcs-adjust-placement.cpp:860` (`pcsXformCount() == 0`).

Widening to `IsSpacePCS()` would complete the ruling. It was deliberately not
done: it is a second behaviour change beyond what the spectral spec authorises,
and it makes two previously-unreachable functions live for a new port type —
which is precisely where both bugs above were hiding. Wants its own commit and
its own tests.

An incidental prerequisite is already in place: `ConnectFirst()`/`ConnectLast()`
used to count edge samples *after* `icGetColorSpaceType()` stripped the channel
count, reporting 0 for every spectral edge. Fixed in `21333dc5`, latent until the
gate widens.

### 3. Task 5 — DONE (`ea2ca3b2`, `9f53b34b`, `a60ea4e1`)

Task 5 of the original plan removes the in-`Apply()` machinery: 16 guarded
`CheckSrcAbs()`/`CheckDstAbs()` call sites, `m_bSrcPcsConversion` /
`m_bDstPcsConversion`, and `NeedAdjustSrcPCS()` / `NeedAdjustDstPCS()`.
`NeedAdjustPCS()` must survive — `CheckPCSConnections()` still uses it in both
edge conditions.

It was halted because the in-`Apply()` path was still genuinely live for spectral
ports. The spectral conversion has removed that dependency, so the precondition
in the plan's Task 5 block is now satisfiable. Re-verify before deleting rather
than assuming.

### 4. Integration — still open, see decision C above

Merge to `master` locally / push and open a PR / keep the branch. Not yet chosen.

## Things a cold reader needs

- **Build/test tree is `out/plan-tests`**, a Visual Studio multi-config tree.
  Every command needs `-C Debug` / `--config Debug`. Omitting it yields
  `Test not available without configuration` and `Not Run`, which is a false
  failure.
- **`out/vs2022-x64` cannot reconfigure on this machine** — vcpkg fails to fetch
  its baseline from GitHub. `out/plan-tests` was configured around it with
  `-DCMAKE_TOOLCHAIN_FILE= -DENABLE_TESTS=ON -DENABLE_TOOLS=OFF
  -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=OFF -DICC_USE_ZLIB=OFF`. That makes it a
  CMM-focused 79-test subset, not the full suite.
- **Three tests report `Not Run`** — `pawg-compression-paths`,
  `pawg-c5-cicp-optional`, `iccviz-writer-serialization`. Their binaries are not
  built in that config. Pre-existing; confirmed against a master baseline
  worktree at the same config with zero new failures.
- **`clang++` and Graphviz are absent**, so the sanitizer build never ran and
  doxygen ran with `HAVE_DOT=NO`. Residual risk, honestly recorded.
- **Tolerance discipline: 1e-5 relative, never bit-equality.** The adjustments
  under test are ~3.5e-3 or larger; re-associating the same affine math moves
  results ~1e-7. That gap is the oracle. Three assertions and one measurement
  were removed from this branch for being unable to fail — check every new
  assertion against that.
- **Two traps in the spectral white-point read.**
  `CIccTagNumArray::GetValues()`'s single-argument overload copies exactly one
  value (it defaults `nVectorSize = 1`), and a profile can disagree with itself
  about its sample count — `icGetSpaceSamples(spectralPCS)` versus
  `m_Header.spectralRange.steps`. `Testing/Display/LaserProjector.icc` is a
  tracked profile that disagrees.
- **`IsSpaceSpectralPCS` is two different functions.** The file-local
  `static __inline` at `IccCmm.cpp:99` tests five signatures after
  `icGetColorSpaceType()`; `IccSignatureUtils.h:108` tests a single `'spc '`
  signature. The latter's parameter is an exact `icColorSpaceSignature` match,
  so including that header would silently win overload resolution. This is why
  several predicates are defined out of line.
- **`CIccXformMpe`'s guard is correct as written.**
  `(m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent)` is
  `!(intent == absolute && tagIntent == absolute)` — skip only when using the
  absolute tag at absolute intent. It reads like an intended `&&` and is not;
  changing it would break the `bUseAbsTagAsRel` fallback.

## Decisions taken on the owner's behalf

Recorded so they can be reviewed and undone. The full trail with reasoning lives
in `.superpowers/sdd/2026-08-26-pcs-adjust-in-pcsxform/progress.md`, which is
gitignored — this is the durable copy.

| # | Decision | Cost if wrong |
|---|---|---|
| 1 | Task 5 deletes four `Set*PCSConversion()` calls, not "three pairs" | None; the compiler enforces it |
| 2 | Task 6 stopped claiming a clipping delta it cannot demonstrate — the XYZ `NegClip` is reachable only via an `IIccAdjustPCSXform` hint with `Scale > 1` | If reachable by a path missed, that path's negative XYZ is unpinned |
| 3 | `Testing/iccApplyNamedCmm.exe` accepted as a master baseline; every commit `96507a4..master` is CI-only | Measured deltas would conflate two changes; one `git log --stat` falsifies it |
| 4 | Work on a branch in the primary checkout, not a worktree | `git checkout master` restores the tree |
| 5 | `CIccXformNamedColor`'s predicates defined out of line, because of the `IsSpaceSpectralPCS` collision above | Two out-of-line definitions instead of inline |
| 6 | Added the spectral guard to `NeedsDstPcsAdjust()`; the plan had omitted it and `Apply()` applies it symmetrically | None identified |
| 7 | *Superseded* — a parked coverage gap was closed properly instead |
| 8 | Task 5 must pin the spectral case by test before deleting, and stop if live. It was live | One extra test if it had been unreachable |
| 9 | Promoted two Minor findings to a fix round after the `ConnectLast` bug raised their weight | One fix round |
| 10 | Task 7's fixture swapped for one that can actually move. **Partially wrong** — the accompanying instruction to relabel a case "unaffected by construction" was disproved by the implementer's probe | Corrected before it shipped |
| 11 | Task 8's task review folded into the whole-branch final review | One document gets one review pass instead of two |
| 12 | Two documentation residuals parked in the BPC delta doc: an intent-41 probe miscount that overclaims closure (8 lines, not 6), and two `IccApplyBPC.cpp` citations overshooting real function bounds | An analysis document ships with an overclaim of the kind this branch removed elsewhere; the count fix is arithmetic |
| 13 | SDD workspace kept rather than deleted, while decisions are open | A gitignored scratch directory persists |

Two of these were overturned by subagents presenting evidence — #7 and #10. Both
corrections were right.

## Loose end

`WEncConv.icc` (780 bytes, untracked, repo root) appeared during this session's
tool runs. Not attributable to a specific step and not in any commit; left in
place rather than deleted.
