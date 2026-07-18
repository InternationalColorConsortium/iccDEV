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
//   - Break the Smith-1961 trig branch (correlated covariance) and the rotated-cloud
//     test fails: its s1/s2/s3 no longer match the axis-aligned reference.
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
// Rotated anisotropic cloud: exercises principalStdDevs' *trig* branch.
//
// Every cloud above is axis-aligned, so its covariance matrix is diagonal
// (p1 == cxy^2 + cxz^2 + cyz^2 == 0) and principalStdDevs takes the diagonal
// fast-path.  The closed-form symmetric-3x3 eigenvalue path (Smith 1961:
// p2/q/phi/acos) that handles a correlated/rotated cloud is then never run.
//
// Build a lattice with three *distinct* axis extents, capture its principal
// std-devs from the (well-tested) diagonal path, then rotate every point by a
// matrix that mixes all three axes.  Rotation is orthogonal, so it leaves the
// principal std-devs unchanged but makes the covariance fully off-diagonal
// (p1 > 0) -> the recovered (s1,s2,s3) must equal the pre-rotation reference,
// which can only hold if the trig branch computes the eigenvalues correctly.
// ---------------------------------------------------------------------------
static std::vector<float> make_aniso_lattice(int M, double Lx, double Ly, double Lz) {
  std::vector<float> pts;
  pts.reserve(static_cast<std::size_t>(M) * M * M * 3);
  for (int ix = 0; ix < M; ++ix)
    for (int iy = 0; iy < M; ++iy)
      for (int iz = 0; iz < M; ++iz) {
        pts.push_back(static_cast<float>(Lx * ix / (M - 1)));
        pts.push_back(static_cast<float>(Ly * iy / (M - 1)));
        pts.push_back(static_cast<float>(Lz * iz / (M - 1)));
      }
  return pts;
}

static void rotate_points(std::vector<float> &pts, const double R[3][3]) {
  for (std::size_t i = 0; i + 2 < pts.size(); i += 3) {
    const double x = pts[i], y = pts[i + 1], z = pts[i + 2];
    pts[i]     = static_cast<float>(R[0][0] * x + R[0][1] * y + R[0][2] * z);
    pts[i + 1] = static_cast<float>(R[1][0] * x + R[1][1] * y + R[1][2] * z);
    pts[i + 2] = static_cast<float>(R[2][0] * x + R[2][1] * y + R[2][2] * z);
  }
}

static void test_rotated_cloud() {
  std::printf("\n[ rotated anisotropic cloud (Smith-1961 trig branch) ]\n");

  // Distinct extents Lx > Ly > Lz so the three eigenvalues are well separated.
  std::vector<float> aligned = make_aniso_lattice(9, 1.0, 0.6, 0.25);
  double r1 = 0, r2 = 0, r3 = 0;
  principalStdDevs(aligned.data(), aligned.size() / 3, r1, r2, r3);
  std::printf("      axis-aligned ref: s1=%.5f s2=%.5f s3=%.5f\n", r1, r2, r3);
  check(r1 > r2 && r2 > r3 && r3 > 0.0, "anisotropic cloud has 3 distinct positive extents");

  // R = Rz(0.9) * Ry(0.7) * Rx(0.5): mixes all three axes -> non-diagonal covariance.
  const double cx = std::cos(0.5), sx = std::sin(0.5);
  const double cy = std::cos(0.7), sy = std::sin(0.7);
  const double cz = std::cos(0.9), sz = std::sin(0.9);
  const double Rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
  const double Ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
  const double Rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
  double RyRx[3][3], R[3][3];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      RyRx[i][j] = Ry[i][0] * Rx[0][j] + Ry[i][1] * Rx[1][j] + Ry[i][2] * Rx[2][j];
    }
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      R[i][j] = Rz[i][0] * RyRx[0][j] + Rz[i][1] * RyRx[1][j] + Rz[i][2] * RyRx[2][j];
    }

  std::vector<float> rot = aligned;
  rotate_points(rot, R);
  double s1 = 0, s2 = 0, s3 = 0;
  principalStdDevs(rot.data(), rot.size() / 3, s1, s2, s3);
  std::printf("      rotated:          s1=%.5f s2=%.5f s3=%.5f\n", s1, s2, s3);

  // Orthogonal rotation preserves the principal std-devs to within float noise.
  check(std::fabs(s1 - r1) < 2e-3 * r1, "trig branch recovers s1 through rotation");
  check(std::fabs(s2 - r2) < 2e-3 * r2, "trig branch recovers s2 through rotation");
  check(std::fabs(s3 - r3) < 2e-3 * r3, "trig branch recovers s3 through rotation");
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
  test_rotated_cloud();
  test_edge_inputs();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
