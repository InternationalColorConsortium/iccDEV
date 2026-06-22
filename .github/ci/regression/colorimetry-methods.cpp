// Contract regression for the IccColorimetry spectral->XYZ reduction module (#1475).
//
// The module (IccProfLib/IccColorimetry.{h,cpp}) provides the plural, registry-
// aligned methods for reducing a spectral reflectance vector to CIE XYZ
// (icXYZCalcDirectSum / icXYZCalcWeighting / icXYZCalcSpragueTo1nm), plus the
// range-reconciliation building blocks they share (icSpectralResample,
// icComputeWeightingTable, icApplyWeightingTable). It is intended to be the
// mechanism colourbill's external characterization tooling and a future MPE-path
// correctness fix build on, so its numeric contract needs a guard that does not
// depend on any profile file or on (still-provisional) registry constants.
//
// Rather than pin "golden" XYZ values -- which the ICC verify-first effort may yet
// revise -- this test pins the method-independent mathematical invariants the
// reduction must satisfy by construction. A sign error, an index slip, a broken
// normalization, or a Sprague-coefficient typo all break at least one invariant:
//
//   A. icSpectralResample reproduces what it must: identity on a matching grid,
//      constants exactly (partition of unity), and a linear ramp exactly (linear
//      interp everywhere; Sprague at interior points; linear extrapolation of a
//      line past the source ends).
//   B. With the observer, illuminant and measurement on one common grid, all three
//      calc methods collapse to the same operator and must agree to fp precision.
//   C. On differing grids (so resampling/extrapolation actually runs), relative
//      colorimetry still holds exactly (perfect diffuser -> Y = 1) and the operator
//      stays linear; the three methods agree on smooth data to within quadrature.
//   D. The free-function weighting path (icComputeWeightingTable +
//      icApplyWeightingTable) matches the calculator's self-computed weighting.
//
// The core algorithm invariants (A-D) use smooth synthetic curves (positive, no
// real CIE data) -- they are properties of the algorithm, not of the numbers, so
// synthetic input exercises every code path without baking in provisional values.
// Parts E and F add real-data anchors -- iccDEV's built-in observer/illuminant
// tables, and the baked-in registry weighting tables -- checked against canonical
// white points. Those validate that the data plumbing hands back the correct
// tables (right observer/illuminant, no transposition/corruption), which is a
// property of external CIE truth, not of a provisional reduction method.
//
// Returns 0 on success; the number of failed invariants otherwise (each printed).

#include "IccColorimetry.h"
#include "IccUtil.h"          // icFtoF16, icSpectralRange, icFloatNumber

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

icSpectralRange makeRange(double startNm, double endNm, int steps)
{
  icSpectralRange r;
  r.start = icFtoF16((icFloat32Number)startNm);
  r.end   = icFtoF16((icFloat32Number)endNm);
  r.steps = (icUInt16Number)steps;
  return r;
}

double rangeStep(double startNm, double endNm, int steps)
{
  return steps > 1 ? (endNm - startNm) / (steps - 1) : 0.0;
}

// Smooth, strictly-positive synthetic colour-matching functions on [startNm,endNm]
// at the given step count: three offset Gaussians filling 3*steps as xbar,ybar,zbar.
std::vector<icFloatNumber> makeObserver(double startNm, double endNm, int steps)
{
  const double dx = rangeStep(startNm, endNm, steps);
  std::vector<icFloatNumber> obs(3 * steps);
  for (int i = 0; i < steps; i++) {
    const double nm = startNm + i * dx;
    const double gx = std::exp(-0.5 * std::pow((nm - 600.0) / 40.0, 2.0));        // xbar lobe
    const double gy = std::exp(-0.5 * std::pow((nm - 555.0) / 45.0, 2.0));        // ybar (luminous)
    const double gz = 1.6 * std::exp(-0.5 * std::pow((nm - 450.0) / 32.0, 2.0));  // zbar (short-wl)
    obs[i]             = (icFloatNumber)gx;
    obs[steps + i]     = (icFloatNumber)gy;
    obs[2 * steps + i] = (icFloatNumber)gz;
  }
  return obs;
}

// Smooth, strictly-positive synthetic illuminant SPD rising 1.0 -> ~1.6.
std::vector<icFloatNumber> makeIlluminant(double startNm, double endNm, int steps)
{
  const double dx = rangeStep(startNm, endNm, steps);
  std::vector<icFloatNumber> ill(steps);
  for (int i = 0; i < steps; i++) {
    const double nm = startNm + i * dx;
    ill[i] = (icFloatNumber)(1.0 + 0.6 * (nm - startNm) / (endNm - startNm));
  }
  return ill;
}

