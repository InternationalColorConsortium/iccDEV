// Regression for #1559 (follow-up to #626): a devicePccTag ('dpcc') on an
// abstract iccMAX profile must replace the profile-level connection conditions
// with the matching members of its profileConnectionConditionsStructure ('pcc ')
// when the CMM reads the profile through the IIccProfileConnectionConditions
// interface (spec 9.2.x+1 / 12.2.y).
//
// The CMM always reaches a profile's PCC via getPccViewingConditions /
// getCustomToStandardPcc / getStandardToCustomPcc / getMediaWhiteXYZ (and the
// illuminant/observer getters, which all derive from getPccViewingConditions).
// This test pins that, for an abstract profile carrying a dpcc tag, those
// getters return the structure members rather than the profile's own tags --
// and that the replacement is gated to abstract iccMAX profiles, so a non-abstract
// profile still falls back to its profile-level tags. Each "dpcc wins" assertion
// is red-green: it fails if getDevicePccElem is bypassed.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"
#include "IccTagMPE.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cstdio>
#include <cmath>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[dpcc-pcc-replacement] FAIL: %s\n", what);
  }
}

CIccTagXYZ *makeXYZ(icFloatNumber X, icFloatNumber Y, icFloatNumber Z)
{
  CIccTagXYZ *p = new CIccTagXYZ();
  p->SetSize(1);
  icXYZNumber *pv = p->GetXYZ(0);
  pv->X = icDtoF(X);
  pv->Y = icDtoF(Y);
  pv->Z = icDtoF(Z);
  return p;
}

bool approx(icFloatNumber a, icFloatNumber b) { return std::fabs(a - b) < 1.0e-4f; }

} // namespace

int main()
{
  // Profile-level media white point (the value that must be SHADOWED by dpcc).
  const icFloatNumber profWhite[3] = { 0.9000f, 1.0000f, 0.8000f };
  // dpcc mwpt member (the replacement value that must WIN for an abstract profile).
  const icFloatNumber pccWhite[3]  = { 0.5000f, 0.6000f, 0.7000f };

  CIccProfile profile;
  profile.m_Header.version     = icVersionNumberV5;
  profile.m_Header.deviceClass = icSigAbstractClass;

  profile.AttachTag(icSigMediaWhitePointTag, makeXYZ(profWhite[0], profWhite[1], profWhite[2]));

  // Build the devicePccTag: a 'pcc ' struct with mwpt/svcn/c2sp/s2cp members.
  CIccTagStruct *pDpcc = new CIccTagStruct();
  pDpcc->SetTagStructType(icSigProfileConnectionConditionsStruct);

  CIccTagXYZ *pPccWhite = makeXYZ(pccWhite[0], pccWhite[1], pccWhite[2]);
  CIccTagSpectralViewingConditions *pSvcn = new CIccTagSpectralViewingConditions();
  CIccTagMultiProcessElement *pC2s = new CIccTagMultiProcessElement();
  CIccTagMultiProcessElement *pS2c = new CIccTagMultiProcessElement();

  pDpcc->AttachElem(icSigPccMediaWhitePointMbr, pPccWhite);
  pDpcc->AttachElem(icSigPccSpectralViewingConditionsMbr, pSvcn);
  pDpcc->AttachElem(icSigPccCustomToStandardPccMbr, pC2s);
  pDpcc->AttachElem(icSigPccStandardToCustomPccMbr, pS2c);

  profile.AttachTag(icSigDevicePccTag, pDpcc);

  // --- Abstract profile: every getter resolves to the dpcc member. ---
  icFloatNumber w[3] = { 0 };
  profile.getMediaWhiteXYZ(w);
  check(approx(w[0], pccWhite[0]) && approx(w[1], pccWhite[1]) && approx(w[2], pccWhite[2]),
        "abstract: getMediaWhiteXYZ returns dpcc mwpt member, not profile tag");

  check(profile.getPccViewingConditions() == pSvcn,
        "abstract: getPccViewingConditions returns dpcc svcn member");
  check(profile.getCustomToStandardPcc() == pC2s,
        "abstract: getCustomToStandardPcc returns dpcc c2sp member");
  check(profile.getStandardToCustomPcc() == pS2c,
        "abstract: getStandardToCustomPcc returns dpcc s2cp member");

  // --- Gate: a non-abstract profile ignores dpcc and falls back to its tags. ---
  // No profile-level svcn/c2sp/s2cp tags were attached, so the fallback path
  // returns NULL there and the profile mediaWhitePointTag for the white point.
  profile.m_Header.deviceClass = icSigOutputClass;

  profile.getMediaWhiteXYZ(w);
  check(approx(w[0], profWhite[0]) && approx(w[1], profWhite[1]) && approx(w[2], profWhite[2]),
        "non-abstract: getMediaWhiteXYZ falls back to profile mediaWhitePointTag");
  check(profile.getPccViewingConditions() == NULL,
        "non-abstract: getPccViewingConditions ignores dpcc svcn member");
  check(profile.getCustomToStandardPcc() == NULL,
        "non-abstract: getCustomToStandardPcc ignores dpcc c2sp member");

  // --- Gate: a v2/v4 abstract profile also ignores dpcc (amendment is v5-only). ---
  profile.m_Header.deviceClass = icSigAbstractClass;
  profile.m_Header.version     = icVersionNumberV4_3;
  check(profile.getCustomToStandardPcc() == NULL,
        "v4 abstract: getCustomToStandardPcc ignores dpcc (v5-only amendment)");

  if (g_fail)
    std::fprintf(stderr, "[dpcc-pcc-replacement] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stderr, "[dpcc-pcc-replacement] all assertions passed\n");

  return g_fail;
}
