// Regression test for the CIccCLUT::Interp6d() non-finite float->unsigned cast
// fixed for #1504 in IccProfLib/IccTagLut.cpp.
//
// Interp6d converts each of the six grid coordinates g0..g5 to an integer grid
// index with `(icUInt32Number)gN`.  A finiteness guard was added that clamps a
// non-finite gN to 0 -- but only for g0..g4; g5 was missed.  Because the value
// clamps below it are `gN < 0` and `gN > mN`, and NaN compares false against
// both, a NaN/Inf in srcPixel[5] flowed straight into `(icUInt32Number)g5`,
// which is undefined behaviour (UBSan: "nan is outside the range of
// representable values of type 'unsigned int'" at IccTagLut.cpp:3290).  The fix
// adds the matching `if (!std::isfinite(g5)) g5 = 0.0f;` so g5 is sanitised
// exactly like g0..g4.
//
// The non-finite coordinate only reaches the cast unclamped when the CLUT's
// clip function is the identity NoClip passthrough rather than the default
// ClutUnitClip (which maps NaN/out-of-range to a finite value).  The spectral
// MPE apply path installs NoClip via SetClipFunc (IccMpeSpectral.cpp), which is
// exactly how the external PoC (iccApplyNamedCmm on a 6-channel spectral
// profile, bisected to 1f0a9dd, 2015) drove a NaN into Interp6d.  This test
// reproduces that configuration directly: it builds a 6-input CLUT, installs an
// identity clip function, and feeds a non-finite srcPixel[5].
//
// This test must be built/run under the float-cast-overflow / UBSan sanitizer
// (the CI ASAN+UBSAN tool job): pre-fix the g5 cast traps and aborts the
// process; post-fix Interp6d completes and returns finite output.
//
// Exit code 0 = pass, 1 = the guarded regression reappeared.

#include "IccTagLut.h"

#include <cstdio>
#include <cmath>

static int g_failures = 0;
static bool check(bool cond, const char *msg) {
  if (cond) { std::printf("ok:   %s\n", msg); }
  else      { std::printf("FAIL: %s\n", msg); ++g_failures; }
  return cond;
}

// Identity passthrough mirroring IccMpeSpectral.cpp's file-static NoClip: the
// spectral apply path installs this via SetClipFunc, disabling the default
// ClutUnitClip NaN/range clamp.  That is the precondition under which a
// non-finite coordinate actually reaches the (icUInt32Number) cast in Interp6d.
static icFloatNumber NoClipIdentity(icFloatNumber v) { return v; }

// Build a 6-input / 3-output CLUT with the identity clip function, push a
// srcPixel whose 6th component is the given non-finite value through Interp6d,
// and verify the call neither traps under the float-cast sanitizer nor returns
// a non-finite result.
static void run6d(const char *label, icFloatNumber bad) {
  std::printf("\n[ %s ]\n", label);

  CIccCLUT clut(6, 3);                 // 6 inputs selects the Interp6d path
  if (!check(clut.Init((icUInt8Number)2), "Init built a 6x2 CLUT (g5 path live)"))
    return;

  // Init allocates but does not initialise the node data; zero it so a clean
  // interpolation (post-fix, every gN clamped finite) yields a finite result
  // rather than reading indeterminate float bits.
  icFloatNumber *pData = clut.GetData(0);
  const int nData = (int)clut.NumPoints() * (int)clut.GetOutputChannels();
  for (int i = 0; i < nData; i++)
    pData[i] = 0.0f;

  clut.SetClipFunc(NoClipIdentity);    // replicate the spectral NoClip apply path

  // Begin() computes m_MaxGridPoint and the node-offset tables; the real apply
  // path always calls it before any Interp.  Without it m_MaxGridPoint is 0 and
  // the (separate, unrelated) ig==max decrement underflows -- a harness setup
  // requirement, not part of the #1504 cast under test.
  clut.Begin();

  icFloatNumber src[6] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, bad };
  icFloatNumber dst[3] = { 0.0f, 0.0f, 0.0f };

  // Pre-fix: the unguarded (icUInt32Number)g5 cast of a non-finite value traps
  // and aborts the process here under the float-cast-overflow sanitizer.
  clut.Interp6d(dst, src);

  check(std::isfinite(dst[0]) && std::isfinite(dst[1]) && std::isfinite(dst[2]),
        "Interp6d returned finite output for non-finite srcPixel[5]");
}

int main() {
  // NaN slips past ClutUnitClip-style range checks (NaN compares false against
  // every bound); +Inf would survive an identity clip too.  Both must reach the
  // cast as 0 after the fix.
  run6d("Interp6d srcPixel[5] = NaN  -- #1504", (icFloatNumber)NAN);
  run6d("Interp6d srcPixel[5] = +Inf -- #1504", (icFloatNumber)INFINITY);

  // Sanity: an all-valid srcPixel must still interpolate cleanly (the existing
  // g0..g4 guards and the new g5 guard are value-preserving for finite input).
  run6d("Interp6d srcPixel[5] = 0.5  -- valid input unchanged", (icFloatNumber)0.5f);

  if (g_failures) { std::printf("\n%d check(s) FAILED\n", g_failures); return 1; }
  std::printf("\nall checks passed\n");
  return 0;
}
