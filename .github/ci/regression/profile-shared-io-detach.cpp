// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression test for issue #2128: the two CIccProfile paths that released an
// attached IO without notifying the profile's loaded tags.
//
// Background. CIccProfile::Detach() (IccProfLib/IccProfile.cpp) notifies tags
// before freeing its own IO:
//
//     for (i = m_Tags.begin(); i != m_Tags.end(); i++)
//       if (i->pTag) i->pTag->DetachIO();
//     delete m_pAttachIO;
//
// A tag can hold IO derived from that IO -- today only CIccTagEmbeddedProfile,
// whose inner profile CIccTagEmbeddedProfile::Read() attaches to a CIccEmbedIO
// wrapping the IO passed in by the reading profile. The DetachIO() override
// that makes this cascade work is covered by the sibling test
// embedded-profile-detachio.cpp. This test covers the two paths that override
// does NOT reach, because they never ran the notification loop at all:
//
//   Path 1 -- CopyAttach(nullptr). The release path the library actually
//     exercises. IccCmm.cpp borrows the caller's IO with
//     CopyAttach(&Profile, true) and hands it back with CopyAttach(nullptr);
//     that release cleared m_pAttachIO with no notification whatsoever, so a
//     fix confined to Detach() would have left the live path untouched.
//
//   Path 2 -- Detach()'s shared-IO branch. Reachable by any caller that does
//     CopyAttach(p, true) and then Detach(). The notification loop sat inside
//     the owning branch, so the shared branch returned true having told the
//     tags nothing.
//
// In both cases the inner profile was left reporting HasIO()==true over an IO
// its parent had released -- a later FindTag() on it reads through that IO,
// and once the lender closes it, through freed memory.
//
// Semantics. On a shared release the borrowed IO is still alive; it belongs to
// the lender. The conservative choice implemented here releases the child
// anyway, because a borrower has no way to observe when the lender closes what
// it lent. That costs the child its ability to read further, which assertion 5
// below pins deliberately rather than by accident.
//
// Asserts:
//   1. PATH 1: after borrower->CopyAttach(nullptr), the embedded tag's inner
//      profile reports HasIO()==false.
//   2. PATH 2: same, released with a shared Detach() instead.
//   3. LOADED DATA SURVIVES: a tag loaded through the borrowed IO before the
//      release still holds its text afterwards -- releasing IO must not
//      destroy already-loaded tag objects.
//   4. THE LENDER IS UNHARMED: after the borrower releases, the lender still
//      reports HasIO()==true and can still load a tag it had not loaded yet.
//      Borrowed IO must be dropped, never deleted; this assertion is what
//      would catch the shared branch being "simplified" into the owning one.
//   5. NO BOGUS SUCCESS: FindTag() on a tag the inner profile never loaded
//      returns NULL after the release rather than reading through the IO.
//   6. m_bSharedIO HYGIENE: Cleanup() (and therefore operator=, which calls
//      it) resets m_bSharedIO. A formerly-shared profile used to sit at
//      m_bSharedIO==true with a NULL IO, where Detach() took the shared branch
//      and returned true having done nothing. It must now return false.
//   7. ATTACH TAKES OWNERSHIP: Attach() clears the flag too. It calls
//      Cleanup() only when tags are already present, so a profile that had
//      just borrowed an IO (no tags yet) and was then attached to one of its
//      own kept the flag set, and Cleanup() at destruction skipped the delete
//      -- a leak of an IO the profile owned, reachable through public API
//      alone and NOT closed by fixing Cleanup(). Observed by counting the
//      IO's destructor, not by leak detection, so it bites under CI's
//      detect_leaks=0 sanitizer legs.
//   8. NO RELEASE, NO NOTIFY: the notification is gated on m_pAttachIO, not on
//      m_bSharedIO. Borrowing from a profile that holds no IO leaves this one
//      flagged as sharing with nothing attached; releasing then frees nothing,
//      so an inner profile carrying its own independently attached IO must be
//      left alone. Guards the hoisted loop against over-reaching.
//   9. READ CLEARS STALE IO: Read()/ReadValidate() carry the same tags-only
//      Cleanup() guard, so a profile that borrowed an IO before it had tags
//      carried that pointer through the read. HasIO() then lied, which makes
//      CIccTagEmbeddedProfile::Read() defer to it and attach an inner profile
//      over an IO the Read() caller owns and destroys on return.
//
// Entirely in-memory -- no corpus fixture, so this test cannot be silently
// skipped by a missing or unbuilt fixture.

