/** @file
    File:       hdr-cmm-integration.cpp

    Contains:   CTest helper for the places the clause 8.10 HDR hint has to
                survive on its way from a caller to CIccXformMatrixTrcHdr, and
                for the CMM machinery that runs beside that xform.

    The HDR tone-mapping path is engaged by a CIccCreateHdrXformHint and by
    nothing else, so every route that builds an xform on a caller's behalf has
    to carry the hint through - and every route that silently drops it renders
    SDR with a success status.  Each case below is one of those routes:

    1. IccConnect CreateStandard with the source profile embedded in the image
       (the iccApplyProfiles -embedded path).  It builds its own hint manager
       rather than going through AddXformFromConfig.

    2. IccConnect CreateSearch (iccApplySearch, and a -cfg hdrTargetHeadroom).
       CIccCmmSearch rebuilds its sub-chains in Begin(), long after the
       caller's hint manager has gone.

    3. A profile opened lazily (OpenIccProfile) and added by reference.  The
       reference AddXform overload detaches the profile's IO before Begin(), so
       everything CIccXformMatrixTrcHdr::Begin() reads has to be loaded while
       the IO is still there.

    4. icHdrToneMapPreferLut on a profile with no LUT in the requested
       direction.  The HDR xform is then the only candidate, and its HAGC tag
       is the only descriptor left - it must be applied, not skipped.

    5. Black point compensation alongside the HDR hint.  CIccApplyBPC estimates
       black through CMMs of its own; they have to run the same HDR chain the
       xform applies, not the profile's SDR fallback LUTs.

    Exit codes:
      0 - every assertion passed
      1 - at least one assertion failed
    Copyright:  See ICC Software License
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

#include "IccConnect.h"
#include "IccCmmConfig.h"
#include "IccCmm.h"
#include "IccApplyBPC.h"
#include "IccProfile.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

static const char *kHagcDisplay = "Testing/HDR/HagcDisplay.icc";
static const char *kHdrDisplayMetadata = "Testing/HDR/HdrDisplayMetadata.icc";
static const char *kSdrDst = "Testing/sRGB_v4_ICC_preference.icc";

static int g_failures = 0;

static void check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "hdr-cmm-integration: PASS  %s\n", label);
  }
  else {
    std::fprintf(stderr, "hdr-cmm-integration: FAIL  %s\n", label);
    g_failures++;
  }
}

static double maxDiff(const icFloatNumber *a, const icFloatNumber *b, int n = 3)
{
  double worst = 0.0;
  for (int i = 0; i < n; i++) {
    double d = std::fabs((double)a[i] - (double)b[i]);
    if (!(d == d))
      return 1.0e30;  // NaN on either side is never a match
    if (d > worst)
      worst = d;
  }
  return worst;
}

static bool ReadFileBytes(const char *path, std::vector<unsigned char> &bytes)
{
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open())
    return false;
  f.seekg(0, std::ios::end);
  const std::streamoff len = f.tellg();
  if (len <= 0)
    return false;
  f.seekg(0, std::ios::beg);
  bytes.resize((size_t)len);
  f.read((char*)&bytes[0], len);
  return f.good() || f.eof();
}

/** Adds a heap-allocated HDR hint (and optionally BPC) to a manager, which owns them. */
static void AddHdrHints(CIccCreateXformHintManager &hints, icFloatNumber headroom,
                        icHdrToneMapPolicy policy, bool bBpc)
{
  CIccCreateHdrXformHint *pHdr = new CIccCreateHdrXformHint();
  pHdr->m_targetHeadroom = headroom;
  pHdr->m_nPolicy = policy;
  hints.AddHint(pHdr);
  if (bBpc)
    hints.AddHint(new CIccApplyBPCHint());
}

static CIccCfgProfilePtr MakeStage(const char *iccFile, icFloatNumber hdrHeadroom)
{
  CIccCfgProfilePtr stage(new CIccCfgProfile());
  stage->m_iccFile = iccFile ? iccFile : "";
  stage->m_intent = icRelativeColorimetric;
  stage->m_transform = icXformLutColor;
  stage->m_interpolation = icInterpTetrahedral;
  stage->m_hdrTargetHeadroom = hdrHeadroom;
  stage->m_hdrToneMap = icHdrToneMapAuto;
  return stage;
}

static bool ApplyConnect(CIccConnectCmm *pConn, const icFloatNumber *src, icFloatNumber *dst)
{
  if (!pConn || !pConn->GetCmm())
    return false;
  return pConn->GetCmm()->Apply(dst, src) == icCmmStatOk;
}

