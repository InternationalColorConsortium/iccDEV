/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// CTest contract for the MCS subset check in CIccPcsXform::Connect() (#2871,
// #2878).
//
// When a multiplex profile sets the MCSNeedsSubset header flag, every MCS
// channel it names must appear among the channels of the profile it connects
// to, in any order (ICC.2:2023 header flags bit 2, 6.4.1, the
// multiplexTypeArrayTag NOTE and Annex I.2). Connect() searched the other
// profile's channel list with the outer loop's index instead of the inner
// one, so it:
//   - compared names position by position, refusing legal chains whose
//     channel order differs; and
//   - indexed past the end of the other list when the flagged profile had
//     more channels, dereferencing the NULL CIccTagArray::GetIndex() returns
//     (Debug, ASan) or reading past the array (Release).
//
// The first group of cases is legal and must begin: an unfixed library refuses
// them with icCmmStatBadMCSLink, so they fail in any build without a crash. The
// control must still be refused. The two issue shapes -- a flagged profile
// with a channel ("Spot White") the other lacks -- come last, because they
// crash an unfixed library; fixed, they are refused.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccUtil.h"

#include <cstdio>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool ok, const char *label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label);
  if (!ok)
    ++g_failures;
}

typedef std::vector<const char *> Names;

CIccTagArray *makeChannelNames(const Names &names)
{
  CIccTagArray *pArray = new CIccTagArray(icSigUtf8TextTypeArray);
  pArray->SetSize((icUInt32Number)names.size());
  for (icUInt32Number i = 0; i < names.size(); i++) {
    CIccTagUtf8Text *pText = new CIccTagUtf8Text();
    pText->SetText(names[i]);
    pArray->AttachTag(i, pText);
  }
  return pArray;
}

// An identity-like matrix element: output r takes input r, or 0.
CIccTagMultiProcessElement *makeMatrix(icUInt16Number nIn, icUInt16Number nOut)
{
  CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement(nIn, nOut);
  CIccMpeMatrix *pMtx = new CIccMpeMatrix;
  pMtx->SetSize(nIn, nOut, true);
  icFloatNumber *m = pMtx->GetMatrix();
  for (int r = 0; r < nOut; r++) {
    for (int c = 0; c < nIn; c++)
      m[r * nIn + c] = (r == c) ? 1.0f : 0.0f;
    pMtx->GetConstants()[r] = 0.0f;
  }
  pTag->Attach(pMtx);
  return pTag;
}

void setMcs(CIccProfile *p, icProfileClassSignature cls, bool bSubset, const Names &names)
{
  p->m_Header.version = icVersionNumberV5;
  p->m_Header.deviceClass = cls;
  p->m_Header.mcs = (icMultiplexColorSignature)icNColorSpaceSig(icSigSrcMCSChannelData,
                                                               (icUInt16Number)names.size());
  p->m_Header.flags = bSubset ? icMCSNeedsSubsetTrue : 0;
  p->AttachTag(icSigMultiplexTypeArrayTag, makeChannelNames(names));
}

// MultiplexIdentification: CMYK data -> MCS through AToM0.
CIccProfile *makeMid(bool bSubset, const Names &names)
{
  CIccProfile *p = new CIccProfile;
  setMcs(p, icSigMultiplexIdentificationClass, bSubset, names);
  p->m_Header.colorSpace = icSigCmykData;
  p->AttachTag(icSigAToM0Tag, makeMatrix(4, (icUInt16Number)names.size()));
  return p;
}

// MultiplexVisualization: MCS -> XYZ through MToB0.
CIccProfile *makeMvis(bool bSubset, const Names &names)
{
  CIccProfile *p = new CIccProfile;
  setMcs(p, icSigMultiplexVisualizationClass, bSubset, names);
  p->m_Header.pcs = icSigXYZData;
  p->AttachTag(icSigMToB0Tag, makeMatrix((icUInt16Number)names.size(), 3));
  return p;
}

icStatusCMM link(CIccProfile *pMid, CIccProfile *pMvis)
{
  CIccCmm cmm(icSigCmykData, icSigXYZData, true);
  icStatusCMM stat = cmm.AddXform(pMid);
  if (stat != icCmmStatOk)
    return stat;
  stat = cmm.AddXform(pMvis);
  if (stat != icCmmStatOk)
    return stat;
  return cmm.Begin();
}

} // namespace

int main()
{
  const Names CMYK  = { "Process Cyan", "Process Magenta", "Process Yellow", "Process Black" };
  const Names KYMC  = { "Process Black", "Process Yellow", "Process Magenta", "Process Cyan" };
  const Names CMYKW = { "Process Cyan", "Process Magenta", "Process Yellow", "Process Black",
                        "Spot White" };
  const Names KYMCW = { "Process Black", "Process Yellow", "Process Magenta", "Process Cyan",
                        "Spot White" };
  const Names CMYO  = { "Process Cyan", "Process Magenta", "Process Yellow", "Process Orange" };

  // Legal: channel order need not match.
  check(link(makeMid(true, CMYK), makeMvis(false, KYMCW)) == icCmmStatOk,
        "flagged MID whose channels the MVIS has in another order: begins");
  check(link(makeMid(false, CMYK), makeMvis(true, KYMC)) == icCmmStatOk,
        "flagged MVIS whose channels the MID has in another order: begins");
  check(link(makeMid(true, CMYK), makeMvis(true, KYMC)) == icCmmStatOk,
        "both flagged, same channels in another order: begins");
  check(link(makeMid(true, CMYKW), makeMvis(true, KYMCW)) == icCmmStatOk,
        "both flagged, same five channels in another order: begins");
  check(link(makeMid(false, CMYK), makeMvis(false, CMYKW)) == icCmmStatOk,
        "neither flagged: begins");

  // Control: a flagged channel the other profile lacks is still refused.
  check(link(makeMid(true, CMYO), makeMvis(false, CMYKW)) == icCmmStatBadMCSLink,
        "flagged MID with a channel the MVIS lacks: refused");

  // The issue shapes: the flagged profile is the longer list.
  check(link(makeMid(true, CMYKW), makeMvis(false, CMYK)) == icCmmStatBadMCSLink,
        "#2878: flagged five-channel MID, four-channel MVIS: refused");
  check(link(makeMid(false, CMYK), makeMvis(true, CMYKW)) == icCmmStatBadMCSLink,
        "#2871: four-channel MID, flagged five-channel MVIS: refused");

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