#include "IccIO.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagEmbedIcc.h"
#include "IccUtil.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>
#include <memory>
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

// Counts its own destruction, so assertion 7 can observe whether the profile
// freed an IO it owns without depending on a leak detector -- the CI
// sanitizer legs run with detect_leaks=0, so a LeakSanitizer-only assertion
// would never bite there.
static int g_nOwnedIoDestroyed = 0;

class CCountingMemIO : public CIccMemIO
{
public:
  virtual ~CCountingMemIO() { g_nOwnedIoDestroyed++; }
};

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

// Outer profile carrying an embedded profile plus two text tags of its own.
// The outer copyrightTag is loaded before the release (assertion 3); the outer
// charTargetTag is deliberately left unloaded so assertion 5 has something
// that must come back NULL afterwards. The inner profile carries a
// copyrightTag it never loads, for the same purpose one level down.
static CIccProfile *makeOuter()
{
  CIccProfile *pInner = new CIccProfile();
  initHeader(pInner);
  pInner->AttachTag(icSigCopyrightTag, makeText("inner-cprt"));

  CIccProfile *pOuter = new CIccProfile();
  initHeader(pOuter);
  pOuter->AttachTag(icSigCopyrightTag, makeText("outer-cprt"));
  pOuter->AttachTag(icSigCharTargetTag, makeText("outer-target"));

  CIccTagEmbeddedProfile *pEmbed = new CIccTagEmbeddedProfile();
  pEmbed->SetProfile(pInner); // takes ownership
  pOuter->AttachTag(icSigEmbeddedV5ProfileTag, pEmbed);

  return pOuter;
}

// Serialize a fixture profile into pMem so it can be re-opened with deferred
// tag loading -- the only way to get an inner profile that is attached to a
// CIccEmbedIO wrapping the reader's IO.
static bool writeFixture(CIccMemIO &mem)
{
  std::unique_ptr<CIccProfile> pOuter(makeOuter());

  if (!mem.Alloc(64 * 1024, true))
    return false;

  return pOuter->Write(&mem, icNeverWriteID);
}

