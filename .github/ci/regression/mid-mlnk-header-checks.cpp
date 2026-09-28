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

#include <cstdio>
#include <string>

#include <cstring>

#include "IccProfile.h"
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

  if (g_fail) {
    std::fprintf(stderr, "[mid-mlnk-header-checks] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[mid-mlnk-header-checks] all checks passed\n");
  return 0;
}
