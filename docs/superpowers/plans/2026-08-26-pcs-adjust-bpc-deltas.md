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

**Result for `Testing/V2/v2CmykLut16.icc`, intent 40** (six `Begin()` calls:
`calcSrcBlackPoint`'s fixed-Perceptual `pixelXfm()`, and `getBlackXfm()`'s
two edges, each run once for the profile used as source and once as
destination):

```
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=1 adjustPCS=1 scale=0.996515,0.996527,0.996521 offset=0.001680,0.001737,0.001435
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=1 adjustPCS=1 scale=1.000000,1.000000,1.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=0.000000,1.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=0.999082,0.999082,0.999082 offset=0.000442,0.000459,0.000379
```

**Result for `Testing/V2/v2CmykLut16.icc`, intent 41:**

```
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=1 scale=0.996527,0.996527,0.996527 offset=0.001674,0.001737,0.001433
ADJUSTPCS-PROBE bInput=0 intent=1 isV2=1 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=1 adjustPCS=1 scale=1.003497,1.003485,1.003491 offset=-0.001686,-0.001743,-0.001440
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=1 adjustPCS=0 scale=1.003497,1.003485,1.003491 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=0 intent=1 isV2=1 adjustPCS=1 scale=1.003485,1.003485,1.003485 offset=-0.001680,-0.001743,-0.001437
```

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
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=1 intent=0 isV2=0 adjustPCS=1 scale=1.001158,1.001158,1.001158 offset=-0.000558,-0.000579,-0.000477
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=1 intent=1 isV2=0 adjustPCS=0 scale=0.000000,0.000000,0.000000 offset=0.000000,0.000000,0.000000
ADJUSTPCS-PROBE bInput=0 intent=0 isV2=0 adjustPCS=1 scale=0.999437,0.999437,0.999437 offset=0.000272,0.000282,0.000232
```

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

The correct mechanism, to the extent this measurement can establish it, is
about *where the perturbation goes*, not *whether it exists*. Inside
`CIccApplyBPC`, the adjusted PCS value (real, non-identity scale/offset —
see the liveness probe above) feeds a black-point *estimation* procedure —
`calcSrcBlackPoint()` / `calcDstBlackPoint()` explicitly clip the estimated
L* to 50 and route through 16-bit `lut16Type` (`v2CmykLut16.icc`) or CLUT
(`CMYK-3DLUTs.icc`) tetrahedral interpolation twice (forward and reverse)
before the estimate is used at all. Both of those steps (the clip and the
LUT interpolation's own discretization) sit downstream of the relocated
adjustment. This measurement cannot establish the exact size of whatever
sub-print-precision perturbation the relocation introduces at the point
the adjustment is applied, only that the clip and double LUT interpolation
downstream of it are coarser operations that a small perturbation could
plausibly be absorbed by — consistent with, though not proof of, why it
does not surface in the final black-point estimate or the final
round-tripped CMYK values at 8-decimal print precision for these
particular profiles and probe rows. This is a substantively different, and
more defensible, claim than the original document's "the adjustment was
identity, so there was nothing to perturb" — that explanation is now known
to be wrong (the probe shows non-identity adjustment on both fixtures) and
has been dropped.

**Conclusion: the measured deltas are within tolerance.** All four
combinations are bit-identical (0.0 delta, vs. the 1e-4 threshold), and —
unlike the first pass — this null result has been verified against a live,
non-identity execution of the moved code, not merely against a
non-stale DLL running an unrelated code path.
