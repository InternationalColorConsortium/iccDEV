// Regression for the HDR Profile machinery of ICC.1 clause 8.10 and for the
// CICP ColourPrimaries amendment to clauses 9.2.17 / 10.3.
//
// Three things here are easy to get subtly wrong and impossible to notice from
// a profile that merely "validates clean":
//
// 1. The chromatic adaptation direction in icGetProfilePrimaries(). Clause 10.3
//    clause 4.3 asks for tristimulus values relative to the profile's *actual*
//    adopted white. The matrix column tags are encoded relative to the PCS
//    adopted white (D50) and the chromaticAdaptationTag is the matrix that took
//    them there, so recovering the actual adopted white means applying its
//    INVERSE. Applying it forwards, or skipping it, still yields a plausible
//    set of chromaticities - just the wrong ones, off by the whole D65-to-D50
//    adaptation. The test pins a real BT.2020/D65 profile whose recovered white
//    must come back at D65 and not at D50.
//
// 2. The display headroom precedence of clause 8.10.5. NOTE 13 makes it
//    normative that DERH wins outright when all three entries are present and
//    disagree, and that the DCV/DRWL derivation "shall not be recomputed". A
//    reader that treats the list as a fallback chain gets the same answer when
//    the entries agree and a different one when they do not, so only a
//    deliberately inconsistent fixture can tell the two apart.
//
// 3. The content-headroom priority order of clause 8.10.4, which applies to the
//    Linear (8) transfer alone. Its three rules divide three different
//    numerators by the same CRWL, so a fixture whose CLL and MDCV agree, or
//    whose CRWL is the 203 default, cannot tell a correct implementation from
//    one that reached for the wrong entry - the arithmetic comes out the same.
//    The four HdrLinear* fixtures are built so that every branch lands on a
//    distinct value, and the fourth pins a ruling rather than a transcription:
//    8.10.4 states its 203 default twice with different conditions and an HAGC
//    tag carrying a custom reference white makes them disagree (HDR-10).
//
// 4. The conforming/intended split. Clause 8.10.1's definition is
//    self-satisfying - an HDR Profile is defined as carrying a cicpTag whose
//    TransferCharacteristics is 8, 16 or 18, so a profile that gets that field
//    wrong is not an HDR Profile and violates nothing. The classifier's
//    "intended" state is the non-circular hook that makes the rule reportable,
//    and the pairing rule of 8.10.3 c) has to stay outside that split because a
//    fully conforming profile can still break it.
//
// The fixtures come from Testing/HDR and must be built first; the test skips
// with a clear message rather than failing if they are absent, because that
// means CreateAllProfiles.sh has not run, not that the code is wrong.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccHdrProfile.h"
#include "IccProfile.h"
#include "IccTag.h"
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
    printf("FAIL: %s (got %.6f, want %.6f)\n", szWhat, got, want);
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

// ---------------------------------------------------------------------------
// 1. The ITU-T H.273 Table 2 lookup
// ---------------------------------------------------------------------------
void testCicpTable()
{
  icCicpPrimaries p;

  check(icGetCicpPrimaries(1, p), "BT.709 (1) is in the table");
  checkClose(p.xRed, 0.640, 1e-6, "BT.709 red x");
  checkClose(p.yGreen, 0.600, 1e-6, "BT.709 green y");
  checkClose(p.xWhite, 0.3127, 1e-6, "BT.709 white x (D65)");

  check(icGetCicpPrimaries(9, p), "BT.2020 (9) is in the table");
  checkClose(p.xRed, 0.708, 1e-6, "BT.2020 red x");
  checkClose(p.yGreen, 0.797, 1e-6, "BT.2020 green y");
  checkClose(p.xBlue, 0.131, 1e-6, "BT.2020 blue x");

  // 11 and 12 share their primaries and differ only in white: DCI white for
  // RP 431-2, D65 for EG 432-1. Conflating them is the classic P3 mistake.
  icCicpPrimaries dci, display;
  check(icGetCicpPrimaries(11, dci), "DCI P3 (11) is in the table");
  check(icGetCicpPrimaries(12, display), "Display P3 (12) is in the table");
  checkClose(dci.xRed, display.xRed, 1e-9, "P3 variants share their red primary");
  checkClose(dci.xGreen, display.xGreen, 1e-9, "P3 variants share their green primary");
  checkClose(dci.xWhite, 0.3140, 1e-6, "DCI P3 white x is DCI white, not D65");
  checkClose(display.xWhite, 0.3127, 1e-6, "Display P3 white x is D65");
  check(fabs(dci.xWhite - display.xWhite) > 1e-4, "the two P3 whites are distinct");

  // 10 is CIE XYZ: the primaries are the axes and the white is equal-energy E,
  // which is the one entry in the table that is not a D-series or C white.
  check(icGetCicpPrimaries(10, p), "CIE XYZ (10) is in the table");
  checkClose(p.xRed, 1.0, 1e-9, "XYZ red x is 1.0");
  checkClose(p.xWhite, 1.0 / 3.0, 1e-6, "XYZ white is equal-energy E");

  // Values with no chromaticities. 2 is the important one: it is not absent
  // because it is unassigned but because clause 9.2.17 sends it to the
  // profile's own tags instead.
  check(!icGetCicpPrimaries(0, p), "Reserved (0) has no primaries");
  check(!icGetCicpPrimaries(2, p), "Unspecified (2) has no fixed primaries");
  check(!icGetCicpPrimaries(3, p), "Reserved (3) has no primaries");
  check(!icGetCicpPrimaries(13, p), "unassigned (13) has no primaries");
  check(!icGetCicpPrimaries(255, p), "unassigned (255) has no primaries");
}

