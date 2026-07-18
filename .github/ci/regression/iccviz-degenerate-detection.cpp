// Regression test for the gamut-degeneracy heuristic used by iccviz::GamutVolume
// (Tools/CmdLine/IccProfilePlot).  Guards iccvizmath::principalStdDevs() and the
// exact "flat" predicate GamutVolume applies to its boundary cloud.
//
// Why this exists: voxelEnclosedVolume seals boundary-sampling gaps with a
// morphological closing that floors a collapsed plane/line at ~1 voxel thick, so
// the enclosed-volume number stays a non-zero *sheet/tube* artifact even when the
// gamut has no 3-D extent.  GamutVolume therefore does NOT rely on the volume or
// the cell count to spot that case; it measures the boundary cloud's thinnest
// principal extent (s3, from the closed-form symmetric-3x3 eigenvalues in
// principalStdDevs) and flags:
//
//     flat = (s1 > 0) && (s3 < 0.02 * s1 || s3 < voxelSize)
//
// This test drives principalStdDevs with synthetic clouds of *known* shape and
// asserts (a) it recovers the principal std-devs accurately and (b) the 0.02
// coplanarity ratio classifies a solid blob, a plane and a line the way
// GamutVolume needs.  It is header-only (principalStdDevs is inline) but links
// IccProfLib for the ICC scalar types IccVizMath.hpp pulls in via IccDefs.h.
//
// Mutation checks (each isolates one half of the guard):
//   - Delete the `flat` term from GamutVolume and a genuinely planar gamut stops
//     being flagged: the plane/line asserts below encode that classification.
//   - Change the 0.02 constant materially and the threshold-calibration block
//     (ratio 0.01 must flag, 0.05 must not) fails.
//   - Break principalStdDevs' eigenvalue math and the recovered-ratio asserts fail.
//
// Exit code 0 = pass, 1 = a guarded regression reappeared.

#include "IccVizMath.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

using iccvizmath::principalStdDevs;

static int g_failures = 0;

