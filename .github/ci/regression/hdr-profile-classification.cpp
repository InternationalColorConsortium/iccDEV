/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

// Regression for the HDR ColorSpace Profile machinery of ICC.1 clause 8.7.1 and for the
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
// 2. The display headroom precedence of clause 8.10.5. NOTE 14 makes it
//    normative that DERH wins outright when all three entries are present and
//    disagree, and that the DCV/DRWL derivation "shall not be recomputed". A
//    reader that treats the list as a fallback chain gets the same answer when
//    the entries agree and a different one when they do not, so only a
//    deliberately inconsistent fixture can tell the two apart.
//
// 3. The content-headroom priority order of clause 8.7.1.4, which applies to the
//    Linear (8) transfer alone. Its three rules divide three different
//    numerators by the same CRWL, so a fixture whose CLL and MDCV agree, or
//    whose CRWL is the 203 default, cannot tell a correct implementation from
//    one that reached for the wrong entry - the arithmetic comes out the same.
//    The four HdrLinear* fixtures are built so that every branch lands on a
//    distinct value, and the fourth pins a ruling rather than a transcription:
//    8.7.1.4 states its 203 default twice with different conditions and an HAGC
//    tag carrying a custom reference white makes them disagree (HDR-10).
//
// 4. The conforming/intended split. Clause 8.7.1.1's definition is
//    self-satisfying - an HDR ColorSpace Profile is defined as carrying a cicpTag whose
//    TransferCharacteristics is 8, 16 or 18, so a profile that gets that field
//    wrong is not an HDR ColorSpace Profile and violates nothing. The classifier's
//    "intended" state is the non-circular hook that makes the rule reportable,
//    and the pairing rule of 8.7.1.3 c) has to stay outside that split because a
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
#include "IccTagDict.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

/* A SKIPPED fixture must not report as PASSED.  These tests open generated .icc
 * under Testing/HDR; when one will not open they print SKIP and carry on, and
 * main() used to return g_failures - which is 0 - so ctest recorded a green
 * PASS for a run that asserted nothing.  Measured from a directory with no
 * fixtures, three of these exited 0 having skipped 19, 7 and 2 checks.
 * g_skips lets main() return 77 instead, which SKIP_RETURN_CODE turns into a
 * ctest Skipped result.  A real failure still wins: 77 is only returned when
 * nothing failed. */
int g_skips = 0;


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
  if (!pProfile) {
    g_skips++;
    printf("SKIP: cannot open %s (run Testing/CreateAllProfiles.sh)\n", path.c_str());
  }

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
  // THE FIXTURE MOVED, because icGetProfilePrimaries() now has only one kind
  // of caller left.  It reads a profile's matrix column tags, and after the
  // 23-09-2026 revision no HDR ColorSpace Profile has any - 8.7 defines none
  // for the ColorSpace class.  What still uses the procedure is the ICC
  // dictType Metadata Registry, whose MDCV and CLL entries define their own
  // primaries code 2 as "the containing profile's matrix column tags", and
  // that rule is the registry's and untouched by this amendment.  So the
  // fixtures under test here are the two that are NOT HDR ColorSpace Profiles
  // and do carry colorants: HdrDisplayMetadata ('mntr') and
  // HdrInputDisplayMeta ('scnr').
  //
  // Both carry a chromaticAdaptationTag, so the unadapted half is synthesized
  // by deleting it in memory.  That is better than a fixture kept solely to
  // have none: the two halves then differ in exactly one tag, which is the
  // thing the adaptation direction is being tested against.
  CIccProfile *pProfile = openFixture("HdrDisplayMetadata.icc");
  if (!pProfile)
    return;

  icCicpPrimaries p;

  check(pProfile->DeleteTag(icSigChromaticAdaptationTag),
        "the chromaticAdaptationTag is removed to synthesize the unadapted case");
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
  // The same fixture read afresh, this time WITH its Bradford
  // chromaticAdaptationTag, so recovering the source primaries means applying
  // that matrix inverted. Done correctly the result is the BT.709 primaries
  // at D65 - the H.273 Table 2 entry for value 1. Skipping the inverse leaves
  // the D50 values above, which differ by roughly 0.033 in white x: far
  // outside these tolerances but entirely plausible-looking on its own, which
  // is why this assertion exists.
  CIccProfile *pAdapted = openFixture("HdrDisplayMetadata.icc");
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

    // THE INFO STRUCT NO LONGER FOLLOWS.  icGetProfilePrimaries() above still
    // recovers the primaries from the matrix column tags, because the ICC
    // dictType Metadata Registry's MDCV and CLL entries define their own
    // primaries code 2 that way and that rule is untouched.  What changed is
    // that cicpTag.ColourPrimaries 2 no longer routes here: 8.7.1.1 sends it
    // to the cicpType custom chromaticity extension of 10.3, which this build
    // cannot read.  The two used to be the same procedure and are now
    // different questions, so the assertion is that they DISAGREE.
    CIccTag *pCicp = pAdapted->FindTag(icSigCicpTag);
    if (pCicp && pCicp->GetType() == icSigCicpType) {
      icUInt8Number cp, tc, mc, fr;
      ((CIccTagCicp*)pCicp)->GetFields(cp, tc, mc, fr);
      ((CIccTagCicp*)pCicp)->SetFields(icCicpPrimariesUnspecified, tc, mc, fr);

      icHdrProfileInfo adaptedInfo;
      check(icGetHdrProfileInfo(pAdapted, adaptedInfo), "info resolved for the adapted profile");
      check(!adaptedInfo.bPrimariesResolved,
            "cicpTag ColourPrimaries 2 resolves nothing in the info struct, even though "
            "icGetProfilePrimaries() recovers from the very same tags for the registry");
    }

    delete pAdapted;
  }

  // icGetResolvedPrimaries() must REFUSE value 2 now.  It used to route it to
  // the profile's matrix column tags and set bFromProfile; 8.7.1.1 routes it
  // to the cicpType extension of 10.3 instead, the out-parameter is gone with
  // the second path, and this build cannot read the extension.  Refusing is
  // the only honest answer, and it is what keeps a substitute matrix out of
  // the rendering chain.
  check(!icGetResolvedPrimaries(pProfile, 2, p),
        "value 2 resolves nothing: 8.7.1.1 wants the 10.3 extension, unreadable here");
  // The discriminator for that assertion: this profile DOES have the matrix
  // column tags the old path read, so a build that still routed value 2 to
  // them would resolve here and pass a weaker test.
  check(icGetProfilePrimaries(pProfile, p),
        "and the tags the old path would have used are present, so the refusal is the rule "
        "and not a missing tag");

  // Any other value must still come from the table, for the same profile.
  check(icGetResolvedPrimaries(pProfile, 9, p),
        "value 9 resolves through the H.273 table");
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

  // THE HDR DISPLAY HALF OF THIS TEST IS GONE.  It asserted the DCV, DRWL and
  // DERH shapes and the four-rule display-headroom precedence of clause
  // 8.10.5, and the 23-09-2026 revision deletes that clause outright:
  // "Physical display characterization ... is intentionally out of scope for
  // the HDR ColorSpace Profile sub-class defined by this amendment, which
  // characterizes a colour encoding rather than a display."  The reader no
  // longer parses the three entries and no longer resolves a display
  // headroom, so there is nothing to assert - and asserting the old
  // precedence would pin behaviour no clause backs.
  //
  // The fixtures went with it: HdrHeadroomDcvDrwl and HdrHeadroomDcvCrwl
  // existed only to isolate rules b) and c).
  check(!meta.HasUnparsedEntries(), "every entry in the fixture parsed");

  delete pProfile;
}

