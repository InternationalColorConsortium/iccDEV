// Behavioural regression for the A2B0/B2A0 HDR fallback bake - the procedure
// of the "Representation of HDR-to-SDR Tone Mapping from a Headroom Adaptive
// Gain Curve in a v4 A2B0 Tag" white paper and of the HAGC amendment's
// informative annex 2.
//
// The bake is a change of representation, not a second rendering algorithm:
// what goes into the tag is the clause 8.10.2 chain the CMM already evaluates,
// resampled at a target headroom of 1.0. That is the property worth testing,
// because it is the one a plausible-looking implementation loses.
//
// What each part guards, and why it is not obvious:
//
// 1. The A curve / CLUT seam. A lutAToBType linearises per channel in its A
//    curves and can only carry [0, 1] there, but an HDR transfer's output
//    reaches ~49 reference whites (PQ) and the HLG OOTF is not a per-channel
//    function at all. The split is therefore not "curve does the transfer,
//    CLUT does the tone map" - it is the peak-normalised per-channel part in
//    the curve and everything else in the CLUT. Test 1 pins the split as an
//    identity against CIccHdrTransfer::ToLinear(), so a future change to
//    either half that does not change the other is caught immediately.
//
// 2. The PCS encoding of the matrix. PCSXYZ is u1Fixed15Number, so an XYZ of
//    1.0 is written as 32768/65535 and the baked matrix is the profile's
//    colorant tags times that constant. Getting it wrong scales every baked
//    rendering by two, which still looks like a picture and still validates.
//    Test 2 catches it by comparing the baked pipeline against the CMM's own
//    HDR path at the same target headroom - two independently written routes
//    to the same number.
//
// 3. What survives serialisation. CLUT samples are unsigned 16-bit, so the
//    tag a file carries is not the tag that was built. Test 3 writes the
//    patched profile to memory, reads it back, and drives it through the CMM
//    as a legacy consumer would - through the LUT, with no HDR hint at all,
//    which is the whole point of the exercise.
//
// 4. The SDR clamp is real and is documented as a limit. A saturated pixel
//    whose tone-mapped component lands above reference white cannot be stored
//    in the tag. Test 2 pins both sides of that: agreement where the value
//    fits, and a deliberate disagreement where it does not, so that the clamp
//    cannot be quietly removed or quietly widened.
//
// 5. The pair is a pair. ICC.1 8.3.2 and clause 8.10 both require an AToB0Tag
//    to come with a BToA0Tag; a bake that cannot invert its gain curve has to
//    refuse rather than attach half of one. Test 5 pins the refusal and that
//    the profile is left untouched by it.
//
// Returns 0 on success; the number of failed assertions otherwise.

#include "IccHdrBake.h"
#include "IccHdrProfile.h"
#include "IccHdrToneMap.h"
#include "IccCmm.h"
#include "IccIO.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagLut.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

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
    printf("FAIL: %s (got %.9f, want %.9f, tol %g)\n", szWhat, got, want, tol);
    g_failures++;
  }
}

CIccProfile *openFixture(const char *szName)
{
  std::string path = "Testing/HDR/";
  path += szName;

  CIccProfile *pProfile = ReadIccProfile(path.c_str());

  if (!pProfile)
    printf("SKIP: cannot open %s (run Testing/CreateAllProfiles.sh)\n", path.c_str());

  return pProfile;
}

// A spread of device values that is not a regular grid, so that none of them
// happens to land on a CLUT node: neutrals, tints and saturated colours across
// the whole encoded range.
const icFloatNumber g_pixels[][3] = {
  { 0.00f, 0.00f, 0.00f },
  { 0.13f, 0.13f, 0.13f },
  { 0.37f, 0.37f, 0.37f },
  { 0.58f, 0.58f, 0.58f },
  { 0.81f, 0.81f, 0.81f },
  { 1.00f, 1.00f, 1.00f },
  { 0.42f, 0.31f, 0.27f },
  { 0.66f, 0.59f, 0.71f },
  { 0.29f, 0.55f, 0.44f },
  { 0.90f, 0.85f, 0.79f },
};

const int g_nPixels = (int)(sizeof(g_pixels) / sizeof(g_pixels[0]));

