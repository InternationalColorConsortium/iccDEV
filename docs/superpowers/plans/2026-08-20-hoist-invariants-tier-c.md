# Tier C: Hoist Pre-Existing Per-Pixel Invariants Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove eight pieces of per-pixel work that `Begin()` can do once, none of which came from the hardening effort, verifying each against the harness for both speed and numerical equivalence.

**Architecture:** Each finding moves invariant computation out of an `Apply()` and into the `Begin()` of the object that owns it. Placement follows one rule: state that is immutable after `Begin()` lives on the shared object (`CIccXform`, `CIccPcsStep`, `CIccTag`); state written during `Apply()` lives in the per-apply object allocated by `GetNewApply()`. The harness from `bench/throughput-harness` is the acceptance test — a checksum that moves without a documented reason is a failure, not a result.

**Tech Stack:** C++17, IccProfLib, `iccBenchApply` from the parent branch.

**Spec:** `docs/superpowers/specs/2026-08-20-apply-throughput-harness-design.md` (sections "Branch plan" and "Conventions binding branches 2 and 3")

## Global Constraints

- **Branch:** `perf/hoist-invariants`, stacked on `bench/throughput-harness`. Do not rebase onto master until #2207 merges.
- **Every removed or moved check leaves a comment** that lets the next reader re-derive the argument, naming where the condition is now decided. Never "removed for perf".
- **Placement by mutability.** Immutable after `Begin()` → shared object. Written during `Apply()` → per-apply object from `GetNewApply()`. When in doubt, ask whether two threads applying concurrently could both write it; if yes, it is per-apply.
- **All Begin-time precomputation must be idempotent.** `CIccCmm::Begin()` returns early when `m_pApply` is set (the contract `e4e05c3a` restored for #1940), but `CIccXform::Begin()` itself can be reached more than once through other paths. The harness checks this on `lut-3d-tetra`.
- **Checksums must not move**, with exactly one authorised exception: Task 6 (C6) deliberately changes `+inf` handling on the `NoClip` path. Every other task must reproduce the baseline checksums bit-identically.
- **No behaviour change is in scope beyond Task 6.** If a hoist appears to require one, stop and report it.

## Baseline

Captured on this branch at 1M pixels, 9 repeats, median, single thread. Every task compares against these.

| case | Mpx/s | checksum |
|---|---|---|
| `matrix-trc` | 7.504 | `0x2f8ddb88` |
| `lut-3d-tetra` | 13.021 | `0x03759972` |
| `spectral-6ch` | 3.883 | `0x469230ed` |
| `monochrome` | 29.213 | `0x54ce851f` |
| `mpe-calc` | 0.140 | `0xe275c18d` |
| `mpe-tonemap` | 0.604 | `0xdc6eb214` |
| `pcs-rel` | 3.419 | `0x2416f560` |
| `pcs-abs` | 3.466 | `0xbf684583` |

Re-capture with:

```bash
BENCH=out/vs2022-x64/bin/Release/iccBenchApply.exe
cmake --build out/vs2022-x64 --config Release --target iccBenchApply
$BENCH -suite -csv -pixels 1048576 -repeats 9
```

**Timing on a busy machine is noise.** A task's speed result is informational; its
checksum result is the gate. Do not hold a task back because the Mpx/s did not
move — several of these are small by design, and one (Task 5) is known from the
baseline to be worth about 1.4%.

## Testing approach

Same as the parent branch: this repo has no unit-test framework. Each task's test
is the harness plus, where behaviour is subtle, the existing CTest suite.

Every task ends with:

```bash
cmake --build out/vs2022-x64 --config Release --target iccBenchApply
$BENCH -suite -csv -pixels 1048576 -repeats 9        # checksums vs baseline
ctest --test-dir out/vs2022-x64 -C Release --label-exclude known-red   # 101/101
```

The full CTest run matters more here than on the parent branch: these edits are
inside `IccProfLib`, so they can break correctness tests the harness cannot see.

---

### Task 1: C1 — stop allocating a sparse matrix per pixel

`CIccPcsStepSparseMatrix::Apply` constructs a `CIccSparseMatrix` with
`bInitFromData=true` on every pixel, and `CIccSparseMatrix::Init`
(`IccSparseMatrix.cpp:145`) does `delete m_Data; m_Data = new CIccSparseMatrix<T>()`.
That is a heap allocation and free per pixel, plus a dimension re-parse and the
4096-dimension bounds test, all from `m_vals`, which never changes.

**Files:**
- Modify: `IccProfLib/IccCmm.h` — `CIccPcsStep` gains an init hook; `CIccPcsStepSparseMatrix` gains a member
- Modify: `IccProfLib/IccCmm.cpp` — `CIccPcsXform::Begin`, `CIccPcsStepSparseMatrix::{ctor,dtor,Apply}`

**Interfaces:**
- Consumes: nothing.
- Produces: `virtual bool CIccPcsStep::BeginStep() { return true; }`, overridden by
  `CIccPcsStepSparseMatrix`. Task 2 onward does not depend on it, but branch 3 may
  reuse the hook.

- [ ] **Step 1: Confirm the allocation is real before optimising it**

```bash
grep -n "bInitFromData && pMatrix" -A 5 IccProfLib/IccSparseMatrix.cpp
grep -n "delete m_Data" IccProfLib/IccSparseMatrix.cpp
```
Expected: `Init()` unconditionally `delete`s and `new`s `m_Data`, and the ctor calls
it when `bInitFromData` is set. If that is not what the code does, stop — the
finding is wrong and the rest of this task is pointless.

- [ ] **Step 2: Add the init hook to `CIccPcsStep`**

In `IccProfLib/IccCmm.h`, in the `CIccPcsStep` public block (around `:661`):

```cpp
  /// Called once by CIccPcsXform::Begin() before any Apply(), after the step
  /// list is fully built. Steps that can precompute invariant state do it here
  /// rather than lazily in Apply(): Apply() is const and runs concurrently on
  /// every worker thread, so a lazy first-call initialisation would be a race.
  /// Return false to fail the enclosing transform.
  virtual bool BeginStep() { return true; }
```

- [ ] **Step 3: Call it from `CIccPcsXform::Begin()`**

`CIccPcsXform::Begin()` is currently `{ return icCmmStatOk; }` inline in the
header (around `:1120`). Move it to `IccCmm.cpp` and walk the step list. First
confirm the member name holding the steps:

```bash
grep -n "class  ICCPROFLIB_API CIccPcsXform" -A 40 IccProfLib/IccCmm.h | grep -E "m_list|CIccPcsStepList|protected"
```

Then, with `<name>` as that member:

```cpp
icStatusCMM CIccPcsXform::Begin()
{
  // Give each step its one chance to precompute. Runs after Connect()/Optimize()
  // have finished building the list, and before GetNewApply(), so a step can
  // build immutable state here and treat it as read-only in Apply().
  if (m_list) {
    CIccPcsStepList::iterator i;
    for (i = m_list->begin(); i != m_list->end(); i++) {
      if (i->ptr && !i->ptr->BeginStep())
        return icCmmStatInvalidLut;
    }
  }
  return icCmmStatOk;
}
```

Remove the inline body from the header, leaving `virtual icStatusCMM Begin();`.

- [ ] **Step 4: Build the matrix once**

`IccProfLib/IccCmm.h`, `CIccPcsStepSparseMatrix` protected block (`:1046`):

```cpp
  CIccSparseMatrix *m_pMtx;   ///< built once by BeginStep(); read-only in Apply()
```

Declare `virtual bool BeginStep();` in its public block. In `IccCmm.cpp`:

```cpp
// The matrix is parsed from m_vals, which is fixed once the step is built, so
// the CIccSparseMatrix it produces is identical for every pixel. Building it
// here rather than in Apply() removes a heap allocation and free per pixel:
// CIccSparseMatrix::Init() (IccSparseMatrix.cpp:145) unconditionally deletes and
// re-news its m_Data accessor, and the ctor calls Init() when bInitFromData is
// set, which Apply() was doing on every call.
//
// It lives on the step, not on a CIccApplyPcsStep, because it is immutable after
// this point -- MultiplyVector() is const and writes only the caller's output
// buffer -- so every thread can share the one instance. Contrast
// CIccPcsStepSrcSparseMatrix, whose matrix is re-pointed at per-pixel source
// data and therefore cannot be shared.
bool CIccPcsStepSparseMatrix::BeginStep()
{
  delete m_pMtx;   // idempotent: BeginStep() may be reached more than once
  m_pMtx = new (std::nothrow) CIccSparseMatrix((icUInt8Number*)m_vals,
                                               m_nBytesPerMatrix,
                                               icSparseMatrixFloatNum, true);
  return m_pMtx != NULL;
}
```

Initialise `m_pMtx = NULL;` in the constructor and `delete m_pMtx;` in the
destructor. Then `Apply()` becomes:

```cpp
void CIccPcsStepSparseMatrix::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  // Matrix built once in BeginStep(); this used to construct a CIccSparseMatrix
  // per pixel, which allocated and freed on every call. See BeginStep().
  if (m_pMtx)
    m_pMtx->MultiplyVector(pDst, pSrc);
}
```

- [ ] **Step 5: Build and check the gate**

```bash
cmake --build out/vs2022-x64 --config Release --target iccBenchApply
$BENCH -suite -csv -pixels 1048576 -repeats 9
```
Expected: **`spectral-6ch` checksum still `0x469230ed`** and its Mpx/s up from
3.883. Every other checksum unchanged. If the checksum moved, the prebuilt matrix
is not equivalent to the per-pixel one — investigate, do not proceed.

- [ ] **Step 6: Run the full suite**

```bash
ctest --test-dir out/vs2022-x64 -C Release --label-exclude known-red
```
Expected: 101/101. `CIccPcsXform::Begin()` moving out of line changes a vtable
entry's definition site, so a link-level mistake shows up here.

- [ ] **Step 7: Commit**

```bash
git add IccProfLib/IccCmm.h IccProfLib/IccCmm.cpp
git commit -m "perf(cmm): build the sparse PCS matrix once instead of per pixel

CIccPcsStepSparseMatrix::Apply constructed a CIccSparseMatrix with
bInitFromData=true on every pixel, and CIccSparseMatrix::Init unconditionally
deletes and re-news its m_Data accessor -- a heap allocation and free per pixel,
plus a dimension re-parse and a bounds test, all derived from m_vals, which is
fixed once the step is built.

Adds CIccPcsStep::BeginStep(), called once by CIccPcsXform::Begin() after the
step list is built, and builds the matrix there. It lives on the step rather than
in a per-apply object because it is immutable afterwards and MultiplyVector() is
const, so all threads can share one instance. Doing it lazily in Apply() instead
would have been a race: Apply() is const and runs on every worker thread.

spectral-6ch checksum unchanged.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: C2 — precompute the monochrome reference white

`CIccXformMonochrome::Apply` (`IccCmm.cpp:5630`) converts the perceptual
reference white through `icXyzToPcs` and, for a Lab PCS, `XyzToLab` — three cube
roots — on every pixel, from compile-time constants. Both branches are
invariant: the input branch's `DstPixel` vector before the final multiply, and
the output branch's divisor.

This is the largest single win in the file: baseline `monochrome` is 29.213 Mpx/s.

**Files:**
- Modify: `IccProfLib/IccCmm.h` — `CIccXformMonochrome` protected members
- Modify: `IccProfLib/IccCmm.cpp` — `CIccXformMonochrome::{Begin,Apply}`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing later tasks use.

- [ ] **Step 1: Read the current Apply and identify exactly what is invariant**

```bash
sed -n '5630,5690p' IccProfLib/IccCmm.cpp
```
Confirm before changing anything: in the `m_bInput` branch, everything from
`DstPixel[0] = icPerceptualRefWhiteX` down to the `XyzToLab` call depends only on
`m_pProfile->m_Header.pcs` and `UseLegacyPCS()`. In the else branch, `Pixel[]` is
built from the same constants and only `Pixel[0]` or `Pixel[1]` is used, as a
divisor.

- [ ] **Step 2: Add the precomputed members**

`IccProfLib/IccCmm.h`, `CIccXformMonochrome` protected block (around `:1261`):

```cpp
  /// Reference white in PCS encoding, computed once by Begin(). The input branch
  /// of Apply() scales this by the curve output; the output branch divides by
  /// m_fWhiteRecip. Immutable after Begin(), so shared across threads.
  icFloatNumber m_PcsWhite[3];
  icFloatNumber m_fWhiteRecip;
```

- [ ] **Step 3: Compute them in `Begin()`**

At the end of `CIccXformMonochrome::Begin()`, before `return icCmmStatOk;`:

```cpp
  // Apply() used to rebuild this on every pixel from compile-time constants --
  // icXyzToPcs plus, for a Lab PCS, XyzToLab and its three cube roots. Nothing
  // in it depends on the source colour, and both m_pProfile->m_Header.pcs and
  // UseLegacyPCS() (a virtual call) are fixed by the time Begin() runs.
  //
  // Idempotent: recomputing from the same constants yields the same values, so a
  // second Begin() is harmless.
  m_PcsWhite[0] = icFloatNumber(icPerceptualRefWhiteX);
  m_PcsWhite[1] = icFloatNumber(icPerceptualRefWhiteY);
  m_PcsWhite[2] = icFloatNumber(icPerceptualRefWhiteZ);

  icXyzToPcs(m_PcsWhite);

  if (m_pProfile->m_Header.pcs == icSigLabData) {
    if (UseLegacyPCS())
      CIccPCSUtil::XyzToLab2(m_PcsWhite, m_PcsWhite, true);
    else
      CIccPCSUtil::XyzToLab(m_PcsWhite, m_PcsWhite, true);
  }

  // The output branch divides by component 0 for a Lab PCS and component 1
  // otherwise. Precompute the reciprocal so Apply() multiplies.
  {
    const icFloatNumber d = (m_pProfile->m_Header.pcs == icSigLabData)
                              ? m_PcsWhite[0] : m_PcsWhite[1];
    m_fWhiteRecip = icNotZero(d) ? (icFloatNumber)(1.0 / d) : (icFloatNumber)0.0;
  }
```

**Verify `icNotZero` is reachable here** — it is a macro in `IccUtil.h`
(`:85`), which `IccCmm.cpp` already includes. If the divisor was previously
allowed to be zero and produce an infinity, note that this changes that to zero;
check whether any test depends on it before assuming it is safe. The baseline
`monochrome` checksum is the arbiter.

- [ ] **Step 4: Rewrite `Apply()` to use them**

```cpp
void CIccXformMonochrome::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  if (m_bSrcPcsConversion)
    SrcPixel = CheckSrcAbs(pApply, SrcPixel);

  if (m_bInput) {
    icFloatNumber v = SrcPixel[0];
    if (m_ApplyCurvePtr)
      v = m_ApplyCurvePtr->Apply(v);

    // m_PcsWhite computed once in Begin(); this block used to redo icXyzToPcs
    // and XyzToLab -- three cube roots -- per pixel from constants.
    DstPixel[0] = m_PcsWhite[0] * v;
    DstPixel[1] = m_PcsWhite[1] * v;
    DstPixel[2] = m_PcsWhite[2] * v;
  }
  else {
    // Same hoist, reciprocal form: the divisor is a fixed component of the
    // PCS-encoded reference white, so Begin() precomputed 1/d.
    const icFloatNumber s = (m_pProfile->m_Header.pcs == icSigLabData)
                              ? SrcPixel[0] : SrcPixel[1];
    DstPixel[0] = s * m_fWhiteRecip;

    if (m_ApplyCurvePtr)
      DstPixel[0] = m_ApplyCurvePtr->Apply(DstPixel[0]);
  }

  if (m_bDstPcsConversion)
    CheckDstAbs(DstPixel);
}
```

The `m_pProfile->m_Header.pcs == icSigLabData` test in the else branch is still a
per-pixel read of an invariant. Leave it for now — it selects which *source*
component to read, not a computation, and folding it in needs a second member.
Note it in the commit as deliberately left.

- [ ] **Step 5: Build and check the gate**

Expected: **`monochrome` checksum still `0x54ce851f`**, Mpx/s well above 29.213.
Everything else unchanged.

If the checksum moved, the most likely cause is the division-to-multiplication
change: `x/d` and `x*(1/d)` are not bit-identical in general. **This is the one
place in tier C where a moved checksum may be legitimate.** If it moved, confirm
that is the cause by temporarily reverting just the reciprocal (keep `s / d`) and
re-running; if the checksum returns, decide whether to keep the division for exact
equivalence or accept the delta and document it. Prefer keeping the division —
the cube-root hoist is the real win, and a divide per pixel is cheap next to it.

- [ ] **Step 6: Run the full suite**

Expected: 101/101.

- [ ] **Step 7: Commit**

```bash
git add IccProfLib/IccCmm.h IccProfLib/IccCmm.cpp
git commit -m "perf(cmm): precompute the monochrome reference white in Begin()

