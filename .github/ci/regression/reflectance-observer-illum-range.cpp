// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression for #1671: reflectance observer setup must size the temporary
// observer matrix by the applied illuminant range, not by the element reflectance
// range. The test uses a 31-step reflectance element with an 81-step custom
// illuminant/observer PCC. Pre-fix, CIccMpeReflectanceObserver::Begin wrote the
// 81-step observer into a 31-step matrix under ASan. The CLUT case exercises the
// analogous reflectance path. One-step or zero-span ranges must be rejected
// before range mapping because SetRange() cannot initialize them.

#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
#include "IccPcc.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char *msg)
{
  if (ok) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

icSpectralRange make_range(float start, float end, icUInt16Number steps)
{
  icSpectralRange range;
  range.start = icFtoF16(start);
  range.end = icFtoF16(end);
  range.steps = steps;
  return range;
}

class TestConnectionConditions : public IIccProfileConnectionConditions
{
public:
  explicit TestConnectionConditions(const icSpectralRange &illumRange)
  {
    std::vector<icFloatNumber> illum(illumRange.steps, 1.0f);
    std::vector<icFloatNumber> observer(illumRange.steps * 3, 0.0f);

    for (icUInt16Number i = 0; i < illumRange.steps; ++i) {
      observer[i] = 0.5f;
      observer[illumRange.steps + i] = 1.0f;
      observer[2 * illumRange.steps + i] = 0.25f;
    }

    m_valid = m_view.setIlluminant(icIlluminantCustom, illumRange, illum.data()) &&
              m_view.setObserver(icStdObsCustom, illumRange, observer.data());
  }

  bool valid() const { return m_valid; }

  const CIccTagSpectralViewingConditions *getPccViewingConditions() override
  {
    return &m_view;
  }

  CIccTagMultiProcessElement *getCustomToStandardPcc() override { return NULL; }
  CIccTagMultiProcessElement *getStandardToCustomPcc() override { return NULL; }

  void getNormIlluminantXYZ(icFloatNumber *pXYZ) override
  {
    pXYZ[0] = 1.0f;
    pXYZ[1] = 1.0f;
    pXYZ[2] = 1.0f;
  }

  void getLumIlluminantXYZ(icFloatNumber *pXYZ) override
  {
    getNormIlluminantXYZ(pXYZ);
  }

  bool getMediaWhiteXYZ(icFloatNumber *pXYZ) override
  {
    getNormIlluminantXYZ(pXYZ);
    return true;
  }

private:
  CIccTagSpectralViewingConditions m_view;
  bool m_valid = false;
};

class TestMpeTag : public CIccTagMultiProcessElement
{
public:
  explicit TestMpeTag(IIccProfileConnectionConditions *pcc)
  {
    m_pAppliedPCC = pcc;
  }
};

bool run_reflectance_observer(const icSpectralRange &reflectanceRange,
                              TestConnectionConditions &pcc)
{
  CIccTagMultiProcessElement mpe;
  mpe.SetChannels(reflectanceRange.steps, 3);

  CIccMpeReflectanceObserver *observer = new CIccMpeReflectanceObserver();
  if (!observer->SetSize(reflectanceRange.steps, 3, reflectanceRange)) {
    delete observer;
    return false;
  }

  icFloatNumber *white = observer->GetWhite();
  for (icUInt16Number i = 0; i < reflectanceRange.steps; ++i)
    white[i] = 1.0f;

  mpe.Attach(observer);
  return mpe.Begin(icElemInterpLinear, NULL, &pcc);
}

icFloatNumber *make_white(icUInt16Number steps)
{
  icFloatNumber *white = static_cast<icFloatNumber *>(std::calloc(steps, sizeof(icFloatNumber)));
  if (!white)
    return NULL;

  for (icUInt16Number i = 0; i < steps; ++i)
    white[i] = 1.0f;

  return white;
}

bool run_reflectance_clut(const icSpectralRange &reflectanceRange,
                          TestConnectionConditions &pcc)
{
  CIccCLUT *sourceClut = new CIccCLUT(1, reflectanceRange.steps, 4);
  if (!sourceClut->Init(2)) {
    delete sourceClut;
    return false;
  }

  icFloatNumber *data = sourceClut->GetData(0);
  for (icUInt32Number i = 0; i < sourceClut->NumPoints() * reflectanceRange.steps; ++i)
    data[i] = 0.5f;

  CIccMpeReflectanceCLUT *clut = new CIccMpeReflectanceCLUT();
  clut->SetData(sourceClut, icValueTypeFloat32, reflectanceRange,
                make_white(reflectanceRange.steps), 3);

  TestMpeTag mpe(&pcc);
  return clut->Begin(icElemInterpLinear, &mpe);
}

} // namespace

