/*
    File:       luminance-normalization-diagnostic.cpp

    Contains:   CTest helper for CIccInfo::CheckLuminance()'s normalized-luminance
                diagnostic (#1811).

    spectralViewingConditionsTag's illuminantXYZ and surroundXYZ carry absolute
    luminance in cd/m^2.  A producer that writes chromaticity-normalized values
    instead leaves Y at exactly 1, and CheckLuminance() reports that as a warning.

    #1811 asked for the diagnostic to be exercised at normalized and at physical
    values, naming 1, 300 and 500 as representative.  Those three are not an
    arbitrary spread -- Testing/Display carries the same profile at each of them:

        sRGB_D65_MAT.xml        X=0.9504    Y=1.0    Z=1.0889
        sRGB_D65_MAT-300lx.xml  X=285.12    Y=300    Z=326.67   ( = 300 x the above )
        sRGB_D65_MAT-500lx.xml  X=475.20    Y=500    Z=544.45   ( = 500 x the above )

    which is also why value 1 is the interesting one.  Scaling the 300 cd/m^2
    fixture down to 1 cd/m^2 reproduces the normalized fixture exactly, so the two
    readings of Y = 1 -- "normalized" and "a legal 1 cd/m^2 dark surround" -- are
    the same three numbers.  No test on the tag's own values can separate them, and
    a 1 cd/m^2 surround is not hypothetical: the BT.2100 fixtures in this corpus
    use 5 cd/m^2.  The diagnostic is therefore phrased as a possibility and left at
    Warning, and this test pins that -- both that it still fires on the normalized
    case and that it stays a Warning rather than escalating.

    Testing CheckLuminance() directly rather than through
    CIccTagSpectralViewingConditions::Validate() keeps the subject isolated: that
    Validate() also reports a critical error for a missing observer CMF, which would
    mask the status under test.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccUtil.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

static void check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "luminance-normalization-diagnostic: PASS  %s\n", label);
    return;
  }
  std::fprintf(stdout, "luminance-normalization-diagnostic: FAIL  %s\n", label);
  ++g_failures;
}

// D65 chromaticity, which is what every fixture in the affected corpus population
// uses.  Scaling it is what turns a normalized triple into a physical one, so the
// helper takes the luminance and derives X and Z from it.
static icFloatXYZNumber d65AtLuminance(icFloatNumber Y)
{
  icFloatXYZNumber XYZ;
  XYZ.X = static_cast<icFloatNumber>(0.9504 * Y);
  XYZ.Y = Y;
  XYZ.Z = static_cast<icFloatNumber>(1.0889 * Y);
  return XYZ;
}

// Returns the status and hands back the report so the caller can assert on the
// message text as well.  sReport starts empty each time: CheckLuminance appends,
// and a shared buffer would let one case's message satisfy the next case's check.
static icValidateStatus runCheck(icFloatNumber Y, std::string& sReport)
{
  CIccInfo Info;
  sReport.clear();
  return Info.CheckLuminance(sReport, d65AtLuminance(Y), "spectralViewingConditionsTag::>illuminantXYZ");
}

int main()
{
  std::string sReport;

  // Y = 1: the normalized case, and simultaneously a legal 1 cd/m^2 reading.  It
  // must warn, and it must not do more than warn.
  check(runCheck(1.0f, sReport) == icValidateWarning, "Y=1 reports a warning");
  check(sReport.find("possibly normalized") != std::string::npos,
        "Y=1 message is phrased as a possibility, not an assertion");
  check(sReport.find("cd/m^2") != std::string::npos,
        "Y=1 message states the expected unit");

  // The two physical values #1811 names.  Both are far outside the window, so a
  // clean report here is what tells a reviewer the diagnostic is not simply always on.
  check(runCheck(300.0f, sReport) == icValidateOK, "Y=300 is accepted");
  check(sReport.empty(), "Y=300 adds nothing to the report");

  check(runCheck(500.0f, sReport) == icValidateOK, "Y=500 is accepted");
  check(sReport.empty(), "Y=500 adds nothing to the report");

  // The window is an absolute +/-0.01 around 1.0, not a relative tolerance.  Pinning
  // both sides keeps a later change from widening it into the physical range, where
  // it would start flagging genuinely dark surrounds: 5 cd/m^2 is in use in this
  // corpus today and must stay clean.
  check(runCheck(0.995f, sReport) == icValidateWarning, "Y=0.995 is inside the window");
  check(runCheck(1.005f, sReport) == icValidateWarning, "Y=1.005 is inside the window");
  check(runCheck(0.98f, sReport) == icValidateOK, "Y=0.98 is outside the window");
  check(runCheck(1.02f, sReport) == icValidateOK, "Y=1.02 is outside the window");
  check(runCheck(5.0f, sReport) == icValidateOK, "Y=5 (BT.2100 surround) is accepted");

  if (g_failures) {
    std::fprintf(stdout, "luminance-normalization-diagnostic: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "luminance-normalization-diagnostic: all checks passed\n");
  return 0;
}
