// Regression test for CIccTagEmbeddedProfile::DetachIO() (IccProfLib/IccTagEmbedIcc.h/.cpp).
//
// The bug: CIccProfile::Detach() (IccProfLib/IccProfile.cpp:750-773) calls
// pTag->DetachIO() on every one of its own loaded tags BEFORE deleting its
// own m_pAttachIO:
//
//     for (i = m_Tags.begin(); i != m_Tags.end(); i++) {
//       if (i->pTag)
//         i->pTag->DetachIO();
//     }
//     delete m_pAttachIO;
//     m_pAttachIO = NULL;
//
// CIccTag::DetachIO() (IccProfLib/IccTagBasic.h) is an empty virtual, and
// nothing overrode it -- CIccTagEmbeddedProfile included. But an embedded
// profile tag's inner profile can itself hold IO: CIccTagEmbeddedProfile::Read
// (IccProfLib/IccTagEmbedIcc.cpp) does
//
//     CIccEmbedIO *pEmbedIO = new CIccEmbedIO();
//     pEmbedIO->Attach(pIO, size);
//     ...
//     if (pProfile && pProfile->HasIO())
//       m_pProfile->Attach(pEmbedIO);
//
// wrapping the PARENT's IO. So when the outer profile detaches and deletes
// that parent IO, the inner profile's CIccEmbedIO is left pointing at freed
// memory while inner->HasIO() still reports true -- any later FindTag() /
// FindAllTags() on the inner reads through it.
//
// The fix adds CIccTagEmbeddedProfile::DetachIO() { if (m_pProfile)
// m_pProfile->Detach(); }, so detaching the outer profile cascades into the
// inner one (and further, into anything nested below it that has actually
// been loaded).
//
// This test builds a 2-deep nested embedded profile in memory (pTop wraps
// pMid wraps pLeaf, matching the shapes in the gitignored measurement
// harness out/scratch-nestdepth/nestdepth.cpp), saves it to a scratch file,
// and reopens it the way wxProfileDump does -- via OpenIccProfile(), which
// defers tag loading. It then loads exactly what a caller following the
// "OpenIccProfile -> load what you need -> Detach" pattern would load before
// detaching:
//   - pTop's embeddedV5ProfileTag  (to reach pMid)
//   - pMid's copyrightTag           (representative loaded data)
//   - pMid's embeddedV5ProfileTag  (to reach pLeaf -- the 2nd nesting level)
// pMid also carries a THIRD tag (luminanceTag) that is deliberately left
// unloaded, so that after detaching there is something on pMid that must
// come back NULL/false rather than reading through freed memory.
//
// Asserts, matching task-6-brief.md's "Regression test" section:
//   1. THE BUG IS FIXED: after outer->Detach(), pMid->HasIO() == false.
//      This is the assertion that fails without the DetachIO() override.
//   2. NO CRASH, NO BOGUS SUCCESS: after the detach, pMid->FindTag() on its
//      still-unloaded luminanceTag returns NULL, and pMid->FindAllTags()
//      returns false (not every tag loaded, no IO left) -- not a read
//      through freed memory. The same is checked on pLeaf's never-loaded
//      copyrightTag for good measure.
//   3. LOADED DATA SURVIVES: pMid's copyrightTag, loaded before the detach,
//      is still there (pTag non-NULL in the directory) and its text is
//      unchanged after the detach -- detaching releases IO, it does not
//      destroy already-loaded tag objects.
//   4. THE CASCADE REACHES TWO LEVELS: a single pTop->Detach() leaves BOTH
//      pMid->HasIO() and pLeaf->HasIO() false, even though pLeaf's IO came
//      from pMid's IO, which itself came from pTop's IO.
//   5. NO DOUBLE FREE: deleting pTop at the end (which owns pMid, which owns
//      pLeaf, transitively, through the embedded-profile tags) exits cleanly.
//      A double free or leak here would surface under the CI sanitizer legs;
//      locally, a clean process exit 0 is what this test can show.
//
// Entirely in-memory (built and saved to a temp file at runtime) -- no
// corpus fixture, so this test can never be silently skipped by a missing
// or unbuilt fixture.

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagEmbedIcc.h"
#include "IccUtil.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>
#include <string>

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

