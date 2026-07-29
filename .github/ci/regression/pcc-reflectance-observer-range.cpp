// #1853 (ci-qa-flags harvest): IIccProfileConnectionConditions::getReflectanceObserver()
// applied the illuminant to the wrong axis and leaked the observer matrix.
//
// getObserverMatrix() returns a 3 x illumRange.steps matrix. rangeMap() returns
// an illumRange.steps x rangeRef.steps resampling matrix, and Mult() combines
// them into a 3 x rangeRef.steps result. The illuminant weights the observer
// sample-for-sample, so it belongs on the first matrix, while it is still as
// wide as the illuminant SPD is long. Scaling the *combined* matrix instead
// weighted the reflectance axis and indexed illum[] out to rangeRef.steps,
// reading past the end of the SPD whenever the reflectance range was the finer
// of the two. Mult() also returns a new matrix without consuming its operands,
// so overwriting the observer pointer with the product leaked it.
//
// The invariant pinned here is physical: resampling a reflectance onto a finer
// grid over the same wavelength span must not move the measured white point.
// Linear interpolation of a constant reflectance is that same constant, so a
// flat reflectance must integrate to the same XYZ no matter how many steps the
// reflectance range carries. Master fails this -- and trips ASan first.
//
// This is the same reflectance-range-vs-illuminant-range confusion that #1671
// fixed in CIccMpeReflectanceObserver::Begin(), which sized a temporary observer
// matrix by the element reflectance range instead of the applied illuminant
// range. That fix did not reach this function, which reaches the same mistake
// from the opposite direction -- indexing the illuminant by the reflectance
// width. iccdev.reflectance-observer-illum-range covers the #1671 site; this
// test covers the PCC one.
//
// The overflow and the leak are observable only under a sanitizer, so the
// sanitizer CI legs are what catch those. The value checks below fail on a
// plain build too.

#include "IccPcc.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccMatrixMath.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <vector>

static const icUInt16Number kIllumSteps = 5;
static const float kStart = 400.0f;
static const float kEnd = 500.0f;

// Deliberately not flat. A constant illuminant or observer would make the
// correct and incorrect weightings agree numerically and hide the defect.
static const icFloatNumber kIllum[kIllumSteps] = {0.50f, 0.80f, 1.00f, 0.90f, 0.60f};
static const icFloatNumber kObserver[3 * kIllumSteps] = {
  0.10f, 0.30f, 0.80f, 0.40f, 0.10f,   // x
  0.05f, 0.35f, 0.90f, 0.55f, 0.15f,   // y (luminance; the row rowSum normalizes)
  0.60f, 0.70f, 0.20f, 0.05f, 0.01f,   // z
};

static icSpectralRange makeRange(float start, float end, icUInt16Number steps)
{
  icSpectralRange r;
  r.start = icFtoF16(start);
  r.end = icFtoF16(end);
  r.steps = steps;
  return r;
}

// Builds a v5 profile carrying a spectral viewing conditions tag. The profile
// itself implements IIccProfileConnectionConditions, so it doubles as the PCC.
static bool attachView(CIccProfile &profile)
{
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV5;

  icSpectralRange illumRange = makeRange(kStart, kEnd, kIllumSteps);

  CIccTagSpectralViewingConditions *view = new CIccTagSpectralViewingConditions();
  if (!view->setIlluminant(icIlluminantCustom, illumRange, kIllum, 5000.0f) ||
      !view->setObserver(icStdObsCustom, illumRange, kObserver)) {
    delete view;
    return false;
  }

  if (!profile.AttachTag(icSigSpectralViewingConditionsTag, view)) {
    delete view;
    return false;
  }
  return true;
}

// Integrates a flat unit reflectance sampled over `steps` points spanning the
// same range as the illuminant, returning the resulting XYZ.
static bool flatReflectanceXYZ(icUInt16Number steps, icFloatNumber *xyz)
{
  CIccProfile profile;
  if (!attachView(profile))
    return false;

  icSpectralRange rangeRef = makeRange(kStart, kEnd, steps);

  // On master this is where ASan reports a heap-buffer-overflow read of the
  // illuminant SPD once steps > kIllumSteps, and where the observer matrix
  // leaks for any steps != kIllumSteps.
  CIccMatrixMath *pMtx = profile.getReflectanceObserver(rangeRef);
  if (!pMtx)
    return false;

  std::vector<icFloatNumber> reflectance(steps, 1.0f);
  pMtx->VectorMult(xyz, reflectance.data());
  delete pMtx;
  return true;
}

static bool nearly(icFloatNumber a, icFloatNumber b, float tol)
{
  return std::fabs((float)(a - b)) < tol;
}

