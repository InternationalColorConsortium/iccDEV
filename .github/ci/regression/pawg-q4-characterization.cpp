// Regression test for iccPawgReport's Q4 characterization round-trip
// (iccquality::evaluate_characterization in IccQualityMetrics.h).
//
// Guards two independent defects fixed together:
//
//  Bug 1 — wrong rendering intent.  Q4 compares the profile's predicted output
//    against the *measured* colorimetry embedded in the profile's CharTarget
//    (targ) tag.  Measured characterization data is absolute colorimetry (the
//    real colour of the patches, paper tint and all), so the only intent whose
//    numbers are defined to match it is ABSOLUTE COLORIMETRIC.  The old code
//    evaluated the perceptual A2B0 table (and, even via the CMM fallback, a
//    media-relative intent), reporting ~7 dE on profiles whose true agreement
//    with their own data is ~0.05 dE.  This test builds a profile whose
//    absolute and relative colorimetric outputs differ (an off-white media
//    white point) and asserts the evaluator reproduces an ABSOLUTE target to
//    ~0 dE while a RELATIVE target diverges — i.e. it really uses absolute.
//
//  Bug 2 — over-strict CGATS parsing.  The old parser parsed every data cell as
//    a number and dropped any row containing a non-numeric token, so a target
//    with the usual alphanumeric SAMPLE_NAME column (e.g. "B17") lost almost
//    every patch (an IT8.7/4 target collapsed from 1617 rows to ~49).  The
//    parser now keeps rows as raw cells and the consumer parses only the columns
//    it needs by name.  This test feeds targets with text SAMPLE_NAME columns,
//    tab and space delimiters, and full-colorant-name headers (CYAN/MAGENTA/…)
//    and asserts every data row survives and maps.
//
// Exit code 0 = pass, 1 = a guarded regression reappeared.

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccTagMPE.h"

#include "IccQualityMetrics.h"

#include <array>
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
// Bug 2: CGATS parsing keeps every data row and maps columns by name.
// ---------------------------------------------------------------------------
static void test_cgats_parser() {
  std::printf("\n[ CGATS parser (bug 2) ]\n");

  // Space-delimited, CMYK + Lab, with a numeric SAMPLE_ID and an alphanumeric
  // SAMPLE_NAME column.  Only one of the four patch names ("100") is all-digits;
  // the old all-numeric-row parser would have kept just that row.
  const char *spaceTarget =
      "CGATS.17\n"
      "ORIGINATOR \"unit-test\"\n"
      "NUMBER_OF_FIELDS 9\n"
      "BEGIN_DATA_FORMAT\n"
      "SAMPLE_ID SAMPLE_NAME CMYK_C CMYK_M CMYK_Y CMYK_K LAB_L LAB_A LAB_B\n"
      "END_DATA_FORMAT\n"
      "NUMBER_OF_SETS 4\n"
      "BEGIN_DATA\n"
      "1 B17    0 100 20  0 54.19 65.0 10.0\n"
      "2 2F12   0  85 20  0 59.58 55.0  8.0\n"
      "3 100    0   0  0  0 95.00  0.0 -2.0\n"
      "4 GRAY50 0   0  0 50 50.00  0.0  0.0\n"
      "END_DATA\n";

  ParsedTargetData ps;
  parse_cgats_text(spaceTarget, ps);
  check(ps.parsedFormat, "space target: DATA_FORMAT parsed");
  check(ps.parsedData, "space target: DATA section parsed");
  check(ps.fields.size() == 9, "space target: 9 field names");
  check(ps.rows.size() == 4,
        "space target: all 4 data rows kept (text SAMPLE_NAME no longer drops rows)");

  const int cCol = find_field_index(ps.fields, {"CMYK_C", "C", "CYAN"});
  const int kCol = find_field_index(ps.fields, {"CMYK_K", "K", "BLACK"});
  const int lCol = find_field_index(ps.fields, {"LAB_L", "L"});
  const int nameCol = find_field_index(ps.fields, {"SAMPLE_NAME"});
  check(cCol == 2 && kCol == 5 && lCol == 6, "space target: device/PCS columns mapped by name");

  // The text SAMPLE_NAME cell must NOT parse as a number, but the numeric
  // device/PCS cells in that same row must, and the row is still present.
  double v = 0.0;
  check(!parse_numeric_cell(ps.rows[0], nameCol, v), "space target: text SAMPLE_NAME cell rejected");
  check(parse_numeric_cell(ps.rows[0], cCol, v) && v == 0.0, "space target: CMYK_C cell parses");
  check(parse_numeric_cell(ps.rows[0], lCol, v) && v == 54.19, "space target: LAB_L cell parses");

  // Tab-delimited, with full colorant-name headers (CYAN/MAGENTA/YELLOW/BLACK)
  // instead of CMYK_*; the widened aliases must still map.
  const char *tabTarget =
      "CGATS.17\n"
      "NUMBER_OF_FIELDS 8\n"
      "BEGIN_DATA_FORMAT\n"
      "SAMPLE_ID\tCYAN\tMAGENTA\tYELLOW\tBLACK\tLAB_L\tLAB_A\tLAB_B\n"
      "END_DATA_FORMAT\n"
      "BEGIN_DATA\n"
      "P1\t0\t0\t0\t0\t95.0\t0.0\t-2.0\n"
      "P2\t100\t0\t0\t0\t55.0\t-37.0\t-50.0\n"
      "END_DATA\n";

  ParsedTargetData pt;
  parse_cgats_text(tabTarget, pt);
  check(pt.fields.size() == 8, "tab target: 8 field names (tab-split format)");
  check(pt.rows.size() == 2 && pt.rows[0].size() == 8,
        "tab target: rows tab-split into 8 cells");
  check(find_field_index(pt.fields, {"CMYK_C", "C", "CYAN"}) == 1,
        "tab target: full colorant name CYAN maps as the cyan channel");
  check(find_field_index(pt.fields, {"CMYK_K", "K", "BLACK"}) == 4,
        "tab target: full colorant name BLACK maps as the black channel");
}