// ---------------------------------------------------------------------------
// 3b. Building matrices from chromaticities
// ---------------------------------------------------------------------------
//
// These exist for SMPTE ST 2094-50 Annex A's gain application colour space,
// and clause 8.7.1.1 NOTE 2's forward matrix will need the same construction.
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
// 3c. Clause 8.7.1.2 c)'s forward matrix, built from the cicpTag (HDR-07)
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
// 4. The content-headroom priority order of clause 8.7.1.4 (Linear transfer)
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
          "the other reading of 8.7.1.4's default is not the one taken");
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
// Defined below, next to the registry-value tests that were its first caller.
CIccTagDict *metaDict(CIccProfile *pProfile);

void testClassification()
{
  icHdrProfileInfo info;

  // A conforming HDR ColorSpace Profile: RGB, ColorSpace class, PCSXYZ, cicp
  // with TransferCharacteristics 16.  THE BASE FIXTURE MOVED with the
  // sub-class: HdrDisplayMetadata is 'mntr' and is now the class NEGATIVE
  // (below), and HdrColorSpaceClass - which was the class negative under
  // 8.10.1 - is the positive.
  CIccProfile *pProfile = openFixture("HdrColorSpaceClass.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved");
    check(info.nClass == icHdrProfileConforming,
          "the ColorSpace-class fixture is a conforming HDR ColorSpace Profile");
    check(info.bRgbColorSpace, "recognised as an RGB ColorSpace-class profile");
    check(info.bPcsXyz, "PCSXYZ, which the matrix of 8.7.1.2 c) produces");
    check(info.bVersion4, "a version 4 profile, so an ICC.1 clause applies to it");
    check(info.bHasCicp && info.bTransferIsHdr, "cicp present with an HDR transfer");
    check(!info.bHasHagc, "no HAGC tag, which 8.7.1.1 NOTE 3 makes optional");
    check(info.bHasAToB0 && info.bHasBToA0,
          "and the AToB0/BToA0 pair that 8.7 requires of every ColorSpace profile");
    check(info.bPrimariesResolved,
          "primaries resolved from the H.273 table, since ColourPrimaries is not 2");
    delete pProfile;
  }

  // THE CLASS NEGATIVES.  A Display or Input profile carrying the very same
  // cicpTag is not an HDR ColorSpace Profile - the amendment says so in as
  // many words: such a profile "is not, however, an HDR ColorSpace Profile
  // under clause 8.7.1".  Both classes are asserted, so a classifier that
  // stopped testing the class would have to fail one of them; testing only
  // one would let the other through.
  {
    static const struct { const char *szFixture; const char *szWhat; } kClassNegatives[] = {
      { "HdrDisplayMetadata.icc",  "a Display-class ('mntr') profile" },
      { "HdrInputDisplayMeta.icc", "an Input-class ('scnr') profile" },
    };

    for (size_t i = 0; i < sizeof(kClassNegatives) / sizeof(kClassNegatives[0]); i++) {
      CIccProfile *pNeg = openFixture(kClassNegatives[i].szFixture);

      if (!pNeg)
        continue;

      check(icGetHdrProfileInfo(pNeg, info), "info resolved for the class negative");
      check(!info.bRgbColorSpace, kClassNegatives[i].szWhat);
      check(info.nClass == icHdrProfileHdrContent,
            "carries HDR content and is NOT an HDR ColorSpace Profile");
      check(info.bHasCicp && info.bTransferIsHdr,
            "and it is only the class that excludes it - every other condition is met");

      // Not a defect.  Failing a definitional condition makes a profile a
      // non-member, not a broken member, and nothing may be reported.
      std::string report;
      check(pNeg->Validate(report) < icValidateWarning,
            "a class negative is a perfectly valid profile");

      delete pNeg;
    }
  }

  // The HAGC tag's own HDR reference white takes precedence over a CRWL entry,
  // because the gain curve in that same tag was authored against it.
  pProfile = openFixture("HagcDisplay.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the HAGC fixture");
    check(info.nClass == icHdrProfileConforming, "HAGC fixture is a conforming HDR ColorSpace Profile");
    check(info.bHasHagc, "HAGC tag seen");
    check(info.bContentReferenceWhiteFromProfile, "reference white came from the profile");
    checkClose(info.contentReferenceWhite, 300.0, 1e-3,
               "reference white is the HAGC tag's custom value, not the 203 default");
    delete pProfile;
  }

  // TransferCharacteristics 13 with a HAGC tag present.  Clause 8.7.1.1's
  // conditions are definitional: this profile is simply not an HDR ColorSpace Profile.  It
  // is a valid ICC Display profile carrying two legal optional tags, so nothing
  // may be reported against it.  The classification is descriptive only.
  pProfile = openFixture("HdrInvalidTransfer.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the sRGB-transfer fixture");
    check(info.nClass == icHdrProfileHdrContent,
          "TransferCharacteristics 13 is HDR-related content, not an HDR ColorSpace Profile");
    check(info.bHasCicp, "cicp present");
    check(!info.bTransferIsHdr, "TransferCharacteristics 13 is not one of 8, 16 or 18");
    check(info.nTransferCharacteristics == 13, "the value is reported as-is");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv < icValidateWarning,
          "not being an HDR ColorSpace Profile is not a defect and draws no diagnostic");
    check(report.find("TransferCharacteristics") == std::string::npos,
          "no message is emitted about a membership condition");
    check(report.find("HDR:") == std::string::npos,
          "clause 8.7.1 says nothing about a profile outside the sub-class");
    delete pProfile;
  }

  // An HDR ColorSpace Profile can still break the pairing rule of 8.7.1.5, which is the one
  // requirement of clause 8.7.1 that a member of the sub-class can actually fail.
  pProfile = openFixture("HdrMissingBToA0.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the unpaired fixture");
    check(info.nClass == icHdrProfileConforming,
          "the unpaired profile is still a conforming HDR ColorSpace Profile");
    check(info.bHasAToB0 && !info.bHasBToA0, "AToB0 present, BToA0 absent");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "the unpaired tag validates non-compliant");
    check(report.find("without its paired BToA0Tag") != std::string::npos,
          "the report names the missing tag");
    // THE SEPARATE "HDR: BToA0Tag missing" MESSAGE IS GONE, and its absence
    // is asserted rather than left untested.  It stated a requirement 8.10.6
    // made of Display-class profiles on top of the parent class's; 8.7.1.5
    // defers the pair to 8.7 instead - "both already unconditionally required
    // of every ColorSpace profile by 8.7" - and CheckRequiredTags() raises
    // that as a CRITICAL error for the ColorSpace class.  Emitting both would
    // report one defect twice, at two severities.
    check(report.find("HDR: BToA0Tag missing") == std::string::npos,
          "and does NOT repeat it as a separate clause-8.7.1.5 requirement");
    check(report.find("Critical tag(s) missing") != std::string::npos,
          "because 8.7's own unconditional requirement has already raised it critically");
    delete pProfile;
  }

  // Clause 8.7.1.5's backward-compatibility pair, AND WHERE IT IS NOW
  // ENFORCED.  Under 8.10.6 the pair was an ADDITIONAL requirement the parent
  // class did not make - the Input and Display branches of
  // CheckRequiredTags() accept the LUT pair OR the six matrix/TRC tags, so a
  // profile satisfying one alternative was never asked about the other, and
  // CheckHdrProfile() was the only thing that asked.  8.7.1.5 defers to 8.7,
  // whose ColorSpace branch requires both tags outright and raises a CRITICAL
  // error.  The requirement did not weaken; it moved into the parent, where
  // it is checked harder.
  pProfile = openFixture("HdrMissingLutPair.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the pairless fixture");

    // Membership first, because the diagnostic depends on it: a profile the
    // classifier rejected would be judged by some other class's rules and this
    // requirement would never be reached.
    check(info.nClass == icHdrProfileConforming,
          "a profile missing the pair is still an HDR ColorSpace Profile");
    check(!info.bHasAToB0 && !info.bHasBToA0, "and carries neither tag");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateCriticalError,
          "a ColorSpace profile with neither tag is CRITICALLY invalid, not merely non-compliant");
    check(report.find("Critical tag(s) missing") != std::string::npos,
          "and 8.7's unconditional requirement is what reports it");
    // Neither HDR-specific message fires any more, and both absences are
    // asserted: leaving them untested is how a duplicated diagnostic survives
    // a refactor.
    check(report.find("HDR: AToB0Tag missing") == std::string::npos,
          "CheckHdrProfile() does not repeat the AToB0Tag requirement");
    check(report.find("HDR: BToA0Tag missing") == std::string::npos,
          "nor the BToA0Tag one");
    delete pProfile;
  }

  // The HAGC tag's version gate.  It was copied from the cicpTag's, which
  // refuses only version 5.0.0.0 exactly, so a 5.1 profile carried the tag
  // and validated clean.
  pProfile = openFixture("HagcDisplay.icc");
  if (pProfile) {
    pProfile->m_Header.version = 0x05100000;

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "a version 5.1 profile carrying a HAGC tag is non-compliant");
    check(report.find("headroomAdaptiveGainCurveType: Invalid tag type") != std::string::npos,
          "and the report names the HAGC tag type");
    delete pProfile;
  }

  // ColourPrimaries 2.  Under 8.10.1 the matrix column tags were required in
  // this case and supplied the matrix directly; 8.7.1.1 routes it to the
  // cicpType custom chromaticity extension of 10.3 instead, taking both the
  // chromaticities and the white point from there rather than from
  // mediaWhitePointTag.  A ColorSpace profile has no matrix column tags to
  // fall back on, and this build cannot read the extension - ICC.1:2022 10.3
  // Table 32 stops at twelve bytes - so the case is refused outright.
  //
  // HdrCicp2NoColumns is DELETED.  Its whole subject was the presence or
  // absence of the matrix column tags, which no longer decides anything, so
  // it and HdrCicpUnspecified had become the same test.
  pProfile = openFixture("HdrCicpUnspecified.icc");
  if (pProfile) {
    check(icGetHdrProfileInfo(pProfile, info), "info resolved for the ColourPrimaries 2 fixture");
    // MEMBERSHIP AND CONFORMANCE ARE DIFFERENT AXES, and this is the fixture
    // that keeps them apart.  The amendment calls a ColourPrimaries 2 profile
    // without the extension "non-conforming" - a broken member, like a
    // profile missing a required tag - not a non-member.
    check(info.nClass == icHdrProfileConforming,
          "ColourPrimaries 2 does not cost membership: the amendment calls such a "
          "profile non-conforming, not a non-member");
    check(info.nColourPrimaries == 2, "the fixture declares ColourPrimaries 2");
    check(!info.bPrimariesResolved, "and no primaries resolve from it");

    icFloatNumber m[9];
    check(!icBuildHdrForwardMatrix(pProfile, info.nColourPrimaries, m),
          "no matrix can be derived for ColourPrimaries 2");
    check(icHdrSelectForwardMatrix(pProfile, info.nColourPrimaries, m) == icHdrMatrixNeedsCicpExt,
          "and the matrix selector names the missing 10.3 extension as the reason");

    std::string report;
    icValidateStatus rv = pProfile->Validate(report);
    check(rv >= icValidateNonCompliant, "the unreadable primaries are reported");
    check(report.find("ColourPrimaries is 2") != std::string::npos,
          "the report names the condition");
    check(report.find("10.3") != std::string::npos,
          "and names the extension clause, so a reader whose profile HAS one can tell "
          "the finding is this build's limitation rather than their file's defect");
    delete pProfile;
  }

  // PROPOSAL-ISSUE HDR-09 IS RESOLVED BY THE REVISION, in the direction this
  // tree had already ruled.  Under 8.10.x the pairing rule was stated twice -
  // unscoped in informative 8.10.3 c), and confined to the Display class in
  // normative 8.10.6 - and the ruling here followed 8.10.6 because an
  // informative clause cannot impose a requirement.  8.7.1.5 states it once,
  // normatively, with no class condition: "When an HDR ColorSpace Profile
  // contains an AToBxTag, the corresponding BToAxTag shall also be present."
  // There is only one class now, so the scope question dissolves.
  //
  // The assertion is therefore the inverse of the old one: the diagnostic must
  // NOT be scoped to Display.
  pProfile = openFixture("HdrMissingBToA0.icc");
  if (pProfile) {
    std::string report;
    pProfile->Validate(report);
    check(report.find("without its paired BToA0Tag") != std::string::npos,
          "the pairing rule of 8.7.1.5 fires");
    check(report.find("Display HDR ColorSpace Profile") == std::string::npos,
          "and is no longer scoped to the Display class");
    // The pair is ALSO a required tag of every ColorSpace profile by 8.7, so
    // CheckRequiredTags() raises it critically.  That is the requirement
    // moving into the parent class, not a second defect.
    check(report.find("Critical tag(s) missing") != std::string::npos,
          "and 8.7's own unconditional requirement is raised critically");
    delete pProfile;
  }

  // A metadata primaries code of 2 resolves against the containing profile,
  // not to "unknown". The registry says so for CLL and CCV -- the primaries
  // are "defined by tags required by [a] three component matrix-based display
  // profile containing this metadata" -- and the ICC HDR Display registration
  // of 2026-06-24 says of DCV that a value of 2 "has the same meaning as in
  // the cicpTag", which under the CICP amendment is the same recovery.
  //
  // THE PROFILE THAT CAN SATISFY IT IS NO LONGER AN HDR ColorSpace Profile.
  // The registry rule reads the containing profile's matrix column tags, and
  // an HDR ColorSpace Profile is a ColorSpace profile (8.7), which has none -
  // this is PROPOSAL-ISSUE HDR-18, and the 23-09-2026 revision turns it from
  // a corner case into the normal one.  So the pair of assertions is:
  //
  //   HdrDisplayMetadata ('mntr', carries colorant tags)  -> code 2 RESOLVES
  //   HdrColorSpaceClass ('spac', has no colorant tags)   -> code 2 does NOT
  //
  // Both halves matter.  The first keeps the registry rule itself under test,
  // so it cannot quietly stop working; the second pins that an entry the
  // reader cannot resolve is reported unresolved rather than guessed at.
  pProfile = openFixture("HdrDisplayMetadata.icc");
  if (pProfile) {
    CIccTagDict *pDict = metaDict(pProfile);
    check(pDict != NULL, "HdrDisplayMetadata carries a metadataTag dict");
    if (pDict) {
      pDict->Set("CLL", "1000.0 400.0 2");
      CIccHdrMetadataReader meta2;
      check(meta2.Read(pProfile), "metadataTag read for the matrix-based profile");
      check(meta2.HasContentLightLevel(), "CLL present");
      check(meta2.ContentLightLevelPrimariesResolved(),
            "a CLL primaries code of 2 resolves against a profile that HAS matrix column tags");
      checkClose(meta2.GetContentLightLevelPrimaries().xRed, 0.6400, 1e-3,
                 "and gives the profile's red primary, not nothing");
    }
    delete pProfile;
  }

  pProfile = openFixture("HdrColorSpaceClass.icc");
  if (pProfile) {
    CIccTagDict *pDict = metaDict(pProfile);
    check(pDict != NULL, "HdrColorSpaceClass carries a metadataTag dict");
    if (pDict) {
      pDict->Set("CLL", "1000.0 400.0 2");
      CIccHdrMetadataReader meta2;
      check(meta2.Read(pProfile), "metadataTag read for the HDR ColorSpace Profile");
      check(meta2.HasContentLightLevel(), "CLL present");
      check(!meta2.ContentLightLevelPrimariesResolved(),
            "HDR-18: a CLL primaries code of 2 resolves NOTHING in an HDR ColorSpace "
            "Profile, which by 8.7 has no matrix column tags for the registry rule to read");
    }
    delete pProfile;
  }

  // The discriminator for both halves: value 2 has no entry in H.273 Table 2
  // at all, so a reader that looked it up there rather than in the profile
  // would report nothing resolved in BOTH cases and pass the second half for
  // entirely the wrong reason.
  {
    icCicpPrimaries table;
    check(!icGetCicpPrimaries(2, table),
          "H.273 Table 2 has no chromaticities for value 2, which is the point");
  }

  // THE HDR-DISPLAY-METADATA-OUT-OF-SCOPE NOTE IS GONE.  It reported HDR
  // Display entries carried by a profile that was not 'mntr', on the strength
  // of clause 8.10.5 opening "An HDR Profile of the Display class ('mntr')
  // may convey HDR display metadata".  The 23-09-2026 revision deletes 8.10.5
  // altogether, so there is no clause left to be outside of, the reader no
  // longer parses DERH, DCV or DRWL at all, and a profile carrying them draws
  // nothing.  Asserting the old note here would pin a diagnostic that no
  // clause supports.
  //
  // HdrInputDisplayMeta survives as the Input-class membership negative, and
  // is asserted with its Display-class twin further up.

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
    g_skips++;
    printf("SKIP: Testing/V2/v2RgbMatrixTRC.icc absent\n");
  }
}


