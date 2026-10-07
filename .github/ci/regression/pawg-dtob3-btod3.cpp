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

// Regression: iccPawgReport treated DToB3Tag and BToD3Tag as tags ICC.1 does
// not define.
//
// ICC.1:2022 permits DToB0Tag to DToB3Tag and BToD0Tag to BToD3Tag in an
// N-component LUT-based Input, Display or Output profile and in a ColorSpace
// profile (8.3.2, 8.4.2, 8.5.2, 8.7); 9.1 gives the 3 pair the absolute
// rendering intent.  PawgReport.cpp listed only the 0 to 2 pairs, in three
// places:
//
//   C5   the optional tag tables, so a profile carrying the 3 pair was reported
//        as carrying "standard tags outside the local class rule table";
//   S1   the channel-count checks, so a 3 pair with the wrong channel count
//        was never looked at.  Adding it showed that S1 checked every DToBx
//        and BToDx against the colorimetric PCS, where ICC.2:2023 connects them
//        to the spectral PCS a v5 header sets (9.2.42), so S1 now does too;
//   C11  the v5-only list, so a v2 or v4 profile carrying the 3 pair was
//        reported as carrying iccMAX/v5-only transform tags.
//
// The base is the tracked sRGB v4 ICC preference profile (RGB data, Lab PCS),
// re-classed in memory, with three-channel DToB3 and BToD3 tags added.
//
// Cases:
//   display, input, output, colorspace   the 3 pair added  -> C5 does not name
//                                        them, C11 OK, S1 OK
//   s1-mismatch     'mntr', a DToB3 with four inputs        -> S1 WARN
//   spectral        v5, 33-channel spectral PCS, DToB3 3->33
//                   and BToD3 33->3                         -> S1 OK
//   spectral-mismatch  the same header, DToB3 3->3           -> S1 WARN
//   abstract-zero-data  v5 'abst', data colour space zero,
//                   33-channel spectral PCS, DToB0 33->33  -> S1 OK
//                   (ICC.2:2023 7.2.8: the spectral PCS is the A side)
//   control-link    'link', DToB3 only                      -> C5 names 'D2B3'
//   control-abstract  'abst', DToB3 only                    -> C5 names 'D2B3'
//                   (DeviceLink and Abstract allow DToB0 alone, 8.6, 8.8)
//   control-a2b3    'mntr', an AToB3Tag in this v4 profile  -> C11 not OK
//                   (AToB3 is ICC.2's)
// The subjects, s1-mismatch, spectral-mismatch and abstract-zero-data fail on
// an unfixed build; the controls pass.  spectral passes there too, since the
// 3 pair is not checked at all; it fails if S1 compares the D tags with the
// colorimetric PCS.
//
// The report is captured from stdout, as iccdev.pawg-c5-cicp-optional does, so
// this test writes its own output to stderr.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "PawgReport.h"

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagMPE.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

static bool captureReport(const char *szProfile, const char *szTmp, std::string &out)
{
  std::fflush(stdout);

  if (!std::freopen(szTmp, "w", stdout))
    return false;

  DumpPawgReport(szProfile, false);

  std::fflush(stdout);

  FILE *f = std::fopen(szTmp, "rb");
  if (!f)
    return false;

  char buf[4096];
  size_t n;
  out.clear();
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
    out.append(buf, n);
  std::fclose(f);

  return true;
}

// The check's line and the indented detail line after it.
static std::string extractCheck(const std::string &report, const char *szId)
{
  std::string key = std::string(" ") + szId + " ";
  size_t pos = report.find(key);
  if (pos == std::string::npos)
    return std::string();

  size_t start = report.rfind('\n', pos);
  start = (start == std::string::npos) ? 0 : start + 1;

  size_t end = start;
  for (int i = 0; i < 2; ++i) {
    size_t nl = report.find('\n', end);
    if (nl == std::string::npos) { end = report.size(); break; }
    end = nl + 1;
  }

  return report.substr(start, end - start);
}

static bool attachMpe(CIccProfile *pIcc, icTagSignature sig, icUInt16Number nIn, icUInt16Number nOut)
{
  CIccTagMultiProcessElement *pMpe = new CIccTagMultiProcessElement(nIn, nOut);
  if (!pIcc->AttachTag(sig, pMpe)) {
    delete pMpe;
    return false;
  }
  return true;
}

enum Expect { kAllowed, kS1Mismatch, kSpectral, kSpectralMismatch, kAbstractZeroData, kLinkWarns, kAbstractWarns, kA2B3Flagged };

struct Case {
  const char *szName;
  icProfileClassSignature cls;
  Expect expect;
};

