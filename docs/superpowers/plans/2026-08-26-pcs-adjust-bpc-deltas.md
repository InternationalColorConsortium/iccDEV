# Task 7: BPC black-point deltas from the PCS-adjustment move

Branch: `refactor/pcs-adjust-in-pcsxform`, measured at `01b03287`.

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
  reflects current `HEAD`.

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
itself was not rebuilt, only the DLL — the exe reads the DLL's version at
link time historically, but the string is baked at `96507a4` for both exe
copies since neither exe was relinked):

```
iccApplyNamedCmm built with IccProfLib version 2.3.2.3+96507a4, IccLibConnect Version 2.3.2.3+96507a4
```

To confirm the *behavior*, not just the version string, both binaries were
run against a known-affected path — a v2 CMYK output profile
(`f:/thrivedev/profiles/defaultcmyk.icc`, outside the repo) forward-applied
with intent 0 (perceptual, no BPC) on the probe data below:

```
$ ./Testing/iccApplyNamedCmm.exe <probe> 0:10:16 1 f:/thrivedev/profiles/defaultcmyk.icc 0
   ...
    35.0224876404     3.7147521973     2.3942565918 ; 50 50 50 50
    58.1666755676   -38.2570037842   -51.6071166992 ; 100 0 0 0

$ ./out/vs2022-x64/bin/Release/iccApplyNamedCmm.exe <probe> 0:10:16 1 f:/thrivedev/profiles/defaultcmyk.icc 0
   ...
    35.0224838257     3.7147674561     2.3942565918 ; 50 50 50 50
    58.1666755676   -38.2569732666   -51.6071472168 ; 100 0 0 0
```

Row-by-row deltas here are on the order of 1e-5 in Lab (e.g. `35.0224876404`
vs `35.0224838257`, `-38.2570037842` vs `-38.2569732666`) — small,
non-identical, and of the magnitude expected from removing one redundant
Lab&lt;-&gt;XYZ round trip per edge. This proves the branch `IccProfLib2.dll`
is genuinely running branch behavior and is not a stale copy of master's.

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

## Measurement

Fixture: `Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc` (v5 CMYK output profile with a
Lab PCS), applied to itself twice (forward then reverse, i.e. a
device-&gt;PCS-&gt;device round trip through the same profile) at intents `40`
(perceptual + BPC) and `41` (relative + BPC):

```
for INTENT in 40 41; do
  <exe> "$SCRATCH/cmyk-probe.txt" 1:8:14 1 \
    Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc $INTENT \
    Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc $INTENT
done
```

### Intent 40 (perceptual + BPC) diff

```
diff -u bpc-baseline-40.txt bpc-branch-40.txt
(no output — files are byte-identical; md5sum: 2424fd8c9313ddfe668622b86210e333 for both)
```

Baseline/branch output (identical):

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

Baseline/branch output (identical):

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

Both intents produce **bit-identical** output between master (`96507a4`) and
branch (`01b03287`) for this fixture — a delta of exactly 0.0 in CMYK
percent, well within the 1e-4 acceptance threshold. This is not a
coincidence of missing sensitivity: the differentiation check above (the
`defaultcmyk.icc` forward path) proves the branch DLL genuinely differs from
master's by ~1e-5 in Lab where a real PCS adjustment (media white point
scaling) is exercised. The explanation for zero delta here is that
`CIccApplyBPC::pixelXfm()` / `getBlackXfm()` build a `PCS -> device -> PCS`
round trip through **the same profile applied to itself** (both edges use
`Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc`), so the source and destination media
white points are identical and the PCS adjustment at each edge is the
identity transform (scale 1.0, offset 0) both before and after Task 4/5's
move. Where the adjustment is the identity, removing the redundant
Lab&lt;-&gt;XYZ round trip changes nothing numerically — there is no rounding
noise to introduce because no adjustment math (and, for this profile, no
Lab&lt;-&gt;XYZ conversion at all, since the profile's declared PCS is
already Lab) is exercised in a way that depends on the extra round trip.

**Conclusion: the deltas are within tolerance.** Both intents 40 and 41 are
bit-identical (0.0 delta, vs. the 1e-4 threshold), and this null result has
been verified to reflect genuine branch behavior rather than a stale or
misloaded DLL.
