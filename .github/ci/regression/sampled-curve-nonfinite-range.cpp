// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression for the fourth, fifth and sixth per-pixel guards retired alongside
// the CLUT ones covered by clut-apply-guard-retirement.cpp. Those three were
// not redundant.
//
// Each of the sampled curves computed
//
//   pos = (v - first) / m_range * m_last;
//   if (!std::isfinite(pos) || pos < 0.0f) pos = 0.0f;
//
// and the isfinite term was dropped on the argument that "Begin() refuses
// m_range == 0, so pos cannot be non-finite". That does not follow: m_range is
// m_lastEntry - m_firstEntry, both endpoints arrive unvalidated from the wire
// (Read() via ReadFloat32Float, the XML front end via atof, which accepts
// "nan"), and NaN == 0.0 is false. A NaN endpoint therefore passed every test
// in Begin(), reached Apply(), and produced a NaN pos that also survived both
// clamps below -- because NaN compares false against everything -- leaving
// static_cast<icUInt32Number>(NaN) to index m_pSamples. That cast is undefined
// (#2324, #2347); under -fno-sanitize-recover=float-cast-overflow it aborts,
// and in a plain build it yields an implementation-defined index far outside
// the array.
//
// The fix is split, because the three sites do not have the same invariant:
//
//   CIccSingleSampledCurve and CIccSampledCalculatorCurve carry a real sampled
//   domain, so their Begin() now refuses a non-finite m_range and Apply() keeps
//   the cleared hot path. isfinite() on the span is exactly the right test: a
//   difference is finite only when both endpoints are finite AND it does not
//   overflow, so one check covers NaN, infinity, and the +/-FLT_MAX pair.
//   testSingleSampledBeginRefusesNonFiniteRange() and
//   testCalculatorCurveBeginRefusesNonFiniteRange() pin that, and the 0..1 rows
//   in each are the controls -- they passed before the guard was added too, so a
//   failure there means the new check over-rejects rather than protects.
//
//   CIccSampledCurveSegment cannot make that promise from Begin(). The outermost
//   segments of a segmented curve are constructed with the icMinFloat32Number /
//   icMaxFloat32Number sentinels (CIccSegmentedCurve::Read), so an infinite
//   m_range is a legitimate configuration there, and even between finite
//   endpoints (v - m_startPoint) can overflow to infinity and give inf/inf.
//   Refusing a non-finite span would reject conforming profiles, so that site
//   keeps the per-pixel isfinite guard. testSegmentApplyKeepsItsGuard() and
//   testSegmentApplySurvivesAnInfiniteSpan() pin both halves of that.
//
// Note also that CIccSegmentedCurve::Read() push_back()s segments directly
// rather than going through Insert(), so Insert()'s contiguity test
// (StartPoint() == previous EndPoint(), which a NaN would fail) never runs on
// the read path and cannot be relied on to keep NaN out.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccMpeBasic.h"
#include "IccMpeCalc.h"
#include "IccTagMPE.h"

#include <cmath>
#include <cstdio>
#include <limits>
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
    std::fprintf(stderr, "[sampled-curve-nonfinite] FAIL: %s\n", what);
  }
}

const icFloatNumber kNaN = std::numeric_limits<icFloatNumber>::quiet_NaN();
const icFloatNumber kInf = std::numeric_limits<icFloatNumber>::infinity();

struct RangeCase {
  icFloatNumber first;
  icFloatNumber last;
  bool          expectBegin;   // Begin() must return this
  const char   *label;
};

// The +/-FLT_MAX row is the one a NaN-only check would miss: both endpoints are
// finite, but their difference overflows to infinity in float, and inf in the
// denominator turns a large numerator into inf/inf == NaN.
const RangeCase kRanges[] = {
  { 0.0f,              1.0f,              true,  "0..1 (control)" },
  { 0.030029f,         4.8046f,           true,  "0.03..4.8 (control, real HLG domain)" },
  { kNaN,              1.0f,              false, "NaN first entry" },
  { 0.0f,              kNaN,              false, "NaN last entry" },
  { kNaN,              kNaN,              false, "NaN both entries" },
  { -kInf,             1.0f,              false, "-inf first entry" },
  { 0.0f,              kInf,              false, "+inf last entry" },
  { icMinFloat32Number, icMaxFloat32Number, false, "+/-FLT_MAX span overflows to inf" },
};

const size_t kNumRanges = sizeof(kRanges) / sizeof(kRanges[0]);

