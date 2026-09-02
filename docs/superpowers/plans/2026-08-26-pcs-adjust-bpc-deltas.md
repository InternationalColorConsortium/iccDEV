# Task 7: BPC black-point deltas from the PCS-adjustment move

Branch: `refactor/pcs-adjust-in-pcsxform`, measured at `01b03287`.

**Revision note:** the first version of this document used
`Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc` as the only fixture and reported a
bit-identical (null) result. Review found that fixture's `getBlackXfm()`
chain could plausibly never engage the moved PCS-adjustment code at all,
which would make the null uninterpretable. This revision adds
`Testing/V2/v2CmykLut16.icc` (an in-repo v2.10 CMYK output profile) as the
primary fixture, and — before trusting any diff — proves with a temporary,
reverted runtime probe that the moved code genuinely executes with
non-identity numbers for both fixtures. The original `CMYK-3DLUTs.icc`
result is kept below as a secondary case, relabeled to match what the probe
actually showed (it is *not* a clean "never engages" case either).

## Baseline provenance

Compared two `iccApplyNamedCmm.exe` + `IccProfLib2.dll` pairs, each loading its
own DLL from its own directory:

- **Master baseline:** `Testing/iccApplyNamedCmm.exe` + `Testing/IccProfLib2.dll`,
  built at `96507a48` ("fix(bpc): estimate black through the tag family the
  xform actually uses (#2310)").
- **Branch:** `out/vs2022-x64/bin/Release/iccApplyNamedCmm.exe` +
  `out/vs2022-x64/bin/Release/IccProfLib2.dll`. The DLL was rebuilt from
  branch `HEAD` (`01b03287`) immediately before measuring:

  ```
  MB="G:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe"
  "$MB" out/vs2022-x64/IccProfLib/IccProfLib2.vcxproj //p:Configuration=Release //p:Platform=x64 //p:BuildProjectReferences=false //m //v:m //nologo
  ```

  The build reported the DLL as already up to date at that `HEAD` (mtime
  `2026-08-26 15:24:45`, a couple of minutes before this rebuild ran), i.e.
  no source changes since the prior rebuild — confirming the artifact
  reflects current `HEAD`. (The DLL was later rebuilt twice more, for the
  liveness probe and to restore a clean copy afterward — see "Liveness
  verification" below. The final measurements in this document were taken
  against a DLL built from an unmodified `IccProfLib/IccCmm.cpp` at the same
  `HEAD`, confirmed via `git status`/`git diff` showing no residual change.)

**Validity of the baseline (plan Ruling 3).** Every commit between `96507a4`
and `master` (`68b1b6f2`) touches only CI/tooling, not `IccProfLib/` or
`IccConnect/`:

```
$ git --no-pager log --stat 96507a4..master -- IccProfLib/ IccConnect/
(no output)
```

This prints nothing, so `96507a4` is a valid stand-in for current
`master` behavior with respect to `IccProfLib`.

**Confirmation the branch binary genuinely loads the rebuilt DLL** (not a
stale copy). Both exes report the same embedded version string (the exe
itself was not rebuilt, only the DLL — the version string is baked into the
exe at `96507a4` for both copies since neither exe was relinked), so the
banner alone cannot distinguish them:

```
iccApplyNamedCmm built with IccProfLib version 2.3.2.3+96507a4, IccLibConnect Version 2.3.2.3+96507a4
```

An earlier revision of this document tried to settle that with a
differentiation probe against a v2 CMYK profile living outside the repo at
a machine-local path. Review correctly flagged that as irreproducible —
nobody else could re-run it — and, separately, redundant: it exercised a
plain forward apply with no BPC flag, so it never entered
`IccApplyBPC.cpp` and could not speak to the code path this document is
actually about. It has been removed. As a same-shape in-repo substitute,
`Testing/V2/v2CmykLut16.icc` forward-applied at intent `0` (no BPC) was
tried and came back byte-identical between the two binaries — evidence
that a plain, non-BPC forward apply is not a reliable place to look for
this refactor's effect on this particular profile, not evidence that the
DLL is stale (the "Liveness verification" section below independently
rules out staleness for the actual code path under test, `IccApplyBPC.cpp`,
using only in-repo fixtures and no unverifiable claim). DLL liveness for
the code path this document reports on is established there, not here.

## Probe data

```
'CMYK' ; Data Format
icEncodePercent ; Encoding
  0.0   0.0   0.0   0.0
 10.0  20.0  30.0  40.0
 50.0  50.0  50.0  50.0
100.0   0.0   0.0   0.0
  0.0   0.0   0.0 100.0
```

## Liveness verification: proving `m_bAdjustPCS` actually fires inside `getBlackXfm()`/`pixelXfm()`

Static analysis of `CIccXform::Begin()`'s gating (`IccProfLib/IccCmm.cpp`,
around line 1557) is not sufficient on its own: it correctly shows the
*perceptual-legacy* branch (`m_nIntent == icPerceptual && (IsVersion2() ||
!HasPerceptualHandling())`) requires a v2 profile and a Perceptual-intent
edge, and it correctly shows `getBlackXfm()`'s second edge always forces
`icRelativeColorimetric`. But there is a second, independent gate — the
absolute-adjustment branch (`bNeedAbsAdjust`, driven by `m_bAbsToRel` /
`m_nTagIntent` tag-family fallback, unrelated to profile version) — and
static reading alone cannot show which of the two, if either, actually
fires for a given profile/intent without tracing tag-family selection at
runtime. Rather than trust either static story, this was checked directly.

**Method.** A single guarded diagnostic line was added temporarily to
`CIccXform::Begin()` in `IccProfLib/IccCmm.cpp`, immediately before its
final `return icCmmStatOk;`:

```cpp
if (getenv("ICC_DEBUG_ADJUSTPCS")) {
  fprintf(stderr, "ADJUSTPCS-PROBE bInput=%d intent=%d isV2=%d adjustPCS=%d scale=%.6f,%.6f,%.6f offset=%.6f,%.6f,%.6f\n",
          (int)m_bInput, (int)m_nIntent, (int)IsVersion2(), (int)m_bAdjustPCS,
          (double)m_PCSScale[0], (double)m_PCSScale[1], (double)m_PCSScale[2],
          (double)m_PCSOffset[0], (double)m_PCSOffset[1], (double)m_PCSOffset[2]);
}
```

The branch DLL was rebuilt with this line, run with `ICC_DEBUG_ADJUSTPCS=1`
against both fixtures at both intents, then `IccProfLib/IccCmm.cpp` was
reverted with `git checkout --` and the DLL rebuilt again from the clean
file. `git status`/`git diff` confirmed no residual source change before
any measurement in this document was recorded — **this document reflects no
source changes**, per the task's own requirement; the instrumentation was
build-and-discard, used only to decide whether the null result below is
interpretable.

**Line labels.** The format above has no call-site field, so the lines
below were attributed after the fact from `bInput`/`intent` and the
resulting `scale`/`offset` values, cross-checked against
`IccProfLib/IccApplyBPC.cpp`. Four call sites feed `CIccXform::Begin()` in
this configuration, each with a `(bInput, intent)` fingerprint:

| Label | Call site | `bInput` | `intent` |
|---|---|---|---|
| `A` | `calcSrcBlackPoint()`'s fixed-`icPerceptual` `pixelXfm()` (`IccApplyBPC.cpp:310`, PCS -> device) | 0 | 0 (always, hard-coded) |
| `B` | `calcSrcBlackPoint()`'s device -> PCS `pixelXfm()` at the xform's actual intent (`:356`) | 1 | the calling xform's own intent |
| `C` | `getBlackXfm()`'s first edge, PCS -> device at `nIntent` (`:611`) | 0 | `nIntent` passed to `getBlackXfm()` |
| `D` | `getBlackXfm()`'s second edge, device -> PCS, forced `icRelativeColorimetric` (`:626`) | 1 | 1 (always, hard-coded) |

`A` and `C` share a fingerprint whenever `nIntent` is `icPerceptual` (both
`(0, 0)`); `B` and `D` share one whenever the calling xform's intent is
`icRelativeColorimetric` (both `(1, 1)`). Where two call sites collide,
identical output values mean they hit the same `Begin()` branch (the
branch result is a pure function of `bInput`/`intent`/`IsVersion2()`, not
of which call site reached it) — this is corroborated below, not merely
assumed. A tagged re-run (add a `site=` field to the `fprintf`) would
remove the need for this reconstruction; that is the recommended fix if
this probe is ever repeated.

**Result for `Testing/V2/v2CmykLut16.icc`, intent 40** (six `Begin()` calls:
`calcSrcBlackPoint`'s fixed-Perceptual `pixelXfm()`, and `getBlackXfm()`'s
two edges, each run once for the profile used as source and once as
destination):

```
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440   # A or C (fingerprint collision; value matches the v2-perceptual-legacy branch)
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=1 adjustPCS=1 scale=0.996515,0.996527,0.996521 offset=0.001680,0.001737,0.001435   # B (unique fingerprint at this test intent)
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=1 adjustPCS=1 scale=1.000000,1.000000,1.000000 offset=0.000000,0.000000,0.000000   # same (bInput,intent) as the line above but a different value -> a second, distinct call reaching (1,0); not accounted for by the A/B/C/D model below (see note)
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440   # A or C (the other of the pair on line 1)
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=0.000000,1.000000,0.000000 offset=0.000000,0.000000,0.000000   # D (unique fingerprint: bInput=1, intent=1 forced-relative; adjustPCS=0 so scale/offset are unused here)
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=0.999082,0.999082,0.999082 offset=0.000442,0.000459,0.000379   # third (0,0) line with yet another distinct value -> not accounted for by the A/B/C/D model below (see note)
```

Only four call sites (`A`, `B`, `C`, `D`) are identified above, but six
lines are present: this block has one extra `(0,0)`-fingerprint line and
one extra `(1,0)`-fingerprint line beyond what `A`+`B`+`C`+`D` predict for
a Perceptual-intent run (`calcDstBlackPoint()`'s nested `calcSrcBlackPoint()`
call is gated on `nIntent==icRelativeColorimetric` at `IccApplyBPC.cpp:437`,
which is false here). The same surplus, in the same shape, recurs in the
secondary-fixture block below at the same intent, so it is a reproducible
pattern rather than fixture-specific noise — but this document cannot
pin down its exact source from static reading, exactly the situation
"Liveness verification" above says static reading cannot resolve. A tagged
re-run would settle it.

**Result for `Testing/V2/v2CmykLut16.icc`, intent 41:**

```
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440   # A (xform1's calcSrcBlackPoint; fixed-Perceptual, unique here since C now reports intent=1)
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # B, B' or D (three-way fingerprint collision at (1,1); adjustPCS=0 so unused
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=1 scale=0.996527,0.996527,0.996527 offset=0.001674,0.001737,0.001433   # B, B' or D -- non-identity v2-perceptual-legacy-shaped value
ADJUSTPCS-PROBE bInput=0 intent=1 isV2=1 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # C (unique fingerprint: bInput=0, intent=1 -- getBlackXfm's first edge at this test's relative intent)
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # B, B' or D
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440   # A (the second calcSrcBlackPoint invocation -- see note)
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=1.003497,1.003485,1.003491 offset=0.000000,0.000000,0.000000   # B, B' or D
ADJUSTPCS-PROBE bInput=0 intent=1 isV2=1 adjustPCS=1 scale=1.003485,1.003485,1.003485 offset=-0.001680,-0.001743,-0.001437   # C
```

Unlike the intent-40 block, this one is fully explained: at relative
intent, `calcDstBlackPoint()`'s nested `calcSrcBlackPoint()` call (gated on
`nIntent==icRelativeColorimetric`, true here) fires a *second*
`calcSrcBlackPoint` invocation (`A`, `B` again), on top of the xform1
invocation and `getBlackXfm()`'s two edges. The fingerprint multiset is
`(0,0)` x2 (`A` twice), `(1,1)` x3 (`B` twice plus `D` once), `(0,1)` x1
(`C` once) = six lines, matching exactly. `B`, the repeated `B`, and `D`
cannot be told apart by fingerprint alone -- only by value, and here two of
the three collapse to the identical zero/identity result, which does not
distinguish them either; this is the concrete case the recommended `site=`
tag would resolve.

