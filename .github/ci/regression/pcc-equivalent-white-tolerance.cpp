/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the ICC
 *    organization endorses or promotes products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
    File:       pcc-equivalent-white-tolerance.cpp

    Contains:   CTest helper for IIccProfileConnectionConditions::isEquivalentPcc()
                white point comparison.

    isEquivalentPcc() is the gate in front of every chromatic adaptation the CMM
    splices: CIccPcsXform::pushXYZConvert() returns immediately when the two
    sides of a connection report equivalent connection conditions, and otherwise
    goes on to build a conversion between them.

    Its last test compared the two normalized illuminant white points with ==.
    Both are computed as X/Y, 1, Z/Y from the float32 m_illuminantXYZ of each
    side's spectralViewingConditions tag, so two profiles naming the same
    illuminant hand back different floats whenever they wrote it at a different
    luminance scale or rounded it differently -- 0 for a pure scale change,
    3.5e-6 for an s15Fixed16 round trip, 2.0e-5 for the four-decimal D55 that
    profiles commonly carry against the full-precision one. Every one of those
    was read as two different viewing conditions.

    What that costs is not a rounding difference, it is a whole adaptation.
    Issue #1860 is the same false negative arrived at from the other direction --
    there one end of the chain lost its 'svcn' tag rather than rounding it
    differently -- and it measured 0.14 to 0.57 per channel of error, out of a
    profile-to-itself round trip that should have been the identity.

    isEquivalentPcc() now compares within icPccWhiteNearRange (1e-4). The band is
    deliberately wider than CIccPcsLabStep::isSameWhite()'s 1e-5: that predicate
    decides whether an exact Lab<->XYZ round trip folds away, this one decides
    whether an adaptation is spliced, and the two errors are not the same size.
    1e-4 is also what CIccTagSpectralViewingConditions::setIlluminant() already
    allows when it classifies a white point as D50 or D65.

    Getting there takes three things at once, and they narrow the branch more
    than the source suggests. Both sides must declare a CUSTOM observer. Neither
    may carry an illuminant SPD -- and getIlluminant() synthesizes one from a
    built-in table for D50, D65, D93 and A, so hasIlluminantSPD() answers true
    for those four however the tag was written, and only illuminants outside
    that table (D55, the F series, equi-power E, blackbody, daylight) can reach
    the comparison at all. And the illuminant may not be icIlluminantUnknown,
    which is refused earlier. The fixtures here name D55 for exactly that
    reason; a D65 fixture never reaches the line under test.

    Two further observations about the branch, neither of them changed here: it
    compares the white points but not the observer functions, so two genuinely
    different custom observers over the same illuminant are already reported
    equivalent; and isStandardPcc() is
    (observer == 1931 2-degree || illuminant == D50), so a custom-observer
    profile naming D50 takes getNormIlluminantXYZ()'s icD50XYZ branch on both
    sides and compares equal whatever its tag says.

    Returns 0 on success; the number of failed assertions otherwise.
