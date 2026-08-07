// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: iccPawgReport check C5 warned on any profile with a cicpTag (#2001).
//
// C5 asks "Is the profile free of additional tags not required for profile class
// (other than allowed optional tags)". icSigCicpTag was missing from
// kCommonOptional in PawgReport.cpp, so every profile carrying one drew:
//
//   [WARN] C5 ... standard tags outside the local class rule table: 'cicp'
//
// The reason it reached the warning rather than the private-tag bucket is the
// non-obvious part, and it is what this test really guards. IsSpecTag() asks
// CIccInfo::GetTagSigName, which resolves 'cicp' to "cicpTag" through
// CIccTagCreator (IccTagFactory.cpp), so the name does not begin with "Unknown"
// and the tag is never classified as private -- it falls straight through
// IsAllowedForClass() to the C5 warning. A conforming profile was therefore
// reported as non-conforming.
//
// Deliberately built on a plain SDR profile. The base fixture is the tracked
// sRGB v4 ICC preference profile and the cicp values written into it are
// BT.709 primaries / BT.709 transfer / BT.709 matrix / limited range (1/1/1/0) --
// no HDR transfer function, no HDR metadata. That matters because the defect was
// found while working on HDR profiles but is not HDR-specific: it hits any
// profile author who adds a cicpTag to an ordinary display profile. The corpus
// offers nothing suitable to reuse: of the nine tracked XML carrying a cicpTag,
// eight are under Testing/HDR/ and the ninth,
// .github/ci/test-data/ub-cicp-colorprimaries-1346.xml, is a deliberately
// malformed UB fixture that iccFromXml refuses outright, so it cannot be turned
// into a profile to assess. Hence the synthesised subject below.
//
// Structure is A/B against the same base profile:
//   control  -- base profile as tracked, no cicpTag  -> C5 must be OK
//   subject  -- same profile + a cicpTag             -> C5 must be OK
// The control is what proves the fixture was not already warning for some
// unrelated reason, so a green subject means something.
//
// This test writes its own output to stderr, because capturing the report means
// redirecting stdout with no portable way to restore it.
//
// C5's verdict is not exposed through PawgReport.h (only the compression verdict
// is, per #1775), so the report is captured from stdout and the C5 line read back.
// Asserting on the emitted text is arguably the better target anyway: the text is
// exactly what a user sees and what the issue was reported against.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "PawgReport.h"

#include "IccProfile.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

// Runs DumpPawgReport with stdout redirected to a file, then returns the whole
// report as a string.
static bool captureReport(const char *szProfile, const char *szTmp, std::string &out)
{
  // stdout is redirected and never restored: there is no portable way back to
  // the original stream (a CI runner has no /dev/tty and no CONOUT$), so this
  // test writes all of its own diagnostics to stderr instead. Each capture
  // simply re-points stdout at the next file.
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

// The report prints one line per check; pull the C5 one plus its detail, which
// continues on the following indented line.
static std::string extractC5(const std::string &report)
{
  size_t pos = report.find(" C5 ");
  if (pos == std::string::npos)
    pos = report.find(" C5\t");
  if (pos == std::string::npos)
    return std::string();

  // Back up to the start of that line, then take this line and the next two so
  // the wrapped detail ("standard tags outside ...") is included.
  size_t start = report.rfind('\n', pos);
  start = (start == std::string::npos) ? 0 : start + 1;

  size_t end = start;
  for (int i = 0; i < 3; ++i) {
    size_t nl = report.find('\n', end);
    if (nl == std::string::npos) { end = report.size(); break; }
    end = nl + 1;
  }

  return report.substr(start, end - start);
}

static void checkC5(const char *szCase, const char *szProfile, const char *szTmp)
{
  std::string report;
  if (!captureReport(szProfile, szTmp, report)) {
    std::fprintf(stderr, "FAIL [%s]: could not capture the report for '%s'\n", szCase, szProfile);
    g_failures++;
    return;
  }

  std::string c5 = extractC5(report);
  if (c5.empty()) {
    std::fprintf(stderr, "FAIL [%s]: no C5 line in the report for '%s'\n", szCase, szProfile);
    g_failures++;
    return;
  }

  bool bWarn = c5.find("[WARN]") != std::string::npos;
  bool bMentionsCicp = c5.find("cicp") != std::string::npos;

  if (bWarn || bMentionsCicp) {
    std::fprintf(stderr, "FAIL [%s]: C5 warned%s --\n%s\n", szCase,
                bMentionsCicp ? " and named 'cicp'" : "", c5.c_str());
    g_failures++;
    return;
  }

  std::fprintf(stderr, "ok   [%s]: C5 clean\n", szCase);
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <sdr-base-profile.icc>\n", argv[0]);
    return 1;
  }

  const char *szBase = argv[1];

  // Control: the tracked profile exactly as it ships, with no cicpTag. If this
  // ever warns, the fixture has changed and the subject case below proves
  // nothing.
  checkC5("control-no-cicp", szBase, "pawg-c5-control.txt");

  // Subject: same profile with a plain BT.709 SDR cicpTag attached.
  {
    CIccProfile *pIcc = ReadIccProfile(szBase);
    if (!pIcc) {
      std::fprintf(stderr, "FAIL [subject-with-cicp]: could not read '%s'\n", szBase);
      g_failures++;
    }
    else {
      CIccTagCicp *pCicp = new CIccTagCicp();
      // BT.709 primaries / BT.709 transfer / BT.709 matrix / limited range.
      pCicp->SetFields(1, 1, 1, 0);

      if (!pIcc->AttachTag(icSigCicpTag, pCicp)) {
        std::fprintf(stderr, "FAIL [subject-with-cicp]: AttachTag(icSigCicpTag) failed\n");
        g_failures++;
        delete pCicp;
      }
      else {
        const char *szSubject = "pawg-c5-cicp-subject.icc";
        if (!SaveIccProfile(szSubject, pIcc)) {
          std::fprintf(stderr, "FAIL [subject-with-cicp]: could not write '%s'\n", szSubject);
          g_failures++;
        }
        else {
          checkC5("subject-with-cicp", szSubject, "pawg-c5-subject.txt");
        }
      }
      delete pIcc;
    }
  }

  if (g_failures) {
    std::fprintf(stderr, "%d case(s) regressed\n", g_failures);
    return 1;
  }

  std::fprintf(stderr, "all cases passed\n");
  return 0;
}