**Conclusion of the probe:** `adjustPCS=1` fires repeatedly for
`v2CmykLut16.icc`, at both intents, with real non-identity scale/offset
(e.g. `scale=0.996515,0.996527,0.996521 offset=0.001680,0.001737,0.001435`
— exactly the v2-perceptual-legacy black-point scale/offset, and, for the
intent-41 case, the same numbers reappearing via the *absolute*-adjustment
branch instead). The moved code is unambiguously live for this fixture at
both intents. This directly satisfies the requirement to prove the path is
live before trusting a null result, and it is the load-bearing liveness
evidence for this document: it runs through `CIccApplyBPC.cpp` itself,
which the removed external differentiation probe never did.

**A correction to the original review's static claim, made for the
secondary fixture below:** the same probe was also run against
`Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc` at intent 40. The review's static
argument — that `IsVersion2()` is false for this v5 profile and
`HasPerceptualHandling()` defaults true for LUT xforms, so the
perceptual-legacy branch can never fire — is correct as far as it goes. But
it is not the only gate: two of the six `Begin()` calls for this fixture
*did* show `adjustPCS=1` with real values, via the independent
absolute-adjustment branch:

```
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # A or C fingerprint (0,0); adjustPCS=0 here, unlike the v2 fixture, since IsVersion2()=0 closes the perceptual-legacy branch
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # B fingerprint (1,0); adjustPCS=0
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=0 adjustPCS=1 scale=1.001158,1.001158,1.001158 offset=-0.000558,-0.000579,-0.000477   # second (1,0) line, non-identity via the absolute-adjustment branch (bNeedAbsAdjust) -- same unexplained-surplus shape as the primary fixture's intent-40 block
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # A or C fingerprint (0,0) (the other of the pair on line 1)
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000   # D fingerprint (1,1), forced-relative edge; adjustPCS=0
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=0 adjustPCS=1 scale=0.999437,0.999437,0.999437 offset=0.000272,0.000282,0.000232   # third (0,0) line, non-identity via the absolute-adjustment branch -- same surplus shape again
```

