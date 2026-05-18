/*
    File:       iccconnect-search-cost.cpp

    Contains:   CTest smoke test for CIccCmmSearch::GetApplyCost.

    Build a single-PCC search CMM from supplied profile paths and verify that
    GetApplyCost() returns a finite, non-negative cost for a representative
    input.  Optionally checks that the cost varies between two inputs (basic
    sanity that the cost is actually evaluated rather than stuck at zero).

    Usage:
      iccconnect-search-cost <fwd_profile.icc> <init_profile.icc> <pcc_profile.icc>

    Where:
      fwd_profile.icc  - forward profile (e.g. a device->PCS spectral profile)
      init_profile.icc - initial destination profile (last in the search chain)
      pcc_profile.icc  - Profile Connection Conditions profile to attach
                         (must be readable as a PCC via OpenIccProfile +
                         ReadPccTags).

    Exit codes:
      0 - all checks passed
      1 - construction or apply-time failure / cost out of range
      2 - usage error
*/

#include "IccCmmSearch.h"
#include "IccProfile.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

static bool IsFiniteNonNegative(icFloatNumber v)
{
  return std::isfinite(v) && v >= 0.0f;
}

int main(int argc, char** argv)
{
  if (argc < 4) {
    std::fprintf(stderr,
      "usage: %s <fwd_profile.icc> <init_profile.icc> <pcc_profile.icc>\n",
      argv[0] ? argv[0] : "iccconnect-search-cost");
    return 2;
  }

  const char* fwdPath  = argv[1];
  const char* initPath = argv[2];
  const char* pccPath  = argv[3];

  // Build a minimal search chain: forward profile + initial-destination
  // profile + one weighted PCC.  The exact colorimetry the chain represents
  // is irrelevant here; the regression target is the behavior of
  // GetApplyCost, not a specific cost value.
  std::unique_ptr<CIccCmmSearch> pCmm(new CIccCmmSearch());

  CIccProfile* pFwd = OpenIccProfile(fwdPath);
  if (!pFwd) {
    std::fprintf(stderr, "iccconnect-search-cost: unable to open '%s'\n", fwdPath);
    return 1;
  }
  if (pCmm->CIccCmm::AddXform(pFwd) != icCmmStatOk) {
    std::fprintf(stderr, "iccconnect-search-cost: forward AddXform failed for '%s'\n", fwdPath);
    delete pFwd;
    return 1;
  }

  CIccProfile* pInit = OpenIccProfile(initPath);
  if (!pInit) {
    std::fprintf(stderr, "iccconnect-search-cost: unable to open '%s'\n", initPath);
    return 1;
  }
  pCmm->SetDstInitProfile(pInit, icAbsoluteColorimetric, icInterpTetrahedral,
                          nullptr, icXformLutColor, false);

  CIccProfile* pPcc = OpenIccProfile(pccPath);
  if (!pPcc || !pPcc->ReadPccTags()) {
    std::fprintf(stderr, "iccconnect-search-cost: unable to read PCC tags from '%s'\n", pccPath);
    delete pPcc;
    return 1;
  }
  pPcc->Detach();
  if (pCmm->AttachPCC(pPcc, /*dWeight=*/1.0f) != icCmmStatOk) {
    std::fprintf(stderr, "iccconnect-search-cost: AttachPCC failed for '%s'\n", pccPath);
    return 1;
  }

  if (pCmm->Begin() != icCmmStatOk) {
    std::fprintf(stderr, "iccconnect-search-cost: Begin() failed\n");
    return 1;
  }

  // Build a sample input matching the chain's source-side sample count.
  // We do not care that the values represent a meaningful color, only that
  // the optimizer is fed a well-defined input.
  const int nSrcSamples = pCmm->GetSourceSamples();
  if (nSrcSamples <= 0) {
    std::fprintf(stderr, "iccconnect-search-cost: invalid GetSourceSamples=%d\n", nSrcSamples);
    return 1;
  }

  std::vector<icFloatNumber> srcA(static_cast<size_t>(nSrcSamples), 0.25f);
  std::vector<icFloatNumber> srcB(static_cast<size_t>(nSrcSamples), 0.75f);

  icFloatNumber costA = -2.0f;
  icStatusCMM rv = pCmm->GetApplyCost(costA, srcA.data());
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "iccconnect-search-cost: GetApplyCost(A) returned status %d\n", (int)rv);
    return 1;
  }
  if (!IsFiniteNonNegative(costA)) {
    std::fprintf(stderr, "iccconnect-search-cost: cost A is out of range (%g); expected finite, >= 0\n",
                 (double)costA);
    return 1;
  }

  icFloatNumber costB = -2.0f;
  rv = pCmm->GetApplyCost(costB, srcB.data());
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "iccconnect-search-cost: GetApplyCost(B) returned status %d\n", (int)rv);
    return 1;
  }
  if (!IsFiniteNonNegative(costB)) {
    std::fprintf(stderr, "iccconnect-search-cost: cost B is out of range (%g); expected finite, >= 0\n",
                 (double)costB);
    return 1;
  }

  // Sanity: -1 sentinel must NOT remain after a successful return.
  if (costA < 0.0f || costB < 0.0f) {
    std::fprintf(stderr, "iccconnect-search-cost: failure sentinel observed after success\n");
    return 1;
  }

  std::fprintf(stdout, "iccconnect-search-cost: PASS  costA=%.6g costB=%.6g\n",
               (double)costA, (double)costB);
  return 0;
}
