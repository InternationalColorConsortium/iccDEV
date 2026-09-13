/*
    File:       parametric-curve-params-json.cpp

    Contains:   CTest helper for #2538, the uninitialised parameters that
                CIccTagJsonParametricCurve::ParseJson() used to accept.

    SetFunctionType() allocates m_dParam for the function type's parameter
    count with a bare new[] and fills nothing.  The reader then wrote only
    min(supplied, required) entries -- none at all when "params" was missing or
    not an array -- and still returned true, so Write() encoded heap contents.
    Measured on master fb5f65d0 with iccFromJson then iccToJson on an ASan
    build, where the unwritten slots carry ASan's 0xbe malloc fill:

      functionType 4, params [2.4]      -> 2.3999939, then six -0.3725433
      functionType 4, no params         -> seven -0.3725433
      functionType 4, params "x"        -> seven -0.3725433
      functionType 65540, seven params  -> seven -0.3725433, function type 4
      functionType -65532, seven params -> seven -0.3725433, function type 4
      functionType 4, eight params      -> the first seven, silently truncated
      functionType 0, params []         -> accepted, its one parameter never written

    Rows four and five are not short arrays.  functionType was read as an int
    and the parameter count came from a switch on that int, while
    SetFunctionType() received it narrowed to 16 bits: 65540 found no case (count
    0, nothing read) yet allocated seven slots as type 4.

    The XML twin refuses every one of these ("data.GetSize() != GetNumParam()",
    plus a bounded function-type parse from #1851); measured on the same build,
    iccFromXml writes no profile for the short, empty, long and 65540 variants
    of Testing's #1851 control.  This helper holds the JSON reader to that
    contract.

    The reader is driven directly rather than through a profile document, as
    in colorant-count-narrowing-json.cpp: ParseJson() is the function under fix,
    and a profile carrying one tag draws unrelated validator failures.

    The discriminators are the return value and, for accepted documents, every
    parameter's value.  Neither depends on what the heap held, so the unfixed
    reader fails this helper on a plain build, not just under a sanitizer --
    measured against master fb5f65d0, exactly the seven cases marked below
    fail, each observing true.  The accepted
    cases compare all parameters of every known function type, so a count table
    that is off by one for any type fails its own case even though the refusal
    cases would pass.

    The unknown-function-type case pins behaviour this fix deliberately did NOT
    change: type 5 has no defined parameters, nothing is left uninitialised, and
    the binary reader keeps unknown types too, so it is still accepted.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccTagJson.h"

#include <cstdio>
#include <string>

static const char *kName = "parametric-curve-params-json";

// A parametricCurveType "data" object.  pParams == NULL omits "params".
static IccJson makeCurve(long long functionType, const double *pParams,
                         size_t nParams)
{
  IccJson j = IccJson::object();
  j["type"] = "parametricCurveType";
  j["functionType"] = functionType;
  if (pParams) {
    IccJson arr = IccJson::array();
    for (size_t i = 0; i < nParams; i++)
      arr.push_back(pParams[i]);
    j["params"] = arr;
  }
  return j;
}

// Parse j, and require it to be refused.
static int refusedCase(const IccJson &j, const char *label)
{
  CIccTagJsonParametricCurve tag;
  std::string parseStr;

  if (tag.ParseJson(j, parseStr)) {
    std::fprintf(stderr,
                 "%s: FAIL  %s (ParseJson returned true, function type %u, "
                 "%u parameters)\n",
                 kName, label, (unsigned)tag.GetFunctionType(),
                 (unsigned)tag.GetNumParam());
    return 1;
  }

  std::fprintf(stdout, "%s: PASS  %s\n", kName, label);
  return 0;
}

// Parse j, and require it to be accepted as nFunctionType with exactly the
// nExpect parameters in pExpect.  Values are chosen to be exact in a float.
static int acceptedCase(const IccJson &j, icUInt16Number nFunctionType,
                        const double *pExpect, icUInt16Number nExpect,
                        const char *label)
{
  CIccTagJsonParametricCurve tag;
  std::string parseStr;

  if (!tag.ParseJson(j, parseStr)) {
    std::fprintf(stderr, "%s: FAIL  %s (ParseJson returned false: %s)\n",
                 kName, label, parseStr.c_str());
    return 1;
  }

  if (tag.GetFunctionType() != nFunctionType || tag.GetNumParam() != nExpect) {
    std::fprintf(stderr,
                 "%s: FAIL  %s (function type %u with %u parameters, expected "
                 "%u with %u)\n",
                 kName, label, (unsigned)tag.GetFunctionType(),
                 (unsigned)tag.GetNumParam(), (unsigned)nFunctionType,
                 (unsigned)nExpect);
    return 1;
  }

  const icFloatNumber *pParams = tag.GetParams();
  for (icUInt16Number i = 0; i < nExpect; i++) {
    if (!pParams || pParams[i] != (icFloatNumber)pExpect[i]) {
      std::fprintf(stderr, "%s: FAIL  %s (parameter %u is %g, expected %g)\n",
                   kName, label, (unsigned)i,
                   pParams ? (double)pParams[i] : 0.0, pExpect[i]);
      return 1;
    }
  }

  std::fprintf(stdout, "%s: PASS  %s\n", kName, label);
  return 0;
}

int main()
{
  int failures = 0;

  static const double kSeven[] = { 2.5, 0.75, 0.25, 0.125, 0.0625, 0.5, 0.375 };
  static const double kEight[] = { 2.5, 0.75, 0.25, 0.125, 0.0625, 0.5, 0.375, 123.0 };
  static const double kOne[]   = { 2.5 };

  // Refused: each of these seven returned true against master.
  failures += refusedCase(makeCurve(4, kOne, 1),
                          "type 4 with one of its seven parameters is refused");
  failures += refusedCase(makeCurve(4, NULL, 0),
                          "type 4 with no params member is refused");
  {
    IccJson j = makeCurve(4, NULL, 0);
    j["params"] = "not-an-array";
    failures += refusedCase(j, "type 4 with a non-array params member is refused");
  }
  failures += refusedCase(makeCurve(65540, kSeven, 7),
                          "functionType 65540, which narrowed to 4, is refused");
  failures += refusedCase(makeCurve(-65532, kSeven, 7),
                          "functionType -65532, which narrowed to 4, is refused");
  failures += refusedCase(makeCurve(4, kEight, 8),
                          "type 4 with eight parameters is refused, not truncated");
  failures += refusedCase(makeCurve(0, kOne, 0),
                          "type 0 with an empty params array is refused");

  // Refused before and after: functionType itself is required.
  {
    IccJson j = makeCurve(4, kSeven, 7);
    j.erase("functionType");
    failures += refusedCase(j, "a curve with no functionType is refused");
  }

  // Accepted: every known type with exactly its count, compared value by value.
  static const icUInt16Number kCounts[] = { 1, 3, 4, 5, 7 };
  static const char *kLabels[] = {
    "type 0 with exactly 1 parameter is accepted intact",
    "type 1 with exactly 3 parameters is accepted intact",
    "type 2 with exactly 4 parameters is accepted intact",
    "type 3 with exactly 5 parameters is accepted intact",
    "type 4 with exactly 7 parameters is accepted intact",
  };
  for (icUInt16Number t = 0; t < 5; t++)
    failures += acceptedCase(makeCurve(t, kSeven, kCounts[t]), t, kSeven,
                             kCounts[t], kLabels[t]);

  // Unchanged: an unknown type has no parameters to leave uninitialised.
  failures += acceptedCase(makeCurve(5, kSeven, 0), 5, NULL, 0,
                           "unknown type 5 with no parameters is still accepted");

  if (failures) {
    std::fprintf(stderr, "%s: %d case(s) failed\n", kName, failures);
    return 1;
  }

  std::fprintf(stdout, "%s: all cases passed\n", kName);
  return 0;
}