int g_fail = 0;

void check(bool ok, const char *what, double err)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[colorimetry-methods] FAIL: %s (err=%.3e)\n", what, err);
  }
}

double maxAbsDiff(const std::vector<icFloatNumber> &a, const std::vector<icFloatNumber> &b)
{
  double m = 0.0;
  for (size_t i = 0; i < a.size() && i < b.size(); i++)
    m = std::max(m, std::fabs((double)a[i] - (double)b[i]));
  return m;
}

const double TOL_EXACT = 1e-6;   // invariants exact by construction (fp/-O2 margin)
const double TOL_AGREE = 2e-2;   // cross-method quadrature agreement on smooth data

// ---- Part A: icSpectralResample contract --------------------------------------
void testResample()
{
  // A1: identity (src grid == dst grid) returns the input.
  {
    icSpectralRange g = makeRange(400, 700, 31);
    std::vector<icFloatNumber> in(31), out(31);
    for (int i = 0; i < 31; i++) in[i] = (icFloatNumber)(0.1 + 0.02 * i);
    icSpectralResample(g, &in[0], g, &out[0], icSpectralInterpSprague, icSpectralExtendHold);
    check(maxAbsDiff(in, out) < TOL_EXACT, "resample identity (matching grid)", maxAbsDiff(in, out));
  }

  // A2: a constant must come back constant for every interpolation method.
  {
    icSpectralRange src = makeRange(400, 700, 31);   // 10 nm
    icSpectralRange dst = makeRange(400, 700, 61);   // 5 nm
    std::vector<icFloatNumber> in(31, (icFloatNumber)0.7), out(61);
    const icSpectralInterpMethod methods[3] =
      { icSpectralInterpLinear, icSpectralInterpCubic, icSpectralInterpSprague };
    for (int m = 0; m < 3; m++) {
      icSpectralResample(src, &in[0], dst, &out[0], methods[m], icSpectralExtendHold);
      double e = 0.0;
      for (int j = 0; j < 61; j++) e = std::max(e, std::fabs((double)out[j] - 0.7));
      check(e < TOL_EXACT, "resample constant -> constant", e);
    }
  }

  // A3: a linear ramp f(nm) = a + b*nm reconstructs exactly -- linear interp at all
  // dst points, Sprague at interior dst points (the documented Table V property).
  {
    const double a = -0.3, b = 0.0025;
    icSpectralRange src = makeRange(400, 700, 31);
    icSpectralRange dst = makeRange(400, 700, 61);
    const double sdx = rangeStep(400, 700, 31), ddx = rangeStep(400, 700, 61);
    std::vector<icFloatNumber> in(31), outL(61), outS(61);
    for (int i = 0; i < 31; i++) in[i] = (icFloatNumber)(a + b * (400 + i * sdx));
    icSpectralResample(src, &in[0], dst, &outL[0], icSpectralInterpLinear,  icSpectralExtendHold);
    icSpectralResample(src, &in[0], dst, &outS[0], icSpectralInterpSprague, icSpectralExtendHold);
    double eL = 0.0, eS = 0.0;
    for (int j = 0; j < 61; j++) {
      const double truth = a + b * (400 + j * ddx);
      eL = std::max(eL, std::fabs((double)outL[j] - truth));
      if (j > 0 && j < 60) eS = std::max(eS, std::fabs((double)outS[j] - truth)); // interior
    }
    check(eL < TOL_EXACT, "resample linear ramp (linear interp, all points)", eL);
    check(eS < TOL_EXACT, "resample linear ramp (Sprague, interior)", eS);
  }

  // A4: linear extrapolation of a line past the source ends is also exact.
  {
    const double a = 0.2, b = 0.001;
    icSpectralRange src = makeRange(400, 700, 31);
    icSpectralRange dst = makeRange(380, 720, 35);   // wider than src on both ends
    const double sdx = rangeStep(400, 700, 31), ddx = rangeStep(380, 720, 35);
    std::vector<icFloatNumber> in(31), out(35);
    for (int i = 0; i < 31; i++) in[i] = (icFloatNumber)(a + b * (400 + i * sdx));
    icSpectralResample(src, &in[0], dst, &out[0], icSpectralInterpLinear, icSpectralExtendLinear);
    double e = 0.0;
    for (int j = 0; j < 35; j++)
      e = std::max(e, std::fabs((double)out[j] - (a + b * (380 + j * ddx))));
    check(e < TOL_EXACT, "resample linear ramp (linear extrapolation past ends)", e);
  }
}