*/

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccPcc.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void check(bool cond, const char *msg)
{
  if (cond) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

// --- fixtures -------------------------------------------------------------

// Full-precision D55 and the four-decimal D55 that profiles commonly carry.
// Normalized they are 2.0e-5 apart in X, which is inside the new band and was
// outside exact equality -- the pair the change exists for.
static const double kD55Full[3] = { 0.95682, 1.0, 0.92149 };
static const double kD55Round[3] = { 0.9568, 1.0, 0.9215 };

// The same D55 at a photometric luminance scale. The division lands on the same
// float here, which is worth having in the set: a chain that only ever hit this
// case would have looked correct under exact equality too.
static const double kD55Scaled[3] = { 0.95682 * 683.0, 683.0, 0.92149 * 683.0 };

// The same D55 after an s15Fixed16 round trip, 3.5e-6 once normalized.
static double s15Fixed16(double v)
{
  return (double)icFtoD(icDtoF((icFloatNumber)v));
}

// A genuinely different white point: D50, 9.7e-2 from D55 in Z, three orders of
// magnitude outside the band.
static const double kD50[3] = { 0.9642, 1.0, 0.8249 };

// A four-step custom observer. The data is not colorimetrically meaningful and
// does not need to be: nothing under test reads the observer functions, only
// getStdObserver() == icStdObsCustom. Both sides get the same one so the pair
// differs in exactly one thing, the white point.
static const icFloatNumber kObserver[12] = {
  0.10f, 0.20f, 0.70f,
  0.30f, 0.60f, 0.10f,
  0.50f, 0.40f, 0.05f,
  0.20f, 0.10f, 0.01f
};

static icSpectralRange makeRange(icFloatNumber start, icFloatNumber end,
                                 icUInt16Number steps)
{
  icSpectralRange r;
  r.start = icFtoF16(start);
  r.end = icFtoF16(end);
  r.steps = steps;
  return r;
}

// D55 named by id with NO illuminant SPD, a custom observer, and the given
// illuminant XYZ.
//
// setIlluminant() with a null SPD is what keeps hasIlluminantSPD() false; with
// one, isEquivalentPcc() refuses the pair on the "else return false" arm before
// it ever looks at a white point, and the test would prove nothing.
static CIccTagSpectralViewingConditions *makeViewingConditions(const double *pXYZ)
{
  CIccTagSpectralViewingConditions *pSvc = new CIccTagSpectralViewingConditions();

  icSpectralRange zeroRange;
  std::memset(&zeroRange, 0, sizeof(zeroRange));
  //D55 rather than D50/D65/D93/A: getIlluminant() would hand back a built-in
  //SPD for any of those four, hasIlluminantSPD() would answer true, and
  //isEquivalentPcc() would refuse the pair before comparing white points.
  pSvc->setIlluminant(icIlluminantD55, zeroRange, NULL, 5503.0f);

  pSvc->setObserver(icStdObsCustom, makeRange(400.0f, 700.0f, 4), kObserver);

  pSvc->m_illuminantXYZ.X = (icFloat32Number)pXYZ[0];
  pSvc->m_illuminantXYZ.Y = (icFloat32Number)pXYZ[1];
  pSvc->m_illuminantXYZ.Z = (icFloat32Number)pXYZ[2];

  return pSvc;
}

static void attachDescriptions(CIccProfile &p, const char *szText)
{
  CIccTagMultiLocalizedUnicode *pDesc = new CIccTagMultiLocalizedUnicode();
  pDesc->SetText(szText);
  p.AttachTag(icSigProfileDescriptionTag, pDesc);

  CIccTagMultiLocalizedUnicode *pCprt = new CIccTagMultiLocalizedUnicode();
  pCprt->SetText("Copyright (C) 2026 The International Color Consortium");
  p.AttachTag(icSigCopyrightTag, pCprt);
}

// A 3-in/3-out identity-curve LUT, enough for CIccCmm::AddXform to have a
// transform to run. Curves are filled before SetColorSpaces() for the reason
// pcs-xyzconvert-matrix-channels.cpp documents: for a Lab/XYZ input it
// relocates the B curves and installs its own.
static CIccTagLut16 *makeIdentityLut(icColorSpaceSignature src,
                                     icColorSpaceSignature dst)
{
  CIccTagLut16 *pLut = new CIccTagLut16;
  pLut->Init(3, 3);

  LPIccCurve *pA = pLut->NewCurvesA();
  LPIccCurve *pB = pLut->NewCurvesB();
  if (!pA || !pB) {
    delete pLut;
    return NULL;
  }
  for (int i = 0; i < 3; i++) {
    CIccTagCurve *pCa = new CIccTagCurve();
    pCa->SetSize(2, icInitIdentity);
    pA[i] = pCa;

    CIccTagCurve *pCb = new CIccTagCurve();
    pCb->SetSize(2, icInitIdentity);
    pB[i] = pCb;
  }

  pLut->SetColorSpaces(src, dst);

  icUInt8Number grid[3] = { 2, 2, 2 };
  if (!pLut->NewCLUT(grid, 2)) {
    delete pLut;
    return NULL;
  }

  CIccCLUT *pClut = pLut->GetCLUT();
  icFloatNumber *pData = pClut->GetData(0);
  const icUInt32Number n = pClut->NumPoints() * 3;
  for (icUInt32Number i = 0; i < n; i++)
    pData[i] = 0.5f;

  return pLut;
}

// An RGB device profile with a Lab PCS, carrying custom-observer viewing
// conditions built over pXYZ. Both LUT directions are attached so the same
// builder serves either end of a chain.
//
// Deliberately WITHOUT customToStandardPcc / standardToCustomPcc tags. That is
// what makes the chain case below a clean oracle: if isEquivalentPcc() reports
// a mismatch, pushXYZConvert() needs one of those MPEs to build the adaptation
// from, finds neither, and fails the chain with icCmmStatBadSpaceLink.
static void buildProfile(CIccProfile &p, const double *pXYZ, const char *szText)
{
  p.InitHeader();
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace = icSigRgbData;
  p.m_Header.pcs = icSigLabData;
  p.m_Header.version = icVersionNumberV4_3;

  CIccTagXYZ *pWtpt = new CIccTagXYZ(1);
  (*pWtpt)[0].X = icDtoF((icFloatNumber)0.9642);
  (*pWtpt)[0].Y = icDtoF((icFloatNumber)1.0000);
  (*pWtpt)[0].Z = icDtoF((icFloatNumber)0.8249);
  p.AttachTag(icSigMediaWhitePointTag, pWtpt);

  p.AttachTag(icSigAToB1Tag, makeIdentityLut(icSigRgbData, icSigLabData));
  p.AttachTag(icSigBToA1Tag, makeIdentityLut(icSigLabData, icSigRgbData));
  p.AttachTag(icSigSpectralViewingConditionsTag, makeViewingConditions(pXYZ));

  attachDescriptions(p, szText);
}

static double maxNormalizedGap(CIccProfile &a, CIccProfile &b)
{
  icFloatNumber XYZ1[3], XYZ2[3];
  a.getNormIlluminantXYZ(&XYZ1[0]);
  b.getNormIlluminantXYZ(&XYZ2[0]);

  double worst = 0.0;
  for (int i = 0; i < 3; i++) {
    const double diff = std::fabs((double)XYZ1[i] - (double)XYZ2[i]);
    if (diff > worst)
      worst = diff;
  }
  return worst;
}

// --- cases ----------------------------------------------------------------

// The oracle for every accept below. Each pair really does reach the white
// point comparison with unequal floats, and the gap really is inside the band.
// Without this the accepts would pass just as well against == on two
// bit-identical vectors.
static void theFixturesReachTheComparisonWithUnequalWhites()
{
  struct Row { const double *pXYZ; const char *szName; double atLeast; };

  double roundTrip[3];
  for (int i = 0; i < 3; i++)
    roundTrip[i] = s15Fixed16(kD55Full[i]);

  const Row kRows[3] = {
    { kD55Scaled, "the same D55 at a photometric luminance scale", 0.0 },
    { roundTrip,  "the same D55 through an s15Fixed16 round trip", 1.0e-6 },
    { kD55Round,  "four-decimal D55 against the full-precision one", 1.0e-5 },
  };

  for (int r = 0; r < 3; r++) {
    char szMsg[192];
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, kRows[r].pXYZ, "pcc tolerance variant");

    const double gap = maxNormalizedGap(a, b);
    std::printf("info: %-48s normalized gap %.3e\n", kRows[r].szName, gap);

    // Both sides must be non-standard, or getNormIlluminantXYZ() short-circuits
    // to the icD50XYZ literal for both and the gap is trivially zero.
    std::snprintf(szMsg, sizeof(szMsg),
                  "pcc oracle: both sides report non-standard PCC (%s)", kRows[r].szName);
    check(!a.isStandardPcc() && !b.isStandardPcc(), szMsg);

    std::snprintf(szMsg, sizeof(szMsg),
                  "pcc oracle: neither side carries an illuminant SPD (%s)", kRows[r].szName);
    check(!a.hasIlluminantSPD() && !b.hasIlluminantSPD(), szMsg);

    std::snprintf(szMsg, sizeof(szMsg),
                  "pcc oracle: the gap is inside icPccWhiteNearRange (%s)", kRows[r].szName);
    check(gap < 1.0e-4, szMsg);

    if (kRows[r].atLeast > 0.0) {
      std::snprintf(szMsg, sizeof(szMsg),
                    "pcc oracle: the gap is real, not float noise (%s)", kRows[r].szName);
      check(gap > kRows[r].atLeast, szMsg);
    }
  }
}

