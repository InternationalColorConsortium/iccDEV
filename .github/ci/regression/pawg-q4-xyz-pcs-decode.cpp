// Regression test for iccPawgReport's internal-PCS-XYZ decode in the quality
// metrics (iccquality::pcs_to_lab + evaluate_characterization in
// IccQualityMetrics.h).  Guards the fix for #1454.
//
// Defect: pcs_to_lab() decoded an XYZ-PCS sample by handing it straight to
//   icXYZtoLab(), which divides by the *actual* D50 white.  But a CMM output is
//   *internal* PCS XYZ, scaled by icXyzToXyzIn (32768/65535 ~= 0.5) relative to
//   actual XYZ.  So the internal sample had to be un-scaled with icXyzFromPcs()
//   first (exactly as IccProfLib/IccEval.cpp does, #1355/#1377).  Without it,
//   white decoded to L* = 116*cbrt(0.5) - 16 ~= 76 instead of 100, and every
//   Q1/Q3/Q4 dE on an XYZ-PCS profile was computed in a distorted Lab space.
//   Lab-PCS profiles were unaffected (they use icLabFromPcs).
//
//   The fix un-scales the internal sample in pcs_to_lab's XYZ branch, and Q4's
//   *measured* XYZ (which is already actual XYZ from the targ, not internal PCS)
//   is converted directly with icXYZtoLab so it is not double-scaled.
//
// Two assertions, both mutation-verified (revert either half of the fix and the
// matching block fails):
//   1. pcs_to_lab decodes an internal-PCS D50 white to L* ~= 100, not ~76.
//   2. Q4 on an XYZ-PCS profile reproduces an absolute-colorimetric measured
//      target (authored on the actual 0..100 XYZ scale) to ~0 dE -- which only
//      holds when predicted (internal PCS, un-scaled) and measured (actual XYZ)
//      land in the same Lab space.
//
// Exit code 0 = pass, 1 = a guarded regression reappeared.

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include "IccQualityMetrics.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

// ---------------------------------------------------------------------------
// Assertion 1: pcs_to_lab un-scales an internal PCS XYZ before icXYZtoLab.
// ---------------------------------------------------------------------------
static void test_white_decode() {
  std::printf("\n[ internal-PCS XYZ decode (pcs_to_lab) ]\n");

  // Actual D50 white, and its *internal* PCS encoding (actual * 32768/65535).
  // This is what a CMM emits for a media-relative white; pcs_to_lab must decode
  // it back to L*=100, a*=b*=0.  Use the literal D50 constant (the value of the
  // library's icD50XYZ) rather than the exported global.  That started as a
  // workaround -- icD50XYZ is DLL data and would not resolve on Windows until
  // #2219 gave it dllimport -- and is kept because icXYZtoLab divides by this
  // same constant internally, so the literal keeps the white decode exact.
  // iccdev.proflib-exported-data-linkage pins icD50XYZ against this triple, so
  // a change to the global fails there rather than drifting past this copy.
  const icFloatNumber actualD50[3] = {0.9642f, 1.0000f, 0.8249f};
  icFloatNumber internalD50[3] = {actualD50[0], actualD50[1], actualD50[2]};
  icXyzToPcs(internalD50);  // actual -> internal PCS (the inverse of the fix)

  icFloatNumber lab[3] = {0, 0, 0};
  pcs_to_lab(icSigXYZData, internalD50, lab);
  std::printf("      internal D50 white -> L*=%.3f a*=%.3f b*=%.3f\n", lab[0], lab[1], lab[2]);
  // Pre-fix this came out at L* ~= 76 (= 116*cbrt(0.5)-16); the fix restores 100.
  check(std::fabs(lab[0] - 100.0) < 0.05, "internal-PCS D50 white decodes to L*=100 (not ~76)");
  check(std::fabs(lab[1]) < 0.05 && std::fabs(lab[2]) < 0.05, "internal-PCS D50 white is neutral (a*=b*=0)");

  // Lab-PCS path must be untouched by the fix: an internal-PCS Lab white still
  // decodes to L*=100 via icLabFromPcs (a*/b* encode as pcs*255-128, so neutral
  // is 128/255, not 0.5).
  icFloatNumber labWhite[3] = {1.0f, 128.0f / 255.0f, 128.0f / 255.0f};  // PCS-encoded L*=100, a*=b*=0
  icFloatNumber labOut[3] = {labWhite[0], labWhite[1], labWhite[2]};
  pcs_to_lab(icSigLabData, labWhite, labOut);
  check(std::fabs(labOut[0] - 100.0) < 0.05 && std::fabs(labOut[1]) < 0.05 && std::fabs(labOut[2]) < 0.05,
        "Lab-PCS white decode unaffected by the fix");
}

