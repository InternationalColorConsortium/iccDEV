// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: an MPE spectral CLUT element built by the XML parser must not end
// up declaring more input channels than its CLUT actually has, and the apply
// path must refuse it if it somehow does.
//
// This is the companion to spectral-clut-channel-count-bound.cpp, which pins the
// same invariant on the binary reader. The two exist separately because the
// bound the binary reader gained does not cover this route at all, and the first
// version of that fix assumed it did.
//
// CIccMpeSpectralCLUT::Read narrows a 16-bit m_nInputChannels to icUInt8Number
// when it constructs the CLUT, and now bounds the field at 16 first, so the
// element's count and the CLUT's dimensionality cannot diverge there. Read() is
// not the only constructor. CIccMpeXmlEmissionCLUT::ParseXml (IccMpeXml.cpp)
// sets m_nInputChannels from the InputChannels attribute via
// icXmlParseChannels, whose cap is kIccXmlMaxChannels == 0xFFFF, and then calls
//
//   icCLutFromXml(pNode, m_nInputChannels, m_Range.steps, ...)
//
// which performs its own independent narrowing -- "icUInt8Number nInput =
// (icUInt8Number)nIn;" in IccTagXml.cpp -- to build the CLUT. So InputChannels
// = "257" leaves a one-dimensional CLUT under an element that still reports 257
// through NumInputChannels(): exactly the state the binary reader used to
// produce. icCLutFromXml() does check Init()'s result, which is why the
// out-of-range counts (17, 255) are refused; the truncating ones are refused by
// nothing in the parser.
//
// The guard therefore belongs where every apply path converges regardless of
// what built the element. CIccMpeEmissionCLUT::Begin() and
// CIccMpeReflectanceCLUT::Begin() now compare m_pCLUT->GetInputDim() against
// m_nInputChannels and refuse a mismatch, CIccMpeCLUT::Begin() carries the same
// comparison, and CIccCLUT::Begin() returns bool so a CLUT that Init() never
// made usable can say so rather than leaving itself half-configured.
//
// The assertions drive the real libxml2 parser rather than simulating it,
// because the point in dispute is precisely whether the parser reaches a state
// the binary reader cannot.
//
// Red-green: removing the GetInputDim() != m_nInputChannels check from
// CIccMpeEmissionCLUT::Begin() makes the 257/259/272 "Begin() false" assertions
// fail while ParseXml still succeeds -- the whole defect in one line.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccMpeXml.h"
#include "IccTagMPE.h"
#include "IccTagLut.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

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
    std::fprintf(stderr, "[spectral-clut-xml] FAIL: %s\n", what);
  }
}

// Build an EmissionCLutElement document with an attacker-chosen InputChannels.
//
// The grid is given as the GridGranularity attribute rather than a GridPoints
// list, because icCLutFromXml() validates a GridPoints list against the
// *declared* count ("points.GetSize() < nInput") while GridGranularity reaches
// CIccCLUT::Init(icUInt8Number) against the *truncated* m_nInput. That
// asymmetry is itself part of the defect: nothing in the parser ties the count
// the element records to the CLUT it builds. Both GridGranularity and TableData
// belong to the element node itself -- icCLutFromXml() is handed the
// EmissionCLutElement node, and reads the attribute off it and TableData from
// its children.
//
// TableData is mandatory (icCLutFromXml() fails with "Cannot find table data"
// without it) and its length must equal NumPoints() * GetOutputChannels() of
// the CLUT actually built -- i.e. sized to the TRUNCATED dimensionality,
// granularity ^ (declared & 0xFF), times the spectral step count. Sizing it
// that way is what a hand-written malicious document would do, and it is why
// this fixture parses at all. When the truncated count is itself out of range
// (17, 255) Init() refuses before TableData is looked at, so a token table is
// emitted rather than an astronomically large one.
std::string buildDoc(unsigned nInputChannels, unsigned nGranularity,
                     unsigned nSteps)
{
  const unsigned nTruncatedDim = nInputChannels & 0xFFu;

  unsigned long long nTableEntries = 4;
  if (nTruncatedDim >= 1 && nTruncatedDim <= 16) {
    unsigned long long nPoints = 1;
    for (unsigned i = 0; i < nTruncatedDim; ++i)
      nPoints *= nGranularity;
    nTableEntries = nPoints * nSteps;
  }

  std::string s;
  s += "<EmissionCLutElement InputChannels=\"";
  s += std::to_string(nInputChannels);
  s += "\" OutputChannels=\"3\" StorageType=\"0\" Flags=\"0\" GridGranularity=\"";
  s += std::to_string(nGranularity);
  s += "\">\n";
  s += "  <Wavelengths start=\"400\" end=\"700\" steps=\"";
  s += std::to_string(nSteps);
  s += "\"/>\n";
  s += "  <WhiteData>\n    ";
  for (unsigned i = 0; i < nSteps; ++i)
    s += "1.0 ";
  s += "\n  </WhiteData>\n";
  s += "  <TableData>\n    ";
  for (unsigned long long i = 0; i < nTableEntries; ++i)
    s += "0.5 ";
  s += "\n  </TableData>\n";
  s += "</EmissionCLutElement>\n";
  return s;
}

struct ParseOutcome {
  bool bParsed;
  bool bBegan;
  unsigned nDeclared;
  unsigned nClutDim;
  bool bHaveClut;
};