// ---------------------------------------------------------------------------
// 8. Values the dictType Metadata Registry defines and the amendment does not
// ---------------------------------------------------------------------------
//
// Found 2026-09-10 by reading the registry entries rather than the amendment,
// which delegates "names, encodings and semantics" to the registry. Each case
// edits one metadataTag entry of an existing fixture IN MEMORY, so the corpus
// and its manifests are untouched, and each asserts a value that differed
// before the fix - so none of them passes against the unfixed build.

CIccTagDict *metaDict(CIccProfile *pProfile)
{
  CIccTag *pTag = pProfile->FindTag(icSigMetaDataTag);
  if (!pTag || pTag->GetType() != icSigDictType)
    return NULL;
  return (CIccTagDict*)pTag;
}

void testRegistryDefinedValues()
{
  // (a) "Value of 0.0 means that the respective value is unknown" - CLL/MDCV.
  // HdrLinearHagcWhite is Linear, HAGC reference white 300, CLL 600 -> 2.0.
  // An unknown CLL maximum is not a peak: resolution must fall through, not
  // report 0 - which is below every target and switched the target clamp off.
  CIccProfile *pLin = openFixture("HdrLinearHagcWhite.icc");
  if (pLin) {
    CIccTagDict *pDict = metaDict(pLin);
    check(pDict != NULL, "HdrLinearHagcWhite carries a metadataTag dict");
    if (pDict) {
      icHdrProfileInfo info;
      pDict->Set("CLL", "0.0 0.0 9");
      check(icGetHdrProfileInfo(pLin, info), "info with an unknown CLL maximum");
      check(info.nContentHeadroomSource == icHdrContentHeadroomDefault,
            "an unknown (0.0) CLL maximum falls through to 8.7.1.4 c), not rule a)");
      checkClose(info.contentHeadroom, 1000.0 / 300.0, 1e-4,
                 "and Hcontent is the 1000 cd/m^2 default over the HAGC white, not 0");

      pDict->Set("MDCV", "1000.0 0.005 9");
      check(icGetHdrProfileInfo(pLin, info), "info with unknown CLL and a known MDCV");
      check(info.nContentHeadroomSource == icHdrContentHeadroomMdcv,
            "an unknown CLL falls through to MDCV when MDCV has a peak");

      pDict->Set("MDCV", "0.0 0.0 9");
      check(icGetHdrProfileInfo(pLin, info), "info with CLL and MDCV both unknown");
      check(info.nContentHeadroomSource == icHdrContentHeadroomDefault,
            "an unknown MDCV maximum does not select rule b) either");
    }
    delete pLin;
  }

  // (b) AND (c) WERE ABOUT DCV, and are gone with clause 8.10.5.  (b) checked
  // that a DCV maximum of 0.0 - "unknown" per the registration - fired
  // neither rule b) nor rule c); (c) checked that the content and display
  // axes divided by the SAME reference white, which was a real defect once
  // (one PAWG report named both divisors "content HDR reference white" and
  // gave them different values).  Neither has a subject now: the reader does
  // not parse DCV and there is no display axis to cross.
  //
  // The half of (c) that survives - that the HAGC tag's reference white beats
  // a CRWL entry - is the PROPOSAL-ISSUE HDR-10 ruling, and it is still
  // covered by HdrLinearHagcCrwlDisagree in the corpus manifest and by the
  // reference-white assertions in testClassification().

  // (d) Primaries code 2 means different things per entry. CLL and CCV: "the
  // primaries are defined by tags required by a three-component matrix-based
  // display profile". MDCV: "reserved for future use".
  //
  // THE FIXTURE MOVED for the reason given at testProfilePrimaries(): the
  // registry rule reads the containing profile's matrix column tags, and no
  // HDR ColorSpace Profile has any.  HdrDisplayMetadata is 'mntr', carries
  // them, and is therefore the shape where CLL's code 2 can resolve at all -
  // so it is where the CLL-versus-MDCV distinction can still be seen.
  CIccProfile *pP = openFixture("HdrDisplayMetadata.icc");
  if (pP) {
    CIccTagDict *pDict = metaDict(pP);
    if (pDict) {
      pDict->Set("CLL", "1000.0 400.0 2");
      pDict->Set("MDCV", "1000.0 0.005 2");
      CIccHdrMetadataReader meta;
      check(meta.Read(pP), "metadata reads");
      check(meta.ContentLightLevelPrimariesResolved(),
            "CLL primaries 2 resolves against the colorant tags (registry, CLL entry)");
      check(meta.HasMasteringDisplayColourVolume(), "MDCV with primaries 2 still parses - 2 is a legal 8-bit code");
      check(!meta.MasteringPrimariesResolved(),
            "MDCV primaries 2 is reserved and stays unresolved (registry, MDCV entry)");
    }
    delete pP;
  }
}

} // namespace