// Exercises one release path end to end. bUseDetach selects Path 2 (a shared
// Detach()) over Path 1 (CopyAttach(nullptr)).
static void checkSharedRelease(bool bUseDetach)
{
  const char *szPath = bUseDetach ? "Detach()" : "CopyAttach(nullptr)";
  char szMsg[256];

  CIccMemIO written;
  if (!writeFixture(written)) {
    std::printf("FAIL: [%s] could not build the in-memory fixture\n", szPath);
    g_failures++;
    return;
  }

  // The lender owns the file IO; the borrower shares it, exactly as
  // IccCmm.cpp's AddXform path does.
  std::unique_ptr<CIccMemIO> pSource(new CIccMemIO);
  if (!pSource->Attach(written.GetData(), written.GetLength(), false)) {
    std::printf("FAIL: [%s] could not attach the fixture bytes\n", szPath);
    g_failures++;
    return;
  }

  CIccProfile lender;
  if (!lender.Attach(pSource.get())) {
    std::printf("FAIL: [%s] lender profile would not attach\n", szPath);
    g_failures++;
    return;
  }
  pSource.release(); // Attach() took ownership only now that it succeeded

  std::unique_ptr<CIccProfile> pBorrower(lender.NewCopy());
  pBorrower->CopyAttach(&lender, true);

  // Load through the borrowed IO: the embedded tag (which attaches the inner
  // profile to a CIccEmbedIO over the lender's IO) and a text tag whose data
  // must survive the release.
  CIccTag *pEmbedTag = pBorrower->FindTag(icSigEmbeddedV5ProfileTag);
  if (!pEmbedTag || pEmbedTag->GetType() != icSigEmbeddedProfileType) {
    std::printf("FAIL: [%s] embedded profile tag did not load through the borrowed IO\n", szPath);
    g_failures++;
    return;
  }
  CIccTagEmbeddedProfile *pEmbed = (CIccTagEmbeddedProfile*)pEmbedTag;
  CIccProfile *pInner = pEmbed->GetProfile();

  CIccTag *pCprt = pBorrower->FindTag(icSigCopyrightTag);
  std::string sCprtBefore;
  if (pCprt && pCprt->GetType() == icSigTextType)
    sCprtBefore = ((CIccTagText*)pCprt)->GetText();

  // Anti-vacuity: if the fixture never gave the inner profile IO in the first
  // place, every assertion below would pass for the wrong reason.
  if (!pInner || !pInner->HasIO()) {
    std::printf("FAIL: [%s] fixture vacuous -- inner profile never held IO to begin with\n", szPath);
    g_failures++;
    return;
  }
  if (sCprtBefore != "outer-cprt") {
    std::printf("FAIL: [%s] fixture vacuous -- outer copyrightTag did not load through the borrowed IO\n",
                szPath);
    g_failures++;
    return;
  }

  // --- The release itself ---
  if (bUseDetach) {
    std::snprintf(szMsg, sizeof(szMsg), "[%s] shared Detach() returns true", szPath);
    check(pBorrower->Detach(), szMsg);
  }
  else {
    pBorrower->CopyAttach(nullptr);
  }

  std::snprintf(szMsg, sizeof(szMsg), "[%s] borrower reports HasIO()==false after the release", szPath);
  check(!pBorrower->HasIO(), szMsg);

  // --- Assertions 1 and 2: the notification reached the embedded tag. ---
  std::snprintf(szMsg, sizeof(szMsg),
                "BUG FIX [%s]: inner profile reports HasIO()==false after the release "
                "(fails without the notification on this path)", szPath);
  check(!pInner->HasIO(), szMsg);

  // --- Assertion 3: loaded data survives. ---
  CIccTag *pCprtAfter = NULL;
  for (TagEntryList::iterator i = pBorrower->m_Tags.begin(); i != pBorrower->m_Tags.end(); ++i) {
    if (i->pTag && i->TagInfo.sig == icSigCopyrightTag) {
      pCprtAfter = i->pTag;
      break;
    }
  }
  std::snprintf(szMsg, sizeof(szMsg),
                "LOADED DATA SURVIVES [%s]: the copyrightTag loaded before the release is still "
                "in the directory with its text intact", szPath);
  check(pCprtAfter != NULL && pCprtAfter->GetType() == icSigTextType &&
        std::string(((CIccTagText*)pCprtAfter)->GetText()) == "outer-cprt", szMsg);

  // --- Assertion 5: no bogus success on what was never loaded. ---
  std::snprintf(szMsg, sizeof(szMsg),
                "NO BOGUS SUCCESS [%s]: FindTag() on the borrower's never-loaded charTargetTag "
                "returns NULL after the release", szPath);
  check(pBorrower->FindTag(icSigCharTargetTag) == NULL, szMsg);

  std::snprintf(szMsg, sizeof(szMsg),
                "NO BOGUS SUCCESS [%s]: FindTag() on the inner profile's never-loaded copyrightTag "
                "returns NULL after the release", szPath);
  check(pInner->FindTag(icSigCopyrightTag) == NULL, szMsg);

  // --- Assertion 4: the lender is unharmed. ---
  // Borrowed IO is dropped, never deleted. The lender had loaded nothing of
  // its own, so a successful load here proves its IO is still usable and not
  // merely still non-NULL.
  std::snprintf(szMsg, sizeof(szMsg), "LENDER UNHARMED [%s]: lender still reports HasIO()==true", szPath);
  check(lender.HasIO(), szMsg);

  CIccTag *pLenderCprt = lender.FindTag(icSigCopyrightTag);
  std::snprintf(szMsg, sizeof(szMsg),
                "LENDER UNHARMED [%s]: lender can still load a tag through its own IO after the "
                "borrower released it", szPath);
  check(pLenderCprt != NULL && pLenderCprt->GetType() == icSigTextType &&
        std::string(((CIccTagText*)pLenderCprt)->GetText()) == "outer-cprt", szMsg);
}

