// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       copyright-tag-icc2.cpp

    Contains:   CTest helper for the tag types a copyrightTag may use (ICC.2:2023
                clause 9.2.56, ICC.1:2022 clause 9.2.22).

    ICC.2 permits multiLocalizedUnicodeType, textType and utf8Type for 'cprt'.
    ICC.1 v4 permits multiLocalizedUnicodeType only, and v2 textType only.
    CIccProfile::IsTypeValid() applied the v4 rule from v4 onwards, so a v5
    profile with a utf8Type copyright was reported NonCompliant ("Invalid tag
    type"), and one with a textType copyright drew the "ICC v2 tag type" warning
    meant for a v2 profile mislabelled as v4.  #2565.

    The case table pins every permitted and refused type at v2, v4 and v5.  The
    utf16 type stays refused at v5: 9.2.56 does not list it, and a check that
    accepted any text type would pass every other row.
*/

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[copyright-tag-icc2] FAIL %s: %s\n", label, what);
  }
}

const icUInt32Number kV2 = 0x02100000;
const icUInt32Number kV4 = 0x04400000;
const icUInt32Number kV5 = icVersionNumberV5;

CIccTag *makeTag(icTagTypeSignature type)
{
  const char *szText = "Copyright 2026 International Color Consortium";
  switch (type) {
    case icSigTextType: {
      CIccTagText *p = new CIccTagText;
      p->SetText(szText);
      return p;
    }
    case icSigMultiLocalizedUnicodeType: {
      CIccTagMultiLocalizedUnicode *p = new CIccTagMultiLocalizedUnicode;
      p->SetText(szText);
      return p;
    }
    case icSigUtf8TextType: {
      CIccTagUtf8Text *p = new CIccTagUtf8Text;
      p->SetText(szText);
      return p;
    }
    case icSigUtf16TextType: {
      CIccTagUtf16Text *p = new CIccTagUtf16Text;
      p->SetText(szText);
      return p;
    }
    default:
      return NULL;
  }
}

// The report lines about the copyright tag's type, and nothing else: the profile
// carries no other tags, so it draws missing-tag errors that are not under test.
std::string copyrightTypeLines(icUInt32Number version, icTagTypeSignature type)
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = version;
  prof.m_Header.deviceClass = icSigDisplayClass;
  prof.m_Header.colorSpace = icSigRgbData;
  prof.m_Header.pcs = icSigXYZData;
  prof.AttachTag(icSigCopyrightTag, makeTag(type));

  std::string report, lines;
  prof.Validate(report);

  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (line.find("copyrightTag") != std::string::npos &&
        (line.find("Invalid tag type") != std::string::npos ||
         line.find("ICC v2 tag type") != std::string::npos))
      lines += line + "\n";
    pos = end + 1;
  }
  return lines;
}

enum Verdict { kClean, kLegacyWarning, kInvalid };

struct Case {
  icUInt32Number version;
  icTagTypeSignature type;
  Verdict want;
};

const Case kCases[] = {
  // ICC.2 9.2.56: all three listed types.
  { kV5, icSigMultiLocalizedUnicodeType, kClean },
  { kV5, icSigTextType,                  kClean },
  { kV5, icSigUtf8TextType,              kClean },
  { kV5, icSigUtf16TextType,             kInvalid },

  // ICC.1 v4: mluc only; textType keeps its mislabelled-v2 warning.
  { kV4, icSigMultiLocalizedUnicodeType, kClean },
  { kV4, icSigTextType,                  kLegacyWarning },
  { kV4, icSigUtf8TextType,              kInvalid },

  // v2: textType only.
  { kV2, icSigTextType,                  kClean },
  { kV2, icSigMultiLocalizedUnicodeType, kInvalid },
  { kV2, icSigUtf8TextType,              kInvalid },
};

} // namespace

int main()
{
  for (const Case &c : kCases) {
    char label[64], typeStr[8];
    std::snprintf(label, sizeof(label), "v%x %s", (unsigned int)(c.version >> 24),
                  icGetSigStr(typeStr, sizeof(typeStr), c.type));

    std::string lines = copyrightTypeLines(c.version, c.type);
    bool invalid = lines.find("NonCompliant") != std::string::npos &&
                   lines.find("Invalid tag type") != std::string::npos;
    bool legacy = lines.find("Warning") != std::string::npos &&
                  lines.find("ICC v2 tag type") != std::string::npos;

    switch (c.want) {
      case kClean:
        check(lines.empty(), label, ("expected no type finding, got: " + lines).c_str());
        break;
      case kLegacyWarning:
        check(legacy && !invalid, label, ("expected the v2-type warning, got: " + lines).c_str());
        break;
      case kInvalid:
        check(invalid, label, ("expected Invalid tag type, got: " + lines).c_str());
        break;
    }
  }

  if (g_fail) {
    std::fprintf(stderr, "[copyright-tag-icc2] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[copyright-tag-icc2] all checks passed\n");
  return 0;
}
