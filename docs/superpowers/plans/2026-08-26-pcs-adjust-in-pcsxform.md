# PCS Adjustments in CIccPcsXform — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `CIccPcsXform` the only place a PCS adjustment is applied, so that adjustments which cancel are optimized away instead of leaving a float residual.

**Architecture:** The decision "does this xform need a PCS adjustment on this side" moves from `Apply()` time (where each `CIccXform` subclass tests its own flags) to `Begin()` time (where `CIccCmm::CheckPCSConnections()` asks the xform through a virtual predicate, and `CIccPcsXform::Connect()` / `ConnectFirst()` / `ConnectLast()` push the adjustment as `CIccPcsStep`s). `CIccPcsXform::Optimize()` then folds and drops whatever is unnecessary. The existing `m_bSrcPcsConversion` / `m_bDstPcsConversion` suppression flags are used as the transition mechanism so the behavior flip is one reviewable commit, then deleted.

**Tech Stack:** C++17, IccProfLib, CMake 3.21+, CTest. Windows MSVC/vcpkg is the local build; Linux clang is the CI reference.

**Spec:** [`docs/superpowers/plans/2026-08-26-pcs-adjust-in-pcsxform-spec.md`](2026-08-26-pcs-adjust-in-pcsxform-spec.md)

## Global Constraints

- Branch: `refactor/pcs-adjust-in-pcsxform` (already created off `master`).
- 2-space C++ indentation, K&R braces, `m_` members, return-value errors (`AGENTS.md`).
- C++17. Compiler floor GCC 11+, Clang 10+, MSVC 19.30+.
- Every new or relocated C/C++ source or header must carry the complete ICC Software License block used by adjacent files. Abbreviated or omitted license text is a blocking defect.
- New regression helpers live in `.github/ci/regression/` and are registered in `Build/Cmake/Testing/CMakeLists.txt` via `iccdev_add_regression_executable()` inside a `function(iccdev_add_<name>_test)` guarded by `if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}") return() endif()`.
- Numeric assertions use a **relative tolerance of 1e-5**. Never assert bit-equality on transform output — the refactor legitimately re-associates float math at ~1e-7, while a dropped or doubled adjustment is ~3.5e-3 or larger. The band separates them. See the spec's "Test strategy".
- `CheckSrcAbs()`, `CheckDstAbs()` and `AdjustPCS()` are `protected` and reachable by third-party subclasses. Deprecate, never delete.
- `bUsePCSConversions` keeps its place in every signature. It becomes ignored, not removed.

## Local build commands

The tracked `out/vs2022-x64` tree cannot re-run CMake configure on this machine — vcpkg fails to fetch its baseline ref from GitHub. Build library targets directly with MSBuild, bypassing `ZERO_CHECK`:

```bash
MB="G:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe"
"$MB" out/vs2022-x64/IccProfLib/IccProfLib2.vcxproj \
  //p:Configuration=Release //p:Platform=x64 //p:BuildProjectReferences=false //m //v:m //nologo
```

Built binaries land in `out/vs2022-x64/bin/Release/`. Adding a *new* CMake target requires a working configure, so if the vcpkg fetch is still failing when you reach Task 1, configure a separate tree with the system vcpkg disabled:

```bash
cmake -S Build/Cmake -B out/plan-tests -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TOOLS=OFF -DENABLE_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R pcs-adjust-placement --output-on-failure
```

If that configure also fails, report it and stop rather than skipping the test — the tests are the point of this plan.

## File Structure

| File | Responsibility |
|---|---|
| `.github/ci/regression/pcs-adjust-placement.cpp` | **New.** Characterization + placement tests for all four chain shapes. Builds its own in-memory v2/v4 profiles, as `v2-legacy-pcs.cpp` does; depends on no tracked `.icc`. |
| `Build/Cmake/Testing/CMakeLists.txt` | **Modify.** Register `iccPcsAdjustPlacementTest`. |
| `IccProfLib/IccCmm.h` | **Modify.** Add virtual predicates; retire the two flags and the two old predicates; deprecate the three in-xform helpers. |
| `IccProfLib/IccCmm.cpp` | **Modify.** Leading-edge condition; the 15 `Connect*` guard sites; the 16 `Apply()` call sites; edge flag clearing. |
| `docs/pcs-adjustment-placement.md` | **New.** Short design note recording where adjustments live after this change and what moved numerically. |

---

### Task 1: Characterization harness

Pins the numeric invariants **before** anything changes. This test must pass on unmodified `master` and must still pass at every later task. It asserts *what the adjustment does*, never *where it happens* — placement assertions arrive in Task 4.

**Files:**
- Create: `.github/ci/regression/pcs-adjust-placement.cpp`
- Modify: `Build/Cmake/Testing/CMakeLists.txt` (new function near line 4833, calls near lines 6271 and 6628)

**Interfaces:**
- Consumes: nothing.
- Produces: for later tasks — `void buildV2CmykOutputProfile(CIccProfile&)`, `void buildV4CmykOutputProfile(CIccProfile&)`, `void attachRequiredTags(CIccProfile&, const char *desc)`, `bool closeRel(double got, double want, double tol)`, `void check(bool, const char*)`, the constant `kTol`, and the file-scope counter `g_failures`. Tasks 3, 4 and 6 add cases to this same file and reuse these.

- [ ] **Step 1: Write the failing test**

Create `.github/ci/regression/pcs-adjust-placement.cpp`. Start by copying the complete ICC Software License block from the top of `.github/ci/regression/v2-legacy-pcs.cpp` verbatim, then the body below.