// Build + run one method, returning XYZ for a reflectance vector.
bool reduce(const icSpectralRange &obsR, const std::vector<icFloatNumber> &obs,
            const icSpectralRange &illR, const std::vector<icFloatNumber> &ill,
            const icSpectralRange &measR, icXYZCalcMethod method,
            const std::vector<icFloatNumber> &refl, icFloatNumber xyz[3])
{
  CIccColorimetricCalculator calc;
  if (!calc.SetObserver(obsR, &obs[0]))   return false;
  if (!calc.SetIlluminant(illR, &ill[0])) return false;
  if (!calc.Prepare(measR, method))       return false;
  return calc.ReflectanceToXYZ(&refl[0], xyz);
}

// ---- Part B: one common grid -> all three methods are identical ----------------
void testSameGrid()
{
  icSpectralRange g = makeRange(400, 700, 31);
  std::vector<icFloatNumber> obs = makeObserver(400, 700, 31);
  std::vector<icFloatNumber> ill = makeIlluminant(400, 700, 31);

  // A non-flat reflectance (smooth hump) so the operators are exercised off-white.
  std::vector<icFloatNumber> refl(31);
  for (int i = 0; i < 31; i++)
    refl[i] = (icFloatNumber)(0.5 + 0.3 * std::sin(i * 0.21));

  icFloatNumber d[3], w[3], s[3];
  bool ok = reduce(g, obs, g, ill, g, icXYZCalcDirectSum,    refl, d)
         && reduce(g, obs, g, ill, g, icXYZCalcWeighting,    refl, w)
         && reduce(g, obs, g, ill, g, icXYZCalcSpragueTo1nm, refl, s);
  check(ok, "same-grid: all methods prepared", 0.0);
  if (!ok) return;

  double dw = 0.0, ds = 0.0;
  for (int c = 0; c < 3; c++) {
    dw = std::max(dw, std::fabs((double)d[c] - (double)w[c]));
    ds = std::max(ds, std::fabs((double)d[c] - (double)s[c]));
  }
  check(dw < TOL_EXACT, "same-grid: DirectSum == Weighting", dw);
  check(ds < TOL_EXACT, "same-grid: DirectSum == SpragueTo1nm", ds);

  // Perfect diffuser -> Y == 1 on this grid too.
  std::vector<icFloatNumber> white(31, (icFloatNumber)1.0);
  icFloatNumber wp[3];
  if (reduce(g, obs, g, ill, g, icXYZCalcDirectSum, white, wp))
    check(std::fabs((double)wp[1] - 1.0) < TOL_EXACT, "same-grid: perfect diffuser Y==1",
          std::fabs((double)wp[1] - 1.0));
}

