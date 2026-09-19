// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       calculator-limits.cpp

    Contains:   CTest helper for the calculatorLimits ('clmt') block of a
                calculatorElement (ICC.2:2023 clause 11.2.1.1, Tables 85 and 85b).

    A calculatorElement may carry a 28-byte calculatorLimits block directly after
    its sub-element positions: 'clmt', a reserved word, the maximum data stack
    size, the maximum number of temporary channels, the maximum total number of
    operations (sub-elements included), and two reserved words.  A limit of 0 means
    no maximum.  The limits "shall be used by main function validity checking",
    and "shall also apply to any sub-calculator elements".

    CIccMpeCalculator::Read() follows the stored positions, so it stepped over the
    block: the element loaded, the limits were never checked, and Write() dropped
    the block.  XML and JSON had no way to carry it.  #2565.

    Case 1 is the binary layout: the block sits at 16 + 8 * (E + 1), is read back,
    is written only when present, and its reserved words round-trip.  Case 2 is
    Validate(): each limit at its exact boundary (the value passes, one less
    fails), the operation count including a sub-calculator, a sub-calculator's
    temporary channels checked against its parent's limit, and nonzero reserved
    words.  Case 3 is XML.  JSON is iccdev.calculator-limits-json, since
    IccMpeXml.h and IccMpeJson.h cannot share a translation unit.

    The fixture functions and the values the checks rely on:
      F1  { in(0,3) 1 1 1 add(3) out(0,3) }   stack peak 6, 6 operations, no temps
      F2  { in(0,3) tsav(4,3) out(0,3) }      temps 4-6 = 7 channels, 3 operations
      P   { in(0,3) calc(0) out(0,3) }        sub-element 0 = F2; 3 + 3 = 6 operations
      F3  { in(0,3) in(0) if { 1 1 1 1 add(3) pop } out(0,3) }
                                              stack peak 7, inside the if block, so the
                                              limit has to reach the nested check
*/

#include "IccProfile.h"
#include "IccMpeCalc.h"
#include "IccMpeXml.h"
#include "IccIO.h"
#include "IccUtil.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[calculator-limits] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

icUInt32Number be32(const icUInt8Number *p)
{
  return ((icUInt32Number)p[0]<<24) | ((icUInt32Number)p[1]<<16) | ((icUInt32Number)p[2]<<8) | p[3];
}

const char *kF1 = "{ in(0,3) 1 1 1 add(3) out(0,3) }";
const char *kF2 = "{ in(0,3) tsav(4,3) out(0,3) }";
const char *kP  = "{ in(0,3) calc(0) out(0,3) }";
const char *kF3 = "{ in(0,3) in(0) if { 1 1 1 1 add(3) pop } out(0,3) }";

CIccMpeCalculator *makeCalc(const char *szFunc)
{
  CIccMpeCalculator *pCalc = new CIccMpeCalculator(3, 3);
  std::string sReport;
  if (pCalc->SetCalcFunc(szFunc, sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "[calculator-limits] cannot build %s: %s\n", szFunc, sReport.c_str());
    delete pCalc;
    return NULL;
  }
  return pCalc;
}

CIccMpeCalculator *makeParent()
{
  CIccMpeCalculator *pParent = new CIccMpeCalculator(3, 3);
  CIccMpeCalculator *pSub = makeCalc(kF2);
  if (!pSub) {
    delete pParent;
    return NULL;
  }
  pParent->SetSubElem(0, pSub);
  std::string sReport;
  if (pParent->SetCalcFunc(kP, sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "[calculator-limits] cannot build parent: %s\n", sReport.c_str());
    delete pParent;
    return NULL;
  }
  return pParent;
}

void setLimits(CIccMpeCalculator &calc, icUInt32Number stack, icUInt32Number temp, icUInt32Number ops)
{
  calc.m_bHasLimits = true;
  calc.m_nMaxStackSize = stack;
  calc.m_nMaxTempChannels = temp;
  calc.m_nMaxOperations = ops;
}

// The report lines about calculatorLimits, and nothing else.
std::string limitLines(const CIccMpeCalculator &calc)
{
  std::string report, lines;
  calc.Validate("", report);
  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (has(line, "calculatorLimits"))
      lines += line + "\n";
    pos = end + 1;
  }
  return lines;
}