CIccXformMonochrome::Apply rebuilt the PCS-encoded perceptual reference white on
every pixel from compile-time constants -- icXyzToPcs plus, for a Lab PCS,
XyzToLab and its three cube roots, and a virtual UseLegacyPCS() call. Nothing in
that block depends on the source colour.

Precomputed in Begin() and held on the xform, which is safe because it is
immutable afterwards. The one invariant left in Apply() is the header PCS test
that selects which source component the output branch reads; it picks a component
rather than computing anything, so folding it in would need another member for
no measurable gain.

monochrome checksum unchanged.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: C3 — cache the MPE source and destination encodings

`CIccXformMpe::Apply` calls `GetSrcSpace()` (`:8394`) and `GetDstSpace()`
(`:8420`) per pixel. `CIccXform::GetSrcSpace()` (`:1827`) branches four ways and
dereferences the profile header. The two intent guards at `:8387` and `:8433` are
invariant too.

**Note:** an earlier draft of the audit called those intent guards a likely
`&&`/`||` slip. They are correct — short-circuit makes the second clause
reachable only when `m_nIntent == icAbsoluteColorimetric`, where
`m_nIntent != m_nTagIntent` is equivalent to the intended
`m_nTagIntent != icAbsoluteColorimetric`. This task hoists them; it does not
change them.

