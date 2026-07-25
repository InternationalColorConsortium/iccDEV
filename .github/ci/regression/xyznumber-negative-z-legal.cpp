// Regression for #1808: a negative Z component of an XYZNumber is legal and must
// validate as a Warning, while a negative X or Y must stay NonCompliant.
//
// iccV5DspObsToV4Dsp integrates against a custom observer and then applies a
// custom->standard (D65->D50) PCC, which emits a redColorantTag whose Z is slightly
// negative. CIccInfo::CheckData classified that as icValidateNonCompliant, so a
// correctly-generated profile was reported as violating the specification. Per Phil
// Green (ICC, #1808): "Negative z-values can arise in some computations (e.g.
// chromatic adaptation) and are legal." The generator is right; the validator was
// wrong, and only its Z branch was relaxed to icValidateWarning.
//
// The asymmetry is the whole point of this test, so it pins both directions. A direct
// CIE integral cannot go negative (the 1931 2-deg / 1964 10-deg observers are
// non-negative at every wavelength), so any negative component implies a non-CIE
// basis, a signed transform applied after the integral (chromatic adaptation / PCC
// matrices -- the #1808 case), or non-physical spectral input. Those mechanisms are
// not channel-specific in principle, but their incidence is: zbar's narrow support
// plus the strong short-wave scaling of D65<->D50 adaptation make a negative Z
// common, a negative X rare and usually a bad matrix or input, and a negative Y
// (negative luminance) effectively always corruption. Hence Z warns and X/Y reject.
//
// Both CheckData overloads are covered. Legality is a property of the physical
// quantity rather than its encoding, so the fixed-point icXYZNumber and float
// icFloatXYZNumber paths must return the same verdict for the same colour --
// otherwise a value would validate differently depending only on how the tag stores
// it. The test calls CheckData directly, so it needs no profile fixture and is fast.
//
// Red-green: reverting the Z branch to icValidateNonCompliant fails the "negative Z
// warns" assertions; over-applying the downgrade to X or Y fails the guard
// assertions that keep those NonCompliant.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccUtil.h"
#include "IccDefs.h"

#include <cstdio>
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
    std::fprintf(stderr, "[xyz-negative-z] FAIL: %s\n", what);
  }
}

// Build a fixed-point XYZNumber from doubles. icDtoF applies the s15Fixed16
// encoding used on the wire, which is signed -- a negative component is
// representable, which is why the validator (not the encoding) is what decides
// whether such a value is acceptable.
icXYZNumber makeXYZ(double x, double y, double z)
{
  icXYZNumber v;
  v.X = icDtoF((icFloatNumber)x);
  v.Y = icDtoF((icFloatNumber)y);
  v.Z = icDtoF((icFloatNumber)z);
  return v;
}

icFloatXYZNumber makeFloatXYZ(double x, double y, double z)
{
  icFloatXYZNumber v;
  v.X = (icFloat32Number)x;
  v.Y = (icFloat32Number)y;
  v.Z = (icFloat32Number)z;
  return v;
}

// A plausible D50 media white point, used as the all-positive control and as the
// base the sign-flipped cases perturb one component of.
const double kWpX = 0.9642;
const double kWpY = 1.0000;
const double kWpZ = 0.8249;

// Each case re-runs against a fresh report string so one verdict cannot be
// contaminated by a previous case's appended text.
icValidateStatus checkFixed(const icXYZNumber &v, std::string &sReport)
{
  CIccInfo info;
  sReport.clear();
  return info.CheckData(sReport, v, "test");
}

icValidateStatus checkFloat(const icFloatXYZNumber &v, std::string &sReport)
{
  CIccInfo info;
  sReport.clear();
  return info.CheckData(sReport, v, "test");
}

// The reports are prefixed with icMsgValidateWarning / icMsgValidateNonCompliant, so
// asserting on the severity word as well as the return code catches a change that
// updates one but not the other (the report text is what users actually read).
bool mentions(const std::string &sReport, const char *needle)
{
  return sReport.find(needle) != std::string::npos;
}

} // namespace

int main()
{
  std::string report;

  // --- fixed-point icXYZNumber ------------------------------------------------

  check(checkFixed(makeXYZ(kWpX, kWpY, kWpZ), report) == icValidateOK,
        "all-positive XYZNumber should validate OK");
  check(!mentions(report, "Negative"),
        "all-positive XYZNumber should not report a negative component");

  // The #1808 case: legal per ICC, so a Warning and explicitly NOT NonCompliant.
  check(checkFixed(makeXYZ(kWpX, kWpY, -0.05), report) == icValidateWarning,
        "negative Z XYZNumber should validate as Warning (#1808)");
  check(mentions(report, "Negative Z value!"),
        "negative Z XYZNumber should report the negative Z message");
  check(mentions(report, "Warning"),
        "negative Z XYZNumber report should carry the Warning prefix");
  check(!mentions(report, "NonCompliant"),
        "negative Z XYZNumber must not be reported as NonCompliant (#1808)");

  // Guards: the downgrade must not have leaked to the other two channels.
  check(checkFixed(makeXYZ(-0.05, kWpY, kWpZ), report) == icValidateNonCompliant,
        "negative X XYZNumber should stay NonCompliant");
  check(mentions(report, "Negative X value!") && mentions(report, "NonCompliant"),
        "negative X XYZNumber should report NonCompliant");

  check(checkFixed(makeXYZ(kWpX, -0.05, kWpZ), report) == icValidateNonCompliant,
        "negative Y XYZNumber should stay NonCompliant");
  check(mentions(report, "Negative Y value!") && mentions(report, "NonCompliant"),
        "negative Y XYZNumber should report NonCompliant");

  // A negative X combined with a negative Z must still reject: icMaxStatus takes the
  // most severe verdict, so the Z warning cannot mask the X violation.
  check(checkFixed(makeXYZ(-0.05, kWpY, -0.05), report) == icValidateNonCompliant,
        "negative X and Z together should stay NonCompliant");

  // --- float icFloatXYZNumber -------------------------------------------------
  // Same expectations: the verdict must not depend on the encoding.

  check(checkFloat(makeFloatXYZ(kWpX, kWpY, kWpZ), report) == icValidateOK,
        "all-positive FloatXYZNumber should validate OK");

  check(checkFloat(makeFloatXYZ(kWpX, kWpY, -0.05), report) == icValidateWarning,
        "negative Z FloatXYZNumber should validate as Warning (#1808)");
  check(mentions(report, "Negative Z value!"),
        "negative Z FloatXYZNumber should report the negative Z message");
  check(!mentions(report, "NonCompliant"),
        "negative Z FloatXYZNumber must not be reported as NonCompliant (#1808)");

  check(checkFloat(makeFloatXYZ(-0.05, kWpY, kWpZ), report) == icValidateNonCompliant,
        "negative X FloatXYZNumber should stay NonCompliant");
  check(checkFloat(makeFloatXYZ(kWpX, -0.05, kWpZ), report) == icValidateNonCompliant,
        "negative Y FloatXYZNumber should stay NonCompliant");

  if (g_fail)
    std::fprintf(stderr, "[xyz-negative-z] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[xyz-negative-z] all assertions passed\n");

  return g_fail;
}
