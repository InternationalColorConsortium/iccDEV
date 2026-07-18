// Regression test for the two scalar visualization metrics in the iccviz engine
// (Tools/CmdLine/IccProfilePlot/IccVizModel): GamutVolume and RoundTripDE.
//
// These sit outside the Enumerate/Render path and each builds CIccXform objects
// through CIccXform::Create(..., bOwnsProfile=false) + ShareProfile(), the
// borrowed-profile ownership path touched by the iccDEV #1712 review -- so
// exercising them end-to-end on a real LUT profile guards the metric numbers and
// keeps that construction path under test.  (It does NOT reproduce the underlying
// double-free: that only fires on Create()'s *failure* path, where a bOwnsProfile
// default of true deletes the still-borrowed profile; the happy path here calls
// ShareProfile() after a successful Create, so it cannot trap it.  The value of
// this test is the metric correctness + degeneracy signalling below.)
//
// Fixture: sRGB_v4_ICC_preference.icc (passed as argv[1]) -- an RGB profile with
// A2B0/A2B1/B2A0/B2A1 LUT tags, i.e. a genuine 3-D gamut, so GamutVolume must
// report a positive volume with degenerate==false, and RoundTripDE must round a
// grid of in-gamut Lab back through B2A/A2B to a small dE.
//
// Assertions (mutation-verified against the shipped implementation):
//   1. GamutVolume(A2B1, relative): ok, 3 colorants, positive finite volume in a
//      plausible sRGB band, > 27 voxels, and NOT degenerate.
//   2. GamutVolume with a deliberately coarse grid (huge voxelSize) still returns
//      ok but sets degenerate==true -- proving the degeneracy signal reaches the
//      caller (the complement of the not-degenerate assertion in #1).
//   3. RoundTripDE(relative): ok, positive point count, ordered mean<=p90<=max,
//      all finite, and a small mean dE (a well-behaved B2A/A2B inverse pair).
//
// Exit code 0 = pass, 1 = a guarded regression reappeared.

#include "IccProfile.h"
#include "IccDefs.h"

#include "IccVizModel.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char *msg) {
  if (cond) {
    std::printf("ok:   %s\n", msg);
  } else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

static bool is_finite(double v) { return std::isfinite(v); }

static void test_gamut_volume(CIccProfile *pIcc) {
  std::printf("\n[ GamutVolume (A2B1, relative colorimetric) ]\n");

  iccviz::GamutVolumeResult gv =
      iccviz::GamutVolume(pIcc, icSigAToB1Tag, icRelativeColorimetric);
  std::printf("      ok=%d volume=%.1f voxels=%lld colorants=%d "
              "samplesPerAxis=%d voxelSize=%.4f degenerate=%d\n",
              gv.ok, gv.volume, gv.voxels, gv.nColorants,
              gv.samplesPerAxis, gv.voxelSize, gv.degenerate);
  if (!gv.ok) {
    std::printf("      error: %s\n", gv.error.c_str());
    check(false, "GamutVolume succeeds on the RGB fixture");
    return;
  }
  check(gv.ok, "GamutVolume succeeds on the RGB fixture");
  check(gv.nColorants == 3, "GamutVolume reports 3 device colorants");
  check(is_finite(gv.volume) && gv.volume > 0.0, "GamutVolume is positive and finite");
  // Voxelized sRGB gamut lands near ~4-9e5 dE*ab^3; a broad band catches a gross
  // voxelization regression without being brittle to resolution tuning.
  check(gv.volume > 1.0e5 && gv.volume < 2.0e6, "GamutVolume is in the plausible sRGB band");
  check(gv.voxels > 27, "GamutVolume encloses more than the degeneracy floor of voxels");
  check(gv.samplesPerAxis >= 2 && gv.voxelSize > 0.0, "GamutVolume auto-picked sane sampling params");
  check(!gv.degenerate, "a real 3-D RGB gamut is NOT flagged degenerate");

  // Complement: force a degenerate result through the public API by collapsing the
  // whole Lab range into a handful of voxels (huge voxelSize) -- the cell-count
  // floor must set degenerate==true while the call still returns ok.
  iccviz::GamutVolumeResult gd =
      iccviz::GamutVolume(pIcc, icSigAToB1Tag, icRelativeColorimetric,
                          /*samplesPerAxis=*/2, /*voxelSize=*/1000.0, /*dilate=*/0);
  std::printf("      coarse-grid: ok=%d voxels=%lld degenerate=%d\n",
              gd.ok, gd.voxels, gd.degenerate);
  check(gd.ok, "coarse-grid GamutVolume still returns ok");
  check(gd.degenerate, "coarse-grid GamutVolume is flagged degenerate");
}

static void test_round_trip(CIccProfile *pIcc) {
  std::printf("\n[ RoundTripDE (relative colorimetric) ]\n");

  iccviz::RoundTripResult rt =
      iccviz::RoundTripDE(pIcc, icRelativeColorimetric);
  std::printf("      ok=%d n=%d colorants=%d meanDE=%.4f p90DE=%.4f maxDE=%.4f stdDE=%.4f\n",
              rt.ok, rt.n, rt.nColorants, rt.meanDE, rt.p90DE, rt.maxDE, rt.stdDE);
  if (!rt.ok) {
    std::printf("      error: %s\n", rt.error.c_str());
    check(false, "RoundTripDE succeeds on the RGB fixture");
    return;
  }
  check(rt.ok, "RoundTripDE succeeds on the RGB fixture");
  check(rt.nColorants == 3, "RoundTripDE reports 3 device colorants");
  check(rt.n > 0, "RoundTripDE evaluated at least one in-gamut seed");
  check(is_finite(rt.meanDE) && is_finite(rt.p90DE) && is_finite(rt.maxDE) && is_finite(rt.stdDE),
        "RoundTripDE statistics are all finite");
  check(rt.meanDE >= 0.0 && rt.meanDE <= rt.p90DE + 1e-6 && rt.p90DE <= rt.maxDE + 1e-6,
        "RoundTripDE stats are ordered (mean <= p90 <= max)");
  // A sane B2A/A2B inverse pair round-trips in-gamut Lab to a small mean dE.
  check(rt.meanDE < 5.0, "RoundTripDE mean dE is small (well-behaved inverse)");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s profile.icc\n", argv[0]);
    return 2;
  }

  // The model echoes diagnostics to stderr by default; silence it for a clean log.
  iccviz::SetSilent(true);

  CIccProfile *pIcc = OpenIccProfile(argv[1]);
  if (!pIcc) {
    std::fprintf(stderr, "could not open profile: %s\n", argv[1]);
    return 1;
  }

  test_gamut_volume(pIcc);
  test_round_trip(pIcc);

  delete pIcc;

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
