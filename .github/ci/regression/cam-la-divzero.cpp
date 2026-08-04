// #1950: division by zero in CIccCamConverter::CalcCoefficients for a negative
// adapting luminance.
//
// CalcCoefficients computed
//
//     k = 1.0f / (5.0f * m_La + 1.0f);
//
// with no domain check on m_La. The denominator is exactly zero when m_La is
// -0.2f: 5.0f * -0.2f rounds to exactly -1.0f in float, so the pole is landed on
// precisely rather than approached. Every negative m_La also made the following
// line take a fractional power of a negative number, leaving NaN in m_Fl.
//
// The value is profile-controlled. icConvertEncodingProfile passes the
// colorEncodingParams ambient white-point luminance straight to
// SetParameter_La, defaulting it to the white-point luminance divided by five
// (IccEncoding.cpp), so an s15Fixed16 ceptWhitePointLuminanceMbr of -1.0 gives
// m_La == -0.2 exactly. UBSan reported it through iccApplyProfiles as
//
//     IccProfLib/IccCAM.cpp:306:11: runtime error: division by zero
//
// This helper is compiled together with IccProfLib/IccCAM.cpp under
// -fsanitize=float-divide-by-zero by
// .github/scripts/iccdev-cam-degenerate-regression-tests.sh. Building the
// converter into the helper is the point of the file: the division is inside
// IccProfLib, so instrumenting only the helper would not see it, and no CI lane
// both builds the library with float-divide-by-zero and runs ctest. Compiling
// the translation unit here makes the check independent of how the library was
// configured.
//
// There is deliberately nothing to assert on. A negative adapting luminance
// already produced finite zeros through the public API before the guard existed,
// because the NaN was flushed to zero downstream by the Hyperbolic() and
// HyperbolicInv() guards, so no return value distinguishes the defective
// converter from the fixed one. The sanitizer report is the signal, and the
// script fails the case on "runtime error:" in the output. The value contract
// for these same luminances is asserted separately, as case 7 of
// cam-degenerate.cpp.

#include "IccCAM.h"

#include <cstdio>

int main()
{
  // -0.2f is the pole itself. The neighbours are carried along so that a future
  // guard written only for the exact value, rather than for the domain, still
  // has the rest of the negative range exercised against the sanitizer.
  static const icFloatNumber kLa[] = {-0.2f, -0.1f, -0.5f, -1.0f, -100.0f,
                                      -1e6f, -1e-6f};

  icFloatNumber whitePoint[3] = {0.9642f, 1.0f, 0.8249f};

  for (size_t i = 0; i < sizeof(kLa) / sizeof(kLa[0]); i++) {
    CIccCamConverter cam;

    cam.SetParameter_WhitePoint(whitePoint);
    cam.SetParameter_La(kLa[i]);
    cam.SetParameter_Yb(20.0f);

    icFloatNumber xyz[3] = {0.25f, 0.50f, 0.75f};
    icFloatNumber jab[3] = {-1.0f, -1.0f, -1.0f};
    cam.XYZToJab(xyz, jab, 1);

    icFloatNumber inJab[3] = {50.0f, 1.0f, -1.0f};
    icFloatNumber outXyz[3] = {-1.0f, -1.0f, -1.0f};
    cam.JabToXYZ(inJab, outXyz, 1);
  }

  std::printf("[cam-la-divzero] swept %zu negative adapting luminances\n",
              sizeof(kLa) / sizeof(kLa[0]));
  return 0;
}