/****************************************************************************
 * 1. CreateStandard with an embedded source profile.
 ****************************************************************************/
static void TestEmbeddedStandard()
{
  std::vector<unsigned char> bytes;
  if (!ReadFileBytes(kHagcDisplay, bytes)) {
    check(false, "read HagcDisplay.icc for the embedded case");
    return;
  }

  const icFloatNumber src[3] = { 0.75f, 0.75f, 0.75f };

  auto build = [&](bool bEmbedded, icFloatNumber headroom) -> CIccConnectCmm* {
    CIccCfgProfileSequence seq;
    seq.m_profiles.push_back(MakeStage(bEmbedded ? "" : kHagcDisplay, headroom));
    seq.m_profiles.push_back(MakeStage(kSdrDst, 0.0f));
    std::string err;
    CIccConnectCmm *p = CIccConnectCmm::CreateStandard(seq,
      bEmbedded ? &bytes[0] : nullptr, bEmbedded ? (unsigned int)bytes.size() : 0, 1, &err);
    if (!p)
      std::fprintf(stderr, "hdr-cmm-integration: CreateStandard: %s\n", err.c_str());
    return p;
  };

  std::unique_ptr<CIccConnectCmm> fileHdr(build(false, 4.0f));
  std::unique_ptr<CIccConnectCmm> embHdr(build(true, 4.0f));
  std::unique_ptr<CIccConnectCmm> embSdr(build(true, 0.0f));

  check(fileHdr && embHdr && embSdr, "embedded: all three CreateStandard chains build");
  if (!fileHdr || !embHdr || !embSdr)
    return;

  CIccXform *pFirst = embHdr->GetCmm() ? embHdr->GetCmm()->GetFirstXform() : NULL;
  check(pFirst && pFirst->GetXformType() == icXformTypeMatrixTrcHdr,
        "embedded: -HDR builds the clause 8.10.2 xform for the embedded source profile");

  icFloatNumber a[16] = { 0 }, b[16] = { 0 }, c[16] = { 0 };
  bool ok = ApplyConnect(fileHdr.get(), src, a) && ApplyConnect(embHdr.get(), src, b) &&
            ApplyConnect(embSdr.get(), src, c);
  check(ok, "embedded: all three chains apply");
  if (!ok)
    return;

  std::fprintf(stdout, "  file+HDR [%.5f %.5f %.5f] emb+HDR [%.5f %.5f %.5f] emb [%.5f %.5f %.5f]\n",
               a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]);
  check(maxDiff(a, b) < 1.0e-5, "embedded: -HDR on the embedded profile matches the same profile named by path");
  check(maxDiff(b, c) > 1.0e-3, "embedded: -HDR on the embedded profile changes the rendering");
}

/****************************************************************************
 * 2. CreateSearch with the HDR hint on the source stage.
 ****************************************************************************/
static void TestSearch()
{
  const icFloatNumber src[3] = { 0.3f, 0.3f, 0.3f };

  auto buildSearch = [&](icFloatNumber headroom) -> CIccConnectCmm* {
    CIccCfgSearchApply cfg;
    cfg.m_profiles.push_back(MakeStage(kHagcDisplay, headroom));
    cfg.m_profiles.push_back(MakeStage(kSdrDst, 0.0f));
    std::string err;
    CIccConnectCmm *p = CIccConnectCmm::CreateSearch(cfg, nullptr, 0, 1, &err);
    if (!p)
      std::fprintf(stderr, "hdr-cmm-integration: CreateSearch: %s\n", err.c_str());
    return p;
  };
  auto buildStandard = [&](icFloatNumber headroom) -> CIccConnectCmm* {
    CIccCfgProfileSequence seq;
    seq.m_profiles.push_back(MakeStage(kHagcDisplay, headroom));
    seq.m_profiles.push_back(MakeStage(kSdrDst, 0.0f));
    std::string err;
    return CIccConnectCmm::CreateStandard(seq, nullptr, 0, 1, &err);
  };

  std::unique_ptr<CIccConnectCmm> searchHdr(buildSearch(4.0f));
  std::unique_ptr<CIccConnectCmm> searchSdr(buildSearch(0.0f));
  std::unique_ptr<CIccConnectCmm> stdHdr(buildStandard(4.0f));

  check(searchHdr && searchSdr && stdHdr, "search: CreateSearch and CreateStandard chains build");
  if (!searchHdr || !searchSdr || !stdHdr)
    return;

  icFloatNumber h[16] = { 0 }, s[16] = { 0 }, n[16] = { 0 };
  bool ok = ApplyConnect(searchHdr.get(), src, h) && ApplyConnect(searchSdr.get(), src, s) &&
            ApplyConnect(stdHdr.get(), src, n);
  check(ok, "search: all chains apply");
  if (!ok)
    return;

  std::fprintf(stdout, "  search+HDR [%.5f %.5f %.5f] search [%.5f %.5f %.5f] standard+HDR [%.5f %.5f %.5f]\n",
               h[0], h[1], h[2], s[0], s[1], s[2], n[0], n[1], n[2]);
  check(maxDiff(h, s) > 1.0e-3, "search: hdrTargetHeadroom changes the search CMM's rendering");
  check(maxDiff(h, n) < 2.0e-3, "search: the hinted search agrees with the hinted standard chain");
}