// Assertion 6. Cleanup() used to clear m_pAttachIO but leave m_bSharedIO set,
// so a reused profile claimed to be sharing an IO it no longer had. Detach()
// then took the shared branch and reported success having done nothing.
// operator= is the reachable route to Cleanup() from public API.
static void checkSharedFlagHygiene()
{
  CIccMemIO written;
  if (!writeFixture(written)) {
    std::printf("FAIL: [hygiene] could not build the in-memory fixture\n");
    g_failures++;
    return;
  }

  std::unique_ptr<CIccMemIO> pSource(new CIccMemIO);
  if (!pSource->Attach(written.GetData(), written.GetLength(), false)) {
    std::printf("FAIL: [hygiene] could not attach the fixture bytes\n");
    g_failures++;
    return;
  }

  CIccProfile lender;
  if (!lender.Attach(pSource.get())) {
    std::printf("FAIL: [hygiene] lender profile would not attach\n");
    g_failures++;
    return;
  }
  pSource.release(); // Attach() took ownership only now that it succeeded

  CIccProfile borrower;
  borrower.CopyAttach(&lender, true);
  check(borrower.HasIO(), "[hygiene] borrower holds the shared IO before reuse");

  // Reuse the borrower for something else. Cleanup() runs inside operator=.
  CIccProfile fresh;
  initHeader(&fresh);
  borrower = fresh;

  check(!borrower.HasIO(), "[hygiene] reused profile reports HasIO()==false");
  check(!borrower.Detach(),
        "SHARED FLAG RESET: Detach() on a reused, formerly-shared profile returns false "
        "(it used to take the shared branch and return true having done nothing)");

  // The lender must be untouched by all of this.
  check(lender.HasIO(), "[hygiene] lender still reports HasIO()==true");
}

// Assertion 7. The same stale flag, reached without Cleanup() ever running.
// Attach() calls Cleanup() only when tags are already present, and a profile
// that has just borrowed an IO via CopyAttach(p, true) has none -- so the flag
// survived into a profile that then owned its IO outright, and Cleanup() at
// destruction skipped the delete. Confirmed as a 48-byte LeakSanitizer leak
// before Attach() was made to clear the flag. Observed here by counting the
// IO's destructor rather than by leak detection, so it bites under CI's
// detect_leaks=0 sanitizer legs too.
static void checkAttachClearsSharedFlag()
{
  CIccMemIO written;
  if (!writeFixture(written)) {
    std::printf("FAIL: [attach-owns] could not build the in-memory fixture\n");
    g_failures++;
    return;
  }

  std::unique_ptr<CIccMemIO> pSource(new CIccMemIO);
  if (!pSource->Attach(written.GetData(), written.GetLength(), false)) {
    std::printf("FAIL: [attach-owns] could not attach the fixture bytes\n");
    g_failures++;
    return;
  }

  CIccProfile lender;
  if (!lender.Attach(pSource.get())) {
    std::printf("FAIL: [attach-owns] lender profile would not attach\n");
    g_failures++;
    return;
  }
  pSource.release(); // Attach() took ownership only now that it succeeded

  g_nOwnedIoDestroyed = 0;
  {
    CIccProfile p;
    p.CopyAttach(&lender, true);

    // Anti-vacuity: this route only matters because the borrower has no tags
    // yet, which is exactly why Attach() skips Cleanup() below.
    check(p.m_Tags.empty(),
          "[attach-owns] borrower has no tags after CopyAttach (so Attach() will skip Cleanup())");

    CCountingMemIO *pOwned = new CCountingMemIO;
    if (!pOwned->Attach(written.GetData(), written.GetLength(), false)) {
      std::printf("FAIL: [attach-owns] owned IO would not attach\n");
      g_failures++;
      delete pOwned;
      return;
    }
    if (!p.Attach(pOwned)) {
      std::printf("FAIL: [attach-owns] profile would not attach the owned IO\n");
      g_failures++;
      delete pOwned; // Attach() only takes ownership on success
      return;
    }
    // p leaves scope here: Cleanup() must delete the IO it now owns.
  }

  check(g_nOwnedIoDestroyed == 1,
        "ATTACH TAKES OWNERSHIP: a profile that borrowed an IO and was then attached to its own "
        "frees that IO at destruction (it used to keep the sharing flag set and leak it)");

  check(lender.HasIO(), "[attach-owns] lender still reports HasIO()==true");
}