// The accepts. Three ways of writing the same D55, each of which used to be
// read as a different set of viewing conditions.
static void equivalentPccAcceptsAnEncodingDifference()
{
  double roundTrip[3];
  for (int i = 0; i < 3; i++)
    roundTrip[i] = s15Fixed16(kD55Full[i]);

  struct Row { const double *pXYZ; const char *szName; };
  const Row kRows[4] = {
    { kD55Full,   "the identical white point" },
    { kD55Scaled, "the same D55 at a photometric luminance scale" },
    { roundTrip,  "the same D55 through an s15Fixed16 round trip" },
    { kD55Round,  "four-decimal D55 against the full-precision one" },
  };

  for (int r = 0; r < 4; r++) {
    char szMsg[192];
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, kRows[r].pXYZ, "pcc tolerance variant");

    std::snprintf(szMsg, sizeof(szMsg), "pcc accept: %s is equivalent", kRows[r].szName);
    check(a.isEquivalentPcc(b), szMsg);

    // Equivalence is a symmetric claim; the predicate reads one side's tag and
    // the other's through a different object, so assert it both ways round.
    std::snprintf(szMsg, sizeof(szMsg), "pcc accept: %s is equivalent in reverse",
                  kRows[r].szName);
    check(b.isEquivalentPcc(a), szMsg);
  }
}

