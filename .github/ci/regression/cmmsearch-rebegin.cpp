/*
    File:       cmmsearch-rebegin.cpp

    Contains:   CTest helper for CIccCmmSearch::Begin() idempotency
                (issue #1940).

    CIccCmm::Begin() opens with "if (m_pApply) return icCmmStatOk;", so calling
    it twice on a CIccCmm is a defined no-op.  CIccCmmSearch::Begin() overrode it
    without that guard while its body destroys state a second pass re-reads: the
    two-profile branch deletes m_pDstProfile and sets it to nullptr, and the
    -INIT initial-destination profile is nulled once consumed, but m_nAttached is
    left describing the profile set that no longer exists.  The second pass then
    bound those null members to the reference AddXform overload.

    Callers cannot route around this by knowing which class they hold:
    CIccConnectCmm stores a CIccCmm* and only dynamic_casts down in
    GetSearchCmm(), so a second Begin() on the base handle reached the override.

    Two sequences reproduced it against 6f58d47e, both reported by scan-build as
    core.NonNullParamChecker "Forming reference to null pointer":

      Begin(); Begin();                -> IccCmmSearch.cpp:423, null m_pDstProfile
      Begin(); AddXform(); Begin();    -> IccCmmSearch.cpp:461, null m_pMidProfile

    The second sequence is the same defect one step removed: AddXform's case 2
    does "m_pMidProfile = m_pDstProfile" to shift the chain along, and the first
    Begin() had already nulled m_pDstProfile, so it installs a null middle
    profile and reports icCmmStatOk.

    Both of those are the LOUD face.  With three chain profiles Begin() takes the
    else branch, which never deletes m_pDstProfile, so a second Begin() finds
    nothing null and survives: it returns icCmmStatOk, the transform results do
    not move, and it quietly re-runs -- pushing duplicate chains and leaking the
    CIccApplyCmm the first pass allocated (332 bytes in 6 allocations against
    6f58d47e). That face is invisible without leak detection, which is why the
    three-profile case is also registered as its own test with
    ASAN_OPTIONS=detect_leaks=1.

    This helper asserts the contract rather than merely surviving: the second
    Begin() must report icCmmStatOk *and* leave the CMM producing the results the
    first one built, which is what distinguishes a genuine no-op from a partial
    rebuild.

    Usage:
      cmmsearch-rebegin <profile.icc>

    Exit codes:
      0 - base and search CMMs both idempotent across a second Begin()
      1 - a Begin() reported failure, or the search results moved
      2 - usage error
*/

#include "IccCmm.h"
#include "IccCmmSearch.h"
#include "IccProfile.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

// The second Begin() is a no-op, so the search runs from identical state and
// must reproduce its own output exactly.  A tolerance is kept only so the test
// states a bound rather than relying on bit equality of an iterative search;
// it is three orders of magnitude below the 0.05 residual that
// cmmsearch-init-pcc allows for the search itself, so a rebuilt chain cannot
// slip through it.
static const icFloatNumber kDriftTolerance = 1.0e-5f;

static const icFloatNumber kSamples[][3] = {
  { 1.0f, 0.0f, 0.0f },
  { 0.0f, 1.0f, 0.0f },
  { 0.0f, 0.0f, 1.0f },
  { 1.0f, 1.0f, 1.0f },
  { 0.5f, 0.25f, 0.125f },
};
static const int kNumSamples = (int)(sizeof(kSamples) / sizeof(kSamples[0]));

class CFailOnceCmmSearch : public CIccCmmSearch
{
public:
  CFailOnceCmmSearch() : m_bFailNextApply(true) {}

  virtual CIccApplyCmm* GetNewApplyCmm(icStatusCMM& status) override
  {
    if (m_bFailNextApply) {
      m_bFailNextApply = false;
      status = icCmmStatAllocErr;
      return NULL;
    }

    return CIccCmmSearch::GetNewApplyCmm(status);
  }

private:
  bool m_bFailNextApply;
};

