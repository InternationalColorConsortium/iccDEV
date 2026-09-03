/*
    File:       IccConnectThreadTest.cpp

    Contains:   CTest smoke test for CIccConnectCmm threaded standard CMMs.
*/

#include "IccConnect.h"
#include "IccCmmThread.h"
#include "IccCmm.h"
#include "IccUtil.h"

#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static bool NearlyEqual(icFloatNumber a, icFloatNumber b)
{
  return std::fabs(a - b) <= 1.0e-5f;
}

static std::vector<icFloatNumber> MakeZeroVector(size_t n)
{
  std::vector<icFloatNumber> v;
  v.reserve(n);
  for (size_t i = 0; i < n; ++i)
    v.push_back(0.0f);
  return v;
}

class CIccNullApplyCmm : public CIccCmm
{
public:
  CIccApplyCmm* GetNewApplyCmm(icStatusCMM&) override
  {
    return nullptr;
  }
};

class CIccNullApplyNamedColorCmm : public CIccNamedColorCmm
{
public:
  CIccApplyCmm* GetNewApplyCmm(icStatusCMM&) override
  {
    return nullptr;
  }
};

static bool CheckNullApplyFailure(const char* profilePath)
{
  CIccNullApplyCmm cmm;
  CIccNullApplyNamedColorCmm namedCmm;
  CIccCmm* cmms[] = { &cmm, &namedCmm };
  const char* names[] = { "standard", "named-color" };

  for (size_t i = 0; i < sizeof(cmms) / sizeof(cmms[0]); ++i) {
    if (cmms[i]->AddXform(profilePath, icRelativeColorimetric) != icCmmStatOk) {
      std::fprintf(stderr, "failed to add %s allocation-failure test profile\n",
                   names[i]);
      return false;
    }
    if (cmms[i]->Begin() != icCmmStatAllocErr || cmms[i]->Valid() ||
        cmms[i]->GetApply()) {
      std::fprintf(stderr,
                   "%s CMM accepted a null apply object as valid\n", names[i]);
      return false;
    }
  }

  return true;
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
    CIccConnectCmm::CreateStandard(profiles, nullptr, 0, 4));

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
  if (!CheckNullApplyFailure(argv[1]))
    return 1;

  json oversizedThreadsJson;
  oversizedThreadsJson["threads"] = CIccThreadedCmm::GetMaxThreads() + 1;
  CIccCfgConnectOptions oversizedOptions;
  if (oversizedOptions.fromJson(oversizedThreadsJson, true)) {
    std::fprintf(stderr, "oversized JSON thread count was accepted\n");
    return 1;
  }

  std::string tooManyThreadsError;
  std::unique_ptr<CIccConnectCmm> tooManyThreads(
    CIccConnectCmm::CreateStandard(
      profiles, nullptr, 0, CIccThreadedCmm::GetMaxThreads() + 1,
      &tooManyThreadsError));
  if (tooManyThreads) {
    std::fprintf(stderr, "oversized thread count was accepted\n");
    return 1;
  }
  if (tooManyThreadsError.find("invalid thread count") == std::string::npos) {
    std::fprintf(stderr, "missing oversized thread count error: %s\n",
                 tooManyThreadsError.c_str());
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

  const icUInt32Number nPixels = 1024;
  std::vector<icFloatNumber> src = MakeZeroVector(nPixels * nSrcSamples);
  std::vector<icFloatNumber> scalarDst = MakeZeroVector(nPixels * nDstSamples);
  std::vector<icFloatNumber> threadedDst = MakeZeroVector(nPixels * nDstSamples);

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
  for (int pass = 0; pass < 8; pass++) {
    if (pThreadedCmm->Apply(threadedDst.data(), src.data(), nPixels) != icCmmStatOk) {
      std::fprintf(stderr, "threaded multi-pixel apply failed on pass %d\n", pass);
      return 1;
    }
  }

  for (size_t i = 0; i < scalarDst.size(); ++i) {
    if (!NearlyEqual(scalarDst[i], threadedDst[i])) {
      std::fprintf(stderr,
                   "threaded mismatch at %zu: scalar=%g threaded=%g\n",
                   i, scalarDst[i], threadedDst[i]);
      return 1;
    }
  }

  // CIccThreadedCmm documents that separate apply objects may be used by
  // concurrent callers.  Exercise that ownership boundary: each outer thread
  // has its own apply object, worker pool, and destination storage while both
  // share only the immutable, Begin()-ed base CMM and source pixels. This
  // validates results on every build; the former m_bValid data race is
  // discriminated only when this test runs under ThreadSanitizer.
  icStatusCMM applyStatusA = icCmmStatBad;
  icStatusCMM applyStatusB = icCmmStatBad;
  std::unique_ptr<CIccApplyCmm> applyA(
    pThreadedCmm->GetNewApplyCmm(applyStatusA));
  std::unique_ptr<CIccApplyCmm> applyB(
    pThreadedCmm->GetNewApplyCmm(applyStatusB));
  if (!applyA || !applyB || applyStatusA != icCmmStatOk ||
      applyStatusB != icCmmStatOk) {
    std::fprintf(stderr, "failed to create concurrent threaded apply objects\n");
    return 1;
  }

  std::vector<icFloatNumber> concurrentDstA =
    MakeZeroVector(nPixels * nDstSamples);
  std::vector<icFloatNumber> concurrentDstB =
    MakeZeroVector(nPixels * nDstSamples);
  std::mutex startMutex;
  std::condition_variable startCondition;
  int ready = 0;
  bool start = false;
  icStatusCMM concurrentStatusA = icCmmStatOk;
  icStatusCMM concurrentStatusB = icCmmStatOk;
  auto applyConcurrent = [&](CIccApplyCmm* apply,
                             std::vector<icFloatNumber>& dst,
                             icStatusCMM& status) {
    {
      std::unique_lock<std::mutex> lock(startMutex);
      ready++;
      startCondition.notify_all();
      startCondition.wait(lock, [&]() { return start; });
    }
    for (int pass = 0; pass < 8 && status == icCmmStatOk; ++pass)
      status = apply->Apply(dst.data(), src.data(), nPixels);
  };

  std::thread threadA(applyConcurrent, applyA.get(),
                      std::ref(concurrentDstA), std::ref(concurrentStatusA));
  std::thread threadB(applyConcurrent, applyB.get(),
                      std::ref(concurrentDstB), std::ref(concurrentStatusB));
  {
    std::unique_lock<std::mutex> lock(startMutex);
    startCondition.wait(lock, [&]() { return ready == 2; });
    start = true;
  }
  startCondition.notify_all();
  threadA.join();
  threadB.join();

  if (concurrentStatusA != icCmmStatOk || concurrentStatusB != icCmmStatOk) {
    std::fprintf(stderr, "concurrent threaded apply failed: %d, %d\n",
                 (int)concurrentStatusA, (int)concurrentStatusB);
    return 1;
  }
  for (size_t i = 0; i < scalarDst.size(); ++i) {
    if (!NearlyEqual(scalarDst[i], concurrentDstA[i]) ||
        !NearlyEqual(scalarDst[i], concurrentDstB[i])) {
      std::fprintf(stderr, "concurrent threaded mismatch at %zu\n", i);
      return 1;
    }
  }

  return 0;
}