The fingerprint multiset here is identical in shape to the primary
fixture's intent-40 block: `(0,0)` x3, `(1,0)` x2, `(1,1)` x1. The two
"extra" lines beyond the four-call-site model are exactly where the
absolute-adjustment branch (`bNeedAbsAdjust`) shows real, non-identity
scale/offset, which is the point of this secondary measurement — but their
exact call-site attribution has the same open question noted above.

So `CMYK-3DLUTs.icc`'s round trip is **not** structurally guaranteed to
skip the moved code either — the perceptual-legacy branch is closed for it,
but the absolute-adjustment branch is genuinely open on 2 of 6 edges, with
non-trivial scale/offset (~1.0e-3 magnitude). It is retained below as a
secondary case, relabeled to say exactly that, rather than the "unaffected
by construction" label originally proposed.

## Primary measurement: `Testing/V2/v2CmykLut16.icc`

In-repo v2.10 CMYK Output profile (`prtr`/`CMYK`/`Lab` PCS, `lut16Type`
`AToB0/1/2` + `BToA0/1/2` tags — generated from the tracked
`Testing/V2/v2CmykLut16.xml` by `CreateAllProfiles.sh`), applied to itself
twice (forward then reverse) at intents `40` (perceptual + BPC) and `41`
(relative + BPC):