// The same idea confined to the part of the encoding the fixture's gain curve
// is a bijection over.
//
// Two separate ceilings meet at nearly the same place for HagcDisplay.icc, and
// both are properties of the tag rather than of this implementation. Above its
// last control point - reference white relative 1.0, which for a 300 cd/m^2
// reference white is PQ code 0.6218 - the annex's extrapolation makes
// gain(x)*x constant, so the forward map is flat and no inverse exists. And a
// tone-mapped component above reference white cannot be stored in an unsigned
// 16-bit CLUT sample, so the bake clamps where the live CMM path does not.
//
// Every component here is below that code, so these pixels are clamped by
// neither. They are the ones on which the bake and the CMM must agree exactly
// and on which the round trip must close. Deliberately not neutral: a neutral
// exercises only the row sums of the colour matrix and would pass with two of
// its columns exchanged.
const icFloatNumber g_lowPixels[][3] = {
  { 0.00f, 0.00f, 0.00f },
  { 0.13f, 0.13f, 0.13f },
  { 0.37f, 0.37f, 0.37f },
  { 0.55f, 0.55f, 0.55f },
  { 0.42f, 0.31f, 0.27f },
  { 0.29f, 0.55f, 0.44f },
  { 0.60f, 0.10f, 0.35f },
  { 0.08f, 0.47f, 0.60f },
};

const int g_nLowPixels = (int)(sizeof(g_lowPixels) / sizeof(g_lowPixels[0]));

// ---------------------------------------------------------------------------
// 1. The A curve / CLUT seam is a partition of ToLinear(), not a paraphrase
// ---------------------------------------------------------------------------
void testTransferSplit()
{
  static const icUInt8Number transfers[] = { icCicpTransferPQ, icCicpTransferHLG,
                                             icCicpTransferLinear };

  int t;

  for (t = 0; t < 3; t++) {
    CIccHdrTransfer transfer;

    check(transfer.Init(transfers[t], (icFloatNumber)203.0), "transfer initialises");

    int i;

    for (i = 0; i < g_nPixels; i++) {
      icFloatNumber whole[3], split[3];

      transfer.ToLinear(whole, g_pixels[i]);

      split[0] = transfer.ToLinearChannel(g_pixels[i][0]);
      split[1] = transfer.ToLinearChannel(g_pixels[i][1]);
      split[2] = transfer.ToLinearChannel(g_pixels[i][2]);

      // The per-channel half has to stay inside what a curveType can hold;
      // that is the entire reason the split exists.
      check(split[0] >= 0.0 && split[0] <= 1.0, "per-channel linearisation stays in [0, 1]");

      transfer.ChannelToReference(split, split);

      checkClose(split[0], whole[0], 1e-6, "split linearisation matches ToLinear (R)");
      checkClose(split[1], whole[1], 1e-6, "split linearisation matches ToLinear (G)");
      checkClose(split[2], whole[2], 1e-6, "split linearisation matches ToLinear (B)");
    }

    // The peak is the constant that maps one domain onto the other, so it has
    // to be exactly what an encoded 1.0 produces at the neutral.
    icFloatNumber white[3] = { 1.0f, 1.0f, 1.0f };
    icFloatNumber lin[3];

    transfer.ToLinear(lin, white);

    checkClose(transfer.GetPeakReferenceLevel(), lin[1], 1e-5,
               "peak reference level is what encoded white linearises to");
  }

  // And the two named cases, against the arithmetic rather than against this
  // implementation: PQ peaks at 10 000 cd/m^2 and HLG at its nominal Lw.
  CIccHdrTransfer pq;

  pq.Init(icCicpTransferPQ, (icFloatNumber)203.0);
  checkClose(pq.GetPeakReferenceLevel(), 10000.0 / 203.0, 1e-4, "PQ peak is 10 000 / CRWL");

  CIccHdrTransfer hlg;

  hlg.Init(icCicpTransferHLG, (icFloatNumber)203.0);
  checkClose(hlg.GetPeakReferenceLevel(), 1000.0 / 203.0, 1e-4, "HLG peak is Lw / CRWL");
}