// ---------------------------------------------------------------------------
// 2. Recovering primaries from the profile, per clause 10.3 of the approved
//    CICP amendment: the procedure is normative body text there and its first
//    NOTE is the no-chromaticAdaptationTag case
// ---------------------------------------------------------------------------
void testProfilePrimaries()
{
  // HagcHexData declares ColourPrimaries 2 and carries sRGB/BT.709 colorants
  // with no chromaticAdaptationTag. The first NOTE governs: the profile's actual
  // adopted white is the PCS adopted white, so the encoded values are used as
  // they stand and the recovered white is D50 - NOT the D65 those colorants
  // would have had before adaptation.
  CIccProfile *pProfile = openFixture("HagcHexData.icc");
  if (!pProfile)
    return;

  icCicpPrimaries p;
  bool bFromProfile = false;

  check(icGetProfilePrimaries(pProfile, p), "primaries recovered from an unadapted profile");

  // D50 is x = 0.3457, y = 0.3585. The tolerance is loose because the media
  // white point is s15Fixed16 and the fixture's colorants are rounded.
  checkClose(p.xWhite, 0.3457, 2e-3, "unadapted profile recovers a D50 white x");
  checkClose(p.yWhite, 0.3585, 2e-3, "unadapted profile recovers a D50 white y");

  // The primaries are the D50-adapted sRGB colorants, so they sit near but not
  // exactly on the BT.709 chromaticities. What matters is that they are in the
  // right neighbourhood - a wrong adaptation direction would move them much
  // further than this.
  check(p.xRed > 0.55 && p.xRed < 0.70, "recovered red x is in the sRGB neighbourhood");
  check(p.yGreen > 0.55 && p.yGreen < 0.75, "recovered green y is in the sRGB neighbourhood");

  // The other branch, and the one that actually tests the adaptation direction.
  // HdrCicpUnspecified carries the same sRGB colorants adapted to D50 *plus* the
  // Bradford chromaticAdaptationTag that adapted them, so recovering the source
  // primaries means applying that matrix inverted. Done correctly the result is
  // the BT.709 primaries at D65 - the H.273 Table 2 entry for value 1. Skipping
  // the inverse leaves the D50 values above, which differ by roughly 0.033 in
  // white x: far outside these tolerances but entirely plausible-looking on its
  // own, which is why this assertion exists.
  CIccProfile *pAdapted = openFixture("HdrCicpUnspecified.icc");
  if (pAdapted) {
    icCicpPrimaries recovered, table;
    check(icGetProfilePrimaries(pAdapted, recovered), "primaries recovered from an adapted profile");
    check(icGetCicpPrimaries(1, table), "BT.709 reference available");

    // The tolerance is set by the s15Fixed16 encoding of the colorants and the
    // chad, not by the algorithm: both quantise to 1/65536.
    checkClose(recovered.xWhite, table.xWhite, 1e-3, "adapted profile recovers a D65 white x");
    checkClose(recovered.yWhite, table.yWhite, 1e-3, "adapted profile recovers a D65 white y");
    checkClose(recovered.xRed, table.xRed, 2e-3, "recovered red x is BT.709");
    checkClose(recovered.yRed, table.yRed, 2e-3, "recovered red y is BT.709");
    checkClose(recovered.xGreen, table.xGreen, 2e-3, "recovered green x is BT.709");
    checkClose(recovered.yGreen, table.yGreen, 2e-3, "recovered green y is BT.709");
    checkClose(recovered.xBlue, table.xBlue, 2e-3, "recovered blue x is BT.709");
    checkClose(recovered.yBlue, table.yBlue, 2e-3, "recovered blue y is BT.709");

    // Explicitly assert the two branches disagree, so that a build in which the
    // chromaticAdaptationTag is silently ignored cannot pass both this and the
    // D50 assertions above.
    check(fabs(recovered.xWhite - 0.3457) > 0.01,
          "the adapted recovery is not the unadapted D50 answer");

    icHdrProfileInfo adaptedInfo;
    check(icGetHdrProfileInfo(pAdapted, adaptedInfo), "info resolved for the adapted profile");
    check(adaptedInfo.bPrimariesResolved && adaptedInfo.bPrimariesFromProfile,
          "ColourPrimaries 2 resolves through the profile in the info struct too");
    checkClose(adaptedInfo.primaries.xWhite, table.xWhite, 1e-3,
               "the info struct carries the D65 recovery");

    delete pAdapted;
  }

  // icGetResolvedPrimaries must route value 2 to the profile and say so.
  check(icGetResolvedPrimaries(pProfile, 2, p, &bFromProfile),
        "value 2 resolves through the profile");
  check(bFromProfile, "value 2 reports that it came from the profile");

  // Any other value must come from the table instead, even for the same profile.
  bFromProfile = true;
  check(icGetResolvedPrimaries(pProfile, 9, p, &bFromProfile),
        "value 9 resolves through the H.273 table");
  check(!bFromProfile, "value 9 reports that it did not come from the profile");
  checkClose(p.xRed, 0.708, 1e-6, "value 9 ignores the profile's own colorants");

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 3. HDR Display metadata and the headroom precedence of clause 8.10.5
// ---------------------------------------------------------------------------
void testDisplayMetadata()
{
  CIccProfile *pProfile = openFixture("HdrDisplayMetadata.icc");
  if (!pProfile)
    return;

  CIccHdrMetadataReader meta;
  check(meta.Read(pProfile), "metadataTag read");

  check(meta.HasContentReferenceWhite(), "CRWL present");
  checkClose(meta.GetContentReferenceWhite(), 203.0, 1e-4, "CRWL value");

  // The registered shapes: CLL and MDCV each carry two floating point fields
  // then an 8-bit ITU-T H.273 ColourPrimaries code, not a chromaticity list.
  check(meta.HasContentLightLevel(), "CLL present");
  checkClose(meta.GetMaxContentLightLevel(), 1000.0, 1e-4, "MaxCLL");
  checkClose(meta.GetMaxFrameAverageLightLevel(), 400.0, 1e-4, "MaxFALL");
  check(meta.ContentLightLevelPrimariesResolved(), "CLL primaries code resolved");
  checkClose(meta.GetContentLightLevelPrimaries().xRed, 0.708, 1e-5,
             "CLL primaries code 9 is BT.2020");

  check(meta.HasMasteringDisplayColourVolume(), "MDCV present");
  check(meta.MasteringPrimariesResolved(), "MDCV primaries code resolved");
  checkClose(meta.GetMasteringPrimaries().xRed, 0.708, 1e-5, "MDCV code 9 is BT.2020");
  checkClose(meta.GetMasteringMaxLuminance(), 1000.0, 1e-4, "MDCV max luminance");
  checkClose(meta.GetMasteringMinLuminance(), 0.005, 1e-6, "MDCV min luminance");

  check(meta.HasDisplayColourVolume(), "DCV present");
  check(meta.DisplayPrimariesResolved(), "DCV primaries code resolved");
  checkClose(meta.GetDisplayPrimaries().xRed, 0.640, 1e-5, "DCV code 1 is BT.709");
  checkClose(meta.GetDisplayMaxLuminance(), 1000.0, 1e-4, "DCV max luminance");

  check(meta.HasDisplayReferenceWhite(), "DRWL present");
  check(meta.HasDisplayHeadroom(), "DERH present");

  check(!meta.HasUnparsedEntries(), "every entry in the fixture parsed");

  // The fixture's entries disagree on purpose: DERH says 4.0 while
  // DCV.maxLuminance / DRWL works out to 1000/203 = 4.926. NOTE 13 says DERH
  // wins and the derived value is not recomputed.
  icFloatNumber headroom = 0.0f;
  icHdrHeadroomSource src = meta.ResolveDisplayHeadroom(headroom);
  check(src == icHdrHeadroomDerh, "rule a) fires when DERH is present");
  checkClose(headroom, 4.0, 1e-5, "DERH wins outright over the DCV/DRWL derivation");
  check(fabs(headroom - 1000.0 / 203.0) > 0.5,
        "the derived value was not recomputed and averaged in");

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 3b. Building matrices from chromaticities
// ---------------------------------------------------------------------------
//
// These exist for SMPTE ST 2094-50 Annex A's gain application colour space,
// and clause 8.10.1 NOTE 2's forward matrix will need the same construction.
// Every expected value below is a published one - the canonical sRGB matrix
// and the standard BT.709 to BT.2020 coefficients - rather than a capture of
// what this implementation produces, which is the only way this test can
// catch the implementation being wrong in a self-consistent way.
void testPrimariesMatrices()
{
  icCicpPrimaries p709, p2020, pDci;
  icFloatNumber m[9];

  check(icGetCicpPrimaries(1, p709) && icGetCicpPrimaries(9, p2020) &&
        icGetCicpPrimaries(11, pDci), "the three primary sets are in the table");

  // The sRGB / BT.709 RGB-to-XYZ matrix, as published to six places.
  check(icBuildRgbToXyzMatrix(p709, m), "BT.709 builds");
  checkClose(m[0], 0.412391, 1e-5, "sRGB matrix [0][0]");
  checkClose(m[1], 0.357584, 1e-5, "sRGB matrix [0][1]");
  checkClose(m[2], 0.180481, 1e-5, "sRGB matrix [0][2]");
  checkClose(m[3], 0.212639, 1e-5, "sRGB matrix [1][0]");
  checkClose(m[4], 0.715169, 1e-5, "sRGB matrix [1][1]");
  checkClose(m[5], 0.072192, 1e-5, "sRGB matrix [1][2]");
  checkClose(m[6], 0.019331, 1e-5, "sRGB matrix [2][0]");
  checkClose(m[7], 0.119195, 1e-5, "sRGB matrix [2][1]");
  checkClose(m[8], 0.950532, 1e-5, "sRGB matrix [2][2]");

  // The defining property: RGB = (1,1,1) is the white point at Y = 1. This is
  // what fixes the column scaling, so a matrix built with the scaling wrong
  // still has the right column directions and fails only here.
  checkClose(m[3] + m[4] + m[5], 1.0, 1e-6, "unit RGB has Y = 1");
  checkClose(m[0] + m[1] + m[2], 0.3127 / 0.3290, 1e-5, "unit RGB has D65's X");

  // A chromaticity with no luminance names no colour.
  icCicpPrimaries bad = p709;
  bad.yGreen = 0.0;
  check(!icBuildRgbToXyzMatrix(bad, m), "a zero y is refused");

  // Same primaries in and out is the identity, and it is computed rather than
  // special cased, so this also says the two halves are consistent.
  check(icBuildPrimariesConversionMatrix(p709, p709, m), "709 to 709 builds");
  checkClose(m[0], 1.0, 1e-6, "identity [0][0]");
  checkClose(m[1], 0.0, 1e-6, "identity [0][1]");
  checkClose(m[4], 1.0, 1e-6, "identity [1][1]");
  checkClose(m[8], 1.0, 1e-6, "identity [2][2]");

  // BT.709 to BT.2020, both at D65 so no adaptation is involved. These are the
  // standard published coefficients.
  check(icBuildPrimariesConversionMatrix(p709, p2020, m), "709 to 2020 builds");
  checkClose(m[0], 0.627404, 1e-5, "709->2020 [0][0]");
  checkClose(m[1], 0.329283, 1e-5, "709->2020 [0][1]");
  checkClose(m[2], 0.043313, 1e-5, "709->2020 [0][2]");
  checkClose(m[3], 0.069097, 1e-5, "709->2020 [1][0]");
  checkClose(m[4], 0.919540, 1e-5, "709->2020 [1][1]");
  checkClose(m[8], 0.895595, 1e-5, "709->2020 [2][2]");

  // Each row sums to one: white is white in both spaces, which holds for any
  // correct conversion and is what a wrong white scaling breaks.
  checkClose(m[0] + m[1] + m[2], 1.0, 1e-6, "709->2020 preserves white, row 0");
  checkClose(m[3] + m[4] + m[5], 1.0, 1e-6, "709->2020 preserves white, row 1");
  checkClose(m[6] + m[7] + m[8], 1.0, 1e-6, "709->2020 preserves white, row 2");

  // DCI P3 to BT.709 crosses two different white points, so this is the case
  // the Bradford adaptation of ICC.1 Annex E actually does something in. The
  // white-to-white property is the strong assertion: a von Kries scaling of
  // XYZ, or no adaptation at all, fails it.
  check(icBuildPrimariesConversionMatrix(pDci, p709, m), "DCI P3 to 709 builds");
  checkClose(m[0] + m[1] + m[2], 1.0, 1e-6, "adapted conversion maps white to white, row 0");
  checkClose(m[3] + m[4] + m[5], 1.0, 1e-6, "adapted conversion maps white to white, row 1");
  checkClose(m[6] + m[7] + m[8], 1.0, 1e-6, "adapted conversion maps white to white, row 2");
  checkClose(m[0], 1.157516, 1e-4, "DCI->709 [0][0], Bradford adapted");
  checkClose(m[1], -0.154962, 1e-4, "DCI->709 [0][1]");
  checkClose(m[8], 1.096628, 1e-4, "DCI->709 [2][2]");

  // The HAGC chromaticities modes. HAGC-09: mode 0 is BT.709, which is H.273
  // value 1 - the proposal says "a value of 2", and value 2 has no
  // chromaticities at all, so a reader following the number gets nothing.
  icCicpPrimaries g;
  check(icHagcGetGainApplicationPrimaries(0, NULL, g), "mode 0 resolves");
  checkClose(g.xRed, 0.640, 1e-6, "mode 0 is BT.709, not H.273 value 2");
  check(icHagcGetGainApplicationPrimaries(1, NULL, g), "mode 1 resolves");
  checkClose(g.xRed, 0.680, 1e-6, "mode 1 is Display P3");
  checkClose(g.xWhite, 0.3127, 1e-6, "mode 1's white is D65, not DCI");
  check(icHagcGetGainApplicationPrimaries(2, NULL, g), "mode 2 resolves");
  checkClose(g.xRed, 0.708, 1e-6, "mode 2 is BT.2020");

  icFloatNumber custom[8] = { (icFloatNumber)0.7, (icFloatNumber)0.3,
                              (icFloatNumber)0.2, (icFloatNumber)0.7,
                              (icFloatNumber)0.1, (icFloatNumber)0.05,
                              (icFloatNumber)0.3127, (icFloatNumber)0.3290 };
  check(icHagcGetGainApplicationPrimaries(3, custom, g), "mode 3 resolves");
  checkClose(g.xGreen, 0.2, 1e-6, "mode 3 reads the tag's own values in order");
  check(!icHagcGetGainApplicationPrimaries(3, NULL, g), "mode 3 with no values is refused");
  check(!icHagcGetGainApplicationPrimaries(4, custom, g), "an unknown mode is refused");
}

// ---------------------------------------------------------------------------
// 3c. Clause 8.10.2 c)'s forward matrix, built from the cicpTag (HDR-07)
// ---------------------------------------------------------------------------
//
// The assertion that carries this one is not a number out of the standard: it
// is that for a CONVENTIONALLY authored profile - one that declares primaries
// in its cicpTag and also carries the matrix column tags a pre-amendment
// profile would have - the matrix derived from the cicpTag reproduces the
// tags. It has to, because both describe the same display, and the tags are
// what an author baked the chromatic adaptation into.
//
// That is exactly what NOTE 2's stated procedure does NOT do, which is HDR-07:
// stopping at the H.273 chromaticities and the adopted white leaves the result
// at the display's own white instead of the PCS adopted white, off by the
// whole D65-to-D50 adaptation.
void testHdrForwardMatrix()
{
  icFloatNumber m[9];

  CIccProfile *pProfile = openFixture("HdrDisplayMetadata.icc");

  if (pProfile) {
    // BT.709 in the cicpTag, sRGB colorants, and a Bradford D65-to-D50
    // chromaticAdaptationTag - the ordinary shape.
    check(icBuildHdrForwardMatrix(pProfile, 1, m), "the forward matrix builds for BT.709");

    // The profile's own red colorant, for comparison. 1e-4 is the s15Fixed16
    // grid the colorant tags are quantised onto plus the chad's own rounding;
    // it is not a loose tolerance, it is the exact one this can be checked to.
    checkClose(m[0], 0.436005, 1e-4, "derived matrix reproduces the red colorant X");
    checkClose(m[3], 0.222504, 1e-4, "derived matrix reproduces the red colorant Y");
    checkClose(m[1], 0.385101, 1e-4, "derived matrix reproduces the green colorant X");
    checkClose(m[4], 0.716904, 1e-4, "derived matrix reproduces the green colorant Y");
    checkClose(m[8], 0.713898, 1e-4, "derived matrix reproduces the blue colorant Z");

    // And the discriminator: NOTE 2 as written would give the canonical sRGB
    // matrix, whose red X is 0.412391. The adaptation is worth 0.024 here -
    // twenty times the agreement above - so a reader cannot mistake one
    // result for the other.
    check(fabs(m[0] - 0.412391) > 0.02,
          "the result is adapted to the PCS white, not left at the display's own");

    // Value 2 has no chromaticities to build from: 9.2.17 sends it to the
    // profile's own matrix column tags instead, and this must say so rather
    // than inventing a matrix.
    check(!icBuildHdrForwardMatrix(pProfile, 2, m), "ColourPrimaries 2 is not built here");
    check(!icBuildHdrForwardMatrix(pProfile, 0, m), "Reserved (0) builds nothing");
    check(!icBuildHdrForwardMatrix(pProfile, 13, m), "an unassigned value builds nothing");
    check(!icBuildHdrForwardMatrix(NULL, 1, m), "a null profile is refused");

    delete pProfile;
  }

  // A profile whose declared primaries and colorant tags genuinely disagree is
  // where the clause's "shall" bites: BT.2020 in the cicpTag against colorants
  // that are not BT.2020. The derived matrix must follow the cicpTag.
  pProfile = openFixture("HagcMixingTypes.icc");

  if (pProfile) {
    check(icBuildHdrForwardMatrix(pProfile, 9, m), "the forward matrix builds for BT.2020");
    check(fabs(m[0] - 0.636963) > 0.02,
          "a disagreeing profile follows its cicpTag, not its colorant tags");

    // BT.2020's blue primary has y = 0.046 and the green x = 0.170, so the
    // matrix is recognisably BT.2020 rather than BT.709: the red column's Y
    // is well above BT.709's 0.2225.
    check(m[3] > 0.26, "the red column's luminance is BT.2020's, not BT.709's");

    delete pProfile;
  }
}

// ---------------------------------------------------------------------------
// 4. The content-headroom priority order of clause 8.10.4 (Linear transfer)
// ---------------------------------------------------------------------------
void testContentHeadroom()
{
  icHdrProfileInfo info;

  // a) CLL wins over MDCV. The fixture's two entries disagree by a factor of
  // nearly seven, so a fallback chain that reached MDCV first returns 19.7
  // where the order returns 2.956 - no tolerance hides that.
  CIccProfile *pProfile = openFixture("HdrLinearCll.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the CLL fixture");
    check(info.nTransferCharacteristics == icCicpTransferLinear, "the fixture is Linear");
    check(info.nClass == icHdrProfileConforming, "Linear (8) is a conforming HDR transfer");
    check(info.nContentHeadroomSource == icHdrContentHeadroomCll,
          "rule a) fires when CLL is present");
    checkClose(info.contentHeadroom, 600.0 / 203.0, 1e-4, "CLL.max / CRWL");
    check(fabs(info.contentHeadroom - 4000.0 / 203.0) > 1.0,
          "the mastering peak was not reached for while CLL was available");
    delete pProfile;
  }

  // b) MDCV when CLL is absent, divided by a CRWL that is deliberately not the
  // 203 default: with 203 the b) branch would give 19.7, and a reader that
  // ignored the CRWL entry would still look right against a 203 fixture.
  pProfile = openFixture("HdrLinearMdcv.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the MDCV fixture");
    check(info.nContentHeadroomSource == icHdrContentHeadroomMdcv,
          "rule b) fires when CLL is absent and MDCV is present");
    checkClose(info.contentHeadroom, 40.0, 1e-4, "MDCV.max / CRWL, at the fixture's CRWL of 100");
    checkClose(info.contentReferenceWhite, 100.0, 1e-4, "the CRWL entry, not the 203 default");
    delete pProfile;
  }

  // c) both defaults at once: no metadataTag at all, so 1000 / 203. This is the
  // branch with no metadata precondition, which is why the order always
  // produces a value for a Linear profile.
  pProfile = openFixture("HdrLinearNoMetadata.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the bare fixture");
    check(info.nContentHeadroomSource == icHdrContentHeadroomDefault,
          "rule c) fires when neither CLL nor MDCV is present");
    checkClose(info.contentHeadroom, 1000.0 / 203.0, 1e-4,
               "the 1000 cd/m^2 typical mastering peak over the 203 default");
    check(!info.bContentReferenceWhiteFromProfile, "the reference white is the default");
    delete pProfile;
  }

  // The HDR-10 ruling: an HAGC tag's custom reference white is the divisor,
  // not the 203 the priority order's parenthesis names. 600/300 = 2.0 against
  // 600/203 = 2.956, and the two readings are indistinguishable on any fixture
  // whose HAGC tag leaves the reference white at its own 203 default.
  pProfile = openFixture("HdrLinearHagcWhite.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the HAGC Linear fixture");
    check(info.bHasHagc, "HAGC tag seen");
    checkClose(info.contentReferenceWhite, 300.0, 1e-3,
               "the HAGC tag's custom reference white is the resolved CRWL");
    check(info.nContentHeadroomSource == icHdrContentHeadroomCll, "CLL still selects rule a)");
    checkClose(info.contentHeadroom, 2.0, 1e-4,
               "Hcontent divides by the HAGC reference white, not by the 203 default");
    check(fabs(info.contentHeadroom - 600.0 / 203.0) > 0.5,
          "the other reading of 8.10.4's default is not the one taken");
    delete pProfile;
  }

  // The order is stated for Linear alone. A PQ profile carrying both entries
  // must report no metadata-derived content headroom at all: PQ fixes a peak
  // in the transfer function, and asserting 1000 cd/m^2 for it - which rule c)
  // would do, since it has no metadata precondition - would contradict it.
  pProfile = openFixture("HdrDisplayMetadata.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the PQ fixture");
    check(info.nTransferCharacteristics == icCicpTransferPQ, "the fixture is PQ");
    check(info.nContentHeadroomSource == icHdrContentHeadroomNone,
          "no content headroom is derived for a non-Linear transfer");
    checkClose(info.contentHeadroom, 0.0, 1e-9, "and no value is left behind");

    // The reader answers the same question directly when a caller asks it
    // outside the transfer gate - the gate is icGetHdrProfileInfo's, not the
    // reader's, and the reader has no cicpTag to consult.
    CIccHdrMetadataReader meta;
    check(meta.Read(pProfile), "metadataTag read");
    icFloatNumber headroom = 0.0f;
    icHdrContentHeadroomSource src = meta.ResolveContentHeadroom(headroom);
    check(src == icHdrContentHeadroomCll, "the reader applies rule a) when asked directly");
    checkClose(headroom, 1000.0 / 203.0, 1e-4, "against its own CRWL");
    delete pProfile;
  }
}