// Builds a two-profile search CMM over the same profile twice, which makes the
// search a device->device identity and keeps the assertion about Begin() rather
// than about color accuracy.
static CIccCmmSearch* buildSearchCmm(const char* profilePath,
                                     CIccCmmSearch* pSearch = NULL)
{
  std::unique_ptr<CIccCmmSearch> pCmm(pSearch ? pSearch : new CIccCmmSearch());

  CIccProfile* pSrc = OpenIccProfile(profilePath);
  CIccProfile* pDst = OpenIccProfile(profilePath);
  if (!pSrc || !pDst) {
    std::fprintf(stderr, "cmmsearch-rebegin: unable to open '%s'\n", profilePath);
    delete pSrc;
    delete pDst;
    return NULL;
  }

  // AddXform owns the profile on every path, including rejection (#1327), so
  // there is nothing to free here on failure.
  if (pCmm->AddXform(pSrc, icRelativeColorimetric) != icCmmStatOk ||
      pCmm->AddXform(pDst, icRelativeColorimetric) != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: AddXform failed for '%s'\n", profilePath);
    return NULL;
  }

  return pCmm.release();
}

// The control: the base class contract this override is being held to.  If this
// ever fails, the premise of the fix has changed and the guard needs revisiting
// rather than the search code.
static int checkBaseIdempotent(const char* profilePath)
{
  // Stack-scoped and driven through the filename overload, matching the shapes
  // the other regression helpers already compile on every lane.
  CIccCmm cmm;

  if (cmm.AddXform(profilePath, icRelativeColorimetric) != icCmmStatOk ||
      cmm.AddXform(profilePath, icRelativeColorimetric) != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  base AddXform failed\n");
    return 1;
  }

  icStatusCMM rv = cmm.Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  base Begin() #1 status %d\n", (int)rv);
    return 1;
  }

  rv = cmm.Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  base Begin() #2 status %d\n", (int)rv);
    return 1;
  }

  std::fprintf(stdout, "cmmsearch-rebegin: PASS  CIccCmm::Begin() is idempotent\n");
  return 0;
}

// Sequence 1: Begin(); Begin().  Pre-fix this bound a null m_pDstProfile to the
// reference AddXform overload at IccCmmSearch.cpp:423.
static int checkSearchRebegin(const char* profilePath)
{
  std::unique_ptr<CIccCmmSearch> pCmm(buildSearchCmm(profilePath));
  if (!pCmm)
    return 1;

  icStatusCMM rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  search Begin() #1 status %d\n", (int)rv);
    return 1;
  }

  // Capture what the CMM produces while it is known good, so the second Begin()
  // can be held to reproducing it rather than merely returning a status.
  icFloatNumber before[kNumSamples][3];
  for (int i = 0; i < kNumSamples; i++) {
    rv = pCmm->Apply(before[i], kSamples[i]);
    if (rv != icCmmStatOk) {
      std::fprintf(stderr, "cmmsearch-rebegin: FAIL  Apply #1 status %d for sample %d\n",
                   (int)rv, i);
      return 1;
    }
  }

  rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  search Begin() #2 status %d\n", (int)rv);
    return 1;
  }

  int failures = 0;
  for (int i = 0; i < kNumSamples; i++) {
    icFloatNumber after[3] = { 0.0f, 0.0f, 0.0f };

    rv = pCmm->Apply(after, kSamples[i]);
    if (rv != icCmmStatOk) {
      std::fprintf(stderr, "cmmsearch-rebegin: FAIL  Apply #2 status %d for sample %d\n",
                   (int)rv, i);
      failures++;
      continue;
    }

    icFloatNumber worst = 0.0f;
    for (int j = 0; j < 3; j++) {
      icFloatNumber diff = (icFloatNumber)std::fabs(after[j] - before[i][j]);
      if (diff > worst)
        worst = diff;
    }

    if (worst > kDriftTolerance) {
      std::fprintf(stderr,
        "cmmsearch-rebegin: FAIL  sample %d moved across Begin() #2 "
        "[%.6f %.6f %.6f] -> [%.6f %.6f %.6f] (max drift %.6f)\n",
        i, (double)before[i][0], (double)before[i][1], (double)before[i][2],
        (double)after[0], (double)after[1], (double)after[2], (double)worst);
      failures++;
    }
  }

  if (!failures)
    std::fprintf(stdout,
      "cmmsearch-rebegin: PASS  CIccCmmSearch::Begin() twice is a no-op over %d samples\n",
      kNumSamples);

  return failures ? 1 : 0;
}