/****************************************************************************
 * 3. A lazily opened profile added by reference.
 ****************************************************************************/
static bool RunByReference(bool bLazy, icFloatNumber *dst, icStatusCMM &beginStatus)
{
  CIccProfile *p = bLazy ? OpenIccProfile(kHagcDisplay) : ReadIccProfile(kHagcDisplay);
  if (!p)
    return false;

  bool ok = false;
  {
    CIccCmm cmm(icSigUnknownData, icSigUnknownData, true);
    CIccCreateXformHintManager hints;
    AddHdrHints(hints, 4.0f, icHdrToneMapAuto, false);

    icStatusCMM st = cmm.AddXform(*p, icRelativeColorimetric, icInterpLinear, NULL,
                                  icXformLutColor, true, &hints);
    icStatusCMM st2 = cmm.AddXform(kSdrDst, icRelativeColorimetric);
    beginStatus = (st == icCmmStatOk && st2 == icCmmStatOk) ? cmm.Begin() : icCmmStatBad;
    if (beginStatus == icCmmStatOk) {
      const icFloatNumber src[3] = { 0.75f, 0.5f, 0.25f };
      ok = cmm.Apply(dst, src) == icCmmStatOk;
    }
  }
  delete p;
  return ok;
}

static void TestLazyReference()
{
  icFloatNumber lazy[16] = { 0 }, full[16] = { 0 };
  icStatusCMM stLazy = icCmmStatBad, stFull = icCmmStatBad;

  bool okFull = RunByReference(false, full, stFull);
  bool okLazy = RunByReference(true, lazy, stLazy);

  std::fprintf(stdout, "  by-reference Begin: read=%d lazy=%d\n", (int)stFull, (int)stLazy);
  check(okFull, "lazy: a fully read HDR profile added by reference begins and applies");
  check(okLazy, "lazy: a lazily opened HDR profile added by reference begins and applies");
  if (okFull && okLazy)
    check(maxDiff(lazy, full) < 1.0e-6, "lazy: lazily opened and fully read profiles render identically");
}

/****************************************************************************
 * 4. icHdrToneMapPreferLut with no LUT in the requested direction.
 ****************************************************************************/
static CIccXformMatrixTrcHdr *CreateHdrXform(const CIccProfile &prof, bool bInput,
                                             icHdrToneMapPolicy policy, icFloatNumber headroom,
                                             CIccXform *&pOwner)
{
  pOwner = NULL;
  CIccCreateXformHintManager hints;
  AddHdrHints(hints, headroom, policy, false);

  CIccProfile *pCopy = new CIccProfile(prof);
  CIccXform *pX = CIccXform::Create(pCopy, bInput, icRelativeColorimetric, icInterpLinear,
                                    NULL, icXformLutColor, true, &hints);
  if (!pX)
    return NULL;
  pOwner = pX;
  if (pX->GetXformType() != icXformTypeMatrixTrcHdr || pX->Begin() != icCmmStatOk)
    return NULL;
  return (CIccXformMatrixTrcHdr*)pX;
}

static bool ApplyXform(CIccXform *pX, const icFloatNumber *src, icFloatNumber *dst)
{
  icStatusCMM st;
  CIccApplyXform *pApply = pX->GetNewApply(st);
  if (!pApply || st != icCmmStatOk) {
    delete pApply;
    return false;
  }
  pX->Apply(pApply, dst, src);
  delete pApply;
  return true;
}

