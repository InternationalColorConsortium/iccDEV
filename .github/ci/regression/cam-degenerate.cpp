// CAM degenerate-state regressions.
//
// Cases 1-4 cover the original degenerate-parameter contract: a converter whose
// state cannot produce a meaningful result must return finite zeros rather than
// NaN or infinity.
//
// Cases 5-6 cover #1817. CIccCamConverter::H_FunctionInv inverts H_Function,
// which maps [0,inf) onto [0,400), so the inverse is only defined for
// 0 <= y < 400. It used to evaluate
//
//     pow (27.13*y / (400.0-y), 1.0/m_exp)
//
// with no domain check, so y == 400 divided by zero and y outside [0,400) made
// the base negative, which pow() with a fractional exponent answers with NaN.
// Either result propagated out through HyperbolicInv() into every channel of the
// XYZ triplet.
//
// This is reachable from profile data: JabToXYZ() feeds rgbP[] into
// HyperbolicInv(), which scales by H_Function(m_Fl), and m_Fl is derived from
// m_La. UBSan reported the division through iccApplyNamedCmm as
//
//     IccProfLib/IccCAM.cpp:150:36: runtime error: division by zero
//       #0 CIccCamConverter::H_FunctionInv(float)
//       #1 CIccCamConverter::HyperbolicInv(float)
//       #2 CIccCamConverter::JabToXYZ(float const*, float*, int)
//
// The checks below drive the public API only, since H_FunctionInv and
// HyperbolicInv are private. They assert on the *value* rather than relying on
// the sanitizer, because plain -fsanitize=undefined does not include
// float-divide-by-zero on either gcc or clang -- a value assertion fails on any
// compiler and in any build configuration, whereas the sanitizer only fires when
// the library is built with float-divide-by-zero enabled.
//
// Against the unfixed converter the sweep in case 5 produces NaN for 342 of its
// 3024 combinations; with the domain guard in place it produces none.

#include "IccCAM.h"

#include <cmath>
#include <cstdio>

static bool is_zero_triplet(const icFloatNumber *v)
{
  return std::fabs(v[0]) < 0.000001f &&
         std::fabs(v[1]) < 0.000001f &&
         std::fabs(v[2]) < 0.000001f;
}

static bool is_finite_triplet(const icFloatNumber *v)
{
  return std::isfinite((double)v[0]) &&
         std::isfinite((double)v[1]) &&
         std::isfinite((double)v[2]);
}