// ---------------------------------------------------------------------------
// 9. Validate() loads no tag for a profile whose header rules out membership
// ---------------------------------------------------------------------------
// CIccProfile::Validate() is const, but CheckHdrProfile() classifies through
// icHdrFindTag(), which loads a tag the profile has not loaded yet.  For a
// profile that was OPENED rather than read, that made validating ANY profile
// load its cicpTag, metadataTag and chromaticAdaptationTag - growing its tag
// list and moving its attached IO - including profiles that clause 8.7.1.1
// excludes on the header alone.  The header conditions (version, class,
// colour space) now gate the classification, so those profiles are left as
// they were opened.  The HDR ColorSpace Profile that follows is the control: gating must
// not also stop an opened member from being held to 8.7.1.5.
void testValidateLoadsNoTagForHeaderNonMembers()
{
  // HdrVersion44 USED TO BE ONE OF THESE, on the strength of the 4.5.0.0
  // lower bound that 4.7 of the 23-09-2026 revision withdraws.  It is a
  // member now, so it belongs to the control below rather than here; putting
  // it back would assert that a member's tags are never loaded, which is the
  // opposite of what this test exists to protect.
  //
  // The two that remain rule membership out from the HEADER alone, which is
  // what icHdrHeaderAdmitsMembership() tests: the data colour space is not
  // RGB, and the device class is not ColorSpace.
  const char *szNonMembers[] = { "HdrNonRgbSpace.icc", "HdrDisplayMetadata.icc" };

  for (size_t n = 0; n < sizeof(szNonMembers) / sizeof(szNonMembers[0]); n++) {
    std::string path = "Testing/HDR/";
    path += szNonMembers[n];

    CIccProfile *pOpened = OpenIccProfile(path.c_str());
    if (!pOpened) {
      g_skips++;
      printf("SKIP: cannot open %s (run Testing/CreateAllProfiles.sh)\n", path.c_str());
      continue;
    }

    std::string what = szNonMembers[n];
    check(pOpened->IsTagPresent(icSigCicpTag),
          (what + " carries a cicpTag, so there is something to load").c_str());
    check(pOpened->FindTagConst(icSigCicpTag) == NULL,
          (what + " opened: its cicpTag is not loaded yet").c_str());

    std::string report;
    pOpened->Validate(report);

    check(pOpened->FindTagConst(icSigCicpTag) == NULL,
          (what + ": Validate() leaves its cicpTag unloaded").c_str());

    delete pOpened;
  }

  CIccProfile *pMember = OpenIccProfile("Testing/HDR/HdrMissingBToA0.icc");
  if (!pMember) {
    g_skips++;
    printf("SKIP: cannot open Testing/HDR/HdrMissingBToA0.icc (run Testing/CreateAllProfiles.sh)\n");
    return;
  }

  std::string report;
  pMember->Validate(report);
  check(report.find("clause 8.7.1.5") != std::string::npos,
        "an opened HDR ColorSpace Profile is still held to clause 8.7.1.5 by Validate()");

  delete pMember;
}