// ---------------------------------------------------------------------------
// Assertion 2: Q4 on an XYZ-PCS profile reproduces an absolute target to ~0 dE.
// ---------------------------------------------------------------------------
static void attach_xyz(CIccProfile &p, icSignature sig, double X, double Y, double Z) {
  CIccTagXYZ *t = new CIccTagXYZ;
  (*t)[0].X = icDtoF(X);
  (*t)[0].Y = icDtoF(Y);
  (*t)[0].Z = icDtoF(Z);
  p.AttachTag(sig, t);
}

// Minimal RGB->XYZ matrix/TRC display profile (XYZ PCS) -- exactly the profile
// family the bug distorted.  Off-white media white point so absolute and
// relative colorimetric differ and "absolute" is observable.
static void build_matrix_profile(CIccProfile &p) {
  p.InitHeader();
  p.m_Header.deviceClass = icSigDisplayClass;
  p.m_Header.colorSpace = icSigRgbData;
  p.m_Header.pcs = icSigXYZData;
  p.m_Header.version = icVersionNumberV4_2;

  attach_xyz(p, icSigRedColorantTag, 0.4361, 0.2225, 0.0139);
  attach_xyz(p, icSigGreenColorantTag, 0.3851, 0.7169, 0.0971);
  attach_xyz(p, icSigBlueColorantTag, 0.1431, 0.0606, 0.7141);
  attach_xyz(p, icSigMediaWhitePointTag, 0.9505, 1.0, 1.0890);

  for (icSignature s : {icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag}) {
    p.AttachTag(static_cast<icSignature>(s), new CIccTagCurve(0));  // identity (linear) TRC
  }
  for (icSignature s : {icSigProfileDescriptionTag, icSigCopyrightTag}) {
    CIccTagMultiLocalizedUnicode *m = new CIccTagMultiLocalizedUnicode();
    m->SetText("pawg-q4 xyz-pcs test");
    p.AttachTag(static_cast<icSignature>(s), m);
  }
}

static const double kPatches[][3] = {
    {1.0, 1.0, 1.0}, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
    {0.0, 0.0, 1.0}, {0.5, 0.5, 0.5}, {0.8, 0.2, 0.4}, {0.2, 0.6, 0.9},
    {0.6, 0.6, 0.1},
};
static const int kNumPatches = static_cast<int>(sizeof(kPatches) / sizeof(kPatches[0]));

// Forward the profile at the given intent, returning *internal* PCS XYZ.
static bool forward_internal_xyz(CIccProfile &p, icRenderingIntent intent,
                                 std::vector<std::array<double, 3>> &out) {
  CIccCmm cmm(icSigRgbData, icSigXYZData, true);
  if (cmm.AddXform(p, intent, icInterpTetrahedral) != icCmmStatOk) return false;
  if (cmm.Begin() != icCmmStatOk) return false;
  out.clear();
  for (int i = 0; i < kNumPatches; ++i) {
    icFloatNumber in[3] = {static_cast<icFloatNumber>(kPatches[i][0]),
                           static_cast<icFloatNumber>(kPatches[i][1]),
                           static_cast<icFloatNumber>(kPatches[i][2])};
    icFloatNumber xyz[3] = {0, 0, 0};
    if (cmm.Apply(xyz, in) != icCmmStatOk) return false;
    out.push_back({xyz[0], xyz[1], xyz[2]});
  }
  return true;
}