int main()
{
  std::setbuf(stdout, NULL);
  const icSpectralRange reflectanceRange = make_range(400.0f, 700.0f, 31);
  const icSpectralRange illumRange = make_range(380.0f, 780.0f, 81);
  const icSpectralRange oneStepReflectanceRange = make_range(500.0f, 500.0f, 1);
  const icSpectralRange oneStepIllumRange = make_range(500.0f, 500.0f, 1);
  const icSpectralRange zeroSpanReflectanceRange = make_range(500.0f, 500.0f, 2);
  const icSpectralRange zeroSpanIllumRange = make_range(500.0f, 500.0f, 2);
  const icSpectralRange reversedReflectanceRange = make_range(700.0f, 400.0f, 31);
  const icSpectralRange reversedIllumRange = make_range(780.0f, 380.0f, 81);

  TestConnectionConditions pcc(illumRange);
  check(pcc.valid(), "custom 81-step illuminant and observer PCC is initialized");

  check(run_reflectance_observer(reflectanceRange, pcc),
        "31-step reflectance observer initializes against 81-step illuminant");
  check(run_reflectance_clut(reflectanceRange, pcc),
        "31-step reflectance CLUT initializes against 81-step illuminant");

  TestConnectionConditions oneStepPcc(oneStepIllumRange);
  check(oneStepPcc.valid(), "custom one-step illuminant PCC is initialized");
  check(!run_reflectance_observer(reflectanceRange, oneStepPcc),
        "one-step reflectance observer illuminant is rejected");
  check(!run_reflectance_clut(reflectanceRange, oneStepPcc),
        "one-step reflectance CLUT illuminant is rejected");
  check(!run_reflectance_observer(oneStepReflectanceRange, pcc),
        "one-step reflectance observer range is rejected");
  check(!run_reflectance_clut(oneStepReflectanceRange, pcc),
        "one-step reflectance CLUT range is rejected");
  check(!run_reflectance_observer(zeroSpanReflectanceRange, pcc),
        "zero-span reflectance observer range is rejected");
  check(!run_reflectance_clut(zeroSpanReflectanceRange, pcc),
        "zero-span reflectance CLUT range is rejected");
  check(!run_reflectance_observer(reversedReflectanceRange, pcc),
        "reversed reflectance observer range is rejected");
  check(!run_reflectance_clut(reversedReflectanceRange, pcc),
        "reversed reflectance CLUT range is rejected");

  TestConnectionConditions zeroSpanIllumPcc(zeroSpanIllumRange);
  check(zeroSpanIllumPcc.valid(), "custom zero-span illuminant PCC is initialized");
  check(!run_reflectance_observer(reflectanceRange, zeroSpanIllumPcc),
        "zero-span reflectance observer illuminant is rejected");
  check(!run_reflectance_clut(reflectanceRange, zeroSpanIllumPcc),
        "zero-span reflectance CLUT illuminant is rejected");

  TestConnectionConditions reversedIllumPcc(reversedIllumRange);
  check(reversedIllumPcc.valid(), "custom reversed illuminant PCC is initialized");
  check(!run_reflectance_observer(reflectanceRange, reversedIllumPcc),
        "reversed reflectance observer illuminant is rejected");
  check(!run_reflectance_clut(reflectanceRange, reversedIllumPcc),
        "reversed reflectance CLUT illuminant is rejected");

  // The regression signal is the Begin() call above. Avoid unrelated sanitizer
  // teardown behavior in locally constructed PCC/MPE fixtures.
  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    std::fflush(stdout);
    std::_Exit(1);
  }

  std::printf("\nall checks passed\n");
  std::fflush(stdout);
  std::_Exit(0);
}
