// Regression test for iccPawgReport's Q1 (round-trip) and Q3 (smoothness)
// quality metrics in IccQualityMetrics.h.  Guards the fix for #1455.
//
// Defect: both metrics build their CMM steps through
//   iccquality::begin_profile_cmm(), which called `cmm.AddXform(*pIcc)` with the
//   default nIntent = icUnknownIntent.  icUnknownIntent resolves to the profile's
//   *header* rendering intent -- Perceptual for most output profiles -- so Q1
//   round-tripped through the perceptual A2B0/B2A0 pair.  Perceptual rendering
//   deliberately compresses gamut, so A2B o B2A is not a clean inverse and the
//   round-trip dE was spuriously large (e.g. CMYK-3DLUTs.icc Q1 max 22.6 dE ->
//   a FALSE FAIL verdict; CRPC6 max ~21 vs ~2 under relative).
//
//   The fix defaults begin_profile_cmm to RELATIVE COLORIMETRIC (the intent
//   iccRoundTrip / CIccEvalCompare already use), which is the correct space in
//   which to assess invertibility / smoothness.
//
// Strategy: build a profile whose A2B0 (perceptual) and A2B1 (relative) tables
// differ, with a Perceptual header intent.  begin_profile_cmm's default forward
// must now match the *relative* (A2B1) output, not the *perceptual* (A2B0) one.
// The perceptual/relative reference transforms are built with a raw CIccCmm at
// an explicit intent (independent of the fix), so the test compiles against the
// pre-fix header too and fails at the assertion, not at build.
//
// Mutation-verified: against the pre-fix header the default matches perceptual
// (the two guarded assertions flip).
//
// Exit code 0 = pass, 1 = a guarded regression reappeared.

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include "IccQualityMetrics.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace iccquality;

static int g_failures = 0;

static void check(bool cond, const char *msg) {
  if (cond) {
    std::printf("ok:   %s\n", msg);
  } else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

// A minimal lut16 RGB->XYZ tag with a *constant* 2x2x2 CLUT (every node output =
// v) and identity in/out curves.  The forward output is ~v for any input, so two
// such tags with different v are trivially separable -- enough to tell which
// A2Bn (and therefore which rendering intent) a CMM selected.
static CIccTagLut16 *make_const_lut16(float v) {
  CIccTagLut16 *pLut = new CIccTagLut16;
  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigRgbData, icSigXYZData);

  LPIccCurve *aCurves = pLut->NewCurvesA();
  LPIccCurve *bCurves = pLut->NewCurvesB();
  if (!aCurves || !bCurves) { delete pLut; return nullptr; }
  for (int i = 0; i < 3; i++) {
    CIccTagCurve *ca = new CIccTagCurve(); ca->SetSize(2, icInitIdentity); aCurves[i] = ca;
    CIccTagCurve *cb = new CIccTagCurve(); cb->SetSize(2, icInitIdentity); bCurves[i] = cb;
  }

  icUInt8Number grid[3] = {2, 2, 2};
  if (!pLut->NewCLUT(grid, 2)) { delete pLut; return nullptr; }
  CIccCLUT *clut = pLut->GetCLUT();
  icFloatNumber *d = clut->GetData(0);
  const icUInt32Number n = clut->NumPoints() * 3;
  for (icUInt32Number i = 0; i < n; ++i) d[i] = v;
  return pLut;
}

static void build_profile(CIccProfile &p) {
  p.InitHeader();
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace = icSigRgbData;
  p.m_Header.pcs = icSigXYZData;
  p.m_Header.renderingIntent = icPerceptual;  // header intent = Perceptual (the trap)
  p.m_Header.version = icVersionNumberV4_3;

  // Distinct perceptual vs relative tables, so the selected intent is observable.
  p.AttachTag(icSigAToB0Tag, make_const_lut16(0.30f));  // Perceptual
  p.AttachTag(icSigAToB1Tag, make_const_lut16(0.60f));  // Relative colorimetric

  for (icSignature s : {icSigProfileDescriptionTag, icSigCopyrightTag}) {
    CIccTagMultiLocalizedUnicode *m = new CIccTagMultiLocalizedUnicode();
    m->SetText("pawg-q1q3 relative-intent test");
    p.AttachTag(static_cast<icSignature>(s), m);
  }
}

// Reference forward at an EXPLICIT intent via a raw CIccCmm (does not touch
// begin_profile_cmm, so it is unaffected by the fix and compiles either way).
static bool apply_explicit(CIccProfile &p, icRenderingIntent intent, icFloatNumber out[3]) {
  CIccCmm cmm(icSigRgbData, icSigXYZData, true);
  if (cmm.AddXform(p, intent) != icCmmStatOk || cmm.Begin() != icCmmStatOk) return false;
  const icFloatNumber in[3] = {0.5f, 0.5f, 0.5f};
  return cmm.Apply(out, in) == icCmmStatOk;
}

// The function under test: the default-intent forward the Q1/Q3 metrics build.
static bool apply_default(CIccProfile &p, icFloatNumber out[3]) {
  CIccCmm cmm(icSigRgbData, icSigXYZData, true);
  std::string reason;
  if (!begin_profile_cmm(&p, cmm, "forward", reason)) return false;
  const icFloatNumber in[3] = {0.5f, 0.5f, 0.5f};
  return cmm.Apply(out, in) == icCmmStatOk;
}

static bool approx(const icFloatNumber a[3], const icFloatNumber b[3]) {
  for (int i = 0; i < 3; ++i)
    if (std::fabs(a[i] - b[i]) > 1e-4) return false;
  return true;
}

int main() {
  CIccProfile p;
  build_profile(p);

  icFloatNumber per[3], rel[3], def[3];
  bool okPer = apply_explicit(p, icPerceptual, per);
  bool okRel = apply_explicit(p, icRelativeColorimetric, rel);
  bool okDef = apply_default(p, def);
  check(okPer && okRel && okDef, "perceptual, relative, and default forwards all build");
  if (!(okPer && okRel && okDef)) {
    std::printf("\n%d check(s) FAILED\n", g_failures ? g_failures : 1);
    return 1;
  }
  std::printf("perceptual=(%.3f) relative=(%.3f) default=(%.3f)\n", per[1], rel[1], def[1]);

  // Precondition: the profile genuinely separates perceptual from relative.
  check(!approx(per, rel), "profile separates perceptual (A2B0) from relative (A2B1)");

  // The fix: Q1/Q3's default-intent forward must be RELATIVE colorimetric...
  check(approx(def, rel), "begin_profile_cmm default == relative colorimetric (the fix)");
  // ...and explicitly NOT the header rendering intent (Perceptual) -- the bug.
  check(!approx(def, per), "begin_profile_cmm default is NOT the header intent (Perceptual)");

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
