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

// #2563.  CIccProfile::CheckHeader ran its class-independent checks -- date,
// platform, flags, attributes, version, preferred CMM, rendering intent,
// illuminant -- in the final else of a class chain whose earlier branches were
// MultiplexIdentification, MultiplexLink and ColorEncodingSpace.  MID and MLNK
// therefore never ran them: a profile of either class with rendering intent 9
// was reported valid.  The fix runs those checks for every class but
// ColorEncodingSpace, which requires the fields to be zero and tests so itself.
//
// It also removes MultiplexIdentification from the zero-data-colour-space
// exemption.  ICC.2-2023 7.2.8 grants a zero data colour space to abstract,
// MultiplexLink and MultiplexVisualization profiles, and not to MID.
//
// No tracked profile exercises either half -- every tracked MID and MLNK
// already has a valid intent and a non-zero data colour space -- so these
// in-memory profiles are the only evidence.  Each is a minimal v5 profile read
// through the public Validate().  Every message matched below has a single
// emitter in the library.
//
// Controls:
//   - an Output profile with intent 9 is refused, on master as well: it proves
//     the intent check is live, so a MID/MLNK pass cannot be a dead check.
//   - MLNK and MVIS with a zero data colour space are still accepted: they are
//     the two multiplex classes 7.2.8 grants it to.
//   - ColorEncodingSpace, with every other header field cleared, passes its
//     zero-field test at intent 0 and fails it at intent 9 -- and is NOT run
//     through the common intent check.  That pins the guard that keeps the
//     common block away from the one class that tests these fields itself.
//   - MID and MLNK are also checked for reserved profile flag bits, a second
//     check from the same block, so moving only the intent switch would fail.
//
// #2756.  ICC.2-2023 7.2.9: an abstract profile with a zero PCS, a non-zero
// data colour space and a non-zero spectral PCS defines its transform in the
// DToB0Tag, whose D side is "the colour space defined by the data field".  That
// field must therefore be a colorimetric PCS ('XYZ ', 'Lab ', Table 16) or,
// under the Extended Device Colour Space amendment, a spectral colour space
// (Table 21); a device space such as 'RGB ' is refused.  Controls: a non-zero
// PCS, a zero data colour space and a zero spectral PCS each put the header
// outside 7.2.9 and draw no such message.

#include <cstdio>
#include <string>

#include <cstring>

#include "IccProfile.h"
#include "IccTagMPE.h"
#include "IccTagLut.h"
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

static CIccProfile *newProfile(icProfileClassSignature cls,
                               icColorSpaceSignature data,
                               icColorSpaceSignature pcs,
                               icUInt32Number intent)
{
  CIccProfile *p = new CIccProfile;
  p->InitHeader();
  p->m_Header.version         = icVersionNumberV5;
  p->m_Header.deviceClass     = cls;
  p->m_Header.colorSpace      = data;
  p->m_Header.pcs             = pcs;
  p->m_Header.renderingIntent = intent;
  // No tags: Validate() runs CheckHeader() before anything that looks at them,
  // and every message this test matches comes from CheckHeader().
  return p;
}

// ColorEncodingSpace requires every other header field to be zero, and
// InitHeader() fills several in (preferred CMM, date, illuminant, creator).
// Clearing them makes the rendering intent the only field its zero-field test
// can be reacting to.
static CIccProfile *zeroedForCes(CIccProfile *p)
{
  p->m_Header.cmmId = 0;
  memset(&p->m_Header.date, 0, sizeof(p->m_Header.date));
  memset(&p->m_Header.illuminant, 0, sizeof(p->m_Header.illuminant));
  p->m_Header.creator = 0;
  return p;
}

static std::string report(CIccProfile *p)
{
  std::string s;
  p->Validate(s);
  delete p;
  return s;
}

static bool has(const std::string &s, const char *what)
{
  return s.find(what) != std::string::npos;
}

// A MultiplexIdentification or MultiplexLink profile needs an MCS signature to
// get past the multiplex designator check; ncXXXX with three channels will do.
static CIccProfile *withMcs(CIccProfile *p)
{
  p->m_Header.mcs = (icMultiplexColorSignature)(icSigSrcMCSChannelData + 3);
  return p;
}