// ---------------------------------------------------------------------------
// Bug 1: build a minimal RGB->XYZ matrix/TRC display profile whose absolute and
// relative colorimetric outputs differ (off-white media white point), so the
// intent the evaluator selects is observable.
// ---------------------------------------------------------------------------
static void attach_xyz(CIccProfile &p, icSignature sig, double X, double Y, double Z) {
  CIccTagXYZ *t = new CIccTagXYZ;
  (*t)[0].X = icDtoF(X);
  (*t)[0].Y = icDtoF(Y);
  (*t)[0].Z = icDtoF(Z);
  p.AttachTag(sig, t);
}

static void build_matrix_profile(CIccProfile &p) {
  p.InitHeader();
  p.m_Header.deviceClass = icSigDisplayClass;
  p.m_Header.colorSpace = icSigRgbData;
  p.m_Header.pcs = icSigXYZData;
  p.m_Header.version = icVersionNumberV4_2;

  // sRGB primaries, Bradford-adapted to the D50 PCS.
  attach_xyz(p, icSigRedColorantTag, 0.4361, 0.2225, 0.0139);
  attach_xyz(p, icSigGreenColorantTag, 0.3851, 0.7169, 0.0971);
  attach_xyz(p, icSigBlueColorantTag, 0.1431, 0.0606, 0.7141);
  // Deliberately OFF-WHITE media white point (a bluish, D65-ish white).  This is
  // what makes absolute colorimetric (which keeps this white) differ from
  // relative colorimetric (which re-references it to the D50 PCS white).
  attach_xyz(p, icSigMediaWhitePointTag, 0.9505, 1.0, 1.0890);

  for (icSignature s : {icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag}) {
    p.AttachTag(static_cast<icSignature>(s), new CIccTagCurve(0));  // identity (linear) TRC
  }
  for (icSignature s : {icSigProfileDescriptionTag, icSigCopyrightTag}) {
    CIccTagMultiLocalizedUnicode *m = new CIccTagMultiLocalizedUnicode();
    m->SetText("pawg-q4 test");
    p.AttachTag(static_cast<icSignature>(s), m);
  }
}

// RGB patches in 0..1 used both to synthesise the target and (after the
// evaluator re-normalises the RGB columns) to drive its forward transform.
static const double kPatches[][3] = {
    {1.0, 1.0, 1.0}, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
    {0.0, 0.0, 1.0}, {0.5, 0.5, 0.5}, {0.8, 0.2, 0.4}, {0.2, 0.6, 0.9},
    {0.6, 0.6, 0.1},
};
static const int kNumPatches = static_cast<int>(sizeof(kPatches) / sizeof(kPatches[0]));

