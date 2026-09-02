/*
    File:       BenchCases.cpp

    Contains:   Built-in case table and profile path resolution for iccBenchApply

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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "BenchCases.h"

// The built-in case table.
//
// Chains, not single profiles, because not every profile round-trips: the
// six-channel SpecRef profile is scnr class and can only appear first, as can
// the v2 grayTRC fixture.
//
// Intent 1 is relative colorimetric throughout except pcs-abs, which uses 3
// (absolute) deliberately: absolute forces m_bAdjustPCS, so CIccPcsXform
// pushes the PCS-adjustment steps at the connection, and the pcs-abs minus
// pcs-rel delta isolates PCS-adjustment cost.
//
// That delta only exists if the two profiles disagree about the media white
// point. The pcs pair is deliberately v2RgbLut8 (D50) -> sRGB_D65_MAT (D65) for
// that reason: the obvious choice of reusing the lut-3d-tetra pair does not work,
// because both Testing/V2 LUT profiles carry a mediaWhitePointTag of exactly
// D50, which makes the absolute adjustment an identity and produces a checksum
// byte-identical to the relative run. Verified: with the D50/D50 pair both
// intents give 0xa805161a, while the D50/D65 pair gives 0xef5f2056 relative and
// 0xf03c2078 absolute.
//
// Each case's resolved chain was confirmed by running it, and the names describe
// what the chain actually builds rather than what the profile is called:
//
//   matrix-trc    MatrixTRC -> PCS -> Mpe
//   lut-3d-tetra  3DLut -> PCS -> 3DLut
//   spectral-6ch  Mpe -> PCS -> 3DLut
//   monochrome    Monochrome -> MatrixTRC          (no PCS step: XYZ both sides)
//   mono-lab      Monochrome -> PCS -> 3DLut        (Lab PCS: reaches XyzToLab)
//   mpe-calc      Mpe -> PCS -> Mpe
//   mpe-tonemap   Mpe -> Mpe                       (no PCS step)
//   pcs-rel       3DLut -> PCS -> Mpe
//   pcs-abs       3DLut -> PCS -> Mpe               (absolute intent)
//
// monochrome and mono-lab differ only in the PCS encoding of the source
// profile, and both are needed: CIccXformMonochrome::Apply takes a different
// branch for a Lab PCS, and only that branch reaches XyzToLab and its three cube
// roots. With the XYZ fixture alone, a change to that branch is unmeasured.
//
// spectral-6ch is deliberately NOT called lut-nd. SixChanCameraRef is MPE-based,
// so it resolves to an Mpe xform and never reaches CIccXformNDLut: Interp5d,
// Interp6d, and InterpND all have no coverage here. Sourcing a tracked LUT-based
// profile with >=5 channels is a separate effort; until then the table must not
// imply coverage it does not have.
static const struct {
  const char *name;
  int         interp;
  const char *chain;   // "path:intent|path:intent"
} kCases[] = {
  { "matrix-trc",   1, "V2/v2RgbMatrixTRC.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "lut-3d-tetra", 1, "V2/v2RgbLut8.icc:1|V2/v2CmykLut16.icc:1" },
  { "spectral-6ch", 1, "SpecRef/SixChanCameraRef.icc:1|V2/v2CmykLut16.icc:1" },
  { "monochrome",   1, "V2/v2GrayTRC.icc:1|V2/v2RgbMatrixTRC.icc:1" },
  { "mono-lab",     1, "V2/v2GrayTRCLab.icc:1|V2/v2CmykLut16.icc:1" },
  { "mpe-calc",     1, "Calc/srgbCalcTest.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "mpe-tonemap",  1, "Display/Rec2100HlgFull.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "pcs-rel",      1, "V2/v2RgbLut8.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "pcs-abs",      1, "V2/v2RgbLut8.icc:3|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:3" },
};

static std::string g_sSourceRoot;
static std::string g_sBuildRoot;
static bool        g_bRootsSet = false;

void icBenchSetRoots(const char *szSourceRoot, const char *szBuildRoot)
{
  g_sSourceRoot = szSourceRoot ? szSourceRoot : "";
  g_sBuildRoot  = szBuildRoot  ? szBuildRoot  : "";
  g_bRootsSet   = true;
}

static void EnsureRoots()
{
  if (g_bRootsSet)
    return;

  const char *szSrc = getenv("ICCDEV_BENCH_SOURCE_ROOT");
  const char *szBld = getenv("ICCDEV_BENCH_BUILD_ROOT");

  // "." covers the common case of running from the repo root by hand.
  icBenchSetRoots(szSrc && *szSrc ? szSrc : ".", szBld ? szBld : "");
}

// Two roots, not one. iccdev.create-profiles writes generated profiles into the
// source tree on POSIX and iccdev.windows-create-profiles writes them into the
// build tree (see the note at Build/Cmake/Testing/CMakeLists.txt:1746), so a
// single hardcoded root works on exactly one platform.
// #2254: each root is tried two ways -- as a directory that CONTAINS Testing/,
// and as a Testing tree itself. One hardcoded "/Testing/" segment was wrong for
// both platforms in different ways.
//
// On Windows the generated profiles do not live under <build>/Testing at all.
// iccdev.windows-create-profiles copies the source Testing/ tree to
// ${ICCDEV_TEST_OUTDIR}/windows-testing and generates its 135 profiles there
// (Build/Cmake/Testing/CMakeLists.txt:5238, RunWindowsBatchTest.cmake:184-194),
// so with ICCDEV_BENCH_BUILD_ROOT=${CMAKE_BINARY_DIR} the probe looked in
// <build>/Testing -- the CTest scratch directory -- and missed all nine cases.
// iccdev.apply-throughput has therefore been passing on every Windows leg
// without measuring anything: 0.01s on run 32563985818, against 6.62s for the
// fixture that generated the profiles it never found. A directory that is
// already a Testing tree can now be named directly.
//
// On POSIX the same second form fixes the other reported case: running the tool
// from inside Testing/, where the default root of "." made every path resolve
// as ./Testing/Testing/<rel> and all nine cases SKIP.
//
// The forms are ordered containing-root first so that a repository root keeps
// resolving exactly as it did; the direct form is only reached when the first
// misses.
bool icBenchResolveProfile(const std::string &rel, std::string &abs)
{
  EnsureRoots();

  const std::string *roots[2] = { &g_sSourceRoot, &g_sBuildRoot };
  for (int i = 0; i < 2; i++) {
    if (roots[i]->empty())
      continue;

    const std::string cands[2] = { *roots[i] + "/Testing/" + rel,
                                   *roots[i] + "/" + rel };
    for (int j = 0; j < 2; j++) {
      FILE *f = fopen(cands[j].c_str(), "rb");
      if (f) {
        fclose(f);
        abs = cands[j];
        return true;
      }
    }
  }
  return false;
}

const std::vector<BenchCase> &icBenchBuiltinCases()
{
  static std::vector<BenchCase> cases;
  if (!cases.empty())
    return cases;

  const size_t nCases = sizeof(kCases) / sizeof(kCases[0]);
  for (size_t i = 0; i < nCases; i++) {
    BenchCase bc;
    bc.name          = kCases[i].name;
    bc.interpolation = kCases[i].interp;

    // Split "path:intent|path:intent" on '|', then each element on its last ':'
    // -- last, not first, so a Windows drive letter in a future absolute path
    // would not be mistaken for the separator.
    const std::string spec = kCases[i].chain;
    size_t pos = 0;
    while (pos <= spec.size()) {
      size_t bar = spec.find('|', pos);
      const std::string item =
        spec.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);

      const size_t colon = item.rfind(':');
      if (colon != std::string::npos) {
        BenchChainLink link;
        link.profile       = item.substr(0, colon);
        link.encodedIntent = atoi(item.c_str() + colon + 1);
        bc.chain.push_back(link);
      }

      if (bar == std::string::npos)
        break;
      pos = bar + 1;
    }

    cases.push_back(bc);
  }

  return cases;
}