**Files:**
- Modify: `IccProfLib/IccCmm.h` — `CIccXformMpe` protected members
- Modify: `IccProfLib/IccCmm.cpp` — `CIccXformMpe::{Begin,Apply}`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing.

- [ ] **Step 1: Confirm `CIccXformMpe` has a `Begin()` to extend**

```bash
grep -n "class ICCPROFLIB_API CIccXformMpe" -A 30 IccProfLib/IccCmm.h | grep -E "Begin|protected|m_pTag"
```
If it has no `Begin()` override, add one that calls the base and then does the
caching. Do not put this in the constructor: `GetSrcSpace()` reads
`m_bPcsAdjustXform` and `m_bUseSpectralPCS`, which are set after construction.

- [ ] **Step 2: Add the cached members**

```cpp
  /// Src/dst PCS encodings and the two abs-conversion decisions, resolved once
  /// in Begin(). Apply() called GetSrcSpace()/GetDstSpace() per pixel, and each
  /// branches four ways through the profile header.
  icColorSpaceSignature m_cachedSrcSpace;
  icColorSpaceSignature m_cachedDstSpace;
  bool                  m_bDoSrcAbs;
  bool                  m_bDoDstAbs;
```

- [ ] **Step 3: Populate in `Begin()`**

```cpp
  // Everything below is fixed once the profile and intent are attached.
  // GetSrcSpace()/GetDstSpace() each branch on m_bInput, m_bPcsAdjustXform,
  // m_bUseSpectralPCS and the profile header, and Apply() called them per pixel.
  m_cachedSrcSpace = GetSrcSpace();
  m_cachedDstSpace = GetDstSpace();

  // The guard is correct as written and is preserved exactly: when m_nIntent is
  // not absolute the first clause is already true, and when it is absolute the
  // second reduces to m_nTagIntent != icAbsoluteColorimetric -- the "B2D3 tags
  // don't need abs conversion" case the original comment describes.
  const bool bIntentNeedsAbs = (m_nIntent != icAbsoluteColorimetric ||
                                m_nIntent != m_nTagIntent);
  m_bDoSrcAbs = bIntentNeedsAbs && m_bSrcPcsConversion;
  m_bDoDstAbs = bIntentNeedsAbs && m_bDstPcsConversion;
```

