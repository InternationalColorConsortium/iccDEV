/*
    File:       colorant-count-validate.cpp

    Contains:   CTest helper for #2541: CIccProfile::Validate() is where a
                colorant count is checked against the profile header, not the
                readers.

    #2542 made every reader of the colorantOrder, colorantTable and chromaticity
    counts refuse a count wider than the tag's 16-bit SetSize(), and accept
    65535 in full.  The colorant-count-narrowing-{binary,xml,json} helpers pin
    that, and their 65535 cases are storage boundaries only.  A profile carrying
    65535 colorants still does not conform.  ICC.1:2022 10.4 and 10.5 say the
    count "shall be in agreement with the data colour space signature of
    7.2.6", and Table 19 there stops at 15 colours.  The readers keep any
    representable tag, and CIccTagColorantOrder::Validate() and
    CIccTagColorantTable::Validate() judge it against the header.  This helper
    pins that second stage through CIccProfile::Validate(), so the tag
    signature reaches each tag's Validate() the way it does for a loaded
    profile.

    colorantTableOutTag (9.2.20, DeviceLink only) states no count rule of its
    own.  The library compares it against the header's PCS field, which holds a
    DeviceLink's output space, and the DeviceLink cases pin that reading.

    Every profile is built in memory.  The headers are chosen so the data colour
    space and the PCS have different channel counts: CMYK (4) over Lab (3) for
    the output profile, and RGB (3) to CMYK (4) for the DeviceLink.  A validator
    that compared against the wrong header field therefore fails a control.

    Each case checks two things.

    - Through CIccProfile::Validate(): a matching count must leave the report
      and status exactly as the same profile without the tag.  A mismatch must
      add the exact "Incorrect number of colorants." line for that tag.  That
      profile is not itself valid (no A2B0 and so on), so its status is already
      icValidateCriticalError and cannot show what the tag contributes.
    - Through the tag's own Validate(), given the same profile: the status the
      tag returns must be icValidateOK for a matching count and at least
      icValidateNonCompliant for a mismatch.  This is the half that fails if a
      mismatch is reported but no longer raises the status.

    Not covered: colorantOrderOutTag ('cloo').  CIccTagColorantOrder::Validate()
    does not look at the tag signature, so it compares 'cloo' against the data
    colour space as well, where ICC.2:2023 9.2.52 associates it with the PCS
    field.  That is left for a separate change.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cstdio>
#include <string>

static const char *const kMismatch = " - Incorrect number of colorants.\n";

static int check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "colorant-count-validate: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "colorant-count-validate: FAIL  %s\n", label);
  return 1;
}

// The header, plus a copyright tag.  CIccProfile::Validate() stops at "No tags
// present" for a profile with no tags at all, so the profile compared against
// must already carry one tag for the colorant tag's contribution to show.
static void initProfile(CIccProfile &profile, icProfileClassSignature deviceClass,
                        icColorSpaceSignature colorSpace,
                        icColorSpaceSignature pcs)
{
  profile.InitHeader();
  profile.m_Header.version     = icVersionNumberV4_3;
  profile.m_Header.deviceClass = deviceClass;
  profile.m_Header.colorSpace  = colorSpace;
  profile.m_Header.pcs         = pcs;

  CIccTagMultiLocalizedUnicode *pCprt = new CIccTagMultiLocalizedUnicode();
  pCprt->SetText("Copyright (C) 2026 The International Color Consortium");
  profile.AttachTag(icSigCopyrightTag, pCprt);
}

static CIccTag *orderTag(icUInt16Number nCount)
{
  CIccTagColorantOrder *pTag = new CIccTagColorantOrder();
  if (!pTag->SetSize(nCount)) {
    delete pTag;
    return NULL;
  }
  for (icUInt32Number i = 0; i < nCount; i++)
    (*pTag)[(int)i] = (icUInt8Number)i;
  return pTag;
}

static CIccTag *tableTag(icUInt16Number nCount)
{
  CIccTagColorantTable *pTag = new CIccTagColorantTable();
  if (!pTag->SetSize(nCount)) {
    delete pTag;
    return NULL;
  }
  for (icUInt32Number i = 0; i < nCount; i++)
    std::snprintf(pTag->GetEntry(i)->name, sizeof(pTag->GetEntry(i)->name),
                  "colorant-%u", (unsigned)i);
  return pTag;
}

// The line a tag's Validate() adds for a count that disagrees with the header,
// spelled from the library's own prefix and tag name.
static std::string mismatchLine(icTagSignature sig)
{
  CIccInfo info;
  return std::string(icMsgValidateNonCompliant) +
         info.GetSigPathName(icGetSigPath(sig)) + kMismatch;
}

struct Verdict {
  icValidateStatus status;
  std::string report;
};

static Verdict validate(const CIccProfile &profile)
{
  Verdict v;
  v.status = profile.Validate(v.report);
  return v;
}

static int reportMismatch(const char *label, const Verdict &bare,
                          const Verdict &tagged, const Verdict &tagOnly)
{
  std::fprintf(stderr,
               "colorant-count-validate: FAIL  %s (profile status %d without the "
               "tag, %d with it; tag status %d)\n--- without ---\n%s--- with "
               "---\n%s--- tag ---\n%s",
               label, (int)bare.status, (int)tagged.status, (int)tagOnly.status,
               bare.report.c_str(), tagged.report.c_str(),
               tagOnly.report.c_str());
  return 1;
}

// Validate the profile without the colorant tag, then the same profile with it
// attached.  bExpectMismatch selects which contract the pair must meet.
static int countCase(icProfileClassSignature deviceClass,
                     icColorSpaceSignature colorSpace,
                     icColorSpaceSignature pcs, icTagSignature sig,
                     CIccTag *pTag, bool bExpectMismatch, const char *label)
{
  if (!pTag)
    return check(false, label);

  CIccProfile bareProfile;
  initProfile(bareProfile, deviceClass, colorSpace, pcs);
  const Verdict bare = validate(bareProfile);

  CIccProfile profile;
  initProfile(profile, deviceClass, colorSpace, pcs);
  if (!profile.AttachTag(sig, pTag)) {
    delete pTag;
    return check(false, label);
  }
  const Verdict tagged = validate(profile);

  // The tag's own verdict, with the same profile and signature path.
  Verdict tagOnly;
  tagOnly.status = pTag->Validate(icGetSigPath(sig), tagOnly.report, &profile);

  const bool bHasLine =
    tagged.report.find(mismatchLine(sig)) != std::string::npos;

  if (!bExpectMismatch) {
    if (bHasLine || tagged.status != bare.status ||
        tagged.report != bare.report || tagOnly.status != icValidateOK)
      return reportMismatch(label, bare, tagged, tagOnly);
    return check(true, label);
  }

  if (!bHasLine || tagOnly.status < icValidateNonCompliant)
    return reportMismatch(label, bare, tagged, tagOnly);
  return check(true, label);
}

int main()
{
  int failures = 0;

  // Output profile: data colour space CMYK (4 channels), PCS Lab (3).
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantOrderTag, orderTag(4), false,
                        "a colorantOrderTag of 4 in a CMYK profile adds nothing to the report");
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantOrderTag, orderTag(3), true,
                        "a colorantOrderTag of 3 in a CMYK profile is non-compliant");
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantOrderTag, orderTag(5), true,
                        "a colorantOrderTag of 5 in a CMYK profile is non-compliant");
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantOrderTag, orderTag(65535), true,
                        "a colorantOrderTag of 65535, the storage bound, is non-compliant in a profile");

  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantTableTag, tableTag(4), false,
                        "a colorantTableTag of 4 in a CMYK profile adds nothing to the report");
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantTableTag, tableTag(3), true,
                        "a colorantTableTag of 3 in a CMYK profile is non-compliant");
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantTableTag, tableTag(5), true,
                        "a colorantTableTag of 5 in a CMYK profile is non-compliant");
  failures += countCase(icSigOutputClass, icSigCmykData, icSigLabData,
                        icSigColorantTableTag, tableTag(65535), true,
                        "a colorantTableTag of 65535, the storage bound, is non-compliant in a profile");

  // DeviceLink: data colour space RGB (3), PCS field CMYK (4).
  // colorantTableTag follows the data colour space, colorantTableOutTag the PCS.
  failures += countCase(icSigLinkClass, icSigRgbData, icSigCmykData,
                        icSigColorantTableTag, tableTag(3), false,
                        "a DeviceLink colorantTableTag matching its RGB data colour space adds nothing");
  failures += countCase(icSigLinkClass, icSigRgbData, icSigCmykData,
                        icSigColorantTableTag, tableTag(4), true,
                        "a DeviceLink colorantTableTag counted from its CMYK PCS field is non-compliant");
  failures += countCase(icSigLinkClass, icSigRgbData, icSigCmykData,
                        icSigColorantTableOutTag, tableTag(4), false,
                        "a DeviceLink colorantTableOutTag matching its CMYK PCS field adds nothing");
  failures += countCase(icSigLinkClass, icSigRgbData, icSigCmykData,
                        icSigColorantTableOutTag, tableTag(3), true,
                        "a DeviceLink colorantTableOutTag counted from its RGB data colour space is non-compliant");

  if (failures) {
    std::fprintf(stderr, "colorant-count-validate: %d case(s) failed\n",
                 failures);
    return 1;
  }

  std::fprintf(stdout, "colorant-count-validate: all cases passed\n");
  return 0;
}
