// Regression for #1985: CIccDefaultEncProfileConverter::ConvertFromParams must
// not free the colorEncodingParams elements it only borrows.
//
// The converter reads the luma/chroma matrix out of the caller's parameter
// struct with
//
//     CIccTagFloat32 *pLumMtx = (CIccTagFloat32*)pParams->FindElemOfType(
//                                 icSigCeptLumaChromaMatrixMbr, icSigFloat32ArrayType);
//
// CIccTagStruct::FindElemOfType returns pEntry->pTag directly -- a pointer
// borrowed from the struct, which keeps the entry and its ownership. Every other
// element the function reads through that same call (the white point, the media
// white point, the transfer and inverse-transfer curves, the three primaries and
// the ambient chromaticity) is left alone for that reason. The luma matrix was
// the single exception: it was deleted after its values had been copied out,
// while the entry stayed in the struct's element list.
//
// That left a dangling entry behind. The owner destroys the struct afterwards --
// icConvertEncodingProfile does exactly that, and so does this test -- and
// CIccTagStruct::Cleanup walks the list doing
//
//     i->ptr->SetParentObject(nullptr);   // write into the freed chunk
//     delete i->ptr;                      // virtual dispatch, then second free
//
// so the premature free became a heap use-after-free write followed by a
// virtual-destructor dispatch through a vptr read out of freed memory, and then
// a second free of the same chunk. The parameters are profile-controlled: a
// colour-encoding-class profile reaches this through CIccXform::Create, which
// calls icConvertEncodingProfile at IccCmm.cpp:559.
//
// Note what detection depends on here, because it is unlike the #1980 test in
// the same area. A premature free cannot be observed without touching the freed
// memory, so there is no build-independent contract check for the defect itself.
// What is asserted unconditionally is the post-fix contract: the borrowed element
// survives the call intact, and the struct then destroys cleanly.
//
// Measured against the unfixed library, both build kinds do report:
//   - ASAN (the lanes CI runs ctest in): the read-back below is flagged as
//     heap-use-after-free before the struct is even destroyed.
//   - unsanitized clang Debug: segmentation fault, exit 139.
// The second is not guaranteed in principle -- it depends on the allocator
// recycling the chunk, which is also what defeats glibc's tcache double-free
// check -- so the ASAN lanes are the authoritative ones. It is recorded because
// it means an unsanitized lane still carries signal rather than none.
//
// Both cases below arm the same dangling entry, and each was red-tested on its
// own: ASAN aborts on the first report regardless of halt_on_error (continuing
// would need -fsanitize-recover=address), so a single red run only ever
// demonstrates whichever case runs first. The second case exists because the
// delete sat in the unconditional flow of the luma-matrix branch, ahead of any
// decision about the return status, so a conversion that fails afterwards left
// the caller holding the same corrupted struct as one that succeeds.
//
// Returns 0 on success; non-zero on failure.

#include "IccEncoding.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

// The trigger. Identity, so the icMatrixInvert3x3 the converter performs on the
// copied-out values for the B2A side succeeds and the success case really does
// reach the end of the function.
const icFloatNumber kLumaMtx[9] = {
  1.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 1.0f
};

// Attach a Float32 member holding the supplied values. Ownership passes to the
// struct on success -- which is the whole point of this test.
bool attachFloats(CIccTagStruct *pParams, icSignature sig,
                  const icFloatNumber *vals, icUInt32Number n)
{
  CIccTagFloat32 *pTag = (CIccTagFloat32 *)CIccTag::Create(icSigFloat32ArrayType);
  if (!pTag)
    return false;

  if (!pTag->SetSize(n)) {
    delete pTag;
    return false;
  }

  for (icUInt32Number i = 0; i < n; i++)
    (*pTag)[i] = vals[i];

  if (!pParams->AttachElem(sig, pTag)) {
    delete pTag;
    return false;
  }

  return true;
}

// Build a colorEncodingParams struct carrying the luma/chroma matrix. The white
// point is required up front, before the converter allocates anything. bPrimaries
// selects how far the conversion gets after the matrix has been read: with the
// three primaries present it runs to completion, without them it fails at the red
// primary lookup -- which is downstream of the borrowed element either way.
CIccTagStruct *buildParams(bool bPrimaries)
{
  CIccTagStruct *pParams = (CIccTagStruct *)CIccTag::Create(icSigTagStructType);
  if (!pParams)
    return NULL;

  if (!pParams->SetTagStructType(icSigColorEncodingParamsSruct)) {
    delete pParams;
    return NULL;
  }

  const icFloatNumber white[2] = {0.3127f, 0.3290f};
  const icFloatNumber red[2]   = {0.6400f, 0.3300f};
  const icFloatNumber green[2] = {0.3000f, 0.6000f};
  const icFloatNumber blue[2]  = {0.1500f, 0.0600f};

  bool ok =
    attachFloats(pParams, icSigCeptWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptMediumWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptLumaChromaMatrixMbr, kLumaMtx, 9);

  if (ok && bPrimaries) {
    ok = attachFloats(pParams, icSigCeptRedPrimaryXYZMbr, red, 2) &&
         attachFloats(pParams, icSigCeptGreenPrimaryXYZMbr, green, 2) &&
         attachFloats(pParams, icSigCeptBluePrimaryXYZMbr, blue, 2);
  }

  if (!ok) {
    delete pParams;
    return NULL;
  }

  return pParams;
}