- [ ] **Step 4: Use them in `Apply()`**

Replace `if (m_nIntent != ... ) { if (m_bSrcPcsConversion) SrcPixel = CheckSrcAbs(...); }`
with `if (m_bDoSrcAbs) SrcPixel = CheckSrcAbs(pApply, SrcPixel);`, the destination
equivalent with `m_bDoDstAbs`, and both `switch (GetSrcSpace())` /
`switch(GetDstSpace())` with the cached members. Leave a comment at each site
naming `Begin()` as where the value now comes from.

- [ ] **Step 5: Build and check the gate**

Expected: **`mpe-calc` `0xe275c18d`, `mpe-tonemap` `0xdc6eb214`, `matrix-trc`
`0x2f8ddb88`, `pcs-rel` `0x2416f560`, `pcs-abs` `0xbf684583` all unchanged.**
Several cases resolve to `Mpe` xforms, so this touches more of the table than it
looks like.

- [ ] **Step 6: Run the full suite.** Expected 101/101.

- [ ] **Step 7: Commit**

```bash
git add IccProfLib/IccCmm.h IccProfLib/IccCmm.cpp
git commit -m "perf(cmm): cache the MPE PCS encodings and abs-conversion decisions

CIccXformMpe::Apply called GetSrcSpace() and GetDstSpace() on every pixel. Each
branches four ways on m_bInput, m_bPcsAdjustXform, m_bUseSpectralPCS and the
profile header, none of which changes after Begin().

Also folds the two intent guards and the m_bSrcPcsConversion /
m_bDstPcsConversion tests into one precomputed bool per direction. The guards are
preserved exactly, not rewritten: an earlier reading of
'm_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent' took it for a
&&/|| slip, but short-circuit makes the second clause reachable only when
m_nIntent is absolute, where it is equivalent to the intended
m_nTagIntent != icAbsoluteColorimetric.

All MPE-path checksums unchanged.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: C4 — filter ACS elements out of the MPE apply list

`CIccTagMultiProcessElement::Apply` (`IccTagMPE.cpp:1552`) makes a virtual
`IsAcs()` call per element per pixel to skip elements that are structural
markers. Whether an element is one is known when the apply list is built.

**Files:**
- Modify: `IccProfLib/IccTagMPE.cpp` — where the apply list is built, and `Apply`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing.

- [ ] **Step 1: Find where the apply list is built and confirm ACS elements can be omitted**

```bash
grep -n "CIccTagMultiProcessElement::Begin\|GetNewApply" IccProfLib/IccTagMPE.cpp | head
grep -rn "IsAcs" IccProfLib/ Tools/ IccXML/ 2>/dev/null
```
**This step is a genuine investigation, not a formality.** If anything other than
`CIccTagMultiProcessElement::Apply` consults `IsAcs()` — element counting,
channel validation, `Describe`, XML round-trip — then omitting ACS elements from
the apply list may change those. If any other consumer exists, do not omit;
instead precompute a `std::vector<bool>` or store the already-filtered pointers
alongside the full list, and report what you found.

- [ ] **Step 2: Make the change indicated by Step 1**

If ACS elements are safe to omit from the *apply* list only, skip appending them
where the list is built, with:

```cpp
        // ACS elements are structural markers with an identity Apply(). Whether
        // an element is one is fixed at load, so they are omitted here rather
        // than skipped per pixel: Apply() previously made a virtual IsAcs() call
        // for every element of every pixel. The full element list is unchanged --
        // only this apply list is filtered.
