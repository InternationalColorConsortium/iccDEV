// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression for the invariants the *authoring* side of the
// headroomAdaptiveGainCurveTag ('HAGC') has to establish, which the binary read
// path establishes for free and which the XML/JSON parsers and the direct
// setter API therefore have to be held to explicitly.
//
// The tag has an unusual property: nothing in the encoding is redundant, so the
// decoder's clamps are not merely defensive - they are what *defines* the
// domain the evaluator is written against. Read a tag from a file and its
// control point X values are in [0, 64] because icHagcDecodeX put them there;
// its coefficient array holds exactly what its mixing type implies because
// Unpack() rebuilt it from the type. Build the same model in memory and none of
// that has happened. Every case below is one of those invariants failing to
// hold on a path that did not go through the decoder.
//
// 1. Component mixing coefficients (XML parser). Types 0 to 2 fix their
//    coefficients implicitly, so a parser has to fill them in from the type.
//    Doing that with an open-coded switch that only *writes* the arms each type
//    needs - rather than zeroing the array first, as the binary reader's shared
//    helper does - leaves the constructor's type 0 default of kMax = 1 in place
//    on a type 3 alternate. Pack() derives the coefficient presence flags from
//    the values, so the phantom entry is written out as a real coefficient: the
//    flag byte is wrong, the record is two bytes longer than authored, and the
//    coefficient sum is 2.0 instead of 1.0, which halves every mixed value the
//    gain curve sees. On a type 1 alternate the same staleness is invisible on
//    disk (types 0 to 2 serialize no coefficients) and changes only what the
//    in-process tag computes. Note that the type 1 assertions here are red only
//    when case 2 below has *also* been reverted: SetMetadata()'s round trip
//    scrubs a stale coefficient that never reached the bytes, which is what
//    "case 2 subsumes case 1" means in practice. They are kept because they
//    state the invariant the parser is meant to hold to on its own.
//
// 2. SetMetadata() round trip. Pack() refuses only a control point count
//    outside 1..32 and too many alternates; everything else it clamps or
//    re-derives. A SetMetadata() that stores the caller's model verbatim beside
//    those bytes leaves a tag whose model and wire form disagree - and since it
//    is the clamps that establish the domain, the disagreement is load bearing.
//    A negative control point X is the sharp case: unencodable, so no file can
//    carry one, and CIccHagcEvaluator's log(x[last]/v) is written on that basis.
//    Fed one, it used to return NaN for every pixel while Init() and
//    IsInvertible() both said yes and Validate() reported nothing at all.
//
// 3. Exported entry points with no bound of their own.
//    icHagcDerivePchipSlopes() sizes its secant arrays by icHagcMaxControlPoints
//    (32) and indexes them by an icUInt8Number (0..255); the in-library caller
//    clamps first, so the cap has to live with the arrays rather than with the
//    one caller that respects it. CIccHdrBaker::Init() documented "at least 2"
//    entries per A curve with no maximum, but CIccTagCurve::SetSize() answers a
//    request above 65536 by freeing its buffer, zeroing its size and returning
//    *true*, and the curve fill then writes nCurveSize floats through an
//    unchecked operator[] onto NULL.
//
// 4. Fields the encoding cannot widen. Application Version is three bits; a
//    model asking for 8 must be refused rather than masked down to 0, in both
//    the parser and Pack(). CIccHagcEvaluator::SetTargetHeadroom() must refuse
//    NaN, which compares false against every bracket and would otherwise fall
//    through to the interpolating branch and make every weight NaN.
//
// Test 1 drives the real XML parser over Testing/HDR/HagcMixingTypes.xml, which
// exists for this and is the only fixture using mixing type 1 or a partly
// authored type 3 coefficient array. The rest need no fixture.
//
// Returns 0 on success; the number of failed assertions otherwise.

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccHdrBake.h"
#include "IccHdrToneMap.h"
#include "IccProfile.h"
#include "IccTagHagc.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool cond, const char *szWhat)
{
  if (!cond) {
    printf("FAIL: %s\n", szWhat);
    g_failures++;
  }
}

void checkClose(double got, double want, double tol, const char *szWhat)
{
  if (!(fabs(got - want) <= tol)) {
    printf("FAIL: %s (got %.9g, want %.9g)\n", szWhat, got, want);
    g_failures++;
  }
}

// The tag as the XML parser built it, before anything has been written out.
// Returned by pointer into the profile, which the caller keeps alive.
const CIccTagHagc *findHagc(CIccProfile *pProfile)
{
  CIccTag *pTag = pProfile ? pProfile->FindTag(icSigHeadroomAdaptiveGainCurveTag) : NULL;

  return pTag && pTag->GetType() == icSigHeadroomAdaptiveGainCurveType
       ? (const CIccTagHagc *)pTag : NULL;
}