// Convert once, then check that the struct still owns an intact luma/chroma
// matrix, then destroy it. Returns 0 on success.
int runCase(const char *szName, bool bPrimaries, bool bExpectProfile)
{
  CIccTagStruct *pParams = buildParams(bPrimaries);
  if (!pParams) {
    std::fprintf(stderr, "[encoding-lumamtx] %s: could not build params struct\n",
                 szName);
    return 1;
  }

  icHeader hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.deviceClass = icSigColorSpaceClass;
  hdr.colorSpace = icSigRgbData;
  hdr.pcs = icSigXYZData;
  hdr.version = icVersionNumberV5;
  hdr.renderingIntent = icPerceptual;

  IIccEncProfileConverter *pConverter = IIccEncProfileConverter::GetHandler();
  if (!pConverter) {
    std::fprintf(stderr, "[encoding-lumamtx] %s: no encoding converter handler\n",
                 szName);
    delete pParams;
    return 1;
  }

  CIccProfilePtr newIcc = NULL;
  icStatusEncConvert stat = pConverter->ConvertFromParams(newIcc, pParams, &hdr);

  int rc = 0;

  if ((newIcc != NULL) != bExpectProfile) {
    std::fprintf(stderr, "[encoding-lumamtx] FAIL: %s: %s (status %d)\n", szName,
                 bExpectProfile ? "expected a converted profile, got none"
                                : "expected no profile, got one",
                 (int)stat);
    rc = 2;
  }

  // The element must still be listed by the struct. Pre-fix this lookup returns
  // the freed pointer rather than NULL, because the delete never touched the
  // element list -- so a NULL here is not the failure mode being guarded against,
  // it just means the struct stopped owning the entry some other way.
  CIccTagFloat32 *pCheck = (CIccTagFloat32 *)pParams->FindElemOfType(
                             icSigCeptLumaChromaMatrixMbr, icSigFloat32ArrayType);
  if (!pCheck) {
    std::fprintf(stderr,
                 "[encoding-lumamtx] FAIL: %s: luma/chroma element missing from "
                 "the params struct after conversion\n", szName);
    rc = 3;
  }
  else if (pCheck->GetSize() < 9) {
    std::fprintf(stderr,
                 "[encoding-lumamtx] FAIL: %s: luma/chroma element shrank to %u "
                 "values\n", szName, (unsigned)pCheck->GetSize());
    rc = 4;
  }
  else {
    // Reads the borrowed element. Under ASAN this is the heap-use-after-free
    // report pre-fix; unsanitized it is a best-effort value check.
    icFloatNumber readBack[9];
    memset(readBack, 0, sizeof(readBack));
    pCheck->GetValues(&readBack[0], 0, 9);

    for (int i = 0; i < 9; i++) {
      if (readBack[i] != kLumaMtx[i]) {
        std::fprintf(stderr,
                     "[encoding-lumamtx] FAIL: %s: luma/chroma value %d is %g, "
                     "expected %g -- the converter freed an element it borrowed\n",
                     szName, i, (double)readBack[i], (double)kLumaMtx[i]);
        rc = 5;
        break;
      }
    }
  }

  // The teardown the owner performs, and where the dangling entry turned into a
  // use-after-free write and a second free: icConvertEncodingProfile does this at
  // IccEncoding.cpp:682 on every return path out of the converter.
  delete pParams;

  if (newIcc)
    delete newIcc;

  if (!rc)
    std::fprintf(stderr, "[encoding-lumamtx] ok: %s\n", szName);

  return rc;
}

} // namespace

int main()
{
  int rc = 0;

  // Full parameters: the conversion succeeds, and the struct must come out of it
  // still owning its luma/chroma matrix.
  rc |= runCase("borrowed luma matrix survives a successful conversion",
                true, true);

  // The same struct minus the primaries. The converter gives up after the matrix
  // has been read and returns without a profile; the borrowed element must be
  // just as intact, since the premature free was never conditional on the outcome.
  rc |= runCase("borrowed luma matrix survives a failed conversion",
                false, false);

  if (rc)
    return rc;

  std::fprintf(stderr,
               "[encoding-lumamtx] all borrowed-element cases passed\n");
  return 0;
}
