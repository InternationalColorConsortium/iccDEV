// Regression for the second half of #1817: the surround ratio in
// CIccDefaultEncProfileConverter::ConvertFromParams must not divide by a
// profile-supplied zero.
//
// The converter reads the white-point luminance straight out of the encoding
// parameters and then formed the surround ratio unguarded:
//
//     icFloatNumber Lw  = pParams->GetElemNumberValue(icSigCeptWhitePointLuminanceMbr, 100);
//     ...
//     icFloatNumber SWr = Lsw / Lw;
//
// Lw is attacker-controlled, so a profile carrying a zero white-point luminance
// made this a division by zero. The same AFL corpus that produced the
// IccCAM.cpp:150 report reaches this path through iccApplyProfiles.
//
// SWr only picks one of three surround categories, so the guard resolves a
// degenerate ratio to 0.0f, which selects Dark surround. Exactly one case
// changes: Lsw > 0 with Lw == 0 used to yield +infinity, and "+inf > 0.2"
// selected *Average* surround -- the least conservative setting, chosen off a
// white point carrying no luminance at all. That is the case exercised below.
//
// The test drives the public IIccEncProfileConverter handler with the smallest
// parameter structure that reaches the surround block. Two things are required
// to get there: the primaries must form an invertible matrix, and the white
// point must not be D50, because the block is inside a "header illuminant is not
// D50" branch. D65 primaries satisfy both.
//
// Detection depends on the build, and it is worth being precise about that. The
// unfixed converter still returns icEncConvertOk here -- the infinite ratio only
// mis-selects the surround, which is not observable from the returned profile
// without reaching into the CAM converter it builds internally. So this test
// catches the regression through the sanitizer, not through its return value:
// under -fsanitize=float-divide-by-zero it aborts with
//
//     IccProfLib/IccEncoding.cpp:435:29: runtime error: division by zero
//
// That is the configuration ENABLE_SANITIZERS produces, and the one the CI
// ASAN+UBSAN jobs build, so the coverage is real -- but on a plain build this
// test only confirms the path still converts cleanly. Note that plain
// -fsanitize=undefined is *not* enough on either gcc or clang;
// float-divide-by-zero has to be requested explicitly.
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

// Attach a Float32 member holding the supplied values. Ownership passes to the
// struct on success.
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

} // namespace

int main()
{
  CIccTagStruct *pParams = (CIccTagStruct *)CIccTag::Create(icSigTagStructType);
  if (!pParams) {
    std::fprintf(stderr, "[encoding-surround] could not create params struct\n");
    return 1;
  }

  if (!pParams->SetTagStructType(icSigColorEncodingParamsSruct)) {
    std::fprintf(stderr, "[encoding-surround] could not set struct type\n");
    delete pParams;
    return 1;
  }

  // D65 white point: not D50, so the viewing-conditions block that carries the
  // surround ratio is reached.
  const icFloatNumber white[2] = {0.3127f, 0.3290f};
  const icFloatNumber red[2]   = {0.6400f, 0.3300f};
  const icFloatNumber green[2] = {0.3000f, 0.6000f};
  const icFloatNumber blue[2]  = {0.1500f, 0.0600f};

  // The #1817 input: a zero white-point luminance, with a positive viewing
  // surround so the ratio is +inf rather than NaN. That is the combination that
  // previously selected Average surround.
  const icFloatNumber whiteLum[1] = {0.0f};
  const icFloatNumber surround[1] = {1.0f};

  bool ok =
    attachFloats(pParams, icSigCeptWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptMediumWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptRedPrimaryXYZMbr, red, 2) &&
    attachFloats(pParams, icSigCeptGreenPrimaryXYZMbr, green, 2) &&
    attachFloats(pParams, icSigCeptBluePrimaryXYZMbr, blue, 2) &&
    attachFloats(pParams, icSigCeptWhitePointLuminanceMbr, whiteLum, 1) &&
    attachFloats(pParams, icSigCeptViewingSurroundMbr, surround, 1);

  if (!ok) {
    std::fprintf(stderr, "[encoding-surround] could not build params struct\n");
    delete pParams;
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
    std::fprintf(stderr, "[encoding-surround] no encoding converter handler\n");
    delete pParams;
    return 1;
  }

  // Pre-fix, this call divided 1.0f by 0.0f while choosing the surround. Under a
  // float-divide-by-zero build it aborts here rather than returning.
  CIccProfilePtr newIcc = NULL;
  icStatusEncConvert stat = pConverter->ConvertFromParams(newIcc, pParams, &hdr);

  delete pParams;

  if (stat != icEncConvertOk) {
    std::fprintf(stderr,
                 "[encoding-surround] FAIL: zero white-point luminance did not "
                 "convert cleanly (status %d)\n", (int)stat);
    if (newIcc)
      delete newIcc;
    return 2;
  }

  if (!newIcc) {
    std::fprintf(stderr, "[encoding-surround] FAIL: converter reported success "
                         "but produced no profile\n");
    return 3;
  }

  delete newIcc;

  std::fprintf(stdout, "[encoding-surround] zero white-point luminance handled\n");
  return 0;
}