// ---- Part C: differing grids -> normalization + linearity hold, methods agree --
void testDifferingGrids()
{
  icSpectralRange obsR  = makeRange(400, 700, 31);   // 10 nm observer
  icSpectralRange illR  = makeRange(400, 700, 16);   // 20 nm illuminant
  icSpectralRange measR = makeRange(400, 700, 61);   //  5 nm measurement
  std::vector<icFloatNumber> obs = makeObserver(400, 700, 31);
  std::vector<icFloatNumber> ill = makeIlluminant(400, 700, 16);

  const icXYZCalcMethod methods[3] =
    { icXYZCalcDirectSum, icXYZCalcWeighting, icXYZCalcSpragueTo1nm };
  const char *names[3] = { "DirectSum", "Weighting", "SpragueTo1nm" };

  // C1: perfect diffuser -> Y == 1 exactly, for every method, despite resampling.
  std::vector<icFloatNumber> white(61, (icFloatNumber)1.0);
  for (int m = 0; m < 3; m++) {
    icFloatNumber xyz[3];
    if (reduce(obsR, obs, illR, ill, measR, methods[m], white, xyz)) {
      char buf[96]; std::snprintf(buf, sizeof(buf), "differing-grid %s: perfect diffuser Y==1", names[m]);
      check(std::fabs((double)xyz[1] - 1.0) < TOL_EXACT, buf, std::fabs((double)xyz[1] - 1.0));
    } else {
      check(false, "differing-grid: method prepared", 0.0);
    }
  }

  // C2: the reduction operator is linear: f(p*R1 + q*R2) == p*f(R1) + q*f(R2).
  std::vector<icFloatNumber> r1(61), r2(61), mix(61);
  for (int i = 0; i < 61; i++) {
    r1[i]  = (icFloatNumber)(0.4 + 0.3 * std::sin(i * 0.17));
    r2[i]  = (icFloatNumber)(0.5 + 0.2 * std::cos(i * 0.11));
    mix[i] = (icFloatNumber)(0.3 * r1[i] + 0.7 * r2[i]);
  }
  for (int m = 0; m < 3; m++) {
    icFloatNumber a[3], b[3], c[3];
    if (reduce(obsR, obs, illR, ill, measR, methods[m], r1, a)
     && reduce(obsR, obs, illR, ill, measR, methods[m], r2, b)
     && reduce(obsR, obs, illR, ill, measR, methods[m], mix, c)) {
      double e = 0.0;
      for (int k = 0; k < 3; k++)
        e = std::max(e, std::fabs((double)c[k] - (0.3 * (double)a[k] + 0.7 * (double)b[k])));
      char buf[96]; std::snprintf(buf, sizeof(buf), "differing-grid %s: operator linearity", names[m]);
      check(e < TOL_EXACT, buf, e);
    }
  }

  // C3: on smooth data the three methods are quadratures of the same integral and
  // must agree within interval error (sanity, not precision).
  icFloatNumber d[3], w[3], s[3];
  if (reduce(obsR, obs, illR, ill, measR, icXYZCalcDirectSum,    r1, d)
   && reduce(obsR, obs, illR, ill, measR, icXYZCalcWeighting,    r1, w)
   && reduce(obsR, obs, illR, ill, measR, icXYZCalcSpragueTo1nm, r1, s)) {
    double e = 0.0;
    for (int k = 0; k < 3; k++) {
      e = std::max(e, std::fabs((double)d[k] - (double)w[k]));
      e = std::max(e, std::fabs((double)d[k] - (double)s[k]));
    }
    check(e < TOL_AGREE, "differing-grid: methods agree on smooth reflectance", e);
  }
}

// ---- Part D: free-function weighting path matches the calculator ----------------
void testWeightingFreeFunctions()
{
  icSpectralRange obsR  = makeRange(400, 700, 31);
  icSpectralRange illR  = makeRange(400, 700, 16);
  icSpectralRange measR = makeRange(400, 700, 61);
  std::vector<icFloatNumber> obs = makeObserver(400, 700, 31);
  std::vector<icFloatNumber> ill = makeIlluminant(400, 700, 16);

  std::vector<icFloatNumber> wt(3 * 61);
  bool built = icComputeWeightingTable(obsR, &obs[0], illR, &ill[0], measR, &wt[0],
                                       icSpectralInterpLinear);
  check(built, "icComputeWeightingTable built", 0.0);
  if (!built) return;

  // Perfect diffuser through the raw table -> Y == 1.
  std::vector<icFloatNumber> white(61, (icFloatNumber)1.0);
  icFloatNumber xyzW[3];
  icApplyWeightingTable(measR, &wt[0], &white[0], xyzW);
  check(std::fabs((double)xyzW[1] - 1.0) < TOL_EXACT, "weighting table: perfect diffuser Y==1",
        std::fabs((double)xyzW[1] - 1.0));

  // And the raw table must equal the calculator's self-computed weighting method
  // (which delegates to icComputeWeightingTable with the same linear reconstruction).
  std::vector<icFloatNumber> refl(61);
  for (int i = 0; i < 61; i++) refl[i] = (icFloatNumber)(0.45 + 0.25 * std::sin(i * 0.19));
  icFloatNumber xyzFree[3], xyzCalc[3];
  icApplyWeightingTable(measR, &wt[0], &refl[0], xyzFree);
  if (reduce(obsR, obs, illR, ill, measR, icXYZCalcWeighting, refl, xyzCalc)) {
    double e = 0.0;
    for (int k = 0; k < 3; k++) e = std::max(e, std::fabs((double)xyzFree[k] - (double)xyzCalc[k]));
    check(e < TOL_EXACT, "weighting table matches calculator icXYZCalcWeighting", e);
  }
}

