// Regression for the second half of #1817 and for #1980: the colour-space
// white-point luminance in CIccDefaultEncProfileConverter::ConvertFromParams
// must be a physically meaningful value before anything is derived from it.
//
// The converter reads the luminance straight out of the encoding parameters:
//
//     icFloatNumber Lw  = pParams->GetElemNumberValue(icSigCeptWhitePointLuminanceMbr, 100);
//     ...
//     icFloatNumber SWr = Lsw / Lw;
//
// Lw is profile-supplied, so a crafted profile controls it. #1817 reported the
// division above: a zero Lw with a positive viewing surround yielded +infinity,
// and "+inf > 0.2" selected *Average* surround -- the least conservative
// setting, chosen off a white point carrying no luminance at all.
//
// Guarding the division alone left the rest of the block reading from the same
// value. Lw also scales the illuminant and surround XYZ written into the
// viewing-conditions tag, defaults the ambient luminance La and the viewing
// surround Lsw, and becomes the CAM Yb parameter. A negative Lw therefore
// propagated a negative luminance into all of that state; the CAM adapting-
// luminance guard added for #1950 contains only the last of those consequences.
// #1980 rejects the parameters outright instead: ICC.2:2023 12.2.3.2.6 defines
// 'wlum' as a luminance in cd/m2 and admits no zero or negative exception.
//
// NOTE: this changes the #1817 contract deliberately. A zero white-point
// luminance used to convert with status icEncConvertOk (having resolved the
// guarded ratio to Dark surround); it is now rejected as bad parameters. The
// zero case is kept below precisely so that change stays visible and asserted
// rather than being dropped along with the defect.
//
// Detection no longer depends on the build. The pre-#1817 defect was only
// observable under -fsanitize=float-divide-by-zero, because an infinite ratio
// merely mis-selected a surround category that is not reachable from the
// returned profile. Rejection is observable from the return status in every
// configuration, so these cases are deterministic contract checks and a plain
// build now carries the same signal as an ASAN+UBSAN one.
//
// The test drives the public IIccEncProfileConverter handler with the smallest
// parameter structure that reaches the surround block. Two things are required
// to get there: the primaries must form an invertible matrix, and the white
// point must not be D50, because the block is inside a "header illuminant is not
// D50" branch. D65 primaries satisfy both.
//
// Returns 0 on success; non-zero on failure.

#include "IccEncoding.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <cstring>
#include <limits>

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

// Build the minimal colorEncodingParams struct that reaches the viewing-
// conditions block. pWhiteLum == NULL omits the white-point luminance member
// entirely, which is the common case: the converter then defaults it to 100.
CIccTagStruct *buildParams(const icFloatNumber *pWhiteLum)
{
  CIccTagStruct *pParams = (CIccTagStruct *)CIccTag::Create(icSigTagStructType);
  if (!pParams)
    return NULL;

  if (!pParams->SetTagStructType(icSigColorEncodingParamsSruct)) {
    delete pParams;
    return NULL;
  }

  // D65 white point: not D50, so the viewing-conditions block that carries the
  // surround ratio is reached.
  const icFloatNumber white[2] = {0.3127f, 0.3290f};
  const icFloatNumber red[2]   = {0.6400f, 0.3300f};
  const icFloatNumber green[2] = {0.3000f, 0.6000f};
  const icFloatNumber blue[2]  = {0.1500f, 0.0600f};

  // A positive viewing surround, so a zero Lw makes the ratio +inf rather than
  // NaN. That is the #1817 combination that selected Average surround.
  const icFloatNumber surround[1] = {1.0f};

  bool ok =
    attachFloats(pParams, icSigCeptWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptMediumWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptRedPrimaryXYZMbr, red, 2) &&
    attachFloats(pParams, icSigCeptGreenPrimaryXYZMbr, green, 2) &&
    attachFloats(pParams, icSigCeptBluePrimaryXYZMbr, blue, 2) &&
    attachFloats(pParams, icSigCeptViewingSurroundMbr, surround, 1);

  if (ok && pWhiteLum)
    ok = attachFloats(pParams, icSigCeptWhitePointLuminanceMbr, pWhiteLum, 1);

  if (!ok) {
    delete pParams;
    return NULL;
  }

  return pParams;
}

// Convert once with the given white-point luminance and check the outcome.
// pWhiteLum == NULL omits the member. Returns 0 on success.
int runCase(const char *szName, const icFloatNumber *pWhiteLum,
            icStatusEncConvert expectStat)
{
  CIccTagStruct *pParams = buildParams(pWhiteLum);
  if (!pParams) {
    std::fprintf(stderr, "[encoding-surround] %s: could not build params struct\n",
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
    std::fprintf(stderr, "[encoding-surround] %s: no encoding converter handler\n",
                 szName);
    delete pParams;
    return 1;
  }

  CIccProfilePtr newIcc = NULL;
  icStatusEncConvert stat = pConverter->ConvertFromParams(newIcc, pParams, &hdr);

  delete pParams;

  int rc = 0;

  if (stat != expectStat) {
    std::fprintf(stderr,
                 "[encoding-surround] FAIL: %s: expected status %d, got %d\n",
                 szName, (int)expectStat, (int)stat);
    rc = 2;
  }

  // A rejected conversion must leave no profile behind, and an accepted one must
  // produce one. The rejection path releases the profile it had already built,
  // so a leak here is what a sanitizer build reports.
  const bool bWantProfile = (expectStat == icEncConvertOk);
  if (!rc && ((newIcc != NULL) != bWantProfile)) {
    std::fprintf(stderr,
                 "[encoding-surround] FAIL: %s: %s\n", szName,
                 bWantProfile ? "converter reported success but produced no profile"
                              : "rejected parameters produced a profile");
    rc = 3;
  }

  if (newIcc)
    delete newIcc;

  if (!rc)
    std::fprintf(stdout, "[encoding-surround] ok: %s\n", szName);

  return rc;
}

} // namespace

int main()
{
  const icFloatNumber zero     = 0.0f;
  const icFloatNumber negative = -1.0f;
  const icFloatNumber nan      = std::numeric_limits<icFloatNumber>::quiet_NaN();
  const icFloatNumber inf      = std::numeric_limits<icFloatNumber>::infinity();
  const icFloatNumber valid    = 100.0f;

  int rc = 0;

  // The #1817 input. Previously converted with icEncConvertOk; rejected as of
  // #1980. See the contract note at the top of this file.
  rc |= runCase("zero white-point luminance rejected", &zero,
                icEncConvertBadParams);

  // The #1980 input: a negative luminance reached the illuminant, surround, CAM
  // Yb and default La state before any guard downstream could contain it.
  rc |= runCase("negative white-point luminance rejected", &negative,
                icEncConvertBadParams);

  rc |= runCase("NaN white-point luminance rejected", &nan,
                icEncConvertBadParams);

  rc |= runCase("infinite white-point luminance rejected", &inf,
                icEncConvertBadParams);

  // Controls: the guard must not narrow the accepted range. A plain positive
  // luminance converts, and so does a struct that omits the member altogether --
  // the overwhelmingly common case, which defaults to 100 cd/m2.
  rc |= runCase("valid white-point luminance accepted", &valid,
                icEncConvertOk);

  rc |= runCase("absent white-point luminance accepted", NULL,
                icEncConvertOk);

  if (rc)
    return rc;

  std::fprintf(stdout, "[encoding-surround] all white-point luminance cases passed\n");
  return 0;
}