int main()
{
  CIccCamConverter cam;
  cam.SetParameter_Yb(0.0f);

  icFloatNumber xyz[3] = {0.25f, 0.50f, 0.75f};
  icFloatNumber jab[3] = {-1.0f, -1.0f, -1.0f};
  cam.XYZToJab(xyz, jab, 1);

  if (!is_finite_triplet(jab))
    return 1;
  if (!is_zero_triplet(jab))
    return 2;

  icFloatNumber inputJab[3] = {50.0f, 1.0f, -1.0f};
  icFloatNumber outputXyz[3] = {-1.0f, -1.0f, -1.0f};
  cam.JabToXYZ(inputJab, outputXyz, 1);

  if (!is_finite_triplet(outputXyz))
    return 3;
  if (!is_zero_triplet(outputXyz))
    return 4;

  // --- #1817: H_FunctionInv domain ------------------------------------------
  //
  // Sweep the adapting luminance, background luminance and Jab inputs across
  // magnitudes that push the HyperbolicInv() argument onto and past the pole at
  // 400. A very large m_La matters specifically: it makes H_Function(m_Fl) round
  // to exactly 400.0f in float, which is how a crafted profile lands the argument
  // precisely on the division by zero rather than merely past it.
  // The negative, infinite and NaN entries are #1950. This sweep originally ran
  // over non-negative adapting luminances only, which is exactly the gap that
  // defect lived in -- see case 7 below.
  static const icFloatNumber kLa[] = {0.0f, 1e-6f, 0.01f, 1.0f, 100.0f,
                                      1e6f, 1e12f, 1e30f, 1e38f,
                                      -1e-6f, -0.1f, -0.2f, -0.5f, -1.0f,
                                      -100.0f, -1e6f, -1e30f,
                                      INFINITY, -INFINITY, NAN};
  static const icFloatNumber kYb[] = {0.0f, 1e-6f, 20.0f, 100.0f, 1e6f, 1e30f};
  static const icFloatNumber kJ[]  = {-1e30f, -100.0f, 0.0f, 50.0f, 100.0f,
                                      1e6f, 1e30f, 1e38f};
  static const icFloatNumber kAB[] = {-1e30f, -1e6f, -100.0f, 0.0f,
                                      100.0f, 1e6f, 1e30f};

  for (size_t iLa = 0; iLa < sizeof(kLa) / sizeof(kLa[0]); iLa++) {
    for (size_t iYb = 0; iYb < sizeof(kYb) / sizeof(kYb[0]); iYb++) {
      for (size_t iJ = 0; iJ < sizeof(kJ) / sizeof(kJ[0]); iJ++) {
        for (size_t iAB = 0; iAB < sizeof(kAB) / sizeof(kAB[0]); iAB++) {
          CIccCamConverter sweepCam;
          icFloatNumber whitePoint[3] = {0.9642f, 1.0f, 0.8249f};

          sweepCam.SetParameter_WhitePoint(whitePoint);
          sweepCam.SetParameter_La(kLa[iLa]);
          sweepCam.SetParameter_Yb(kYb[iYb]);

          icFloatNumber sweepJab[3] = {kJ[iJ], kAB[iAB], kAB[iAB]};
          icFloatNumber sweepXyz[3] = {-1.0f, -1.0f, -1.0f};
          sweepCam.JabToXYZ(sweepJab, sweepXyz, 1);

          if (!is_finite_triplet(sweepXyz)) {
            std::fprintf(stderr,
                         "[cam-degenerate] FAIL (#1817): La=%g Yb=%g J=%g ab=%g"
                         " -> xyz=%g %g %g\n",
                         (double)kLa[iLa], (double)kYb[iYb],
                         (double)kJ[iJ], (double)kAB[iAB],
                         (double)sweepXyz[0], (double)sweepXyz[1],
                         (double)sweepXyz[2]);
            return 5;
          }
        }
      }
    }
  }

  // A converter left at its defaults must still round-trip an ordinary colour,
  // so the domain guard cannot have been written so broadly that it swallows
  // valid input and collapses everything to zero.
  CIccCamConverter roundTripCam;
  icFloatNumber roundTripWhite[3] = {0.9642f, 1.0f, 0.8249f};
  roundTripCam.SetParameter_WhitePoint(roundTripWhite);
  roundTripCam.SetParameter_La(100.0f);
  roundTripCam.SetParameter_Yb(20.0f);

  icFloatNumber midXyz[3] = {0.2f, 0.25f, 0.3f};
  icFloatNumber midJab[3] = {-1.0f, -1.0f, -1.0f};
  roundTripCam.XYZToJab(midXyz, midJab, 1);

  icFloatNumber backXyz[3] = {-1.0f, -1.0f, -1.0f};
  roundTripCam.JabToXYZ(midJab, backXyz, 1);

  if (!is_finite_triplet(midJab) || !is_finite_triplet(backXyz))
    return 6;

  for (int i = 0; i < 3; i++) {
    if (std::fabs(backXyz[i] - midXyz[i]) > 0.01f) {
      std::fprintf(stderr,
                   "[cam-degenerate] FAIL (#1817): round trip drifted on"
                   " channel %d: %g -> %g\n",
                   i, (double)midXyz[i], (double)backXyz[i]);
      return 6;
    }
  }

  // --- #1950: negative adapting luminance ------------------------------------
  //
  // CalcCoefficients computed k = 1.0f / (5.0f * m_La + 1.0f) with no domain
  // check on m_La. The denominator is exactly zero at m_La == -0.2f, because
  // 5.0f * -0.2f rounds to exactly -1.0f in float, so the pole is landed on
  // rather than approached. iccApplyProfiles reached it from profile data: the
  // colorEncodingParams white-point luminance defaults the ambient luminance to
  // Lw/5, so an s15Fixed16 Lw of -1.0 produces m_La == -0.2 exactly.
  //
  // Read case 7 together with the dbz helper in
  // .github/scripts/iccdev-cam-degenerate-regression-tests.sh. This case cannot
  // detect the division on its own and is not written as though it can: every
  // negative m_La, the one that divided by zero included, already returned
  // finite zeros through the public API, because the NaN that the same line
  // produced downstream was flushed to zero by the Hyperbolic()/HyperbolicInv()
  // guards. What case 7 pins is that contract -- that the guard added for the
  // division resolves to the degenerate zero state and does not start returning
  // NaN, infinity or some new non-zero answer for these luminances. The division
  // itself is caught by the helper, which compiles IccCAM.cpp with
  // -fsanitize=float-divide-by-zero regardless of how the library was built.
  static const icFloatNumber kNegLa[] = {-0.2f, -0.5f, -1.0f, -1e6f};

  for (size_t i = 0; i < sizeof(kNegLa) / sizeof(kNegLa[0]); i++) {
    CIccCamConverter negCam;
    icFloatNumber negWhite[3] = {0.9642f, 1.0f, 0.8249f};

    negCam.SetParameter_WhitePoint(negWhite);
    negCam.SetParameter_La(kNegLa[i]);
    negCam.SetParameter_Yb(20.0f);

    icFloatNumber negXyz[3] = {0.25f, 0.50f, 0.75f};
    icFloatNumber negJab[3] = {-1.0f, -1.0f, -1.0f};
    negCam.XYZToJab(negXyz, negJab, 1);

    icFloatNumber negInJab[3] = {50.0f, 1.0f, -1.0f};
    icFloatNumber negOutXyz[3] = {-1.0f, -1.0f, -1.0f};
    negCam.JabToXYZ(negInJab, negOutXyz, 1);

    if (!is_finite_triplet(negJab) || !is_zero_triplet(negJab) ||
        !is_finite_triplet(negOutXyz) || !is_zero_triplet(negOutXyz)) {
      std::fprintf(stderr,
                   "[cam-degenerate] FAIL (#1950): La=%g -> jab=%g %g %g"
                   " xyz=%g %g %g\n",
                   (double)kNegLa[i],
                   (double)negJab[0], (double)negJab[1], (double)negJab[2],
                   (double)negOutXyz[0], (double)negOutXyz[1],
                   (double)negOutXyz[2]);
      return 7;
    }
  }

  return 0;
}