static void TestPreferLutWithoutLut()
{
  CIccProfile *p = ReadIccProfile(kHagcDisplay);
  if (!p) {
    check(false, "prefer-lut: read HagcDisplay.icc");
    return;
  }

  // An Input-class HDR Profile needs no BToA0Tag (8.10.6 requires the pair for
  // Display class only), so as a destination it has no LUT in that direction.
  p->m_Header.deviceClass = icSigInputClass;
  p->DeleteTag(icSigBToA0Tag);

  CIccXform *pOwnLut = NULL, *pOwnHagc = NULL;
  CIccXformMatrixTrcHdr *pLut = CreateHdrXform(*p, false, icHdrToneMapPreferLut, 1.0f, pOwnLut);
  CIccXformMatrixTrcHdr *pHagc = CreateHdrXform(*p, false, icHdrToneMapPreferHagc, 1.0f, pOwnHagc);

  check(pLut != NULL, "prefer-lut: with no BToA0Tag the destination still takes the HDR xform");
  check(pHagc != NULL, "prefer-lut: PreferHagc control builds");

  if (pLut && pHagc) {
    check(pLut->IsToneMapping(), "prefer-lut: the HAGC tag, the only descriptor left, is applied");

    const icFloatNumber pcs[3] = { 0.30f, 0.32f, 0.26f };
    icFloatNumber a[16] = { 0 }, b[16] = { 0 };
    bool ok = ApplyXform(pLut, pcs, a) && ApplyXform(pHagc, pcs, b);
    check(ok, "prefer-lut: both xforms apply");
    if (ok) {
      std::fprintf(stdout, "  PreferLut [%.5f %.5f %.5f] PreferHagc [%.5f %.5f %.5f]\n",
                   a[0], a[1], a[2], b[0], b[1], b[2]);
      check(maxDiff(a, b) < 1.0e-6, "prefer-lut: renders the same as PreferHagc when no LUT exists");
    }
  }

  delete pOwnLut;
  delete pOwnHagc;
  delete p;
}

/****************************************************************************
 * 5. Black point compensation with the HDR hint.
 ****************************************************************************/

// Runs src->dst at relative colorimetric with the HDR (and BPC) hint on the HDR
// profile's end of the chain.  The CMM takes ownership of the profile copies.
static bool RunBpcChain(const CIccProfile &hdr, bool bHdrIsSource, bool bBpc,
                        const icFloatNumber *src, icFloatNumber *dst, icStatusCMM &status)
{
  CIccCmm cmm(icSigUnknownData, icSigUnknownData, true);
  CIccCreateXformHintManager hdrHints, sdrHints;
  AddHdrHints(hdrHints, 4.0f, icHdrToneMapAuto, bBpc);
  if (bBpc)
    sdrHints.AddHint(new CIccApplyBPCHint());

  status = icCmmStatBad;
  CIccProfile *pSdr = ReadIccProfile(kSdrDst);
  if (!pSdr)
    return false;

  icStatusCMM s1, s2;
  if (bHdrIsSource) {
    s1 = cmm.AddXform(new CIccProfile(hdr), icRelativeColorimetric, icInterpLinear, NULL,
                      icXformLutColor, true, &hdrHints);
    s2 = cmm.AddXform(pSdr, icRelativeColorimetric, icInterpLinear, NULL, icXformLutColor, true,
                      &sdrHints);
  }
  else {
    s1 = cmm.AddXform(pSdr, icRelativeColorimetric, icInterpLinear, NULL, icXformLutColor, true,
                      &sdrHints);
    s2 = cmm.AddXform(new CIccProfile(hdr), icRelativeColorimetric, icInterpLinear, NULL,
                      icXformLutColor, true, &hdrHints);
  }
  if (s1 != icCmmStatOk || s2 != icCmmStatOk)
    return false;

  status = cmm.Begin();
  if (status != icCmmStatOk)
    return false;
  return cmm.Apply(dst, src) == icCmmStatOk;
}

// Lifts the first entry of each of the lutAtoBType's B curves, so the SDR
// fallback's device black no longer maps to PCS black.  The HDR chain never
// reads this tag.
static bool LiftAToB0Black(CIccProfile &prof, icFloatNumber lift)
{
  CIccTag *pTag = prof.FindTag(icSigAToB0Tag);
  if (!pTag || !pTag->IsMBBType())
    return false;
  LPIccCurve *pCurves = ((CIccMBB*)pTag)->GetCurvesB();
  if (!pCurves)
    return false;
  for (int i = 0; i < 3; i++) {
    if (!pCurves[i] || pCurves[i]->GetType() != icSigCurveType)
      return false;
    CIccTagCurve *pCurve = (CIccTagCurve*)pCurves[i];
    if (pCurve->GetSize() < 2)
      return false;
    (*pCurve)[0] = lift;
  }
  return true;
}