// ---- Part E: built-in standard observer/illuminant accessors --------------------
void testStandardAccessors()
{
  // E1/E2: the two built-in CIE observers resolve to an 81-sample 380-780 nm table.
  const icStandardObserver knownObs[2] = { icStdObs1931TwoDegrees, icStdObs1964TenDegrees };
  for (int i = 0; i < 2; i++) {
    icSpectralRange r;
    const icFloatNumber *p = icGetStandardObserver(knownObs[i], r);
    bool ok = p && r.steps == 81
           && std::fabs((double)icF16toF(r.start) - 380.0) < 1.0
           && std::fabs((double)icF16toF(r.end)   - 780.0) < 1.0;
    check(ok, "icGetStandardObserver returns built-in 380-780@5nm table", 0.0);
  }

  // E3: an observer with no built-in data returns nullptr (not a stale/garbage ptr).
  {
    icSpectralRange r;
    check(icGetStandardObserver(icStdObsUnknown, r) == NULL,
          "icGetStandardObserver(unknown) -> nullptr", 0.0);
  }

  // E4: the four built-in illuminants resolve; a non-built-in one (F2) returns null.
  const icIlluminant knownIll[4] = { icIlluminantD50, icIlluminantD65, icIlluminantD93, icIlluminantA };
  for (int i = 0; i < 4; i++) {
    icSpectralRange r;
    const icFloatNumber *p = icGetStandardIlluminant(knownIll[i], r);
    check(p && r.steps == 81, "icGetStandardIlluminant returns built-in 81-sample SPD", 0.0);
  }
  {
    icSpectralRange r;
    check(icGetStandardIlluminant(icIlluminantF2, r) == NULL,
          "icGetStandardIlluminant(F2, no built-in) -> nullptr", 0.0);
  }

  // E6: real-data anchor. Reducing a perfect diffuser with the built-in D50 SPD and
  // 1931 2-deg observer (iccDEV's own 5 nm rectangular sum) must reproduce the D50
  // white point: Y == 1 exactly and X,Z within a band of the canonical 0.9642 /
  // 0.8249 (iccDEV's 5 nm tables sit ~5e-4 low on Z, well inside 2e-3). This pins
  // that the accessors hand back the correct tables and the baseline integrates
  // them correctly -- it anchors the plumbing, not a reference/registry method.
  {
    CIccColorimetricCalculator calc;
    icSpectralRange obsR;
    const icFloatNumber *obs = icGetStandardObserver(icStdObs1931TwoDegrees, obsR);
    bool ready = obs
              && calc.SetStandardObserver(icStdObs1931TwoDegrees)
              && calc.SetStandardIlluminant(icIlluminantD50)
              && calc.Prepare(obsR, icXYZCalcDirectSum);
    check(ready, "D50 anchor: standard obs/illum prepared on built-in grid", 0.0);
    if (ready) {
      std::vector<icFloatNumber> white(obsR.steps, (icFloatNumber)1.0);
      icFloatNumber xyz[3] = { 0, 0, 0 };
      calc.ReflectanceToXYZ(&white[0], xyz);
      std::printf("[colorimetry-methods] D50/1931 white point = %.5f %.5f %.5f\n",
                  (double)xyz[0], (double)xyz[1], (double)xyz[2]);
      check(std::fabs((double)xyz[1] - 1.0)    < TOL_EXACT, "D50 anchor: Y == 1", std::fabs((double)xyz[1] - 1.0));
      check(std::fabs((double)xyz[0] - 0.9642) < 2e-3, "D50 anchor: X ~ 0.9642", std::fabs((double)xyz[0] - 0.9642));
      check(std::fabs((double)xyz[2] - 0.8249) < 2e-3, "D50 anchor: Z ~ 0.8249", std::fabs((double)xyz[2] - 0.8249));
    }
  }
}