```cpp
/*
    File:       pcs-adjust-placement.cpp

    Contains:   Characterization tests for PCS adjustment placement.

    A PCS adjustment (absolute media-white scaling, the v2-perceptual black
    point shift) is applied either by CIccPcsXform or inside CIccXform::Apply,
    depending on where the transform sits in the chain. This file pins what the
    adjustment *does*, so that moving *where* it happens is provably behaviour
    preserving.

    Tolerance: assertions use 1e-5 relative. The adjustments under test are
    ~3.5e-3 (v2 perceptual) or larger (absolute); re-associating the same affine
    math into CIccPcsSteps moves results by ~1e-7. Bit-equality is the wrong
    oracle and would fail on correct code.
*/

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char *msg)
{
  if (cond) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

// Relative comparison with an absolute floor, so values near zero do not
// demand impossible precision.
static bool closeRel(double got, double want, double tol)
{
  const double diff = std::fabs(got - want);
  const double scale = std::fabs(want) > 1.0 ? std::fabs(want) : 1.0;
  return diff / scale <= tol;
}

static const double kTol = 1e-5;

// --- fixtures -------------------------------------------------------------
// Built in memory rather than read from Testing/*.icc: Testing/**/*.icc is
// gitignored and generated by CreateAllProfiles.sh, so a tracked fixture is not
// available on a fresh clone.

static void attachRequiredTags(CIccProfile &profile, const char *desc)
{
  CIccTagTextDescription *pDesc = new CIccTagTextDescription();
  pDesc->SetText(desc);
  profile.AttachTag(icSigProfileDescriptionTag, pDesc);

  CIccTagText *pCprt = new CIccTagText();
  pCprt->SetText("Copyright (C) 2026 The International Color Consortium");
  profile.AttachTag(icSigCopyrightTag, pCprt);

  // Deliberately NOT the D50 illuminant. The absolute-intent adjustment is
  // driven by (mediaWhite != illuminant); a D50 media white would make the
  // absolute cases silently no-ops and the test would prove nothing.
  CIccTagXYZ *pWtpt = new CIccTagXYZ(1);
  (*pWtpt)[0].X = icDtoF((icFloatNumber)0.9000);
  (*pWtpt)[0].Y = icDtoF((icFloatNumber)0.9500);
  (*pWtpt)[0].Z = icDtoF((icFloatNumber)0.7500);
  profile.AttachTag(icSigMediaWhitePointTag, pWtpt);
}

// A smooth CMYK -> Lab cell. Values are PCS-encoded 0..1.
static void cmykToLabCell(const double *in, icFloatNumber *out)
{
  const double k = in[3];
  const double lum = (1.0 - k) * (1.0 - 0.30 * in[0] - 0.20 * in[1] - 0.10 * in[2]);
  out[0] = (icFloatNumber)(lum < 0.0 ? 0.0 : lum);            // L* 0..1
  out[1] = (icFloatNumber)(0.5 + 0.25 * (in[0] - in[1]));     // a* offset-encoded
  out[2] = (icFloatNumber)(0.5 + 0.25 * (in[1] - in[2]));     // b* offset-encoded
}

// A rough inverse of the above. It does not have to invert exactly -- every
// assertion in this file compares two chains built from the *same* CLUTs, so
// LUT round-trip error cancels and only the PCS adjustment can differ.
static void labToCmykCell(const double *in, icFloatNumber *out)
{
  const double da = in[1] - 0.5;
  const double db = in[2] - 0.5;
  double v[4];
  v[0] = 0.5 + 2.0 * da;
  v[1] = 0.5 - 2.0 * da + 2.0 * db;
  v[2] = 0.5 - 2.0 * db;
  v[3] = 1.0 - in[0];
  for (int i = 0; i < 4; i++) {
    if (v[i] < 0.0) v[i] = 0.0;
    if (v[i] > 1.0) v[i] = 1.0;
    out[i] = (icFloatNumber)v[i];
  }
}

// Build an nIn-in / nOut-out LUT tag with identity curves either side of a CLUT
// filled by pFill.
//
// 'mft2' keeps the matrix on the input side so B is the input curve array,
// while 'mAB ' clears m_bInputMatrix so A is. Filling the wrong one writes
// nIn pointers into an nOut-pointer array.
template <class TTag>
static TTag *buildLut(icUInt8Number nGrid,
                      icUInt8Number nIn, icUInt8Number nOut,
                      icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace,
                      void (*pFill)(const double *, icFloatNumber *))
{
  TTag *pTag = new TTag();
  pTag->Init(nIn, nOut);
  pTag->SetColorSpaces(srcSpace, dstSpace);

  const bool bInputIsB = pTag->IsInputB();

  LPIccCurve *pIn = bInputIsB ? pTag->NewCurvesB() : pTag->NewCurvesA();
  for (int i = 0; i < (int)nIn; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    pIn[i] = pCurve;
  }

  CIccCLUT *pCLUT = new CIccCLUT(nIn, (icUInt16Number)nOut);
  if (!pCLUT->Init(nGrid)) {
    delete pCLUT;
    delete pTag;
    return NULL;
  }
  icFloatNumber *pData = pCLUT->GetData(0);
  for (icUInt32Number p = 0; p < pCLUT->NumPoints(); p++) {
    double in[4] = { 0.0, 0.0, 0.0, 0.0 };
    icUInt32Number rem = p;
    for (int ax = (int)nIn - 1; ax >= 0; ax--) {   // axis 0 varies slowest
      in[ax] = (double)(rem % nGrid) / (double)(nGrid - 1);
      rem /= nGrid;
    }
    pFill(in, &pData[(size_t)p * nOut]);
  }
  pTag->SetCLUT(pCLUT);

  LPIccCurve *pOut = bInputIsB ? pTag->NewCurvesA() : pTag->NewCurvesB();
  for (int i = 0; i < (int)nOut; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    pOut[i] = pCurve;
  }
  return pTag;
}

// AToB0/AToB1 and BToA0/BToA1 get identical CLUT content on purpose. A
// perceptual chain and a relative chain then differ by the PCS adjustment and
// by nothing else, which is what makes them comparable.
//
// Both directions are attached because a CMYK -> Lab -> CMYK chain needs BToA
// for its second xform, and a Lab -> CMYK chain needs it for its only one.
template <class TAtoB, class TBtoA>
static void attachLutPair(CIccProfile &p)
{
  p.AttachTag(icSigAToB0Tag, buildLut<TAtoB>(5, 4, 3, icSigCmykData, icSigLabData, cmykToLabCell));
  p.AttachTag(icSigAToB1Tag, buildLut<TAtoB>(5, 4, 3, icSigCmykData, icSigLabData, cmykToLabCell));
  p.AttachTag(icSigBToA0Tag, buildLut<TBtoA>(9, 3, 4, icSigLabData, icSigCmykData, labToCmykCell));
  p.AttachTag(icSigBToA1Tag, buildLut<TBtoA>(9, 3, 4, icSigLabData, icSigCmykData, labToCmykCell));
}

// v2 output CMYK, Lab PCS. IsVersion2() is what turns on the perceptual
// black-point adjustment, so this is the fixture for the perceptual cases.
void buildV2CmykOutputProfile(CIccProfile &p)
{
  p.InitHeader();
  p.m_Header.version     = icVersionNumberV2_1;
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace  = icSigCmykData;
  p.m_Header.pcs         = icSigLabData;
  attachLutPair<CIccTagLut16, CIccTagLut16>(p);
  attachRequiredTags(p, "pcs adjust placement v2 fixture");
}

// v4 output CMYK, Lab PCS. HasPerceptualHandling() is true here, so perceptual
// gets no adjustment and only absolute intent does -- the control.
void buildV4CmykOutputProfile(CIccProfile &p)
{
  p.InitHeader();
  p.m_Header.version     = icVersionNumberV4_3;
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace  = icSigCmykData;
  p.m_Header.pcs         = icSigLabData;
  attachLutPair<CIccTagLutAtoB, CIccTagLutBtoA>(p);
  attachRequiredTags(p, "pcs adjust placement v4 fixture");
}

// --- cases ----------------------------------------------------------------

// The size of the adjustment is the whole oracle: if it is smaller than the
// tolerance band, every other assertion in this file is vacuous.
static void adjustmentIsLargerThanTheToleranceBand()
{
  const double perceptualScale = 1.0 - icPerceptualRefBlackY / icPerceptualRefWhiteY;
  check(std::fabs(1.0 - perceptualScale) > 100.0 * kTol,
        "v2 perceptual adjustment is at least 100x the 1e-5 tolerance band");
}

// Trailing PCS edge: CMYK -> Lab, one profile.
static void trailingEdgeAdjustmentApplies(icRenderingIntent nIntent,
                                          bool bExpectAdjust,
                                          const char *label)
{
  CIccProfile v2;
  buildV2CmykOutputProfile(v2);

  icFloatNumber cmyk[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
  icFloatNumber withIntent[3] = { 0 }, relative[3] = { 0 };

  {
    CIccCmm cmm(icSigCmykData, icSigLabData, true);
    check(cmm.AddXform(v2, nIntent) == icCmmStatOk, "trailing: AddXform");
    check(cmm.Begin() == icCmmStatOk, "trailing: Begin");
    check(cmm.Apply(withIntent, cmyk) == icCmmStatOk, "trailing: Apply");
  }
  {
    // icRelativeColorimetric on a v2 profile takes no adjustment, so it is the
    // unadjusted reference the adjusted result must differ from.
    CIccCmm cmm(icSigCmykData, icSigLabData, true);
    check(cmm.AddXform(v2, icRelativeColorimetric) == icCmmStatOk, "trailing: ref AddXform");
    check(cmm.Begin() == icCmmStatOk, "trailing: ref Begin");
    check(cmm.Apply(relative, cmyk) == icCmmStatOk, "trailing: ref Apply");
  }

  const bool moved = !closeRel(withIntent[0], relative[0], kTol);
  if (bExpectAdjust)
    check(moved, label);
  else
    check(!moved, label);
}

// Interior PCS connection: CMYK -> Lab -> CMYK, two profiles, same intent both
// ways. The two adjustments are exact inverses and adjacent in the PCS, so the
// round trip must equal the same round trip run with no adjustment at all.
static void interiorAdjustmentsCancel()
{
  CIccProfile v2a, v2b, v2c, v2d;
  buildV2CmykOutputProfile(v2a);
  buildV2CmykOutputProfile(v2b);
  buildV2CmykOutputProfile(v2c);
  buildV2CmykOutputProfile(v2d);

  icFloatNumber cmyk[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
  icFloatNumber perceptual[4] = { 0 }, relative[4] = { 0 };

  {
    CIccCmm cmm(icSigCmykData, icSigCmykData, true);
    check(cmm.AddXform(v2a, icPerceptual) == icCmmStatOk, "interior: AddXform 1");
    check(cmm.AddXform(v2b, icPerceptual) == icCmmStatOk, "interior: AddXform 2");
    check(cmm.Begin() == icCmmStatOk, "interior: Begin");
    check(cmm.Apply(perceptual, cmyk) == icCmmStatOk, "interior: Apply");
  }
  {
    CIccCmm cmm(icSigCmykData, icSigCmykData, true);
    check(cmm.AddXform(v2c, icRelativeColorimetric) == icCmmStatOk, "interior: ref AddXform 1");
    check(cmm.AddXform(v2d, icRelativeColorimetric) == icCmmStatOk, "interior: ref AddXform 2");
    check(cmm.Begin() == icCmmStatOk, "interior: ref Begin");
    check(cmm.Apply(relative, cmyk) == icCmmStatOk, "interior: ref Apply");
  }

  // Both chains use AToB0/BToA0 vs AToB1/BToA1 respectively, but this fixture
  // gives both intents the same CLUT, so the only possible difference is the
  // adjustment -- and it must cancel.
  bool same = true;
  for (int i = 0; i < 4; i++)
    same = same && closeRel(perceptual[i], relative[i], kTol);
  check(same,
        "interior: perceptual round trip equals relative round trip "
        "(the two PCS adjustments cancel)");
}

// Connect() reports the cancellation directly: with the same profile and intent
// on both sides, every pushed step folds away and Connect returns identity.
static void connectReportsIdentityWhenAdjustmentsCancel()
{
  CIccProfile *pIn  = new CIccProfile();
  CIccProfile *pOut = new CIccProfile();
  buildV2CmykOutputProfile(*pIn);
  buildV2CmykOutputProfile(*pOut);

  // CIccXform::Create takes ownership of the profile it is given.
  CIccXform *pFrom = CIccXform::Create(pIn,  true,  icPerceptual, icInterpTetrahedral);
  CIccXform *pTo   = CIccXform::Create(pOut, false, icPerceptual, icInterpTetrahedral);
  check(pFrom != NULL && pTo != NULL, "connect: xforms created");
  if (!pFrom || !pTo) { delete pFrom; delete pTo; return; }

  pFrom->Begin();
  pTo->Begin();

  // This is what CheckPCSConnections() does before calling Connect().
  pFrom->SetDstPCSConversion(false);
  pTo->SetSrcPCSConversion(false);

  CIccPcsXform pcs;
  const icStatusCMM rv = pcs.Connect(pFrom, pTo);
  check(rv == icCmmStatIdentityXform,
        "connect: matched perceptual adjustments fold to icCmmStatIdentityXform");

  delete pFrom;
  delete pTo;
}

int main(int /*argc*/, char ** /*argv*/)
{
  adjustmentIsLargerThanTheToleranceBand();

  trailingEdgeAdjustmentApplies(icPerceptual, true,
      "trailing edge: v2 perceptual result differs from the unadjusted reference");
  trailingEdgeAdjustmentApplies(icAbsoluteColorimetric, true,
      "trailing edge: absolute result differs from the unadjusted reference");
  trailingEdgeAdjustmentApplies(icRelativeColorimetric, false,
      "trailing edge: relative result is the unadjusted reference");

  interiorAdjustmentsCancel();
  connectReportsIdentityWhenAdjustmentsCancel();

  if (g_failures) {
    std::printf("\n%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
```

