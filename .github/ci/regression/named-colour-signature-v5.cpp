// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// #2562.  ICC.2-2023 8.10 requires a v5 NamedColor profile to contain
// namedColorTag, whose signature is 'nmcl', and 13.2.1 gives the
// namedColorArray array type identifier as 'ncol'.  iccDEV used ICC.1's
// namedColor2Tag ('ncl2') at every version and spelled the array type
// identifier 'nmcl', so a v5 named colour profile was wrong in exactly those
// two fields and right everywhere else 'nmcl' appears.
//
// 'nmcl' has four other correct roles, which is why this cannot be fixed by
// replacing the string: the NamedColor profile class signature (7.2.5), the
// namedColorTag signature itself, the namedColorStructure struct type
// identifier (12.2.5.1), and the named colour data colour space.  Only the
// array type identifier moves to 'ncol'.
//
// A 'ncl2' tag in a v5 profile is an unrecognized tag for that version.  It is
// ignored for computation and earns a missing-required-tag error; it is NOT
// accepted as a substitute, because that would entrench a non-conforming
// spelling in the profiles iccDEV writes.
//
// What is asserted:
//
//   1. The v5 fixture loads from XML, and its named colour tag is reachable as
//      namedColorTag and NOT as namedColor2Tag.  A fixture that reverted to the
//      old spelling would fail here rather than silently removing the coverage.
//   2. On the wire the tag signature is 'nmcl', its type is 'tary', and bytes
//      8..11 of the payload -- the array type identifier -- are 'ncol'.  These
//      are the two fields the issue is about, read as bytes rather than through
//      the accessors that were themselves wrong.
//   3. The profile validates with no missing-required-tag error and no
//      "Invalid tag type" -- the positive control for case 5.
//   4. NEGATIVE, the required-tag half: the same bytes with the tag table
//      signature patched back to 'ncl2' report a missing required tag, and do
//      NOT report an invalid tag type: at v5 'ncl2' is an unrecognized tag, so
//      IsTypeValid leaves it alone and CheckRequiredTags reports it.
//   5. NEGATIVE, the IsTypeValid half: the same bytes with the array type
//      identifier patched back to 'nmcl' report "Invalid tag type".
//   6. The v4 fixture still carries namedColor2Tag and still validates, so the
//      ICC.1 path is untouched.
//
// The profile is saved with no profile ID.  With one, patching any byte would
// break the MD5 and draw "Bad Profile ID", which a status threshold cannot tell
// apart from the check under test; case 5 once passed that way with IsTypeValid
// accepting anything.  So cases 4 and 5 match the specific diagnostic.
//
// Which reverts each case catches:
//   CheckRequiredTags back to namedColor2Tag   cases 3 and 4
//   icSigNamedColorArray back to 'nmcl'        cases 2 and 5
//   IsTypeValid accepting any namedColorTag    case 5
//   IsTypeValid type-checking 'ncl2' at v5     case 4

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "IccProfile.h"
#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccTagFactory.h"
#include "IccUtil.h"

// CIccProfileXml::LoadXml() builds tags through the tag factory stack, so the XML
// factory has to be pushed before it runs or every tag comes back unparsed.  The
// stack is LIFO and each scope pushes exactly one.
class ScopedTagFactory {
public:
  explicit ScopedTagFactory(IIccTagFactory *pFactory)
  {
    CIccTagCreator::PushFactory(pFactory);
  }
  ~ScopedTagFactory()
  {
    delete CIccTagCreator::PopFactory();
  }
  ScopedTagFactory(const ScopedTagFactory &) = delete;
  ScopedTagFactory &operator=(const ScopedTagFactory &) = delete;
};

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

// Read a whole file, or leave the vector empty.
static std::vector<icUInt8Number> readFile(const char *szPath)
{
  std::vector<icUInt8Number> data;
  FILE *f = fopen(szPath, "rb");
  if (!f)
    return data;

  if (fseek(f, 0, SEEK_END) == 0) {
    long n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) {
      data.resize((size_t)n);
      if (fread(&data[0], 1, data.size(), f) != data.size())
        data.clear();
    }
  }
  fclose(f);
  return data;
}

static icUInt32Number be32(const icUInt8Number *p)
{
  return ((icUInt32Number)p[0] << 24) | ((icUInt32Number)p[1] << 16) |
         ((icUInt32Number)p[2] << 8)  |  (icUInt32Number)p[3];
}

// Locate a tag table entry by its 4-byte signature.  Returns false if absent.
static bool findTag(const std::vector<icUInt8Number> &d, const char *sig,
                    size_t &tableEntry, icUInt32Number &offset,
                    icUInt32Number &size)
{
  if (d.size() < 132)
    return false;

  icUInt32Number count = be32(&d[128]);
  for (icUInt32Number i = 0; i < count; i++) {
    size_t pos = 132 + (size_t)i * 12;
    if (pos + 12 > d.size())
      return false;
    if (!memcmp(&d[pos], sig, 4)) {
      tableEntry = pos;
      offset = be32(&d[pos + 4]);
      size   = be32(&d[pos + 8]);
      return offset + size <= d.size();
    }
  }
  return false;
}