namespace {

// ---------------------------------------------------------------------------
// 10. What a metadata value has to be before it is taken as stated
// ---------------------------------------------------------------------------
void testMetadataParsing()
{
  struct Case {
    const char *szKey;
    const char *szValue;
    bool bParses;
    const char *szWhat;
  };

  // CRWL divides 10 000 cd/m^2 (the PQ peak) and every metadata luminance, so
  // "positive" is not enough: 1e-50 is 0.0 as a float, 1e-40 is subnormal and
  // 1e-36 leaves 10 000 over it outside the float range.
  const Case cases[] = {
    { "CRWL", "203.0",          true,  "CRWL as a plain decimal" },
    { "CRWL", " 203.0 ",        true,  "CRWL with surrounding separators" },
    { "CRWL", "+203",           true,  "CRWL in signed integer form" },
    { "CRWL", "1e-50",          false, "a CRWL that is 0.0 as a float" },
    { "CRWL", "1e-40",          false, "a subnormal CRWL" },
    { "CRWL", "1e-36",          false, "a CRWL 10 000 cd/m^2 cannot be divided by" },
    { "CRWL", "abc",            false, "a CRWL that is text" },
    { "CRWL", "203.0x",         false, "a CRWL with trailing text" },
    { "CRWL", "inf",            false, "an infinite CRWL" },
    { "CRWL", "nan",            false, "a NaN CRWL" },
    { "CRWL", "203,0",          false, "a comma-decimal CRWL (two fields, not one)" },
    { "CLL",  "1000.0.005 9",   false, "glued numbers, which strtod read as three" },
    { "CLL",  "1000.0 400.0 9", true,  "the registered CLL shape" },
    // The DERH and DRWL subnormal rows are gone with clause 8.10.5: the
    // reader no longer parses either key, so "is it taken as stated" has no
    // answer to assert.  MDCV keeps the multi-field shape under test.
    { "MDCV", "1000.0 0.005 9", true,  "the registered MDCV shape" },
    { "MDCV", "1e-40",          false, "an MDCV with too few fields" },
  };

  for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
    CIccProfile *pProfile = openFixture("HdrDisplayMetadata.icc");
    if (!pProfile)
      return;

    CIccTagDict *pDict = metaDict(pProfile);
    check(pDict != NULL, "HdrDisplayMetadata carries a metadataTag dict");
    if (!pDict) {
      delete pProfile;
      return;
    }

    pDict->Set(cases[n].szKey, cases[n].szValue);

    CIccHdrMetadataReader meta;
    check(meta.Read(pProfile), "metadataTag read");

    bool bHas = false;
    if (!strcmp(cases[n].szKey, "CRWL"))
      bHas = meta.HasContentReferenceWhite();
    else if (!strcmp(cases[n].szKey, "CLL"))
      bHas = meta.HasContentLightLevel();
    else if (!strcmp(cases[n].szKey, "MDCV"))
      bHas = meta.HasMasteringDisplayColourVolume();

    std::string what = cases[n].szWhat;
    check(bHas == cases[n].bParses,
          (what + (cases[n].bParses ? " is taken as stated" : " is not taken as stated")).c_str());
    check(meta.HasUnparsedEntries() == !cases[n].bParses,
          (what + (cases[n].bParses ? " leaves nothing unparsed" : " is reported unparsed")).c_str());

    if (!strcmp(cases[n].szKey, "CRWL") && !cases[n].bParses)
      checkClose(meta.GetResolvedContentReferenceWhite(), 203.0, 0.0,
                 (what + " resolves to the 203 default, not to its own value").c_str());

    delete pProfile;
  }