// ---------------------------------------------------------------------------
// 1. The XML parser must leave an alternate's coefficient array holding exactly
//    what its mixing type implies and nothing else.
// ---------------------------------------------------------------------------
void testXmlMixingTypeCoefficients()
{
  CIccProfileXml profile;
  std::string parseStr;

  if (!profile.LoadXml("Testing/HDR/HagcMixingTypes.xml", NULL, &parseStr)) {
    printf("SKIP: cannot parse Testing/HDR/HagcMixingTypes.xml (%s)\n", parseStr.c_str());
    return;
  }

  const CIccTagHagc *pTag = findHagc(&profile);

  if (!pTag) {
    check(false, "HagcMixingTypes.xml has no headroomAdaptiveGainCurveTag");
    return;
  }

  const icHagcMetadata &m = pTag->GetMetadata();

  check(m.GetNumAlternates() == 2, "HagcMixingTypes has two alternate images");

  const icHagcAlternateImage *pAlt0 = m.GetAlternate(0);
  const icHagcAlternateImage *pAlt1 = m.GetAlternate(1);

  if (!pAlt0 || !pAlt1) {
    check(false, "HagcMixingTypes alternates are readable");
    return;
  }

  // Alternate 0 is type 3 with only kRed authored. The other five must be zero:
  // a leftover kMax = 1 here is the defect, and it is worth naming the one that
  // matters most - the coefficient sum, which is what divides the mixed value.
  check(pAlt0->m_nMixingType == icHagcMixingCustom, "alternate 0 is mixing type 3");
  checkClose(pAlt0->m_coef[icHagcCoefRed], 1.0, 1e-6, "alternate 0 kRed is what was authored");
  checkClose(pAlt0->m_coef[icHagcCoefMax], 0.0, 1e-12,
             "alternate 0 kMax is zero, not the constructor's type 0 default");

  double sum = 0.0;
  for (int i = 0; i < icHagcNumCoefficients; i++)
    sum += pAlt0->m_coef[i];
  checkClose(sum, 1.0, 1e-6, "alternate 0 coefficients sum to what was authored");

  // Alternate 1 is type 1, whose only non-zero coefficient is kComponent.
  check(pAlt1->m_nMixingType == icHagcMixingComponent, "alternate 1 is mixing type 1");
  checkClose(pAlt1->m_coef[icHagcCoefComponent], 1.0, 1e-6, "alternate 1 kComponent is 1");
  checkClose(pAlt1->m_coef[icHagcCoefMax], 0.0, 1e-12,
             "alternate 1 kMax is zero, not the constructor's type 0 default");

  // The parsed model and the bytes it produced have to agree, which is the
  // property that makes the two paths one path.  This is where the type 1 case
  // shows up at all: it serializes no coefficients, so only a decode of the
  // authored bytes can say what a reader will see.
  icHagcMetadata decoded;
  check(decoded.Unpack(pTag->GetRawMetadata(), pTag->GetRawMetadataSize()),
        "the authored metadata block decodes");

  const icHagcAlternateImage *pDec0 = decoded.GetAlternate(0);
  const icHagcAlternateImage *pDec1 = decoded.GetAlternate(1);

  if (pDec0 && pDec1) {
    for (int i = 0; i < icHagcNumCoefficients; i++) {
      checkClose(pDec0->m_coef[i], pAlt0->m_coef[i], 1e-4,
                 "alternate 0 coefficient survives the round trip unchanged");
      checkClose(pDec1->m_coef[i], pAlt1->m_coef[i], 1e-4,
                 "alternate 1 coefficient survives the round trip unchanged");
    }
  }
  else {
    check(false, "the decoded block has both alternates");
  }

  // Two evaluators - one over the parsed model, one over a decode of its own
  // bytes - have to render identically.  This is where the type 1 alternate
  // shows up at all: a stale kMax = 1 sitting beside kComponent = 1 changes the
  // component mixing decision and therefore the gain, but the tag it wrote is
  // unaffected, so only comparing the in-process tag against its own output
  // catches it.  A headroom of 4.0 falls between the two alternates, so both
  // contribute.
  CIccHagcEvaluator evParsed, evDecoded;

  if (evParsed.Init(m) && evDecoded.Init(decoded) &&
      evParsed.SetTargetHeadroom(4.0f) && evDecoded.SetTargetHeadroom(4.0f)) {
    static const icFloatNumber kPixels[][3] = {
      { 0.10f, 0.10f, 0.10f },
      { 0.80f, 0.20f, 0.05f },
      { 0.05f, 0.60f, 0.90f },
      { 0.99f, 0.99f, 0.01f },
    };

    check(evParsed.IsInvertible() == evDecoded.IsInvertible(),
          "the parsed tag and its own bytes agree about invertibility");

    for (size_t p = 0; p < sizeof(kPixels) / sizeof(kPixels[0]); p++) {
      icFloatNumber a[3], b[3];

      evParsed.Apply(a, kPixels[p]);
      evDecoded.Apply(b, kPixels[p]);

      for (int c = 0; c < 3; c++)
        checkClose(a[c], b[c], 1e-5,
                   "the parsed tag renders a pixel the same as its own bytes do");
    }
  }
  else {
    check(false, "both evaluators initialise on HagcMixingTypes");
  }
}