// The band has to have an outside, or the predicate would be answering "yes"
// to everything that got this far.
static void equivalentPccRefusesADifferentWhite()
{
  {
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, kD50, "pcc tolerance D50 variant");
    std::printf("info: %-48s normalized gap %.3e\n",
                "D55 against D50", maxNormalizedGap(a, b));
    check(!a.isEquivalentPcc(b),
          "pcc refuse: a D55/D50 white point difference is not equivalent");
    check(!b.isEquivalentPcc(a),
          "pcc refuse: a D55/D50 white point difference is not equivalent in reverse");
  }

  // Just outside the band, which pins where the edge is rather than that an
  // edge exists somewhere. 2e-4 in X is twice the band and still far below any
  // real illuminant difference.
  {
    const double justOutside[3] = { kD55Full[0] + 2.0e-4, kD55Full[1], kD55Full[2] };
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, justOutside, "pcc tolerance just-outside variant");
    std::printf("info: %-48s normalized gap %.3e\n",
                "2e-4 perturbation in X", maxNormalizedGap(a, b));
    check(!a.isEquivalentPcc(b),
          "pcc refuse: 2e-4 in X is outside the band and is not equivalent");
  }
}

// Everything else isEquivalentPcc() decides on, pinned so that widening the
// white point test cannot be mistaken for widening the predicate. Each row
// differs from the reference in exactly one declaration and must still be
// refused -- with an identical white point, so the white point comparison
// cannot be what refuses it.
static void equivalentPccStillRefusesEverythingElse()
{
  // A different standard illuminant.
  {
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, kD55Full, "pcc tolerance illuminant variant");
    CIccTagSpectralViewingConditions *pSvc =
      (CIccTagSpectralViewingConditions*)b.FindTag(icSigSpectralViewingConditionsTag);
    check(pSvc != NULL, "pcc other: variant carries viewing conditions");
    if (pSvc) {
      icSpectralRange zeroRange;
      std::memset(&zeroRange, 0, sizeof(zeroRange));
      pSvc->setIlluminant(icIlluminantA, zeroRange, NULL, 2856.0f);
      check(!a.isEquivalentPcc(b),
            "pcc other: a different standard illuminant is not equivalent");
    }
  }

  // A different observer. 1931 2-degree also makes isStandardPcc() true on that
  // side, which is a second reason to refuse; the point is only that the
  // predicate still does.
  {
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, kD55Full, "pcc tolerance observer variant");
    CIccTagSpectralViewingConditions *pSvc =
      (CIccTagSpectralViewingConditions*)b.FindTag(icSigSpectralViewingConditionsTag);
    if (pSvc) {
      pSvc->setObserver(icStdObs1931TwoDegrees, makeRange(400.0f, 700.0f, 4), kObserver);
      check(!a.isEquivalentPcc(b),
            "pcc other: a different observer is not equivalent");
    }
  }

  // Daylight at two colour temperatures. The CCT comparison is still exact, and
  // should be: a daylight illuminant is defined by its CCT, so 6500 and 6504
  // name two different illuminants rather than one rounded differently.
  {
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance daylight 5503");
    buildProfile(b, kD55Full, "pcc tolerance daylight 5500");
    CIccTagSpectralViewingConditions *pA =
      (CIccTagSpectralViewingConditions*)a.FindTag(icSigSpectralViewingConditionsTag);
    CIccTagSpectralViewingConditions *pB =
      (CIccTagSpectralViewingConditions*)b.FindTag(icSigSpectralViewingConditionsTag);
    if (pA && pB) {
      icSpectralRange zeroRange;
      std::memset(&zeroRange, 0, sizeof(zeroRange));
      pA->setIlluminant(icIlluminantDaylight, zeroRange, NULL, 6504.0f);
      pB->setIlluminant(icIlluminantDaylight, zeroRange, NULL, 6500.0f);
      check(!a.isEquivalentPcc(b),
            "pcc other: daylight at two colour temperatures is not equivalent");

      // Control: the same CCT on both sides, so the row above is refused by the
      // CCT and not by something the fixture did to the tag.
      pB->setIlluminant(icIlluminantDaylight, zeroRange, NULL, 6504.0f);
      check(a.isEquivalentPcc(b),
            "pcc other: daylight at the same colour temperature is equivalent");
    }
  }

  // An unknown illuminant is refused even against itself.
  {
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance unknown A");
    buildProfile(b, kD55Full, "pcc tolerance unknown B");
    for (CIccProfile *p : { &a, &b }) {
      CIccTagSpectralViewingConditions *pSvc =
        (CIccTagSpectralViewingConditions*)p->FindTag(icSigSpectralViewingConditionsTag);
      if (pSvc) {
        icSpectralRange zeroRange;
        std::memset(&zeroRange, 0, sizeof(zeroRange));
        pSvc->setIlluminant(icIlluminantUnknown, zeroRange, NULL);
      }
    }
    check(!a.isEquivalentPcc(b),
          "pcc other: an unknown illuminant is not equivalent to itself");
  }

  // An illuminant SPD on either side is refused outright, white points or not.
  {
    CIccProfile a, b;
    buildProfile(a, kD55Full, "pcc tolerance reference");
    buildProfile(b, kD55Full, "pcc tolerance SPD variant");
    CIccTagSpectralViewingConditions *pSvc =
      (CIccTagSpectralViewingConditions*)b.FindTag(icSigSpectralViewingConditionsTag);
    if (pSvc) {
      const icFloatNumber spd[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
      pSvc->setIlluminant(icIlluminantD55, makeRange(400.0f, 700.0f, 4), spd, 5503.0f);
      check(b.hasIlluminantSPD(), "pcc other: the SPD variant reports an SPD");
      check(!a.isEquivalentPcc(b),
            "pcc other: an illuminant SPD on one side is not equivalent");
    }
  }
}