```
for INTENT in 40 41; do
  <exe> "$SCRATCH/cmyk-probe.txt" 1:8:14 1 \
    Testing/V2/v2CmykLut16.icc $INTENT \
    Testing/V2/v2CmykLut16.icc $INTENT
done
```

### Intent 40 (perceptual + BPC) diff

```
diff -u bpc2-baseline-40.txt bpc2-branch-40.txt
(no output — files are byte-identical)
```

Baseline/branch output (identical):

```
'CMYK'	; Data Format
icEncodePercent	; Encoding

;Source Data Format: 'CMYK'
;Source Data Encoding: icEncodePercent
;Source data is after semicolon

;Profiles applied
; Testing/V2/v2CmykLut16.icc
; Testing/V2/v2CmykLut16.icc

    50.00000000    50.00136948    50.00152588     0.00036955	;     0.00000000     0.00000000     0.00000000     0.00000000
    47.27932358    51.25760269    54.18593979    46.88261795	;    10.00000000    20.00000000    30.00000000    40.00000000
    50.00000000    50.00137711    50.00151825    66.81841278	;    50.00000000    50.00000000    50.00000000    50.00000000
    84.92651367    22.08301735    15.03691959    29.96361923	;   100.00000000     0.00000000     0.00000000     0.00000000
    50.00000000    50.00136948    50.00152588    99.17430878	;     0.00000000     0.00000000     0.00000000   100.00000000
```

### Intent 41 (relative + BPC) diff

```
diff -u bpc2-baseline-41.txt bpc2-branch-41.txt
(no output — files are byte-identical)
```

Baseline/branch output (identical):

```
'CMYK'	; Data Format
icEncodePercent	; Encoding

;Source Data Format: 'CMYK'
;Source Data Encoding: icEncodePercent
;Source data is after semicolon

;Profiles applied
; Testing/V2/v2CmykLut16.icc
; Testing/V2/v2CmykLut16.icc

    50.00000000    50.00136948    50.00152588     0.00000000	;     0.00000000     0.00000000     0.00000000     0.00000000
    47.27023315    51.26105499    54.20110703    46.96060944	;    10.00000000    20.00000000    30.00000000    40.00000000
    50.00000000    50.00136948    50.00152588    66.99981689	;    50.00000000    50.00000000    50.00000000    50.00000000
    85.00061035    22.00045967    14.99984837    30.00122261	;   100.00000000     0.00000000     0.00000000     0.00000000
    50.00000000    50.00136948    50.00152588   100.00000000	;     0.00000000     0.00000000     0.00000000   100.00000000
```

Both diffs were re-confirmed against the clean (post-revert) rebuild of the
branch DLL, not just the instrumented one, to rule out the instrumentation
itself having changed anything.

## Secondary measurement (retained for contrast): `Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc`

Kept from the original version of this document. As shown above, this
fixture's round trip does exercise `m_bAdjustPCS` on 2 of 6 `Begin()` calls
(via the absolute-adjustment branch, with real ~1.0e-3-magnitude scale and
offset), so — unlike the initial write-up claimed — this is not a case that
structurally cannot engage the moved code. It is retained as a second data
point showing the same live-but-numerically-inert pattern as the primary
fixture, under a different adjustment branch.

### Intent 40 (perceptual + BPC) diff

```
diff -u bpc-baseline-40.txt bpc-branch-40.txt
(no output — files are byte-identical; md5sum: 2424fd8c9313ddfe668622b86210e333 for both)
```

```
'CMYK'	; Data Format
icEncodePercent	; Encoding

;Source Data Format: 'CMYK'
;Source Data Encoding: icEncodePercent
;Source data is after semicolon

;Profiles applied
; Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc
; Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc

     0.00000000     0.00000000     0.00085811     0.00000000	;     0.00000000     0.00000000     0.00000000     0.00000000
    21.61770821    33.81699753    45.40148544    36.55809021	;    10.00000000    20.00000000    30.00000000    40.00000000
    35.80407715    35.34764862    31.46599007    75.79375458	;    50.00000000    50.00000000    50.00000000    50.00000000
    89.47635651     2.30753899     0.75326627     0.75531989	;   100.00000000     0.00000000     0.00000000     0.00000000
    40.38120270    38.73536682    42.48589706    90.37289429	;     0.00000000     0.00000000     0.00000000   100.00000000
```