// Run the profile's forward transform at the given intent, returning the
// *actual* XYZ for each patch.  Used to author the synthetic targ so its
// "measured" values are exactly what that intent produces.
//
// CIccCmm with an XYZ PCS emits *internal* PCS XYZ (scaled by icXyzToXyzIn ~=
// 0.5).  Measured characterization data in a real targ is actual colorimetry,
// and the Q4 evaluator's measured-XYZ path feeds the targ columns straight to
// icXYZtoLab while un-scaling its own predicted side via icXyzFromPcs (#1454).
// So author the targ in actual XYZ too -- apply the same icXyzFromPcs to the
// CMM output -- otherwise predicted (un-scaled) and measured (raw internal)
// land on different scales and even a perfect profile shows ~20 dE.
static bool forward_xyz(CIccProfile &p, icRenderingIntent intent,
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
    icXyzFromPcs(xyz);   // internal PCS XYZ -> actual XYZ (match evaluator, #1454)
    out.push_back({xyz[0], xyz[1], xyz[2]});
  }
  return true;
}

// Build a CGATS targ block: numeric SAMPLE_ID, *text* SAMPLE_NAME, RGB device
// columns (0..100), and the supplied actual XYZ as the measured columns.
// The XYZ values are actual colorimetry on the 0..1 scale (<= ~1) so the
// evaluator's measured-XYZ rescale is a no-op and "measured" lands on exactly
// the same actual-XYZ footing as its own (un-scaled) predicted PCS — isolating
// the rendering-intent difference, which is what this test targets.
static std::string make_targ(const std::vector<std::array<double, 3>> &xyz) {
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
    std::snprintf(line, sizeof(line), "%d %s %.4f %.4f %.4f %.6f %.6f %.6f\n",
                  i + 1, names[i], kPatches[i][0] * 100.0, kPatches[i][1] * 100.0,
                  kPatches[i][2] * 100.0, xyz[i][0], xyz[i][1], xyz[i][2]);
    s += line;
  }
  s += "END_DATA\n";
  return s;
}

static bool evaluate_with_targ(const std::string &targ, CharacterizationMetrics &m,
                               std::string &reason) {
  CIccProfile p;
  build_matrix_profile(p);
  CIccTagText *t = new CIccTagText;
  t->SetText(targ.c_str());
  p.AttachTag(icSigCharTargetTag, t);
  return evaluate_characterization(&p, m, reason);
}

static void test_absolute_intent() {
  std::printf("\n[ absolute-colorimetric intent (bug 1) ]\n");

  CIccProfile probe;
  build_matrix_profile(probe);
  std::vector<std::array<double, 3>> absXyz, relXyz;
  if (!forward_xyz(probe, icAbsoluteColorimetric, absXyz) ||
      !forward_xyz(probe, icRelativeColorimetric, relXyz)) {
    check(false, "could not build forward CMMs for the in-memory profile");
    return;
  }

  // Precondition: the profile genuinely separates the two intents, else the test
  // could not tell them apart.
  double sep = 0.0;
  for (int i = 0; i < kNumPatches; ++i)
    for (int j = 0; j < 3; ++j) sep += std::fabs(absXyz[i][j] - relXyz[i][j]);
  check(sep > 0.05, "profile separates absolute vs relative colorimetric");

  // Feed the evaluator a target measured under ABSOLUTE colorimetric: a correct
  // absolute-intent evaluator must reproduce it to ~0 dE, on every patch.
  CharacterizationMetrics mAbs;
  std::string reasonAbs;
  bool okAbs = evaluate_with_targ(make_targ(absXyz), mAbs, reasonAbs);
  check(okAbs, "absolute target evaluates");
  if (okAbs) {
    check(mAbs.rowsUsed == kNumPatches,
          "absolute target: every patch used (text SAMPLE_NAME rows survive end-to-end)");
    check(mAbs.avgDe00 < 0.05, "absolute target: avg dE00 ~ 0 (evaluator uses absolute colorimetric)");
    check(mAbs.maxDe00 < 0.10, "absolute target: max dE00 ~ 0");
  }

  // Feed the evaluator a target measured under RELATIVE colorimetric.  An
  // absolute-intent evaluator must NOT match it — if Q4 ever regresses to the
  // relative/perceptual intent this is the assertion that fails.
  CharacterizationMetrics mRel;
  std::string reasonRel;
  bool okRel = evaluate_with_targ(make_targ(relXyz), mRel, reasonRel);
  check(okRel, "relative target evaluates");
  if (okRel) {
    check(mRel.avgDe00 > 2.0,
          "relative target: avg dE00 large (evaluator is NOT using relative/perceptual)");
  }
}

// ---------------------------------------------------------------------------
// When the absolute-colorimetric forward transform cannot be built, the reason
// names the failing step, carries the CMM's status text, and says what
// forward-transform tags the profile carries.  These used to collapse into one
// sentence with the status discarded, so a reader could not tell a profile Q4
// has no path for from a transform that failed for another reason.
//
// Q4 builds through the colorimetric LUT path with the D2Bx tags opted out, so
// it considers AToBx, then the matrix/TRC model (RGB) or grayTRC (Gray), and
// never a DToBx tag for a colorimetric-PCS profile.
// ---------------------------------------------------------------------------
static bool has(const std::string &s, const char *what) {
  return s.find(what) != std::string::npos;
}

