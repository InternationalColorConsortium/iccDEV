/*
    File:       IccConnectThreadTest.cpp

    Contains:   CTest smoke test for CIccConnectCmm threaded standard CMMs.
*/

#include "IccConnect.h"
#include "IccCmmThread.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

static bool NearlyEqual(icFloatNumber a, icFloatNumber b)
{
  return std::fabs(a - b) <= 1.0e-5f;
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s profile.icc\n", argv[0]);
    return 2;
  }

  CIccCfgProfileSequence profiles;
  CIccCfgProfilePtr profile(new CIccCfgProfile());
  profile->m_iccFile = argv[1];
  profile->m_intent = icRelativeColorimetric;
  profile->m_transform = icXformLutColor;
  profile->m_interpolation = icInterpTetrahedral;
  profiles.m_profiles.push_back(profile);

  std::unique_ptr<CIccConnectCmm> scalar(
    CIccConnectCmm::CreateStandard(profiles, nullptr, 0, 1));
  std::unique_ptr<CIccConnectCmm> threaded(
    CIccConnectCmm::CreateStandard(profiles, nullptr, 0, 2));

  if (!scalar || !scalar->GetCmm()) {
    std::fprintf(stderr, "failed to create scalar IccConnect CMM\n");
    return 1;
  }
  if (!threaded || !threaded->GetCmm()) {
    std::fprintf(stderr, "failed to create threaded IccConnect CMM\n");
    return 1;
  }
  if (scalar->IsThreaded()) {
    std::fprintf(stderr, "scalar CMM unexpectedly reports threaded wrapper\n");
    return 1;
  }
  if (!threaded->IsThreaded()) {
    std::fprintf(stderr, "threaded CMM did not use CIccThreadedCmm\n");
    return 1;
  }

  CIccCmm* pScalarCmm = scalar->GetCmm();
  CIccCmm* pThreadedCmm = threaded->GetCmm();

  int nSrcSamples = pScalarCmm->GetSourceSamples();
  int nDstSamples = pScalarCmm->GetDestSamples();
  if (nSrcSamples <= 0 || nDstSamples <= 0) {
    std::fprintf(stderr, "invalid sample counts src=%d dst=%d\n", nSrcSamples, nDstSamples);
    return 1;
  }

  const icUInt32Number nPixels = 8;
  std::vector<icFloatNumber> src(nPixels * nSrcSamples, 0.0f);
  std::vector<icFloatNumber> scalarDst(nPixels * nDstSamples, 0.0f);
  std::vector<icFloatNumber> threadedDst(nPixels * nDstSamples, 0.0f);

  for (icUInt32Number p = 0; p < nPixels; ++p) {
    for (int c = 0; c < nSrcSamples; ++c) {
      src[p * nSrcSamples + c] =
        static_cast<icFloatNumber>((p + c + 1) % 8) / 7.0f;
    }
  }

  if (pScalarCmm->Apply(scalarDst.data(), src.data(), nPixels) != icCmmStatOk) {
    std::fprintf(stderr, "scalar multi-pixel apply failed\n");
    return 1;
  }
  if (pThreadedCmm->Apply(threadedDst.data(), src.data(), nPixels) != icCmmStatOk) {
    std::fprintf(stderr, "threaded multi-pixel apply failed\n");
    return 1;
  }

  for (size_t i = 0; i < scalarDst.size(); ++i) {
    if (!NearlyEqual(scalarDst[i], threadedDst[i])) {
      std::fprintf(stderr,
                   "threaded mismatch at %zu: scalar=%g threaded=%g\n",
                   i, scalarDst[i], threadedDst[i]);
      return 1;
    }
  }

  return 0;
}