```

Then remove the `if (!pElem->IsAcs())` test and its `GetElem()` call from the
middle loop of `Apply`, leaving a comment pointing at the filter.

- [ ] **Step 3: Build and check the gate**

Expected: `mpe-calc`, `mpe-tonemap`, `spectral-6ch`, `matrix-trc` checksums
unchanged.

- [ ] **Step 4: Run the full suite.** Expected 101/101. This is the task most
likely to break an XML or Describe test, so read any failure carefully rather
than assuming it is unrelated.

- [ ] **Step 5: Commit** with a message stating what Step 1 found about other
`IsAcs()` consumers.

---

### Task 5: C5 — cache the AdjustPCS dispatch

`CIccXform::AdjustPCS` (`IccCmm.cpp:1728`) re-reads `m_pProfile->m_Header.pcs`
and makes two virtual `UseLegacyPCS()` calls per pixel.

**The baseline says this is worth about 1.4%** (`pcs-abs` 3.466 vs `pcs-rel`
3.419), so it is a tidiness task. Do it because the code is clearer, not because
it is fast.

**Files:**
- Modify: `IccProfLib/IccCmm.h` — a cached enum on `CIccXform`
- Modify: `IccProfLib/IccCmm.cpp` — `CIccXform::Begin`, `AdjustPCS`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing.

- [ ] **Step 1: Add the cached mode**

In `CIccXform`'s protected block:

```cpp
  /// Which PCS adjustment AdjustPCS() performs, resolved once in Begin().
  /// AdjustPCS() ran per pixel and re-read the profile header and called the
  /// virtual UseLegacyPCS() twice to rediscover this.
  enum icPcsAdjustMode { icPcsAdjustXyz, icPcsAdjustLab, icPcsAdjustLab2 };
  icPcsAdjustMode m_nPcsAdjustMode;
