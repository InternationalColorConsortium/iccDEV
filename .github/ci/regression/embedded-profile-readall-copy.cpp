// Regression test for the three ownership/ordering invariants that
// wxProfileDump's "open embedded profile in its own window" feature depends
// on (Tools/wxWidget/wxProfileDump/wxProfileDump.cpp, MyChild::OnTagClicked).
//
// Double-clicking an embeddedV5ProfileTag ('ICC5', tag type 'ICCp') used to
// just flat-text-dump the tag. It now does:
//
//     if (pEmbed->ReadAll() && pEmbed->GetProfile()) {
//       CIccProfile *pCopy = pEmbed->GetProfile()->NewCopy();
//       ... hand pCopy to a new window, which owns and frees it ...
//     }
//
// Three things have to hold for that to be correct, and none of them are
// exercised anywhere else in the regression suite:
//
// 1. ORDERING: CIccTagEmbeddedProfile::Read() Attaches the embedded profile
//    to a CIccEmbedIO (deferred load) rather than reading it outright,
//    whenever the outer profile it came from has its own IO (i.e. was opened
//    lazily, the way OpenIccProfile()/wxProfileDump does it). Attach() only
//    reads the header and tag directory -- IccProfile.cpp's ReadBasic() populates
//    every m_Tags entry with pTag = NULL. CIccProfile's copy constructor
//    (IccProfile.cpp ~line 130) only carries over tags that are already
//    *loaded*: for each m_Tags entry it looks the tag up in the source's
//    loaded-tag list and copies NULL if it isn't there. So NewCopy() before
//    ReadAll() silently produces a copy with a full tag directory and no
//    usable tags -- a window built from that copy would show tag rows with
//    no readable content. ReadAll() has to run first.
//
// 2. POPULATION: after ReadAll(), the same profile's tag entries do carry
//    loaded tags, and a fresh NewCopy() taken at that point carries them too
//    -- this is the state the GUI actually hands to the new window.
//
// 3. INDEPENDENCE: the copy must be a real deep copy, not an alias. The GUI
//    closes the outer profile's window (and with it the whole outer
//    CIccProfile, which owns the CIccTagEmbeddedProfile, which owns the
//    original inner CIccProfile) while the embedded window -- built from the
//    copy -- stays open. If the copy shared any tag pointers or IO with the
//    original, freeing the outer profile would leave the embedded window
//    holding dangling pointers: at best garbage values, at worst a
//    use-after-free that only a sanitizer catches. This test deletes the
//    original outer profile and then reads through the copy, so a regression
//    here surfaces under ASan/LSan as a heap-use-after-free or -free, not
//    just as wrong output.
//
// Fixture: Testing/hybrid/BeyondRGB.icc, a tracked profile with exactly one
// embeddedV5ProfileTag (a v5 profile with 6 tags: desc, A2B3, wtpt, D2B3,
// B2D3, svcn), opened the same way wxProfileDump opens files -- via
// OpenIccProfile(), which defers tag loading -- so the embedded tag really is
// in the "attached but not read" state when FindTag() first returns it.
//
// Exit code 0 = pass, non-zero = fail.

#include "IccProfile.h"
#include "IccTagEmbedIcc.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <string>

static const char *kProfile = "Testing/hybrid/BeyondRGB.icc";

static int g_failures = 0;

static void check(bool bCondition, const char *szMessage)
{
  if (bCondition) {
    std::printf("ok:   %s\n", szMessage);
  }
  else {
    std::printf("FAIL: %s\n", szMessage);
    g_failures++;
  }
}

// Count total tag-directory entries and how many of them already carry a
// loaded tag object (entry.pTag != NULL). m_Tags is public; m_TagVals (the
// loaded-tag list itself) is protected, so entry.pTag is the correct public
// way to tell loaded tags from directory-only entries.
static void countTagEntries(const CIccProfile *pProfile, size_t &nTotal, size_t &nLoaded)
{
  nTotal = 0;
  nLoaded = 0;
  for (TagEntryList::const_iterator i = pProfile->m_Tags.begin();
       i != pProfile->m_Tags.end(); ++i) {
    nTotal++;
    if (i->pTag)
      nLoaded++;
  }
}