int main()
{
  // 1. Baseline: reflectance sampled exactly on the illuminant grid. rangeMap()
  //    returns NULL here, so this path is untouched by the fix and pins it.
  icFloatNumber base[3] = {-1.0f, -1.0f, -1.0f};
  if (!flatReflectanceXYZ(kIllumSteps, base)) {
    printf("FAIL: no observer for the matched range\n");
    return 1;
  }

  // The Y row is normalized by its own sum, so a flat unit reflectance must
  // integrate to exactly 1.0 regardless of the observer values chosen above.
  if (!nearly(base[1], 1.0f, 1e-5f)) {
    printf("FAIL: matched-range Y = %f, expected 1.0\n", (double)base[1]);
    return 2;
  }

  // Hand-computed from kObserver/kIllum: X = sum(x[i]*illum[i]) / sum(y[i]*illum[i]),
  // and likewise for Z. Pins the absolute values, not just the ratio, so a change
  // in how the illuminant is applied cannot pass by scaling every row alike.
  {
    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (int i = 0; i < (int)kIllumSteps; i++) {
      sx += (double)kObserver[i] * kIllum[i];
      sy += (double)kObserver[kIllumSteps + i] * kIllum[i];
      sz += (double)kObserver[2 * kIllumSteps + i] * kIllum[i];
    }
    if (!nearly(base[0], (icFloatNumber)(sx / sy), 1e-5f) ||
        !nearly(base[2], (icFloatNumber)(sz / sy), 1e-5f)) {
      printf("FAIL: matched-range XYZ = %f %f %f, expected %f 1.0 %f\n",
             (double)base[0], (double)base[1], (double)base[2], sx / sy, sz / sy);
      return 3;
    }
  }

  // 2. The finer grid: more reflectance steps than the illuminant has samples.
  //    This is the combination that read past the end of the SPD, so it is
  //    checked before the coarse grid -- on master a sanitizer build aborts
  //    here, rather than exiting earlier on a value mismatch that would leave
  //    the memory error unreported.
  icFloatNumber fine[3] = {-1.0f, -1.0f, -1.0f};
  if (!flatReflectanceXYZ(9, fine)) {
    printf("FAIL: no observer for the fine range\n");
    return 4;
  }
  for (int i = 0; i < 3; i++) {
    if (!nearly(fine[i], base[i], 1e-4f)) {
      printf("FAIL: fine-grid XYZ[%d] = %f, expected %f\n",
             i, (double)fine[i], (double)base[i]);
      return 5;
    }
  }

  // 3. The same flat reflectance on a COARSER grid. rangeMap() runs, so the
  //    Mult() path is exercised, but illum[] is still indexed within bounds on
  //    master -- this isolates the leak from the overflow.
  icFloatNumber coarse[3] = {-1.0f, -1.0f, -1.0f};
  if (!flatReflectanceXYZ(3, coarse)) {
    printf("FAIL: no observer for the coarse range\n");
    return 6;
  }
  for (int i = 0; i < 3; i++) {
    if (!nearly(coarse[i], base[i], 1e-4f)) {
      printf("FAIL: coarse-grid XYZ[%d] = %f, expected %f\n",
             i, (double)coarse[i], (double)base[i]);
      return 7;
    }
  }

  // 4. calcMediaWhiteXYZ() dereferenced the observer without checking it.
  //
  //    Reaching that line takes some care: the function returns through its
  //    media-white-point branch unless BOTH a spectral viewing conditions tag
  //    and a spectralWhitePointTag are present, the white point is a numeric
  //    array, and the header carries a reflectance spectral PCS with at least
  //    as many samples as the array holds. A profile with no tags never calls
  //    getReflectanceObserver() at all, so it would not cover this.
  //
  //    Here everything is present and well formed except that the illuminant
  //    carries a single sample. icCanMapSpectralRange() requires more than one
  //    step on both sides once the ranges differ, so getReflectanceObserver()
  //    returns NULL on its guard -- and master then dereferences it.
  {
    const icUInt16Number kSamples = 4;

    CIccProfile refl;
    refl.InitHeader();
    refl.m_Header.version = icVersionNumberV5;
    // "rs" + the sample count in the low bits; icGetSpaceSamples() reads the
    // count back out of the signature.
    refl.m_Header.spectralPCS =
      (icColorSpaceSignature)((icUInt32Number)icSigReflectanceSpectralData | kSamples);
    refl.m_Header.spectralRange = makeRange(kStart, kEnd, kSamples);

    // A one-step illuminant cannot be range-mapped onto the 4-step reflectance.
    icSpectralRange oneStep = makeRange(kStart, kStart, 1);
    icFloatNumber single = 1.0f;

    CIccTagSpectralViewingConditions *view = new CIccTagSpectralViewingConditions();
    if (!view->setIlluminant(icIlluminantCustom, oneStep, &single, 5000.0f) ||
        !view->setObserver(icStdObsCustom, oneStep, kObserver) ||
        !refl.AttachTag(icSigSpectralViewingConditionsTag, view)) {
      delete view;
      printf("FAIL: could not build the degenerate-illuminant profile\n");
      return 8;
    }

    CIccTagFloat32 *white = new CIccTagFloat32();
    white->SetSize(kSamples);
    for (icUInt16Number i = 0; i < kSamples; i++)
      (*white)[i] = 0.5f;
    if (!refl.AttachTag(icSigSpectralWhitePointTag, white)) {
      delete white;
      printf("FAIL: could not attach the spectral white point\n");
      return 9;
    }

    // Guard the premise: if this ever stops returning NULL the case below stops
    // covering the fix, and would pass for the wrong reason.
    icSpectralRange refRange = makeRange(kStart, kEnd, kSamples);
    CIccMatrixMath *probe = refl.getReflectanceObserver(refRange);
    if (probe) {
      delete probe;
      printf("FAIL: the degenerate illuminant no longer yields a NULL observer\n");
      return 10;
    }

    // Master dereferences NULL here. The return value is not asserted -- the
    // fall-through legitimately reports false with no media white point tag.
    icFloatNumber xyz[3] = {-1.0f, -1.0f, -1.0f};
    refl.calcMediaWhiteXYZ(xyz, &refl);
  }

  printf("PASS: reflectance observer is range-invariant "
         "(XYZ = %f %f %f)\n", (double)base[0], (double)base[1], (double)base[2]);
  return 0;
}