// ---------------------------------------------------------------------------
// 5. Classification and resolution
// ---------------------------------------------------------------------------
void testClassification()
{
  icHdrProfileInfo info;

  // A conforming HDR Profile: 4.5.0.0, RGB, Display, matrix-based, cicp with
  // TransferCharacteristics 16.
  CIccProfile *pProfile = openFixture("HdrDisplayMetadata.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved");
    check(info.nClass == icHdrProfileConforming, "metadata fixture is a conforming HDR Profile");
    // The revision's structural conditions, not the previous one's. An HDR
    // Profile is RGB and Input or Display; 8.10.1 then says the TRC tags
    // "shall not be present", so requiring the conventional six - which is
    // what bRgbMatrixBased still reports - would make every revision-shaped
    // profile unclassifiable.
    check(info.bRgbInputOrDisplay, "recognised as an RGB Input or Display profile");
    check(!info.bTrcTagsPresent, "and carries none of the three prohibited TRC tags");
    check(!info.bRgbMatrixBased, "so it is not the conventional six-tag shape NOTE 3 contrasts it with");
    check(info.bVersion4_5, "recognised as declaring 4.5.0.0");
    check(info.bHasCicp && info.bTransferIsHdr, "cicp present with an HDR transfer");
    check(!info.bHasHagc, "no HAGC tag, which 8.10.1 NOTE 3 makes optional");
    check(info.nHeadroomSource == icHdrHeadroomDerh, "headroom resolved through DERH");
    checkClose(info.displayHeadroom, 4.0, 1e-5, "resolved headroom");
    check(info.bPrimariesResolved && !info.bPrimariesFromProfile,
          "primaries resolved from the table, since ColourPrimaries is 9");
    delete pProfile;
  }

  // The HAGC tag's own HDR reference white takes precedence over a CRWL entry,
  // because the gain curve in that same tag was authored against it.
  pProfile = openFixture("HagcDisplay.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the HAGC fixture");
    check(info.nClass == icHdrProfileConforming, "HAGC fixture is a conforming HDR Profile");
    check(info.bHasHagc, "HAGC tag seen");
    check(info.bContentReferenceWhiteFromProfile, "reference white came from the profile");
    checkClose(info.contentReferenceWhite, 300.0, 1e-3,
               "reference white is the HAGC tag's custom value, not the 203 default");
    delete pProfile;
  }

  // TransferCharacteristics 13 with a HAGC tag present.  Clause 8.10.1's
  // conditions are definitional: this profile is simply not an HDR Profile.  It
  // is a valid ICC Display profile carrying two legal optional tags, so nothing
  // may be reported against it.  The classification is descriptive only.
  pProfile = openFixture("HdrInvalidTransfer.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the sRGB-transfer fixture");
    check(info.nClass == icHdrProfileHdrContent,
          "TransferCharacteristics 13 is HDR-related content, not an HDR Profile");
    check(info.bHasCicp, "cicp present");
    check(!info.bTransferIsHdr, "TransferCharacteristics 13 is not one of 8, 16 or 18");
    check(info.nTransferCharacteristics == 13, "the value is reported as-is");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv < icValidateWarning,
          "not being an HDR Profile is not a defect and draws no diagnostic");
    check(report.find("TransferCharacteristics") == std::string::npos,
          "no message is emitted about a membership condition");
    check(report.find("HDR:") == std::string::npos,
          "clause 8.10 says nothing about a profile outside the sub-class");
    delete pProfile;
  }

  // An HDR Profile can still break the pairing rule of 8.10.6, which is the one
  // requirement of clause 8.10 that a member of the sub-class can actually fail.
  pProfile = openFixture("HdrMissingBToA0.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the unpaired fixture");
    check(info.nClass == icHdrProfileConforming,
          "the unpaired profile is still a conforming HDR Profile");
    check(info.bHasAToB0 && !info.bHasBToA0, "AToB0 present, BToA0 absent");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "the unpaired tag validates non-compliant");
    check(report.find("without its paired BToA0Tag") != std::string::npos,
          "the report names the missing tag");
    delete pProfile;
  }

  // Clause 8.10.6's mandatory backward-compatibility pair. The AToB0Tag is
  // required "regardless of profile class" and nothing checked it: the Display
  // branch of CheckRequiredTags() accepts the pair OR the six matrix/TRC tags,
  // so a profile satisfying one alternative is never asked about the other.
  // That was right for a conventional profile and wrong for an HDR Profile,
  // where 8.10.6 requires the pair on top of everything else.
  pProfile = openFixture("HdrMissingLutPair.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the pairless fixture");

    // Membership first, because the diagnostic depends on it: a profile the
    // classifier rejected would be judged by some other class's rules and this
    // requirement would never be reached.
    check(info.nClass == icHdrProfileConforming,
          "a profile missing the pair is still an HDR Profile");
    check(!info.bHasAToB0 && !info.bHasBToA0, "and carries neither tag");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "the missing AToB0Tag is reported");
    check(report.find("AToB0Tag missing") != std::string::npos,
          "the report names the tag and the clause");
    delete pProfile;
  }

  // The one case where the matrix column tags ARE required: ColourPrimaries 2.
  // Value 2 names no chromaticities, so 8.10.2 c) has nothing to compute a
  // matrix from and the tags that would supply it directly are absent - the
  // profile has no RGB-to-PCSXYZ matrix at all.
  pProfile = openFixture("HdrCicp2NoColumns.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the columnless fixture");
    check(info.nClass == icHdrProfileConforming,
          "missing matrix columns is a missing required tag, not a failure to qualify");
    check(info.nColourPrimaries == 2, "the fixture declares ColourPrimaries 2");
    check(!info.bMatrixColumnsPresent, "and carries none of the three");

    // And the matrix genuinely cannot be built, which is why the clause
    // requires them: icBuildHdrForwardMatrix refuses value 2 by design.
    icFloatNumber m[9];
    check(!icBuildHdrForwardMatrix(pProfile, info.nColourPrimaries, m),
          "no matrix can be derived for ColourPrimaries 2");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "the missing matrix columns are reported");
    check(report.find("ColourPrimaries is 2") != std::string::npos,
          "the report names the condition that makes them required");
    delete pProfile;
  }

  // An Input-class HDR Profile with an AToB0Tag and no BToA0Tag is NOT
  // reported, and that is a ruling rather than an oversight (HDR-09).
  // 8.10.3 c) says "Whenever an AToBxTag is present, its paired BToAxTag shall
  // also be present" with no class condition; 8.10.6 confines the requirement
  // to Display. The two cannot both hold now that the AToB0Tag is mandatory in
  // both classes - and 8.10.3's own heading, "Tone-mapping descriptors
  // (informative precedence)", decides it: an informative clause cannot impose
  // a requirement, so 8.10.6 governs. HdrMissingBToA0 is a Display profile, so
  // it IS reported; this asserts the message says why.
  pProfile = openFixture("HdrMissingBToA0.icc");
  if (pProfile) {
    std::string report;
    pProfile->Validate(report);
    check(report.find("Display RGB HDR Profile") != std::string::npos,
          "the pairing diagnostic is scoped to the Display class, per 8.10.6");
    delete pProfile;
  }

  // A metadata primaries code of 2 resolves against the containing profile,
  // not to "unknown". The registry says so for CLL and CCV -- the primaries
  // are "defined by tags required by [a] three component matrix-based display
  // profile containing this metadata" -- and the ICC HDR Display registration
  // of 2026-06-24 says of DCV that a value of 2 "has the same meaning as in
  // the cicpTag", which under the CICP amendment is the same recovery.
  //
  // HdrCicpUnspecified is the one shape where that can be satisfied: it
  // declares ColourPrimaries 2, so 8.10.1 requires the matrix column tags, and
  // they are what resolves. Its colorants are sRGB with no chromatic
  // adaptation, so NOTE 2's case applies and the recovered white is D50.
  pProfile = openFixture("HdrCicpUnspecified.icc");
  if (pProfile) {
    CIccHdrMetadataReader meta2;
    check(meta2.Read(pProfile), "metadataTag read for the ColourPrimaries 2 fixture");

    check(meta2.HasContentLightLevel(), "CLL present");
    check(meta2.ContentLightLevelPrimariesResolved(),
          "a CLL primaries code of 2 resolves against the profile's own tags");
    checkClose(meta2.GetContentLightLevelPrimaries().xRed, 0.6400, 1e-3,
               "and gives the profile's red primary, not nothing");

    check(meta2.HasDisplayColourVolume(), "DCV present");
    check(meta2.DisplayPrimariesResolved(),
          "a DCV primaries code of 2 resolves the same way");

    // The discriminator: value 2 has no entry in H.273 Table 2 at all, so a
    // reader that looked it up there rather than in the profile would report
    // nothing resolved and no chromaticities.
    icCicpPrimaries table;
    check(!icGetCicpPrimaries(2, table),
          "H.273 Table 2 has no chromaticities for value 2, which is the point");

    delete pProfile;
  }

  // HDR Display metadata in an Input-class profile. Clause 8.10.5 opens "An HDR
  // Profile of the Display class ('mntr') may convey HDR display metadata",
  // 8.10.1's bullet repeats the condition, and the ICC registration of
  // 2026-06-24 files DCV, DRWL and DERH under a category named HDR Display. So
  // the entries are outside the only clause that gives them meaning - but
  // nothing forbids them, and they are Optional, so the profile is valid.
  pProfile = openFixture("HdrInputDisplayMeta.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the Input-class fixture");
    check(info.nClass == icHdrProfileConforming,
          "an Input-class HDR Profile with AToB0 and no BToA0 is conforming");
    check(info.bHasAToB0 && !info.bHasBToA0,
          "8.10.6 requires the pair only for Display; this is HDR-09's ruling");

    // The reader still reports what the file contains - it reports bytes, and
    // scope belongs to the consumer.
    CIccHdrMetadataReader meta3;
    check(meta3.Read(pProfile), "the metadataTag is still read");
    check(meta3.HasDisplayHeadroom(), "and the out-of-scope DERH is still reported");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv < icValidateWarning, "the profile is valid: nothing forbids the bytes");
    check(report.find("not of the Display class") != std::string::npos,
          "but the scope note is emitted");
    delete pProfile;
  }

  // A plain SDR profile must draw nothing. This is the false-positive guard:
  // the corpus is full of RGB display profiles, and a classifier that keyed on
  // the version alone, or on the presence of a cicpTag alone, would start
  // reporting HDR diagnostics against all of them.
  CIccProfile *pSdr = ReadIccProfile("Testing/V2/v2RgbMatrixTRC.icc");
  if (pSdr) {
    check(icGetHdrProfileInfo(pSdr, info), "info resolved for an SDR profile");
    check(info.nClass == icHdrProfileNone, "a plain v2 RGB display profile is not HDR");
    checkClose(info.contentReferenceWhite, 203.0, 1e-6,
               "the 203 cd/m^2 default applies even with no HDR content");
    check(!info.bContentReferenceWhiteFromProfile, "and is reported as a default, not a tag value");

    std::string report;
    pSdr->Validate(report);
    check(report.find("HDR:") == std::string::npos, "an SDR profile draws no HDR diagnostic");
    delete pSdr;
  }
  else {
    printf("SKIP: Testing/V2/v2RgbMatrixTRC.icc absent\n");
  }
}

} // namespace

int main()
{
  testCicpTable();
  testProfilePrimaries();
  testDisplayMetadata();
  testPrimariesMatrices();
  testHdrForwardMatrix();
  testContentHeadroom();
  testClassification();

  if (g_failures)
    printf("hdr-profile-classification: %d assertion(s) failed\n", g_failures);
  else
    printf("hdr-profile-classification: all assertions passed\n");

  return g_failures;
}