// ---------------------------------------------------------------------------
// 1b. A copied profile is still the same profile
// ---------------------------------------------------------------------------
//
// CIccTagCicp's copy constructor had an empty body, so a copied cicpTag came
// out with four uninitialized fields. That is invisible until something reads
// them, and clause 8.10 is the first thing in the library that does: the
// transfer characteristic decides whether a profile is an HDR Profile at all.
// CIccXform::Create(CIccProfile&) copies the profile it is lent, so a
// consumer that used that overload got the ordinary matrix/TRC chain for a
// conforming HDR profile, with no diagnostic and a plausible-looking picture.
void testProfileCopyPreservesCicp()
{
  CIccProfile *pProfile = openFixture("HagcDisplay.icc");

  if (!pProfile)
    return;

  icHdrProfileInfo original;
  icHdrProfileInfo copied;

  check(icGetHdrProfileInfo(pProfile, original), "the fixture classifies");

  CIccProfile copy(*pProfile);

  check(icGetHdrProfileInfo(&copy, copied), "a copy of the fixture classifies");

  check(copied.nColourPrimaries == original.nColourPrimaries,
        "a copied profile keeps its CICP colour primaries");
  check(copied.nTransferCharacteristics == original.nTransferCharacteristics,
        "a copied profile keeps its CICP transfer characteristic");
  check(copied.nClass == original.nClass,
        "a copied profile keeps its clause 8.10 classification");

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 2. The baked pipeline is the CMM's own HDR path at a target headroom of 1.0
// ---------------------------------------------------------------------------

// Evaluate one pixel through the CMM's clause 8.10.2 path for a profile.
bool applyHdrPath(CIccProfile &profile, const icFloatNumber *src, icFloatNumber *dst)
{
  CIccCreateXformHintManager hints;
  CIccCreateHdrXformHint *pHint = new CIccCreateHdrXformHint();

  pHint->m_targetHeadroom = (icFloatNumber)icHdrBakeTargetHeadroom;
  pHint->m_nPolicy = icHdrToneMapPreferHagc;

  hints.AddHint(pHint);

  CIccXform *pXform = CIccXform::Create(profile, true, icRelativeColorimetric, icInterpLinear,
                                        NULL, icXformLutColor, true, &hints);

  if (!pXform) {
    printf("FAIL: no HDR xform created\n");
    g_failures++;
    return false;
  }

  icStatusCMM status = pXform->Begin();

  if (status != icCmmStatOk) {
    printf("FAIL: HDR xform Begin() returned %d\n", (int)status);
    g_failures++;
    delete pXform;
    return false;
  }

  CIccApplyXform *pApply = pXform->GetNewApply(status);

  if (!pApply || status != icCmmStatOk) {
    printf("FAIL: HDR xform GetNewApply() failed\n");
    g_failures++;
    delete pApply;
    delete pXform;
    return false;
  }

  pXform->Apply(pApply, dst, src);

  delete pApply;
  delete pXform;

  return true;
}

void testBakerMatchesCmm()
{
  CIccProfile *pProfile = openFixture("HagcDisplay.icc");

  if (!pProfile)
    return;

  CIccHdrBaker baker;

  if (!baker.Init(pProfile)) {
    printf("FAIL: baker declined HagcDisplay.icc: %s\n", baker.GetUnsupportedReason());
    g_failures++;
    delete pProfile;
    return;
  }

  check(baker.IsInvertible(), "the fixture's gain curve is invertible at this target");
  check(!baker.IsToneMapIdentity(), "the fixture's gain curve does something at this target");

  int i;

  for (i = 0; i < g_nLowPixels; i++) {
    icFloatNumber baked[3], live[3];

    baker.ToPcs(baked, g_lowPixels[i]);

    if (!applyHdrPath(*pProfile, g_lowPixels[i], live))
      break;

    // The two routes share no code below CIccHdrTransfer and CIccHagcEvaluator:
    // the CMM assembles the matrix from the colorant tags and scales through
    // XYZScale(), the baker assembles it here and scales by
    // icHdrBakePcsXyzScale. Agreement to float precision is the evidence that
    // both conventions are the same convention.
    //
    // The neutrals and near-neutrals above are chosen so that no channel's
    // tone-mapped value exceeds reference white; see below for what happens
    // when one does.
    checkClose(baked[0], live[0], 1e-5, "baked pipeline matches the CMM HDR path (X)");
    checkClose(baked[1], live[1], 1e-5, "baked pipeline matches the CMM HDR path (Y)");
    checkClose(baked[2], live[2], 1e-5, "baked pipeline matches the CMM HDR path (Z)");
  }

  // The documented limit, pinned so that it cannot drift in either direction.
  // A saturated red at full amplitude drives the component mixing well below
  // the component itself, so the gain that the mixed value earns leaves the
  // red channel above reference white - a value the CLUT's unsigned 16-bit
  // samples cannot hold. The bake clamps, the live path does not, and they
  // must therefore disagree here.
  icFloatNumber red[3] = { 1.0f, 0.0f, 0.0f };
  icFloatNumber bakedRed[3], liveRed[3];

  baker.ToPcs(bakedRed, red);

  if (applyHdrPath(*pProfile, red, liveRed)) {
    check(liveRed[0] > bakedRed[0] + 1e-4,
          "a saturated pixel above reference white is clamped by the bake and not by the CMM");
  }

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 3. What a legacy consumer gets out of the written file
// ---------------------------------------------------------------------------

// Serialise a profile and read it back, so that everything downstream sees the
// 16-bit samples a file actually carries rather than the floats that were
// built. Returns NULL on failure; the caller owns the result.
CIccProfile *roundTripThroughMemory(CIccProfile *pProfile)
{
  CIccMemIO io;

  // Generously sized: two 3-channel CLUTs and six curves, plus the tags that
  // were already there. Alloc() fails rather than growing, so this is sized
  // for the largest bake this test asks for.
  if (!io.Alloc(8 * 1024 * 1024, true)) {
    printf("FAIL: could not allocate the serialisation buffer\n");
    g_failures++;
    return NULL;
  }

  if (!pProfile->Write(&io)) {
    printf("FAIL: could not write the patched profile\n");
    g_failures++;
    return NULL;
  }

  // Not Tell(): Write() seeks back to fill in the header and the tag directory
  // once the tag data has been laid out, so the stream position afterwards is
  // near the front of the buffer rather than at its end. The size it recorded
  // in the header is the authoritative length.
  CIccProfile *pRead = ReadIccProfile(io.GetData(), pProfile->m_Header.size);

  if (!pRead) {
    printf("FAIL: could not read the patched profile back\n");
    g_failures++;
  }

  return pRead;
}

// Evaluate one pixel the way a CMM that knows nothing about clause 8.10 would:
// no hint at all, so CIccXform::Create() takes the A2B0 tag.
bool applyLutPath(CIccProfile &profile, const icFloatNumber *src, icFloatNumber *dst,
                  bool bInput, icXformType *pType)
{
  CIccXform *pXform = CIccXform::Create(profile, bInput, icRelativeColorimetric, icInterpLinear,
                                        NULL, icXformLutColor, true, NULL);

  if (!pXform) {
    printf("FAIL: no LUT xform created\n");
    g_failures++;
    return false;
  }

  if (pType)
    *pType = pXform->GetXformType();

  icStatusCMM status = pXform->Begin();

  if (status != icCmmStatOk) {
    printf("FAIL: LUT xform Begin() returned %d\n", (int)status);
    g_failures++;
    delete pXform;
    return false;
  }

  CIccApplyXform *pApply = pXform->GetNewApply(status);

  if (!pApply || status != icCmmStatOk) {
    printf("FAIL: LUT xform GetNewApply() failed\n");
    g_failures++;
    delete pApply;
    delete pXform;
    return false;
  }

  pXform->Apply(pApply, dst, src);

  delete pApply;
  delete pXform;

  return true;
}

void testBakedTags()
{
  CIccProfile *pProfile = openFixture("HagcDisplay.icc");

  if (!pProfile)
    return;

  CIccHdrBaker baker;

  if (!baker.Init(pProfile)) {
    printf("FAIL: baker declined HagcDisplay.icc: %s\n", baker.GetUnsupportedReason());
    g_failures++;
    delete pProfile;
    return;
  }

  const icChar *szReason = NULL;

  if (!icAddHdrFallbackTags(pProfile, NULL, &szReason)) {
    printf("FAIL: icAddHdrFallbackTags declined: %s\n", szReason ? szReason : "no reason");
    g_failures++;
    delete pProfile;
    return;
  }

  // The gain curve stays, or an HDR-aware CMM has lost the thing the bake was
  // made from and the profile has become SDR-only.
  check(pProfile->FindTag(icSigHeadroomAdaptiveGainCurveTag) != NULL,
        "the HAGC tag survives the bake");
  check(pProfile->m_Header.version == icVersionNumberV4_5,
        "the default version policy leaves the header alone");

  CIccTag *pAtoB = pProfile->FindTag(icSigAToB0Tag);
  CIccTag *pBtoA = pProfile->FindTag(icSigBToA0Tag);

  check(pAtoB && pAtoB->GetType() == icSigLutAtoBType, "an AToB0Tag of the right type is attached");
  check(pBtoA && pBtoA->GetType() == icSigLutBtoAType, "a BToA0Tag of the right type is attached");

  if (pAtoB && pAtoB->GetType() == icSigLutAtoBType) {
    CIccTagLutAtoB *pLut = (CIccTagLutAtoB*)pAtoB;

    // ICC.1 10.12 permits four stage lists, and A, CLUT, M, Matrix, B is the
    // only one of them that has both a CLUT and a matrix. The white paper's
    // A, CLUT, Matrix is not among the four, which is why the identity M and
    // B curves are here at all.
    check(pLut->GetCurvesA() != NULL, "the AToB tag has A curves");
    check(pLut->GetCLUT() != NULL, "the AToB tag has a CLUT");
    check(pLut->GetCurvesM() != NULL, "the AToB tag has M curves");
    check(pLut->GetMatrix() != NULL, "the AToB tag has a matrix");
    check(pLut->GetCurvesB() != NULL, "the AToB tag has B curves");

    if (pLut->GetCLUT())
      check(pLut->GetCLUT()->GridPoints() == icHdrBakeDefaultGridPoints,
            "the CLUT has the default grid size");
  }

  std::string report;

  check(pProfile->Validate(report) <= icValidateWarning,
        "the patched profile validates without errors");

  CIccProfile *pFile = roundTripThroughMemory(pProfile);

  if (!pFile) {
    delete pProfile;
    return;
  }

  int i;
  icXformType nType = icXformTypeUnknown;

  for (i = 0; i < g_nPixels; i++) {
    icFloatNumber want[3], got[3];

    baker.ToPcs(want, g_pixels[i]);

    if (!applyLutPath(*pFile, g_pixels[i], got, true, &nType))
      break;

    // Three sources of difference are folded into this tolerance: the CLUT's
    // trilinear interpolation between 33 nodes of a curved surface, the 16-bit
    // quantisation of both the curve and the CLUT samples, and the A curve's
    // own resampling. The first dominates by an order of magnitude, which is
    // why raising the grid size is what tightens it.
    //
    // Measured at the default 33 points, the worst pixel here is off by 0.008
    // and every other one by under 0.002. That worst case is the neutral at
    // device 0.81, which sits above the gain curve's last control point where
    // the annex's extrapolation makes the tone map flat: a trilinear cell
    // whose corners straddle that knee is the one place the surface has a
    // crease rather than a curve.
    checkClose(got[0], want[0], 1e-2, "the written AToB0 reproduces the bake (X)");
    checkClose(got[1], want[1], 1e-2, "the written AToB0 reproduces the bake (Y)");
    checkClose(got[2], want[2], 1e-2, "the written AToB0 reproduces the bake (Z)");
  }

  check(nType == icXformType3DLut,
        "a hintless consumer takes the baked LUT and not the matrix/TRC path");

  // And the round trip a paired A2B0/B2A0 exists to support.
  //
  // Checked in the PCS rather than only in device values, because the PCS is
  // where the invariant actually lives: what a BToA owes is device values that
  // re-render to the colour that was asked for, not device values that match
  // the ones some AToB happened to start from.
  //
  // The distinction is not pedantic here. A channel far darker than the other
  // two contributes less to the PCS than the forward direction's own
  // interpolation error does, and recovering it means extracting a term of
  // order 1e-6 from XYZ values of order 0.1 - the subtraction cancels
  // catastrophically and no BToA can undo it. That is a property of inverting
  // a colour matrix, not of this bake: with {0.08, 0.47, 0.60} the red channel
  // comes back at 0.045 while the rendered colour is right to four decimal
  // places. So the device comparison below is made only where a channel
  // carries a recoverable share of the PCS, and the PCS comparison is made
  // everywhere.
  for (i = 0; i < g_nLowPixels; i++) {
    icFloatNumber pcs[3], back[3], again[3];
    int c;

    if (!applyLutPath(*pFile, g_lowPixels[i], pcs, true, NULL))
      break;

    if (!applyLutPath(*pFile, pcs, back, false, NULL))
      break;

    if (!applyLutPath(*pFile, back, again, true, NULL))
      break;

    // Bounded by the forward direction's own interpolation error, since this
    // is that error evaluated at two nearby points rather than one - so it is
    // necessarily looser than a single forward pass and cannot be tighter.
    checkClose(again[0], pcs[0], 5e-3, "B2A0 output re-renders to the same PCS (X)");
    checkClose(again[1], pcs[1], 5e-3, "B2A0 output re-renders to the same PCS (Y)");
    checkClose(again[2], pcs[2], 5e-3, "B2A0 output re-renders to the same PCS (Z)");

    for (c = 0; c < 3; c++) {
      if (g_lowPixels[i][c] < 0.25)
        continue;

      checkClose(back[c], g_lowPixels[i][c], 2e-2, "B2A0 undoes A2B0 for a channel it can recover");
    }
  }

  delete pFile;
  delete pProfile;
}

// ---------------------------------------------------------------------------
// 4. The analytic round trip, without any table in the way
// ---------------------------------------------------------------------------
void testAnalyticRoundTrip()
{
  CIccProfile *pProfile = openFixture("HagcDisplay.icc");

  if (!pProfile)
    return;

  CIccHdrBaker baker;

  if (!baker.Init(pProfile)) {
    printf("FAIL: baker declined HagcDisplay.icc: %s\n", baker.GetUnsupportedReason());
    g_failures++;
    delete pProfile;
    return;
  }

  int i;

  for (i = 0; i < g_nLowPixels; i++) {
    icFloatNumber pcs[3], back[3];

    baker.ToPcs(pcs, g_lowPixels[i]);

    if (!baker.FromPcs(back, pcs)) {
      printf("FAIL: FromPcs() refused an in-gamut colour\n");
      g_failures++;
      break;
    }

    // No table anywhere in this loop, so the only tolerance is the inverse
    // bisection's own convergence.
    checkClose(back[0], g_lowPixels[i][0], 1e-4, "FromPcs undoes ToPcs (R)");
    checkClose(back[1], g_lowPixels[i][1], 1e-4, "FromPcs undoes ToPcs (G)");
    checkClose(back[2], g_lowPixels[i][2], 1e-4, "FromPcs undoes ToPcs (B)");
  }

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 4b. The other two transfer characteristics, end to end
// ---------------------------------------------------------------------------
//
// HagcDisplay.icc is PQ, and PQ is the case every stage was designed around.
// The other two exercise parts of the baker that it does not reach at all:
// HLG puts the OOTF - the one piece of the linearisation with no per-channel
// form - inside the CLUT, and Linear replaces the analytic transfer with the
// profile's own TRC tags, so its A curves come from CIccCurve::Apply() and its
// inverse from CIccCurve::Find() rather than from a closed form.
//
// The assertions here are the two that hold whatever the fixture is. The
// per-pixel agreement with the live CMM path and the device-space round trip
// both depend on where a particular gain curve stops being invertible, which
// is a property of the tag rather than of the code, so they stay with the
// fixture they were reasoned about.
void testTransferCoverage(const char *szFixture)
{
  CIccProfile *pProfile = openFixture(szFixture);

  if (!pProfile)
    return;

  CIccHdrBaker baker;

  if (!baker.Init(pProfile)) {
    printf("FAIL: baker declined %s: %s\n", szFixture, baker.GetUnsupportedReason());
    g_failures++;
    delete pProfile;
    return;
  }

  const icChar *szReason = NULL;

  if (!icAddHdrFallbackTags(pProfile, NULL, &szReason)) {
    printf("FAIL: icAddHdrFallbackTags declined %s: %s\n", szFixture,
           szReason ? szReason : "no reason");
    g_failures++;
    delete pProfile;
    return;
  }

  std::string report;

  check(pProfile->Validate(report) <= icValidateWarning,
        "the patched profile validates without errors");

  CIccProfile *pFile = roundTripThroughMemory(pProfile);

  if (!pFile) {
    delete pProfile;
    return;
  }

  int i;

  for (i = 0; i < g_nPixels; i++) {
    icFloatNumber want[3], pcs[3], back[3], again[3];

    baker.ToPcs(want, g_pixels[i]);

    if (!applyLutPath(*pFile, g_pixels[i], pcs, true, NULL))
      break;

    // Looser than the PQ fixture's 1e-2 for a reason that is worth stating,
    // because it bounds what any bake of this shape can promise. The
    // tone-mapped surface is not merely curved: at the gain curve's last
    // control point the annex's extrapolation makes it flat, so there is a
    // crease, and no interpolation over a grid reproduces a crease. HLG feels
    // it most, because its encoding packs ~4.9 reference whites into [0, 1]
    // and so spends much of its range beyond that knee.
    //
    // Measured on HagcCommonParams.icc, the worst pixel is the neutral just
    // below full scale: 0.012 forward and 0.030 through both tags at the
    // default 33 grid points, 0.005 and 0.013 at 45. Raising the grid is the
    // lever, and it is not a smooth one - at 65 points that pixel's forward
    // error is 0.009 again, because what matters is where the knee happens to
    // fall between two nodes rather than how many nodes there are.
    checkClose(pcs[0], want[0], 1.5e-2, "the written AToB0 reproduces the bake (X)");
    checkClose(pcs[1], want[1], 1.5e-2, "the written AToB0 reproduces the bake (Y)");
    checkClose(pcs[2], want[2], 1.5e-2, "the written AToB0 reproduces the bake (Z)");

    if (!applyLutPath(*pFile, pcs, back, false, NULL))
      break;

    if (!applyLutPath(*pFile, back, again, true, NULL))
      break;

    // The invariant that survives a flat tone map and a clamped channel alike:
    // whatever device values the BToA produces have to re-render to the colour
    // that was asked for.
    checkClose(again[0], pcs[0], 3.5e-2, "B2A0 output re-renders to the same PCS (X)");
    checkClose(again[1], pcs[1], 3.5e-2, "B2A0 output re-renders to the same PCS (Y)");
    checkClose(again[2], pcs[2], 3.5e-2, "B2A0 output re-renders to the same PCS (Z)");
  }

  delete pFile;
  delete pProfile;
}

// ---------------------------------------------------------------------------
// 5. What the bake refuses, and what it leaves behind when it does
// ---------------------------------------------------------------------------
void testRefusals()
{
  CIccHdrBaker baker;

  check(!baker.Init(NULL), "a null profile is refused");
  check(baker.GetUnsupportedReason() != NULL, "a refusal names its reason");

  CIccProfile *pProfile = openFixture("HagcDisplay.icc");

  if (!pProfile)
    return;

  icHdrBakeParams params;

  icHdrBakeParamsInit(params);
  params.nGridPoints = 1;
  check(!baker.Init(pProfile, &params), "a one-point CLUT axis is refused");

  icHdrBakeParamsInit(params);
  params.nCurveSize = 1;
  // Not an undersized table but a different tag: a curveType of size 1 is a
  // gamma value, so accepting it would write something that is not a sampled
  // curve at all.
  check(!baker.Init(pProfile, &params), "a one-entry A curve is refused");

  delete pProfile;

  // A profile whose cicpTag names a transfer characteristic outside clause
  // 8.10.1 has no HDR EOTF to invert, and the bake has to say so rather than
  // fall back to something plausible.
  pProfile = openFixture("HdrInvalidTransfer.icc");

  if (pProfile) {
    check(!baker.Init(pProfile), "a non-HDR transfer characteristic is refused");

    const icChar *szReason = NULL;

    check(!icAddHdrFallbackTags(pProfile, NULL, &szReason), "and no tags are attached");
    check(pProfile->FindTag(icSigAToB0Tag) == NULL, "the refused profile keeps no AToB0Tag");
    check(pProfile->FindTag(icSigBToA0Tag) == NULL, "the refused profile keeps no BToA0Tag");

    delete pProfile;
  }

  // The version policy is the one thing the bake changes about the header, and
  // only when asked.
  pProfile = openFixture("HagcDisplay.icc");

  if (pProfile) {
    icHdrBakeParams v44;

    icHdrBakeParamsInit(v44);
    v44.nVersionPolicy = icHdrBakeVersionV4_4;
    // A small grid: this case is about the header, and a 33-point bake of it
    // would cost a second of test time for nothing.
    v44.nGridPoints = 5;
    v44.nCurveSize = 32;

    check(icAddHdrFallbackTags(pProfile, &v44, NULL), "the v4.4 policy bakes");
    check(pProfile->m_Header.version == icVersionNumberV4_4,
          "the v4.4 policy rewrites the header version");

    delete pProfile;
  }
}

} // namespace

int main()
{
  testTransferSplit();
  testProfileCopyPreservesCicp();
  testBakerMatchesCmm();
  testBakedTags();
  testAnalyticRoundTrip();
  testTransferCoverage("HagcCommonParams.icc");   // HLG
  testTransferCoverage("HagcHexData.icc");        // Linear
  testRefusals();

  if (g_failures)
    printf("hdr-bake: %d failure(s)\n", g_failures);
  else
    printf("hdr-bake: all checks passed\n");

  return g_failures;
}