  // A profile whose only HDR entry did not parse still carries one.
  CIccProfile *pProfile = openFixture("HdrDisplayMetadata.icc");
  if (pProfile) {
    CIccTagDict *pDict = metaDict(pProfile);
    if (pDict) {
      pDict->Remove("CLL");
      pDict->Remove("MDCV");
      pDict->Remove("DERH");
      pDict->Remove("DRWL");
      pDict->Remove("DCV");
      pDict->Set("CRWL", "abc");

      CIccHdrMetadataReader meta;
      check(meta.Read(pProfile), "metadataTag with one unparsed entry read");
      check(!meta.HasContentReferenceWhite() && meta.HasAnyHdrEntry(),
            "an HDR entry that did not parse still counts as an HDR entry");
    }
    delete pProfile;
  }

  // The same values in a host that set a comma-decimal locale.  strtod follows
  // it; a registry value is not locale text.
  std::string savedLocale;
  const char *szLocale = setlocale(LC_NUMERIC, NULL);
  savedLocale = szLocale ? szLocale : "C";

  const char *szCommaLocales[] = { "de_DE.UTF-8", "de_DE.utf8", "de_DE", "fr_FR.UTF-8", "fr_FR" };
  bool bSwitched = false;

  for (size_t n = 0; n < sizeof(szCommaLocales) / sizeof(szCommaLocales[0]) && !bSwitched; n++)
    bSwitched = setlocale(LC_NUMERIC, szCommaLocales[n]) != NULL;

