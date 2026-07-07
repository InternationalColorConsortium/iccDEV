// Regression test for iccPawgReport's Q2 curve-invertibility check
// (iccquality::measure_curve_invertibility in IccQualityMetrics.h).  Guards the
// fix for #1458.
//
// Defect: Q2 round-tripped EVERY embedded curve via Find(Apply(x)) and treated a
//   large inverse error as a defect -- including the A/B/M shaper curves inside
//   the A2B/B2A LUT tags.  A B2A output shaper curve legitimately has a flat
//   plateau at the dark end (normal gamut/ink clamping): it is monotonic and
//   spans [0,1] but is not invertible across the plateau, so Find(Apply(x))
//   returns the plateau start and the round-trip "error" equals the plateau
//   width (e.g. APTEC 0.0977, CRPC6 0.1406).  Correct CMYK output profiles thus
//   spuriously WARNed.
//
//   Invertibility is a required property of tone-reproduction curves only
//   (rTRC/gTRC/bTRC/kTRC).  The fix scopes the inverse-error metric to tone
//   curves; shaper curves keep only the monotonicity / dead-curve checks.
//
// The test keys on the pre-existing CurveResult fields (name, maxError) so it
// compiles against the pre-fix header too and fails at the ASSERTION, not the
// build.  Mutation-verified: pre-fix the shaper plateau scores maxError ~0.141
// and the first block's assertion fails; with the fix it is 0.
//
// Exit code 0 = pass, 1 = a guarded regression reappeared.

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include "IccQualityMetrics.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace iccquality;

static int g_failures = 0;
static void check(bool cond, const char *msg) {
  if (cond) { std::printf("ok:   %s\n", msg); }
  else      { std::printf("FAIL: %s\n", msg); ++g_failures; }
}

// Tone-curve names append_curve_target uses; everything else is a LUT shaper
// curve (named like "B2A0 B[0]").
static bool is_tone_name(const std::string &n) {
  return n == "rTRC" || n == "gTRC" || n == "bTRC" || n == "kTRC";
}

// 256-entry curve: flat 0 for the first nPlateau samples, then a linear ramp to
// 1.  Monotonic and not wholly flat, but not invertible across the plateau --
// inverse round-trip error ~= nPlateau/255.  This is the real B2A dark-end
// clamping shape the bug tripped on.
static CIccTagCurve *make_plateau_curve(int nPlateau) {
  CIccTagCurve *c = new CIccTagCurve();
  c->SetSize(256);
  for (int i = 0; i < 256; ++i) {
    const double v = (i <= nPlateau) ? 0.0
                                     : double(i - nPlateau) / double(255 - nPlateau);
    (*c)[static_cast<icUInt32Number>(i)] = static_cast<icFloatNumber>(v);
  }
  return c;
}

// Strictly monotonic gamma curve -> invertible, inverse error ~0.
static CIccTagCurve *make_gamma_curve() {
  CIccTagCurve *c = new CIccTagCurve();
  c->SetSize(256);
  for (int i = 0; i < 256; ++i)
    (*c)[static_cast<icUInt32Number>(i)] = static_cast<icFloatNumber>(std::pow(i / 255.0, 2.2));
  return c;
}

// B2A Lut16 whose *output* (B) shaper curves carry the dark-end plateau.
static CIccTagLut16 *make_b2a_plateau() {
  CIccTagLut16 *pLut = new CIccTagLut16;
  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigLabData, icSigRgbData);
  LPIccCurve *a = pLut->NewCurvesA();
  LPIccCurve *b = pLut->NewCurvesB();
  for (int i = 0; i < 3; ++i) {
    CIccTagCurve *ca = new CIccTagCurve(); ca->SetSize(2, icInitIdentity); a[i] = ca;
    b[i] = make_plateau_curve(36);   // 36/255 = 0.141 plateau (matches CRPC6)
  }
  icUInt8Number grid[3] = {2, 2, 2};
  pLut->NewCLUT(grid, 2);
  return pLut;
}

// Max inverse error over curves whose name matches the predicate.
template <typename Pred>
static double max_err_where(const CurveInvertibilityMetrics &m, Pred pred) {
  double e = 0.0;
  for (const auto &c : m.curves)
    if (pred(c.name)) e = std::max(e, c.maxError);
  return e;
}
static int count_where(const CurveInvertibilityMetrics &m, bool tone) {
  int n = 0;
  for (const auto &c : m.curves)
    if (is_tone_name(c.name) == tone) ++n;
  return n;
}

int main() {
  // --- Shaper (B2A output) plateau must NOT be inverse-thresholded (the fix) --
  std::printf("\n[ shaper curve plateau (B2A output) ]\n");
  {
    CIccProfile p; p.InitHeader();
    p.m_Header.deviceClass = icSigOutputClass;
    p.m_Header.colorSpace = icSigCmykData; p.m_Header.pcs = icSigLabData;
    p.AttachTag(icSigBToA0Tag, make_b2a_plateau());

    const auto m = measure_curve_invertibility(&p);
    check(count_where(m, /*tone=*/false) >= 3, "B2A output shaper curves were assessed");
    const double shaperErr = max_err_where(m, [](const std::string &n){ return !is_tone_name(n); });
    std::printf("      shaper max inverse error = %.4f (pre-fix ~0.141)\n", shaperErr);
    check(shaperErr < 0.01,
          "shaper plateau inverse error NOT thresholded -> Q2 OK on CMYK output profiles");
  }

  // --- A tone curve (rTRC) plateau IS still flagged (defect kept in scope) ----
  std::printf("\n[ tone curve plateau (rTRC) still checked ]\n");
  {
    CIccProfile p; p.InitHeader();
    p.m_Header.deviceClass = icSigDisplayClass;
    p.m_Header.colorSpace = icSigRgbData; p.m_Header.pcs = icSigXYZData;
    p.AttachTag(icSigRedTRCTag, make_plateau_curve(36));

    const auto m = measure_curve_invertibility(&p);
    check(count_where(m, /*tone=*/true) >= 1, "rTRC assessed as a tone curve");
    const double toneErr = max_err_where(m, is_tone_name);
    std::printf("      tone max inverse error = %.4f\n", toneErr);
    check(toneErr > 0.05, "tone-curve plateau inverse error IS computed -> Q2 WARN");
  }

  // --- An invertible tone curve passes ---------------------------------------
  std::printf("\n[ invertible tone curve (gamma) ]\n");
  {
    CIccProfile p; p.InitHeader();
    p.m_Header.deviceClass = icSigDisplayClass;
    p.m_Header.colorSpace = icSigRgbData; p.m_Header.pcs = icSigXYZData;
    p.AttachTag(icSigRedTRCTag, make_gamma_curve());

    const auto m = measure_curve_invertibility(&p);
    const double toneErr = max_err_where(m, is_tone_name);
    check(toneErr < 0.02, "invertible gamma tone curve inverse error ~0 -> Q2 OK");
  }

  if (g_failures) { std::printf("\n%d check(s) FAILED\n", g_failures); return 1; }
  std::printf("\nall checks passed\n");
  return 0;
}