// ---------------------------------------------------------------------------
// 2. SetMetadata() must keep a decode of its own bytes, not the caller's model.
// ---------------------------------------------------------------------------
void testSetMetadataRoundTrip()
{
  icHagcMetadata m;

  m.m_bHeadroomAdaptiveToneMap = true;
  m.m_baselineHeadroom = 0.0f;
  check(m.SetNumAlternates(1), "one alternate image");

  icHagcAlternateImage *pAlt = m.GetAlternate(0);

  if (!pAlt) {
    check(false, "the alternate is readable");
    return;
  }

  pAlt->m_headroom = 3.0f;
  pAlt->SetMixingType(icHagcMixingMax);
  pAlt->m_nControlPoints = 3;

  // Negative X is unencodable - icHagcDecodeX clamps to [0, 64] - so no file
  // can carry this and the evaluator's log(x[last]/v) never has to cope with it.
  pAlt->m_x[0] = -3.0f; pAlt->m_y[0] = 0.1f; pAlt->m_slope[0] = 0.0f;
  pAlt->m_x[1] = -2.0f; pAlt->m_y[1] = 0.2f; pAlt->m_slope[1] = 0.0f;
  pAlt->m_x[2] = -1.0f; pAlt->m_y[2] = 0.3f; pAlt->m_slope[2] = 0.0f;

  CIccTagHagc tag;

  if (!tag.SetMetadata(m)) {
    check(false, "SetMetadata accepts a model Pack() can encode");
    return;
  }

  icHagcMetadata decoded;
  check(decoded.Unpack(tag.GetRawMetadata(), tag.GetRawMetadataSize()),
        "the stored block decodes");

  const icHagcAlternateImage *pKept = tag.GetMetadata().GetAlternate(0);
  const icHagcAlternateImage *pDec = decoded.GetAlternate(0);

  if (!pKept || !pDec) {
    check(false, "both models have their alternate");
    return;
  }

  for (int i = 0; i < (int)pKept->m_nControlPoints; i++) {
    checkClose(pKept->m_x[i], pDec->m_x[i], 1e-4,
               "the model the tag kept matches a decode of its own bytes");
    check(pKept->m_x[i] >= 0.0f,
          "no control point X survives SetMetadata outside the encodable range");
  }

  // The evaluator now sees what a reader would see: three equal X values, which
  // is not strictly increasing, so it refuses rather than returning NaN pixels.
  CIccHagcEvaluator ev;
  bool bInit = ev.Init(tag.GetMetadata());

  check(!bInit, "the evaluator refuses the clamped, non-increasing curve");

  if (!bInit) {
    icFloatNumber in[3] = { 0.5f, 0.25f, 0.75f }, out[3] = { 0, 0, 0 };
    ev.Apply(out, in);
    for (int i = 0; i < 3; i++)
      check(out[i] == in[i], "an unsupported evaluator is the identity, not NaN");
  }

  // Validate() has to say something about it; an empty report on a tag the
  // evaluator refuses is how this stayed invisible.
  std::string report;
  icValidateStatus status = tag.Validate("tag:", report, NULL);
  check(status != icValidateOK, "Validate refuses the clamped curve");
  check(!report.empty(), "Validate says why");
}

