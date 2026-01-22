/*
    File:       iccFromCube.cpp

    Contains:   Console app to parse cube file and create ICC.2 device link profile

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
// -Initial implementation by Max Derhak 3-09-2023
//
//////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <string>
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccProfLibVer.h"
#include "IccUtil.h"

class CubeFile
{
public:
  CubeFile(const char* szFilename)
  {
    m_sFilename = szFilename;
  }
  ~CubeFile() { close(); }

  void close()
  {
    if (m_f)
      fclose(m_f);
    m_f = nullptr;
  }

  bool parseHeader()
  {
    if (!open())
      return false;

    m_title.clear();
    m_comments.clear();
    m_sizeLut3D = 0;
    m_fMinInput[0] = m_fMinInput[1] = m_fMinInput[2] = 0.0f;
    m_fMaxInput[0] = m_fMaxInput[1] = m_fMaxInput[2] = 1.0f;

    bool bAddBlankLine = false;
    while (!isEOF()) {
      long pos = ftell(m_f);
      std::string line = getNextLine();

      if (line[0] == '-' || line[0] == '.' || (line[0] >= '0' && line[0] <= '9')) {
        //undo getNextLine so it can be 3D table can be parsed
        fseek(m_f, pos, SEEK_SET);
        break;
      }

      if (!line.size()) {
        if (m_comments.size()) {
          bAddBlankLine = true;
        }
      }
      else if (line.substr(0, 6) == "TITLE ") {
        if (m_title.size()) {
          m_title += "\n";
        }
        m_title += getTitle(line.c_str() + 6);
      }
      else if (line[0] == '#') {
        if (bAddBlankLine) {
          m_comments += "\n";
        }
        if (line[1]==' ')
          m_comments += line.c_str() + 2;
        else
          m_comments += line.c_str() + 1;
        m_comments += '\n';

        bAddBlankLine = false;
      }
      else if (line.substr(0, 12) == "LUT_1D_SiZE ") {
        printf("1DLUTs are not supported\n");
        return false;
      }
      else if (line.substr(0, 12) == "LUT_3D_SIZE ") {
        m_sizeLut3D = atoi(line.c_str() + 12);
      }
      else if (line.substr(0, 19) == "LUT_3D_INPUT_RANGE ") {
        m_fMinInput[0] = m_fMinInput[1] = m_fMinInput[2] = (icFloatNumber)atof(line.c_str() + 19);
        const char* next = getNext(line.c_str() + 19);
        if (next) {
          m_fMaxInput[0] = m_fMaxInput[1] = m_fMaxInput[2] = (icFloatNumber)atof(next);
        }
      }
      else if (line.substr(0, 11) == "DOMAIN_MIN ") {
        m_fMinInput[0] = (icFloatNumber)atof(line.c_str() + 11);
        const char* next = getNext(line.c_str());
        if (next) {
          m_fMinInput[1] = (icFloatNumber)atof(next);
          next = getNext(next);
          if (next) {
            m_fMinInput[2] = (icFloatNumber)atof(next);
          }
          else
            m_fMinInput[2] = m_fMinInput[1];
        }
        else {
          m_fMinInput[1] = m_fMinInput[2] = m_fMinInput[0];
        }
      }
      else if (line.substr(0, 11) == "DOMAIN_MAX ") {
        m_fMaxInput[0] = (icFloatNumber)atof(line.c_str() + 11);
        const char* next = getNext(line.c_str());
        if (next) {
          m_fMaxInput[1] = (icFloatNumber)atof(next);
          next = getNext(next);
          if (next) {
            m_fMaxInput[2] = (icFloatNumber)atof(next);
          }
          else
            m_fMaxInput[2] = m_fMaxInput[1];
        }
        else {
          m_fMaxInput[1] = m_fMaxInput[2] = m_fMaxInput[0];
        }
      }
      else if (line.substr(0, 18) == "LUT_IN_VIDEO_RANGE")
        m_bLutInVideoRange = true;
      else if (line.substr(0, 19) == "LUT_OUT_VIDEO_RANGE")
        m_bLutOutVideoRange = true;
      else {
        printf("Unknown keyword '%s'\n", line.c_str());
        return false;
      }
    }

    return !isEOF();
  }

  std::string getDescription() { return m_title; }
  std::string getCopyright() { return m_comments; }

  icFloatNumber* getMinInput() { return m_fMinInput; }
  icFloatNumber* getMaxInput() { return m_fMaxInput; }

  bool isCustomInputRange()
  {
    if (!icIsNear(m_fMinInput[0], 0.0) || !icIsNear(m_fMinInput[1], 0.0) || !icIsNear(m_fMinInput[2], 0.0) ||
        !icIsNear(m_fMaxInput[0], 1.0) || !icIsNear(m_fMaxInput[1], 1.0) || !icIsNear(m_fMaxInput[2], 1.0))
      return true;
    return false;
  }

  int sizeLut3D() { return m_sizeLut3D; }
  bool parse3DTable(icFloatNumber* toLut, icUInt32Number nSizeLut)
  {
    icUInt32Number num = m_sizeLut3D * m_sizeLut3D * m_sizeLut3D;

    //
    if (!m_sizeLut3D || nSizeLut != num*3)
      return false;

    const char* next;
    for (auto n = 0u; n < num && !isEOF();) {
      std::string line = getNextLine();

      //Skip empty and commented lines
      if (line[0] == '#' || line.size() == 0)
        continue;
      *toLut++ = (icFloatNumber)atof(line.c_str());
      next = getNext(line.c_str());
      if (!next) {
        printf("Invalid 3DLUT entry\n");
        return false;
      }
      *toLut++ = (icFloatNumber)atof(next);
      next = getNext(next);
      if (!next) {
        printf("Invalid 3DLUT entry\n");
        return false;
      }
      *toLut++ = (icFloatNumber)atof(next);

      n++;
    }
    return true;
  }

protected:
  std::string m_sFilename;
  
  bool open()
  {
    if (!m_f) {
      m_f = fopen(m_sFilename.c_str(), "rb");
    }
    else {
      fseek(m_f, 0, SEEK_SET);
    }
    return m_f != nullptr;
  }

  std::string getTitle(const char* str)
  {
    std::string rv;
    bool bNeedQuote = false;
    if (*str == '\"') {
      bNeedQuote = true;
      str++;
    }
    while (*str && (!bNeedQuote || *str != '\"')) {
      rv += *str++;
    }

    return rv;
  }

  const char* getNext(const char* str)
  {
    while (*str && *str == ' ') str++;
    while (*str && *str != ' ') str++;
    while (*str && *str == ' ') str++;

    return str;
  }

  std::string toEnd(const char* str)
  {
    std::string rv;
    while (*str && *str != '\"') {
      rv += *str++;
    }

    return rv;
  }

  bool isEOF() { return m_f ? feof(m_f)!=0 : true; }

#define MAX_LINE_LEN 255

  std::string getNextLine()
  {
    std::string rv;
    for (int n=0; n<MAX_LINE_LEN && !isEOF(); n++) {
      char c = fgetc(m_f);

      if ((c < 0 && feof(m_f)) || c == '\n')
        break;

      if (c == '\r') //skip unsupported carriage returns
        continue;

      rv += (unsigned char)c;
    }

    return rv;
  }

  FILE* m_f=nullptr;

  int m_sizeLut3D = 0;
  icFloatNumber m_fMinInput[3] = { 0.0f, 0.0f, 0.0f };
  icFloatNumber m_fMaxInput[3] = { 1.0f, 1.0f, 1.0f };

  std::string m_title;
  std::string m_comments;

  bool m_bLutInVideoRange = false;
  bool m_bLutOutVideoRange = false;
};

int main(int argc, char* argv[])
{
  if (argc <= 2) {
    printf("Usage: iccFromCube cube_file output_icc_file\n");
    printf("Built with IccProfLib version " ICCPROFLIBVER "\n");

    return -1;
  }

  CubeFile cube(argv[1]);

  if (!cube.parseHeader()) {
    printf("Unable to parse '%s'\n", argv[1]);
    return -2;
  }

  if (!cube.sizeLut3D()) {
    printf("3DLUT not found in '%s'\n", argv[1]);
    return -3;
  }

  CIccProfile profile;

  //Initialize profile header
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV5;
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigRgbData;
  profile.m_Header.deviceClass = icSigLinkClass;

  //Create A2B0 Tag with LUT
  CIccTagMultiProcessElement* pTag = new CIccTagMultiProcessElement(3, 3);
  if (cube.isCustomInputRange()) {
    icFloatNumber* minVal = cube.getMinInput();
    icFloatNumber* maxVal = cube.getMaxInput();
    CIccMpeCurveSet* pCurves = new CIccMpeCurveSet(3);
    CIccSingleSampledCurve* pCurve0 = new CIccSingleSampledCurve(minVal[0], maxVal[0]);
    
    pCurve0->SetSize(2);
    pCurve0->GetSamples()[0] = 0;
    pCurve0->GetSamples()[1] = 1;

    pCurves->SetCurve(0, pCurve0);

    CIccSingleSampledCurve* pCurve1 = pCurve0;
    if (minVal[1] != minVal[0] || maxVal[1] != maxVal[0]) {
      pCurve1 = new CIccSingleSampledCurve(minVal[1], maxVal[1]);

      pCurve1->SetSize(2);
      pCurve1->GetSamples()[0] = 0;
      pCurve1->GetSamples()[1] = 1;
    }

    pCurves->SetCurve(1, pCurve1);

    CIccSingleSampledCurve* pCurve2 = pCurve0;

    if (minVal[2] != minVal[0] || maxVal[2] != maxVal[0]) {
      if (minVal[2] == minVal[1] && maxVal[2] == maxVal[1])
        pCurve2 = pCurve1;
      else {
        pCurve2 = new CIccSingleSampledCurve(minVal[2], maxVal[2]);

        pCurve2->SetSize(2);
        pCurve2->GetSamples()[0] = 0;
        pCurve2->GetSamples()[1] = 1;
      }
    }

    pCurves->SetCurve(2, pCurve2);

    pTag->Attach(pCurves);
  }

  CIccMpeCLUT* pMpeCLUT = new CIccMpeCLUT();
  CIccCLUT* pCLUT = new CIccCLUT(3, 3);
  pCLUT->Init(cube.sizeLut3D());
  bool bSuccess = cube.parse3DTable(pCLUT->GetData(0), pCLUT->NumPoints()*3);

  pMpeCLUT->SetCLUT(pCLUT);
  pTag->Attach(pMpeCLUT);

  profile.AttachTag(icSigAToB0Tag, pTag);

  cube.close();

  if (!bSuccess) {
    printf("Unable to parse LUT from '%s'\n", argv[1]);
    return (-4);
  }

  //Add description Tag
  CIccTagMultiLocalizedUnicode* pTextTag = new CIccTagMultiLocalizedUnicode();
  std::string desc = cube.getDescription();
  if (desc.size()) {
    pTextTag->SetText(desc.c_str());
  }
  else {
    pTextTag->SetText((std::string("Device link created from ") + argv[1]).c_str());
  }
  profile.AttachTag(icSigProfileDescriptionTag, pTextTag);


  //Add copyright Tag
  if (cube.getCopyright().size()) {
    pTextTag = new CIccTagMultiLocalizedUnicode();
    pTextTag->SetText(cube.getCopyright().c_str());
    profile.AttachTag(icSigCopyrightTag, pTextTag);
  }

  if (SaveIccProfile(argv[2], &profile)) {
    printf("'%s' successfully created\n", argv[2]);
  }
  else {
    printf("Unable to save profile '%s'\n", argv[2]);
    return -5;
  }

  return 0;
}