### Intent 41 (relative + BPC) diff

```
diff -u bpc-baseline-41.txt bpc-branch-41.txt
(no output — files are byte-identical; md5sum: 4c140f12259be43a2a3b6f0557ea8ef3 for both)
```

```
'CMYK'	; Data Format
icEncodePercent	; Encoding

;Source Data Format: 'CMYK'
;Source Data Encoding: icEncodePercent
;Source data is after semicolon

;Profiles applied
; Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc
; Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc

     0.00000000     0.00000000     0.00106182     0.00000000	;     0.00000000     0.00000000     0.00000000     0.00000000
    22.92844582    32.72074890    43.62459946    28.27006531	;    10.00000000    20.00000000    30.00000000    40.00000000
    35.15167999    35.72314835    32.20074081    69.59024811	;    50.00000000    50.00000000    50.00000000    50.00000000
   100.00000000     0.00000000     0.00000000     0.76256406	;   100.00000000     0.00000000     0.00000000     0.00000000
    39.16203690    36.87323761    41.30582809    87.96249390	;     0.00000000     0.00000000     0.00000000   100.00000000
```

## Classification

All four measured combinations (two fixtures × two intents) produce
**bit-identical** output between master (`96507a4`) and branch (`01b03287`)
— a delta of exactly 0.0 in CMYK percent against every probe row, well
within the 1e-4 acceptance threshold. Unlike the first pass at this task,
this null result is not a "the code never ran" artifact: the liveness
probe above shows `m_bAdjustPCS` firing with genuine non-identity
scale/offset on multiple edges inside `CIccApplyBPC::pixelXfm()` /
`getBlackXfm()` for both fixtures, at both intents.

There is a stronger, structural explanation, not just the downstream-coarseness
argument below, and it should be stated first: the measured chain itself —
`CMYK -> Lab -> CMYK`, two device-class profiles round-tripped through an
interior PCS connection — has **no PCS edge at all**. Both of the outer
CMM's boundaries (`GetSourceSpace()`/`GetDestSpace()`) are `icSigCmykData`;
the PCS only appears at the *interior* connection between the two xforms,
and interior connections were already routed through `CIccPcsXform::Connect()`
before this branch — that code path is unchanged by this branch (Task 4
only moved the *leading/trailing edge* adjustment into `CIccPcsXform`;
see `docs/pcs-adjustment-placement.md`). So the outer chain this document
measures is a configuration this branch cannot touch, by construction, and
a byte-identical result for it is not a coincidence needing an explanation
at all.

The only places in this measurement where a genuine chain edge exists are
`CIccApplyBPC`'s own internal probe chains, confirmed against
`IccProfLib/IccApplyBPC.cpp`: `pixelXfm()` builds a single-xform
`CIccCmm(SrcSpace, icSigUnknownData, ...)` that is PCS on one side and
device on the other (an edge), and `getBlackXfm()` builds a two-xform
`PCS -> device -> PCS` round trip (an edge at *both* ends) — exactly the
shape this branch's Task 4 changes. The liveness probe above shows real,
non-identity scale/offset moving through those edges. But their output
feeds a black-point *estimation*, not the pixel path directly:
`calcSrcBlackPoint()` (`IccApplyBPC.cpp:368-370`) explicitly clips the
estimated L* to 50 before converting back to XYZ, and both
`calcSrcBlackPoint()`/`calcDstBlackPoint()` route the probe pixel through
16-bit `lut16Type` (`v2CmykLut16.icc`) or CLUT (`CMYK-3DLUTs.icc`)
tetrahedral interpolation twice (once per edge of the round trip) before
the estimate is used at all. An L*-clip and two rounds of 16-bit LUT
quantization are far coarser than a ~1e-7-relative re-association delta,
so a perturbation at the edge has a concrete, named mechanism for being
absorbed before it ever reaches an 8-decimal-print-precision comparison —
this is a positive account of why the null is expected, not merely a
tolerance argument. This is a substantively different, and
more defensible, claim than the original document's "the adjustment was
identity, so there was nothing to perturb" — that explanation is now known
to be wrong (the probe shows non-identity adjustment on both fixtures) and
has been dropped.

**Conclusion: the measured deltas are within tolerance.** All four
combinations are bit-identical (0.0 delta, vs. the 1e-4 threshold), and —
unlike the first pass — this null result has been verified against a live,
non-identity execution of the moved code, not merely against a
non-stale DLL running an unrelated code path.

---

# Task 8: Full sweep and handoff

