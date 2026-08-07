/*
    File:       iccHdrFallback.cpp

    Contains:   Console app that bakes an HDR profile's tone-mapped SDR
                rendering into an A2B0/B2A0 pair, so that a CMM implementing
                none of ICC.1 clause 8.10 still reproduces it

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * Copyright (c) International Color Consortium.
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
// -Initial implementation of the A2B0/B2A0 HDR fallback bake
//
//////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "IccHdrBake.h"
#include "IccHdrProfile.h"
#include "IccProfLibVer.h"
#include "IccProfile.h"
#include "IccUtil.h"
#include "../IccCmdLineUtil.h"

static void usage()
{
  printf("Usage: iccHdrFallback [options] src_profile dst_profile\n\n");
  printf("Bakes the SDR rendering an HDR profile describes into a lutAToBType\n");
  printf("AToB0Tag and a lutBToAType BToA0Tag, evaluated at a target headroom of\n");
  printf("1.0.  Every other tag is copied unchanged, the headroomAdaptiveGainCurveTag\n");
  printf("included, so an HDR-aware CMM keeps evaluating the curve dynamically and\n");
  printf("only a CMM that cannot reaches for the baked pair.\n\n");
  printf("Options:\n");
  printf("  -grid n     CLUT grid points per axis (2..255, default %d)\n", icHdrBakeDefaultGridPoints);
  printf("  -curve n    entries per A curve (2..%d, default %d)\n",
         icHdrBakeMaxCurveSize, icHdrBakeDefaultCurveSize);
  printf("  -hlggamma g HLG OOTF system gamma (default %g)\n", (double)icHlgDefaultGamma);
  printf("  -hlgpeak L  HLG nominal display peak luminance in cd/m^2 (default %g)\n",
         (double)icHlgDefaultPeakLuminance);
  printf("  -v44        rewrite the header version as 4.4, for consumers that reject\n");
  printf("              the version an HDR Profile carries.  The profile then no\n");
  printf("              longer classifies under clause 8.10.1.\n");
  printf("\nBuilt with IccProfLib version " ICCPROFLIBVER "\n");
}

/** Parse an unsigned decimal argument, refusing anything with trailing text so
 * that a typo becomes an error rather than a silently different table size. */
static bool parseUInt(const char *szArg, unsigned long nMin, unsigned long nMax,
                      unsigned long &nValue)
{
  char *szEnd = NULL;

  if (!szArg || !*szArg)
    return false;

  unsigned long v = strtoul(szArg, &szEnd, 10);

  if (!szEnd || *szEnd || v < nMin || v > nMax)
    return false;

  nValue = v;

  return true;
}

/** Parse a positive floating-point argument, on the same terms. */
static bool parsePositive(const char *szArg, double &value)
{
  char *szEnd = NULL;

  if (!szArg || !*szArg)
    return false;

  double v = strtod(szArg, &szEnd);

  if (!szEnd || *szEnd || !(v > 0.0))
    return false;

  value = v;

  return true;
}

/** Report what the bake was made from, so that a run whose output looks wrong
 * can be told apart from a profile whose metadata made it so - the resolved
 * reference white in particular is a defaulted value more often than not. */
static void describeSource(const CIccProfile *pIcc, const CIccHdrBaker &baker)
{
  icHdrProfileInfo info;

  if (!icGetHdrProfileInfo(pIcc, info))
    return;

  const icChar *szTransfer = icGetHdrTransferName(info.nTransferCharacteristics);

  printf("  transfer characteristic : %u (%s)\n",
         (unsigned)info.nTransferCharacteristics, szTransfer ? szTransfer : "unsupported");
  printf("  content reference white : %g cd/m^2 (%s)\n",
         (double)info.contentReferenceWhite,
         info.bContentReferenceWhiteFromProfile ? "from the profile" : "clause 8.10.4 default");
  printf("  gain curve              : %s\n",
         !info.bHasHagc ? "none - baking the transfer and an SDR clamp only" :
         baker.IsToneMapIdentity() ? "present, identity at this target" : "present");

  if (baker.UsesDerivedSlopes()) {
    // The slope rule behind this flag is a reconstruction of a SMPTE clause
    // that is not in hand, so a profile that leans on it deserves to be named.
    printf("  NOTE: a contributing gain curve carries no slope angles; its slopes were\n");
    printf("        derived by the PCHIP rule of icHagcDerivePchipSlopes()\n");
  }
}