```

- [ ] **Step 2: Resolve it in `CIccXform::Begin()`**

```cpp
  // Fixed once the profile is attached. AdjustPCS() rediscovered it per pixel.
  if (m_pProfile && m_pProfile->m_Header.pcs == icSigLabData)
    m_nPcsAdjustMode = UseLegacyPCS() ? icPcsAdjustLab2 : icPcsAdjustLab;
  else
    m_nPcsAdjustMode = icPcsAdjustXyz;
```

Confirm `CIccXform::Begin()` exists and that derived `Begin()` overrides call it;
if some do not, put the assignment in a small helper both paths call, or in the
constructor plus a re-resolve in `Begin()`. **Do not assume every derived
`Begin()` chains to the base** — check:

```bash
grep -n "CIccXform::Begin()" IccProfLib/IccCmm.cpp
```

- [ ] **Step 3: Switch on it in `AdjustPCS`**, replacing both
`if (Space==icSigLabData) { if (UseLegacyPCS()) ... }` blocks. Comment each with
where the mode is decided.

- [ ] **Step 4: Also collapse the `CheckSrcAbs`/`CheckDstAbs` double test.**
Every `Apply` does `if (m_bSrcPcsConversion) SrcPixel = CheckSrcAbs(...)`, and
`CheckSrcAbs` then re-tests `m_bAdjustPCS && !m_bInput`. Add
`m_bDoSrcAbsAdjust` / `m_bDoDstAbsAdjust` bools resolved in `Begin()` and test
those instead. Note that Task 3 already did this for `CIccXformMpe`; keep the
naming consistent.

- [ ] **Step 5: Build and check the gate.** All eight checksums unchanged.

- [ ] **Step 6: Run the full suite.** Expected 101/101.

- [ ] **Step 7: Commit**, saying in the message that the measured win is ~1.4% and
the change is for clarity.

---

### Task 6: C6 — remove the CLUT clip indirect call

Every CLUT interpolator calls `m_UnitClipFunc` once per channel per pixel — an
indirect call that blocks vectorising the clamp — and then redoes the same work
inline with `isfinite` and two comparisons.

**This is the one task authorised to move a checksum.** Per the decision recorded
for this branch, the two paths are unified on `+inf -> mx`: the default
`ClutUnitClip` already maps `+inf` to `1.0` and thence to `mx`, while the
spectral `NoClip` path let it reach the `isfinite` test and produced `0`. Unifying
makes `NoClip` agree with the default. `spectral-6ch`, and any other case whose
profile installs `NoClip`, will report a new checksum. That is expected and must
be recorded in the commit.

**Files:**
- Modify: `IccProfLib/IccTagLut.cpp` — `Interp1d`, `Interp2d`, `Interp3dTetra`,
  `Interp3d`, `Interp4d`, `Interp5d`, `Interp6d`, `InterpND`
- Modify: `IccProfLib/IccTagLut.h` — `m_UnitClipFunc` and `SetClipFunc`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing.

- [ ] **Step 1: Record the pre-change checksums for every case.** Not just
`spectral-6ch` — you need to know exactly which cases move, because any case that
moves *without* installing `NoClip` indicates a mistake in the clamp rewrite
rather than the intended semantic change.

- [ ] **Step 2: Rewrite one interpolator first — `Interp1d`**

```cpp
  // Clamp inline rather than through m_UnitClipFunc. That call was an indirect
  // call per channel per pixel which the surrounding code had already made
  // redundant: on the default path ClutUnitClip did isnan plus a clamp to [0,1],
  // and the interpolator then redid the work as isfinite plus two comparisons.
  // Clamping to [0,mx] after the multiply subsumes clamping to [0,1] before it.
  //
  // The form below reproduces ClutUnitClip's semantics exactly for NaN (-> 0),
  // -inf (-> 0) and +inf (-> mx), and deliberately UNIFIES the NoClip path onto
  // the same behaviour: NoClip previously let +inf reach the isfinite test and
  // yield 0, disagreeing with the default path for no stated reason. See #NNNN.
  icFloatNumber x = srcPixel[0] * mx;
  if (!(x > 0.0f)) x = 0.0f;      // false for NaN, so NaN lands here too
  else if (x > mx) x = mx;
