/*
    File:       iccRoundTrip.cpp

    Contains:   Console app to parse and display profile round-trip statistics

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 10-1-2007
//
//////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cmath>
#include "IccUtil.h"
#include "IccEval.h"
#include "IccPrmg.h"
#include "IccProfLibVer.h"

class CIccMinMaxEval : public CIccEvalCompare
{
public:
  CIccMinMaxEval();

  void Compare(icFloatNumber *pixel, icFloatNumber *deviceLab, icFloatNumber *lab1, icFloatNumber *lab2);

  icFloatNumber GetMean1() { return sum1 / num1; }
  icFloatNumber GetMean2() { return sum2 / num2; }

  icFloatNumber minDE1, minDE2;
  icFloatNumber maxDE1, maxDE2;
  icUInt32Number num3, m_nTotal;

  icFloatNumber maxLab1[3], maxLab2[3];

protected:

  icFloatNumber sum1, sum2;
  icFloatNumber num1, num2;
};

CIccMinMaxEval::CIccMinMaxEval()
{
  minDE1 = minDE2 = 10000;
  maxDE1 = maxDE2 = -1;
  sum1 = sum2 = 0;
  num1 = num2 = 0.0;
  num3 = m_nTotal = 0;

  memset(&maxLab1[0], 0, sizeof(maxLab1));
  memset(&maxLab2[0], 0, sizeof(maxLab2));
}

void CIccMinMaxEval::Compare(icFloatNumber * /*pixel*/, icFloatNumber *deviceLab, icFloatNumber *lab1, icFloatNumber *lab2)
{
  icFloatNumber DE1 = icDeltaE(deviceLab, lab1);
  icFloatNumber DE2 = icDeltaE(lab1, lab2);

  if (DE1<minDE1) {
    minDE1 = DE1;
  }

  if (DE1>maxDE1) {
    maxDE1 = DE1;
    memcpy(&maxLab1[0], deviceLab, sizeof(maxLab1));
  }

  if (DE2<minDE2) {
    minDE2 = DE2;
  }

  if (DE2>maxDE2) {
    maxDE2 = DE2;
    memcpy(&maxLab2[0], deviceLab, sizeof(maxLab2));
  }

  if(DE2 <= 1.0)
    num3 += 1;

  sum1 += DE1;
  num1 += 1.0;

  sum2 += DE2;
  num2 += 1.0;

  m_nTotal += 1;
}


int main(int argc, char* argv[])
{
  if (argc<=1) {
    printf("Usage: iccRoundTrip profile {rendering_intent=1 {use_mpe=0}}\n");
    printf("Built with IccProfLib version " ICCPROFLIBVER "\n");
    printf("  where rendering_intent is (0=perceptual, 1=relative, 2=saturation, 3=absolute)\n");
    return -1;
  }

  icRenderingIntent nIntent = icRelativeColorimetric;
  int nUseMPE = 0;

  if (argc>2) {
    nIntent = (icRenderingIntent)atoi(argv[2]);
    if (argc>3) {
      nUseMPE = atoi(argv[3]);
    }
  }

  CIccMinMaxEval eval;

  icStatusCMM stat = eval.EvaluateProfile(argv[1], 0, nIntent, icInterpLinear, (nUseMPE!=0));

  if (stat!=icCmmStatOk) {
    printf("Unable to perform round trip on '%s'\n", argv[1]);
    return -1;
  }

  CIccPRMG prmg;

  stat = prmg.EvaluateProfile(argv[1], nIntent, icInterpLinear, (nUseMPE!=0));

  if (stat!=icCmmStatOk) {
    printf("Unable to perform PRMG analysis on '%s'\n", argv[1]);
    return -1;
  }

  CIccInfo info;

  printf("Profile:          '%s'\n", argv[1]);
  printf("Rendering Intent: %s\n", info.GetRenderingIntentName(nIntent));
  printf("Specified Gamut:  %s\n", prmg.m_bPrmgImplied ? "Perceptual Reference Medium Gamut" : "Not Specified");

  printf("\nRound Trip 1\n");
  printf(  "------------\n");
  printf("Min DeltaE:    %8.2" ICFLOATSFX "\n", eval.minDE1);
  printf("Mean DeltaE:   %8.2" ICFLOATSFX "\n", eval.GetMean1());
  printf("Max DeltaE:    %8.2" ICFLOATSFX "\n\n", eval.maxDE1);

  printf("Max L, a, b:   " ICFLOATFMT ", " ICFLOATFMT ", " ICFLOATFMT "\n", eval.maxLab1[0], eval.maxLab1[1], eval.maxLab1[2]);

  printf("\nRound Trip 2\n");
  printf(  "------------\n");
  printf("Min DeltaE:    %8.2" ICFLOATSFX "\n", eval.minDE2);
  printf("Mean DeltaE:   %8.2" ICFLOATSFX "\n", eval.GetMean2());
  printf("Max DeltaE:    %8.2" ICFLOATSFX "\n\n", eval.maxDE2);

  printf("Max L, a, b:   " ICFLOATFMT ", " ICFLOATFMT ", " ICFLOATFMT "\n", eval.maxLab2[0], eval.maxLab2[1], eval.maxLab2[2]);

  if (prmg.m_nTotal) {
    printf("\nPRMG Interoperability - Round Trip Results\n");
    printf(  "------------------------------------------------------\n");

    printf("DE <= 1.0 (%8u): %5.1f%%\n", prmg.m_nDE1, (float)prmg.m_nDE1/(float)prmg.m_nTotal*100.0); 
    printf("DE <= 2.0 (%8u): %5.1f%%\n", prmg.m_nDE2, (float)prmg.m_nDE2/(float)prmg.m_nTotal*100.0);
    printf("DE <= 3.0 (%8u): %5.1f%%\n", prmg.m_nDE3, (float)prmg.m_nDE3/(float)prmg.m_nTotal*100.0);
    printf("DE <= 5.0 (%8u): %5.1f%%\n", prmg.m_nDE5, (float)prmg.m_nDE5/(float)prmg.m_nTotal*100.0);
    printf("DE <=10.0 (%8u): %5.1f%%\n", prmg.m_nDE10, (float)prmg.m_nDE10/(float)prmg.m_nTotal*100.0);
    printf("Total     (%8u)\n", prmg.m_nTotal);
  }
  return 0;
}