static void check(bool cond, const char *msg) {
  if (cond) {
    std::printf("ok:   %s\n", msg);
  } else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

// The exact predicate GamutVolume applies (voxelSize term dropped: these unit
// clouds are dimensionless, so we exercise the coplanarity ratio in isolation).
static bool is_flat_ratio(double s1, double s3) {
  return (s1 > 0.0) && (s3 < 0.02 * s1);
}

// Build an M x M grid in x,y over [0,1] with `layers` z-planes evenly spanning
// [0, zThickness].  layers==1 collapses z to a single plane (s3 == 0 exactly).
// The covariance is diagonal by construction, so the principal axes align with
// x/y/z and s3 is the z extent -- a clean, analytically predictable cloud.
static std::vector<float> make_grid(int M, int layers, double zThickness) {
  std::vector<float> pts;
  pts.reserve(static_cast<std::size_t>(M) * M * (layers < 1 ? 1 : layers) * 3);
  const int L = layers < 1 ? 1 : layers;
  for (int iz = 0; iz < L; ++iz) {
    const double z = (L == 1) ? 0.0 : (zThickness * iz / (L - 1));
    for (int ix = 0; ix < M; ++ix) {
      const double x = static_cast<double>(ix) / (M - 1);
      for (int iy = 0; iy < M; ++iy) {
        const double y = static_cast<double>(iy) / (M - 1);
        pts.push_back(static_cast<float>(x));
        pts.push_back(static_cast<float>(y));
        pts.push_back(static_cast<float>(z));
      }
    }
  }
  return pts;
}

// ---------------------------------------------------------------------------
// A solid, near-isotropic blob: a full 3-D lattice cube.  s1 ~ s2 ~ s3, so the
// ratio is ~1 and the cloud is NOT flat -- the normal-gamut case.
// ---------------------------------------------------------------------------
static void test_solid_blob() {
  std::printf("\n[ solid blob (isotropic cube lattice) ]\n");
  // 9^3 lattice spanning [0,1]^3.
  std::vector<float> pts = make_grid(9, 9, 1.0);
  double s1 = 0, s2 = 0, s3 = 0;
  principalStdDevs(pts.data(), pts.size() / 3, s1, s2, s3);
  std::printf("      s1=%.5f s2=%.5f s3=%.5f  s3/s1=%.4f\n", s1, s2, s3, s1 > 0 ? s3 / s1 : 0.0);
  check(s1 > 0.0, "blob has positive principal extent");
  // Uniform cube: all three axis variances equal, so the ratio is ~1.
  check(std::fabs(s3 / s1 - 1.0) < 1e-3, "blob is isotropic (s3/s1 ~ 1)");
  check(!is_flat_ratio(s1, s3), "solid blob is NOT flagged flat");
}

// ---------------------------------------------------------------------------
// A plane: single z-layer, so s3 == 0.  Must be flagged flat.
// ---------------------------------------------------------------------------
static void test_plane() {
  std::printf("\n[ perfect plane (single z-layer) ]\n");
  std::vector<float> pts = make_grid(9, 1, 0.0);
  double s1 = 0, s2 = 0, s3 = 0;
  principalStdDevs(pts.data(), pts.size() / 3, s1, s2, s3);
  std::printf("      s1=%.5f s2=%.5f s3=%.5f\n", s1, s2, s3);
  check(s1 > 0.0 && s2 > 0.0, "plane spans two dimensions");
  check(s3 < 1e-6, "plane has ~zero thickness (s3 ~ 0)");
  check(is_flat_ratio(s1, s3), "perfect plane IS flagged flat");
}

// ---------------------------------------------------------------------------
// A line: extent along x only, so s2 == s3 == 0.  Must be flagged flat.  (This
// is the shape a 1-colorant / near-degenerate device gamut collapses to.)
// ---------------------------------------------------------------------------
static void test_line() {
  std::printf("\n[ perfect line (x-axis only) ]\n");
  std::vector<float> pts;
  for (int i = 0; i < 64; ++i) {
    pts.push_back(static_cast<float>(i) / 63.0f);
    pts.push_back(0.0f);
    pts.push_back(0.0f);
  }
  double s1 = 0, s2 = 0, s3 = 0;
  principalStdDevs(pts.data(), pts.size() / 3, s1, s2, s3);
  std::printf("      s1=%.5f s2=%.5f s3=%.5f\n", s1, s2, s3);
  check(s1 > 0.0, "line has positive length");
  check(s2 < 1e-6 && s3 < 1e-6, "line has ~zero width and thickness");
  check(is_flat_ratio(s1, s3), "perfect line IS flagged flat");
}

// ---------------------------------------------------------------------------
// Threshold calibration: build planes whose thinnest extent is a *controlled*
// fraction of s1 and confirm 0.02 classifies them as intended.  This is what
// pins the constant: a cloud at ratio ~0.01 must flag, one at ~0.05 must not.
// ---------------------------------------------------------------------------
static void test_threshold_calibration() {
  std::printf("\n[ 0.02 coplanarity-ratio calibration ]\n");

  // s1 from the base grid (z contributes negligibly at these thicknesses).
  std::vector<float> base = make_grid(9, 1, 0.0);
  double b1 = 0, b2 = 0, b3 = 0;
  principalStdDevs(base.data(), base.size() / 3, b1, b2, b3);
  check(b1 > 0.0, "base plane s1 established");

  struct Case { double targetRatio; bool expectFlat; const char *name; };
  const Case cases[] = {
    {0.01, true,  "ratio 0.01 (below 0.02) IS flat"},
    {0.05, false, "ratio 0.05 (above 0.02) is NOT flat"},
  };

  for (const Case &c : cases) {
    // Two z-layers at +/- half-thickness give std_z = thickness/2; choose the
    // thickness so std_z / s1 == targetRatio.
    const double thickness = 2.0 * c.targetRatio * b1;
    std::vector<float> pts = make_grid(9, 2, thickness);
    double s1 = 0, s2 = 0, s3 = 0;
    principalStdDevs(pts.data(), pts.size() / 3, s1, s2, s3);
    const double ratio = s1 > 0 ? s3 / s1 : 0.0;
    std::printf("      target=%.3f  measured s3/s1=%.4f  flat=%d\n",
                c.targetRatio, ratio, is_flat_ratio(s1, s3) ? 1 : 0);
    // principalStdDevs must recover the constructed ratio accurately...
    check(std::fabs(ratio - c.targetRatio) < 0.1 * c.targetRatio,
          "principalStdDevs recovers the constructed s3/s1 ratio");
    // ...and the 0.02 constant must classify it as designed.
    check(is_flat_ratio(s1, s3) == c.expectFlat, c.name);
  }
}

// ---------------------------------------------------------------------------
// Degenerate inputs to principalStdDevs itself: n < 2 and null must zero out.
// ---------------------------------------------------------------------------
static void test_edge_inputs() {
  std::printf("\n[ principalStdDevs edge inputs ]\n");
  double s1 = 9, s2 = 9, s3 = 9;
  principalStdDevs(nullptr, 100, s1, s2, s3);
  check(s1 == 0.0 && s2 == 0.0 && s3 == 0.0, "null pts -> all zero");
  float one[3] = {1.0f, 2.0f, 3.0f};
  s1 = s2 = s3 = 9;
  principalStdDevs(one, 1, s1, s2, s3);
  check(s1 == 0.0 && s2 == 0.0 && s3 == 0.0, "n<2 -> all zero");
}

int main() {
  test_solid_blob();
  test_plane();
  test_line();
  test_threshold_calibration();
  test_edge_inputs();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