```

Build and check: `lut-3d-tetra` (which uses `ClutUnitClip`) must be
**unchanged**, since this form is exactly equivalent on that path. If it moves,
the clamp is wrong — fix it before touching the other seven.

- [ ] **Step 3: Apply the same form to the remaining seven interpolators**, one
per channel, keeping the full comment only in `Interp1d` and a one-line
back-reference in the others.

- [ ] **Step 4: Retire `m_UnitClipFunc`**

Once no interpolator calls it, `m_UnitClipFunc`, `SetClipFunc`, `icCLUTCLIPFUNC`,
`ClutUnitClip`, and the `NoClip` statics in `IccMpeBasic.cpp` and
`IccMpeSpectral.cpp` are all dead. `SetClipFunc` is **public API** — removing it
breaks any external caller. Keep it as a no-op with a comment explaining that
clipping is now unconditional and why, rather than deleting it:

```cpp
  /// Retained for source compatibility. Clipping is now performed inline by the
  /// Interp* functions, uniformly for both the former ClutUnitClip and NoClip
  /// behaviours, so this no longer has any effect. See #NNNN.
  void SetClipFunc(icCLUTCLIPFUNC /* ClipFunc */) {}
```

Leave the seven `SetClipFunc(NoClip)` call sites in `IccMpeBasic.cpp` and
`IccMpeSpectral.cpp` in place — they become no-ops and removing them is churn on
a branch that is already the behaviour-changing one.

- [ ] **Step 5: Build and check the gate**

Expected: `lut-3d-tetra`, `matrix-trc`, `monochrome`, `pcs-rel`, `pcs-abs`
unchanged. `spectral-6ch` **moves** — record the new value. Determine whether
`mpe-calc` and `mpe-tonemap` move and explain either outcome.

- [ ] **Step 6: Run the full suite, and read `interp6d-nan-cast` specifically**

```bash
ctest --test-dir out/vs2022-x64 -C Release --label-exclude known-red
ctest --test-dir out/vs2022-x64 -C Release -R interp6d --output-on-failure
```
That test asserts only that `Interp6d` returns **finite** output for a non-finite
`srcPixel[5]`, which `+inf -> mx` satisfies. It must still pass. If it fails, the
clamp is wrong, not the test.

- [ ] **Step 7: Commit**, with the old and new `spectral-6ch` checksums both in
the message and the semantic change stated in the first paragraph, not buried.

---

### Task 7: C7 — precompute the XYZ-to-Lab white point reciprocals

`icXYZtoLab` (`IccUtil.cpp:897`) calls `icSafeXYZRatio` three times, each a
two-comparison `icNotZero` plus a divide, against a white point that is invariant
per step.

Per the decision for this branch, this adds a **public overload** in `IccUtil.h`;
the existing `icXYZtoLab` is untouched.

**Files:**
- Modify: `IccProfLib/IccUtil.h`, `IccProfLib/IccUtil.cpp` — new overload
- Modify: `IccProfLib/IccCmm.h`, `IccProfLib/IccCmm.cpp` — `CIccPcsStepXYZToLab`
- Modify: `IccProfLib/IccMpeSpectral.cpp` — `CIccMpeSpectralObserver`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  void icXYZtoLabRecip(icFloatNumber *Lab, const icFloatNumber *XYZ,
                       const icFloatNumber *WhiteRecip);
  ```
  Task 8 also touches `CIccMpeSpectralObserver`; do Task 7 first and Task 8 on top.

- [ ] **Step 1: Add the overload**

`IccUtil.h`, beside the existing declaration:

```cpp
/// As icXYZtoLab, but takes the reciprocal of each white-point component instead
/// of the component itself, so a caller with an invariant white point can
/// precompute 1/w once and pay three multiplies per call rather than three
/// divides and six comparisons. A zero reciprocal reproduces icSafeXYZRatio's
/// zero result for a zero white-point component.
ICCPROFLIB_API void icXYZtoLabRecip(icFloatNumber *Lab, const icFloatNumber *XYZ,
                                    const icFloatNumber *WhiteRecip);
```

`IccUtil.cpp`:

```cpp
void icXYZtoLabRecip(icFloatNumber *Lab, const icFloatNumber *XYZ,
                     const icFloatNumber *WhiteRecip)
{
  const icFloatNumber Xn = icCubeth(XYZ[0] * WhiteRecip[0]);
  const icFloatNumber Yn = icCubeth(XYZ[1] * WhiteRecip[1]);
  const icFloatNumber Zn = icCubeth(XYZ[2] * WhiteRecip[2]);

  Lab[0] = (icFloatNumber)(116.0 * Yn - 16.0);
  Lab[1] = (icFloatNumber)(500.0 * (Xn - Yn));
  Lab[2] = (icFloatNumber)(200.0 * (Yn - Zn));
}
```

**Check the `ICCPROFLIB_API` spelling against the neighbouring declarations** —
`IccUtil.h` may not decorate free functions the same way as classes.

- [ ] **Step 2: Add a helper for building the reciprocals**, so the three call
sites do not each re-derive the zero rule:

```cpp
/// Fills WhiteRecip with 1/w per component, or 0 where the component is zero,
/// matching what icXYZtoLab's internal icSafeXYZRatio produces for that case.
ICCPROFLIB_API void icXYZWhiteRecip(icFloatNumber *WhiteRecip,
                                    const icFloatNumber *WhiteXYZ);
```