// ---------------------------------------------------------------------------
// Fixture helpers, adapted from out/scratch-nestdepth/nestdepth.cpp (a
// gitignored measurement harness, not part of the repo).
// ---------------------------------------------------------------------------

static void initHeader(CIccProfile *p)
{
  p->m_Header.deviceClass = icSigAbstractClass;
  p->m_Header.colorSpace  = icSigLabData;
  p->m_Header.pcs         = icSigLabData;
  p->m_Header.version     = icVersionNumberV5;
  p->m_Header.magic       = icMagicNumber;
  p->m_Header.platform    = icSigMicrosoft;
  p->m_Header.renderingIntent = icPerceptual;
  p->m_Header.illuminant.X = 0x0000f6d6;
  p->m_Header.illuminant.Y = 0x00010000;
  p->m_Header.illuminant.Z = 0x0000d32d;
}

static CIccTagText *makeText(const char *s)
{
  CIccTagText *p = new CIccTagText();
  p->SetText(s);
  return p;
}

// Innermost profile: non-empty, and carries a tag (copyrightTag) that is
// deliberately never loaded by this test, so it can prove "unloaded tag
// after detach reads as NULL/false, not through freed memory" a second time
// at the second nesting level.
static CIccProfile *makeLeaf()
{
  CIccProfile *p = new CIccProfile();
  initHeader(p);
  p->AttachTag(icSigCopyrightTag, makeText("leaf-cprt"));
  return p;
}

// Wrap pInner in a new profile carrying it as an embeddedV5ProfileTag, plus
// its own copyrightTag.
static CIccProfile *wrap(CIccProfile *pInner, const char *szCopyrightText)
{
  CIccProfile *p = new CIccProfile();
  initHeader(p);

  p->AttachTag(icSigCopyrightTag, makeText(szCopyrightText));

  CIccTagEmbeddedProfile *pEmbed = new CIccTagEmbeddedProfile();
  pEmbed->SetProfile(pInner); // takes ownership
  p->AttachTag(icSigEmbeddedV5ProfileTag, pEmbed);

  return p;
}

// Fetch a CIccTagEmbeddedProfile* for sig out of pProfile's tag directory,
// loading it via FindTag() if necessary, and confirming the tag type along
// the way. Returns NULL (and records a failure) if anything along the path
// is not what is expected.
static CIccTagEmbeddedProfile *findEmbeddedProfileTag(CIccProfile *pProfile, icSignature sig,
                                                        const char *szWhat)
{
  CIccTag *pTag = pProfile->FindTag(sig);
  if (!pTag) {
    std::printf("FAIL: %s -- FindTag() returned NULL\n", szWhat);
    g_failures++;
    return NULL;
  }
  if (pTag->GetType() != icSigEmbeddedProfileType) {
    std::printf("FAIL: %s -- tag type 0x%08x is not an embedded profile\n",
                szWhat, (unsigned)pTag->GetType());
    g_failures++;
    return NULL;
  }
  return (CIccTagEmbeddedProfile*)pTag;
}