void testSingleSampledBeginRefusesNonFiniteRange()
{
  for (size_t i = 0; i < kNumRanges; i++) {
    const RangeCase &c = kRanges[i];

    CIccSingleSampledCurve curve(c.first, c.last);
    check(curve.SetSize(4), "single sampled curve allocates its samples");
    icFloatNumber *pSamples = curve.GetSamples();
    if (!pSamples)
      continue;
    for (int s = 0; s < 4; s++)
      pSamples[s] = (icFloatNumber)(s / 3.0);

    const bool began = curve.Begin(icElemInterpLinear, NULL);

    char msg[192];
    std::snprintf(msg, sizeof(msg), "CIccSingleSampledCurve::Begin(%s) returns %s",
                  c.label, c.expectBegin ? "true" : "false");
    check(began == c.expectBegin, msg);

    // Only the accepted rows may be applied; an element whose Begin() failed is
    // never applied by the CMM, so exercising a refused one here would test a
    // path the library does not use.
    if (!began)
      continue;

    for (int p = 0; p <= 8; p++) {
      const icFloatNumber v = c.first + (c.last - c.first) * (icFloatNumber)(p / 8.0);
      const icFloatNumber out = curve.Apply(v);
      std::snprintf(msg, sizeof(msg),
                    "CIccSingleSampledCurve::Apply(%s) stays finite and in [0,1]", c.label);
      check(std::isfinite((double)out) && out >= 0.0f && out <= 1.0f, msg);
    }
  }
}

// A 1-in/1-out identity calculator, the smallest thing that gets past
// CIccSampledCalculatorCurve::Begin()'s channel-count and null checks so the
// range test underneath them is actually reached.
CIccMpeCalculator *newIdentityCalculator()
{
  CIccMpeCalculator *pCalc = new CIccMpeCalculator(1, 1);
  std::string sReport;
  if (pCalc->SetCalcFunc("{ in(0) out(0) }", sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "[sampled-curve-nonfinite] calculator setup: %s\n", sReport.c_str());
    delete pCalc;
    return NULL;
  }
  return pCalc;
}

void testCalculatorCurveBeginRefusesNonFiniteRange()
{
  for (size_t i = 0; i < kNumRanges; i++) {
    const RangeCase &c = kRanges[i];

    CIccMpeCalculator *pCalc = newIdentityCalculator();
    check(pCalc != NULL, "identity calculator builds");
    if (!pCalc)
      return;

    // A real enclosing element, not NULL: CIccMpeCalculator::Begin() reads
    // pMPE->GetCmmEnvLookup() without a null test (IccMpeCalc.cpp:5231), so the
    // NULL that the other two curves in this file accept would crash here
    // before reaching the range check under test. Declared before the curve so
    // it outlives it: Begin() parks a pointer into this element inside the
    // calculator the curve owns and deletes.
    CIccTagMultiProcessElement mpe(1, 1);

    CIccSampledCalculatorCurve curve(c.first, c.last);
    check(curve.SetCalculator(pCalc), "sampled calculator curve takes the calculator");
    check(curve.SetRecommendedSize(16), "sampled calculator curve takes a size");

    const bool began = curve.Begin(icElemInterpLinear, &mpe);

    char msg[192];
    std::snprintf(msg, sizeof(msg), "CIccSampledCalculatorCurve::Begin(%s) returns %s",
                  c.label, c.expectBegin ? "true" : "false");
    check(began == c.expectBegin, msg);

    if (!began)
      continue;

    for (int p = 0; p <= 8; p++) {
      const icFloatNumber v = c.first + (c.last - c.first) * (icFloatNumber)(p / 8.0);
      const icFloatNumber out = curve.Apply(v);
      std::snprintf(msg, sizeof(msg),
                    "CIccSampledCalculatorCurve::Apply(%s) stays finite", c.label);
      check(std::isfinite((double)out), msg);
    }
  }
}

// Build a sampled segment with the given span, preceded by a formula segment so
// Begin()'s pPrevSeg requirement is met, and overwrite the samples afterwards
// with known values -- Begin() seeds m_pSamples[0] from the previous segment,
// which for a NaN start point is itself NaN, and a NaN return would not tell us
// whether the index was in range.
struct SegmentFixture {
  CIccFormulaCurveSegment *pPrev;
  CIccSampledCurveSegment *pSeg;

  SegmentFixture() : pPrev(NULL), pSeg(NULL) {}
  ~SegmentFixture() { delete pSeg; delete pPrev; }
};