- [ ] **Step 2: Register the test in CMake**

In `Build/Cmake/Testing/CMakeLists.txt`, immediately after the closing `endfunction()` of `iccdev_add_v2_legacy_pcs_test` (around line 4880), add:

```cmake
# Characterization tests for PCS adjustment placement. Builds its own v2/v4
# CMYK fixtures in memory -- Testing/**/*.icc is gitignored and generated, so a
# tracked profile is not available on a fresh clone. Links IccProfLib alone.
function(iccdev_add_pcs_adjust_placement_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPcsAdjustPlacementTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcs-adjust-placement.cpp"
  )
  target_compile_features(iccPcsAdjustPlacementTest PRIVATE cxx_std_17)
  target_include_directories(iccPcsAdjustPlacementTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccPcsAdjustPlacementTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPcsAdjustPlacementTest)

  if(WIN32)
    add_test(
      NAME iccdev.pcs-adjust-placement
      COMMAND "$<TARGET_FILE:iccPcsAdjustPlacementTest>"
    )
  else()
    add_test(
      NAME iccdev.pcs-adjust-placement
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccPcsAdjustPlacementTest>"
    )
  endif()
  set_tests_properties(iccdev.pcs-adjust-placement PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;pcs;cmm;regression"
  )
  if(WIN32)
    set(_pcs_adjust_placement_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPcsAdjustPlacementTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcs_adjust_placement_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcs-adjust-placement PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcs_adjust_placement_windows_env_mods}"
    )
  endif()
endfunction()
```

Then add `iccdev_add_pcs_adjust_placement_test()` immediately after **both** existing `iccdev_add_v2_legacy_pcs_test()` call sites — one around line 6271 (indented, inside the guarded block) and one around line 6628 (unindented). Missing either leaves the test unregistered in one build configuration.

- [ ] **Step 3: Run the test to verify it passes on unmodified code**

```bash
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R pcs-adjust-placement --output-on-failure
```

Expected: PASS, all checks `ok:`.

This test characterizes existing behavior, so a failure here means the *test* is wrong, not the library. The most likely cause is a fixture that makes an adjustment vacuous — check that `attachRequiredTags` media white is not D50 (absolute cases) and that `buildV2CmykOutputProfile` really reports `icVersionNumberV2_1` (perceptual cases). Debug with the sizes: print `withIntent[0]` and `relative[0]` and confirm they differ by ~3.5e-3, not ~1e-8.

- [ ] **Step 4: Commit**

```bash
git add .github/ci/regression/pcs-adjust-placement.cpp Build/Cmake/Testing/CMakeLists.txt
git commit -m "test: characterize PCS adjustment behavior before moving it"
```

---

### Task 2: Make the leading-edge condition symmetric

`CheckPCSConnections()` builds a trailing-edge `CIccPcsXform` when the last xform reports `NeedAdjustPCS()`, but the leading-edge condition has no equivalent term. Without it, Task 4 would have nowhere to put a source-side adjustment when the caller's PCS already matches the profile's.

This task is a no-op behaviorally: `ConnectFirst()`'s adjustment branches are still gated on `NeedAdjustSrcPCS()`, which is still `false`. Any `CIccPcsXform` built by the new term collapses to `icCmmStatIdentityXform` and is deleted.

**Files:**
- Modify: `IccProfLib/IccCmm.cpp:9574`
- Test: `.github/ci/regression/pcs-adjust-placement.cpp` (existing, unchanged — it must keep passing)

**Interfaces:**
- Consumes: nothing.
- Produces: a leading-edge `CIccPcsXform` exists whenever `pToXform->NeedAdjustPCS()`, which Task 4 relies on.

- [ ] **Step 1: Make the change**

In `IccProfLib/IccCmm.cpp`, replace:

```cpp
    icColorSpaceSignature lastSpace = last->ptr->GetSrcSpace();
    if (!last->ptr->IsInput() && IsSpaceColorimetricPCS(lastSpace) && (GetSourceSpace() !=lastSpace || last->ptr->UseLegacyPCS())) {
```

with:

```cpp
    icColorSpaceSignature lastSpace = last->ptr->GetSrcSpace();
    // NeedAdjustPCS() mirrors the trailing-edge condition below. Without it a
    // chain whose source PCS already matches the profile's gets no leading edge
    // xform, leaving nowhere to put a source-side PCS adjustment.
    if (!last->ptr->IsInput() && IsSpaceColorimetricPCS(lastSpace) &&
        (last->ptr->NeedAdjustPCS() || GetSourceSpace() != lastSpace || last->ptr->UseLegacyPCS())) {
```

- [ ] **Step 2: Rebuild and run the characterization test**

