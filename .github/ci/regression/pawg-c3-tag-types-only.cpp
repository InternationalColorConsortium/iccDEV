/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// Regression: iccPawgReport check C3 reported the whole validation report.
//
// C3 asks "Do tags only use tag types allowed for the tag?".  It took its
// verdict from CIccProfile::Validate() and its detail from that report's first
// line, so any validation finding showed under C3: a negative Z in an XYZ tag,
// which is legal and which C1 already reports, read as a tag-type warning
// (#2806), and a calculator stack underflow read as a tag-type failure.
//
// C3 now calls CIccProfile::CheckTagTypes(), the step of Validate() that
// compares each tag's type with the types its signature allows.
//
// The base is the tracked sRGB v4 ICC preference profile, which passes C1 and
// C3 as tracked.
//
// Validate() also reports on the profile as a whole: its header, the tags the
// library requires, duplicate tags.  Those findings belong to no tag, so C1's
// per-tag pass did not see them and C3 was the only check that showed them.
// C1, which already carried a critical finding of that kind, now carries the
// rest when nothing else accounts for the verdict, so none is lost and the
// exit code of iccPawgReport does not change.
//
// Cases:
//   base              as tracked                          -> C1 OK, C3 OK
//   negative-z        mediaWhitePointTag Z set negative   -> C1 WARN naming it,
//                                                            C3 OK
//   header-month      header creation month set to 15     -> C1 WARN naming it,
//                                                            C3 OK
//   missing-cprt      copyrightTag deleted                -> C1 FAIL saying a
//                                                            required tag is
//                                                            missing, C3 OK
//   control-type      copyrightTag holding an XYZType     -> C3 FAIL naming the
//                                                            tag
//   control-v2-type   profileDescriptionTag holding a     -> C3 WARN naming the
//                     textDescriptionType in this v4         type
//                     profile
// The two controls show C3 still reports a tag type that is not allowed, at
// both levels the library gives it.  negative-z, header-month and missing-cprt
// fail on an unfixed build: each had its finding under C3.
//
// The report is captured from stdout, as iccdev.pawg-c5-cicp-optional does, so
// this test writes its own output to stderr.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "PawgReport.h"

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

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

enum Edit { kNone, kNegativeZ, kHeaderMonth, kNoCopyright, kXyzCopyright, kV2Description };

// Writes the base profile with the given edit applied.
static bool makeSubject(const char *szBase, const char *szOut, Edit edit)
{
  CIccProfile *pIcc = ReadIccProfile(szBase);
  if (!pIcc)
    return false;

  bool ok = true;
  if (edit == kNegativeZ) {
    CIccTag *pTag = pIcc->FindTag(icSigMediaWhitePointTag);
    if (pTag && pTag->GetType() == icSigXYZType)
      (*static_cast<CIccTagXYZ*>(pTag))[0].Z = icDtoF(-0.01f);
    else
      ok = false;
  }
  else if (edit == kHeaderMonth) {
    pIcc->m_Header.date.month = 15;
  }
  else if (edit == kNoCopyright) {
    ok = pIcc->DeleteTag(icSigCopyrightTag);
  }
  else if (edit == kXyzCopyright) {
    CIccTagXYZ *pXYZ = new CIccTagXYZ();
    (*pXYZ)[0].X = icDtoF(0.5f);
    (*pXYZ)[0].Y = icDtoF(0.5f);
    (*pXYZ)[0].Z = icDtoF(0.5f);
    ok = pIcc->DeleteTag(icSigCopyrightTag) && pIcc->AttachTag(icSigCopyrightTag, pXYZ);
    if (!ok)
      delete pXYZ;
  }
  else if (edit == kV2Description) {
    CIccTagTextDescription *pDesc = new CIccTagTextDescription();
    pDesc->SetText("v2 description");
    ok = pIcc->DeleteTag(icSigProfileDescriptionTag) && pIcc->AttachTag(icSigProfileDescriptionTag, pDesc);
    if (!ok)
      delete pDesc;
  }

  if (ok)
    ok = SaveIccProfile(szOut, pIcc);

  delete pIcc;
  return ok;
}

struct Case {
  const char *szName;
  Edit edit;
  const char *szC1;       // verdict C1 must carry
  const char *szC1Names;  // text C1's detail must carry, or NULL
  const char *szC3;       // verdict C3 must carry
  const char *szC3Names;  // text C3's detail must carry, or NULL
};

static void runCase(const Case &c, const char *szBase)
{
  std::string subject = std::string("pawg-c3-") + c.szName + ".icc";
  std::string capture = std::string("pawg-c3-") + c.szName + ".txt";

  if (!makeSubject(szBase, subject.c_str(), c.edit)) {
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

  std::string c1 = extractCheck(report, "C1");
  std::string c3 = extractCheck(report, "C3");
  if (c1.empty() || c3.empty()) {
    std::fprintf(stderr, "FAIL [%s]: no C1 or C3 line in the report\n", c.szName);
    g_failures++;
    return;
  }

  bool bPass = c3.find(c.szC3) != std::string::npos &&
               (!c.szC3Names || c3.find(c.szC3Names) != std::string::npos) &&
               (!c.szC1 || c1.find(c.szC1) != std::string::npos) &&
               (!c.szC1Names || c1.find(c.szC1Names) != std::string::npos);

  // A finding C1 names must not be repeated under C3.
  if (c.szC1Names && c3.find(c.szC1Names) != std::string::npos)
    bPass = false;

  if (!bPass) {
    std::fprintf(stderr, "FAIL [%s]:\n%s%s\n", c.szName, c1.c_str(), c3.c_str());
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
    { "base",            kNone,          "[OK",    NULL,               "[OK",    NULL },
    { "negative-z",      kNegativeZ,     "[WARN]", "Negative Z value", "[OK",    NULL },
    { "header-month",    kHeaderMonth,   "[WARN]", "Invalid month",    "[OK",    NULL },
    { "missing-cprt",    kNoCopyright,   "[FAIL]", "Required tags missing", "[OK", NULL },
    { "control-type",    kXyzCopyright,  NULL,     NULL,               "[FAIL]", "copyrightTag" },
    { "control-v2-type", kV2Description, NULL,     NULL,               "[WARN]", "ICC v2 tag type" },
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
