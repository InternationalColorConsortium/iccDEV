/*
    File:       iccV5DspObsToV4Dsp.cpp

    Contains:   Convert an ICCVersion5 display and observer profiles to ICCVersion4 display profile

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2023 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

//////////////////////////////////////////////////////////////////////
// HISTORY:
//
// -Initial implementation by Max Derhak 10-11-2023
//
//////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstring>
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagMPE.h"
#include "IccTagLut.h"
#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include <memory>


// we aren't sharing these, so they could be unique_ptr
typedef std::shared_ptr<CIccProfile> CIccProfileSharedPtr;
typedef std::shared_ptr<CIccApplyTagMpe> CICCApplyMPEPtr;


void Usage() {
    printf("Usage: iccV5DspObsToV4Dsp inputV5.icc inputObserverV5.icc outputV4.icc\n");
    printf("Convert an ICCVersion5 display and observer to ICCVersion4 display profile,\n");
    printf("allowing version5 profiles to be used by legacy applications.\n");
    printf("\tinputV5.icc: input version5 display profile\n");
    printf("\tinputObserverV5.icc: input version5 observer profile\n");
    printf("\toutputV4.icc: output version4 display profile\n");
    printf("Built with IccProfLib version " ICCPROFLIBVER "\n");
}


/**
 ******************************************************************************
 * iccV5DspObsToV4Dsp -- STRICT INPUT CONTRACT
 *
 * This tool performs exactly ONE conversion (see Usage() above):
 *
 *     iccV5DspObsToV4Dsp  inputV5.icc  inputObserverV5.icc  outputV4.icc
 *
 *   inputV5.icc         : an ICC v5 *display* profile carrying SPECTRAL data --
 *                         an AToB1 multiProcessElement of exactly
 *                         [CurveSetElement, EmissionMatrixElement], 3->3 (the
 *                         spectral emission of the R/G/B primaries + white).
 *   inputObserverV5.icc : an ICC v5 *observer* / Profile Connection Conditions
 *                         profile (ColorSpace class) -- a spectralViewingConditions
 *                         tag (observer colour-matching functions + illuminant)
 *                         and a customToStandardPcc multiProcessElement, 3->3.
 *
 * It emits a plain v4.3 RGB matrix/TRC *display* profile (PCSXYZ): the display's
 * spectral emission is integrated against the observer to yield the rXYZ/gXYZ/bXYZ
 * colorants and the media white point (M*(1,1,1)) in the standard D50 PCS, and the
 * display tone response becomes rTRC/gTRC/bTRC.
 *
 * Because only this one output shape is ever produced, the implementation is
 * deliberately narrow: any input not satisfying the contract above is REJECTED
 * (return -2), never coerced.  Do not broaden the accepted inputs without
 * revisiting the construction in main().
 ******************************************************************************
 */