```bash
MB="G:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe"
"$MB" out/vs2022-x64/IccProfLib/IccProfLib2.vcxproj //p:Configuration=Release //p:Platform=x64 //p:BuildProjectReferences=false //m //v:m //nologo
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R "pcs-adjust-placement|pcs-edge-metadata" --output-on-failure
```

Expected: both PASS. `pcs-edge-metadata` exercises `ConnectFirst()` directly and is the test most likely to catch a mistake here.

- [ ] **Step 3: Commit**

```bash
git add IccProfLib/IccCmm.cpp
git commit -m "fix: build a leading-edge CIccPcsXform when the xform needs a PCS adjust"
```

---

### Task 3: Introduce virtual per-side adjustment predicates

`CheckSrcAbs()` acts only when `m_bAdjustPCS && !m_bInput`; `CheckDstAbs()` only when `m_bAdjustPCS && m_bInput`. `CIccXformMpe` and `CIccXformNamedColor` add further conditions inside `Apply()`. All of it has to be answerable at `Begin()` time. This task adds the predicates and their overrides. Nothing consumes them yet, so behavior is unchanged.

**Files:**
- Modify: `IccProfLib/IccCmm.h` (near line 515, and in the `CIccXformMpe` / `CIccXformNamedColor` class declarations)
- Test: `.github/ci/regression/pcs-adjust-placement.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `virtual bool CIccXform::NeedsSrcPcsAdjust() const` and `virtual bool CIccXform::NeedsDstPcsAdjust() const`, overridden in `CIccXformMpe` and `CIccXformNamedColor`. Task 4 replaces every `NeedAdjustSrcPCS()` / `NeedAdjustDstPCS()` call in `Connect*` with these.

- [ ] **Step 1: Write the failing test**

Append to `.github/ci/regression/pcs-adjust-placement.cpp`, and add the calls to `main()` before the `g_failures` check:

```cpp
// The predicates must reproduce exactly what CheckSrcAbs/CheckDstAbs test:
// source side only for output xforms, destination side only for input xforms.
// An abstract-class xform has PCS on both sides but must still adjust on
// exactly one, and this is the assertion that pins it.
static void perSideAdjustPredicatesFollowDirection()
{
  CIccProfile *pIn  = new CIccProfile();
  CIccProfile *pOut = new CIccProfile();
  buildV2CmykOutputProfile(*pIn);
  buildV2CmykOutputProfile(*pOut);

  CIccXform *pInput  = CIccXform::Create(pIn,  true,  icPerceptual, icInterpTetrahedral);
  CIccXform *pOutput = CIccXform::Create(pOut, false, icPerceptual, icInterpTetrahedral);
  check(pInput != NULL && pOutput != NULL, "predicates: xforms created");
  if (!pInput || !pOutput) { delete pInput; delete pOutput; return; }

  pInput->Begin();
  pOutput->Begin();

  check(pInput->NeedAdjustPCS(), "predicates: v2 perceptual input xform needs an adjust");
  check(pOutput->NeedAdjustPCS(), "predicates: v2 perceptual output xform needs an adjust");

  check(!pInput->NeedsSrcPcsAdjust(),
        "predicates: an input xform never adjusts on its source side");
  check(pInput->NeedsDstPcsAdjust(),
        "predicates: an input xform adjusts on its destination side");

  check(pOutput->NeedsSrcPcsAdjust(),
        "predicates: an output xform adjusts on its source side");
  check(!pOutput->NeedsDstPcsAdjust(),
        "predicates: an output xform never adjusts on its destination side");

  delete pInput;
  delete pOutput;
}

// A v4 profile has HasPerceptualHandling(), so perceptual sets no adjustment at
// all. Guards against a predicate that returns true unconditionally.
static void perSideAdjustPredicatesAreOffWhenNothingToAdjust()
{
  CIccProfile *pOut = new CIccProfile();
  buildV4CmykOutputProfile(*pOut);

  CIccXform *pOutput = CIccXform::Create(pOut, false, icPerceptual, icInterpTetrahedral);
  check(pOutput != NULL, "predicates: v4 xform created");
  if (!pOutput) return;
  pOutput->Begin();

  check(!pOutput->NeedAdjustPCS(), "predicates: v4 perceptual needs no adjust");
  check(!pOutput->NeedsSrcPcsAdjust(), "predicates: v4 perceptual source side off");
  check(!pOutput->NeedsDstPcsAdjust(), "predicates: v4 perceptual destination side off");

  delete pOutput;
}
```

Add to `main()`:

```cpp
  perSideAdjustPredicatesFollowDirection();
  perSideAdjustPredicatesAreOffWhenNothingToAdjust();
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
```

Expected: compile error — `'NeedsSrcPcsAdjust': is not a member of 'CIccXform'`.

- [ ] **Step 3: Add the base predicates**

In `IccProfLib/IccCmm.h`, immediately after the existing `NeedAdjustDstPCS()` line (around line 517), add:

```cpp
  /// True when this xform's PCS adjustment applies to values entering it.
  /// Mirrors the condition inside CheckSrcAbs(): only an output (PCS->device)
  /// xform adjusts on the way in. Virtual so subclasses whose Apply() carried
  /// extra conditions can answer at Begin() time, which is when
  /// CIccPcsXform::Connect() has to decide whether to push the steps.
  virtual bool NeedsSrcPcsAdjust() const { return m_bAdjustPCS && !m_bInput; }

  /// True when this xform's PCS adjustment applies to values leaving it.
  /// Mirrors the condition inside CheckDstAbs(): only an input (device->PCS)
  /// xform adjusts on the way out.
  virtual bool NeedsDstPcsAdjust() const { return m_bAdjustPCS && m_bInput; }
```

- [ ] **Step 4: Override in `CIccXformMpe`**

`CIccXformMpe::Apply()` guards the adjustment with `m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent` — B2D3 / D2B3 tags already carry absolute colorimetry and must not be adjusted again. Move that condition into the predicates. In the `CIccXformMpe` class declaration in `IccProfLib/IccCmm.h`, in its `public:` section, add:

```cpp
  /// B2D3/D2B3 tags are already absolute, so they take no PCS adjustment.
  /// This condition used to live inside Apply(); CIccPcsXform::Connect() needs
  /// the answer at Begin() time instead.
  virtual bool NeedsSrcPcsAdjust() const
  {
    return CIccXform::NeedsSrcPcsAdjust() &&
           (m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent);
  }
  virtual bool NeedsDstPcsAdjust() const
  {
    return CIccXform::NeedsDstPcsAdjust() &&
           (m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent);
  }
```

- [ ] **Step 5: Override in `CIccXformNamedColor`**

`CIccXformNamedColor::Apply()` only reaches `CheckSrcAbs()` inside `if (IsSrcPCS())` and, in the tag-array branch, only when the source space is *not* a spectral PCS; it only reaches `CheckDstAbs()` inside `if (IsDestPCS())`. In the `CIccXformNamedColor` class declaration in `IccProfLib/IccCmm.h`, in its `public:` section, add:

```cpp
  /// Named-colour lookups only take a PCS adjustment when the side in question
  /// really is a colorimetric PCS. A spectral PCS source is matched against
  /// spectral data and never goes through AdjustPCS().
  virtual bool NeedsSrcPcsAdjust() const
  {
    return CIccXform::NeedsSrcPcsAdjust() && IsSrcPCS() && !IsSpaceSpectralPCS(m_nSrcSpace);
  }
  virtual bool NeedsDstPcsAdjust() const
  {
    return CIccXform::NeedsDstPcsAdjust() && IsDestPCS();
  }
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R pcs-adjust-placement --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add IccProfLib/IccCmm.h .github/ci/regression/pcs-adjust-placement.cpp
git commit -m "feat: add per-side virtual PCS adjust predicates answerable at Begin time"
```

---

### Task 4: Move the adjustment into CIccPcsXform at every edge

This is the behavior flip. `CheckPCSConnections()` clears the suppression flag on the first and last xforms the same way it already does for interior pairs, and the `Connect*` guards switch to the Task 3 predicates so those branches finally run.

Both halves must land together. Clearing the flags without switching the guards drops the adjustment; switching the guards without clearing the flags applies it twice.

**Files:**
- Modify: `IccProfLib/IccCmm.cpp` — 15 guard sites (lines 2349, 2356, 2375, 2382, 2408, 2415, 2431, 2438, 2469, 2490, 2538, 2559, 2604, 2625 in `Connect()`; 2728, 2754, 2760 in `ConnectFirst()`; 2831 in `ConnectLast()`), plus the two edge blocks in `CheckPCSConnections()`
- Test: `.github/ci/regression/pcs-adjust-placement.cpp`

**Interfaces:**
- Consumes: `NeedsSrcPcsAdjust()` / `NeedsDstPcsAdjust()` from Task 3; the leading-edge `CIccPcsXform` from Task 2.
- Produces: after `CIccCmm::Begin()`, no xform in the chain reports both `NeedAdjustPCS()` and an uncleared conversion flag. Task 5 relies on that to delete the in-xform path.

- [ ] **Step 1: Write the failing test**

Append to `.github/ci/regression/pcs-adjust-placement.cpp`, and add the calls to `main()`:

```cpp
// Reaching into the built chain is the only way to assert *where* the
// adjustment happens. m_Xforms is protected on CIccCmm, so subclass it.
//
// Note these methods are non-const: NeedAdjustPCS() and friends are non-const
// members of CIccXform.
class CmmProbe : public CIccCmm
{
public:
  CmmProbe(icColorSpaceSignature nSrc, icColorSpaceSignature nDst, bool bInput)
    : CIccCmm(nSrc, nDst, bInput) {}

