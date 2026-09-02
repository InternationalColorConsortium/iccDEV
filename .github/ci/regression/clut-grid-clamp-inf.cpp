/*
    File:       clut-grid-clamp-inf.cpp

    Contains:   CTest helper pinning CIccCLUT's grid-coordinate clamp semantics

    Copyright:  (c) see below -- ICC Software License, Version 0.2
*/

// The Interp* functions used to scale each source component through the
// m_UnitClipFunc function pointer and then re-check the result with isfinite and
// a range clamp. That did the same work twice on the default path, and the
// indirect call is what stopped the sequence vectorizing, so it was replaced by
// an inline clamp (icClutGridClamp in IccTagLut.cpp).
//
// The replacement is exactly equivalent to the former default path
// (ClutUnitClip) for every input. It DELIBERATELY CHANGES one case on the former
// NoClip path, which the MPE and spectral CLUT elements installed via
// SetClipFunc:
//
//                    old ClutUnitClip     old NoClip      now (both)
//     +Inf           mx  (saturate high)  0  (collapse)   mx
//     -Inf           0                    0               0
//     NaN            0                    0               0
//     above range    mx                   mx              mx
//     below range    0                     0              0
//
// So the two paths disagreed about +Inf, at opposite ends of the grid, and now
// agree that it saturates high.
//
// This test exists because that change is invisible to everything else. The
// throughput harness reported no checksum movement at all, on any case, even
// with +Inf placed on every channel of every profile: the only profiles in the
// corpus that install the NoClip path are three-channel, and the only profile
// with enough channels to reach the wider interpolators has no CLUT element.
// Without this test the change would be unverified rather than merely untested.
//
// Exit code 0 = pass, 1 = the clamp semantics moved.

#include "IccTagLut.h"

#include <cmath>
#include <cstdio>
#include <limits>

static int g_failures = 0;

static bool check(bool cond, const char *msg)
{
  if (cond) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
  return cond;
}

// Identity passthrough mirroring the file-static NoClip in IccMpeBasic.cpp and
// IccMpeSpectral.cpp, which those files install via SetClipFunc. SetClipFunc is
// now a documented no-op; installing it here is the point -- the result must be
// the same either way.
static icFloatNumber NoClipIdentity(icFloatNumber v) { return v; }

// Builds a 1-input CLUT whose node values equal their own grid index, so the
// interpolated output reads back the clamped grid coordinate directly. With
// nGrid nodes the output for a saturated-high input must be the top index,
// nGrid-1.
static void run1d(const char *label, icFloatNumber in, icFloatNumber expect,
                  bool bInstallNoClip)
{
  const icUInt8Number nGrid = 4;

  CIccCLUT clut(1, 1);
  if (!check(clut.Init(nGrid), "Init built a 1-input CLUT"))
    return;

  icFloatNumber *pData = clut.GetData(0);
  for (icUInt8Number i = 0; i < nGrid; i++)
    pData[i] = (icFloatNumber)i;

  if (bInstallNoClip)
    clut.SetClipFunc(NoClipIdentity);

  clut.Begin();

  icFloatNumber dst = -1.0f;
  clut.Interp1d(&dst, &in);

  char buf[192];
  std::snprintf(buf, sizeof(buf), "%s: %s clip -> %.4f (expected %.4f)",
                label, bInstallNoClip ? "NoClip" : "default",
                (double)dst, (double)expect);
  check(std::fabs(dst - expect) < 1.0e-5f, buf);
}

int main()
{
  const icFloatNumber inf = std::numeric_limits<icFloatNumber>::infinity();
  const icFloatNumber nan = std::numeric_limits<icFloatNumber>::quiet_NaN();
  const icFloatNumber top = 3.0f;   // nGrid-1 for the 4-node CLUT above

  // The behaviour under test: +Inf saturates high on BOTH paths. Before the
  // change the second of these produced 0.
  std::printf("\n[ +Inf saturates high on both paths ]\n");
  run1d("+Inf", inf, top, false);
  run1d("+Inf", inf, top, true);

  // Everything else was already identical on both paths and must stay so.
  std::printf("\n[ unchanged cases ]\n");
  run1d("-Inf", -inf, 0.0f, false);
  run1d("-Inf", -inf, 0.0f, true);
  run1d("NaN", nan, 0.0f, false);
  run1d("NaN", nan, 0.0f, true);
  run1d("above range", 2.0f, top, false);
  run1d("above range", 2.0f, top, true);
  run1d("below range", -1.0f, 0.0f, false);
  run1d("below range", -1.0f, 0.0f, true);

  // In-range input still interpolates normally: 0.5 of the way up a grid whose
  // nodes equal their index is (nGrid-1)/2.
  std::printf("\n[ in-range input unaffected ]\n");
  run1d("mid", 0.5f, top / 2.0f, false);
  run1d("mid", 0.5f, top / 2.0f, true);

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
