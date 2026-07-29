// Regression for #1853: CIccProfile::calcMediaWhiteXYZ() must compute the spectral media
// white point from the white point tag's actual contents, not from uninitialized heap.
//
// CIccTagNumArray::GetValues() declares nStart = 0 and nVectorSize = 1 as defaults, so the
// single-argument call the spectral branch used to make copied exactly ONE float into a
// buffer allocated for the whole spectrum. Everything downstream then read all `samples`
// entries -- VectorMult() against the reflectance observer, getEmissiveObserver()'s k
// accumulation -- so for a 31-sample profile, 30 of the 31 values feeding the returned XYZ
// were whatever the allocator last left in that buffer.
//
// This was invisible because nothing asserted on the value. Measured over Testing/ before
// the fix, calcMediaWhiteXYZ() returned Y = 15.02, 98.93, 1.23e+26 and -nan for profiles
// whose media white point must be normalised to Y = 1, and it was not even reproducible:
// three runs of one unfixed binary over one unchanged corpus returned different
// colorimetry, because the values came from heap that other work had disturbed differently
// each time.
//
// The companion test iccdev.rangemap-uninitialized pins the rangeMap() contract that the
// same change fixes, but it drives the matrix helper directly and would not have caught
// this: the defect only shows up when a whole profile's white point is computed.
//
// The profile is built in memory rather than read from Testing/. An earlier version of
// this test loaded three generated PCC profiles and passed everywhere except Windows,
// where the iccdev_profiles fixture is satisfied by iccdev.windows-create-profiles writing
// into the build tree instead of the source tree, so the files were simply not at the path
// this test looked in. Constructing the inputs here removes the fixture dependency
// altogether and lets the expected XYZ be derived in closed form, which is a stronger
// assertion than a tolerance around a reference white.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[spectral-media-white] FAIL: %s\n", what);
  }
}

const icUInt16Number kSteps = 9;

icSpectralRange makeRange(icUInt16Number steps)
{
  icSpectralRange r;
  r.start = icRange380nm;
  r.end = icRange780nm;
  r.steps = steps;
  return r;
}

// Assemble the smallest profile that reaches the reflectance arm of
// calcMediaWhiteXYZ: a spectral viewing-conditions tag to supply the illuminant and
// observer, a spectralWhitePoint num-array tag, and a header whose spectralPCS declares
// a reflectance space of the same width as the range. The caller owns the result.
//
// observerY carries the y-bar row, which is the one that decides the answer:
// getReflectanceObserver() divides the whole matrix by RowSum(1).
CIccProfile *buildReflectanceProfile(const std::vector<icFloatNumber> &white,
                                     const std::vector<icFloatNumber> &illum,
                                     const std::vector<icFloatNumber> &observerX,
                                     const std::vector<icFloatNumber> &observerY,
                                     const std::vector<icFloatNumber> &observerZ)
{
  const icUInt16Number steps = (icUInt16Number)white.size();
  icSpectralRange range = makeRange(steps);

  CIccProfile *pProfile = new CIccProfile();

  pProfile->InitHeader();
  pProfile->m_Header.deviceClass = icSigOutputClass;
  pProfile->m_Header.colorSpace = icSigCmykData;
  pProfile->m_Header.pcs = icSigLabData;
  // Low 16 bits of a spectral signature carry the channel count, so this is a
  // reflectance space exactly `steps` samples wide -- what icGetSpaceSamples() reports
  // and what the range must agree with.
  pProfile->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralData + steps);
  pProfile->m_Header.spectralRange = range;

  CIccTagSpectralViewingConditions *pView = new CIccTagSpectralViewingConditions();
  pView->setIlluminant(icIlluminantUnknown, range, &illum[0]);

  // setObserver() takes the three colour matching functions as one contiguous block of
  // 3 * steps, x-bar then y-bar then z-bar.
  std::vector<icFloatNumber> observer;
  observer.insert(observer.end(), observerX.begin(), observerX.end());
  observer.insert(observer.end(), observerY.begin(), observerY.end());
  observer.insert(observer.end(), observerZ.begin(), observerZ.end());
  pView->setObserver(icStdObsCustom, range, &observer[0]);

  pProfile->AttachTag(icSigSpectralViewingConditionsTag, pView);

  CIccTagFloat32 *pWhiteTag = new CIccTagFloat32();
  pWhiteTag->SetSize(steps);
  for (icUInt16Number i = 0; i < steps; ++i)
    (*pWhiteTag)[i] = white[i];

  pProfile->AttachTag(icSigSpectralWhitePointTag, pWhiteTag);

  return pProfile;
}