  // Device xforms that need an adjustment and have NOT handed it over.
  //
  // "Handed over" means CheckPCSConnections() cleared the xform's conversion
  // flag, which is precisely what the flag-based NeedAdjustSrcPCS() /
  // NeedAdjustDstPCS() report: they are m_bAdjustPCS && !m_b*PcsConversion, so
  // true means "the CIccPcsXform owns this side". An xform that needs an
  // adjustment but reports neither is still doing it inside Apply().
  //
  // Task 5 deletes those two predicates; this method is rewritten there.
  int unhandedAdjustCount()
  {
    int n = 0;
    for (CIccXformList::iterator i = m_Xforms->begin(); i != m_Xforms->end(); i++) {
      if (i->ptr->GetXformType() == icXformTypePCS)
        continue;
      if (!i->ptr->NeedAdjustPCS())
        continue;
      if (!i->ptr->NeedAdjustSrcPCS() && !i->ptr->NeedAdjustDstPCS())
        n++;
    }
    return n;
  }

  int pcsXformCount()
  {
    int n = 0;
    for (CIccXformList::iterator i = m_Xforms->begin(); i != m_Xforms->end(); i++) {
      if (i->ptr->GetXformType() == icXformTypePCS)
        n++;
    }
    return n;
  }

  int adjustingXformCount()
  {
    int n = 0;
    for (CIccXformList::iterator i = m_Xforms->begin(); i != m_Xforms->end(); i++) {
      if (i->ptr->GetXformType() != icXformTypePCS && i->ptr->NeedAdjustPCS())
        n++;
    }
    return n;
  }
};

// The core claim of this refactor, asserted structurally rather than
// numerically: after Begin(), no device xform is left holding an adjustment.
static void noXformPerformsItsOwnAdjustment(icColorSpaceSignature nSrc,
                                            icColorSpaceSignature nDst,
                                            bool bInput,
                                            const char *label)
{
  CIccProfile v2;
  buildV2CmykOutputProfile(v2);

  CmmProbe cmm(nSrc, nDst, bInput);
  check(cmm.AddXform(v2, icPerceptual) == icCmmStatOk, "placement: AddXform");
  check(cmm.Begin() == icCmmStatOk, "placement: Begin");
  check(cmm.adjustingXformCount() >= 1,
        "placement: the fixture really does need an adjustment");
  check(cmm.pcsXformCount() >= 1, "placement: a CIccPcsXform was inserted");
  check(cmm.unhandedAdjustCount() == 0, label);
}
```

Add to `main()`:

```cpp
  noXformPerformsItsOwnAdjustment(icSigCmykData, icSigLabData, true,
      "trailing PCS edge: the device xform no longer adjusts, the PcsXform does");
  noXformPerformsItsOwnAdjustment(icSigLabData, icSigCmykData, false,
      "leading PCS edge: the device xform no longer adjusts, the PcsXform does");
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R pcs-adjust-placement --output-on-failure
```

Expected: FAIL — two `FAIL: ... the device xform no longer adjusts` lines, because at both edges the xform still holds the adjustment.

- [ ] **Step 3: Clear the suppression flags at both edges**

In `IccProfLib/IccCmm.cpp`, in `CheckPCSConnections()`, inside the leading-edge block, immediately before `rv = pPcs->ConnectFirst(last->ptr, GetSourceSpace());`, add:

```cpp
      // The CIccPcsXform now owns the source-side adjustment, exactly as the
      // interior loop below hands over an interior one.
      last->ptr->SetSrcPCSConversion(false);
```

In the trailing-edge block, immediately before `rv = pPcs->ConnectLast(last->ptr, GetDestSpace());`, add:

```cpp
      // The CIccPcsXform now owns the destination-side adjustment.
      last->ptr->SetDstPCSConversion(false);
