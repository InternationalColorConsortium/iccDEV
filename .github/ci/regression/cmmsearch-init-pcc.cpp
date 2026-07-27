/*
    File:       cmmsearch-init-pcc.cpp

    Contains:   CTest helper for CIccCmmSearch connection-condition resolution
                across the -INIT initial-destination profile (issue #1860).

    CIccCmmSearch::Begin() builds its sub-chains with two different
    CIccCmm::AddXform overloads.  The src/mid/dst profiles go in by reference,
    which copy-constructs the profile and then detaches the copy from its file
    IO, while the -INIT initial-destination profile goes in by pointer and keeps
    its IO.  A detached copy can no longer load spectralViewingConditions on
    demand, so CheckPCSConnections()/pushXYZConvert() saw the D50/2-degree
    default at one end of the chain and the profile's real PCC at the other.
    isEquivalentPcc() then reported a mismatch and spliced a chromatic-adaptation
    CIccPcsXform into mid_to_dst that the non--INIT chain does not have.

    For a v5 profile that declares a non-D50 illuminant in 'svcn' this turned an
    identity profile->itself search into a D50<->D65 adaptation.  Because the
    search is fenced by a large out-of-gamut barrier cost, the corrupted starting
    point could not be recovered from, and any color with a channel at 0.0 or 1.0
    came back wrong while interior colors stayed exact.

    This helper drives a profile->itself search with an initial-destination
    profile attached and asserts the round trip is the identity at the gamut
    corners.  Before the fix the corner samples miss by more than 0.04; after it
    they land within search tolerance.

    Usage:
      cmmsearch-init-pcc <profile.icc>

    Exit codes:
      0 - identity round trip observed at every sample
      1 - round trip diverged, or the CMM could not be built
      2 - usage error
*/

#include "IccCmmSearch.h"
#include "IccProfile.h"

#include <cmath>
#include <cstdio>
#include <memory>

// The search minimises a numerical cost rather than inverting analytically, so
// an exact match is not expected and the residual depends on the profile: the
// matrix/TRC v5 case lands within 0.0011 per channel, while inverting the v4
// LUT profile leaves up to 0.0174 at the white corner.  The spurious PCS
// adaptation this test guards against produced errors of 0.1386 to 0.5702, so a
// bound of 0.05 separates legitimate search residual from the defect with room
// on both sides.
static const icFloatNumber kTolerance = 0.05f;

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <profile.icc>\n",
                 argv[0] ? argv[0] : "cmmsearch-init-pcc");
    return 2;
  }

  const char* profilePath = argv[1];

  // Gamut corners plus one interior sample.  Only the corners regressed: an
  // interior color such as the last row stayed exact even with the spurious
  // adaptation in place, which is why the defect survived the existing tests.
  static const icFloatNumber kSamples[][3] = {
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 0.5f, 0.25f, 0.125f },
  };
  static const int kNumSamples = (int)(sizeof(kSamples) / sizeof(kSamples[0]));

  std::unique_ptr<CIccCmmSearch> pCmm(new CIccCmmSearch());

  // Two stages of the same profile: source and destination are identical, so the
  // search must return the device values it was handed.
  CIccProfile* pSrc = OpenIccProfile(profilePath);
  CIccProfile* pDst = OpenIccProfile(profilePath);
  if (!pSrc || !pDst) {
    std::fprintf(stderr, "cmmsearch-init-pcc: unable to open '%s'\n", profilePath);
    delete pSrc;
    delete pDst;
    return 1;
  }

  // AddXform owns the profile on every path, including rejection.
  if (pCmm->AddXform(pSrc, icRelativeColorimetric) != icCmmStatOk ||
      pCmm->AddXform(pDst, icRelativeColorimetric) != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-init-pcc: AddXform failed for '%s'\n", profilePath);
    return 1;
  }

  // This is the -INIT path: a separately opened copy of the last profile that
  // seeds the search's starting point.  Its presence is what used to change the
  // shape of mid_to_dst.
  CIccProfile* pInit = OpenIccProfile(profilePath);
  if (!pInit) {
    std::fprintf(stderr, "cmmsearch-init-pcc: unable to open initial destination '%s'\n",
                 profilePath);
    return 1;
  }
  pCmm->SetDstInitProfile(pInit, icRelativeColorimetric, icInterpLinear, NULL,
                          icXformLutColor, true);

  icStatusCMM rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-init-pcc: Begin() failed (status %d)\n", (int)rv);
    return 1;
  }

  int failures = 0;
  for (int i = 0; i < kNumSamples; i++) {
    icFloatNumber dst[3] = { 0.0f, 0.0f, 0.0f };

    rv = pCmm->Apply(dst, kSamples[i]);
    if (rv != icCmmStatOk) {
      std::fprintf(stderr, "cmmsearch-init-pcc: FAIL  Apply status %d for sample %d\n",
                   (int)rv, i);
      failures++;
      continue;
    }

    icFloatNumber worst = 0.0f;
    for (int j = 0; j < 3; j++) {
      icFloatNumber diff = (icFloatNumber)std::fabs(dst[j] - kSamples[i][j]);
      if (diff > worst)
        worst = diff;
    }

    if (worst > kTolerance) {
      std::fprintf(stderr,
        "cmmsearch-init-pcc: FAIL  sample %d [%.4f %.4f %.4f] -> [%.4f %.4f %.4f] (max err %.4f)\n",
        i, (double)kSamples[i][0], (double)kSamples[i][1], (double)kSamples[i][2],
        (double)dst[0], (double)dst[1], (double)dst[2], (double)worst);
      failures++;
    }
    else {
      std::fprintf(stdout,
        "cmmsearch-init-pcc: PASS  sample %d [%.4f %.4f %.4f] (max err %.4f)\n",
        i, (double)kSamples[i][0], (double)kSamples[i][1], (double)kSamples[i][2],
        (double)worst);
    }
  }

  return failures ? 1 : 0;
}
