/*
 * The ICC Software License, Version 0.2
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
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// #2562.  Six rows of CIccProfile::CheckRequiredTags' v5 branch disagreed with
// ICC.2-2023 clause 8:
//
//   Input (8.3)        "one or more of" AToB0-3 or DToB0-3; only AToB0, AToB1
//                      and AToB3 were accepted.
//   Display (8.4)      one or more A-side tags AND one or more B-side tags; the
//                      B-side test was commented out, and only AToB0, AToB1
//                      and AToB3 counted as the A-side.
//   Output (8.5)       the same two bullets; either side alone was accepted.
//   xCLR output (8.5)  colorantInfoTag is "a recommended tag"; a missing
//                      colorantTableTag, which 8.5 does not name, was reported
//                      as non-compliant.
//   DeviceLink (8.6)   "one or more of AToB0Tag, DToB0Tag"; only AToB0Tag.
//   MID/MLNK/MVIS      the transform and multiplexTypeArrayTag are separate
//   (8.11-8.13)        "shall" bullets, so both are required; either was
//                      accepted, and MVIS took only MToB0 and MToS0.
//
// Each case below builds a minimal single-class v5 profile in memory and reads
// the report CheckRequiredTags produces through the public Validate().  Every
// "Critical tag" line comes from that function's class switch and nowhere else,
// so on a one-class profile a match can only be that class's own verdict.
//
// Every row that ACCEPTS something new is paired with a control that must still
// be refused, so a test that stopped reaching the check would fail its control
// rather than pass vacuously.  Some accepting cases also pass on master -- they
// are here because they pin one half of a row against a mutant that keeps the
// other half: MVIS {MToB2, mcta} is refused by a build with the AND but without
// the eight-transform set.
//
// Each AND row is tested from both sides, transform without multiplexTypeArrayTag
// and the reverse, so dropping either half of the requirement goes red.  The xCLR
// row is tested with each colorant tag alone, and a v4 profile pins the owner
// ruling that the ICC.1 path stays non-compliant.
//
// Display and Output are tested the same way, A-side alone and B-side alone.
// Input, Display and Output are also tested with a spectral PCS and a zero PCS
// field: that header used to skip these rows entirely.
//
// The grayTRCTag and matrix/TRC alternatives this function already accepted are
// left as they were and are not asserted here; ICC.2 clause 8 does not list them.

#include <cstdio>
#include <cstring>
#include <string>

#include "IccProfile.h"
#include "IccTagMPE.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

int g_fail = 0;

static void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "  [FAIL] %s\n", what);
  }
  else {
    std::fprintf(stdout, "  [PASS] %s\n", what);
  }
}

// A v5 profile of one class, with the given data and PCS colour spaces.
static CIccProfile *newProfile(icProfileClassSignature cls,
                               icColorSpaceSignature data,
                               icColorSpaceSignature pcs)
{
  CIccProfile *p = new CIccProfile;
  p->InitHeader();
  p->m_Header.version     = icVersionNumberV5;
  p->m_Header.deviceClass = cls;
  p->m_Header.colorSpace  = data;
  p->m_Header.pcs         = pcs;
  return p;
}

// The same, with a 36-channel reflectance spectral PCS and a zero PCS field.
static CIccProfile *newSpectralProfile(icProfileClassSignature cls,
                                       icColorSpaceSignature data)
{
  CIccProfile *p = newProfile(cls, data, (icColorSpaceSignature)0);
  p->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | 36);
  return p;
}

// Presence is all CheckRequiredTags tests, so an empty transform will do.
static void addMpe(CIccProfile *p, icTagSignature sig)
{
  p->AttachTag(sig, new CIccTagMultiProcessElement(3, 3));
}

static void addArray(CIccProfile *p, icTagSignature sig, icArraySignature arr)
{
  p->AttachTag(sig, new CIccTagArray(arr));
}

// CheckRequiredTags returns "No tags present." before its class switch when a
// profile has no tags at all, so a control with nothing attached would test that
// early return instead of the row.  One tag the row does not consult keeps the
// switch reachable.
static void addFiller(CIccProfile *p)
{
  p->AttachTag(icSigCopyrightTag, new CIccTagMultiProcessElement(3, 3));
}

// The matrix/TRC display model: three colorant columns and three curves.
static void addMatrixTrc(CIccProfile *p)
{
  p->AttachTag(icSigRedMatrixColumnTag, new CIccTagXYZ);
  p->AttachTag(icSigGreenMatrixColumnTag, new CIccTagXYZ);
  p->AttachTag(icSigBlueMatrixColumnTag, new CIccTagXYZ);
  p->AttachTag(icSigRedTRCTag, new CIccTagCurve);
  p->AttachTag(icSigGreenTRCTag, new CIccTagCurve);
  p->AttachTag(icSigBlueTRCTag, new CIccTagCurve);
}

static std::string report(CIccProfile *p)
{
  std::string s;
  p->Validate(s);
  delete p;
  return s;
}

static bool hasCritical(const std::string &s)
{
  return s.find("Critical tag") != std::string::npos;
}

int main()
{
  std::fprintf(stdout, "=== #2562 v5 required-tag sets ===\n");

  // ---- Input (8.3) ----
  {
    CIccProfile *p = newProfile(icSigInputClass, icSigRgbData, icSigXYZData);
    addMpe(p, icSigDToB0Tag);
    check(!hasCritical(report(p)), "Input with only DToB0Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigInputClass, icSigRgbData, icSigXYZData);
    addMpe(p, icSigAToB2Tag);
    check(!hasCritical(report(p)), "Input with only AToB2Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigInputClass, icSigGrayData, icSigXYZData);
    addMpe(p, icSigDToB3Tag);
    check(!hasCritical(report(p)), "gray Input with only DToB3Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigInputClass, icSigRgbData, icSigXYZData);
    addFiller(p);
    check(hasCritical(report(p)), "control: Input with no transform is refused");
  }
  {
    CIccProfile *p = newProfile(icSigInputClass, icSigGrayData, icSigXYZData);
    addFiller(p);
    check(hasCritical(report(p)),
          "control: gray Input with no transform and no grayTRCTag is refused");
  }

  // ---- Display (8.4) ----
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigRgbData, icSigXYZData);
    addMpe(p, icSigAToB0Tag);
    check(hasCritical(report(p)), "Display with only AToB0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigRgbData, icSigXYZData);
    addMpe(p, icSigBToA0Tag);
    check(hasCritical(report(p)), "Display with only BToA0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigRgbData, icSigXYZData);
    addMpe(p, icSigAToB2Tag);
    addMpe(p, icSigBToA2Tag);
    check(!hasCritical(report(p)), "Display with AToB2Tag and BToA2Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigRgbData, icSigXYZData);
    addMpe(p, icSigDToB1Tag);
    addMpe(p, icSigBToD3Tag);
    check(!hasCritical(report(p)), "Display with DToB1Tag and BToD3Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigGrayData, icSigXYZData);
    addMpe(p, icSigAToB0Tag);
    check(hasCritical(report(p)), "gray Display with only AToB0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigGrayData, icSigXYZData);
    addMpe(p, icSigBToA1Tag);
    check(hasCritical(report(p)), "gray Display with only BToA1Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigDisplayClass, icSigGrayData, icSigXYZData);
    addMpe(p, icSigDToB0Tag);
    addMpe(p, icSigBToD0Tag);
    check(!hasCritical(report(p)), "gray Display with DToB0Tag and BToD0Tag is accepted");
  }

  // ---- Output (8.5) ----
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSigCmykData, icSigLabData);
    addMpe(p, icSigAToB0Tag);
    check(hasCritical(report(p)), "Output with only AToB0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSigCmykData, icSigLabData);
    addMpe(p, icSigBToA0Tag);
    check(hasCritical(report(p)), "Output with only BToA0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSigCmykData, icSigLabData);
    addMpe(p, icSigDToB3Tag);
    addMpe(p, icSigBToD1Tag);
    check(!hasCritical(report(p)), "Output with DToB3Tag and BToD1Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSigGrayData, icSigLabData);
    addMpe(p, icSigAToB1Tag);
    check(hasCritical(report(p)), "gray Output with only AToB1Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSigGrayData, icSigLabData);
    addMpe(p, icSigBToA1Tag);
    check(hasCritical(report(p)), "gray Output with only BToA1Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSigGrayData, icSigLabData);
    addMpe(p, icSigAToB2Tag);
    addMpe(p, icSigBToD2Tag);
    check(!hasCritical(report(p)), "gray Output with AToB2Tag and BToD2Tag is accepted");
  }
  {
    // Display's matrix/TRC alternative does not extend to Output.
    CIccProfile *p = newProfile(icSigOutputClass, icSigRgbData, icSigLabData);
    addMatrixTrc(p);
    check(hasCritical(report(p)),
          "Output with matrix/TRC tags and no transform is refused");
  }

  // ---- spectral PCS, zero PCS field ----
  {
    CIccProfile *p = newSpectralProfile(icSigInputClass, icSigRgbData);
    addMpe(p, icSigDToB1Tag);
    check(!hasCritical(report(p)), "spectral Input with only DToB1Tag is accepted");
  }
  {
    CIccProfile *p = newSpectralProfile(icSigInputClass, icSigRgbData);
    addFiller(p);
    check(hasCritical(report(p)), "spectral Input with no transform is refused");
  }
  {
    CIccProfile *p = newSpectralProfile(icSigDisplayClass, icSigRgbData);
    addMpe(p, icSigDToB0Tag);
    check(hasCritical(report(p)), "spectral Display with only DToB0Tag is refused");
  }
  {
    CIccProfile *p = newSpectralProfile(icSigDisplayClass, icSigRgbData);
    addMpe(p, icSigDToB0Tag);
    addMpe(p, icSigBToD0Tag);
    check(!hasCritical(report(p)),
          "spectral Display with DToB0Tag and BToD0Tag is accepted");
  }
  {
    CIccProfile *p = newSpectralProfile(icSigOutputClass, icSigCmykData);
    addMpe(p, icSigDToB3Tag);
    check(hasCritical(report(p)), "spectral Output with only DToB3Tag is refused");
  }
  {
    CIccProfile *p = newSpectralProfile(icSigOutputClass, icSigCmykData);
    addMpe(p, icSigBToD3Tag);
    check(hasCritical(report(p)), "spectral Output with only BToD3Tag is refused");
  }
  {
    CIccProfile *p = newSpectralProfile(icSigOutputClass, icSigCmykData);
    addMpe(p, icSigDToB3Tag);
    addMpe(p, icSigBToD3Tag);
    check(!hasCritical(report(p)),
          "spectral Output with DToB3Tag and BToD3Tag is accepted");
  }

  // ---- DeviceLink (8.6) ----
  {
    CIccProfile *p = newProfile(icSigLinkClass, icSigRgbData, icSigCmykData);
    addMpe(p, icSigDToB0Tag);
    check(!hasCritical(report(p)), "DeviceLink with only DToB0Tag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigLinkClass, icSigRgbData, icSigCmykData);
    addFiller(p);
    check(hasCritical(report(p)), "control: DeviceLink with no transform is refused");
  }

  // ---- MultiplexIdentification (8.11) ----
  {
    CIccProfile *p = newProfile(icSigMultiplexIdentificationClass,
                                icSigRgbData, (icColorSpaceSignature)0);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(hasCritical(report(p)),
          "MID with multiplexTypeArrayTag but no AToM0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexIdentificationClass,
                                icSigRgbData, (icColorSpaceSignature)0);
    addMpe(p, icSigAToM0Tag);
    check(hasCritical(report(p)),
          "MID with AToM0Tag but no multiplexTypeArrayTag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexIdentificationClass,
                                icSigRgbData, (icColorSpaceSignature)0);
    addMpe(p, icSigAToM0Tag);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(!hasCritical(report(p)), "control: MID with both is accepted");
  }

  // ---- MultiplexLink (8.12) ----
  {
    CIccProfile *p = newProfile(icSigMultiplexLinkClass,
                                (icColorSpaceSignature)0, icSigCmykData);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(hasCritical(report(p)),
          "MLNK with multiplexTypeArrayTag but no MToA0Tag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexLinkClass,
                                (icColorSpaceSignature)0, icSigCmykData);
    addMpe(p, icSigMToA0Tag);
    check(hasCritical(report(p)),
          "MLNK with MToA0Tag but no multiplexTypeArrayTag is refused");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexLinkClass,
                                (icColorSpaceSignature)0, icSigCmykData);
    addMpe(p, icSigMToA0Tag);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(!hasCritical(report(p)), "control: MLNK with both is accepted");
  }

  // ---- MultiplexVisualization (8.13) ----
  {
    CIccProfile *p = newProfile(icSigMultiplexVisualizationClass,
                                (icColorSpaceSignature)0, icSigXYZData);
    addMpe(p, icSigMToB2Tag);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(!hasCritical(report(p)),
          "MVIS with MToB2Tag and multiplexTypeArrayTag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexVisualizationClass,
                                (icColorSpaceSignature)0, icSigXYZData);
    addMpe(p, icSigMToS3Tag);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(!hasCritical(report(p)),
          "MVIS with MToS3Tag and multiplexTypeArrayTag is accepted");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexVisualizationClass,
                                (icColorSpaceSignature)0, icSigXYZData);
    addArray(p, icSigMultiplexTypeArrayTag, icSigUtf8TextTypeArray);
    check(hasCritical(report(p)),
          "MVIS with multiplexTypeArrayTag but no transform is refused");
  }
  {
    CIccProfile *p = newProfile(icSigMultiplexVisualizationClass,
                                (icColorSpaceSignature)0, icSigXYZData);
    addMpe(p, icSigMToB0Tag);
    check(hasCritical(report(p)),
          "MVIS with MToB0Tag but no multiplexTypeArrayTag is refused");
  }

  // ---- xCLR output (8.5) ----
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSig6colorData, icSigLabData);
    addMpe(p, icSigAToB0Tag);
    addMpe(p, icSigBToA0Tag);
    addArray(p, icSigColorantInfoTag, icSigColorantInfoArray);
    std::string s = report(p);
    check(s.find("xCLR output profile has neither") == std::string::npos,
          "xCLR Output carrying only colorantInfoTag is not flagged");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSig6colorData, icSigLabData);
    addMpe(p, icSigAToB0Tag);
    addMpe(p, icSigBToA0Tag);
    addArray(p, icSigColorantTableTag, icSigColorantInfoArray);
    std::string s = report(p);
    check(s.find("xCLR output profile has neither") == std::string::npos,
          "xCLR Output carrying only colorantTableTag is not flagged");
  }
  {
    // The owner ruling left ICC.1 as it was: a v4 xCLR output profile without a
    // colorantTableTag is still non-compliant.
    CIccProfile *p = newProfile(icSigOutputClass, icSig6colorData, icSigLabData);
    p->m_Header.version = icVersionNumberV4_3;
    addMpe(p, icSigAToB0Tag);
    addMpe(p, icSigBToA0Tag);
    std::string s = report(p);
    check(s.find("NonCompliant! - xCLR output profile is missing colorantTableTag")
            != std::string::npos,
          "control: a v4 xCLR Output without colorantTableTag stays non-compliant");
  }
  {
    CIccProfile *p = newProfile(icSigOutputClass, icSig6colorData, icSigLabData);
    addMpe(p, icSigAToB0Tag);
    addMpe(p, icSigBToA0Tag);
    std::string s = report(p);
    size_t at = s.find("xCLR output profile has neither");
    check(at != std::string::npos,
          "control: xCLR Output with neither colorant tag is flagged");
    // The diagnostic is a warning: its line starts with the warning prefix.
    size_t bol = (at == std::string::npos) ? 0 : s.rfind('\n', at);
    bol = (bol == std::string::npos) ? 0 : bol + 1;
    check(at != std::string::npos &&
          s.compare(bol, strlen(icMsgValidateWarning), icMsgValidateWarning) == 0,
          "that diagnostic is a warning, not non-compliance");
  }

  if (g_fail) {
    std::fprintf(stderr, "[v5-required-tag-sets] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[v5-required-tag-sets] all checks passed\n");
  return 0;
}