int main(int argc, char **argv)
{
  const char *szScratchPath =
      (argc > 1) ? argv[1] : "iccdev-embedded-profile-detachio-scratch.icc";

  // --- Build: pTop -> embeddedV5ProfileTag -> pMid -> embeddedV5ProfileTag -> pLeaf ---
  CIccProfile *pLeaf = makeLeaf();
  CIccProfile *pMid  = wrap(pLeaf, "mid-cprt");
  // A third tag on pMid that this test deliberately never loads before the
  // detach, so there is something to prove reads NULL/false afterwards
  // rather than succeeding through freed memory.
  pMid->AttachTag(icSigLuminanceTag, makeText("mid-extra-unloaded"));
  CIccProfile *pTop = wrap(pMid, "top-cprt");

  bool bSaved = SaveIccProfile(szScratchPath, pTop);
  delete pTop; // owns pMid, which owns pLeaf
  pTop = NULL;
  pMid = NULL;
  pLeaf = NULL;

  check(bSaved, "in-memory 2-level nested profile saves to a scratch file");
  if (!bSaved) {
    std::printf("\n%d check(s) FAILED -- cannot continue without the nested scratch profile\n", g_failures);
    return 1;
  }

  // --- Reopen lazily, like wxProfileDump / OpenIccProfile->FindAllTags->Detach do ---
  CIccProfile *pOuter = OpenIccProfile(szScratchPath);
  check(pOuter != NULL, "nested scratch profile re-opens (lazily)");
  if (!pOuter) {
    std::remove(szScratchPath);
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  CIccTagEmbeddedProfile *pEmbedMid =
      findEmbeddedProfileTag(pOuter, icSigEmbeddedV5ProfileTag, "outer's embeddedV5ProfileTag");
  if (!pEmbedMid) {
    delete pOuter;
    std::remove(szScratchPath);
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  CIccProfile *pMidProfile = pEmbedMid->GetProfile();
  check(pMidProfile != NULL, "outer's embedded tag carries an inner profile object (pMid)");
  if (!pMidProfile) {
    delete pOuter;
    std::remove(szScratchPath);
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  // Load pMid's copyrightTag (the "loaded data" that must survive the
  // detach) and pMid's own embeddedV5ProfileTag (the second nesting level).
  // pMid's luminanceTag is deliberately left unloaded.
  CIccTag *pMidCprtTag = pMidProfile->FindTag(icSigCopyrightTag);
  check(pMidCprtTag != NULL, "pMid's copyrightTag loads before the detach");

  std::string sMidCprtBefore;
  if (pMidCprtTag && pMidCprtTag->GetType() == icSigTextType) {
    sMidCprtBefore = ((CIccTagText*)pMidCprtTag)->GetText();
  }
  check(sMidCprtBefore == "mid-cprt", "pMid's copyrightTag reads back its original text before the detach");

  CIccTagEmbeddedProfile *pEmbedLeaf =
      findEmbeddedProfileTag(pMidProfile, icSigEmbeddedV5ProfileTag, "pMid's embeddedV5ProfileTag");
  if (!pEmbedLeaf) {
    delete pOuter;
    std::remove(szScratchPath);
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  CIccProfile *pLeafProfile = pEmbedLeaf->GetProfile();
  check(pLeafProfile != NULL, "pMid's embedded tag carries an inner profile object (pLeaf)");
  if (!pLeafProfile) {
    delete pOuter;
    std::remove(szScratchPath);
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  // --- Before detach: every level reports IO, nothing about pLeaf is loaded ---
  check(pOuter->HasIO(), "BEFORE detach: outer profile reports HasIO()==true");
  check(pMidProfile->HasIO(), "BEFORE detach: pMid reports HasIO()==true");
  check(pLeafProfile->HasIO(), "BEFORE detach: pLeaf reports HasIO()==true");

  size_t nLeafLoadedBefore = 0;
  for (TagEntryList::iterator i = pLeafProfile->m_Tags.begin(); i != pLeafProfile->m_Tags.end(); ++i) {
    if (i->pTag)
      nLeafLoadedBefore++;
  }
  check(nLeafLoadedBefore == 0, "BEFORE detach: nothing in pLeaf is loaded yet (its own tags were never asked for)");

  // --- The detach itself ---
  bool bDetached = pOuter->Detach();
  check(bDetached, "outer->Detach() returns true");

  // --- Assertion 1: THE BUG IS FIXED. Must fail without the DetachIO() override. ---
  check(!pMidProfile->HasIO(),
        "BUG FIX: pMid reports HasIO()==false after outer->Detach() cascaded into it "
        "(fails without CIccTagEmbeddedProfile::DetachIO())");

  // --- Assertion 4: the cascade reaches two levels. ---
  check(!pLeafProfile->HasIO(),
        "CASCADE: pLeaf (2 levels down) also reports HasIO()==false after a single outer->Detach()");
  check(!pOuter->HasIO(), "outer profile itself reports HasIO()==false after its own Detach()");

  // --- Assertion 2: no crash, no bogus success reading afterwards. ---
  // pMid's luminanceTag was never loaded, and pMid now has no IO -- FindTag()
  // must return NULL via the short-circuit in CIccProfile::FindTag(IccTagEntry&):
  // "if (!entry.pTag && m_pAttachIO) LoadTag(...)".
  //
  // Note what these two checks do and do not prove.  They lock in the correct
  // post-fix behavior: no IO left, so nothing is attempted and nothing bogusly
  // succeeds.  They do NOT by themselves prove the absence of a read through
  // freed memory -- without the DetachIO() override, pMid still has a live-looking
  // m_pAttachIO and these checks pass by luck, reading through the freed parent
  // IO (undefined behavior that happened to yield NULL).  Assertion 1's
  // HasIO()==false check is what actually catches that, and the sanitizer CI legs
  // are what would surface the read itself.
  CIccTag *pMidLumiAfter = pMidProfile->FindTag(icSigLuminanceTag);
  check(pMidLumiAfter == NULL,
        "NO BOGUS SUCCESS: FindTag() on pMid's still-unloaded luminanceTag returns NULL after the detach");

  bool bMidFindAllAfter = pMidProfile->FindAllTags();
  check(!bMidFindAllAfter,
        "NO BOGUS SUCCESS: pMid->FindAllTags() returns false after the detach "
        "(luminanceTag is still unloaded and there is no IO left to load it from)");

  // Same check one level further down, on pLeaf's never-loaded copyrightTag.
  CIccTag *pLeafCprtAfter = pLeafProfile->FindTag(icSigCopyrightTag);
  check(pLeafCprtAfter == NULL,
        "NO BOGUS SUCCESS: FindTag() on pLeaf's never-loaded copyrightTag returns NULL after the detach");

  bool bLeafFindAllAfter = pLeafProfile->FindAllTags();
  check(!bLeafFindAllAfter,
        "NO BOGUS SUCCESS: pLeaf->FindAllTags() returns false after the detach");

  // --- Assertion 3: loaded data survives the detach. ---
  bool bMidCprtStillInDirectory = false;
  CIccTag *pMidCprtTagAfter = NULL;
  for (TagEntryList::iterator i = pMidProfile->m_Tags.begin(); i != pMidProfile->m_Tags.end(); ++i) {
    if (i->TagInfo.sig == icSigCopyrightTag) {
      bMidCprtStillInDirectory = (i->pTag != NULL);
      pMidCprtTagAfter = i->pTag;
      break;
    }
  }
  check(bMidCprtStillInDirectory,
        "SURVIVES DETACH: pMid's copyrightTag directory entry still carries a loaded tag object");
  check(pMidCprtTagAfter == pMidCprtTag,
        "SURVIVES DETACH: it is the SAME tag object as before the detach (Detach() does not "
        "replace or delete already-loaded tags)");

  std::string sMidCprtAfter;
  if (pMidCprtTagAfter && pMidCprtTagAfter->GetType() == icSigTextType) {
    sMidCprtAfter = ((CIccTagText*)pMidCprtTagAfter)->GetText();
  }
  check(sMidCprtAfter == sMidCprtBefore && sMidCprtAfter == "mid-cprt",
        "SURVIVES DETACH: pMid's copyrightTag still reads back its original text after the detach");

  // --- Assertion 5: no double free. Tear everything down and exit cleanly. ---
  // Deleting pOuter recursively deletes pMid (owned by pOuter's embedded
  // profile tag) and pLeaf (owned by pMid's), and each profile's own
  // Cleanup() sees m_pAttachIO == NULL post-detach, so nothing double-frees.
  delete pOuter;
  std::remove(szScratchPath);

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nall checks passed\n");
  return 0;
}