int main()
{
  CIccProfile *pOuter = OpenIccProfile(kProfile);
  if (!pOuter) {
    std::printf("FAIL: cannot open %s\n", kProfile);
    return 1;
  }

  CIccTag *pTag = pOuter->FindTag(icSigEmbeddedV5ProfileTag);
  check(pTag != NULL, "outer profile carries an embeddedV5ProfileTag");
  check(pTag && pTag->GetType() == icSigEmbeddedProfileType,
        "the tag's type identifies it as CIccTagEmbeddedProfile");

  if (g_failures) {
    std::printf("\n%d check(s) FAILED -- fixture does not match expectations\n", g_failures);
    delete pOuter;
    return 1;
  }

  CIccTagEmbeddedProfile *pEmbed = (CIccTagEmbeddedProfile*)pTag;
  CIccProfile *pInner = pEmbed->GetProfile();
  check(pInner != NULL, "the embedded tag carries an inner profile object");
  if (!pInner) {
    std::printf("\n%d check(s) FAILED -- cannot continue without an inner profile\n", g_failures + 1);
    delete pOuter;
    return 1;
  }

  // --- Invariant 1: ordering matters -----------------------------------
  // Before ReadAll(), the inner profile's tag directory exists (Attach() read
  // it) but nothing in it is loaded yet.
  size_t nTotalBefore = 0, nLoadedBefore = 0;
  countTagEntries(pInner, nTotalBefore, nLoadedBefore);
  check(nTotalBefore > 0, "inner profile's tag directory is non-empty before ReadAll()");
  check(nLoadedBefore == 0, "no tags are loaded yet before ReadAll()");

  CIccProfile *pUnreadCopy = pInner->NewCopy();
  check(pUnreadCopy != NULL, "NewCopy() succeeds even with nothing loaded");
  if (pUnreadCopy) {
    size_t nTotalCopy = 0, nLoadedCopy = 0;
    countTagEntries(pUnreadCopy, nTotalCopy, nLoadedCopy);
    check(nTotalCopy == nTotalBefore,
          "a copy taken before ReadAll() still carries the full tag directory");
    check(nLoadedCopy == 0,
          "but none of that copy's tag entries carry a loaded tag (pTag == NULL) -- "
          "this is the property that makes ReadAll()-before-NewCopy() load-bearing");
    delete pUnreadCopy;
  }

  // --- Invariant 2: ReadAll() then NewCopy() gives a populated copy -----
  bool bReadAll = pEmbed->ReadAll();
  check(bReadAll, "ReadAll() succeeds on the inner profile");

  size_t nTotalAfter = 0, nLoadedAfter = 0;
  countTagEntries(pInner, nTotalAfter, nLoadedAfter);
  check(nTotalAfter == nTotalBefore, "ReadAll() does not change the tag directory size");
  check(nLoadedAfter == nTotalAfter, "ReadAll() loads every tag in the directory");

  CIccProfile *pLoadedCopy = pEmbed->GetProfile()->NewCopy();
  check(pLoadedCopy != NULL, "NewCopy() after ReadAll() succeeds");
  if (!pLoadedCopy) {
    std::printf("\n%d check(s) FAILED -- cannot continue without a populated copy\n", g_failures);
    delete pOuter;
    return 1;
  }

  size_t nTotalLoadedCopy = 0, nLoadedLoadedCopy = 0;
  countTagEntries(pLoadedCopy, nTotalLoadedCopy, nLoadedLoadedCopy);
  check(nTotalLoadedCopy == nTotalAfter, "the populated copy carries the full tag directory");
  check(nLoadedLoadedCopy == nTotalAfter, "and every entry in it carries a loaded tag");

  CIccTag *pDescInCopy = pLoadedCopy->FindTag(icSigProfileDescriptionTag);
  check(pDescInCopy != NULL,
        "a representative tag (profileDescriptionTag) is retrievable from the copy");

  // --- Invariant 3: the copy outlives the parent ------------------------
  // This mirrors the GUI: the outer window (and everything it owns -- the
  // embedded tag, and that tag's own original inner profile) can be closed
  // while the embedded window, built from pLoadedCopy, stays open.
  delete pOuter;
  pOuter = NULL;
  pTag = NULL;
  pEmbed = NULL;
  pInner = NULL;
  pDescInCopy = NULL; // now dangling -- do not touch; re-fetch from the copy below

  size_t nDescribed = 0;
  size_t nEntries = 0;
  for (TagEntryList::const_iterator i = pLoadedCopy->m_Tags.begin();
       i != pLoadedCopy->m_Tags.end(); ++i) {
    nEntries++;
    if (i->pTag) {
      std::string sDescription;
      i->pTag->Describe(sDescription, 100);
      if (!sDescription.empty())
        nDescribed++;
    }
  }
  check(nEntries > 0, "the independent copy still has tags to walk after the parent is freed");
  check(nDescribed == nEntries,
        "every tag in the copy can still be read (Describe() succeeds) after the parent is freed");

  CIccTag *pDescAgain = pLoadedCopy->FindTag(icSigProfileDescriptionTag);
  check(pDescAgain != NULL,
        "the representative tag is still retrievable from the copy after the parent is freed");

  delete pLoadedCopy;

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nall checks passed\n");
  return 0;
}