// ---------------------------------------------------------------------------
// 3. Exported entry points bound their own inputs.
// ---------------------------------------------------------------------------
void testExportedEntryPointBounds()
{
  icFloatNumber x[256], y[256], slope[256];

  for (int i = 0; i < 256; i++) {
    x[i] = (icFloatNumber)(i + 1);
    y[i] = (icFloatNumber)(i * 0.5);
    slope[i] = (icFloatNumber)-1.0;
  }

  check(icHagcDerivePchipSlopes(x, y, icHagcMaxControlPoints, slope),
        "PCHIP slopes accept the maximum control point count");
  check(!icHagcDerivePchipSlopes(x, y, icHagcMaxControlPoints + 1, slope),
        "PCHIP slopes refuse one point past the maximum");
  check(!icHagcDerivePchipSlopes(x, y, 255, slope),
        "PCHIP slopes refuse the largest count the argument type can hold");
  check(!icHagcDerivePchipSlopes(x, y, 0, slope), "PCHIP slopes refuse zero points");

  // The baker's A curve size.  CIccTagCurve::SetSize() reports a request above
  // 65536 as success with an empty buffer, so anything that trusts that return
  // writes through a NULL curve.
  CIccProfile *pProfile = ReadIccProfile("Testing/HDR/HagcMixingTypes.icc");

  if (!pProfile) {
    printf("SKIP: cannot open Testing/HDR/HagcMixingTypes.icc "
           "(run Testing/CreateAllProfiles.sh)\n");
    return;
  }

  icHdrBakeParams params;
  CIccHdrBaker baker;

  icHdrBakeParamsInit(params);
  params.nCurveSize = icHdrBakeMaxCurveSize;
  check(baker.Init(pProfile, &params), "the baker accepts the largest curve a curveType holds");

  icHdrBakeParamsInit(params);
  params.nCurveSize = icHdrBakeMaxCurveSize + 1;
  check(!baker.Init(pProfile, &params), "the baker refuses one entry past that");

  icHdrBakeParamsInit(params);
  params.nCurveSize = 1;
  check(!baker.Init(pProfile, &params), "the baker refuses a one entry curve");

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 4. Fields the encoding cannot widen, and NaN.
// ---------------------------------------------------------------------------
void testUnwidenableFields()
{
  icHagcMetadata m;

  m.m_bHeadroomAdaptiveToneMap = true;
  m.m_nApplicationVersion = icHagcMaxApplicationVersion;

  std::vector<icUInt8Number> buf;
  check(m.Pack(buf), "Pack accepts the largest application version the field holds");

  m.m_nApplicationVersion = icHagcMaxApplicationVersion + 1;
  buf.clear();
  check(!m.Pack(buf), "Pack refuses an application version past the field, rather than masking it");

  m.m_nApplicationVersion = 0;
  m.m_nMinApplicationVersion = icHagcMaxApplicationVersion + 1;
  buf.clear();
  check(!m.Pack(buf), "Pack refuses an out of range minimum application version too");

  // NaN target headroom.  Every bracket comparison is false against it, so it
  // would reach the interpolating branch and make both weights NaN.
  icHagcMetadata t;
  t.m_bHeadroomAdaptiveToneMap = true;
  t.m_baselineHeadroom = 0.0f;

  if (!t.SetNumAlternates(1))
    return;

  icHagcAlternateImage *pAlt = t.GetAlternate(0);

  if (!pAlt)
    return;

  pAlt->m_headroom = 3.0f;
  pAlt->SetMixingType(icHagcMixingMax);
  pAlt->m_nControlPoints = 2;
  pAlt->m_x[0] = 0.5f; pAlt->m_y[0] = 0.1f; pAlt->m_slope[0] = 0.0f;
  pAlt->m_x[1] = 1.5f; pAlt->m_y[1] = 0.3f; pAlt->m_slope[1] = 0.0f;

  // Through the tag rather than straight into the evaluator: Init() requires a
  // model that came out of Unpack(), which is the same property test 2 pins
  // from the other side.
  CIccTagHagc tag;
  CIccHagcEvaluator ev;

  if (!tag.SetMetadata(t) || !ev.Init(tag.GetMetadata())) {
    check(false, "the NaN case's evaluator initialises");
    return;
  }

  check(ev.SetTargetHeadroom(1.0f), "a finite target is accepted");

  const icFloatNumber nan = (icFloatNumber)(0.0 / 0.0);
  check(!ev.SetTargetHeadroom(nan), "a NaN target headroom is refused");

  // The refusal must leave the last good target in place, not a half-set one.
  icFloatNumber in[3] = { 0.5f, 0.25f, 0.75f }, out[3] = { 0, 0, 0 };
  ev.Apply(out, in);
  for (int i = 0; i < 3; i++)
    check(out[i] == out[i], "the refused NaN target did not reach the pixels");
}

}  // namespace

int main()
{
  // The XML tag handlers have to be on the stack before LoadXml(), or the HAGC
  // element parses as an unknown tag and test 1 has nothing to look at.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  testXmlMixingTypeCoefficients();
  testSetMetadataRoundTrip();
  testExportedEntryPointBounds();
  testUnwidenableFields();

  if (g_failures)
    printf("%d assertion(s) failed\n", g_failures);
  else
    printf("all HAGC authoring invariants hold\n");

  return g_failures;
}