  if (bSwitched) {
    pProfile = openFixture("HdrLinearMdcv.icc");
    if (pProfile) {
      icHdrProfileInfo info;
      check(icGetHdrProfileInfo(pProfile, info), "info under a comma-decimal locale");
      checkClose(info.contentReferenceWhite, 100.0, 1e-4,
                 "the stated CRWL of 100.0 is read under a comma-decimal locale");
      delete pProfile;
    }
    setlocale(LC_NUMERIC, savedLocale.c_str());
  }
  else {
    printf("NOTE: no comma-decimal locale is installed; the locale case was not exercised\n");
  }
}

// A rule that is selected but whose quotient is not a finite float yields no
// value - not an infinity, and not the next rule's value.
void testUnrepresentableHeadroom()
{
  // The DCV 1e30 / DRWL 1e-30 case went with clause 8.10.5 - there is no
  // display headroom to overflow.  The content axis, which has the same
  // arithmetic and the same rule, still carries the assertion.
  CIccProfile *pProfile = openFixture("HdrLinearCll.icc");
  if (pProfile) {
    CIccTagDict *pDict = metaDict(pProfile);
    if (pDict) {
      // 10 000 over 1e-34 still fits a float, so this CRWL is accepted; CLL
      // 1e30 over it does not.
      pDict->Set("CRWL", "1e-34");
      pDict->Set("CLL", "1e30 0.0 9");

      icHdrProfileInfo info;
      check(icGetHdrProfileInfo(pProfile, info), "info with CLL 1e30 over CRWL 1e-34");
      check(info.bContentReferenceWhiteFromProfile, "a CRWL of 1e-34 is taken as stated");
      check(info.nContentHeadroomSource == icHdrContentHeadroomNone,
            "8.7.1.4 a) with an unrepresentable quotient produces no content headroom, not MDCV's");
      check(std::isfinite((double)info.contentHeadroom), "and leaves no infinity behind");
    }
    delete pProfile;
  }
}

