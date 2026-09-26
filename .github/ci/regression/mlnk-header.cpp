// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       mlnk-header.cpp

    Contains:   CTest helper for the MultiplexLink header contract (ICC.2:2023
                clauses 7.2.8 and 7.2.9; identical in ICC.2:2019).

    7.2.8: "For MultiplexLink and MultiplexVisualization profiles the data colour
    space signature shall be zero."  7.2.9: for DeviceLink or MultiplexLink "the
    value of the PCS shall be the appropriate data colour space", i.e. the B-side
    device space the link produces.

    CIccProfile::CheckHeader() had MultiplexVisualization exempt from the
    known-data-space test and MultiplexLink not, then required a zero PCS for
    MultiplexIdentification and MultiplexLink together.  A conforming MLNK profile
    therefore failed twice: a critical "Unknown colour space!" for the zero data
    space the text mandates, and a NonCompliant "Invalid PCS designator" for the
    device space the text tells it to put there.  A third report came from the tag
    validator, which took MToA0's output channel count from the data colour space
    -- zero for an MLNK -- rather than from the PCS field.  #2563.

    Testing/mcs carries MID and MVIS fixtures and no MLNK, so none of this was
    reachable from the corpus.

    The cases below pin the fix in both directions.  A conforming MLNK is clean;
    an MLNK that breaks either clause is still reported; and the MID and MVIS
    controls keep the verdicts they had, so a fix that merely deleted the checks
    would fail here.
*/

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagMPE.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
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
    std::fprintf(stderr, "[mlnk-header] FAIL %s: %s\n", label, what);
  }
}

// "mc" + a channel count, the multiplex (MCS) channel-space form of 7.2.10.
icMultiplexColorSignature mcsSig(icUInt16Number nChannels)
{
  return (icMultiplexColorSignature)(icSigSrcMCSChannelData + nChannels);
}

// A minimal multiplex profile.  Only the header fields under test and the one
// transform tag are set; the profile is deliberately missing desc/cprt and the
// rest, so the report carries their findings too and the helpers below match on
// the specific wording rather than on the overall verdict.
//
// nMToA0Out is the output channel count declared by the MToA0 tag, which the tag
// validator compares against the count implied by the header.  Passing 0 leaves
// the tag off entirely.
std::string headerReport(icProfileClassSignature deviceClass,
                         icColorSpaceSignature colorSpace,
                         icColorSpaceSignature pcs,
                         icUInt16Number nMcsChannels,
                         icUInt16Number nMToA0Out)
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = icVersionNumberV5;
  prof.m_Header.deviceClass = deviceClass;
  prof.m_Header.colorSpace = colorSpace;
  prof.m_Header.pcs = pcs;
  prof.m_Header.mcs = mcsSig(nMcsChannels);

  if (nMToA0Out) {
    CIccTagMultiProcessElement *pTag =
      new CIccTagMultiProcessElement(nMcsChannels, nMToA0Out);
    prof.AttachTag(icSigMToA0Tag, pTag);
  }

  std::string report;
  prof.Validate(report);
  return report;
}

bool has(const std::string &report, const char *needle)
{
  return report.find(needle) != std::string::npos;
}

const icColorSpaceSignature kNoSpace = icSigNoColorData;
const icColorSpaceSignature kCmyk    = icSigCmykData;

} // namespace