bool buildSegment(SegmentFixture &f, icFloatNumber start, icFloatNumber end)
{
  f.pPrev = new CIccFormulaCurveSegment(icMinFloat32Number, start);
  icFloatNumber params[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
  f.pPrev->SetFunction(0, 4, params);
  if (!f.pPrev->Begin(NULL))
    return false;

  f.pSeg = new CIccSampledCurveSegment(start, end);
  if (!f.pSeg->SetSize(4))
    return false;
  if (!f.pSeg->Begin(f.pPrev))
    return false;

  icFloatNumber *pSamples = f.pSeg->GetSamples();
  if (!pSamples)
    return false;
  for (int s = 0; s < 4; s++)
    pSamples[s] = (icFloatNumber)(0.25 + s * 0.25);
  return true;
}

// A NaN start point reaches Apply() here by design: Begin() cannot refuse a
// non-finite span for a segment without also refusing the sentinel-bounded
// outer segments of every conforming segmented curve. So the per-pixel guard is
// what has to catch it. Every sample is in [0.25, 1.0], so a returned value in
// that interval proves the index stayed inside the array; on master the cast is
// undefined and the read is far outside it.
void testSegmentApplyKeepsItsGuard()
{
  SegmentFixture f;
  check(buildSegment(f, kNaN, 1.0f), "segment with a NaN start point still begins");
  if (!f.pSeg)
    return;

  for (int p = 0; p <= 8; p++) {
    const icFloatNumber v = (icFloatNumber)(p / 8.0);
    const icFloatNumber out = f.pSeg->Apply(v);
    check(std::isfinite((double)out) && out >= 0.25f && out <= 1.0f,
          "CIccSampledCurveSegment::Apply over a NaN span returns a real sample");
  }
}

// The sentinel case the segment guard exists for: a finite span whose numerator
// overflows. (v - -FLT_MAX) is infinite for a large v, and m_range is infinite
// too, so pos is inf/inf == NaN with no NaN anywhere in the inputs.
void testSegmentApplySurvivesAnInfiniteSpan()
{
  SegmentFixture f;
  check(buildSegment(f, icMinFloat32Number, icMaxFloat32Number),
        "segment spanning the float sentinels still begins");
  if (!f.pSeg)
    return;

  const icFloatNumber probes[5] = { 0.0f, 1.0f, 1.0e38f, -1.0e38f, icMaxFloat32Number };
  for (int p = 0; p < 5; p++) {
    const icFloatNumber out = f.pSeg->Apply(probes[p]);
    check(std::isfinite((double)out) && out >= 0.25f && out <= 1.0f,
          "CIccSampledCurveSegment::Apply over an infinite span returns a real sample");
  }
}

// CIccSampledCurveSegment::Validate has to test its endpoints rather than their
// difference. The last segment of a segmented curve carries the
// icMaxFloat32Number sentinel, so a sufficiently negative breakpoint overflows
// the span to infinity while both endpoints stay sound -- warning on the span
// would report a conforming profile as malformed, and would contradict the
// reason Apply() keeps its per-pixel guard. Only a non-finite endpoint is a real
// defect.
void testSegmentValidateWarnsOnEndpointsNotSpans()
{
  struct SegCase {
    icFloatNumber start;
    icFloatNumber end;
    bool          expectWarning;
    const char   *label;
  };

  const SegCase cases[] = {
    { icMinFloat32Number, icMaxFloat32Number, false, "both sentinels: span overflows, endpoints sound" },
    { -1.0e32f,           icMaxFloat32Number, false, "very negative breakpoint to the end sentinel" },
    { 1.0f,               icMaxFloat32Number, false, "ordinary last segment" },
    { 0.0f,               1.0f,               false, "ordinary interior segment" },
    { kNaN,               1.0f,               true,  "NaN start point" },
    { 0.0f,               kNaN,               true,  "NaN end point" },
    { -kInf,              1.0f,               true,  "-inf start point" },
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const SegCase &c = cases[i];

    CIccSampledCurveSegment seg(c.start, c.end);
    check(seg.SetSize(4), "segment allocates its samples");

    std::string report;
    seg.Validate("", report, NULL, NULL);
    const bool warned = report.find("non-finite") != std::string::npos;

    char msg[192];
    std::snprintf(msg, sizeof(msg), "CIccSampledCurveSegment::Validate(%s) %s",
                  c.label, c.expectWarning ? "warns" : "stays quiet");
    check(warned == c.expectWarning, msg);
  }
}

} // namespace

int main()
{
  testSingleSampledBeginRefusesNonFiniteRange();
  testCalculatorCurveBeginRefusesNonFiniteRange();
  testSegmentApplyKeepsItsGuard();
  testSegmentApplySurvivesAnInfiniteSpan();
  testSegmentValidateWarnsOnEndpointsNotSpans();

  if (g_fail) {
    std::fprintf(stderr, "[sampled-curve-nonfinite] %d assertion(s) failed\n", g_fail);
    // Deliberately not "return g_fail" as the neighbouring regression tests do.
    // This one asserts per probe value, so on an unfixed tree the count is 77 --
    // the exit status this repository uses with SKIP_RETURN_CODE to mean
    // "skipped". A red run must never be able to read as a skip, and a count
    // above 255 would wrap to an arbitrary status including 0, so report the
    // count on stderr and exit with a plain failure.
    return 1;
  }
  std::printf("[sampled-curve-nonfinite] all assertions passed\n");
  return 0;
}
