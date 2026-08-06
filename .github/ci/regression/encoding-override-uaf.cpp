// Regression for #1991: icConvertEncodingProfile's local-override loop must copy
// the encoding profile's overrides into the base parameter struct, not delete and
// re-attach them inside the struct it is iterating.
//
// When a colour-encoding profile's referenceName is not "ISO 22028-1", the
// converter loads a base encoding profile named by its colorSpaceName, copies that
// profile's colorEncodingParams struct into pParams, and is then supposed to layer
// the profile's own colorEncodingParams over it as local overrides. The loop that
// did the layering read:
//
//     for (entry=pTags->begin(); entry!=pTags->end(); entry++) {
//       if (entry->pTag) {
//         pStruct->DeleteElem(entry->TagInfo.sig);
//         pStruct->AttachElem(entry->TagInfo.sig, entry->pTag->NewCopy());
//       }
//     }
//
// Both mutations targeted pStruct -- pEncodeIcc's own struct, the list being
// walked -- rather than pParams. CIccTagStruct::DeleteElem erases the list node
// and deletes the tag (IccTagComposite.cpp:865), so that produced three defects at
// once:
//
//   1. Use-after-free. `entry` names the node just erased, and the next expression
//      reads entry->TagInfo.sig and entry->pTag back out of it. Under ASAN this is
//      a heap-use-after-free read of the IccTagEntry node, which CIccTagStruct::Read
//      allocated from profile-controlled input.
//   2. Erased-iterator walk. entry++ advances an iterator that is no longer in the
//      list, so when the read does not fault the loop does not terminate. Driving
//      the same defect through iccApplyToLink with a file-read profile hangs for
//      minutes rather than faulting, which is why the ctest entry carries a
//      timeout even though this binary itself faults promptly in both build kinds.
//   3. The override never applied. pParams was never written, so the loop only
//      deleted each element and re-attached a copy of itself. The local-override
//      path was a no-op even when it did not crash.
//
// Defect 3 is the one with a build-independent contract, and it is what this test
// asserts positively: the value the converter receives in pParams must be the
// profile's override, not the base profile's. Asserting that also pins defects 1
// and 2, because the fix that makes the override arrive is precisely the one that
// stops the loop mutating the list it iterates.
//
// Measured against the unfixed library, the assertions are not what reports first
// -- the call faults before returning, in both build kinds:
//   - ASAN (the lanes CI runs ctest in): heap-use-after-free, a 4-byte read of the
//     freed IccTagEntry node at IccEncoding.cpp:666, freed at :665.
//   - unsanitized clang Debug: segmentation fault, exit 139.
// The value and identity checks below therefore carry the contract for any build
// where the read happens to land on still-mapped memory, and the fault carries it
// everywhere else.
//
// The test drives the real icConvertEncodingProfile but replaces both of its
// collaborators, so it needs no files and no particular working directory:
//   - the cache handler returns an in-memory base profile instead of resolving
//     "ISO22028-Encoded-<name>.icc" relative to the caller's cwd;
//   - the converter captures pParams rather than building a profile from it.
//
// Returns 0 on success; non-zero on failure.

#include "IccEncoding.h"
#include "IccProfile.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"

#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

// Deliberately different so a discarded override is visible as a value, not just
// as a sanitizer report: the base supplies 100, the profile overrides it with 250.
const icFloatNumber kBaseLuminance     = 100.0f;
const icFloatNumber kOverrideLuminance = 250.0f;

// Attach a single-valued Float32 member. GetElemNumberValue reads element 0 of any
// num-array member, which is how the converter reads 'wlum' at IccEncoding.cpp:420.
// Ownership passes to the struct on success.
bool attachNumber(CIccTagStruct *pStruct, icSignature sig, icFloatNumber val)
{
  CIccTagFloat32 *pTag = (CIccTagFloat32 *)CIccTag::Create(icSigFloat32ArrayType);
  if (!pTag)
    return false;

  if (!pTag->SetSize(1)) {
    delete pTag;
    return false;
  }
  (*pTag)[0] = val;

  if (!pStruct->AttachElem(sig, pTag)) {
    delete pTag;
    return false;
  }

  return true;
}

bool attachText(CIccProfile *pIcc, icSignature sig, const char *szText)
{
  CIccTagUtf8Text *pTag = (CIccTagUtf8Text *)CIccTag::Create(icSigUtf8TextType);
  if (!pTag)
    return false;

  pTag->SetText(szText);

  if (!pIcc->AttachTag(sig, pTag)) {
    delete pTag;
    return false;
  }

  return true;
}

CIccTagStruct *buildParams(icFloatNumber luminance)
{
  CIccTagStruct *pParams = (CIccTagStruct *)CIccTag::Create(icSigTagStructType);
  if (!pParams)
    return NULL;

  if (!pParams->SetTagStructType(icSigColorEncodingParamsSruct) ||
      !attachNumber(pParams, icSigCeptWhitePointLuminanceMbr, luminance)) {
    delete pParams;
    return NULL;
  }

  return pParams;
}