- [ ] **Step 3: Move `CIccPcsStepXYZToLab` over.** Add an
`icFloatNumber m_xyzWhiteRecip[3];` member, fill it in the constructor beside the
`m_xyzWhite` memcpy (the white point is a constructor argument, so no `BeginStep`
is needed), and call `icXYZtoLabRecip` in `Apply`.

- [ ] **Step 4: Move `CIccMpeSpectralObserver::Apply` over**, filling its
reciprocals in the same `Begin()` that Task 8 will touch.

- [ ] **Step 5: Build and check the gate**

**Expect checksums to move here, and decide deliberately.** `x/w` and `x*(1/w)`
are not bit-identical. Run first and see:

- If nothing moves, keep the reciprocal form.
- If `pcs-rel`/`pcs-abs`/`spectral-6ch` move, that is float reassociation, not a
  defect — but it costs the exact-equality property the oracle relies on for the
  rest of tier C. Given the divide is not the dominant cost in these cases
  (baseline `pcs-rel` 3.419 with three divides per pixel), **prefer reverting to
  the division form and keeping only the six comparisons hoisted**: pass the
  white point *and* a precomputed "all components non-zero" bool, and skip
  `icNotZero` rather than the divide. Report which you chose and why.

- [ ] **Step 6: Run the full suite.** Expected 101/101.

- [ ] **Step 7: Commit**, recording the Step 5 decision.

---

### Task 8: C8 — hoist the observer flag decode and the named-colour dispatch

Two small invariant re-derivations:
`CIccMpeSpectralObserver::Apply` (`IccMpeSpectral.cpp:2004`) decodes `m_flags`
into two bools per pixel; `CIccApplyNamedColorCmm::Apply` (`IccCmm.cpp:10843`)
re-calls `GetXform()`, `GetXformType()` and `GetInterface()` per xform per pixel.

The named-colour half is not on any benchmark case, so it is justified by
inspection only. Say so in the commit rather than implying it was measured.

**Files:**
- Modify: `IccProfLib/IccMpeSpectral.cpp` — `CIccMpeSpectralObserver::{Begin,Apply}`
- Modify: `IccProfLib/IccMpeSpectral.h` — two bool members
- Modify: `IccProfLib/IccCmm.cpp` — `CIccApplyNamedColorCmm::Apply`

**Interfaces:**
- Consumes: Task 7's reciprocal members on the observer.
- Produces: nothing.

- [ ] **Step 1: Hoist the observer flags.** Add
`bool m_bUseAbsoluteFlag; bool m_bLabFlag;` resolved in `Begin()` from `m_flags`,
with a comment naming `Begin()`. Use them in `Apply`.

- [ ] **Step 2: Build and check** `spectral-6ch` — unchanged from whatever Task 6
left it at.

- [ ] **Step 3: Hoist the named-colour dispatch.** Store a resolved kind on each
apply-list entry when the list is built, rather than re-querying per pixel. If
that turns out to need a new member on `CIccApplyXform`, note that
`CIccApplyXform` is per-thread, so it is a valid place for it — but the *kind* is
immutable, so prefer the shared xform if one is reachable.

**If this turns out to require restructuring `CIccApplyXformList`, stop and
report.** It is the lowest-value item in the branch and not worth a risky
refactor; leaving it undone with a note is the better outcome.

- [ ] **Step 4: Run the full suite.** Expected 101/101. Named-colour has CTest
coverage even though it has no benchmark case, so this is the real gate.

- [ ] **Step 5: Commit**, stating which parts were measured and which were not.

---

### Task 9: Record the results

**Files:**
- Create: `docs/superpowers/results/2026-08-20-tier-c-results.md`

- [ ] **Step 1: Capture the final numbers**

```bash
$BENCH -suite -csv -pixels 1048576 -repeats 9
$BENCH -suite -csv -pixels 1048576 -repeats 9 -threads 1,4,8
```

- [ ] **Step 2: Write the table** — baseline Mpx/s, final Mpx/s, delta, and
checksum before/after per case. Mark the `spectral-6ch` checksum change as the
authorised Task 6 delta, with the reason.

- [ ] **Step 3: State what did not improve.** Any task whose case did not move,
and why — small win, noise floor, or not covered by a case. A results document
that only lists wins is not useful for deciding what branch 3 should prioritise.

- [ ] **Step 4: Commit.**

## Self-review notes

- **Every tier-C finding has a task:** C1→1, C2→2, C3→3, C4→4, C5→5, C6→6, C7→7,
  C8→8, plus Task 9 for results.
- **Three tasks carry a real risk of a moved checksum** and each says what to do
  about it rather than assuming: Task 2 (divide → multiply), Task 6 (the
  authorised `+inf` change), Task 7 (divide → multiply again, with a documented
  fallback to keep the divide).
- **Three tasks contain genuine stop-and-report conditions**: Task 4 if another
  `IsAcs()` consumer exists, Task 8 Step 3 if the named-colour dispatch needs a
  list refactor, and the global constraint about any hoist that seems to need a
  behaviour change.
- **Two things deliberately left undone**, both noted in their tasks rather than
  silently skipped: the header-PCS component selection in Task 2's output branch,
  and the `SetClipFunc` call sites in Task 6 Step 4.