// ---------------------------------------------------------------------------
// 11. Membership needs PCSXYZ; a malformed chad is not an absent one
// ---------------------------------------------------------------------------
void testLabPcsMembership()
{
  CIccProfile *pProfile = openFixture("HagcCommonParams.icc");
  if (!pProfile)
    return;

  icHdrProfileInfo info;
  check(icHdrHeaderAdmitsMembership(pProfile), "the XYZ-PCS fixture's header admits membership");
  check(icGetHdrProfileInfo(pProfile, info) && info.bPcsXyz &&
        info.nClass == icHdrProfileConforming, "and it is a conforming HDR ColorSpace Profile");

  pProfile->m_Header.pcs = icSigLabData;
  check(!icHdrHeaderAdmitsMembership(pProfile), "a Lab PCS header does not admit membership");
  check(icGetHdrProfileInfo(pProfile, info) && !info.bPcsXyz, "bPcsXyz reports the Lab PCS");
  check(info.nClass == icHdrProfileHdrContent,
        "the Lab-PCS profile is HDR-related content, not an HDR ColorSpace Profile");

  delete pProfile;
}

void testMalformedChad()
{
  CIccProfile *pProfile = openFixture("HdrLinearNoMetadata.icc");
  if (!pProfile)
    return;

  icFloatNumber m[9];

  check(!icHdrHasMalformedChad(pProfile), "a well-formed chromaticAdaptationTag is not malformed");
  check(icBuildHdrForwardMatrix(pProfile, 1, m), "and the forward matrix builds");
  const icFloatNumber adaptedX = m[0];

  CIccTagS15Fixed16 *pShort = new CIccTagS15Fixed16(8);
  for (icUInt32Number i = 0; i < 8; i++)
    (*pShort)[i] = icDtoF((i == 0 || i == 4) ? 1.0 : 0.0);
  pProfile->DeleteTag(icSigChromaticAdaptationTag);
  pProfile->AttachTag(icSigChromaticAdaptationTag, pShort);

  check(icHdrHasMalformedChad(pProfile), "an eight-value chromaticAdaptationTag is malformed");
  check(!icBuildHdrForwardMatrix(pProfile, 1, m),
        "and the forward matrix is refused rather than built unadapted");

  pProfile->DeleteTag(icSigChromaticAdaptationTag);
  pProfile->AttachTag(icSigChromaticAdaptationTag, new CIccTagXYZ());

  check(icHdrHasMalformedChad(pProfile), "a chromaticAdaptationTag of the wrong type is malformed");
  check(!icBuildHdrForwardMatrix(pProfile, 1, m), "and refused likewise");

  pProfile->DeleteTag(icSigChromaticAdaptationTag);

  check(!icHdrHasMalformedChad(pProfile), "an absent chromaticAdaptationTag is not malformed");
  check(icBuildHdrForwardMatrix(pProfile, 1, m), "and builds the unadapted matrix");
  check(fabs(m[0] - adaptedX) > 0.01, "which is a different matrix from the adapted one");

  delete pProfile;
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
  testRegistryDefinedValues();
  testValidateLoadsNoTagForHeaderNonMembers();
  testMetadataParsing();
  testUnrepresentableHeadroom();
  testLabPcsMembership();
  testMalformedChad();

  if (g_failures)
    printf("hdr-profile-classification: %d assertion(s) failed\n", g_failures);
  else
    printf("hdr-profile-classification: all assertions passed\n");

  /* 77 = ctest Skipped (see SKIP_RETURN_CODE); a real failure still wins. */
  if (!g_failures && g_skips)
    return 77;

  /* Not the raw count: 77 failures would read as a ctest Skip, and a count
   * that is a multiple of 256 as a pass. */
  return g_failures ? 1 : 0;
}