// Begin(false) prepares the search chains without reserving a default Apply
// object. A subsequent Begin(true) must supply that object without rebuilding
// the chains, matching the base CMM's lazy-apply contract.
static int checkSearchDeferredApply(const char* profilePath)
{
  std::unique_ptr<CIccCmmSearch> pCmm(buildSearchCmm(profilePath));
  if (!pCmm)
    return 1;

  icStatusCMM rv = pCmm->Begin(false);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  search Begin(false) status %d\n", (int)rv);
    return 1;
  }

  rv = pCmm->Begin(true);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  search Begin(true) status %d\n", (int)rv);
    return 1;
  }

  icFloatNumber dst[3] = { 0.0f, 0.0f, 0.0f };
  rv = pCmm->Apply(dst, kSamples[0]);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  Apply after deferred Begin status %d\n", (int)rv);
    return 1;
  }

  std::fprintf(stdout,
    "cmmsearch-rebegin: PASS  Begin(false); Begin(true) supplies the default Apply\n");
  return 0;
}

// Force the default Apply allocation to fail after the search chains are
// valid. The next Begin(true) must retry that allocation without rebuilding
// or invalidating the chains.
static int checkSearchApplyAllocationRetry(const char* profilePath)
{
  std::unique_ptr<CIccCmmSearch> pCmm(
    buildSearchCmm(profilePath, new CFailOnceCmmSearch()));
  if (!pCmm)
    return 1;

  icStatusCMM rv = pCmm->Begin(true);
  if (rv != icCmmStatAllocErr) {
    std::fprintf(stderr,
      "cmmsearch-rebegin: FAIL  forced Apply allocation returned status %d\n",
      (int)rv);
    return 1;
  }

  rv = pCmm->Begin(true);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr,
      "cmmsearch-rebegin: FAIL  Begin(true) retry returned status %d\n",
      (int)rv);
    return 1;
  }

  icFloatNumber dst[3] = { 0.0f, 0.0f, 0.0f };
  rv = pCmm->Apply(dst, kSamples[0]);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr,
      "cmmsearch-rebegin: FAIL  Apply after allocation retry returned status %d\n",
      (int)rv);
    return 1;
  }

  std::fprintf(stdout,
    "cmmsearch-rebegin: PASS  Begin(true) retries a failed Apply allocation\n");
  return 0;
}

// Sequence 2: Begin(); AddXform(); Begin().  The added profile shifts the chain
// through AddXform's case 2, which copies the already-nulled m_pDstProfile into
// m_pMidProfile; pre-fix the second Begin() then bound that null at
// IccCmmSearch.cpp:461.  The contract being asserted is the base one -- once
// Begin() has run, a further AddXform does not rebuild the CMM -- so the second
// Begin() must report icCmmStatOk and leave the built chain alone.
static int checkSearchAddXformAfterBegin(const char* profilePath)
{
  std::unique_ptr<CIccCmmSearch> pCmm(buildSearchCmm(profilePath));
  if (!pCmm)
    return 1;

  icStatusCMM rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  search Begin() #1 status %d\n", (int)rv);
    return 1;
  }

  CIccProfile* pThird = OpenIccProfile(profilePath);
  if (!pThird) {
    std::fprintf(stderr, "cmmsearch-rebegin: unable to reopen '%s'\n", profilePath);
    return 1;
  }

  // AddXform owns pThird on every path, so this leaks nothing regardless of the
  // status it reports.
  pCmm->AddXform(pThird, icRelativeColorimetric);

  rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr,
      "cmmsearch-rebegin: FAIL  Begin() after a post-Begin AddXform status %d\n", (int)rv);
    return 1;
  }

  // The chain must still work, not merely have avoided the null reference.
  icFloatNumber dst[3] = { 0.0f, 0.0f, 0.0f };
  rv = pCmm->Apply(dst, kSamples[0]);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr,
      "cmmsearch-rebegin: FAIL  Apply after a post-Begin AddXform status %d\n", (int)rv);
    return 1;
  }

  std::fprintf(stdout,
    "cmmsearch-rebegin: PASS  AddXform() after Begin() leaves the built chain intact\n");
  return 0;
}