int main(int argc, char *argv[])
{
  icHdrBakeParams params;
  icHdrBakeParamsInit(params);

  int nArg = 1;

  while (nArg < argc && argv[nArg][0] == '-') {
    const char *szOpt = argv[nArg];
    unsigned long nValue = 0;
    double fValue = 0.0;

    if (!strcmp(szOpt, "-v44")) {
      params.nVersionPolicy = icHdrBakeVersionV4_4;
      nArg++;
      continue;
    }

    if (nArg + 1 >= argc) {
      printf("Option %s needs a value\n\n", icSanitizeConsoleText(szOpt).c_str());
      usage();
      return 1;
    }

    const char *szValue = argv[nArg + 1];

    if (!strcmp(szOpt, "-grid")) {
      if (!parseUInt(szValue, 2, 255, nValue)) {
        printf("Invalid grid size '%s'\n\n", icSanitizeConsoleText(szValue).c_str());
        usage();
        return 1;
      }

      params.nGridPoints = (icUInt8Number)nValue;
    }
    else if (!strcmp(szOpt, "-curve")) {
      // Same bound the baker enforces, named from the one place that defines
      // it so the tool and the library cannot drift apart.
      if (!parseUInt(szValue, 2, icHdrBakeMaxCurveSize, nValue)) {
        printf("Invalid curve size '%s'\n\n", icSanitizeConsoleText(szValue).c_str());
        usage();
        return 1;
      }

      params.nCurveSize = (icUInt32Number)nValue;
    }
    else if (!strcmp(szOpt, "-hlggamma")) {
      if (!parsePositive(szValue, fValue)) {
        printf("Invalid HLG gamma '%s'\n\n", icSanitizeConsoleText(szValue).c_str());
        usage();
        return 1;
      }

      params.hlgGamma = (icFloatNumber)fValue;
    }
    else if (!strcmp(szOpt, "-hlgpeak")) {
      if (!parsePositive(szValue, fValue)) {
        printf("Invalid HLG peak luminance '%s'\n\n", icSanitizeConsoleText(szValue).c_str());
        usage();
        return 1;
      }

      params.hlgPeakLuminance = (icFloatNumber)fValue;
    }
    else {
      printf("Unknown option %s\n\n", icSanitizeConsoleText(szOpt).c_str());
      usage();
      return 1;
    }

    nArg += 2;
  }

  if (argc - nArg != 2) {
    usage();
    return argc <= 1 ? 0 : 1;
  }

  const char *szSrc = argv[nArg];
  const char *szDst = argv[nArg + 1];

  // Read rather than open: every tag has to be in memory before the profile is
  // written back out, and an opened profile carries a tag directory whose
  // entries load lazily from an IO object that is about to be the destination.
  CIccProfile *pIcc = ReadIccProfile(szSrc);

  if (!pIcc) {
    printf("Unable to read '%s'\n", icSanitizeConsoleText(szSrc).c_str());
    return 2;
  }

  CIccHdrBaker baker;

  if (!baker.Init(pIcc, &params)) {
    printf("Cannot bake '%s': %s\n", icSanitizeConsoleText(szSrc).c_str(),
           baker.GetUnsupportedReason());
    delete pIcc;
    return 3;
  }

  printf("Baking '%s'\n", icSanitizeConsoleText(szSrc).c_str());
  describeSource(pIcc, baker);
  printf("  tables                  : %u^3 CLUT, %u-entry curves\n",
         (unsigned)params.nGridPoints, (unsigned)params.nCurveSize);

  const icChar *szReason = NULL;

  if (!icAddHdrFallbackTags(pIcc, &params, &szReason)) {
    printf("Unable to bake the fallback tags: %s\n", szReason ? szReason : "unknown reason");
    delete pIcc;
    return 4;
  }

  if (!SaveIccProfile(szDst, pIcc)) {
    printf("Unable to write '%s'\n", icSanitizeConsoleText(szDst).c_str());
    delete pIcc;
    return 5;
  }

  printf("Wrote '%s'\n", icSanitizeConsoleText(szDst).c_str());

  delete pIcc;

  return 0;
}
