// Regression for the non-terminating hue normalization in CIccPRMG::GetChroma.
//
// GetChroma reduces an arbitrary hue angle into [0, 360) before deriving the
// icPRMG_Chroma row index. That reduction used to be a pair of loops:
//
//     while (h < 0.0)
//       h += 360.0;
//     while (h >= 360.0)
//       h -= 360.0;
//
// which do not terminate for large-magnitude hue angles. Once |h| reaches about
// 1e20, a step of 360 is smaller than the ULP of the float, so "h += 360.0"
// rounds straight back to h: the loop condition never changes and the call spins
// forever. Measured on the unfixed code, h = 1e20f, 1e30f and 3.4e38f all made
// zero progress after 5e7 iterations, while h = -1e8f still terminated (277778
// iterations) because 360 is still above the ULP there.
//
// The isfinite() guard at the top of GetChroma does not cover this. These inputs
// are finite -- they are simply too large for a step of 360 to move, which is a
// different failure than the NaN/infinity case that guard was written for.
//
// The fix replaces both loops with a single fmod() reduction. fmod is exact in
// IEEE-754, so hue angles that already worked are unaffected, and it cannot loop.
//
// Reachability, stated honestly: no shipped tool reaches this with an unbounded
// hue. Both in-tree callers (iccRoundTrip, wxProfileDump) go through
// CIccPRMG::EvaluateProfile -> InGamut(Lab) -> icLab2Lch, and an atan2-derived
// angle is already in [0, 360). This is a hang in the public API surface
// (CIccPRMG::GetChroma and InGamut(L,c,h) are both public in IccPrmg.h), not an
// attacker-reachable denial of service through a profile. The test drives the
// public entry point directly, which is exactly the exposure being fixed.
//
// A hang has no return value to assert on, so the unfixed code fails this test by
// exceeding the CTest TIMEOUT rather than by returning something wrong. The
// value assertions below then cover the other half: that the reduction is the
// mathematically correct one and not merely a bail-out.
//
// Returns 0 on success; non-zero on failure.

#include "IccPrmg.h"

#include <cmath>
#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void fail(const char *what)
{
  std::fprintf(stderr, "[prmg-hue] FAIL: %s\n", what);
  g_failures++;
}

// The reduction GetChroma is expected to perform, computed independently here so
// the test checks the *result* of the normalization rather than just that the
// call came back.
icFloatNumber normalizeHue(icFloatNumber h)
{
  icFloatNumber r = (icFloatNumber)std::fmod((double)h, 360.0);
  if (r < 0.0)
    r += 360.0f;
  if (r >= 360.0f)
    r = 0.0f;
  return r;
}

} // namespace

int main()
{
  CIccPRMG prmg;

  // A mid-gamut lightness, so every hue below lands on a real table row rather
  // than one of the out-of-range rejections.
  const icFloatNumber L = 50.0f;

  // 1. The existing rejection contract must be untouched by the fix.
  if (prmg.GetChroma((icFloatNumber)NAN, 0.0f) >= 0.0f)
    fail("NaN L was not rejected");
  if (prmg.GetChroma(L, (icFloatNumber)NAN) >= 0.0f)
    fail("NaN h was not rejected");
  if (prmg.GetChroma((icFloatNumber)INFINITY, 0.0f) >= 0.0f)
    fail("infinite L was not rejected");
  if (prmg.GetChroma(L, (icFloatNumber)INFINITY) >= 0.0f)
    fail("infinite h was not rejected");
  if (prmg.GetChroma(3.0f, 0.0f) >= 0.0f)
    fail("L below 3.5 was not rejected");
  if (prmg.GetChroma(101.0f, 0.0f) >= 0.0f)
    fail("L above 100 was not rejected");

  // 2. Ordinary in-range hues must still produce a usable chroma. These are the
  //    only values the shipped tools ever supply, so they are the regression
  //    surface that matters most.
  const icFloatNumber ordinary[] = {0.0f, 45.0f, 180.0f, 359.9f};
  for (icFloatNumber h : ordinary) {
    icFloatNumber c = prmg.GetChroma(L, h);
    if (!(c >= 0.0f) || !std::isfinite(c)) {
      std::fprintf(stderr, "[prmg-hue] h=%g gave chroma %g\n", (double)h, (double)c);
      fail("ordinary hue did not yield a finite non-negative chroma");
    }
  }

  // 3. Hue is periodic, and the reduction must preserve that. Values in this
  //    range terminated even before the fix, so this pins the behaviour the fix
  //    had to keep rather than the behaviour it changed.
  for (icFloatNumber h : ordinary) {
    if (prmg.GetChroma(L, h) != prmg.GetChroma(L, h + 720.0f))
      fail("hue + 720 did not match the base hue");
    if (prmg.GetChroma(L, h) != prmg.GetChroma(L, h - 720.0f))
      fail("hue - 720 did not match the base hue");
  }

  // 4. The regression itself. Every one of these hangs the unfixed loops; under
  //    the fix each returns immediately, and must agree with the reduction
  //    computed independently above.
  const icFloatNumber huge[] = {
    1.0e20f, -1.0e20f, 1.0e30f, -1.0e30f, 3.4e38f, -3.4e38f
  };
  for (icFloatNumber h : huge) {
    icFloatNumber c = prmg.GetChroma(L, h);

    if (!std::isfinite(c)) {
      std::fprintf(stderr, "[prmg-hue] h=%g gave non-finite chroma %g\n",
                   (double)h, (double)c);
      fail("large-magnitude hue produced a non-finite chroma");
      continue;
    }
    if (!(c >= 0.0f)) {
      std::fprintf(stderr, "[prmg-hue] h=%g gave chroma %g\n", (double)h, (double)c);
      fail("large-magnitude hue was rejected instead of reduced");
      continue;
    }

    icFloatNumber expected = prmg.GetChroma(L, normalizeHue(h));
    if (c != expected) {
      std::fprintf(stderr,
                   "[prmg-hue] h=%g -> chroma %g, but reduced hue %g -> %g\n",
                   (double)h, (double)c, (double)normalizeHue(h), (double)expected);
      fail("large-magnitude hue did not reduce to its fmod equivalent");
    }
  }

  // 5. The smallest and largest finite floats, as a boundary check on the
  //    reduction rather than on the table lookup.
  for (icFloatNumber h : {1.4e-45f, -1.4e-45f, 1.17549435e-38f}) {
    icFloatNumber c = prmg.GetChroma(L, h);
    if (!std::isfinite(c) || !(c >= 0.0f))
      fail("subnormal hue did not yield a finite non-negative chroma");
  }

  if (g_failures) {
    std::fprintf(stderr, "[prmg-hue] %d check(s) failed\n", g_failures);
    return 1;
  }

  std::fprintf(stdout,
               "[prmg-hue] hue normalization terminates and reduces correctly\n");
  return 0;
}