// The quiet face of the same defect.  With three chain profiles Begin() takes
// the else branch, and that branch never deletes m_pDstProfile -- only the
// two-profile branch does -- so a second Begin() finds nothing null, SURVIVES,
// and returns icCmmStatOk with unchanged results.  What it does instead is
// re-run: it pushes another chain into m_dst_to_mid and m_src_to_mid and
// overwrites m_pApply, leaking the CIccApplyCmm the first pass allocated
// (IccCmmSearch.cpp:528).  Measured against 6f58d47e that is 332 bytes in 6
// allocations, and nothing about it is observable from the transform results,
// so this case ships green wherever leak detection is off.  It is registered
// separately with ASAN_OPTIONS=detect_leaks=1 for that reason.
static int checkSearchThreeProfileRebegin(const char* profilePath)
{
  std::unique_ptr<CIccCmmSearch> pCmm(new CIccCmmSearch());

  for (int i = 0; i < 3; i++) {
    CIccProfile* pProfile = OpenIccProfile(profilePath);
    if (!pProfile) {
      std::fprintf(stderr, "cmmsearch-rebegin: unable to open '%s'\n", profilePath);
      return 1;
    }
    if (pCmm->AddXform(pProfile, icRelativeColorimetric) != icCmmStatOk) {
      std::fprintf(stderr, "cmmsearch-rebegin: AddXform %d failed\n", i);
      return 1;
    }
  }

  // The three-profile branch reports icCmmStatBadConnection unless a weighted
  // connection condition is attached, so the shape needs one to be reachable.
  CIccProfile* pPcc = OpenIccProfile(profilePath);
  if (!pPcc || !pPcc->ReadPccTags()) {
    std::fprintf(stderr, "cmmsearch-rebegin: unable to read PCC tags from '%s'\n", profilePath);
    delete pPcc;
    return 1;
  }
  pPcc->Detach();
  // AttachPCC only takes ownership when it accepts, matching iccconnect-search-cost.
  if (pCmm->AttachPCC(pPcc, 1.0f) != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: AttachPCC failed\n");
    delete pPcc;
    return 1;
  }

  icStatusCMM rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  3-profile Begin() #1 status %d\n", (int)rv);
    return 1;
  }

  icFloatNumber before[3] = { 0.0f, 0.0f, 0.0f };
  rv = pCmm->Apply(before, kSamples[kNumSamples - 1]);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  3-profile Apply #1 status %d\n", (int)rv);
    return 1;
  }

  rv = pCmm->Begin();
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  3-profile Begin() #2 status %d\n", (int)rv);
    return 1;
  }

  icFloatNumber after[3] = { 0.0f, 0.0f, 0.0f };
  rv = pCmm->Apply(after, kSamples[kNumSamples - 1]);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "cmmsearch-rebegin: FAIL  3-profile Apply #2 status %d\n", (int)rv);
    return 1;
  }

  for (int j = 0; j < 3; j++) {
    icFloatNumber diff = (icFloatNumber)std::fabs(after[j] - before[j]);
    if (diff > kDriftTolerance) {
      std::fprintf(stderr,
        "cmmsearch-rebegin: FAIL  3-profile result moved across Begin() #2 (drift %.6f)\n",
        (double)diff);
      return 1;
    }
  }

  // The leak is the real assertion here and LeakSanitizer makes it, at exit,
  // only when this process runs with detect_leaks=1.
  std::fprintf(stdout,
    "cmmsearch-rebegin: PASS  3-profile Begin() twice is a no-op (leak asserted by LSan)\n");
  return 0;
}

int main(int argc, char** argv)
{
  if (argc < 2 || argc > 3) {
    std::fprintf(stderr, "usage: %s <profile.icc> [--three-profile-only]\n",
                 argv[0] ? argv[0] : "cmmsearch-rebegin");
    return 2;
  }

  const char* profilePath = argv[1];
  bool bThreeProfileOnly = (argc == 3 && !std::strcmp(argv[2], "--three-profile-only"));
  if (argc == 3 && !bThreeProfileOnly) {
    std::fprintf(stderr, "cmmsearch-rebegin: unknown option '%s'\n", argv[2]);
    return 2;
  }

  // The three-profile check has to run in a process of its own.  The two-profile
  // sequences below it bind a null reference on an unfixed library, and the
  // sanitizer halts on that first report -- so bundled together the quiet
  // three-profile leak would never be reached, and the leak test would be
  // reporting somebody else's failure.
  if (bThreeProfileOnly)
    return checkSearchThreeProfileRebegin(profilePath) ? 1 : 0;

  int failures = 0;
  failures += checkBaseIdempotent(profilePath);
  failures += checkSearchRebegin(profilePath);
  failures += checkSearchDeferredApply(profilePath);
  failures += checkSearchApplyAllocationRetry(profilePath);
  failures += checkSearchAddXformAfterBegin(profilePath);

  return failures ? 1 : 0;
}