// What the predicate is for, at the level a chain uses it.
//
// Neither profile carries a customToStandardPcc or standardToCustomPcc MPE, so
// the two outcomes are cleanly separated in a plain build with no sanitizer and
// no pixel comparison: if the two PCCs are equivalent, pushXYZConvert() returns
// before it needs one and the chain begins; if they are not, it needs one, finds
// neither, and the chain fails with icCmmStatBadSpaceLink.
static icStatusCMM beginChain(const double *pSrcXYZ, const double *pDstXYZ)
{
  CIccProfile *pSrc = new CIccProfile();
  CIccProfile *pDst = new CIccProfile();
  buildProfile(*pSrc, pSrcXYZ, "pcc tolerance chain source");
  buildProfile(*pDst, pDstXYZ, "pcc tolerance chain destination");

  CIccCmm cmm(icSigRgbData, icSigRgbData, true);
  if (cmm.AddXform(pSrc, icRelativeColorimetric) != icCmmStatOk) {
    delete pSrc;
    delete pDst;
    return icCmmStatBadXform;
  }
  if (cmm.AddXform(pDst, icRelativeColorimetric) != icCmmStatOk) {
    delete pDst;
    return icCmmStatBadXform;
  }

  return cmm.Begin();
}

static void aChainAcrossAnEncodingDifferenceNeedsNoAdaptation()
{
  const icStatusCMM rvSame = beginChain(kD55Full, kD55Round);
  std::printf("info: chain across a 2.0e-5 white point difference: Begin() = %d\n",
              (int)rvSame);
  check(rvSame == icCmmStatOk,
        "pcc chain: four-decimal against full-precision D55 needs no adaptation");

  // Control. A real illuminant difference must still send the connection on to
  // build an adaptation -- which this fixture cannot supply, so the chain fails.
  // Without this row the case above would pass on a predicate that says yes to
  // everything.
  const icStatusCMM rvDiff = beginChain(kD55Full, kD50);
  std::printf("info: chain across a D55/D50 difference: Begin() = %d\n", (int)rvDiff);
  check(rvDiff != icCmmStatOk,
        "pcc chain: a D55/D50 difference still demands an adaptation");
}

int main()
{
  theFixturesReachTheComparisonWithUnequalWhites();
  equivalentPccAcceptsAnEncodingDifference();
  equivalentPccRefusesADifferentWhite();
  equivalentPccStillRefusesEverythingElse();
  aChainAcrossAnEncodingDifferenceNeedsNoAdaptation();

  if (g_failures)
    std::printf("\n%d assertion(s) failed\n", g_failures);
  else
    std::printf("\nall checks passed\n");

  return g_failures;
}