Branch: `refactor/pcs-adjust-in-pcsxform`, swept at `fd1b1830`, branched from
`master` at `68b1b6f2`. No source changes were made during this sweep; the
tree at the end of it is identical to the tree at the start, modulo one
stray test-artifact file (`WEncConv.icc`, written to the repo root by a
ctest run whose `WORKING_DIRECTORY` is the repo root) that was deleted as
cleanup and was never tracked.

## Corrected contract matrix

The brief's Step 4 matrix (in `task-8-brief.md`) describes several rows as
though Task 5's deletions had happened. **They have not.** Task 5 is
halted, pending a decision from the repository owner (`task-5-report.md`,
and the "Known gap" section of `docs/pcs-adjustment-placement.md`), because
it found the in-`Apply()` PCS-adjustment path is still genuinely live for a
v5 spectral-PCS profile at a chain edge under absolute colorimetric intent —
proven by execution (`spectralTrailingEdgeStillAdjustsInsideApply()` in
`.github/ci/regression/pcs-adjust-placement.cpp`), not by static reading.

**Updated after Task 5.** The rows below were re-verified against the tree at
`ea2ca3b2`, which retired the in-xform adjustment path. The three rows that
described that path as live have been rewritten; the spectral-edge behaviour
they cited had already changed under S5 of the spectral white point work, and
the deletion then removed the path itself. Line numbers are current as of
`ea2ca3b2`.

| Surface | Producer | Consumer | Behavior change |
|---|---|---|---|
| `CIccXform::NeedsSrcPcsAdjust()` / `NeedsDstPcsAdjust()` | new virtuals on `CIccXform` (`IccProfLib/IccCmm.h:524,529`) | `CIccPcsXform::Connect()` / `ConnectFirst()` / `ConnectLast()` (e.g. `IccProfLib/IccCmm.cpp:2349,2356,2831`) | New API, in tree as designed. Overridden in `CIccXformMpe` (`IccCmm.h:1619-1629`) and `CIccXformNamedColor` (`IccCmm.cpp:8009,8025`, both excluding the spectral case). |
| `SetSrcPCSConversion()` / `SetDstPCSConversion()` | **Removed** (Task 5, `ea2ca3b2`), along with the `m_bSrcPcsConversion`/`m_bDstPcsConversion` members they set | none -- the four `CheckPCSConnections()` call sites went with them | Removed public API. They existed only to suppress the in-`Apply()` adjustment once a `CIccPcsXform` had taken it over; with no in-`Apply()` path left there is nothing to suppress. An out-of-tree caller of either setter no longer compiles. |
| `CheckSrcAbs()` / `CheckDstAbs()` / `AdjustPCS()` | **Deleted outright**, along with the `m_AbsLab` scratch buffer only `CheckSrcAbs()` used. All 16 guarded call sites were deleted in Task 5 (`ea2ca3b2`); the three declarations/definitions and `m_AbsLab` followed once the repository owner ruled against the retain-and-deprecate ruling below | none -- there is no caller left in `IccProfLib`, and an out-of-tree subclass that still called them no longer compiles | The repository owner overturned the "retained, deprecated, third-party subclasses only" decision this row previously recorded: keeping the functions callable turned a compile error at the caller's own line into a silent double-application, since `CIccPcsXform` performs the adjustment at the connection unconditionally and the guard flags that used to suppress a second application were already gone. Deleting them restores the loud failure. The spectral-edge case an earlier version of this row cited had already stopped firing under S5 of the spectral white point work (the base predicates answer false at a spectral port); that is unaffected by this deletion. **One reachable exception remains a behaviour change from before Task 5, unaffected by this deletion** -- an MCS destination port fed by an `IIccAdjustPCSXform` hint; see `pcsAdjustHintReachesANonPcsPort()` and `task-5-report.md` §6. |
| `bUsePCSConversions` | **Retained but now genuinely ignored** — still a parameter of `CIccCmm::Begin()` / `CIccNamedColorCmm::Begin()` / `CheckPCSConnections()`, explicitly `(void)`-discarded in the last of these | nothing | Task 5 (`ea2ca3b2`) deleted the `!bUsePCSConversions &&` term from the interior condition, so the interior `CIccPcsXform` is built unconditionally for colorimetric PCS links. This was the one place where "ignored" previously overstated it, and it was also the one place where deleting the in-`Apply()` path would otherwise have changed behaviour. No in-tree caller ever passed `true`; an out-of-tree caller that did now gets the `CIccPcsXform` path instead. |
| XYZ-PCS negative clamp (`CIccPCSUtil::NegClip`, formerly inside `AdjustPCS()`) | Removed from the new `CIccPcsStep` affine chain (`docs/pcs-adjustment-placement.md`, "What moved numerically") | any XYZ-PCS chain whose adjustment is folded into a `CIccPcsXform` | Negative XYZ is no longer forced to zero for adjustments that reach a `CIccPcsXform`. Unreachable from any built-in adjustment (absolute-intent uses zero offset/positive scale; v2-perceptual uses a positive offset) — only an `IIccAdjustPCSXform` hint with `Scale > 1` (`CIccApplyBPC`) can drive a non-negative input negative. Pinned by `xyzPcsChainStillAdjusts()`, which confirms the adjustment survives the move but, by the brief's own scope correction (Task 6 report), cannot itself demonstrate the clip delta since no in-tree fixture drives a negative offset. |
| Task 5's deletions (16 call sites above, the two `m_b{Src,Dst}PcsConversion` flags, `NeedAdjustSrcPCS()`/`NeedAdjustDstPCS()`, and documenting `bUsePCSConversions` as ignored) | — | — | **Performed**, in `ea2ca3b2`, after its spectral precondition was discharged by the spectral white point work (`21333dc5`, `1516fcce`, `769a34b3`, `0b8b5e9e`). `NeedAdjustSrcPCS()`/`NeedAdjustDstPCS()` are gone; `NeedAdjustPCS()` survives because `CheckPCSConnections()` still uses it in both edge conditions. Reachability was re-established by instrumenting the adjustment and running the whole suite (0 firings; 64 with the handover disabled, as a control) plus a port-by-port static argument -- see `task-5-report.md`. That work found **one reachable port the deletion does change**: an MCS destination fed by an `IIccAdjustPCSXform` hint, recorded in `task-5-report.md` §6 and pinned by `pcsAdjustHintReachesANonPcsPort()`. |