```

- [ ] **Step 4: Switch every Connect guard to the new predicates**

In `IccProfLib/IccCmm.cpp`, replace every call to `NeedAdjustDstPCS()` with `NeedsDstPcsAdjust()` and every call to `NeedAdjustSrcPCS()` with `NeedsSrcPcsAdjust()`. There are 18 call sites, all inside `Connect()`, `ConnectFirst()` and `ConnectLast()`:

```bash
sed -i 's/NeedAdjustDstPCS()/NeedsDstPcsAdjust()/g; s/NeedAdjustSrcPCS()/NeedsSrcPcsAdjust()/g' IccProfLib/IccCmm.cpp
```

Verify the count and that nothing outside the three functions changed:

```bash
git --no-pager diff --stat IccProfLib/IccCmm.cpp
grep -c "NeedsSrcPcsAdjust()\|NeedsDstPcsAdjust()" IccProfLib/IccCmm.cpp   # expect 18
grep -c "NeedAdjustSrcPCS()\|NeedAdjustDstPCS()" IccProfLib/IccCmm.cpp     # expect 0
```

- [ ] **Step 5: Run the test to verify it passes**

```bash
MB="G:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe"
"$MB" out/vs2022-x64/IccProfLib/IccProfLib2.vcxproj //p:Configuration=Release //p:Platform=x64 //p:BuildProjectReferences=false //m //v:m //nologo
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R "pcs-adjust-placement|pcs-edge-metadata|v2-legacy-pcs|xform-abstorel" --output-on-failure
```

Expected: all PASS. The Task 1 numeric assertions passing here is the proof that the move preserved behavior; the new placement assertions passing is the proof that it happened.

If `xform-abstorel-adjust` fails: it drives a single-profile device→PCS chain, which is a trailing PCS edge, so it is directly on this path. Compare its expected value against what the chain now produces — a difference at ~1e-7 means the tolerance in *that* test is too tight for the reassociated math and the test should be widened; a difference at ~1e-3 or larger means the adjustment was dropped or doubled and the bug is in Steps 3–4.

- [ ] **Step 6: Commit**

```bash
git add IccProfLib/IccCmm.cpp .github/ci/regression/pcs-adjust-placement.cpp
git commit -m "refactor: CIccPcsXform performs PCS adjustments at chain edges too"
```

---

### Task 5: Delete the in-xform adjustment path

With every adjustment now owned by a `CIccPcsXform`, the 16 guarded call sites are dead. Remove them and the machinery that fed them.

> **PRECONDITION — established by Task 4's review, must be settled before any deletion.**
>
> Task 4's stated postcondition — "after `Begin()`, no xform reports both `NeedAdjustPCS()` and an uncleared conversion flag" — does **not** hold for spectral-PCS ports, and this task is written assuming it does.
>
> `CIccXform::GetDstSpace()` returns a *spectral* signature when `m_bUseSpectralPCS && m_pProfile->m_Header.spectralPCS` ([IccCmm.cpp:1878-1882](../../../IccProfLib/IccCmm.cpp#L1878-L1882)), but `CheckPCSConnections()`'s edge blocks gate on `IsSpaceColorimetricPCS(GetDstSpace())` (:9628, :9703). Meanwhile `CIccXform::Begin()` sets `m_bAdjustPCS` from `m_pProfile->m_Header.pcs` — which stays Lab/XYZ on a v5 spectral profile — with no spectral exclusion (:1611-1615, :1645-1648). So such an xform keeps `m_bAdjustPCS` true **and** both conversion flags set, and still performs the adjustment inside `Apply()` today.
>
> Reachability, narrowed: absolute intent only (a v5 profile is not `IsVersion2()` and does have `HasPerceptualHandling()`, so the perceptual path cannot fire), media white ≠ illuminant, xform at a chain edge, and for `CIccXformMpe` additionally `m_nTagIntent != icAbsoluteColorimetric`. A v5 spectral profile applied at absolute intent is a real configuration.
>
> No fixture in the suite is spectral, so **nothing would fail** if this adjustment were deleted.
>
> **Required first step of this task:** write a test that constructs this case and pins whether the adjustment happens today.
> - **If it is live: STOP and report.** Do not delete the path for it. Whether a spectral pixel should receive an XYZ media-white scale and offset on its first three samples is a colour-science question for the repository owner, and the spec's non-goals explicitly exclude "changing what any adjustment computes". Deleting it would be a behaviour change wearing a refactor's clothes.
> - **If it is provably unreachable:** proceed with the deletion and record the proof in the task report.
>
> Task 3 already had to add `!IsSpaceSpectralPCS(...)` to the `CIccXformNamedColor` overrides for the same underlying reason, which is independent evidence that the base predicates do not cover spectral ports.

**Files:**
- Modify: `IccProfLib/IccCmm.cpp` (16 call sites at lines 5753, 5791, 6081, 6123, 6496, 6579, 6877, 6937, 7377, 7463, 7701, 7740, 7826, 7855, 8536, 8582; `CIccXform::CIccXform` at 458-459; `CIccCmm::Begin()`; `CIccNamedColorCmm::Begin()`; `CheckPCSConnections()`)
- Modify: `IccProfLib/IccCmm.h` (lines 513-517, 556-558, 530-533)
- Modify: `.github/ci/regression/pcs-adjust-placement.cpp` (`CmmProbe::unhandedAdjustCount()` reads two predicates this task deletes — see Step 4; every other assertion must keep passing untouched)

**Interfaces:**
- Consumes: the guarantee from Task 4 that no xform is left holding an adjustment.
- Produces: `CIccXform` no longer has `m_bSrcPcsConversion`, `m_bDstPcsConversion`, `SetSrcPCSConversion()`, `SetDstPCSConversion()`, `NeedAdjustSrcPCS()` or `NeedAdjustDstPCS()`. `NeedAdjustPCS()` survives — `CheckPCSConnections()` still uses it in both edge conditions.

- [ ] **Step 1: Remove the 16 call sites**

Each is a two-line `if (m_bSrcPcsConversion)` / `SrcPixel = CheckSrcAbs(pApply, SrcPixel);` or `if (m_bDstPcsConversion)` / `CheckDstAbs(DstPixel);` pair. Delete both lines of each pair — the guard *and* the call.

Two need care because the surrounding structure changes shape:

In `CIccXformMpe::Apply()` (around line 8534), the enclosing intent test exists only to guard the removed call, so it goes with it:

```cpp
  icFloatNumber temp[3];
  if (!m_bInput || m_bPcsAdjustXform) { //PCS comming in?
    //Since MPE tags use "real" values for PCS we need to convert from
    //internal encoding used by IccProfLib
    switch (GetSrcSpace()) {
```

and near line 8580:

```cpp
      default:
        break;
    }
  }
}
```

In `CIccXformNamedColor::Apply()`, the four sites sit inside `if (IsSrcPCS())` / `if (IsDestPCS())` blocks that do other work. Delete only the two-line pairs; leave every enclosing block intact.

Verify:

```bash
# 2 = the two constructor initialisers, removed in Step 2.
grep -c "m_bSrcPcsConversion\|m_bDstPcsConversion" IccProfLib/IccCmm.cpp   # expect 2
# 4 = two function definitions plus their two "* Name: CIccXform::Check..."
# Doxygen header lines. grep -c counts lines, not matches.
grep -c "CheckSrcAbs\|CheckDstAbs" IccProfLib/IccCmm.cpp                   # expect 4
```

- [ ] **Step 2: Remove the flags and the old predicates**

In `IccProfLib/IccCmm.cpp`, delete these two lines from the `CIccXform` constructor (around 458):

```cpp
  m_bSrcPcsConversion = true;
  m_bDstPcsConversion = true;
```

In `IccProfLib/IccCmm.h`, delete:

```cpp
  void SetSrcPCSConversion(bool bPcsConvert) { m_bSrcPcsConversion = bPcsConvert; }
  void SetDstPCSConversion(bool bPcsConvert) { m_bDstPcsConversion = bPcsConvert; }
```

```cpp
  bool NeedAdjustSrcPCS() { return m_bAdjustPCS && !m_bSrcPcsConversion; }
  bool NeedAdjustDstPCS() { return m_bAdjustPCS && !m_bDstPcsConversion; }
```

```cpp
  //Temporary field
  bool m_bSrcPcsConversion;
  bool m_bDstPcsConversion;
```

Keep `bool NeedAdjustPCS() { return m_bAdjustPCS; }`.

- [ ] **Step 3: Remove the flag clearing from `CheckPCSConnections()`**

In `IccProfLib/IccCmm.cpp`, delete **all four** `SetSrcPCSConversion(false)` / `SetDstPCSConversion(false)` calls: the single leading-edge call and the single trailing-edge call added by Task 4, plus the interior pair at what was line 9607. Step 2 deletes the setters from the header, so a leftover call is a compile error rather than a silent bug. Replace the interior pair with a comment so the next reader knows why nothing is set:

```cpp
        // No handover needed: CIccPcsXform performs every PCS adjustment, and
        // CIccXform::Apply() performs none.
```

- [ ] **Step 4: Rewrite the probe method that depended on the deleted predicates**

`CmmProbe::unhandedAdjustCount()` in `.github/ci/regression/pcs-adjust-placement.cpp` reads `NeedAdjustSrcPCS()` / `NeedAdjustDstPCS()`, which Step 2 just deleted, so the test no longer compiles. The handover it detected is now true by construction — there is no in-xform path left to hand over *from* — so the surviving invariant is structural. Replace the method with:

```cpp
  // Task 5 removed the in-xform adjustment path, so "handed over" is no longer
  // a state an xform can be in. What remains checkable is that an adjustment
  // exists and a CIccPcsXform is present to carry it; the numeric assertions
  // in this file carry the rest of the weight.
  int unhandedAdjustCount()
  {
    return 0;
  }
