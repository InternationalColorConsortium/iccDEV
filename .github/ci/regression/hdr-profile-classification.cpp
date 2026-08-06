// Regression for the HDR Profile machinery of ICC.1 clause 8.10 and for the
// CICP ColourPrimaries amendment to clauses 9.2.17 / 10.3.
//
// Three things here are easy to get subtly wrong and impossible to notice from
// a profile that merely "validates clean":
//
// 1. The chromatic adaptation direction in icGetProfilePrimaries(). Clause 10.3
//    NOTE 1 asks for tristimulus values relative to the profile's *actual*
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
// 3. The conforming/intended split. Clause 8.10.1's definition is
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
// 2. Recovering primaries from the profile, per clause 10.3 NOTE 1 and NOTE 2
// ---------------------------------------------------------------------------
void testProfilePrimaries()
{
  // HagcHexData declares ColourPrimaries 2 and carries sRGB/BT.709 colorants
  // with no chromaticAdaptationTag. NOTE 2 governs: the profile's actual
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

  check(meta.HasContentLightLevel(), "CLL present");
  checkClose(meta.GetMaxContentLightLevel(), 1000.0, 1e-4, "MaxCLL");
  checkClose(meta.GetMaxFrameAverageLightLevel(), 400.0, 1e-4, "MaxFALL");

  check(meta.HasMasteringDisplayColourVolume(), "MDCV present");
  checkClose(meta.GetMasteringPrimaries().xRed, 0.708, 1e-5, "MDCV red x is BT.2020");
  checkClose(meta.GetMasteringMaxLuminance(), 1000.0, 1e-4, "MDCV max luminance");

  check(meta.HasDisplayColourVolume(), "DCV present");
  checkClose(meta.GetDisplayPrimaries().xRed, 0.640, 1e-5, "DCV red x is BT.709");
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
// 4. Classification and resolution
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
    check(info.bRgbMatrixBased, "recognised as three-component matrix-based");
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

  // TransferCharacteristics 13 with a HAGC tag present: the profile asserts HDR
  // machinery it does not qualify for, so it is "intended" rather than
  // conforming - which is what makes the TC rule reportable at all.
  pProfile = openFixture("HdrInvalidTransfer.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the bad-transfer fixture");
    check(info.nClass == icHdrProfileIntended, "bad TransferCharacteristics is 'intended', not conforming");
    check(info.bHasCicp, "cicp present");
    check(!info.bTransferIsHdr, "TransferCharacteristics 13 is not an HDR transfer");
    check(info.nTransferCharacteristics == 13, "the offending value is reported as-is");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "the bad transfer validates non-compliant");
    check(report.find("TransferCharacteristics is 13") != std::string::npos,
          "the report names the offending value");
    delete pProfile;
  }

  // A conforming HDR Profile can still break the pairing rule of 8.10.3 c),
  // which is why that check sits outside the conforming/intended split.
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
  testClassification();

  if (g_failures)
    printf("hdr-profile-classification: %d assertion(s) failed\n", g_failures);
  else
    printf("hdr-profile-classification: all assertions passed\n");

  return g_failures;
}