// Assertion 8. The notification is gated on m_pAttachIO, not m_bSharedIO.
// CopyAttach(p, true) where p holds no IO leaves this profile flagged as
// sharing with nothing attached; releasing then frees nothing, so there is no
// parent-derived view for a tag to be holding. Notifying anyway would detach an
// inner profile that SetProfile() gave its own independently attached IO --
// CIccTagEmbeddedProfile::DetachIO() releases the inner profile whatever the
// origin of its IO -- which the pre-fix shared branch never did. Guards against
// the hoisted loop over-reaching.
static void checkNoReleaseNoNotify()
{
  CIccMemIO innerBytes;
  if (!writeFixture(innerBytes)) {
    std::printf("FAIL: [no-release] could not build the in-memory fixture\n");
    g_failures++;
    return;
  }

  // An inner profile holding IO of its own, not derived from any parent.
  std::unique_ptr<CIccProfile> pInner(new CIccProfile());
  std::unique_ptr<CIccMemIO> pInnerIO(new CIccMemIO);
  if (!pInnerIO->Attach(innerBytes.GetData(), innerBytes.GetLength(), false) ||
      !pInner->Attach(pInnerIO.get())) {
    std::printf("FAIL: [no-release] inner profile would not attach its own IO\n");
    g_failures++;
    return;
  }
  pInnerIO.release(); // Attach() took ownership only now that it succeeded

  CIccProfile *pInnerRaw = pInner.get();
  CIccProfile outer;
  initHeader(&outer);
  CIccTagEmbeddedProfile *pEmbed = new CIccTagEmbeddedProfile();
  pEmbed->SetProfile(pInner.release()); // takes ownership
  if (!outer.AttachTag(icSigEmbeddedV5ProfileTag, pEmbed)) {
    std::printf("FAIL: [no-release] could not attach the embedded tag\n");
    g_failures++;
    delete pEmbed;
    return;
  }

  // Borrow from a profile that has no IO: flagged shared, nothing attached.
  CIccProfile lenderWithoutIo;
  outer.CopyAttach(&lenderWithoutIo, true);
  check(!outer.HasIO(), "[no-release] borrowing from an IO-less profile leaves nothing attached");
  check(pInnerRaw->HasIO(), "[no-release] inner profile holds its own IO before the release");

  check(outer.Detach(), "[no-release] Detach() still reports true on the shared branch");
  check(pInnerRaw->HasIO(),
        "NO RELEASE, NO NOTIFY: releasing a profile that had nothing attached leaves an inner "
        "profile's own independently attached IO alone");
}

// Assertion 9. Read() and ReadValidate() carry the same tags-only Cleanup()
// guard Attach() does, so a profile that borrowed an IO before it had tags
// carried that borrowed pointer straight through the read. HasIO() then lied,
// which is what makes CIccTagEmbeddedProfile::Read() defer to it and attach an
// inner profile over an IO the Read() caller owns and destroys on return.
// Read() loads eagerly and attaches nothing, so HasIO() must be false after it.
static void checkReadClearsStaleBorrowedIo()
{
  CIccMemIO written;
  if (!writeFixture(written)) {
    std::printf("FAIL: [read-stale] could not build the in-memory fixture\n");
    g_failures++;
    return;
  }

  std::unique_ptr<CIccMemIO> pSource(new CIccMemIO);
  if (!pSource->Attach(written.GetData(), written.GetLength(), false)) {
    std::printf("FAIL: [read-stale] could not attach the fixture bytes\n");
    g_failures++;
    return;
  }

  CIccProfile lender;
  if (!lender.Attach(pSource.get())) {
    std::printf("FAIL: [read-stale] lender profile would not attach\n");
    g_failures++;
    return;
  }
  pSource.release(); // Attach() took ownership only now that it succeeded

  CIccProfile p;
  p.CopyAttach(&lender, true);
  check(p.HasIO() && p.m_Tags.empty(),
        "[read-stale] borrower holds the shared IO and has no tags (so Read() skips Cleanup())");

  CIccMemIO readFrom;
  if (!readFrom.Attach(written.GetData(), written.GetLength(), false)) {
    std::printf("FAIL: [read-stale] could not attach the read source\n");
    g_failures++;
    return;
  }

  check(p.Read(&readFrom), "[read-stale] Read() succeeds");
  check(!p.HasIO(),
        "READ CLEARS STALE IO: a profile that borrowed an IO before it had tags reports "
        "HasIO()==false after Read(), which loads eagerly and attaches nothing");

  check(lender.HasIO(), "[read-stale] lender still reports HasIO()==true");
}

int main()
{
  std::printf("--- Path 1: CopyAttach(nullptr) ---\n");
  checkSharedRelease(false);

  std::printf("\n--- Path 2: shared Detach() ---\n");
  checkSharedRelease(true);

  std::printf("\n--- m_bSharedIO hygiene ---\n");
  checkSharedFlagHygiene();

  std::printf("\n--- Attach() clears the sharing flag ---\n");
  checkAttachClearsSharedFlag();

  std::printf("\n--- releasing nothing notifies nobody ---\n");
  checkNoReleaseNoNotify();

  std::printf("\n--- Read() clears a stale borrowed IO ---\n");
  checkReadClearsStaleBorrowedIo();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nAll checks passed\n");
  return 0;
}