// Author a targ whose measured XYZ columns are *actual* XYZ on the 0..1 scale.
// We derive actual XYZ from the absolute-intent internal PCS via icXyzFromPcs --
// i.e. the genuine colorimetry of each patch -- so a correct evaluator
// reproduces it to ~0 dE only when its predicted side also un-scales internal
// PCS (the #1454 fix).  (A wide-gamut blue patch has actual Z > 1, so values are
// kept on the 0..1 scale rather than *100 -- that stays under the evaluator's
// "> 2 means a 0..100 target" rescale heuristic, leaving the actual values as-is.)
static std::string make_targ_actual(const std::vector<std::array<double, 3>> &internalXyz) {
  static const char *names[] = {"WHITE", "BLACK", "RED",  "GREEN", "BLUE",
                                "GREY",  "P_80",  "P_29", "OLIVE"};
  std::string s =
      "CGATS.17\n"
      "NUMBER_OF_FIELDS 8\n"
      "BEGIN_DATA_FORMAT\n"
      "SAMPLE_ID SAMPLE_NAME RGB_R RGB_G RGB_B XYZ_X XYZ_Y XYZ_Z\n"
      "END_DATA_FORMAT\n"
      "BEGIN_DATA\n";
  char line[256];
  for (int i = 0; i < kNumPatches; ++i) {
    icFloatNumber xyz[3] = {static_cast<icFloatNumber>(internalXyz[i][0]),
                            static_cast<icFloatNumber>(internalXyz[i][1]),
                            static_cast<icFloatNumber>(internalXyz[i][2])};
    icXyzFromPcs(xyz);  // internal PCS -> actual XYZ (0..1)
    std::snprintf(line, sizeof(line), "%d %s %.4f %.4f %.4f %.6f %.6f %.6f\n",
                  i + 1, names[i], kPatches[i][0] * 100.0, kPatches[i][1] * 100.0,
                  kPatches[i][2] * 100.0, xyz[0], xyz[1], xyz[2]);
    s += line;
  }
  s += "END_DATA\n";
  return s;
}

static void test_q4_xyz_pcs() {
  std::printf("\n[ Q4 round-trip on XYZ-PCS profile (evaluate_characterization) ]\n");

  CIccProfile probe;
  build_matrix_profile(probe);
  std::vector<std::array<double, 3>> absInternal;
  if (!forward_internal_xyz(probe, icAbsoluteColorimetric, absInternal)) {
    check(false, "could not build absolute-colorimetric forward CMM");
    return;
  }

  // Sanity: the forward really is on the internal PCS scale (white Y ~= 0.5, not 1).
  check(absInternal[0][1] > 0.4 && absInternal[0][1] < 0.6,
        "forward white Y is internal-PCS scale (~0.5)");

  CIccProfile p;
  build_matrix_profile(p);
  CIccTagText *t = new CIccTagText;
  t->SetText(make_targ_actual(absInternal).c_str());
  p.AttachTag(icSigCharTargetTag, t);

  CharacterizationMetrics m;
  std::string reason;
  bool ok = evaluate_characterization(&p, m, reason);
  check(ok, "XYZ-PCS characterization evaluates");
  if (ok) {
    std::printf("      rowsUsed=%d avgDe00=%.4f maxDe00=%.4f\n", m.rowsUsed, m.avgDe00, m.maxDe00);
    check(m.rowsUsed == kNumPatches, "every patch used");
    // Pre-fix the predicted side stayed on internal scale while measured was
    // actual -> tens of dE (white alone is L*=76 vs 100).  The fix collapses it.
    check(m.avgDe00 < 0.10, "avg dE00 ~ 0 (predicted internal PCS un-scaled to match actual measured)");
    check(m.maxDe00 < 0.20, "max dE00 ~ 0");
  } else {
    std::printf("      reason: %s\n", reason.c_str());
  }
}

int main() {
  test_white_decode();
  test_q4_xyz_pcs();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