std::vector<icUInt8Number> writeElem(CIccMpeCalculator &calc)
{
  CIccMemIO io;
  std::vector<icUInt8Number> out;
  if (!io.Alloc(1 << 16, true) || !calc.Write(&io))
    return out;
  out.assign(io.GetData(), io.GetData() + (size_t)io.Tell());
  return out;
}

bool readElem(std::vector<icUInt8Number> &bytes, CIccMpeCalculator &calc)
{
  CIccMemIO io;
  io.Attach(bytes.data(), bytes.size());
  return calc.Read((icUInt32Number)bytes.size(), &io);
}

void caseBinary()
{
  CIccMpeCalculator *pPlain = makeCalc(kF1);
  CIccMpeCalculator *pLimited = makeCalc(kF1);
  if (!pPlain || !pLimited) {
    check(false, "binary", "fixture did not build");
    delete pPlain;
    delete pLimited;
    return;
  }
  setLimits(*pLimited, 6, 0, 6);

  std::vector<icUInt8Number> plain = writeElem(*pPlain);
  std::vector<icUInt8Number> limited = writeElem(*pLimited);
  check(!plain.empty() && limited.size() == plain.size() + icCalcLimitsSize, "binary write",
        "the block must add exactly 28 bytes, and only when present");

  size_t at = 16 + 8 * (0 + 1);  // no sub-elements: one position, the main function
  for (size_t i = 0; i + 4 <= plain.size(); i += 4)
    check(be32(&plain[i]) != icSigCalcLimits, "binary write plain", "'clmt' written without limits");
  if (limited.size() >= at + icCalcLimitsSize) {
    check(be32(&limited[at]) == icSigCalcLimits, "binary write", "'clmt' is not right after the positions");
    check(be32(&limited[at+8]) == 6 && be32(&limited[at+12]) == 0 && be32(&limited[at+16]) == 6,
          "binary write", "limit values are not at Table 85b's offsets");
    check(be32(&limited[16]) == at + icCalcLimitsSize, "binary write", "the main function does not start after the block");
  }

  CIccMpeCalculator back;
  check(readElem(limited, back) && back.m_bHasLimits && back.m_nMaxStackSize == 6 &&
          back.m_nMaxTempChannels == 0 && back.m_nMaxOperations == 6,
        "binary read", "limits not read back");
  std::vector<icUInt8Number> again = writeElem(back);
  check(again == limited, "binary round trip", "a read and write changed the element");

  CIccMpeCalculator backPlain;
  backPlain.m_bHasLimits = true;
  check(readElem(plain, backPlain) && !backPlain.m_bHasLimits, "binary read plain", "limits reported for an element without them");

  // Reserved words are kept, and Validate() reports them.
  std::vector<icUInt8Number> reserved = limited;
  if (reserved.size() >= at + icCalcLimitsSize) {
    reserved[at+7] = 1;
    reserved[at+27] = 2;
    CIccMpeCalculator r;
    check(readElem(reserved, r) && r.m_nLimitsReserved[0] == 1 && r.m_nLimitsReserved[2] == 2,
          "binary reserved", "reserved words not kept");
    check(writeElem(r) == reserved, "binary reserved", "reserved words not written back");
    check(has(limitLines(r), "calculatorLimits reserved fields must be zero"), "validate reserved", "not reported");
  }

  // A block cut short by the element size is refused, not read past.
  std::vector<icUInt8Number> truncated(limited.begin(), limited.begin() + at + 12);
  CIccMpeCalculator t;
  check(!readElem(truncated, t), "binary truncated", "a truncated block was accepted");

  delete pPlain;
  delete pLimited;
}

void expectLimit(CIccMpeCalculator &calc, const char *label, icUInt32Number stack, icUInt32Number temp,
                 icUInt32Number ops, const char *want)
{
  setLimits(calc, stack, temp, ops);
  std::string lines = limitLines(calc);
  if (want)
    check(has(lines, want), label, ("expected \"" + std::string(want) + "\", got: " + lines).c_str());
  else
    check(lines.empty(), label, ("expected no finding, got: " + lines).c_str());
}