// getReflectanceObserver() scales the observer matrix by 1 / RowSum(1) after weighting it
// by the illuminant, so for row r the returned component is
//
//   sum_i( white[i] * observer_r[i] * illum[i] ) / sum_i( observerY[i] * illum[i] )
//
// which is what this computes. Deriving it rather than hard-coding a reference white is
// what makes every sample of `white` load-bearing: read only the first one, as the defect
// did, and no component matches.
icFloatNumber expectedComponent(const std::vector<icFloatNumber> &white,
                                const std::vector<icFloatNumber> &illum,
                                const std::vector<icFloatNumber> &observerRow,
                                const std::vector<icFloatNumber> &observerY)
{
  double num = 0.0, den = 0.0;
  for (size_t i = 0; i < white.size(); ++i) {
    num += (double)white[i] * (double)observerRow[i] * (double)illum[i];
    den += (double)observerY[i] * (double)illum[i];
  }
  return (icFloatNumber)(num / den);
}

void exercise(const char *label,
              const std::vector<icFloatNumber> &white,
              const std::vector<icFloatNumber> &illum,
              const std::vector<icFloatNumber> &observerX,
              const std::vector<icFloatNumber> &observerY,
              const std::vector<icFloatNumber> &observerZ)
{
  char msg[256];

  CIccProfile *pProfile = buildReflectanceProfile(white, illum, observerX, observerY, observerZ);

  icFloatNumber xyz[3] = { -1.0f, -1.0f, -1.0f };
  bool ok = pProfile->calcMediaWhiteXYZ(xyz, pProfile);

  std::snprintf(msg, sizeof(msg), "%s: calcMediaWhiteXYZ reaches the spectral path", label);
  check(ok, msg);

  // Named separately so the -nan results the defect produced read as such in the log
  // rather than showing up only as three failed comparisons.
  bool finite = std::isfinite((double)xyz[0]) && std::isfinite((double)xyz[1]) &&
                std::isfinite((double)xyz[2]);
  std::snprintf(msg, sizeof(msg), "%s: XYZ is finite (got %g, %g, %g)",
                label, (double)xyz[0], (double)xyz[1], (double)xyz[2]);
  check(finite, msg);
  if (!finite) {
    delete pProfile;
    return;
  }

  const icFloatNumber tol = 1.0e-5f;
  const icFloatNumber eX = expectedComponent(white, illum, observerX, observerY);
  const icFloatNumber eY = expectedComponent(white, illum, observerY, observerY);
  const icFloatNumber eZ = expectedComponent(white, illum, observerZ, observerY);

  std::snprintf(msg, sizeof(msg), "%s: X == %g (got %g)", label, (double)eX, (double)xyz[0]);
  check(std::fabs((double)xyz[0] - (double)eX) <= (double)tol, msg);

  std::snprintf(msg, sizeof(msg), "%s: Y == %g (got %g)", label, (double)eY, (double)xyz[1]);
  check(std::fabs((double)xyz[1] - (double)eY) <= (double)tol, msg);

  std::snprintf(msg, sizeof(msg), "%s: Z == %g (got %g)", label, (double)eZ, (double)xyz[2]);
  check(std::fabs((double)xyz[2] - (double)eZ) <= (double)tol, msg);

  delete pProfile;
}

} // namespace

int main()
{
  std::vector<icFloatNumber> ones(kSteps, 1.0f);
  std::vector<icFloatNumber> obsX(kSteps, 0.5f);
  std::vector<icFloatNumber> obsY(kSteps, 1.0f);
  std::vector<icFloatNumber> obsZ(kSteps, 0.3f);

  // A perfect reflector under any illuminant must come back normalised to Y = 1.0
  // exactly, because the observer was divided by its own illuminant-weighted Y sum.
  // That identity is what the corrupted white vectors broke, by factors of 15 to 1e26.
  exercise("perfect reflector", ones, ones, obsX, obsY, obsZ);

  // Non-uniform inputs so no component can come out right by symmetry, and so a reader
  // that honours only the first sample cannot match any of the three. The ramps are
  // chosen to have no repeated values.
  std::vector<icFloatNumber> white(kSteps), illum(kSteps), x(kSteps), y(kSteps), z(kSteps);
  for (icUInt16Number i = 0; i < kSteps; ++i) {
    white[i] = 0.20f + 0.07f * (icFloatNumber)i;   // 0.20 .. 0.76
    illum[i] = 0.60f + 0.11f * (icFloatNumber)i;   // 0.60 .. 1.48
    x[i]     = 0.15f + 0.05f * (icFloatNumber)i;
    y[i]     = 0.90f - 0.04f * (icFloatNumber)i;
    z[i]     = 0.25f + 0.03f * (icFloatNumber)i;
  }
  exercise("non-uniform reflectance", white, illum, x, y, z);

  // Every sample after the first is deliberately far from white[0], so a single-value
  // read reports roughly white[0] scaled by 1 while the true answer is several times
  // that. This is the shape of the original defect in its starkest form.
  std::vector<icFloatNumber> spike(kSteps, 4.0f);
  spike[0] = 0.05f;
  exercise("first sample unrepresentative", spike, ones, obsX, obsY, obsZ);

  if (g_fail)
    std::fprintf(stderr, "[spectral-media-white] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[spectral-media-white] all assertions passed\n");

  return g_fail;
}
