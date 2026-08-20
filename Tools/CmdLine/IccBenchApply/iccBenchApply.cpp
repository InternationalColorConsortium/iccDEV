/*
    File:       iccBenchApply.cpp

    Contains:   Console app that measures apply-path throughput for a profile chain

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

//////////////////////////////////////////////////////////////////////
// HISTORY:
//
// -Initial implementation by Claude Opus 5 8-20-2026
//
//////////////////////////////////////////////////////////////////////


#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccUtil.h"

static icUInt32Number g_nPixels  = 1048576;
static int            g_nRepeats = 7;

static void Usage()
{
  printf("iccBenchApply built with IccProfLib version " ICCPROFLIBVER "\n\n");
  printf("Usage: iccBenchApply {options} interpolation"
         " {profile_path rendering_intent {-PCC pcc_path}}...\n\n");
  printf("  interpolation      0 = Linear, 1 = Tetrahedral\n");
  printf("  rendering_intent   0..3, plus +1000 / +10000 modifiers"
         " (as iccApplyToLink)\n\n");
  printf("  -pixels N          pixels per buffer      (default 1048576)\n");
  printf("  -repeats N         timed repeats per case (default 7)\n");
  printf("\nNo timing threshold is ever asserted. This tool records.\n");
}

// Verbatim from iccApplyToLink.cpp:820 -- same parsing contract, so the two
// tools accept and reject exactly the same argument strings.
static bool ParseIntArg(const char *arg, int minValue, int maxValue, int &value)
{
  char *end = NULL;
  long parsed;

  if (!arg || !*arg)
    return false;

  errno = 0;
  parsed = strtol(arg, &end, 10);

  if (errno == ERANGE || end == arg || *end != '\0' ||
      parsed < minValue || parsed > maxValue ||
      parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }

  value = (int)parsed;
  return true;
}

// Names for the icXformType values in IccCmm.h:177-187. PCS is named explicitly
// rather than falling through to "Unknown" because the CIccPcsXform entries
// Begin() inserts between profiles are the rows this harness exists to expose.
static const char *XformTypeName(icXformType t)
{
  switch (t) {
    case icXformTypeMatrixTRC:   return "MatrixTRC";
    case icXformType3DLut:       return "3DLut";
    case icXformType4DLut:       return "4DLut";
    case icXformTypeNDLut:       return "NDLut";
    case icXformTypeNamedColor:  return "NamedColor";
    case icXformTypeMpe:         return "Mpe";
    case icXformTypeMonochrome:  return "Monochrome";
    case icXformTypePCS:         return "PCS";
    default:                     return "Unknown";
  }
}

// Collects the resolved chain. CIccCmm::IterateXforms (IccCmm.h:1814) is the
// public hook, and it reports the chain after Begin() has inserted the PCS
// xforms, which is the only way to see them from outside the library.
class CChainReporter : public IXformIterator
{
public:
  virtual void iterate(const CIccXform *pXform)
  {
    if (!pXform)
      return;
    // IterateXforms hands out const pointers, but GetNewApply() is non-const
    // and allocates a per-apply object without mutating the xform's own state.
    // The CMM owns these and outlives this tool's use of them.
    m_xforms.push_back(const_cast<CIccXform *>(pXform));
  }

  std::vector<CIccXform *> m_xforms;
};

int main(int argc, const char *argv[])
{
  if (argc < 3) {
    Usage();
    return 1;
  }

  int nArg = 1;

  while (nArg < argc && argv[nArg][0] == '-') {
    if (!stricmp(argv[nArg], "-pixels")) {
      int n;
      if (nArg + 1 >= argc || !ParseIntArg(argv[nArg + 1], 1, 1 << 26, n)) {
        printf("Invalid -pixels value: expected 1..67108864\n");
        return 1;
      }
      g_nPixels = (icUInt32Number)n;
      nArg += 2;
    }
    else if (!stricmp(argv[nArg], "-repeats")) {
      int n;
      if (nArg + 1 >= argc || !ParseIntArg(argv[nArg + 1], 1, 1000, n)) {
        printf("Invalid -repeats value: expected 1..1000\n");
        return 1;
      }
      g_nRepeats = n;
      nArg += 2;
    }
    else {
      printf("Unknown option '%s'\n", argv[nArg]);
      Usage();
      return 1;
    }
  }

  int nInterp;
  if (nArg >= argc || !ParseIntArg(argv[nArg], 0, 1, nInterp)) {
    printf("Invalid interpolation: expected 0 (linear) or 1 (tetrahedral)\n");
    return 1;
  }
  nArg++;

  if (nArg >= argc) {
    printf("No profiles given: a chain needs at least one"
           " 'profile_path rendering_intent' pair\n");
    return 1;
  }

  CIccCmm theCmm(icSigUnknownData, icSigUnknownData, true);
  int nProfiles = 0;

  while (nArg < argc) {
    const char *szProfile = argv[nArg];

    if (nArg + 1 >= argc) {
      printf("Profile '%s' has no rendering intent\n", szProfile);
      return 1;
    }

    int nEncoded;
    if (!ParseIntArg(argv[nArg + 1], INT_MIN, INT_MAX, nEncoded)) {
      printf("Invalid rendering intent '%s': expected an integer code\n",
             argv[nArg + 1]);
      return 1;
    }
    nArg += 2;

    // Same decode as iccApplyToLink.cpp:1054-1059.
    bool bUseSubProfile = (nEncoded / 1000) > 0;
    int nIntent = nEncoded % 1000;
    nIntent = nIntent % 100;
    int nType = abs(nIntent) / 10;
    nIntent = nIntent % 10;

    bool bUseD2BxB2DxTags = true;
    if (nType == 1) {
      nType = 0;
      bUseD2BxB2DxTags = false;
    }

    if (nIntent < (int)icPerceptual || nIntent > (int)icAbsoluteColorimetric) {
      printf("Invalid rendering intent '%s': decoded intent out of range\n",
             argv[nArg - 1]);
      return 1;
    }

    CIccProfile *pPccProfile = NULL;
    if (nArg + 1 < argc && !stricmp(argv[nArg], "-PCC")) {
      pPccProfile = ReadIccProfile(argv[nArg + 1]);
      if (!pPccProfile) {
        printf("Unable to read PCC profile '%s'\n", argv[nArg + 1]);
        return 1;
      }
      nArg += 2;
    }

    icStatusCMM stat = theCmm.AddXform(szProfile,
                                       (icRenderingIntent)nIntent,
                                       nInterp ? icInterpTetrahedral
                                               : icInterpLinear,
                                       pPccProfile,
                                       (icXformLutType)nType,
                                       bUseD2BxB2DxTags,
                                       NULL,
                                       bUseSubProfile);
    if (stat != icCmmStatOk) {
      printf("Unable to add '%s' to the chain: %s\n",
             szProfile, CIccCmm::GetStatusText(stat));
      return 1;
    }
    nProfiles++;
  }

  icStatusCMM stat = theCmm.Begin();
  if (stat != icCmmStatOk) {
    printf("Begin() failed: %s\n", CIccCmm::GetStatusText(stat));
    return 1;
  }

  printf("chain: %d profile(s), %d source sample(s) -> %d destination sample(s)\n",
         nProfiles, (int)theCmm.GetSourceSamples(),
         (int)theCmm.GetDestSamples());

  CChainReporter reporter;
  theCmm.IterateXforms(&reporter);

  printf("resolved transforms:\n");
  for (size_t i = 0; i < reporter.m_xforms.size(); i++) {
    printf("  %2d  %-12s\n", (int)i,
           XformTypeName(reporter.m_xforms[i]->GetXformType()));
  }

  return 0;
}