// A v5 abstract profile with a zero data colour space, a Lab PCS, and an
// nSteps-channel reflectance spectral PCS that defines its A side (7.2.8).
static CIccProfile *zeroDataSpectralAbstract(icUInt16Number nSteps)
{
  CIccProfile *p = newProfile(icSigAbstractClass, (icColorSpaceSignature)0, icSigLabData, 0);
  p->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | nSteps);
  p->m_Header.spectralRange.start = icFtoF16(380.0f);
  p->m_Header.spectralRange.end = icFtoF16(730.0f);
  p->m_Header.spectralRange.steps = nSteps;
  return p;
}

int main()
{
  std::fprintf(stdout, "=== #2563 MID and MLNK header checks ===\n");

  const icColorSpaceSignature nc3 = (icColorSpaceSignature)(icSigNChannelData + 3);
  const icColorSpaceSignature none = (icColorSpaceSignature)0;

  // ---- the common checks now run for MID and MLNK ----
  check(has(report(withMcs(newProfile(icSigMultiplexIdentificationClass, nc3, none, 9))),
            "Unknown rendering intent"),
        "MID with rendering intent 9 is refused");
  check(has(report(withMcs(newProfile(icSigMultiplexLinkClass, none, icSigCmykData, 9))),
            "Unknown rendering intent"),
        "MLNK with rendering intent 9 is refused");
  check(has(report(newProfile(icSigOutputClass, icSigCmykData, icSigLabData, 9)),
            "Unknown rendering intent"),
        "control: Output with rendering intent 9 is refused");

  // A second check from the same block, so a fix that moved only the intent
  // switch out of the class chain would still go red.
  {
    CIccProfile *p = withMcs(newProfile(icSigMultiplexIdentificationClass, nc3, none, 0));
    p->m_Header.flags = 0x00000010;            // bit 4, reserved at v5
    check(has(report(p), "Reserved profile flags (bits 4-15) are non-zero"),
          "MID with a reserved profile flag bit set is reported");
  }
  {
    CIccProfile *p = withMcs(newProfile(icSigMultiplexLinkClass, none, icSigCmykData, 0));
    p->m_Header.flags = 0x00000010;
    check(has(report(p), "Reserved profile flags (bits 4-15) are non-zero"),
          "MLNK with a reserved profile flag bit set is reported");
  }

  // ---- ColorEncodingSpace stays outside the common block ----
  {
    std::string s = report(zeroedForCes(newProfile(icSigColorEncodingClass, none, none, 0)));
    check(!has(s, "Encoding Class has non-zero Header data"),
          "control: an all-zero ColorEncodingSpace header passes its zero-field test");
  }
  {
    std::string s = report(zeroedForCes(newProfile(icSigColorEncodingClass, none, none, 9)));
    check(has(s, "Encoding Class has non-zero Header data"),
          "ColorEncodingSpace with only intent 9 set fails its own zero-field test");
    check(!has(s, "Unknown rendering intent"),
          "ColorEncodingSpace is not run through the common intent check");
  }

  // ---- MID no longer gets a zero data colour space ----
  check(has(report(withMcs(newProfile(icSigMultiplexIdentificationClass, none, none, 0))),
            "Unknown colour space"),
        "MID with a zero data colour space is refused");
  check(!has(report(withMcs(newProfile(icSigMultiplexLinkClass, none, icSigCmykData, 0))),
             "Unknown colour space"),
        "control: MLNK with a zero data colour space is still accepted");
  check(!has(report(withMcs(newProfile(icSigMultiplexVisualizationClass, none, icSigXYZData, 0))),
             "Unknown colour space"),
        "control: MVIS with a zero data colour space is still accepted");
  check(!has(report(withMcs(newProfile(icSigMultiplexIdentificationClass, nc3, none, 0))),
             "Unknown colour space"),
        "control: MID with an ncXXXX data colour space is accepted");

  // ---- #2725: NamedColor and Abstract need no data colour space ----
  // Maintainer ruling: a single data colour space describes neither
  // class.  NamedColor carries its data in namedColorTag (and several of them
  // may use different device spaces); an abstract profile "does not represent
  // any device model" (ICC.2-2023 8.9), and 7.2.8 lets its data colour space be
  // zero, the spectral PCS fields then defining the A side.
  {
    CIccProfile *p = newProfile(icSigAbstractClass, none, none, 0);
    p->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | 36);
    check(!has(report(p), "Unknown colour space"),
          "v5 Abstract with a zero data colour space is accepted");
  }
  check(!has(report(newProfile(icSigNamedColorClass, none, icSigLabData, 0)),
             "Unknown colour space"),
        "control: v5 NamedColor with a zero data colour space is still accepted");
  {
    // The exemption is v5 only: a zero data colour space is iccMAX-only.
    CIccProfile *p = newProfile(icSigAbstractClass, none, icSigLabData, 0);
    p->m_Header.version = icVersionNumberV4_3;
    check(has(report(p), "Invalid data colour space (0x00000000) for a v2/v4 profile"),
          "control: v4 Abstract with a zero data colour space is still refused");
  }
  check(has(report(newProfile(icSigAbstractClass, (icColorSpaceSignature)0x7a7a7a7a, icSigLabData, 0)),
            "Unknown colour space"),
        "control: v5 Abstract with an unknown non-zero data colour space is still refused");

  // ---- #2725: the tag validators take that A side from the spectral PCS ----
  // With a zero data colour space, 7.2.8 says "the spectral PCS signature and
  // spectral range fields shall be used to define the A side", so an AToB0Tag
  // or DToB0Tag takes the spectral PCS's channels as input.  The validators
  // used to expect icGetSpaceSamples(0) = 0 inputs.
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(36, 3));
    check(!has(report(p), "Incorrect number of input channels"),
          "zero-data Abstract: an AToB0Tag MPE with 36 spectral inputs is accepted");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->AttachTag(icSigDToB0Tag, new CIccTagMultiProcessElement(36, 36));
    check(!has(report(p), "Incorrect number of input channels"),
          "zero-data Abstract: a DToB0Tag MPE with 36 spectral inputs is accepted");
  }
  {
    // A lut-based AToB0Tag cannot take 36 inputs; three spectral channels keep
    // the LUT validator's own input check in reach.
    CIccProfile *p = zeroDataSpectralAbstract(3);
    CIccTagLutAtoB *pLut = new CIccTagLutAtoB();
    pLut->Init(3, 3);
    p->AttachTag(icSigAToB0Tag, pLut);
    check(!has(report(p), "Incorrect number of input channels"),
          "zero-data Abstract: an AToB0Tag LUT with 3 spectral inputs is accepted");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(3, 3));
    check(has(report(p), "Incorrect number of input channels"),
          "control: zero-data Abstract: an AToB0Tag MPE with 3 inputs is refused");
  }
  {
    // A non-zero data colour space still sets the A side.
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.colorSpace = icSigLabData;
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(36, 3));
    check(has(report(p), "Incorrect number of input channels"),
          "control: Lab-data Abstract: an AToB0Tag MPE with 36 inputs is refused");
  }

  // ---- #2725 ruling: with a zero data colour space an AToB0Tag needs the
  // spectral PCS and spectral range (they define its A side); a DToB0Tag alone
  // has no A side and does not ----
  const char *kNoASide = "zero data colour space and an AToB0Tag";
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(36, 3));
    check(!has(report(p), kNoASide),
          "zero-data Abstract: AToB0Tag with spectral PCS and range is accepted");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.spectralPCS = (icColorSpaceSignature)0;
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(36, 3));
    check(has(report(p), kNoASide),
          "zero-data Abstract: AToB0Tag without a spectral PCS is refused");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.spectralRange.steps = 0;
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(36, 3));
    check(has(report(p), kNoASide),
          "zero-data Abstract: AToB0Tag without a spectral range is refused");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.spectralPCS = (icColorSpaceSignature)0;
    p->m_Header.spectralRange.steps = 0;
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(36, 3));
    p->AttachTag(icSigDToB0Tag, new CIccTagMultiProcessElement(36, 36));
    check(has(report(p), kNoASide),
          "zero-data Abstract: AToB0Tag and DToB0Tag without spectral fields is refused");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.spectralPCS = (icColorSpaceSignature)0;
    p->m_Header.spectralRange.steps = 0;
    p->AttachTag(icSigDToB0Tag, new CIccTagMultiProcessElement(36, 36));
    check(!has(report(p), kNoASide),
          "zero-data Abstract: DToB0Tag alone without spectral fields is accepted");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.colorSpace = icSigLabData;
    p->m_Header.spectralPCS = (icColorSpaceSignature)0;
    p->m_Header.spectralRange.steps = 0;
    p->AttachTag(icSigAToB0Tag, new CIccTagMultiProcessElement(3, 3));
    check(!has(report(p), kNoASide),
          "control: Lab-data Abstract with AToB0Tag needs no spectral fields");
  }

  // ---- #2756: 7.2.9's D side with a zero PCS ----
  // PCS zero, data colour space non-zero, spectral PCS non-zero: the DToB0Tag's
  // D side is "the colour space defined by the data field", so that field must
  // be a colorimetric PCS ('XYZ ', 'Lab ', Table 16) or, under the Extended
  // Device Colour Space amendment, a spectral colour space (Table 21).  The
  // DToB0Tag requirement itself is pinned in v5-required-tag-sets.
  const char *kNoDSide = "cannot define the D side of the DToB0Tag";
  const icColorSpaceSignature refl36 =
      (icColorSpaceSignature)(icSigReflectanceSpectralData | 36);
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = icSigLabData;
    check(!has(report(p), kNoDSide),
          "zero-PCS Abstract with a Lab data colour space is accepted");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = icSigXYZData;
    check(!has(report(p), kNoDSide),
          "zero-PCS Abstract with an XYZ data colour space is accepted");
  }
  {
    // The Testing/SpecRef RefDecC/RefDecH/RefIncW header shape: reflectance
    // in, reflectance out, one header range for both (those fixtures use 31
    // steps; the helper's 36 are kept so the data space matches its range).
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = refl36;
    std::string s = report(p);
    check(!has(s, kNoDSide),
          "zero-PCS Abstract with a reflectance data colour space is accepted");
    check(!has(s, "Unknown colour space"),
          "control: that reflectance data colour space is a known space");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = icSigRgbData;
    check(has(report(p), kNoDSide),
          "zero-PCS Abstract with an RGB data colour space is refused");
  }
  {
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = nc3;
    check(has(report(p), kNoDSide),
          "zero-PCS Abstract with an ncXXXX data colour space is refused");
  }
  {
    // An unknown signature is reported once, by the data colour space block,
    // and not a second time here.
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = (icColorSpaceSignature)0x7a7a7a7a;
    std::string s = report(p);
    check(has(s, "Unknown colour space") && !has(s, kNoDSide),
          "zero-PCS Abstract with an unknown data colour space draws only the unknown-space message");
  }
  {
    // A non-zero PCS is outside 7.2.9: the AToBx/BToAx tags take it as their
    // B side and the data field is an ordinary A side.
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.colorSpace = icSigRgbData;
    check(!has(report(p), kNoDSide),
          "control: Lab-PCS Abstract with an RGB data colour space draws no D-side message");
  }
  {
    // A zero data colour space is 7.2.8's form, tested above.
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    check(!has(report(p), kNoDSide),
          "control: zero-PCS zero-data Abstract draws no D-side message");
  }
  {
    // A zero spectral PCS fails 7.2.9's third condition.
    CIccProfile *p = zeroDataSpectralAbstract(36);
    p->m_Header.pcs = none;
    p->m_Header.colorSpace = icSigRgbData;
    p->m_Header.spectralPCS = none;
    check(!has(report(p), kNoDSide),
          "control: zero-PCS Abstract without a spectral PCS draws no D-side message");
  }

  if (g_fail) {
    std::fprintf(stderr, "[mid-mlnk-header-checks] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[mid-mlnk-header-checks] all checks passed\n");
  return 0;
}