static bool makeSubject(const char *szBase, const char *szOut, const Case &c)
{
  CIccProfile *pIcc = ReadIccProfile(szBase);
  if (!pIcc)
    return false;

  pIcc->m_Header.deviceClass = c.cls;

  bool ok = true;
  switch (c.expect) {
    case kAllowed:
      ok = attachMpe(pIcc, icSigDToB3Tag, 3, 3) && attachMpe(pIcc, icSigBToD3Tag, 3, 3);
      break;
    case kS1Mismatch:
      ok = attachMpe(pIcc, icSigDToB3Tag, 4, 3);
      break;
    case kSpectral:
    case kSpectralMismatch:
      pIcc->m_Header.version = icVersionNumberV5;
      pIcc->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralData | 33);
      ok = c.expect == kSpectral ?
        attachMpe(pIcc, icSigDToB3Tag, 3, 33) && attachMpe(pIcc, icSigBToD3Tag, 33, 3) :
        attachMpe(pIcc, icSigDToB3Tag, 3, 3);
      break;
    case kAbstractZeroData:
      pIcc->m_Header.version = icVersionNumberV5;
      pIcc->m_Header.colorSpace = icSigNoColorData;
      pIcc->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralData | 33);
      // The base's AToBx and BToAx tags are three-channel and would rightly
      // mismatch a 33-channel A side; a DToB0Tag is the transform here.
      ok = pIcc->DeleteTag(icSigAToB0Tag) && pIcc->DeleteTag(icSigAToB1Tag) &&
           pIcc->DeleteTag(icSigBToA0Tag) && pIcc->DeleteTag(icSigBToA1Tag) &&
           attachMpe(pIcc, icSigDToB0Tag, 33, 33);
      break;
    case kLinkWarns:
    case kAbstractWarns:
      ok = attachMpe(pIcc, icSigDToB3Tag, 3, 3);
      break;
    case kA2B3Flagged:
      ok = attachMpe(pIcc, icSigAToB3Tag, 3, 3);
      break;
  }

  if (ok)
    ok = SaveIccProfile(szOut, pIcc);

  delete pIcc;
  return ok;
}

static void runCase(const Case &c, const char *szBase)
{
  std::string subject = std::string("pawg-dtob3-") + c.szName + ".icc";
  std::string capture = std::string("pawg-dtob3-") + c.szName + ".txt";

  if (!makeSubject(szBase, subject.c_str(), c)) {
    std::fprintf(stderr, "FAIL [%s]: could not build the subject profile\n", c.szName);
    g_failures++;
    return;
  }

  std::string report;
  if (!captureReport(subject.c_str(), capture.c_str(), report)) {
    std::fprintf(stderr, "FAIL [%s]: could not capture the report\n", c.szName);
    g_failures++;
    return;
  }

  std::string c5 = extractCheck(report, "C5");
  std::string c11 = extractCheck(report, "C11");
  std::string s1 = extractCheck(report, "S1");
  if (c5.empty() || c11.empty() || s1.empty()) {
    std::fprintf(stderr, "FAIL [%s]: no C5, C11 or S1 line in the report\n", c.szName);
    g_failures++;
    return;
  }

  bool bNames3 = c5.find("'D2B3'") != std::string::npos || c5.find("'B2D3'") != std::string::npos;
  bool bC11Ok = c11.find("[OK") != std::string::npos;
  bool bS1Ok = s1.find("[OK") != std::string::npos;
  // A mismatch must be found, not merely not ruled out: N/A or a profile that
  // did not load would not be "[OK" either.
  bool bS1Warn = s1.find("[WARN") != std::string::npos &&
                 s1.find("inconsistent") != std::string::npos;

  bool bPass = false;
  switch (c.expect) {
    case kAllowed:    bPass = !bNames3 && bC11Ok && bS1Ok; break;
    case kS1Mismatch: bPass = bS1Warn; break;
    case kSpectral:   bPass = bS1Ok; break;
    case kSpectralMismatch: bPass = bS1Warn; break;
    case kAbstractZeroData: bPass = bS1Ok; break;
    case kLinkWarns:
    case kAbstractWarns: bPass = c5.find("'D2B3'") != std::string::npos; break;
    case kA2B3Flagged:
      bPass = !bC11Ok && c11.find("v5-only transform tags") != std::string::npos;
      break;
  }

  if (!bPass) {
    std::fprintf(stderr, "FAIL [%s]:\n%s%s%s\n", c.szName, c5.c_str(), c11.c_str(), s1.c_str());
    g_failures++;
    return;
  }

  std::fprintf(stderr, "ok   [%s]\n", c.szName);
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <sRGB_v4_ICC_preference.icc>\n", argv[0]);
    return 1;
  }

  const Case cases[] = {
    { "display",      icSigDisplayClass,    kAllowed },
    { "input",        icSigInputClass,      kAllowed },
    { "output",       icSigOutputClass,     kAllowed },
    { "colorspace",   icSigColorSpaceClass, kAllowed },
    { "s1-mismatch",  icSigDisplayClass,    kS1Mismatch },
    { "spectral",     icSigDisplayClass,    kSpectral },
    { "spectral-mismatch", icSigDisplayClass, kSpectralMismatch },
    { "abstract-zero-data", icSigAbstractClass, kAbstractZeroData },
    { "control-link", icSigLinkClass,       kLinkWarns },
    { "control-abstract", icSigAbstractClass, kAbstractWarns },
    { "control-a2b3", icSigDisplayClass,    kA2B3Flagged },
  };
  for (const Case &c : cases)
    runCase(c, argv[1]);

  if (g_failures) {
    std::fprintf(stderr, "%d case(s) regressed\n", g_failures);
    return 1;
  }

  std::fprintf(stderr, "all cases passed\n");
  return 0;
}