void caseValidate()
{
  CIccMpeCalculator *pF1 = makeCalc(kF1);
  CIccMpeCalculator *pF2 = makeCalc(kF2);
  CIccMpeCalculator *pP = makeParent();
  if (!pF1 || !pF2 || !pP) {
    check(false, "validate", "fixture did not build");
    delete pF1; delete pF2; delete pP;
    return;
  }

  expectLimit(*pF1, "all zero", 0, 0, 0, NULL);
  expectLimit(*pF1, "stack 6", 6, 0, 0, NULL);
  expectLimit(*pF1, "stack 5", 5, 0, 0, "data stack larger than the calculatorLimits maximum of 5");
  expectLimit(*pF1, "ops 6", 0, 0, 6, NULL);
  expectLimit(*pF1, "ops 5", 0, 0, 5, "Has 6 operations; calculatorLimits allows 5");
  expectLimit(*pF1, "temp 1, none used", 0, 1, 0, NULL);
  CIccMpeCalculator *pF3 = makeCalc(kF3);
  check(pF3 != NULL, "validate", "F3 did not build");
  if (pF3) {
    expectLimit(*pF3, "nested stack 7", 7, 0, 0, NULL);
    expectLimit(*pF3, "nested stack 6", 6, 0, 0, "data stack larger than the calculatorLimits maximum of 6");
    delete pF3;
  }
  expectLimit(*pF2, "temp 7", 0, 7, 0, NULL);
  expectLimit(*pF2, "temp 6", 0, 6, 0, "uses 7 temporary channels; calculatorLimits allows 6");

  // Parent limits reach the sub-calculator, and its operations are counted.
  expectLimit(*pP, "parent temp 7", 0, 7, 0, NULL);
  expectLimit(*pP, "parent temp 6", 0, 6, 0, "uses 7 temporary channels; calculatorLimits allows 6");
  expectLimit(*pP, "parent ops 6", 0, 0, 6, NULL);
  expectLimit(*pP, "parent ops 5", 0, 0, 5, "Has 6 operations; calculatorLimits allows 5");

  // Without the block nothing is checked.
  pF1->m_bHasLimits = false;
  pF1->m_nMaxStackSize = 1;
  check(limitLines(*pF1).empty(), "no block", "limits applied without a block");

  delete pF1;
  delete pF2;
  delete pP;
}

bool parseXml(const std::string &doc, CIccMpeXmlCalculator &calc, std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "calc.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return false;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool ok = pRoot && calc.ParseXml(pRoot, parseStr);
  xmlFreeDoc(pDoc);
  return ok;
}

std::string calcDoc(const char *limits)
{
  return std::string("<CalculatorElement InputChannels=\"3\" OutputChannels=\"3\">") + limits +
         "<MainFunction>{ in(0,3) 1 1 1 add(3) out(0,3) }</MainFunction></CalculatorElement>";
}

void caseXml()
{
  std::string parseStr;
  CIccMpeXmlCalculator plain;
  check(parseXml(calcDoc(""), plain, parseStr) && !plain.m_bHasLimits, "xml absent", "limits set without the element");
  std::string xml;
  plain.ToXml(xml, "");
  check(!has(xml, "CalculatorLimits"), "xml absent", "written without limits");

  CIccMpeXmlCalculator limited;
  check(parseXml(calcDoc("<CalculatorLimits MaxStackSize=\"6\" MaxTempChannels=\"0\" MaxOperations=\"6\"/>"),
                 limited, parseStr) &&
          limited.m_bHasLimits && limited.m_nMaxStackSize == 6 && limited.m_nMaxOperations == 6,
        "xml read", ("limits not read: " + parseStr).c_str());
  xml.clear();
  limited.ToXml(xml, "");
  check(has(xml, "<CalculatorLimits MaxStackSize=\"6\" MaxTempChannels=\"0\" MaxOperations=\"6\"/>"),
        "xml write", "not written");

  CIccMpeXmlCalculator partial;
  check(parseXml(calcDoc("<CalculatorLimits MaxTempChannels=\"9\"/>"), partial, parseStr) &&
          partial.m_bHasLimits && partial.m_nMaxTempChannels == 9 && !partial.m_nMaxStackSize && !partial.m_nMaxOperations,
        "xml partial", "a missing attribute must read as 0");

  CIccMpeXmlCalculator bad;
  parseStr.clear();
  check(!parseXml(calcDoc("<CalculatorLimits MaxStackSize=\"6x\"/>"), bad, parseStr), "xml bad", "trailing text accepted");
  CIccMpeXmlCalculator neg;
  check(!parseXml(calcDoc("<CalculatorLimits MaxOperations=\"-1\"/>"), neg, parseStr), "xml negative", "a negative limit accepted");
}

} // namespace

int main()
{
  caseBinary();
  caseValidate();
  caseXml();

  if (g_fail) {
    std::fprintf(stderr, "[calculator-limits] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[calculator-limits] all checks passed\n");
  return 0;
}