int main()
{
  // ---- 7.2.8, the data colour space -------------------------------------
  {
    // A conforming MLNK: data colour space zero, device space in the PCS.
    std::string r = headerReport(icSigMultiplexLinkClass, kNoSpace, kCmyk, 4, 4);
    check(!has(r, "Unknown colour space"), "mlnk zero data space",
          "a zero data colour space is what 7.2.8 requires of an MLNK");
    check(!has(r, "Invalid PCS designator"), "mlnk device pcs",
          "7.2.9 puts the device output space in the PCS of an MLNK");
    check(!has(r, "MToA0Tag") || !has(r, "Incorrect number of output channels"),
          "mlnk MToA0 output count",
          "MToA0's output count comes from the PCS for an MLNK, not the data space");

    // MVIS was already exempt; it must stay exempt.
    std::string v = headerReport(icSigMultiplexVisualizationClass, kNoSpace,
                                 icSigXYZData, 4, 0);
    check(!has(v, "Unknown colour space"), "mvis zero data space",
          "7.2.8 names MultiplexVisualization alongside MultiplexLink");

    // MID is not named by 7.2.8 and keeps a real data colour space; a profile
    // with a space that is not a known one is still reported.  This is the
    // control that the exemption was widened by one class and not emptied.
    std::string u = headerReport(icSigMultiplexIdentificationClass,
                                 (icColorSpaceSignature)0x7A7A7A7A, kNoSpace, 4, 0);
    check(has(u, "Unknown colour space"), "mid unknown data space",
          "an unrecognised data colour space must still be critical");
  }

  // ---- 7.2.9, the PCS field ---------------------------------------------
  {
    // An MLNK whose PCS is zero has no output space: still NonCompliant, now
    // through the DeviceLink test rather than the MID one.
    std::string r = headerReport(icSigMultiplexLinkClass, kNoSpace, kNoSpace, 4, 0);
    check(has(r, "Unknown pcs colour space"), "mlnk zero pcs",
          "an MLNK must name its output space in the PCS");

    // ... and one whose PCS is not a colour space at all.
    std::string b = headerReport(icSigMultiplexLinkClass, kNoSpace,
                                 (icColorSpaceSignature)0x7A7A7A7A, 4, 0);
    check(has(b, "Unknown pcs colour space"), "mlnk bogus pcs",
          "an unrecognised PCS must be critical for an MLNK");

    // MID keeps the opposite rule: its PCS shall be zero.
    std::string m = headerReport(icSigMultiplexIdentificationClass,
                                 icNColorSpaceSig(icSigNChannelData, 4), kCmyk, 4, 0);
    check(has(m, "Invalid PCS designator"), "mid non-zero pcs",
          "a MultiplexIdentification profile still requires a zero PCS");

    std::string mz = headerReport(icSigMultiplexIdentificationClass,
                                  icNColorSpaceSig(icSigNChannelData, 4), kNoSpace, 4, 0);
    check(!has(mz, "Invalid PCS designator"), "mid zero pcs",
          "a conforming MID profile draws no PCS finding");
  }

  // ---- the MToA0 channel count ------------------------------------------
  {
    // CMYK in the PCS means four output channels.  A tag that declares three
    // must still be reported, or the fix would have removed the check rather
    // than moved its source field.
    std::string r = headerReport(icSigMultiplexLinkClass, kNoSpace, kCmyk, 4, 3);
    check(has(r, "Incorrect number of output channels"), "mlnk MToA0 wrong count",
          "a wrong output count must stay critical for an MLNK");

    // And the count really does follow the PCS: with a six-channel PCS the
    // four-channel tag above becomes wrong and a six-channel one becomes right.
    icColorSpaceSignature six = icNColorSpaceSig(icSigNChannelData, 6);
    std::string four = headerReport(icSigMultiplexLinkClass, kNoSpace, six, 4, 4);
    check(has(four, "Incorrect number of output channels"), "mlnk MToA0 follows pcs",
          "a four-channel MToA0 is wrong against a six-channel PCS");

    std::string sixTag = headerReport(icSigMultiplexLinkClass, kNoSpace, six, 4, 6);
    check(!has(sixTag, "Incorrect number of output channels"), "mlnk MToA0 six",
          "a six-channel MToA0 matches a six-channel PCS");

    // The control for the class test itself: every class other than MLNK names
    // its device space in the data colour space field, so an MToA0 on one of them
    // must still be counted against that field.  A CMYK display profile with a
    // three-channel PCS and a four-channel MToA0 is correct today; it is only
    // wrong if the PCS became the source for every class instead of for MLNK.
    std::string other = headerReport(icSigDisplayClass, kCmyk, icSigXYZData, 4, 4);
    check(!has(other, "Incorrect number of output channels"), "non-mlnk MToA0 source",
          "outside an MLNK the output count still comes from the data colour space");
  }

  if (g_fail) {
    std::fprintf(stderr, "[mlnk-header] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[mlnk-header] all checks passed\n");
  return 0;
}