// ---- Part F: baked-in registry weighting tables --------------------------------
// The 10 registry tables (icGetColorimetryWeightingTable) are static const data
// injected from registry.color.org/colorimetry-data by the maintainer generator.
// They are deliberately NOT pinned value-by-value -- the LWL weights are provisional
// pending CIE TC1-101. Instead each table is validated by the one property every
// colorimetric weighting table has by construction: applied to a perfect diffuser it
// reproduces its illuminant's white point on the registry's CIE Y=100 scale
// (sum(Wy)=100; X/Y and Z/Y = the canonical chromaticity). That anchors the injected
// data to external truth (CIE white points) and catches a corrupted, mislabeled or
// Wx/Wz-transposed table, without depending on any provisional per-wavelength weight.
void testRegistryWeightingTables()
{
  struct Combo {
    icStandardObserver               obs;
    icColorimetryWeightingIlluminant illum;
    const char                      *name;
    bool                             hasCanon;  // canonical white point pinned below?
    double                           Xn, Zn;    // canonical X/Y, Z/Y (Y=1) if hasCanon
  };
  // Canonical white points: CIE 15 D50/D65/A for the 2 deg (1931) and 10 deg (1964)
  // observers -- rock-solid external references. F11 and LED-B1 white points vary
  // more between sources, so they are checked for presence + Y=100 only (not pinned).
  const Combo combos[10] = {
    { icStdObs1931TwoDegrees, icWtIllumD50,    "D50/1931",    true,  0.96422, 0.82521 },
    { icStdObs1931TwoDegrees, icWtIllumD65,    "D65/1931",    true,  0.95047, 1.08883 },
    { icStdObs1931TwoDegrees, icWtIllumA,      "A/1931",      true,  1.09850, 0.35585 },
    { icStdObs1931TwoDegrees, icWtIllumLED_B1, "LED-B1/1931", false, 0.0,     0.0     },
    { icStdObs1931TwoDegrees, icWtIllumF11,    "F11/1931",    false, 0.0,     0.0     },
    { icStdObs1964TenDegrees, icWtIllumD50,    "D50/1964",    true,  0.96720, 0.81427 },
    { icStdObs1964TenDegrees, icWtIllumD65,    "D65/1964",    true,  0.94811, 1.07304 },
    { icStdObs1964TenDegrees, icWtIllumA,      "A/1964",      true,  1.11144, 0.35200 },
    { icStdObs1964TenDegrees, icWtIllumLED_B1, "LED-B1/1964", false, 0.0,     0.0     },
    { icStdObs1964TenDegrees, icWtIllumF11,    "F11/1964",    false, 0.0,     0.0     },
  };
  const double WP_BAND = 4e-3;   // absorbs the LWL-vs-canonical method delta (~1e-4)

  for (int i = 0; i < 10; i++) {
    icSpectralRange r;
    const icFloatNumber *w = icGetColorimetryWeightingTable(combos[i].obs, combos[i].illum, r);
    bool grid = w && r.steps == 41
             && std::fabs((double)icF16toF(r.start) - 380.0) < 1.0
             && std::fabs((double)icF16toF(r.end)   - 780.0) < 1.0;
    check(grid, "registry table present on 380-780@10nm grid", 0.0);
    if (!grid) continue;

    std::vector<icFloatNumber> diffuser(r.steps, (icFloatNumber)1.0);
    icFloatNumber xyz[3];
    icApplyWeightingTable(r, w, &diffuser[0], xyz);
    std::printf("[colorimetry-methods] registry %-11s perfect-diffuser XYZ = %8.4f %8.4f %8.4f\n",
                combos[i].name, (double)xyz[0], (double)xyz[1], (double)xyz[2]);

    // Y=100 normalization invariant -- holds for all 10 tables by construction.
    check(std::fabs((double)xyz[1] - 100.0) < 1e-2, "registry table sum(Wy)==100",
          std::fabs((double)xyz[1] - 100.0));

    // Canonical white-point anchor (external truth) for the well-tabulated combos.
    if (combos[i].hasCanon && xyz[1] > 1e-6) {
      double xn = (double)xyz[0] / (double)xyz[1];
      double zn = (double)xyz[2] / (double)xyz[1];
      char buf[112];
      std::snprintf(buf, sizeof(buf), "registry %s: white point X/Y matches canonical", combos[i].name);
      check(std::fabs(xn - combos[i].Xn) < WP_BAND, buf, std::fabs(xn - combos[i].Xn));
      std::snprintf(buf, sizeof(buf), "registry %s: white point Z/Y matches canonical", combos[i].name);
      check(std::fabs(zn - combos[i].Zn) < WP_BAND, buf, std::fabs(zn - combos[i].Zn));
    }
  }

  // An (obs,illum) outside the registry's 10 combinations -> nullptr + zeroed range.
  {
    icSpectralRange r;
    const icFloatNumber *w = icGetColorimetryWeightingTable(icStdObsUnknown, icWtIllumD50, r);
    check(w == NULL && r.steps == 0, "icGetColorimetryWeightingTable(unknown obs) -> nullptr", 0.0);
  }
}