static std::string reason_for(CIccProfile &p, const std::string &targ) {
  CIccTagText *t = new CIccTagText;
  t->SetText(targ.c_str());
  p.AttachTag(icSigCharTargetTag, t);
  CharacterizationMetrics m;
  std::string reason;
  bool ok = evaluate_characterization(&p, m, reason);
  check(!ok, "  a profile with no usable forward transform is not evaluated");
  std::printf("    reason: %s\n", reason.c_str());
  return reason;
}

static void strip_matrix_trc(CIccProfile &p) {
  for (icSignature s : {icSigRedColorantTag, icSigGreenColorantTag, icSigBlueColorantTag,
                        icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag})
    p.DeleteTag(static_cast<icSignature>(s));
}

static void test_failure_reasons() {
  std::printf("\n[ forward-transform failure reasons ]\n");

  std::vector<std::array<double, 3>> xyz(kNumPatches, {0.5, 0.5, 0.5});
  const std::string rgbTarg = make_targ(xyz);

  // RGB, no AToBx and no matrix/TRC: CIccXform::Create still returns a
  // matrix/TRC transform for RGB, which then fails to START - the step that
  // is now named.
  {
    CIccProfile p;
    build_matrix_profile(p);
    strip_matrix_trc(p);
    std::string r = reason_for(p, rgbTarg);
    check(has(r, "an absolute-colorimetric forward transform could not be built"),
          "  the established lead sentence is kept");
    check(has(r, "created but could not be started ("), "  RGB, no tags: the failing step is Begin()");
    check(has(r, "no AToBx tag") && has(r, "no matrix/TRC tags"),
          "  RGB, no tags: the reason says what the profile does not carry");
  }

  // RGB with an incomplete matrix/TRC set: the missing tags are named.
  {
    CIccProfile p;
    build_matrix_profile(p);
    p.DeleteTag(icSigBlueTRCTag);
    std::string r = reason_for(p, rgbTarg);
    check(has(r, "an incomplete matrix/TRC set (missing bTRC)"),
          "  RGB, incomplete set: the missing tag is named");
  }

  // RGB whose only forward transform is a DToB0 tag.  Q4 does not use DToBx
  // for a colorimetric-PCS profile, and the reason says so - the case a
  // reader most needs told apart from a broken profile.
  {
    CIccProfile p;
    build_matrix_profile(p);
    strip_matrix_trc(p);
    p.m_Header.version = icVersionNumberV5;
    p.AttachTag(icSigDToB0Tag, new CIccTagMultiProcessElement(3, 3));
    std::string r = reason_for(p, rgbTarg);
    check(has(r, "DToB0, which Q4 does not use"), "  DToB0-only: the DToB0 tag is named as unused");
    check(has(r, "cannot be evaluated here"),
          "  DToB0-only: the reason says such a profile cannot be evaluated by Q4");
  }

  // CMYK with no AToBx: no transform is created at all - a different step,
  // and the one the RGB cases above cannot reach.
  {
    CIccProfile p;
    p.InitHeader();
    p.m_Header.deviceClass = icSigOutputClass;
    p.m_Header.colorSpace = icSigCmykData;
    p.m_Header.pcs = icSigXYZData;
    p.m_Header.version = icVersionNumberV4_2;
    attach_xyz(p, icSigMediaWhitePointTag, 0.9642, 1.0, 0.8249);
    std::string targ =
        "CGATS.17\nNUMBER_OF_FIELDS 8\nBEGIN_DATA_FORMAT\n"
        "SAMPLE_ID CMYK_C CMYK_M CMYK_Y CMYK_K XYZ_X XYZ_Y XYZ_Z\nEND_DATA_FORMAT\nBEGIN_DATA\n"
        "1 0 0 0 0 0.9 0.9 0.8\n2 100 0 0 0 0.2 0.3 0.6\n3 0 100 0 0 0.4 0.2 0.3\n"
        "4 0 0 100 0 0.8 0.9 0.1\n5 0 0 0 100 0.01 0.01 0.01\nEND_DATA\n";
    std::string r = reason_for(p, targ);
    check(has(r, "creating the transform failed ("), "  CMYK, no AToBx: the failing step is creation");
    check(has(r, "The profile carries no AToBx tag."),
          "  CMYK, no AToBx: the reason says what the profile does not carry");
  }
}

int main() {
  test_cgats_parser();
  test_absolute_intent();
  test_failure_reasons();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