// Validate a profile held in memory and say whether a required tag was missing.
static bool reportsMissingRequiredTag(const std::vector<icUInt8Number> &d,
                                      std::string &sReport)
{
  icValidateStatus nStatus = icValidateOK;
  sReport.clear();
  CIccProfile *pIcc = ValidateIccProfile(&d[0], (icUInt32Number)d.size(),
                                         sReport, nStatus);
  delete pIcc;
  return sReport.find("Critical tag(s) missing") != std::string::npos;
}

int main(int argc, char *argv[])
{
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: %s <NamedColor.xml> <NamedColorV4.xml> <scratch-dir>\n",
                 argv[0]);
    return 2;
  }

  const char *szV5Xml = argv[1];
  const char *szV4Xml = argv[2];
  const std::string scratch = argv[3];

  std::fprintf(stdout, "=== #2562 v5 named colour signatures ===\n");

  // ---- 1. the v5 fixture, through the XML reader ----
  {
    ScopedTagFactory xmlFactory(new CIccTagXmlFactory());
    CIccProfileXml v5;
    std::string sReport;
    check(v5.LoadXml(szV5Xml, NULL, &sReport),
          "v5 fixture loads from XML");
    check(v5.FindTag(icSigNamedColorTag) != NULL,
          "v5 named colour tag is reachable as namedColorTag");
    check(v5.FindTag(icSigNamedColor2Tag) == NULL,
          "v5 named colour tag is NOT reachable as namedColor2Tag");

    const std::string iccPath = scratch + "/issue-2562-namedcolor-v5.icc";
    check(SaveIccProfile(iccPath.c_str(), &v5, icNeverWriteID),
          "v5 fixture saves as a binary profile");

    // ---- 2. the two fields, read as bytes ----
    std::vector<icUInt8Number> d = readFile(iccPath.c_str());
    check(!d.empty(), "v5 binary profile reads back");

    size_t entry = 0;
    icUInt32Number off = 0, sz = 0;
    bool bFound = !d.empty() && findTag(d, "nmcl", entry, off, sz);
    check(bFound, "tag table carries signature 'nmcl'");

    // Everything below dereferences the located tag, so it is guarded rather
    // than short-circuited: a "!bFound ||" spelling would report these as
    // passes on a build that never found the tag at all.
    if (bFound) {
      check(!memcmp(&d[off], "tary", 4),
            "the namedColorTag payload is a tagArrayType");
      check(sz >= 12 && !memcmp(&d[off + 8], "ncol", 4),
            "the array type identifier at bytes 8..11 is 'ncol'");

      size_t staleEntry = 0;
      icUInt32Number staleOff = 0, staleSz = 0;
      check(!findTag(d, "ncl2", staleEntry, staleOff, staleSz),
            "tag table carries no 'ncl2'");

      // ---- 3. it validates (and is the control for case 5) ----
      std::string sValid;
      check(!reportsMissingRequiredTag(d, sValid),
            "v5 profile validates with no missing required tag");
      check(sValid.find("Invalid tag type") == std::string::npos,
            "v5 profile draws no invalid-tag-type diagnostic");

      // ---- 4. NEGATIVE: the required-tag half ----
      {
        std::vector<icUInt8Number> bad(d);
        memcpy(&bad[entry], "ncl2", 4);       // tag table signature only
        std::string sBad;
        check(reportsMissingRequiredTag(bad, sBad),
              "a v5 profile spelling the tag 'ncl2' is missing a required tag");
        check(sBad.find("Invalid tag type") == std::string::npos,
              "at v5 'ncl2' is unrecognized, not type-checked");
      }

      // ---- 5. NEGATIVE: the IsTypeValid half ----
      {
        std::vector<icUInt8Number> bad(d);
        memcpy(&bad[off + 8], "nmcl", 4);     // array type identifier only
        std::string sBad;
        reportsMissingRequiredTag(bad, sBad);
        check(sBad.find("Invalid tag type") != std::string::npos,
              "a v5 namedColorTag whose array identifier is 'nmcl' is an invalid tag type");
      }
    }
  }

  // ---- 6. the ICC.1 path is untouched ----
  {
    ScopedTagFactory xmlFactory(new CIccTagXmlFactory());
    CIccProfileXml v4;
    std::string sReport;
    check(v4.LoadXml(szV4Xml, NULL, &sReport),
          "v4 fixture loads from XML");
    check(v4.FindTag(icSigNamedColor2Tag) != NULL,
          "v4 named colour tag is still namedColor2Tag");
    check(v4.FindTag(icSigNamedColorTag) == NULL,
          "v4 fixture carries no namedColorTag");

    const std::string iccPath = scratch + "/issue-2562-namedcolor-v4.icc";
    check(SaveIccProfile(iccPath.c_str(), &v4, icNeverWriteID),
          "v4 fixture saves as a binary profile");

    std::vector<icUInt8Number> d = readFile(iccPath.c_str());
    check(!d.empty(), "v4 binary profile reads back");
    if (!d.empty()) {
      std::string sValid;
      check(!reportsMissingRequiredTag(d, sValid),
            "v4 profile validates with no missing required tag");
    }
  }

  if (g_fail) {
    std::fprintf(stderr, "[named-colour-signature-v5] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[named-colour-signature-v5] all checks passed\n");
  return 0;
}
