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
#include <climits>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cfloat>
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccProfLibVer.h"
#include "IccUtil.h"
#include "IccCmdLineUtil.h"

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
      if (pos < 0) {
        printf("header parsing error\n");
        return false;
      }
      std::string line = getNextLine();

      if (line.empty()) {
        if (m_comments.size()) {
          bAddBlankLine = true;
        }
      }
      else if (line[0] == '-' || line[0] == '.' || (line[0] >= '0' && line[0] <= '9')) {
        //undo getNextLine so it can be 3D table can be parsed
        int result = fseek(m_f, pos, SEEK_SET);
        if (result < 0) {
          printf("header parsing error\n");
          return false;
        }
        break;
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
      else if (line.substr(0, 12) == "LUT_1D_SIZE ") {
        printf("1DLUTs are not supported\n");
        return false;
      }
      else if (line.substr(0, 12) == "LUT_3D_SIZE ") {
        int64_t temp;
        if (!parseInteger(line.c_str() + 12, temp)) {
          printf("Invalid LUT_3D_SIZE value\n");
          return false;
        }
        if (temp < 2) {
            printf("LUT too small to process\n");
            return false;
        }
        if (temp > 255) {
            printf("LUT too large to process\n");
            return false;
        }
        m_sizeLut3D = (int)temp;
      }
      else if (line.substr(0, 19) == "LUT_3D_INPUT_RANGE ") {
        const char* cursor = line.c_str() + 19;
        icFloatNumber minVal, maxVal;
        if (!parseNextFloat(cursor, minVal)) {
          printf("Invalid LUT_3D_INPUT_RANGE value\n");
          return false;
        }
        m_fMinInput[0] = m_fMinInput[1] = m_fMinInput[2] = minVal;
        if (parseNextFloat(cursor, maxVal))
          m_fMaxInput[0] = m_fMaxInput[1] = m_fMaxInput[2] = maxVal;
        if (hasTrailingData(cursor)) {
          printf("Invalid LUT_3D_INPUT_RANGE value\n");
          return false;
        }
      }
      else if (line.substr(0, 11) == "DOMAIN_MIN ") {
        const char* cursor = line.c_str() + 11;
        if (!parseNextFloat(cursor, m_fMinInput[0])) {
          printf("Invalid DOMAIN_MIN value\n");
          return false;
        }
        if (!parseNextFloat(cursor, m_fMinInput[1])) {
          m_fMinInput[1] = m_fMinInput[2] = m_fMinInput[0];
        }
        else if (!parseNextFloat(cursor, m_fMinInput[2])) {
          m_fMinInput[2] = m_fMinInput[1];
        }
        if (hasTrailingData(cursor)) {
          printf("Invalid DOMAIN_MIN value\n");
          return false;
        }
      }
      else if (line.substr(0, 11) == "DOMAIN_MAX ") {
        const char* cursor = line.c_str() + 11;
        if (!parseNextFloat(cursor, m_fMaxInput[0])) {
          printf("Invalid DOMAIN_MAX value\n");
          return false;
        }
        if (!parseNextFloat(cursor, m_fMaxInput[1])) {
          m_fMaxInput[1] = m_fMaxInput[2] = m_fMaxInput[0];
        }
        else if (!parseNextFloat(cursor, m_fMaxInput[2])) {
          m_fMaxInput[2] = m_fMaxInput[1];
        }
        if (hasTrailingData(cursor)) {
          printf("Invalid DOMAIN_MAX value\n");
          return false;
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
    if (m_sizeLut3D < 2 || nSizeLut <= 0)
        return false;
    
    if (m_sizeLut3D > 255)
        return false;
    
    uint64_t temp = (uint64_t)m_sizeLut3D * (uint64_t)m_sizeLut3D * (uint64_t)m_sizeLut3D;
    if (temp > UINT_MAX)
        return false;
    icUInt32Number num = (icUInt32Number)temp;

    if ((uint64_t)nSizeLut != temp*3)
      return false;

    const icUInt32Number nGrid = (icUInt32Number)m_sizeLut3D;

    icUInt32Number n = 0;
    for (; n < num && !isEOF();) {
      std::string line = getNextLine();

      //Skip empty and commented lines
      if (line.empty() || line[0] == '#')
        continue;
      const char* cursor = line.c_str();

      // A .cube table and an ICC CLUT disagree about which input axis varies
      // fastest, so the rows cannot be copied across in file order.  A .cube
      // table runs the red index fastest and blue slowest, while CIccCLUT::Init()
      // lays the grid out the other way round: m_DimSize[] shrinks as the channel
      // index rises, so input channel 0 (red) carries the largest stride and
      // varies slowest.  Streaming row n into entry n therefore transposes the
      // first and last input axes, which is why an identity .cube used to swap
      // red and blue -- source (1,0,0) came back as (0,0,1) (#1843).  Recover the
      // row's (r,g,b) grid coordinate from its ordinal and write the triplet at
      // the matching ICC offset instead.  num is bounded by 255^3 above, so the
      // offset cannot overflow icUInt32Number.
      const icUInt32Number r = n % nGrid;
      const icUInt32Number g = (n / nGrid) % nGrid;
      const icUInt32Number b = n / (nGrid * nGrid);
      icFloatNumber* pEntry = toLut + 3 * ((r * nGrid + g) * nGrid + b);

      if (!parseNextFloat(cursor, pEntry[0])) {
        printf("Invalid 3DLUT entry\n");
        return false;
      }
      if (!parseNextFloat(cursor, pEntry[1])) {
        printf("Invalid 3DLUT entry\n");
        return false;
      }
      if (!parseNextFloat(cursor, pEntry[2])) {
        printf("Invalid 3DLUT entry\n");
        return false;
      }
      if (hasTrailingData(cursor)) {
        printf("Invalid 3DLUT entry\n");
        return false;
      }

      n++;
    }
    if (n != num) {
      printf("Incomplete 3DLUT table\n");
      return false;
    }

    while (!isEOF()) {
      std::string line = getNextLine();
      if (!line.empty() && line[0] != '#') {
        printf("Too many 3DLUT entries\n");
        return false;
      }
    }

    return true;
  }

protected:
  std::string m_sFilename;

  static bool isSpace(char c)
  {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
  }

  bool parseNextFloat(const char*& str, icFloatNumber& value)
  {
    while (*str && isSpace(*str))
      str++;

    if (!*str || *str == '#')
      return false;

    errno = 0;
    char* end = nullptr;
    double temp = std::strtod(str, &end);
    if (end == str || errno == ERANGE || !std::isfinite(temp) || temp < -FLT_MAX || temp > FLT_MAX)
      return false;

    const char* tokenEnd = end;
    if (*tokenEnd && !isSpace(*tokenEnd) && *tokenEnd != '#')
      return false;

    value = (icFloatNumber)temp;
    str = end;
    return true;
  }

  bool parseInteger(const char* str, int64_t& value)
  {
    while (*str && isSpace(*str))
      str++;

    if (!*str || *str == '#')
      return false;

    errno = 0;
    char* end = nullptr;
    long long temp = std::strtoll(str, &end, 10);
    if (end == str || errno == ERANGE)
      return false;

    if (*end && !isSpace(*end) && *end != '#')
      return false;
    if (hasTrailingData(end))
      return false;

    value = (int64_t)temp;
    return true;
  }

  bool hasTrailingData(const char* str)
  {
    while (*str && isSpace(*str))
      str++;

    return *str && *str != '#';
  }
  
  bool open()
  {
    if (!m_f) {
      m_f = fopen(m_sFilename.c_str(), "rb");
    }
    else {
      int result = fseek(m_f, 0, SEEK_SET);
      if (result < 0)
        return false;
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

    return *str ? str : nullptr;
  }

  std::string toEnd(const char* str)
  {
    std::string rv;
    while (*str && *str != '\"') {
      rv += *str++;
    }

    return rv;
  }

  // ferror() belongs in this test as much as feof().  A read that FAILS -- a directory
  // (EISDIR), an I/O error mid-file -- makes fgetc() return EOF while leaving feof()
  // FALSE and setting ferror() instead, so getNextLine() returned an empty string and
  // the three `while (!isEOF())` loops below never advanced: iccFromCube spun forever
  // on a directory argument, at exit code 124 under any timeout (#2414, CWE-835).
  //
  // This is the whole fix, deliberately.  Probing the path with icIsReadableFile()
  // before opening it looks like the tidier guard and is a REGRESSION: the probe
  // opens, reads and closes, so a second fopen() of a single-reader FIFO blocks on a
  // writer that has already been consumed.  Measured -- `mkfifo p; cat x.cube > p &`
  // then iccFromCube on p exits 254 here and hung at 124 with that guard in place.
  // Validate the stream you are holding, not the path you are about to open again.
  //
  // It does NOT terminate every unreadable stream, and the limit is worth naming:
  // getNextLine()'s own `while ((c = fgetc(m_f)) != EOF && c != '\n')` never consults
  // isEOF(), so an ENDLESS readable stream still spins -- /dev/zero hangs at 124 both
  // before and after this change, at a flat RSS, so it is CPU exhaustion rather than an
  // allocation blow-up.  That is a separate pre-existing defect in the line reader:
  // filed as #2442, because bounding it changes what a legitimately long line means and
  // MAX_LINE_LEN below caps what is STORED, not what is CONSUMED.
  bool isEOF() { return m_f ? (feof(m_f)!=0 || ferror(m_f)!=0) : true; }

// Longest line iccFromCube keeps the text of.  The .cube format sets no line
// length limit, but reading a line unbounded would let one pathological line
// drive an unbounded allocation, so a cap stays -- sized so that real titles,
// comment blocks and table rows all fit well inside it rather than sitting just
// above the longest row seen in practice.
#define MAX_LINE_LEN 8192u

  std::string getNextLine()
  {
    std::string rv;
    int c;

    // Consume to the end of the physical line, not merely to MAX_LINE_LEN.  The
    // previous loop stopped once it had taken MAX_LINE_LEN characters *without*
    // consuming the rest of the line, so the tail came back as the following
    // call's "line" and was then parsed as a keyword or a table row in its own
    // right.  A 300-character comment was enough to make a legal .cube fail with
    // "Unknown keyword 'xxx...'" (#1843).  Characters past the cap are dropped
    // from the returned text instead of being re-read: that leaves an over-long
    // table row short of its three floats, so it is rejected explicitly by
    // parse3DTable() rather than silently misread as a different row.
    while ((c = fgetc(m_f)) != EOF && c != '\n') {
      if (c == '\r') //skip unsupported carriage returns
        continue;

      if (rv.size() < MAX_LINE_LEN)
        rv += static_cast<char>(static_cast<unsigned char>(c));
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
  if (argc != 3) {
    printf("Usage: iccFromCube cube_file output_icc_file\n");
    printf("Built with IccProfLib version " ICCPROFLIBVER "\n");

    return argc <= 2 ? 0 : 1;
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
  
  if (!pCLUT->Init(cube.sizeLut3D()) ) {
    printf("Unable to create LUT from '%s'\n", argv[1]);
    delete pCLUT;
    delete pMpeCLUT;
    delete pTag;
    return -4;
  }

  bool bSuccess = cube.parse3DTable(pCLUT->GetData(0), pCLUT->NumPoints()*3);
  if (!bSuccess) {
    printf("Unable to parse LUT from '%s'\n", argv[1]);
    delete pCLUT;
    delete pMpeCLUT;
    delete pTag;
    return (-4);
  }

  pMpeCLUT->SetCLUT(pCLUT);
  pTag->Attach(pMpeCLUT);

  profile.AttachTag(icSigAToB0Tag, pTag);

  cube.close();

  //Add description Tag
  CIccTagMultiLocalizedUnicode* pTextTag = new CIccTagMultiLocalizedUnicode();
  std::string desc = cube.getDescription();
  if (desc.size()) {
    // remove escape sequences and malicious commands
    std::string cleanText = icSanitizeTagText(desc);
    pTextTag->SetText(cleanText.c_str());
  }
  else {
    pTextTag->SetText((std::string("Device link created from ") + argv[1]).c_str());
  }
  profile.AttachTag(icSigProfileDescriptionTag, pTextTag);


  //Add copyright Tag -- required in every profile (ICC.2).  Emit it
  //unconditionally, using the cube comments when present and a default otherwise,
  //so the output is not missing a required tag when the cube had no comments (#1379).
  pTextTag = new CIccTagMultiLocalizedUnicode();
  std::string copyright = cube.getCopyright();
  if (copyright.size()) {
    // remove escape sequences and malicious commands
    std::string cleanText = icSanitizeTagText(copyright);
    pTextTag->SetText(cleanText.c_str());
  }
  else
    pTextTag->SetText("Copyright ICC");
  profile.AttachTag(icSigCopyrightTag, pTextTag);

  //Add profileSequenceDescTag -- required in a DeviceLink profile (ICC.2).  It
  //was omitted entirely, leaving the link non-conformant (#1379).  Describe the
  //single cube-derived stage of the link.
  CIccTagProfileSeqDesc* pSeqTag = new CIccTagProfileSeqDesc();
  CIccProfileDescStruct seqDesc;
  seqDesc.m_deviceMfg = 0;
  seqDesc.m_deviceModel = 0;
  seqDesc.m_attributes = 0;
  seqDesc.m_technology = (icTechnologySignature)0;
  seqDesc.m_deviceMfgDesc.SetType(icSigMultiLocalizedUnicodeType);
  ((CIccTagMultiLocalizedUnicode*)seqDesc.m_deviceMfgDesc.GetTag())->SetText("International Color Consortium");
  seqDesc.m_deviceModelDesc.SetType(icSigMultiLocalizedUnicodeType);
  ((CIccTagMultiLocalizedUnicode*)seqDesc.m_deviceModelDesc.GetTag())->SetText(
    (std::string("Device link created from ") + argv[1]).c_str());
  pSeqTag->m_Descriptions->push_back(seqDesc);
  profile.AttachTag(icSigProfileSequenceDescTag, pSeqTag);

  if (SaveIccProfile(argv[2], &profile)) {
    printf("'%s' successfully created\n", argv[2]);
  }
  else {
    printf("Unable to save profile '%s'\n", argv[2]);
    return 5;
  }

  return 0;
}
