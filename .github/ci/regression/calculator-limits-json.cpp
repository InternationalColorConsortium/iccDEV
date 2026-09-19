// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       calculator-limits-json.cpp

    Contains:   CTest helper for the JSON form of a calculatorElement's
                calculatorLimits (ICC.2:2023 Table 85b).  #2565.

    The JSON half of iccdev.calculator-limits, in its own executable because
    IccMpeXml.h and IccMpeJson.h cannot share a translation unit.  The object
    "calculatorLimits" is written only when the element has limits, a missing key
    reads as 0 (no maximum), and a value that is not an unsigned integer, or a
    "calculatorLimits" that is not an object, is refused.
*/

#include "IccProfile.h"
#include "IccMpeCalc.h"
#include "IccMpeJson.h"
#include "IccUtil.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[calculator-limits-json] FAIL %s: %s\n", label, what);
  }
}

bool parse(const char *text, CIccMpeJsonCalculator &calc)
{
  std::string parseStr;
  IccJson j = IccJson::parse(text);
  j["inputChannels"] = 3;
  j["outputChannels"] = 3;
  j["mainFunction"] = "{ in(0,3) out(0,3) }";
  return calc.ParseJson(j, parseStr);
}

} // namespace

int main()
{
  CIccMpeJsonCalculator plain;
  check(parse("{}", plain) && !plain.m_bHasLimits, "absent", "limits set without the key");
  IccJson out;
  plain.ToJson(out);
  check(!out.contains("calculatorLimits"), "absent", "written without limits");

  CIccMpeJsonCalculator limited;
  check(parse("{\"calculatorLimits\": {\"maxStackSize\": 6, \"maxTempChannels\": 7, \"maxOperations\": 8}}", limited) &&
          limited.m_bHasLimits && limited.m_nMaxStackSize == 6 && limited.m_nMaxTempChannels == 7 &&
          limited.m_nMaxOperations == 8,
        "read", "limits not read");
  IccJson j;
  limited.ToJson(j);
  check(j.contains("calculatorLimits") && j["calculatorLimits"]["maxStackSize"] == 6 &&
          j["calculatorLimits"]["maxTempChannels"] == 7 && j["calculatorLimits"]["maxOperations"] == 8,
        "write", "limits not written");

  CIccMpeJsonCalculator back;
  std::string parseStr;
  check(back.ParseJson(j, parseStr) && back.m_bHasLimits && back.m_nMaxOperations == 8, "round trip", "does not read back");

  CIccMpeJsonCalculator partial;
  check(parse("{\"calculatorLimits\": {\"maxOperations\": 8}}", partial) && partial.m_bHasLimits &&
          !partial.m_nMaxStackSize && !partial.m_nMaxTempChannels && partial.m_nMaxOperations == 8,
        "partial", "a missing key must read as 0");

  const char *bad[] = {
    "{\"calculatorLimits\": {\"maxStackSize\": -1}}",
    "{\"calculatorLimits\": {\"maxStackSize\": 1.5}}",
    "{\"calculatorLimits\": {\"maxTempChannels\": \"7\"}}",
    "{\"calculatorLimits\": [6, 7, 8]}",
  };
  for (const char *text : bad) {
    CIccMpeJsonCalculator c;
    check(!parse(text, c), text, "malformed calculatorLimits accepted");
  }

  if (g_fail) {
    std::fprintf(stderr, "[calculator-limits-json] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[calculator-limits-json] all checks passed\n");
  return 0;
}