```

Delete `unhandedAdjustCount()` outright and drop its `check()` call instead if you prefer — but then `noXformPerformsItsOwnAdjustment()` must keep its other two assertions, which are the ones that still mean something.

- [ ] **Step 5: Deprecate the three helpers**

In `IccProfLib/IccCmm.h`, in the `protected:` section of `CIccXform`, replace the three declarations with:

```cpp
  /// Deprecated. CIccPcsXform performs all PCS adjustments as of the
  /// CheckSrcAbs/CheckDstAbs retirement; IccProfLib no longer calls these.
  /// They are retained because they are protected and a third-party CIccXform
  /// subclass may call them from its own Apply(). Such a subclass now applies
  /// the adjustment twice and should stop calling them.
  const icFloatNumber *CheckSrcAbs(CIccApplyXform *pApply, const icFloatNumber *Pixel) const;
  void CheckDstAbs(icFloatNumber *Pixel) const;
  void AdjustPCS(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;
```

Leave their definitions in `IccCmm.cpp` untouched.

- [ ] **Step 6: Document `bUsePCSConversions` as ignored**

In `IccProfLib/IccCmm.h`, above the `CheckPCSConnections()` declaration (around line 1911), add:

```cpp
  /// bUsePCSConversions is ignored. It used to select an in-xform adjustment
  /// path that no longer exists; CIccPcsXform performs every PCS adjustment.
  /// The parameter is retained for source compatibility.
```

Add the same comment above the `Begin()` declarations on `CIccCmm` and `CIccNamedColorCmm`. In `IccCmm.cpp`, mark the parameter unused at the top of `CheckPCSConnections()`:

```cpp
  (void)bUsePCSConversions;  // see header: retained for source compatibility
```

and delete the `!bUsePCSConversions &&` term from the interior condition, so the interior `CIccPcsXform` is built unconditionally for colorimetric PCS links.

- [ ] **Step 7: Build and run the full CMM test set**

```bash
MB="G:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe"
"$MB" out/vs2022-x64/IccProfLib/IccProfLib2.vcxproj //p:Configuration=Release //p:Platform=x64 //p:BuildProjectReferences=false //m //v:m //nologo
cmake --build out/plan-tests -j
ctest --test-dir out/plan-tests -L cmm --output-on-failure
ctest --test-dir out/plan-tests -R "pcs-|v2-legacy|xform-abstorel|cmmsearch|rangemap" --output-on-failure
```

Expected: all PASS.

- [ ] **Step 8: Commit**

```bash
git add IccProfLib/IccCmm.cpp IccProfLib/IccCmm.h .github/ci/regression/pcs-adjust-placement.cpp
git commit -m "refactor: retire the in-xform PCS adjustment path"
```

---

### Task 6: Pin XYZ-PCS behavior and record the clip's real reach

`AdjustPCS()` ends with `CIccPCSUtil::NegClip` on each component when the PCS is XYZ (guarded by `SAMPLEICC_NOCLIPLABTOXYZ`). The pushed `CIccPcsStep` chain is pure affine and does not clamp. Per spec R8 no clip step is added — a non-affine step in the middle would block the cancellation this refactor exists to enable.

**Scope correction from the pre-flight scan (Ruling 2).** That clip is only reachable when the affine can drive a non-negative input negative, which requires a *negative* offset. The built-in adjustments cannot produce one: the absolute-intent path sets `m_PCSOffset` to all zeros with a positive scale, and the v2-perceptual path sets a positive offset. A negative offset arises only from an `IIccAdjustPCSXform` hint with `Scale > 1` — which is `CIccApplyBPC`, and Task 7 measures that path directly.

So this task does **not** try to demonstrate a clipping delta on the built-in adjustments; there isn't one to demonstrate. It asserts that an XYZ-PCS chain still applies its adjustment after the move, and records the reachability finding in the design note so R8's real blast radius is on paper. Do not add an assertion claiming a negative-XYZ delta — it would not fire, and a test that cannot fail is worse than no test.

**Files:**
- Modify: `.github/ci/regression/pcs-adjust-placement.cpp`

**Interfaces:**
- Consumes: `check()`, `closeRel()`, `attachRequiredTags()` from Task 1.
- Produces: nothing consumed later.

- [ ] **Step 1: Write the test**

Append to `.github/ci/regression/pcs-adjust-placement.cpp` and add the call to `main()`:

```cpp
// An XYZ-PCS matrix/TRC profile: the only PCS shape where AdjustPCS() used to
// clamp negatives. See spec R8 -- the CIccPcsStep chain is pure affine, so a
// negative component that used to be clamped to zero now survives.
static void buildXyzPcsRgbProfile(CIccProfile &p)
{
  p.InitHeader();
  p.m_Header.version     = icVersionNumberV2_1;
  p.m_Header.deviceClass = icSigDisplayClass;
  p.m_Header.colorSpace  = icSigRgbData;
  p.m_Header.pcs         = icSigXYZData;

  static const struct { icSignature sig; double x, y, z; } kCols[3] = {
    { icSigRedMatrixColumnTag,   0.4360, 0.2225, 0.0139 },
    { icSigGreenMatrixColumnTag, 0.3851, 0.7169, 0.0971 },
    { icSigBlueMatrixColumnTag,  0.1431, 0.0606, 0.7139 },
  };
  for (int i = 0; i < 3; i++) {
    CIccTagXYZ *pCol = new CIccTagXYZ(1);
    (*pCol)[0].X = icDtoF((icFloatNumber)kCols[i].x);
    (*pCol)[0].Y = icDtoF((icFloatNumber)kCols[i].y);
    (*pCol)[0].Z = icDtoF((icFloatNumber)kCols[i].z);
    p.AttachTag(kCols[i].sig, pCol);
  }

  static const icSignature kTrc[3] = {
    icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag
  };
  for (int i = 0; i < 3; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    p.AttachTag(kTrc[i], pCurve);
  }

  attachRequiredTags(p, "pcs adjust placement XYZ PCS fixture");
}

// The contract: an XYZ PCS chain still runs, still applies the adjustment, and
// stays within the tolerance band of the unadjusted reference by the expected
// margin. What it deliberately does NOT assert is that output is non-negative.
static void xyzPcsChainStillAdjusts()
{
  CIccProfile rgb;
  buildXyzPcsRgbProfile(rgb);

  icFloatNumber dev[3] = { 0.02f, 0.03f, 0.04f };
  icFloatNumber absolute[3] = { 0 }, relative[3] = { 0 };

  {
    CIccCmm cmm(icSigRgbData, icSigXYZData, true);
    check(cmm.AddXform(rgb, icAbsoluteColorimetric) == icCmmStatOk, "xyzpcs: AddXform abs");
    check(cmm.Begin() == icCmmStatOk, "xyzpcs: Begin abs");
    check(cmm.Apply(absolute, dev) == icCmmStatOk, "xyzpcs: Apply abs");
  }
  {
    CIccCmm cmm(icSigRgbData, icSigXYZData, true);
    check(cmm.AddXform(rgb, icRelativeColorimetric) == icCmmStatOk, "xyzpcs: AddXform rel");
    check(cmm.Begin() == icCmmStatOk, "xyzpcs: Begin rel");
    check(cmm.Apply(relative, dev) == icCmmStatOk, "xyzpcs: Apply rel");
  }

  check(!closeRel(absolute[0], relative[0], kTol),
        "xyzpcs: absolute intent still applies its media-white adjustment");

  // Record the values so a reviewer can see the clipping delta rather than
  // having to reason about it. Not an assertion.
  std::printf("info: XYZ PCS absolute = %.9f %.9f %.9f\n",
              (double)absolute[0], (double)absolute[1], (double)absolute[2]);
  std::printf("info: XYZ PCS relative = %.9f %.9f %.9f\n",
              (double)relative[0], (double)relative[1], (double)relative[2]);
}
```

Add to `main()`:

```cpp
  xyzPcsChainStillAdjusts();
```

- [ ] **Step 2: Run the test**

```bash
cmake --build out/plan-tests --target iccPcsAdjustPlacementTest -j
ctest --test-dir out/plan-tests -R pcs-adjust-placement --output-on-failure --verbose
```

Expected: PASS, with the two `info:` lines visible in the output.

- [ ] **Step 3: Record the delta in the design note**

Create `docs/pcs-adjustment-placement.md`:

```markdown
# PCS adjustment placement

As of the `CheckSrcAbs`/`CheckDstAbs` retirement, every PCS adjustment --
absolute-colorimetric media-white scaling, the v2-perceptual black point shift,
and `IIccAdjustPCSXform` hints such as BPC -- is performed by `CIccPcsXform`.
`CIccXform::Apply()` performs none.

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

## What did not change

A `PCS -> device -> PCS` round trip is not exact. The two adjustments bracket
the device LUTs, are never algebraically adjacent, and cannot cancel. This
affects `CIccApplyBPC`'s black-point probes, which build exactly that shape.

## Deprecated

`CIccXform::CheckSrcAbs()`, `CheckDstAbs()` and `AdjustPCS()` are retained but
unused by IccProfLib. They are `protected`, so a third-party subclass may call
them from its own `Apply()` -- such a subclass now applies the adjustment twice
and should stop.

`bUsePCSConversions` on `CIccCmm::Begin()`, `CIccNamedColorCmm::Begin()` and
`CheckPCSConnections()` is ignored. It selected the in-xform path, which no
longer exists.
```

- [ ] **Step 4: Commit**

```bash
git add .github/ci/regression/pcs-adjust-placement.cpp docs/pcs-adjustment-placement.md
git commit -m "test: pin XYZ-PCS behaviour after the clip retirement; document placement"
```

---

### Task 7: Measure the BPC black-point deltas

`CIccApplyBPC::pixelXfm()` and `getBlackXfm()` build `PCS -> device -> PCS` chains, whose two adjustments bracket the device LUTs. Those chains are affected — each edge now does one fewer `Lab<->XYZ` conversion — so the estimated black points move slightly, and so does everything downstream. Measure it rather than assume it.

**Files:**
- No source changes. Produces `docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md`.

**Interfaces:**
- Consumes: the completed refactor.
- Produces: the delta record the reviewer needs.

- [ ] **Step 1: Write the probe data file**

```bash
SCRATCH="$(mktemp -d)"
cat > "$SCRATCH/cmyk-probe.txt" <<'EOF'
'CMYK' ; Data Format
icEncodePercent ; Encoding
  0.0   0.0   0.0   0.0
 10.0  20.0  30.0  40.0
 50.0  50.0  50.0  50.0
100.0   0.0   0.0   0.0
  0.0   0.0   0.0 100.0
EOF
echo "$SCRATCH"
```

Keep `$SCRATCH` for the rest of this task. `Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc` is the fixture; it is a v5 CMYK output profile with a Lab PCS, so intent `40` exercises BPC and `41` exercises BPC on relative.

- [ ] **Step 2: Capture baseline output from `master`**

Use the tracked `Testing/iccApplyNamedCmm.exe`. Per pre-flight Ruling 3, it was built at `96507a4`, and every commit between there and `master` (`68b1b6f2`) is CI-only — labeler, Docker/vcpkg validation, iccSpecSepToTiff validation, LTO link time. None touches IccProfLib, so it is a valid master-behavior baseline. Confirm that before trusting it:

```bash
git --no-pager log --stat 96507a4..master -- IccProfLib/ IccConnect/   # expect no output
./Testing/iccApplyNamedCmm.exe 2>&1 | head -1                          # expect "+96507a4"

for INTENT in 40 41; do
  ./Testing/iccApplyNamedCmm.exe "$SCRATCH/cmyk-probe.txt" 1:8:14 1 \
    Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc $INTENT \
    Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc $INTENT > "$SCRATCH/bpc-baseline-$INTENT.txt"
done
```

If the first command prints anything, the assumption is falsified — fall back to a worktree build:

```bash
git worktree add ../iccdev-master-baseline master
cmake -S ../iccdev-master-baseline/Build/Cmake -B ../iccdev-master-baseline/out/baseline \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_TOOLS=ON -DCMAKE_TOOLCHAIN_FILE=
cmake --build ../iccdev-master-baseline/out/baseline --target iccApplyNamedCmm -j
find ../iccdev-master-baseline/out/baseline -name 'iccApplyNamedCmm*' -type f
```

and remove the worktree with `git worktree remove --force ../iccdev-master-baseline` when done.

- [ ] **Step 3: Capture the same output on this branch and diff**

```bash
cmake --build out/plan-tests --target iccApplyNamedCmm -j
BRANCH=$(find out/plan-tests -name 'iccApplyNamedCmm*' -type f | head -1)
for INTENT in 40 41; do
  "$BRANCH" "$SCRATCH/cmyk-probe.txt" 1:8:14 1 \
    Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc $INTENT \
    Testing/CMYK-3DLUTs/CMYK-3DLUTs.icc $INTENT > "$SCRATCH/bpc-branch-$INTENT.txt"
done
diff -u "$SCRATCH/bpc-baseline-40.txt" "$SCRATCH/bpc-branch-40.txt"
diff -u "$SCRATCH/bpc-baseline-41.txt" "$SCRATCH/bpc-branch-41.txt"
```

- [ ] **Step 4: Record the result**

Write `docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md` containing both diffs verbatim and one paragraph classifying the magnitude. The acceptance criterion: **every difference is below 1e-4 in CMYK percent.** A difference at 1e-3 or larger is not reassociation noise — it means an adjustment was dropped, doubled, or inverted somewhere in Tasks 4-5. Stop and find it; do not record it as expected.

- [ ] **Step 5: Clean up and commit**

```bash
git worktree remove --force ../iccdev-master-baseline
rm -rf "$SCRATCH"
git add docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md
git commit -m "docs: record BPC black-point deltas from the PCS adjustment move"
```

---

### Task 8: Full sweep and handoff

**Files:**
- No source changes expected. Fix whatever the sweep finds.

- [ ] **Step 1: Full CTest run, compared against a `master` baseline**

Some tests in this repo are red on `master` for unrelated reasons, so "all green" is the wrong bar — "no *new* failures" is. Capture both lists and diff them.

```bash
cmake --build out/plan-tests -j
ctest --test-dir out/plan-tests --output-on-failure --no-tests=error 2>&1 \
  | tee /tmp/ctest-branch.txt
grep -E "^\s*[0-9]+ - .*\(Failed\)" /tmp/ctest-branch.txt | sed 's/^ *[0-9]* - //' \
  | sort > /tmp/fails-branch.txt

git worktree add ../iccdev-master-baseline master
cmake -S ../iccdev-master-baseline/Build/Cmake -B ../iccdev-master-baseline/out/baseline \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_TOOLS=ON -DENABLE_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=
cmake --build ../iccdev-master-baseline/out/baseline -j
ctest --test-dir ../iccdev-master-baseline/out/baseline --no-tests=error 2>&1 \
  | grep -E "^\s*[0-9]+ - .*\(Failed\)" | sed 's/^ *[0-9]* - //' \
  | sort > /tmp/fails-baseline.txt

comm -13 /tmp/fails-baseline.txt /tmp/fails-branch.txt   # new failures — must be empty
git worktree remove --force ../iccdev-master-baseline
```

Expected: the `comm` output is empty. Anything listed is a regression introduced by this branch; investigate before proceeding. Record the pre-existing failures in the delta document so a reviewer knows they were already red.

- [ ] **Step 2: Sanitizer build**

```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build . -j && ctest --output-on-failure -R "pcs-|cmm|v2-legacy|xform-abstorel"
```

Expected: clean. The Task 5 deletions remove reads of two member bools; ASan confirms nothing else read them.

- [ ] **Step 3: Doxygen check**

```bash
doxygen .github/ci/doxygen/Doxyfile
```

Expected: `docs/generated/doxygen-warnings.log` empty. The new `docs/pcs-adjustment-placement.md` must not introduce a Markdown link pointing outside the Doxygen INPUT tree — use an explicit HTML anchor if it does. Do not commit generated Doxygen output.

- [ ] **Step 4: Record the contract matrix**

`AGENTS.md` requires a `base...HEAD` contract matrix for every changed cross-cutting surface before the first automated review. Append it to `docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md` covering, at minimum:

| Surface | Producer | Consumer | Behavior change |
|---|---|---|---|
| `CIccXform::NeedsSrcPcsAdjust()` / `NeedsDstPcsAdjust()` | new virtuals on `CIccXform` | `CIccPcsXform::Connect*` | new API; subclass overrides in `CIccXformMpe`, `CIccXformNamedColor` |
| `SetSrcPCSConversion()` / `SetDstPCSConversion()` | removed | none in-tree | removed public API; third-party breakage possible |
| `CheckSrcAbs()` / `CheckDstAbs()` / `AdjustPCS()` | retained, deprecated | third-party subclasses only | a subclass calling them now double-applies |
| `bUsePCSConversions` | retained, ignored | `CIccCmm::Begin()`, `CIccNamedColorCmm::Begin()` | a caller passing `true` gets different behavior |
| XYZ-PCS negative clamp | removed | any XYZ-PCS chain | negative XYZ no longer forced to zero |

- [ ] **Step 5: Commit and push**

```bash
git add docs/superpowers/plans/2026-08-26-pcs-adjust-bpc-deltas.md
git commit -m "docs: contract matrix for the PCS adjustment refactor"
git push -u origin refactor/pcs-adjust-in-pcsxform
```