## Step 1 — full CTest run vs. `master` baseline

**Branch** (`out/plan-tests`, Debug config, tools/IccXML/IccJSON/zlib
disabled because vcpkg cannot fetch its baseline on this machine):

```
cmake --build out/plan-tests --config Debug -j        # succeeded
ctest --test-dir out/plan-tests -C Debug --output-on-failure --no-tests=error
96% tests passed, 3 tests failed out of 79
```

- 79 tests registered. This is a CMM-focused subset, **not the full test
  suite**.
- 3 report `Not Run`: `iccdev.pawg-compression-paths`,
  `iccdev.pawg-c5-cicp-optional`, `iccdev.iccviz-writer-serialization`.
  Verified directly (not taken on the brief's word) that their executables
  genuinely do not exist in this build tree — ctest's own output says
  `Could not find executable .../iccPawgCompressionPathsTest.exe` etc. These
  are configuration artefacts of building with `ENABLE_TOOLS=OFF` (the
  `iccviz-writer-serialization` binary needs `Tools/CmdLine/.../IccCmdLineUtil.h`,
  which isn't available without the tools build), not regressions.
- 1 reports `Skipped`: `iccdev.fileio-reopen-nonregular` — pre-existing,
  expected in this configuration.
- 0 tests report `(Failed)`.

**Master baseline** (fresh worktree at `../iccdev-master-baseline`, `master`
@ `68b1b6f2`, configured with the corrected flags from the task-8 brief
rather than the brief's original Step 1 command, which does not match this
tree's actual configuration):

```
git worktree add ../iccdev-master-baseline master
cmake -S ../iccdev-master-baseline/Build/Cmake -B ../iccdev-master-baseline/out/baseline \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE= \
  -DENABLE_TESTS=ON -DENABLE_TOOLS=OFF -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=OFF -DICC_USE_ZLIB=OFF
cmake --build ../iccdev-master-baseline/out/baseline --config Debug --target build-test-binaries
ctest --test-dir ../iccdev-master-baseline/out/baseline -C Debug --no-tests=error
96% tests passed, 3 tests failed out of 78
```

- 78 tests registered (one fewer than the branch — the branch added test
  coverage across Tasks 5–7, e.g. new functions in
  `pcs-adjust-placement.cpp`).
- The same 3 tests report `Not Run`, for the same reason (same missing
  executables; `iccViz WriterSerializationTest` fails the same
  `IccCmdLineUtil.h` include on `master` too).
- 2 report `Skipped`: `iccdev.fileio-reopen-nonregular` (matches the
  branch) plus `iccdev.embedded-profile-onelevel-load`, which the branch run
  did *not* skip. Both trees register this test identically
  (`SKIP_RETURN_CODE 77`); the skip is a runtime self-skip inside the test
  binary (its own return code), not a difference in how the two trees were
  configured or built. Recorded here as an observed environment-dependent
  flake, not a regression — it went from "runs and passes" to "self-skips,"
  never to "fails," and only on the baseline run.
- 0 tests report `(Failed)`.

**Diff.** Extracting `(Failed)` lines from each run's output and comparing:

```
comm -13 fails-baseline.txt fails-branch.txt
(empty)
```

Both sets are empty — neither run has a single test reporting `(Failed)`.
**No regression was introduced by this branch.** The only non-`Passed`
outcomes on either side are the three configuration-artefact `Not Run`
tests (identical on both trees) and the pre-existing/flaky `Skipped` tests
discussed above.

Cleanup: `git worktree remove --force ../iccdev-master-baseline` was run;
`git worktree list` confirms it is gone.

## Step 2 — sanitizer build

**Not run.** `clang++` is not installed on this machine (`which clang++`
fails against the full `PATH`). The brief's Step 2 command
(`CC=clang CXX=clang++ cmake ... -DENABLE_ASAN=ON -DENABLE_UBSAN=ON`) cannot
configure without it. No substitute (e.g. MSVC `/fsanitize=address`) was run
in its place and none is claimed as equivalent — MSVC ASan and
clang ASan+UBSan do not cover the same ground, and the brief's own rationale
for this step ("ASan confirms nothing else read" the two conversion-flag
bools) is in any case moot for this sweep: Task 5 never deleted those bools,
so there is nothing to sanitizer-check with respect to that deletion here.
**This step is unverified in this environment.**

## Step 3 — Doxygen check

`doxygen .github/ci/doxygen/Doxyfile` was run. The Doxyfile has
`HAVE_DOT = YES`, but Graphviz's `dot` is not installed on this machine
(`which dot` fails), which made the unmodified command fail mid-run trying
to render call/collaboration graphs and never reach the point of writing
`docs/generated/doxygen-warnings.log`. To get a real answer on the one thing
this step actually needs to check — whether the new/changed Markdown under
`docs/` introduces a broken or out-of-INPUT-tree link — the same Doxyfile
was re-run once, piped through stdin with `HAVE_DOT=NO` appended as a
config override (a diagnostic-only invocation; nothing under
`.github/ci/doxygen/` was edited):

```
(cat .github/ci/doxygen/Doxyfile; echo "HAVE_DOT=NO") | doxygen -
```

Result: `docs/generated/doxygen-warnings.log` contained 8 lines, none of
them about `docs/pcs-adjustment-placement.md`,
`docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md`, or any other
file touched by this branch:

- 1 warning that `/usr/include` (an `INCLUDE_PATH` entry) isn't readable —
  pre-existing, Windows-vs-Doxyfile-default noise, unrelated to this branch.
- 4 `ignoring \dotfile command because HAVE_DOT is not set` warnings, all in
  `docs/iccapply/*.dox` — an artefact of this diagnostic run's own
  `HAVE_DOT=NO` override; they would not appear with Graphviz installed and
  the Doxyfile's real `HAVE_DOT=YES`, and none reference this branch's docs.
- 3 `explicit link request to 'operator=' could not be resolved` warnings in
  `IccProfLib/IccTagBasic.cpp` — pre-existing, in a file this branch does
  not touch.

Confirmed `docs/` is in `INPUT` and that the new/changed files were
genuinely scanned, not silently skipped: `docs/generated/html/` contains
rendered pages for `pcs-adjustment-placement_8md.html`,
`2026-08-26-pcs-adjust-bpc-deltas_8md.html`,
`2026-08-26-pcs-adjust-in-pcsxform_8md.html`, and
`2026-08-26-pcs-adjust-in-pcsxform-spec_8md.html`. None of the 8 warnings
above are attributed to any of them, so the check the brief cares about —
no Markdown link pointing outside the Doxygen INPUT tree — passes.

**What this does not verify:** the *unmodified* Doxyfile with real
`HAVE_DOT=YES` and Graphviz present, which is what CI actually runs. Missing
Graphviz on this machine is an environment gap, the same category as the
missing `clang++` — it is not something this task's "no source changes"
scope extends to fixing. `docs/generated/` was deleted after both doxygen
runs; nothing under it was committed.

## What this sweep did and did not cover on this machine

**Covered:** a full build and ctest run of the `out/plan-tests` CMM-focused
subset (79 tests) with a `(Failed)`-set diff against a fresh `master`
baseline worktree (78 tests) showing zero new failures; a Doxygen run
confirming this branch's documentation changes introduce no warnings and
are genuinely inside the INPUT tree; a direct `grep`-verified re-check of
every symbol named in the brief's Step 4 matrix against the tree at
`fd1b1830`, which is what produced the corrected matrix above.

**Not covered:** the sanitizer build (Step 2) — no `clang++` on this
machine, not run, not substituted. The Doxygen check was run against a
temporarily-modified copy of the config (`HAVE_DOT=NO`, never written to
disk under `.github/ci/doxygen/`) because Graphviz is absent here; the
warning-count result is trustworthy for the link-safety question Step 3
exists to answer, but the exact warning set under the real, unmodified
CI configuration was not reproduced locally. The test run itself is a
CMM-focused subset (tools/IccXML/IccJSON/zlib disabled), not the full
suite — CI's full-feature configuration was not exercised here.

No source file under `IccProfLib/`, `IccConnect/`, or elsewhere was
modified during this sweep. The only filesystem change made and then
undone was the temporary `docs/generated/` Doxygen output (deleted) and one
stray test-artifact file, `WEncConv.icc`, written to the repo root by a
test's own working directory during the ctest run (deleted; it was never
tracked and is not part of this branch's deliverable).
