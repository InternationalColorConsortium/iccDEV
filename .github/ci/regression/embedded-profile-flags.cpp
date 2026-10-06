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

/*
 * embedded-profile-flags.cpp -- the embedded ICC.2 profile's header flags are
 * validated against ICC Technical Note 04-2018.
 *
 * CIccTagEmbeddedProfile::Validate() validated the child profile itself and
 * then checked only its channel count and device class against the parent.
 * The technical note's "ICC.2 Profile header requirements" also say that bit
 * 0 of the child's flags "should be set to 1 to indicate that the profile is
 * embedded in another file", and that "only ICC.2 profiles containing 0 in
 * bit position 1 should be embedded in other profiles".  Neither was checked
 * (#2693).  Both are "should", so each now draws a warning.
 *
 * This pins, for a v4 display host holding a v5 display child:
 *   1. a child with bit 0 set and bit 1 clear draws neither warning;
 *   2. a child with bit 0 clear draws the not-marked-embedded warning;
 *   3. a child with bit 1 set draws the cannot-be-used-independently warning;
 *   4. the two warnings are independent, and the result is a warning, not an
 *      error;
 *   5. the note's "shall ... have the same device space": a child whose
 *      device space differs from the host's is a critical error, whether it
 *      is wider (4CLR under RGB, which the channel-count check accepted) or
 *      narrower, and the same space draws nothing.
 * The note says nothing about the child's PCS, so none is asserted.
 */

#include "IccProfile.h"
#include "IccTagEmbedIcc.h"
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

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[embedded-profile-flags] FAIL %s\n", what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

const char *kNotEmbedded = "do not mark it as embedded in another file";
const char *kDataOnly = "cannot be used independently of embedded colour data";

/* A v5 display child with the given header flags and device space, owned by
   the tag. */
CIccTagEmbeddedProfile *newEmbedded(icUInt32Number flags, icColorSpaceSignature space = icSigRgbData)
{
  CIccProfile *pChild = new CIccProfile;
  pChild->InitHeader();
  pChild->m_Header.version = icVersionNumberV5;
  pChild->m_Header.deviceClass = icSigDisplayClass;
  pChild->m_Header.colorSpace = space;
  pChild->m_Header.pcs = icSigXYZData;
  pChild->m_Header.flags = flags;

  CIccTagEmbeddedProfile *pTag = new CIccTagEmbeddedProfile;
  pTag->SetProfile(pChild);
  return pTag;
}

icValidateStatus validate(icUInt32Number flags, std::string &report,
                          icColorSpaceSignature childSpace = icSigRgbData)
{
  CIccProfile host;
  host.InitHeader();
  host.m_Header.version = icVersionNumberV4_3;
  host.m_Header.deviceClass = icSigDisplayClass;
  host.m_Header.colorSpace = icSigRgbData;
  host.m_Header.pcs = icSigXYZData;

  CIccTagEmbeddedProfile *pTag = newEmbedded(flags, childSpace);
  icValidateStatus rv = pTag->Validate("", report, &host);
  delete pTag;
  return rv;
}

void testFlags()
{
  std::string report;

  validate(icEmbeddedProfileTrue, report);
  check(!has(report, kNotEmbedded) && !has(report, kDataOnly),
        "bit 0 set and bit 1 clear: neither flag warning");

  report.clear();
  icValidateStatus rv = validate(0, report);
  check(has(report, kNotEmbedded), "bit 0 clear: the not-marked-embedded warning is drawn");
  check(!has(report, kDataOnly), "bit 0 clear: the bit 1 warning is not drawn");
  check(rv >= icValidateWarning, "bit 0 clear: the status is at least a warning");

  report.clear();
  validate(icEmbeddedProfileTrue | icUseWithEmbeddedDataOnly, report);
  check(has(report, kDataOnly), "bit 1 set: the cannot-be-used-independently warning is drawn");
  check(!has(report, kNotEmbedded), "bit 1 set: the bit 0 warning is not drawn");

  report.clear();
  rv = validate(icUseWithEmbeddedDataOnly, report);
  check(has(report, kNotEmbedded) && has(report, kDataOnly), "both wrong: both warnings are drawn");
  check(has(report, icMsgValidateWarning), "the flag findings carry the warning prefix");
  check(!has(report, "device class does not match"), "a matching device class draws no class error");
  check(!has(report, "device space"), "a matching device space draws no space error");
}

/* The note's "shall ... have the same device space", compared as a device
   space rather than as a channel count. */
void testDeviceSpace()
{
  CIccInfo info;
  std::string rgb = info.GetColorSpaceSigName(icSigRgbData);
  std::string clr4 = info.GetColorSpaceSigName(icSig4colorData);
  std::string gray = info.GetColorSpaceSigName(icSigGrayData);

  std::string report;
  icValidateStatus rv = validate(icEmbeddedProfileTrue, report, icSig4colorData);
  check(has(report, ("device space " + clr4 + " does not match the parent profile's " + rgb).c_str()),
        "a 4CLR child under an RGB host: the device-space error names both spaces");
  check(!has(report, "fewer device channels"), "a wider child draws no channel-count error");
  check(rv >= icValidateCriticalError, "a different device space is a critical error");

  report.clear();
  rv = validate(icEmbeddedProfileTrue, report, icSigGrayData);
  check(has(report, ("device space " + gray + " does not match the parent profile's " + rgb).c_str()),
        "a GRAY child under an RGB host: the device-space error is drawn");
  check(has(report, "fewer device channels"), "a narrower child also draws the channel-count error");
  check(rv >= icValidateCriticalError, "a narrower child is a critical error");

  /* The child is a bare header, so its own validation already reports
     missing tags at critical level; only the device-space message is at
     issue here. */
  report.clear();
  validate(icEmbeddedProfileTrue, report, icSigRgbData);
  check(!has(report, "device space"), "the same device space draws no device-space error");
}

}  // namespace

int main()
{
  testFlags();
  testDeviceSpace();

  if (g_fail) {
    std::fprintf(stderr, "[embedded-profile-flags] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[embedded-profile-flags] PASS\n");
  return 0;
}