// Parse the document into a real CIccMpeXmlEmissionCLUT, and optionally ask the
// element to Begin() the way the CMM would.
//
// Begin() is passed a NULL pMPE, which is deliberate and is why bCallBegin
// exists. CIccMpeEmissionCLUT::Begin() needs the enclosing tag only to reach an
// applied PCC for its observer, and it does so *after* the CLUT and
// channel-count checks this test is about. So for a mismatched element Begin()
// must refuse before pMPE is ever touched -- and if a future change reorders
// those checks, this test crashes rather than passing quietly, which is the
// signal we want. A well-formed element gets past them and would then
// dereference the NULL, so the control below parses only.
ParseOutcome parseAndBegin(const std::string &doc, bool bCallBegin)
{
  ParseOutcome r = { false, false, 0, 0, false };

  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
                               XML_PARSE_HUGE);
  if (!pDoc) {
    std::fprintf(stderr, "[spectral-clut-xml] test fixture is not valid XML\n");
    return r;
  }

  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  if (pRoot) {
    CIccMpeXmlEmissionCLUT elem;
    std::string parseStr;
    r.bParsed = elem.ParseXml(pRoot, parseStr);
    if (r.bParsed) {
      r.nDeclared = (unsigned)elem.NumInputChannels();
      CIccCLUT *pCLUT = elem.GetCLUT();
      r.bHaveClut = (pCLUT != NULL);
      if (pCLUT)
        r.nClutDim = (unsigned)pCLUT->GetInputDim();
      if (bCallBegin)
        r.bBegan = elem.Begin(icElemInterpLinear, NULL);
    }
    else if (!parseStr.empty()) {
      std::fprintf(stderr, "[spectral-clut-xml] parse refused: %s",
                   parseStr.c_str());
    }
  }

  xmlFreeDoc(pDoc);
  return r;
}

// Assert the contract for a declared count that narrows onto a dimensionality a
// CIccCLUT can hold.
//
// The contract is an either/or, and both halves are real answers: the parser may
// refuse the document, or it may accept it -- but if it accepts it, the count the
// element reports MUST equal the dimensionality of the CLUT it built. What must
// never happen is a parsed element carrying a disagreement, because from that
// point on nothing in the object records which of the two numbers is the truth.
//
// Written this way the assertion does not depend on where the fix lives. It held
// before icCLutFromXml() gained its bound only if Begin() refused; it holds after
// because the parser refuses. Either is acceptable; neither being true is not.
void checkTruncating(unsigned nDeclared, unsigned nGranularity, unsigned nSteps)
{
  ParseOutcome r = parseAndBegin(buildDoc(nDeclared, nGranularity, nSteps),
                                 /*bCallBegin*/false);

  char msg[224];
  std::snprintf(msg, sizeof(msg),
                "%u input channels: parser must refuse or stay consistent "
                "(parsed=%s declared=%u clutDim=%u)",
                nDeclared, r.bParsed ? "true" : "false",
                r.nDeclared, r.nClutDim);
  check(!r.bParsed || (r.bHaveClut && r.nDeclared == r.nClutDim), msg);
}

} // namespace

int main()
{
  // ---- The XML route to the mismatch -----------------------------------------
  //
  // 257 narrows to 1, 259 to 3, 272 to 16 -- the last landing exactly on the
  // supported maximum, so nothing about the resulting CLUT looks wrong in
  // isolation. A granularity of 2 keeps the 16-dimensional case at 65536 nodes.
  checkTruncating(/*declared*/257, /*granularity*/2, /*steps*/4);
  checkTruncating(/*declared*/259, /*granularity*/2, /*steps*/4);
  checkTruncating(/*declared*/272, /*granularity*/2, /*steps*/2);

  // ---- Out of range: icCLutFromXml() checks Init(), so these are refused -----
  //
  // 17 and 255 survive the cast unchanged, Init() rejects them, and
  // icCLutFromXml() returns NULL. Pinned so the parser's Init() check cannot be
  // dropped the way the library's own call sites had been.
  {
    ParseOutcome r = parseAndBegin(buildDoc(/*declared*/17, /*gran*/2, /*steps*/2), /*bCallBegin*/true);
    check(!r.bParsed || !r.bBegan, "17 input channels -> not usable");
  }
  {
    ParseOutcome r = parseAndBegin(buildDoc(/*declared*/255, /*gran*/2, /*steps*/2), /*bCallBegin*/true);
    check(!r.bParsed || !r.bBegan, "255 input channels -> not usable");
  }

  // ---- Control: an ordinary element must still parse and begin ---------------
  //
  // 3 input channels, a 2-point grid per dimension, 4 spectral steps. If this
  // regresses, the guards above are over-tight. Begin() is not asserted here:
  // the spectral overrides need an applied PCC from the enclosing MPE tag to
  // build their observer, which a bare element cannot supply, so the meaningful
  // control is that the element parses and its counts agree.
  {
    ParseOutcome r = parseAndBegin(buildDoc(/*declared*/3, /*gran*/2, /*steps*/4), /*bCallBegin*/false);
    check(r.bParsed, "valid 3-channel EmissionCLutElement -> ParseXml true");
    if (r.bParsed) {
      check(r.bHaveClut, "valid element has a CLUT");
      check(r.nDeclared == 3, "valid element declares 3 input channels");
      check(r.nClutDim == 3, "valid element's CLUT is 3-dimensional");
    }
  }

  if (g_fail) {
    std::fprintf(stderr, "[spectral-clut-xml] %d assertion(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[spectral-clut-xml] all assertions passed\n");
  return 0;
}