// ---- Part G: emissive / radiant reduction --------------------------------------
void testEmissive()
{
  // Built-in D50 SPD doubles as the adopted white AND the emitted spectrum: a source
  // radiating its own adopted white reproduces the white point, so the emissive
  // reduction must give the SAME D50 white point as the reflectance perfect-diffuser
  // anchor (0.96425 / 1 / 0.82468), Y == 1. This pins the no-illuminant emissive
  // normalization (k = sum(ybar*white), mirroring CIccPcc::getEmissiveObserver).
  icSpectralRange r;
  const icFloatNumber *d50 = icGetStandardIlluminant(icIlluminantD50, r);
  CIccColorimetricCalculator calc;
  bool ready = d50
            && calc.SetStandardObserver(icStdObs1931TwoDegrees)
            && calc.SetEmissiveWhite(r, d50)
            && calc.PrepareEmissive(r);
  check(ready, "emissive: observer + adopted white prepared", 0.0);
  if (ready) {
    icFloatNumber xyz[3] = { 0, 0, 0 };
    calc.RadianceToXYZ(d50, xyz);   // emit exactly the adopted white spectrum
    std::printf("[colorimetry-methods] emissive D50-white radiance = %.5f %.5f %.5f\n",
                (double)xyz[0], (double)xyz[1], (double)xyz[2]);
    check(std::fabs((double)xyz[1] - 1.0)    < TOL_EXACT, "emissive: adopted white Y == 1", std::fabs((double)xyz[1] - 1.0));
    check(std::fabs((double)xyz[0] - 0.9642) < 2e-3, "emissive: adopted white X ~ 0.9642", std::fabs((double)xyz[0] - 0.9642));
    check(std::fabs((double)xyz[2] - 0.8249) < 2e-3, "emissive: adopted white Z ~ 0.8249", std::fabs((double)xyz[2] - 0.8249));

    // Emissive XYZ scales with radiance magnitude (unlike reflectance): twice the
    // radiance -> twice the XYZ, so a 2x-bright white emitter has Y = 2.
    std::vector<icFloatNumber> twice(r.steps);
    for (int i = 0; i < (int)r.steps; i++) twice[i] = (icFloatNumber)(2.0 * (double)d50[i]);
    icFloatNumber xyz2[3] = { 0, 0, 0 };
    calc.RadianceToXYZ(&twice[0], xyz2);
    double e = 0.0;
    for (int k = 0; k < 3; k++) e = std::max(e, std::fabs((double)xyz2[k] - 2.0 * (double)xyz[k]));
    check(e < 1e-5, "emissive: radiance scales XYZ (2x white -> Y=2)", e);

    // Operator linearity on emissive inputs.
    std::vector<icFloatNumber> l2(r.steps), mix(r.steps);
    for (int i = 0; i < (int)r.steps; i++) {
      l2[i]  = (icFloatNumber)((double)d50[i] * (0.5 + 0.4 * std::sin(i * 0.2)));
      mix[i] = (icFloatNumber)(0.3 * (double)d50[i] + 0.7 * (double)l2[i]);
    }
    icFloatNumber a[3], b[3], c[3];
    calc.RadianceToXYZ(d50, a);
    calc.RadianceToXYZ(&l2[0], b);
    calc.RadianceToXYZ(&mix[0], c);
    double el = 0.0;
    for (int k = 0; k < 3; k++) el = std::max(el, std::fabs((double)c[k] - (0.3 * (double)a[k] + 0.7 * (double)b[k])));
    check(el < TOL_EXACT, "emissive: operator linearity", el);
  }

  // Without an adopted white PrepareEmissive must fail (no normalization reference).
  CIccColorimetricCalculator noWhite;
  noWhite.SetStandardObserver(icStdObs1931TwoDegrees);
  check(d50 && !noWhite.PrepareEmissive(r), "emissive: missing white -> PrepareEmissive fails", 0.0);
}

} // namespace

int main()
{
  testResample();
  testSameGrid();
  testDifferingGrids();
  testWeightingFreeFunctions();
  testStandardAccessors();
  testRegistryWeightingTables();
  testEmissive();

  if (g_fail) {
    std::fprintf(stderr, "[colorimetry-methods] %d invariant(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[colorimetry-methods] all invariants passed\n");
  return 0;
}
