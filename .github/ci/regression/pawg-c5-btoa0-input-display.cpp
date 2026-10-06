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

// Regression: iccPawgReport check C5 warned on a BToA0Tag in an Input or
// Display profile.
//
// C5 reported "standard tags outside the local class rule table: 'B2A0'" for
// any Input or Display profile carrying a BToA0Tag.  ICC.1:2022 permits the tag
// in every Input and Display profile (8.3.2-8.3.4, 8.4.3, 8.4.4) and requires
// it of an N-component LUT-based Display profile (8.4.2), so C5 warned on a tag
// the specification demands.  The class rule tables allowed BToA1Tag and
// BToA2Tag through kCommonOptional but not BToA0Tag, which is required in the
// Output and ColorSpace classes and so sits in their required lists instead.
//
// The fix allows it for Input and Display in C5 only.  It must not join
// kMatrixTrcAlternative: that table is C4's any-of set, and a BToA0Tag there
// would let C4 pass an Input or Display profile with no forward transform.
//
// The base is the tracked sRGB v4 ICC preference profile, a ColorSpace profile
// carrying AToB0/AToB1 and BToA0/BToA1, with its header class set to Display
// and then to Input: exactly the LUT-based case, with no generated fixture.
//
// Cases:
//   display-lut     class 'mntr'                         -> C4 OK, C5 OK, no 'B2A0'
//   input-lut       class 'scnr'                         -> C4 OK, C5 OK, no 'B2A0'
//   control-pseq    'mntr' plus a profileSequenceDescTag -> C4 OK, C5 WARN naming 'pseq'
//   control-c4      'mntr' without its AToB0Tag          -> C4 not OK, naming the
//                                                           missing transform
// The two controls show C5 still flags a tag outside the table and C4 still
// refuses a BToA0Tag as the only transform.  The first two fail on an unfixed
// build.
//
// The report is captured from stdout, as iccdev.pawg-c5-cicp-optional does, so
// this test writes its own output to stderr.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "PawgReport.h"

#include "IccProfile.h"
#include "IccTagBasic.h"

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

// Writes the base profile with a new header class and the given edit applied.
static bool makeSubject(const char *szBase, const char *szOut, icProfileClassSignature cls,
                        bool bAddPseq, bool bDropAToB0)
{
  CIccProfile *pIcc = ReadIccProfile(szBase);
  if (!pIcc)
    return false;

  pIcc->m_Header.deviceClass = cls;

  bool ok = true;
  if (bAddPseq) {
    CIccTagProfileSeqDesc *pSeq = new CIccTagProfileSeqDesc();
    if (!pIcc->AttachTag(icSigProfileSequenceDescTag, pSeq)) {
      delete pSeq;
      ok = false;
    }
  }
  if (ok && bDropAToB0)
    ok = pIcc->DeleteTag(icSigAToB0Tag);

  if (ok)
    ok = SaveIccProfile(szOut, pIcc);

  delete pIcc;
  return ok;
}

struct Case {
  const char *szName;
  icProfileClassSignature cls;
  bool bAddPseq;
  bool bDropAToB0;
};

static void runCase(const Case &c, const char *szBase)
{
  std::string subject = std::string("pawg-c5-btoa0-") + c.szName + ".icc";
  std::string capture = std::string("pawg-c5-btoa0-") + c.szName + ".txt";

  if (!makeSubject(szBase, subject.c_str(), c.cls, c.bAddPseq, c.bDropAToB0)) {
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

  std::string c4 = extractCheck(report, "C4");
  std::string c5 = extractCheck(report, "C5");
  if (c4.empty() || c5.empty()) {
    std::fprintf(stderr, "FAIL [%s]: no C4 or C5 line in the report\n", c.szName);
    g_failures++;
    return;
  }

  bool bC4Ok = c4.find("[OK") != std::string::npos;
  bool bC5Ok = c5.find("[OK") != std::string::npos;
  bool bC5Warn = c5.find("[WARN]") != std::string::npos;
  bool bNamesB2A0 = c5.find("'B2A0'") != std::string::npos;
  bool bNamesPseq = c5.find("'pseq'") != std::string::npos;

  // C4's detail names the transform it found missing, so the control fails for
  // the reason it exists and not because the subject did not load.
  bool bC4MissesTransform = c4.find("A2B0 or matrix/TRC transform") != std::string::npos;

  bool bPass;
  if (c.bDropAToB0)
    bPass = !bC4Ok && bC4MissesTransform;
  else if (c.bAddPseq)
    bPass = bC4Ok && bC5Warn && bNamesPseq;
  else
    bPass = bC4Ok && bC5Ok && !bNamesB2A0;

  if (!bPass) {
    std::fprintf(stderr, "FAIL [%s]:\n%s%s\n", c.szName, c4.c_str(), c5.c_str());
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
    { "display-lut",  icSigDisplayClass, false, false },
    { "input-lut",    icSigInputClass,   false, false },
    { "control-pseq", icSigDisplayClass, true,  false },
    { "control-c4",   icSigDisplayClass, false, true  },
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