// Stands in for the file-backed default handler. icConvertEncodingProfile takes
// ownership of nothing here -- it copies the params struct out and deletes the
// profile it was handed -- so a fresh profile is returned on every call.
class CaptureCacheHandler : public IIccEncProfileCacheHandler
{
public:
  virtual CIccProfile *GetEncodingProfile(const icUChar * /*szColorSpaceName*/)
  {
    CIccProfile *pIcc = new CIccProfile();
    if (!pIcc)
      return NULL;

    pIcc->m_Header.deviceClass = icSigColorEncodingClass;

    CIccTagStruct *pParams = buildParams(kBaseLuminance);
    if (!pParams || !pIcc->AttachTag(icSigColorEncodingParamsTag, pParams)) {
      delete pParams;
      delete pIcc;
      return NULL;
    }

    return pIcc;
  }
};

// Records what the override loop actually produced. Returning a status without
// building a profile is enough: the caller passes it straight back, which also
// confirms the call reached the converter rather than short-circuiting earlier.
class CaptureConverter : public IIccEncProfileConverter
{
public:
  CaptureConverter() : m_bCalled(false), m_luminance(0.0f) {}

  virtual icStatusEncConvert ConvertFromParams(CIccProfilePtr &newIcc,
                                               CIccTagStruct *pParams,
                                               icHeader * /*pHeader*/)
  {
    newIcc = NULL;
    m_bCalled = true;
    if (pParams)
      m_luminance = pParams->GetElemNumberValue(icSigCeptWhitePointLuminanceMbr, 0.0f);

    return icEncConvertBadParams;
  }

  bool m_bCalled;
  icFloatNumber m_luminance;
};

int runOverrideTest()
{
  // Both setters take ownership -- each deletes the handler it replaces, and each
  // ignores a null argument -- so these must be heap objects and cannot be
  // uninstalled afterwards. The converter is kept to read its capture back out.
  CaptureConverter *pConverter = new CaptureConverter();

  IIccEncProfileCacheHandler::SetEncCacheHandler(new CaptureCacheHandler());
  IIccEncProfileConverter::SetEncProfileConverter(pConverter);

  CIccProfile icc;
  icc.m_Header.deviceClass = icSigColorEncodingClass;

  // Anything other than "ISO 22028-1" selects the base-profile-plus-overrides
  // branch; the colorSpaceName is what the cache handler would resolve to a file.
  if (!attachText(&icc, icSigReferenceNameTag, "local override test") ||
      !attachText(&icc, icSigColorSpaceNameTag, "TestSpace")) {
    printf("[FAIL] could not build the encoding profile's text tags\n");
    return 1;
  }

  CIccTagStruct *pOverrides = buildParams(kOverrideLuminance);
  if (!pOverrides || !icc.AttachTag(icSigColorEncodingParamsTag, pOverrides)) {
    printf("[FAIL] could not build the override params struct\n");
    delete pOverrides;
    return 1;
  }

  // Captured before the call so the check below never dereferences the tag --
  // against the unfixed library that object is freed, and comparing addresses
  // stays defined where reading through one would not.
  const CIccTag *pBefore = pOverrides->FindElem(icSigCeptWhitePointLuminanceMbr);
  if (!pBefore) {
    printf("[FAIL] the override member is missing before the call\n");
    return 1;
  }

  CIccProfilePtr newIcc = NULL;
  icStatusEncConvert stat = icConvertEncodingProfile(newIcc, &icc);
  delete newIcc;

  int rc = 0;

  if (!pConverter->m_bCalled) {
    printf("[FAIL] the converter was never reached (status %d)\n", (int)stat);
    return 1;
  }

  // The defect: the override was dropped and the base value survived.
  if (pConverter->m_luminance != kOverrideLuminance) {
    printf("[FAIL] the override did not reach pParams: expected %g, got %g\n",
           (double)kOverrideLuminance, (double)pConverter->m_luminance);
    rc = 1;
  }

  // The source struct is the caller's, and the converter has no business
  // rewriting it. The unfixed loop deleted this element and attached a copy, so
  // the entry survives with a different tag pointer -- after the original has
  // already been freed and read back through.
  const CIccTag *pAfter = pOverrides->FindElem(icSigCeptWhitePointLuminanceMbr);
  if (pAfter != pBefore) {
    printf("[FAIL] the encoding profile's own override element was replaced\n");
    rc = 1;
  }

  if (!rc)
    printf("[PASS] the local override reached pParams and the source struct was left intact\n");

  return rc;
}

}  // namespace

int main()
{
  // The installed handlers stay owned by the library globals for the life of the
  // process; SetEnc*Handler ignores a null argument, so there is nothing to undo.
  return runOverrideTest();
}
