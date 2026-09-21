/*
    File:       formula-segment-nonfinite-parameter.cpp

    Contains:   CTest helper for a formulaCurveSegment with a NaN or infinite
                parameter.

    The binary reader copies the parameters as float32 without looking at
    them, so a profile can carry a NaN or infinite parameter.  The segment's
    output is then NaN or infinite wherever the formula reads that parameter,
    which for most types and positions is every input.  Before #2547 Begin()
    accepted it and Validate() checked only the parameter count, so the
    profile validated clean and the CMM applied it.

    The contract pinned here, for each function type 0 to 7 and for a NaN,
    +inf and -inf in each parameter position:
      - Validate() reports a critical error naming the non-finite parameter.
      - Begin() refuses the segment.
    The same parameters with finite values are the control: Begin() accepts
    them and Validate() reports no error, so a refusal can only come from the
    non-finite value.  One case also goes through Write() and Read(), to show
    the binary reader still loads the value that Validate() then reports.
*/

#include "IccDefs.h"
#include "IccIO.h"
#include "IccMpeBasic.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

static int g_failures = 0;

static void check(bool cond, const char* msg)
{
  if (cond) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

static const char* kNonFiniteReport = "non-finite formulaCurveSegment parameter";

/* ICC.2-2023 Table 111 parameter counts, and finite values for each type that
   Begin() accepts. */
struct FormulaType {
  icUInt16Number type;
  icUInt8Number nParams;
  icFloatNumber params[7];
};

static const FormulaType kTypes[] = {
  { 0, 4, { 2.0f, 1.0f, 0.0f, 0.0f } },
  { 1, 5, { 2.0f, 1.0f, 1.0f, 1.0f, 0.0f } },
  { 2, 5, { 1.0f, 2.0f, 1.0f, 0.0f, 0.0f } },
  { 3, 5, { 2.0f, 1.0f, 1.0f, 0.0f, 0.0f } },
  { 4, 5, { 2.0f, 1.0f, -1.0f, 0.0f, 1.0f } },
  { 5, 6, { 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f } },
  { 6, 7, { 0.5f, 0.5f, 0.0f, 1.0f, 0.5f, 1.0f, 1.0f } },
  { 7, 6, { 0.5f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f } },
};

static void checkSegment(const CIccFormulaCurveSegment& seg, bool bExpectRefused, const char* what)
{
  CIccFormulaCurveSegment copy(seg);
  std::string report;
  icValidateStatus status = copy.Validate("", report);
  bool bReported = report.find(kNonFiniteReport) != std::string::npos;
  bool bBegan = copy.Begin(NULL);

  char msg[160];
  if (bExpectRefused) {
    std::snprintf(msg, sizeof(msg), "%s: Validate() reports a critical error", what);
    check(status == icValidateCriticalError && bReported, msg);
    std::snprintf(msg, sizeof(msg), "%s: Begin() refuses", what);
    check(!bBegan, msg);
  }
  else {
    // The control is about the PARAMETERS: every one is finite, so the non-finite
    // report must be absent and the segment must be accepted.  It is deliberately
    // declared over the whole float range, the shape a segmented curve's first and
    // last segments actually take, and that range is not domain-clean for every
    // formula: the type 6 and 7 controls carry a fractional g, so their base is
    // negative below zero, and type 6's denominator reaches zero inside it.  Those
    // are Warnings this file is not about -- iccdev.formula-segment-negative-domain
    // owns them -- so the assertion is "accepted, and not reported as non-finite"
    // rather than a bare icValidateOK, which would make this test fail whenever a
    // sibling adds a true range diagnostic.
    std::snprintf(msg, sizeof(msg), "%s: Validate() reports no parameter error", what);
    check(status < icValidateCriticalError && !bReported, msg);
    std::snprintf(msg, sizeof(msg), "%s: Begin() accepts", what);
    check(bBegan, msg);
  }
  if (!report.empty() &&
      (bExpectRefused ? !bReported : status >= icValidateCriticalError))
    std::printf("      report: %s", report.c_str());
}

int main()
{
  const icFloatNumber kBad[] = {
    std::numeric_limits<icFloatNumber>::quiet_NaN(),
    std::numeric_limits<icFloatNumber>::infinity(),
    -std::numeric_limits<icFloatNumber>::infinity(),
  };
  const char* kBadName[] = { "NaN", "+inf", "-inf" };

  for (const FormulaType& t : kTypes) {
    icFloatNumber params[7];
    char what[96];

    CIccFormulaCurveSegment control(icMinFloat32Number, icMaxFloat32Number);
    for (int i = 0; i < t.nParams; i++)
      params[i] = t.params[i];
    control.SetFunction(t.type, t.nParams, params);
    std::snprintf(what, sizeof(what), "type %u finite control", t.type);
    checkSegment(control, false, what);

    for (int b = 0; b < 3; b++) {
      for (int p = 0; p < t.nParams; p++) {
        for (int i = 0; i < t.nParams; i++)
          params[i] = t.params[i];
        params[p] = kBad[b];

        CIccFormulaCurveSegment seg(icMinFloat32Number, icMaxFloat32Number);
        seg.SetFunction(t.type, t.nParams, params);
        std::snprintf(what, sizeof(what), "type %u parameter %d %s", t.type, p, kBadName[b]);
        checkSegment(seg, true, what);
      }
    }
  }

  /* Binary round trip: Read() loads the NaN, and Validate() reports it. */
  {
    icFloatNumber params[4] = { 2.0f, std::numeric_limits<icFloatNumber>::quiet_NaN(), 0.0f, 0.0f };
    CIccFormulaCurveSegment out(icMinFloat32Number, icMaxFloat32Number);
    out.SetFunction(0, 4, params);

    CIccMemIO io;
    bool bIo = io.Alloc(1024, true) && out.Write(&io);
    size_t nWritten = (size_t)io.Tell();
    io.Seek(0, icSeekSet);

    CIccFormulaCurveSegment in(icMinFloat32Number, icMaxFloat32Number);
    bIo = bIo && in.Read(nWritten, &io);
    check(bIo, "binary: a NaN parameter writes and reads back");
    if (bIo)
      checkSegment(in, true, "binary: type 0 parameter 1 NaN after Read()");
  }

  if (g_failures) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