static void TestBpc()
{
  CIccProfile *pOrig = ReadIccProfile(kHdrDisplayMetadata);
  CIccProfile *pLifted = ReadIccProfile(kHdrDisplayMetadata);
  CIccProfile *pNoLut = ReadIccProfile(kHdrDisplayMetadata);
  if (!pOrig || !pLifted || !pNoLut) {
    check(false, "bpc: read HdrDisplayMetadata.icc");
    delete pOrig; delete pLifted; delete pNoLut;
    return;
  }

  check(LiftAToB0Black(*pLifted, 0.1f), "bpc: lift the SDR AToB0Tag's black");
  pNoLut->DeleteTag(icSigAToB0Tag);
  pNoLut->DeleteTag(icSigBToA0Tag);

  const icFloatNumber srcDev[3] = { 0.3f, 0.3f, 0.3f };

  for (int dir = 0; dir < 2; dir++) {
    bool bHdrIsSource = (dir == 0);
    const char *side = bHdrIsSource ? "source" : "destination";
    char label[256];

    icFloatNumber o[16] = { 0 }, l[16] = { 0 }, n[16] = { 0 }, nb[16] = { 0 };
    icStatusCMM so, sl, sn, snb;
    bool okO = RunBpcChain(*pOrig, bHdrIsSource, true, srcDev, o, so);
    bool okL = RunBpcChain(*pLifted, bHdrIsSource, true, srcDev, l, sl);
    bool okN = RunBpcChain(*pNoLut, bHdrIsSource, true, srcDev, n, sn);
    bool okNb = RunBpcChain(*pNoLut, bHdrIsSource, false, srcDev, nb, snb);

    std::fprintf(stdout, "  BPC HDR %s: orig=%d [%.5f] lifted=%d [%.5f] nolut=%d [%.5f] nolut-noBPC=%d [%.5f]\n",
                 side, (int)so, o[0], (int)sl, l[0], (int)sn, n[0], (int)snb, nb[0]);

    snprintf(label, sizeof(label), "bpc (HDR %s): BPC chains with the original and lifted AToB0 begin", side);
    check(okO && okL, label);
    snprintf(label, sizeof(label), "bpc (HDR %s): BPC does not depend on the SDR fallback LUT's black", side);
    check(okO && okL && maxDiff(o, l) < 1.0e-5, label);
    snprintf(label, sizeof(label), "bpc (HDR %s): BPC works on an HDR Profile with no LUT pair", side);
    check(okN && okO && maxDiff(n, o) < 1.0e-5, label);
    (void)okNb;
  }

  delete pOrig;
  delete pLifted;
  delete pNoLut;
}

/****************************************************************************
 * 6. The -cfg policy name matches as -HDRMAP does.
 *
 * The command line matched the policy name without regard to case and the
 * JSON reader did not, so "HAGC" worked as -HDRMAP HAGC and refused the whole
 * profileSequence in a -cfg file.
 ****************************************************************************/
static void TestPolicyNameCase()
{
  CIccCfgProfile upper;
  check(upper.fromJson(json::parse("{\"iccFile\":\"x.icc\",\"hdrToneMap\":\"HAGC\"}"), true) &&
        upper.m_hdrToneMap == icHdrToneMapPreferHagc,
        "config: a -cfg hdrToneMap name matches without regard to case");

  CIccCfgProfile unknown;
  check(!unknown.fromJson(json::parse("{\"iccFile\":\"x.icc\",\"hdrToneMap\":\"hagcx\"}"), true),
        "config: an unknown hdrToneMap name is still refused");
}

int main()
{
  // The HDR fixtures are generated by the iccdev_profiles CTest fixture; without
  // them there is nothing to exercise, which is a skip rather than a pass.
  {
    std::ifstream probe(kHagcDisplay, std::ios::binary);
    if (!probe.is_open()) {
      std::fprintf(stderr, "hdr-cmm-integration: SKIP  %s not generated\n", kHagcDisplay);
      return 77;
    }
  }

  TestEmbeddedStandard();
  TestSearch();
  TestLazyReference();
  TestPreferLutWithoutLut();
  TestBpc();
  TestPolicyNameCase();

  if (g_failures) {
    std::fprintf(stderr, "hdr-cmm-integration: %d assertion(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "hdr-cmm-integration: all assertions passed\n");
  return 0;
}