int main(int argc, char* argv[])
{
  if (argc < 4) {   // name + 3 arguments
    Usage();
    return 0;
  }

  CIccProfileSharedPtr dspIcc( ReadIccProfile(argv[1], true) );

  if (!dspIcc) {
    printf("Unable to parse '%s'\n", argv[1]);
    return -2;
  }

  if (dspIcc->m_Header.version < icVersionNumberV5 ||
    dspIcc->m_Header.deviceClass != icSigDisplayClass) {
    printf("%s is not a V5 display profile\n", argv[1]);
    return -2;
  }

  // Per the input contract the display must be RGB: the output is an RGB
  // matrix/TRC profile built from three (1,0,0)/(0,1,0)/(0,0,1) primaries below,
  // so a non-RGB data colour space cannot be honoured.
  if (dspIcc->m_Header.colorSpace != icSigRgbData) {
    printf("%s is not an RGB display profile (data colour space must be 'RGB ')\n", argv[1]);
    return -2;
  }

  CIccTagMultiProcessElement* pTagIn = (CIccTagMultiProcessElement*)dspIcc->FindTagOfType(icSigAToB1Tag, icSigMultiProcessElementType);

  if (!pTagIn) {
    printf("%s doesn't have an AToB1Tag of type mulitProcessElementType\n", argv[1]);
    return -2;
  }

  CIccMultiProcessElement *curveMpe, *matrixMpe;

  if (pTagIn->NumElements() != 2 ||
      pTagIn->NumInputChannels() != 3 ||
      pTagIn->NumOutputChannels() != 3 ||
      ((curveMpe = pTagIn->GetElement(0))==nullptr) ||
        curveMpe->GetType()!= icSigCurveSetElemType ||
      ((matrixMpe = pTagIn->GetElement(1))==nullptr) ||
        matrixMpe->GetType()!=icSigEmissionMatrixElemType) {
    printf("%s doesn't have a spectral emission AToB1Tag\n", argv[1]);
    return -2;
  }

  CIccProfileSharedPtr pccIcc( ReadIccProfile(argv[2]) );

  if (!pccIcc) {
    printf("Unable to parse '%s'\n", argv[2]);
    return -2;
  }

  // The observer input is a v5 Profile Connection Conditions profile, which the
  // ICC spec carries as a ColorSpace-class ('spac') profile (subclass 'pcc ').
  if (pccIcc->m_Header.version < icVersionNumberV5 ||
    pccIcc->m_Header.deviceClass != icSigColorSpaceClass) {
    printf("%s is not a V5 observer (ColorSpace-class PCC) profile\n", argv[2]);
    return -2;
  }

  CIccTagSpectralViewingConditions* pTagSvcn = (CIccTagSpectralViewingConditions*)pccIcc->FindTagOfType(icSigSpectralViewingConditionsTag, icSigSpectralViewingConditionsType);
  CIccTagMultiProcessElement* pTagC2S = (CIccTagMultiProcessElement*)pccIcc->FindTagOfType(icSigCustomToStandardPccTag, icSigMultiProcessElementType);

  // Require both PCC pieces the conversion actually uses: a customToStandardPcc
  // 3->3 transform, and a spectralViewingConditions tag that carries a usable
  // observer (its colour-matching functions are integrated against the display
  // emission below).  Checking getObserver() here turns a would-be cryptic
  // failure in EmissionMatrix::Begin into a clear up-front rejection.
  icSpectralRange obsRange;
  if (!pTagSvcn ||
    !pTagC2S ||
    pTagC2S->NumInputChannels() != 3 ||
    pTagC2S->NumOutputChannels() != 3 ||
    !pTagSvcn->getObserver(obsRange) ||
    !obsRange.steps) {
    printf("%s doesn't have Profile Connection Conditions (observer + customToStandardPcc)\n", argv[2]);
    return -2;
  }

  if (!pTagIn->Begin(icElemInterpLinear, dspIcc.get(), pccIcc.get())) {
    printf("bad tagIn in %s\n", argv[1]);
    return -2;
  }

  CICCApplyMPEPtr pApplyMpe( pTagIn->GetNewApply() );

  auto applyList = pApplyMpe->GetList();
  auto applyIter = applyList->begin();
  auto curveApply = applyIter->ptr;
  applyIter++;
  auto mtxApply = applyIter->ptr;

  if (!pTagC2S->Begin(icElemInterpLinear, pccIcc.get())) {
    printf("bad transform c2s in %s\n", argv[2]);
    return -2;
  }
  
  CICCApplyMPEPtr pAppyC2S( pTagC2S->GetNewApply() );

  CIccProfilePtr pIcc( new CIccProfile() );

  pIcc->InitHeader();
  pIcc->m_Header.deviceClass = icSigDisplayClass;
  pIcc->m_Header.version = icVersionNumberV4_3;
  // The output is an RGB device profile whose connection space is XYZ: a
  // matrix/TRC display profile is PCSXYZ-only (ICC.1 8.3.2).  InitHeader() leaves
  // colorSpace = 0 (NoData) and pcs = the icSigLabData default, so without these
  // two assignments the tool emits a header its own validator rejects (#1359
  // flags colorSpace = 0 as a critical error) and which cannot be round-tripped
  // (#1371).  Set them to match the rTRC/gTRC/bTRC + rXYZ/gXYZ/bXYZ tags below.
  pIcc->m_Header.colorSpace = icSigRgbData;
  pIcc->m_Header.pcs        = icSigXYZData;

  CIccTag* pDesc = dspIcc->FindTag(icSigProfileDescriptionTag);

  CIccTagMultiLocalizedUnicode* pDspText = new CIccTagMultiLocalizedUnicode();
  std::string text;
  if (!icGetTagText(pDesc, text))
    text = std::string("Display profile from '") + argv[1] + "' and PCC '" + argv[2] + "'";
  pDspText->SetText(text.c_str());

  // pointer ownership is passed to the profile
  pIcc->AttachTag(icSigProfileDescriptionTag, pDspText);

  pDspText = new CIccTagMultiLocalizedUnicode();
  pDspText->SetText("Copyright (C) 2025 International Color Consortium");

  // pointer ownership is passed to the profile
  pIcc->AttachTag(icSigCopyrightTag, pDspText);

  CIccTagCurve* pTrcR = new CIccTagCurve(2048);
  CIccTagCurve* pTrcG = new CIccTagCurve(2048);
  CIccTagCurve* pTrcB = new CIccTagCurve(2048);

  icFloatNumber in[3], out[3];
  for (icUInt16Number i=0; i<2048; i++) {
    in[0] = in[1] = in[2] = (icFloatNumber)i / 2047.0f;
    curveMpe->Apply(curveApply, out, in);
    (*pTrcR)[i] = out[0];
    (*pTrcG)[i] = out[1];
    (*pTrcB)[i] = out[2];
  }
  // pointer ownership is passed to the profile
  pIcc->AttachTag(icSigRedTRCTag, pTrcR);
  pIcc->AttachTag(icSigGreenTRCTag, pTrcG);
  pIcc->AttachTag(icSigBlueTRCTag, pTrcB);

  const icFloatNumber rRGB[3] = { 1.0f, 0.0f, 0.0f };
  const icFloatNumber gRGB[3] = { 0.0f, 1.0f, 0.0f };
  const icFloatNumber bRGB[3] = { 0.0f, 0.0f, 1.0f };

  matrixMpe->Apply(mtxApply, in, rRGB);
  pTagC2S->Apply(pAppyC2S.get(), out, in);
 
  // The red/green/blue matrix-column tags (rXYZ/gXYZ/bXYZ, a.k.a.
  // red/green/blueColorantTag) must be XYZType per ICC.1 9.2.46 / 9.2.31 /
  // 9.2.4 (each an array of one XYZNumber, 10.31). Emitting them as
  // s15Fixed16ArrayType ('sf32') produced a non-compliant profile that
  // consumers keying on the spec type (e.g. dynamic_cast<CIccTagXYZ*>) cannot
  // read. Use CIccTagXYZ so the tag carries the correct 'XYZ ' type signature.
  CIccTagXYZ* primaryXYZ = new CIccTagXYZ;
  (*primaryXYZ)[0].X = icDtoF(out[0]); (*primaryXYZ)[0].Y = icDtoF(out[1]); (*primaryXYZ)[0].Z = icDtoF(out[2]);
  pIcc->AttachTag(icSigRedColorantTag, primaryXYZ); // pointer ownership is passed to the profile

  matrixMpe->Apply(mtxApply, in, gRGB);
  pTagC2S->Apply(pAppyC2S.get(), out, in);

  primaryXYZ = new CIccTagXYZ;
  (*primaryXYZ)[0].X = icDtoF(out[0]); (*primaryXYZ)[0].Y = icDtoF(out[1]); (*primaryXYZ)[0].Z = icDtoF(out[2]);
  pIcc->AttachTag(icSigGreenColorantTag, primaryXYZ); // pointer ownership is passed to the profile

  matrixMpe->Apply(mtxApply, in, bRGB);
  pTagC2S->Apply(pAppyC2S.get(), out, in);

  primaryXYZ = new CIccTagXYZ;
  (*primaryXYZ)[0].X = icDtoF(out[0]); (*primaryXYZ)[0].Y = icDtoF(out[1]); (*primaryXYZ)[0].Z = icDtoF(out[2]);
  pIcc->AttachTag(icSigBlueColorantTag, primaryXYZ); // pointer ownership is passed to the profile

  // mediaWhitePointTag is mandatory for a v4 display profile (ICC.1 8.3.4 / Table
  // 32).  The white point is a DERIVED quantity, not a copy of either input's
  // stored white: push full white RGB(1,1,1) through the same observer-integrated
  // emission matrix and custom->standard PCC used for the colorants above -- i.e.
  // M*(1,1,1).  This is exactly the white a CMM reproduces from the output matrix
  // (the sum of the colorant columns), already in the standard D50 PCS.  Neither
  // source white is usable here: argv[1]'s is pre-observer-integration and
  // argv[2]'s is pre-D65->D50 adaptation.  Its absence left the profile
  // non-conformant (#1371).
  const icFloatNumber wRGB[3] = { 1.0f, 1.0f, 1.0f };
  matrixMpe->Apply(mtxApply, in, wRGB);
  pTagC2S->Apply(pAppyC2S.get(), out, in);
  CIccTagXYZ* whiteXYZ = new CIccTagXYZ;
  (*whiteXYZ)[0].X = icDtoF(out[0]); (*whiteXYZ)[0].Y = icDtoF(out[1]); (*whiteXYZ)[0].Z = icDtoF(out[2]);
  pIcc->AttachTag(icSigMediaWhitePointTag, whiteXYZ); // pointer ownership is passed to the profile

  if (!SaveIccProfile(argv[3], pIcc)) {
    printf("Unable to create %s\n", argv[3]);
    delete pIcc;
    return -1;
  }
  printf("%s successfully created\n", argv[3]);
  delete pIcc;

  return 0;
 }
