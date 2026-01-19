/** @file
    File:       IccTagLut.cpp

    Contains:   Implementation of the Lut Tag classes

    Version:    V1

    Copyright:  � see ICC Software License
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
// -Initial implementation by Max Derhak 5-15-2003
//
// -Moved LUT tags to separate file 4-30-2005
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
  #include <windows.h>
#endif
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "IccTag.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include "IccMpeBasic.h"
#include <cmath>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
****************************************************************************
* Name: CIccCurve::Find
* 
* Purpose: Read in the tag contents into a data block
* 
* Args: 
*  v = index to be searched,
*  v0 = index less than/equal to v,
*  p0 = the value at index v0,
*  v1 = index greater than/equal to v,
*  p1 = value at index v1
* 
* Return: The value at the requested index 
*  
*****************************************************************************
*/
icFloatNumber CIccCurve::Find(icFloatNumber v,
  icFloatNumber p0, icFloatNumber v0,
  icFloatNumber p1, icFloatNumber v1)
{
  if (v<=v0)
    return p0;
  if (v>=v1)
    return p1;

  if (p1-p0 <= 0.00001) {
    icFloatNumber d0 = (icFloatNumber)fabs(v-v0);
    icFloatNumber d1 = (icFloatNumber)fabs(v1-v);

    if (d0<d1)
      return p0;
    return p1;
  }

  icFloatNumber np = (icFloatNumber)((p0 + p1)/2.0);
  icFloatNumber nv = Apply(np);

  if (v<=nv) {
    return Find(v, p0, v0, np, nv);
  }
  return Find(v, np, nv, p1, v1);
}


/**
****************************************************************************
* Name: CIccTagCurve::CIccTagCurve
* 
* Purpose: Constructor
* 
*****************************************************************************
*/
CIccTagCurve::CIccTagCurve(int nSize/*=0*/)
{
  m_nSize = (nSize < 0) ? 0 : nSize;
  m_nMaxIndex = 0;
  m_Curve = NULL;
  
  if (m_nSize > 0)
    m_Curve = (icFloatNumber*)calloc(nSize, sizeof(icFloatNumber));
}


/**
****************************************************************************
* Name: CIccTagCurve::CIccTagCurve
* 
* Purpose: Copy Constructor
*
* Args:
*  ITCurve = The CIccTagCurve object to be copied
*****************************************************************************
*/
CIccTagCurve::CIccTagCurve(const CIccTagCurve &ITCurve)
{
  m_nSize = ITCurve.m_nSize;
  m_nMaxIndex = ITCurve.m_nMaxIndex;
  m_Curve = NULL;
  
  if (m_nSize > 0) {
    m_Curve = (icFloatNumber*)calloc(m_nSize, sizeof(icFloatNumber));
    memcpy(m_Curve, ITCurve.m_Curve, m_nSize*sizeof(icFloatNumber));
  }
}


/**
****************************************************************************
* Name: CIccTagCurve::operator=
* 
* Purpose: Copy Operator
*
* Args:
*  CurveTag = The CIccTagCurve object to be copied
*****************************************************************************
*/
CIccTagCurve &CIccTagCurve::operator=(const CIccTagCurve &CurveTag)
{
  if (&CurveTag == this)
    return *this;

  m_nSize = CurveTag.m_nSize;
  m_nMaxIndex = CurveTag.m_nMaxIndex;

  if (m_Curve)
    free(m_Curve);
  m_Curve = NULL;
  
  if (m_nSize > 0) {
    m_Curve = (icFloatNumber*)calloc(m_nSize, sizeof(icFloatNumber));
    memcpy(m_Curve, CurveTag.m_Curve, m_nSize*sizeof(icFloatNumber));
  }

  return *this;
}


/**
****************************************************************************
* Name: CIccTagCurve::~CIccTagCurve
* 
* Purpose: Destructor
* 
*****************************************************************************
*/
CIccTagCurve::~CIccTagCurve()
{
  if (m_Curve)
    free(m_Curve);
}


/**
****************************************************************************
* Name: CIccTagCurve::Read
* 
* Purpose: Read in the tag contents into a data block
* 
* Args:
*  size - # of bytes in tag,
*  pIO - IO object to read tag from
* 
* Return: 
*  true = successful, false = failure
*****************************************************************************
*/
bool CIccTagCurve::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  icUInt32Number headerSize = sizeof(icTagTypeSignature) +
                              sizeof(icUInt32Number) +
                              sizeof(icUInt32Number);
  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number nSize;

  if (!pIO->Read32(&nSize))
    return false;

  if (headerSize + (icUInt64Number)nSize * sizeof(icUInt16Number) > size)
    return false;

  if (!SetSize(nSize, icInitNone))
    return false;

  if (m_nSize) {
    if (pIO->ReadUInt16Float(m_Curve, m_nSize)!= m_nSize)
      return false;
  }

  return true;
}


/**
****************************************************************************
* Name: CIccTagCurve::Write
* 
* Purpose: Write the tag to a file
* 
* Args: 
*  pIO - The IO object to write tag to.
* 
* Return: 
*  true = succesful, false = failure
*****************************************************************************
*/
bool CIccTagCurve::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nSize))
    return false;

  if (m_nSize)
    if (pIO->WriteUInt16Float(m_Curve, m_nSize)!= m_nSize)
      return false;

  pIO->Align32();

  return true;
}


/**
****************************************************************************
* Name: CIccTagCurve::Describe
* 
* Purpose: Dump data associated with the tag to a string
* 
* Args: 
*  sDescription - string to concatenate tag dump to
*****************************************************************************
*/
void CIccTagCurve::Describe(std::string &sDescription, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize], *ptr;

  if (!m_nSize) {
    snprintf(buf, bufSize, "BEGIN_CURVE In_Out\n");
    sDescription += buf;
    sDescription += "Y = X\n";
  }
  else if (m_nSize==1) {
    icFloatNumber dGamma = (icFloatNumber)(m_Curve[0] * 256.0);
    snprintf(buf, bufSize, "BEGIN_CURVE In_Out\n");
    sDescription += buf;
    snprintf(buf, bufSize, "Y = X ^ %.4lf\n", dGamma);
    sDescription += buf;
  }
  else {
    int i;

    snprintf(buf, bufSize, "BEGIN_LUT In_Out 1 1\n");
    sDescription += buf;

    if (nVerboseness > 75) {
      sDescription += "IN OUT\n";

      for (i=0; i<(int)m_nSize; i++) {
        ptr = buf;

        icColorValue(buf, bufSize, (icFloatNumber)i/(m_nSize-1), icSigMCH1Data, 1);
        ptr += strlen(buf);

        strcpy(ptr, " ");
        ptr ++;

        icColorValue(ptr, bufSize, m_Curve[i], icSigMCH1Data, 1);

        ptr += strlen(ptr);

        strcpy(ptr, "\n");
       sDescription += buf;
      }
    }
  }
}


/**
****************************************************************************
* Name: CIccTagCurve::DumpLut
* 
* Purpose: Dump data associated with the tag to a string. Basically has 
*  the same function as Describe()
* 
* Args: 
*  sDescription = string to concatenate tag dump to,
*  szName = name of the curve to be printed,
*  csSig = color space signature of the LUT data,
*  nIndex = the channel number of color space
*****************************************************************************
*/
void CIccTagCurve::DumpLut(std::string &sDescription, const icChar *szName,
  icColorSpaceSignature csSig, int nIndex, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize], *ptr;

  if (!m_nSize) {
    snprintf(buf, bufSize, "BEGIN_CURVE %s\n", szName);
    sDescription += buf;
    sDescription += "Y = X\n";
  }
  else if (m_nSize==1) {
    icFloatNumber dGamma = (icFloatNumber)(m_Curve[0] * 256.0);
    snprintf(buf, bufSize, "BEGIN_CURVE %s\n", szName);
    sDescription += buf;
    snprintf(buf, bufSize, "Y = X ^ %.4lf\n", dGamma);
    sDescription += buf;
  }
  else {
    int i;

    snprintf(buf, bufSize, "BEGIN_LUT %s 1 1\n", szName);

    if (nVerboseness > 75) {
      sDescription += buf;
      sDescription += "IN OUT\n";

      sDescription.reserve(sDescription.size() + m_nSize * 20);

      for (i=0; i<(int)m_nSize; i++) {
        ptr = buf;

        icColorValue(buf, bufSize, (icFloatNumber)i/(m_nSize-1), csSig, nIndex);
        ptr += strlen(buf);

        strcpy(ptr, " ");
        ptr ++;

// TODO - this length is wrong because we're mid buffer, needs work
        icColorValue(ptr, bufSize, m_Curve[i], csSig, nIndex);

        ptr += strlen(ptr);

        strcpy(ptr, "\n");

        sDescription += buf;
      }
    }
  }
}


/**
****************************************************************************
* Name: CIccTagCurve::SetSize
* 
* Purpose: Sets the size of the curve array.
* 
* Args: 
*  nSize - number of entries in the curve,
*  nSizeOpt - flag to zero newly formed values
*****************************************************************************
*/
bool CIccTagCurve::SetSize(icUInt32Number nSize, icTagCurveSizeInit nSizeOpt/*=icInitZero*/)
{
  if (nSize==m_nSize)
    return true;

  if (!nSize && m_Curve) {
    free(m_Curve);
    m_Curve = NULL;
  }
  else {
    if (!m_Curve)
      m_Curve = (icFloatNumber*)malloc(nSize*sizeof(icFloatNumber));
    else
      m_Curve = (icFloatNumber*)icRealloc(m_Curve, nSize*sizeof(icFloatNumber));

    if (!m_Curve) {
      m_nSize = 0;
      return false;
    }

    switch (nSizeOpt) {
    case icInitNone:
    default:
      break;

    case icInitZero:
      if (m_nSize < nSize) {
        memset(&m_Curve[m_nSize], 0, (nSize-m_nSize)*sizeof(icFloatNumber));
      }
      break;

    case icInitIdentity:
      if (nSize>1) {
        icUInt32Number i;
        icFloatNumber last = (icFloatNumber)(nSize-1);

        for (i=0; i<nSize; i++) {
          m_Curve[i] = (icFloatNumber)i/last;
        }
      }
      else if (nSize==1) {
        //Encode a gamma 1.0 u8Fixed8Number converted to 16 bit as a 0.0 to 1.0 float
        m_Curve[0] = (icFloatNumber)(0x0100) / (icFloatNumber)65535.0;
      }
      break;
    }
  }
  m_nSize = nSize;
  m_nMaxIndex = (icUInt16Number)(nSize - 1);

  return true;
}

/**
****************************************************************************
* Name: refIccMAX::CIccTagCurve::SetGamma
* 
* Purpose: Set the curve with a single gamma value.
* 
* Args:
*  gamma - gamma value to use
*****************************************************************************
*/
bool CIccTagCurve::SetGamma(icFloatNumber gamma)
{
  if (!SetSize(1, icInitNone))
    return false;

  icInt16Number whole = (icInt16Number)gamma;
  icFloatNumber frac = gamma - (icFloatNumber)whole;

  m_Curve[0] = (icFloatNumber)((whole * 256) + (frac*256.0)) / (icFloatNumber)65535.0; 
  return true;
}

/**
****************************************************************************
*  
*****************************************************************************
*/
const icFloatNumber VERYSMALLNUM = (icFloatNumber)0.0000001;
static bool IsUnity(const icFloatNumber& num)
{
  return (num>(1.0-VERYSMALLNUM) && num<(1.0+VERYSMALLNUM));
}

/**
****************************************************************************
* Name: CIccTagCurve::IsIdentity
* 
* Purpose: Checks if this is an identity curve.
* 
* Return: true if the curve is an identity
*  
*****************************************************************************
*/
bool CIccTagCurve::IsIdentity()
{
  if (!m_nSize) {
    return true;
  }

  if (m_nSize==1) {
    return  IsUnity(icFloatNumber(m_Curve[0]*65535.0/256.0));
  }

  icUInt32Number i;
  for (i=0; i<m_nSize; i++) {
    if (fabs(m_Curve[i]-((icFloatNumber)i/(icFloatNumber)m_nMaxIndex))>VERYSMALLNUM) {
      return false;
    }
  }

  return true;
}

/**
****************************************************************************
* Name: CIccTagCurve::Apply
* 
* Purpose: Applies the curve to the value passed.
* 
* Args: 
*  v = value to be passed through the curve.
*
* Return: The value modified by the curve. 
*  
*****************************************************************************
*/
icFloatNumber CIccTagCurve::Apply(icFloatNumber v) const
{
  if(v<0.0) v = 0.0;
  else if(v>1.0) v = 1.0;

  icUInt32Number nIndex = (icUInt32Number)(v * m_nMaxIndex);

  if (!m_nSize) {
    return v;
  }
  if (m_nSize==1) {
    //Convert 0.0 to 1.0 float to 16bit and then convert from u8Fixed8Number
    icFloatNumber dGamma = (icFloatNumber)(m_Curve[0] * 65535.0 / 256.0);
    return (icFloatNumber)pow(v, dGamma);
  }
  if (nIndex == m_nMaxIndex) {
    return m_Curve[nIndex];
  }
  else {
    icFloatNumber nDif = v*m_nMaxIndex - nIndex;
    icFloatNumber p0 = m_Curve[nIndex];

    icFloatNumber rv = p0 + (m_Curve[nIndex+1]-p0)*nDif;
    if (rv>1.0)
      rv=1.0;

    return rv;
  }
}


/**
******************************************************************************
* Name: CIccTagCurve::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagCurve::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (sig==icSigBlueTRCTag || sig==icSigRedTRCTag || sig==icSigGreenTRCTag || sig==icSigGrayTRCTag) {
    if (m_nSize>1) {
      if (m_Curve) {
        if (m_Curve[0]>0.0 || m_Curve[m_nSize-1]<1.0) {
          sReport += icMsgValidateWarning;
          sReport += sSigPathName;
          sReport += " - Curve cannot be accurately inverted.\n";
          rv = icMaxStatus(rv, icValidateWarning);
        }
      }
    }
  }

  return rv;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::CIccTagParametricCurve
* 
* Purpose: Constructor
* 
*****************************************************************************
*/
CIccTagParametricCurve::CIccTagParametricCurve()
{
  m_nFunctionType = 0xffff;
  m_nNumParam = 0;
  m_dParam = NULL;
  m_nReserved2 = 0;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::CIccTagParametricCurve
* 
* Purpose: Copy Constructor
*
* Args:
*  ITPC = The CIccTagParametricCurve object to be copied
*****************************************************************************
*/
CIccTagParametricCurve::CIccTagParametricCurve(const CIccTagParametricCurve &ITPC)
{
  m_nFunctionType = ITPC.m_nFunctionType;
  m_nNumParam = ITPC.m_nNumParam;
  m_nReserved2 = 0;

  m_dParam = new icFloatNumber[m_nNumParam];
  memcpy(m_dParam, ITPC.m_dParam, m_nNumParam*sizeof(icFloatNumber));  
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::operator=
* 
* Purpose: Copy Operator
*
* Args:
*  ParamCurveTag = The CIccTagParametricCurve object to be copied
*****************************************************************************
*/
CIccTagParametricCurve &CIccTagParametricCurve::operator=(const CIccTagParametricCurve &ParamCurveTag)
{
  if (&ParamCurveTag == this)
    return *this;

  m_nFunctionType = ParamCurveTag.m_nFunctionType;
  m_nNumParam = ParamCurveTag.m_nNumParam;

  if (m_dParam)
    delete [] m_dParam;
  m_dParam = new icFloatNumber[m_nNumParam];
  memcpy(m_dParam, ParamCurveTag.m_dParam, m_nNumParam*sizeof(icFloatNumber));

  return *this;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::~CIccTagParametricCurve
* 
* Purpose: Destructor
* 
*****************************************************************************
*/
CIccTagParametricCurve::~CIccTagParametricCurve()
{
  if (m_dParam)
    delete [] m_dParam;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::Read
* 
* Purpose: Read in the tag contents into a data block
* 
* Args:
*  size - # of bytes in tag,
*  pIO - IO object to read tag from
* 
* Return: 
*  true = successful, false = failure
*****************************************************************************
*/
bool CIccTagParametricCurve::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt16Number nFunctionType;

  icUInt32Number nHdrSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    2*sizeof(icUInt16Number);

  if ( nHdrSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig) ||
    !pIO->Read32(&m_nReserved) ||
    !pIO->Read16(&nFunctionType) ||
    !pIO->Read16(&m_nReserved2))
    return false;

  SetFunctionType(nFunctionType);

  if (!m_nNumParam) {
    m_nNumParam = (icUInt16Number)((size-nHdrSize) / sizeof(icS15Fixed16Number));
    m_dParam = new icFloatNumber[m_nNumParam];
  }

  if (m_nNumParam) {
    int i;
    if (nHdrSize + m_nNumParam*sizeof(icS15Fixed16Number) > size)
      return false;

    for (i=0; i<m_nNumParam; i++) {
      icS15Fixed16Number num;
      if (!pIO->Read32(&num, 1))
        return false;
      m_dParam[i]=icFtoD(num);
    }
  }

  return true;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::Write
* 
* Purpose: Write the tag to a file
* 
* Args: 
*  pIO - The IO object to write tag to.
* 
* Return: 
*  true = succesful, false = failure
*****************************************************************************
*/
bool CIccTagParametricCurve::Write(CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (!pIO) {
    return false;
  }

  sig = GetType();

  if (!pIO->Write32(&sig) ||
    !pIO->Write32(&m_nReserved) ||
    !pIO->Write16(&m_nFunctionType) ||
    !pIO->Write16(&m_nReserved2))
    return false;

  if (m_nNumParam) {
    int i;
    for (i=0; i<m_nNumParam; i++) {
      icS15Fixed16Number num = icDtoF(m_dParam[i]);
      if (!pIO->Write32(&num, 1))
        return false;
    }
  }

  if (!pIO->Align32())
    return false;

  return true;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::Describe
* 
* Purpose: Dump data associated with the tag to a string
* 
* Args: 
*  sDescription - string to concatenate tag dump to
*****************************************************************************
*/
void CIccTagParametricCurve::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "FunctionType: %04Xh\n", m_nFunctionType);
  sDescription += buf;

  switch(m_nFunctionType) {
case 0x0000:
  snprintf(buf, bufSize, "Y = X ^ %.4lf\n", m_dParam[0]);
  sDescription += buf;
  return;

case 0x0001:
  snprintf(buf, bufSize, "Y = 0 when (X < %.4lf / %.4lf)\n",
    -m_dParam[2], m_dParam[1]);
  sDescription += buf;

  snprintf(buf, bufSize, "Y = (%.4lf * X + %.4lf) ^ %.4lf   when (X >= %.4lf / %.4lf)\n",
    m_dParam[1], m_dParam[2], m_dParam[0],
    m_dParam[2], m_dParam[1]);
  sDescription += buf;
  return;

case 0x0002:
  snprintf(buf, bufSize, "Y = %.4lf   when (X < %.4lf / %.4lf)\n", m_dParam[3],
    -m_dParam[2], m_dParam[1]);
  sDescription += buf;

  snprintf(buf, bufSize, "Y = (%.4lf * X + %.4lf) ^ %.4lf + %.4lf   when (X >= %.4lf / %.4lf)\n",
    m_dParam[1], m_dParam[2], m_dParam[0],
    m_dParam[3],
    -m_dParam[2], m_dParam[1]);
  sDescription += buf;
  return;

case 0x0003:
  snprintf(buf, bufSize, "Y = %lf * X   when (X < %.4lf)\n",
    m_dParam[3], m_dParam[4]);
  sDescription += buf;

  snprintf(buf, bufSize, "Y = (%.4lf * X + %.4lf) ^ %.4lf   when (X >= %.4lf)\n",
    m_dParam[1], m_dParam[2], m_dParam[0],
    m_dParam[4]);
  sDescription += buf;
  return;

case 0x0004:
  snprintf(buf, bufSize, "Y = %lf * X + %.4lf  when (X < %.4lf)\n",
    m_dParam[3], m_dParam[6], m_dParam[4]);
  sDescription += buf;

  snprintf(buf, bufSize, "Y = (%.4lf * X + %.4lf) ^ %.4lf + %.4lf  when (X >= %.4lf)\n",
    m_dParam[1], m_dParam[2], m_dParam[0],
    m_dParam[5], m_dParam[4]);
  sDescription += buf;
  return;

default:
  int i;
  snprintf(buf, bufSize, "Unknown Function with %d parameters:\n", m_nNumParam);
  sDescription += buf;

  for (i=0; i<m_nNumParam; i++) {
    snprintf(buf, bufSize, "Param[%d] = %.4lf\n", i, m_dParam[i]);
    sDescription += buf;
  }
  }
}

/**
****************************************************************************
* Name: CIccTagParametricCurve::DumpLut
* 
* Purpose: Dump data associated with the tag to a string. Basically has 
*  the same function as Describe()
* 
* Args: 
*  sDescription = string to concatenate tag dump to,
*  szName = name of the curve to be printed,
*  csSig = color space signature of the curve data,
*  nIndex = the channel number of color space
*****************************************************************************
*/
void CIccTagParametricCurve::DumpLut(std::string &sDescription, const icChar *szName,
  icColorSpaceSignature /* csSig */, int /* nIndex */, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "BEGIN_CURVE %s\n", szName);
  sDescription += buf;
  Describe(sDescription, nVerboseness);
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::SetFunctionType
* 
* Purpose: Sets the type of the function the Parametric curve represents
* 
* Args: 
*  nFunctionType = the type of the function encoded as 0-4
*
* Return:
*  always true!!
*****************************************************************************
*/
bool CIccTagParametricCurve::SetFunctionType(icUInt16Number nFunctionType)
{
  icUInt16Number nNumParam;

  switch(nFunctionType) {
    case 0x0000:
      nNumParam = 1;
      break;

    case 0x0001:
      nNumParam = 3;
      break;

    case 0x0002:
      nNumParam = 4;
      break;

    case 0x0003:
      nNumParam = 5;
      break;

    case 0x0004:
      nNumParam = 7;
      break;

    default:
      nNumParam = 0;
  }

  if (m_dParam)
    delete m_dParam;
  m_nNumParam = nNumParam;
  m_nFunctionType = nFunctionType;

  if (m_nNumParam)
    m_dParam = new icFloatNumber[m_nNumParam];
  else
    m_dParam = NULL;

  return true;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::IsIdentity
* 
* Purpose: Checks if this is an identity curve.
* 
* Return: true if the curve is an identity
*  
*****************************************************************************
*/
bool CIccTagParametricCurve::IsIdentity()
{
  switch(m_nFunctionType) {
    case 0x0000:
      return IsUnity(m_dParam[0]);

    case 0x0001:
    case 0x0002:
    case 0x0003:
    case 0x0004:
      return false;

    default:
      return true;
  }
}

/**
****************************************************************************
* Name: CIccTagParametricCurve::Apply
* 
* Purpose: Applies the curve to the value passed.
* 
* Args: 
*  x = value to be passed through the curve.
*
* Return: The value modified by the curve. 
*  
*****************************************************************************
*/
icFloatNumber CIccTagParametricCurve::Apply(icFloatNumber X) const
{
  double a, b;

  switch(m_nFunctionType) {
    case 0x0000:
      return (icFloatNumber)pow(X, m_dParam[0]);

    case 0x0001:
      a=m_dParam[1];
      b=m_dParam[2];

      if (X >= -b/a) {
        return (icFloatNumber)pow((double)a*X + b, (double)m_dParam[0]);
      }
      else {
        return 0;
      }

    case 0x0002:
      a=m_dParam[1];
      b=m_dParam[2];

      if (X >= -b/a) {
        return (icFloatNumber)pow((double)a*X + b, (double)m_dParam[0]) + m_dParam[3];
      }
      else {
        return m_dParam[3];
      }

    case 0x0003:
      if (X >= m_dParam[4]) {
        return (icFloatNumber)pow((double)m_dParam[1]*X + m_dParam[2], (double)m_dParam[0]);
      }
      else {
        return m_dParam[3]*X;
      }

    case 0x0004:
      if (X >= m_dParam[4]) {
        return (icFloatNumber)pow((double)m_dParam[1]*X + m_dParam[2], (double)m_dParam[0]) + m_dParam[5];
      }
      else {
        return m_dParam[3]*X + m_dParam[6];
      }

    default:
      return X;
  }
}


/**
******************************************************************************
* Name: CIccTagParametricCurve::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagParametricCurve::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (m_nReserved2!=0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Reserved Value must be zero.\n";

    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  switch(m_nFunctionType) {
case 0x0000:
  if (m_nNumParam!=1) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Number of parameters inconsistent with function type.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }
  break;

case 0x0001:
  if (m_nNumParam!=3) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Number of parameters inconsistent with function type.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }
  break;

case 0x0002:
  if (m_nNumParam!=4) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Number of parameters inconsistent with function type.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }
  break;

case 0x0003:
  if (m_nNumParam!=5) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Number of parameters inconsistent with function type.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }
  break;

case 0x0004:
  if (m_nNumParam!=7) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Number of parameters inconsistent with function type.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }
  break;

default:
  sReport += icMsgValidateCriticalError;
  sReport += sSigPathName;
  sReport += " - Unknown function type.\n";
  rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (sig==icSigBlueTRCTag || sig==icSigRedTRCTag || sig==icSigGreenTRCTag || sig==icSigGrayTRCTag) {
    icFloatNumber lval = Apply(0.0);
    icFloatNumber uval = Apply(1.0);
    if (lval>0.0 || uval<1.0) {
      sReport += icMsgValidateWarning;
      sReport += sSigPathName;
      sReport += " - Curve cannot be accurately inverted.\n";
      rv = icMaxStatus(rv, icValidateWarning);
    }
  }

  return rv;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::CIccTagSegmentedCurve
* 
* Purpose: Constructor
* 
*****************************************************************************
*/
CIccTagSegmentedCurve::CIccTagSegmentedCurve()
{
  m_pCurve = NULL;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::CIccTagSegmentedCurve
* 
* Purpose: Constructor
* 
*****************************************************************************
*/
CIccTagSegmentedCurve::CIccTagSegmentedCurve(const CIccTagSegmentedCurve &ITSCurve)
{
  if (ITSCurve.m_pCurve)
    m_pCurve = (CIccSegmentedCurve*)ITSCurve.m_pCurve->NewCopy();
  else
    m_pCurve = NULL;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::CIccTagSegmentedCurve
* 
* Purpose: copy operator
* 
*****************************************************************************
*/
CIccTagSegmentedCurve &CIccTagSegmentedCurve::operator=(const CIccTagSegmentedCurve &CurveTag)
{
  if (&CurveTag == this)
    return *this;

  if (m_pCurve)
    delete m_pCurve;

  if (CurveTag.m_pCurve)
    m_pCurve = (CIccSegmentedCurve*)CurveTag.m_pCurve->NewCopy();
  else
    m_pCurve = NULL;
  
  return *this;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::~CIccTagSegmentedCurve
* 
* Purpose: Destructor
* 
*****************************************************************************
*/
CIccTagSegmentedCurve::~CIccTagSegmentedCurve()
{
  if (m_pCurve)
    delete m_pCurve;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::Describe
* 
* Purpose: gets information about the segmented curve
* 
*****************************************************************************
*/
void CIccTagSegmentedCurve::Describe(std::string &sDescription, int nVerboseness)
{
  if (m_pCurve) {
    m_pCurve->Describe(sDescription, nVerboseness);
  }
  else {
    sDescription += "TagSegmentedCurve is undefined\n";
  }
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::DumpLut
* 
* Purpose: gets information about segmented curve
* 
*****************************************************************************
*/
void CIccTagSegmentedCurve::DumpLut(std::string &sDescription, const icChar *szName, 
                     icColorSpaceSignature /* csSig */, int /* nIndex */, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "BEGIN_SEGMENTED_CURVE %s\n", szName);
  sDescription += buf;
  Describe(sDescription, nVerboseness);
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::Read
* 
* Purpose: read segmented curve from file
* 
*****************************************************************************
*/
bool CIccTagSegmentedCurve::Read(icUInt32Number size, CIccIO *pIO)
{
   CIccSegmentedCurve *pCurve = new CIccSegmentedCurve();

   if (pCurve->Read(size, pIO)) {
     SetCurve(pCurve);

     return true;
   }

   return false;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::Write
* 
* Purpose: write segmented curve to file
* 
*****************************************************************************
*/
bool CIccTagSegmentedCurve::Write(CIccIO *pIO)
{
  if (!m_pCurve)
    return false;

  return m_pCurve->Write(pIO);
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::SetCurve
* 
* Purpose: set the current curve object
* 
*****************************************************************************
*/
void CIccTagSegmentedCurve::SetCurve(CIccSegmentedCurve *pCurve)
{
  if (m_pCurve)
    delete m_pCurve;

  m_pCurve = pCurve;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::Begin
* 
* Purpose: Get ready to call apply
* 
*****************************************************************************
*/
void CIccTagSegmentedCurve::Begin()
{
  if (m_pCurve)
    m_pCurve->Begin(icElemInterpLinear, NULL);
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::Apply
* 
* Purpose: convert input to output values using segmented curve
* 
*****************************************************************************
*/
icFloatNumber CIccTagSegmentedCurve::Apply(icFloatNumber v) const
{
  if (m_pCurve)
    return m_pCurve->Apply(v);
  
  return v;
}

/**
****************************************************************************
* Name: CIccTagSegmentedCurve::Validate
* 
* Purpose: Constructor
* 
*****************************************************************************
*/
icValidateStatus CIccTagSegmentedCurve::Validate(std::string sigPath, std::string &sReport, const CIccProfile* /* pProfile =NULL*/) const
{
  if (!m_pCurve) {
    sReport += "Invalid Segmented Curve Data!\n";

    return icValidateCriticalError;
  }
  
  return m_pCurve->Validate(sigPath, sReport, NULL);
}


/**
****************************************************************************
* Name: CIccTagSegmentedCurve::IsIdentity
* 
* Purpose: Constructor
* 
*****************************************************************************
*/
bool CIccTagSegmentedCurve::IsIdentity()
{
  if (!m_pCurve)
    return true;

  return false;
}


/**
****************************************************************************
* Name: CIccMatrix::CIccMatrix
* 
* Purpose: Constructor
* 
* Args:
*  bUseConstants = true if the matrix contains additional row for constants
*****************************************************************************
*/
CIccMatrix::CIccMatrix(bool bUseConstants/*=true*/)
{
  m_bUseConstants = bUseConstants;
  m_e[0] = m_e[4] = m_e[8] = 1.0;
  m_e[1] = m_e[2] = m_e[3] =
    m_e[5] = m_e[6] = m_e[7] = 0.0;

  if (!m_bUseConstants) {
    m_e[9] = m_e[10] = m_e[11] = 0.0;
  }
}


/**
****************************************************************************
* Name: CIccMatrix::CIccMatrix
* 
* Purpose: Copy Constructor
*
* Args:
*  MatrixClass = The CIccMatrix object to be copied
*****************************************************************************
*/
CIccMatrix::CIccMatrix(const CIccMatrix &MatrixClass)
{
  m_bUseConstants = MatrixClass.m_bUseConstants;
  memcpy(m_e, MatrixClass.m_e, sizeof(m_e));
}


/**
****************************************************************************
* Name: CIccMatrix::operator=
* 
* Purpose: Copy Operator
*
* Args:
*  MatrixClass = The CIccMatrix object to be copied
*****************************************************************************
*/
CIccMatrix &CIccMatrix::operator=(const CIccMatrix &MatrixClass)
{
  if (&MatrixClass == this)
    return *this;

  m_bUseConstants = MatrixClass.m_bUseConstants;
  memcpy(m_e, MatrixClass.m_e, sizeof(m_e));

  return *this;
}


/**
****************************************************************************
* Name: CIccTagParametricCurve::DumpLut
* 
* Purpose: Dump the matrix data to a string. 
* 
* Args: 
*  sDescription = string to concatenate tag dump to,
*  szName = name of the curve to be printed
*****************************************************************************
*/
void CIccMatrix::DumpLut(std::string &sDescription, const icChar *szName, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "BEGIN_MATRIX %s\n", szName);
  sDescription += buf;

  if (!m_bUseConstants) {
    snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf\n",
      m_e[0], m_e[1], m_e[2]);
    sDescription += buf;
    snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf\n",
      m_e[3], m_e[4], m_e[5]);
    sDescription += buf;
    snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf\n",
      m_e[6], m_e[7], m_e[8]);
    sDescription += buf;
  }
  else {
    snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf  +  %8.4lf\n",
      m_e[0], m_e[1], m_e[2], m_e[9]);
    sDescription += buf;
    snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf  +  %8.4lf\n",
      m_e[3], m_e[4], m_e[5], m_e[10]);
    sDescription += buf;
    snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf  +  %8.4lf\n",
      m_e[6], m_e[7], m_e[8], m_e[11]);
    sDescription += buf;
  }
}

/**
****************************************************************************
* Name: CIccMatrix::IsIdentity
* 
* Purpose: Checks if the matrix is identity
* 
* Return: 
*  true if matrix is identity and uses no constants, else false
*
*****************************************************************************
*/
bool CIccMatrix::IsIdentity()
{
  if (m_bUseConstants) {
    if (fabs(m_e[9])>0.0 || fabs(m_e[10])>0.0 || fabs(m_e[11])>0.0) {
      return false;
    }
  }

  if (!IsUnity(m_e[0]) || !IsUnity(m_e[4]) || !IsUnity(m_e[8])) {
    return false;
  }

  if (fabs(m_e[1])>0.0 || fabs(m_e[2])>0.0 || fabs(m_e[3])>0.0 ||
      fabs(m_e[5])>0.0 || fabs(m_e[6])>0.0 || fabs(m_e[7])>0.0) 
  {
    return false;
  }

  return true;
}

/**
****************************************************************************
* Name: CIccMatrix::Apply
* 
* Purpose: Multiplies the pixel by the matrix.
* 
* Args: 
*  Pixel = Pixel to be multiplied by the matrix
*
*****************************************************************************
*/
void CIccMatrix::Apply(icFloatNumber *Pixel) const
{
  icFloatNumber a=Pixel[0];
  icFloatNumber b=Pixel[1];
  icFloatNumber c=Pixel[2];

  icFloatNumber x = m_e[0]*a + m_e[1]*b + m_e[2]*c;
  icFloatNumber y = m_e[3]*a + m_e[4]*b + m_e[5]*c;
  icFloatNumber z = m_e[6]*a + m_e[7]*b + m_e[8]*c;

  if (m_bUseConstants) {
    x += m_e[9];
    y += m_e[10];
    z += m_e[11];
  }

  Pixel[0] = x;
  Pixel[1] = y;
  Pixel[2] = z;
}


/**
******************************************************************************
* Name: CIccMatrix::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccMatrix::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = icValidateOK;
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (sig==icSigLut8Type || sig==icSigLut16Type) {
    if (pProfile->m_Header.pcs!=icSigXYZData) {
      CIccInfo Info;
      std::string sSigPathName = Info.GetSigPathName(sigPath);
      icFloatNumber sum=0.0;
      for (int i=0; i<9; i++) {
        sum += m_e[i];
      }
      if (m_e[0]!=1.0 || m_e[4]!=1.0 || m_e[9]!=1.0 || sum!=3.0) {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += " - Matrix must be identity.\n";
        rv = icValidateNonCompliant;
      }
    }
  }

  return rv;
}

  
static icFloatNumber ClutUnitClip(icFloatNumber v)
{
	// Check for NaN first - NaN comparisons always return false
  if (std::isnan(v))
    return 0;
  if (v<0)
    return 0;
  else if (v>1.0)
    return 1.0;
 
  return v;
}

/**
**************************************************************************
* Name: CIccApplyCLUT::CIccApplyCLUT
*
* Purpose:
*  Constructor
**************************************************************************
*/
CIccApplyCLUT::CIccApplyCLUT()
{
  m_df = NULL;
  m_s = NULL;
  m_g = NULL;
  m_ig = NULL;
}


/**
**************************************************************************
* Name: CIccApplyNDLutXform::~CIccApplyNDLutXform
*
* Purpose:
*  Destructor
**************************************************************************
*/
CIccApplyCLUT::~CIccApplyCLUT()
{
  if (m_df)
    free(m_df);
  if (m_s)
    free(m_s);
  if (m_g)
    free(m_g);
  if (m_ig)
    free(m_ig);
}


/**
**************************************************************************
* Name: CIccApplyCLUT::Init
*
* Purpose:
*  Initializer
**************************************************************************
*/
bool CIccApplyCLUT::Init(icUInt8Number nSrcChannels, icUInt32Number nNodes)
{
  if (nSrcChannels > 6) {

    m_df = (icFloatNumber*)malloc(nNodes * sizeof(icFloatNumber));
    m_s = (icFloatNumber*)malloc(nSrcChannels * sizeof(icFloatNumber));
    m_g = (icFloatNumber*)malloc(nSrcChannels * sizeof(icFloatNumber));
    m_ig = (icUInt32Number*)malloc(nSrcChannels * sizeof(icUInt32Number));

    if (!m_df || !m_s || !m_g || !m_ig)
      return false;
  }
  return true;
}

/**
 ****************************************************************************
 * Name: CIccCLUT::CIccCLUT
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nInputChannels = number of input channels,
 *  nOutputChannels = number of output channels 
 * 
 *****************************************************************************
 */
CIccCLUT::CIccCLUT(icUInt8Number nInputChannels, icUInt16Number nOutputChannels, icUInt8Number nPrecision/*=2*/)
{
  m_nInput = nInputChannels;
  m_nOutput = nOutputChannels;
  m_nPrecision = nPrecision;
  m_pData = NULL;
  m_nOffset = NULL;
  m_pOutText = NULL;
  m_pVal = NULL;
  m_nNumPoints = 0;
  m_nNodes = 0;
  m_csInput = icSigUnknownData;
  m_csOutput = icSigUnknownData;
  memset(&m_nReserved2, 0 , sizeof(m_nReserved2));

  UnitClip = ClutUnitClip;
}


/**
 ****************************************************************************
 * Name: CIccCLUT::CIccCLUT
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ICLUT = The CIccCLUT object to be copied
 *****************************************************************************
 */
CIccCLUT::CIccCLUT(const CIccCLUT &ICLUT)
{
  m_pData = NULL;
  m_nOffset = NULL;
  m_nInput = ICLUT.m_nInput;
  m_nOutput = ICLUT.m_nOutput;
  m_nPrecision = ICLUT.m_nPrecision;
  m_nNumPoints = ICLUT.m_nNumPoints;

  m_csInput = ICLUT.m_csInput;
  m_csOutput = ICLUT.m_csOutput;

  memcpy(m_GridPoints, ICLUT.m_GridPoints, sizeof(m_GridPoints));
  memcpy(m_DimSize, ICLUT.m_DimSize, sizeof(m_DimSize));
  memcpy(m_GridAdr, ICLUT.m_GridAdr, sizeof(m_GridAdr));
  memcpy(&m_nReserved2, &ICLUT.m_nReserved2, sizeof(m_nReserved2));

  int num = NumPoints()*m_nOutput;
  m_pData = new icFloatNumber[num];
  memcpy(m_pData, ICLUT.m_pData, num*sizeof(icFloatNumber));

  UnitClip = ICLUT.UnitClip;
}


/**
 ****************************************************************************
 * Name: CIccCLUT::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  CLUTTag = The CIccCLUT object to be copied
 *****************************************************************************
 */
CIccCLUT &CIccCLUT::operator=(const CIccCLUT &CLUTTag)
{
  if (&CLUTTag == this)
    return *this;
  
  m_nInput = CLUTTag.m_nInput;
  m_nOutput = CLUTTag.m_nOutput;
  m_nPrecision = CLUTTag.m_nPrecision;
  m_nNumPoints = CLUTTag.m_nNumPoints;

  m_csInput = CLUTTag.m_csInput;
  m_csOutput = CLUTTag.m_csOutput;

  memcpy(m_GridPoints, CLUTTag.m_GridPoints, sizeof(m_GridPoints));
  memcpy(m_DimSize, CLUTTag.m_DimSize, sizeof(m_DimSize));
  memcpy(m_GridAdr, CLUTTag.m_GridAdr, sizeof(m_GridAdr));
  memcpy(m_nReserved2, &CLUTTag.m_nReserved2, sizeof(m_nReserved2));

  int num;
  if (m_pData)
    delete [] m_pData;
  num = NumPoints()*m_nOutput;
  m_pData = new icFloatNumber[num];
  memcpy(m_pData, CLUTTag.m_pData, num*sizeof(icFloatNumber));

  UnitClip = CLUTTag.UnitClip;

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccCLUT::~CIccCLUT
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccCLUT::~CIccCLUT()
{
  if (m_pData)
    delete [] m_pData;

  if (m_nOffset)
    delete [] m_nOffset;

}

/**
 ****************************************************************************
 * Name: CIccCLUT::Init
 * 
 * Purpose: Initializes and sets the size of the CLUT
 * 
 * Args:
 *  nGridPoints = number of grid points in the CLUT
 *****************************************************************************
 */
bool CIccCLUT::Init(icUInt8Number nGridPoints, icUInt32Number nMaxSize, icUInt8Number nBytesPerPoint)
{
  memset(&m_GridPoints, 0, sizeof(m_GridPoints));
  // m_GridPoints[] is a fixed length of 16
  if (m_nInput > 16)
    return false;
  memset(m_GridPoints, nGridPoints, m_nInput);
  return Init(&m_GridPoints[0], nMaxSize, nBytesPerPoint);
}

/**
 ****************************************************************************
 * Name: CIccCLUT::Init
 * 
 * Purpose: Initializes and sets the size of the CLUT
 * 
 * Args:
 *  pGridPoints = number of grid points in the CLUT
 *****************************************************************************
 */
bool CIccCLUT::Init(const icUInt8Number *pGridPoints, icUInt32Number nMaxSize, icUInt8Number nBytesPerPoint)
{
  if (nMaxSize && !nBytesPerPoint)
    return false;
  
  // must have at least 1 input and 1 output
  if (m_nInput < 1 || m_nOutput < 1)
    return false;

  icUInt64Number nNumPoints;
  memset(m_nReserved2, 0, sizeof(m_nReserved2));
  if (pGridPoints!=&m_GridPoints[0]) {
    // m_GridPoints[] is fixed size of 16
    if (m_nInput > 16)
      return false;
    memcpy(m_GridPoints, pGridPoints, m_nInput);
    if (m_nInput<16)
      memset(m_GridPoints+m_nInput, 0, 16-m_nInput);
  }

  // There should be at least 2 grid points for each input dimension for interpolation to work
  for (int i = 0; i < m_nInput; i++) {
    if (pGridPoints[i] < 2)
      return false;
  }

  if (m_pData) {
    delete [] m_pData;
    m_pData = NULL;
  }

  int i = m_nInput-1;

  // m_DimSize[] is a fixed size of 16
  if (i >= 16)
    return false;

  m_DimSize[i] = m_nOutput;
  nNumPoints = m_GridPoints[i];
  for (i--; i>=0; i--) {
    m_DimSize[i] = m_DimSize[i+1] * m_GridPoints[i+1];
    nNumPoints *= m_GridPoints[i];
    if (nMaxSize && nNumPoints * m_nOutput * nBytesPerPoint > nMaxSize)
      return false;
  }
  m_nNumPoints = (icUInt32Number)nNumPoints;

  icUInt32Number nSize = NumPoints() * m_nOutput;

  if (!nSize)
    return false;

  m_pData = new icFloatNumber[nSize];

  return (m_pData != NULL);
}


/**
 ****************************************************************************
 * Name: CIccCLUT::ReadData
 * 
 * Purpose: Reads the CLUT data points into the data buffer
 * 
 * Args:
 *  size = # of bytes in the tag,
 *  pIO = IO object to read data from,
 *  nPrecision = data precision (8bit encoded as 1 or 16bit encoded as 2)
 *
 * Return:
 *  true = data read succesfully,
 *  false = read data failed
 *****************************************************************************
 */
bool CIccCLUT::ReadData(icUInt32Number size, CIccIO *pIO, icUInt8Number nPrecision)
{
  icUInt32Number nNum=NumPoints() * m_nOutput;

  if (nNum * nPrecision > size)
    return false;

  if (m_pData == NULL)
    return false;

  if (nPrecision==1) {
    if (pIO->ReadUInt8Float(m_pData, nNum)!= nNum)
      return false;
  }
  else if (nPrecision==2) {
    if (pIO->ReadUInt16Float(m_pData, nNum)!= nNum)
      return false;
  }
  else
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccCLUT::WriteData
 * 
 * Purpose: Writes the CLUT data points from the data buffer
 * 
 * Args:
 *  pIO = IO object to write data to,
 *  nPrecision = data precision (8bit encoded as 1 or 16bit encoded as 2)
 *
 * Return:
 *  true = data written succesfully,
 *  false = write operation failed
 *****************************************************************************
 */
bool CIccCLUT::WriteData(CIccIO *pIO, icUInt8Number nPrecision)
{
  icUInt32Number nNum=NumPoints() * m_nOutput;

  if (nPrecision==1) {
    if (pIO->WriteUInt8Float(m_pData, nNum)!= nNum)
      return false;
  }
  else if (nPrecision==2) {
    if (pIO->WriteUInt16Float(m_pData, nNum)!= nNum)
      return false;
  }
  else
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccCLUT::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccCLUT::Read(icUInt32Number size, CIccIO *pIO)
{
  if (size < 20)
    return false;

  if (pIO->Read8(m_GridPoints, 16)!=16 ||
      !pIO->Read8(&m_nPrecision) ||
      pIO->Read8(&m_nReserved2[0], 3)!=3)
    return false;

  if (!Init(m_GridPoints, size - 20, m_nPrecision))
    return false;

  return ReadData(size-20, pIO, m_nPrecision);
}


/**
 ****************************************************************************
 * Name: CIccCLUT::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccCLUT::Write(CIccIO *pIO)
{
  if (pIO->Write8(m_GridPoints, 16)!=16 ||
      !pIO->Write8(&m_nPrecision) ||
      pIO->Write8(&m_nReserved2[0], 3)!=3)
    return false;

  return WriteData(pIO, m_nPrecision);
}

/**
 ****************************************************************************
 * Name: CIccCLUT::Iterate
 * 
 * Purpose: Iterate through the CLUT to dump the data
 * 
 * Args: 
 *  sDescription = string to concatenate data dump to,
 *  nIndex = the channel number,
 *  nPos = the current position in the CLUT
 *  bufSize = the size of the buffer m_pOutText, for error checking
 *
 *****************************************************************************
 */
void CIccCLUT::Iterate(std::string &sDescription, icUInt8Number nIndex, icUInt32Number nPos, size_t bufSize, bool bUseLegacy )
{
  if (nIndex < m_nInput) {
    int i;
    for (i=0; i<m_GridPoints[nIndex]; i++) {
      m_GridAdr[nIndex] = i;
      Iterate(sDescription, nIndex+1, nPos, bUseLegacy);
      nPos += m_DimSize[nIndex];
    }
  }
  else {
    icChar *ptr = m_pOutText;
    icChar *ptrEnd = ptr + bufSize;
    icFloatNumber *pData = &m_pData[nPos];
    int i;

    for (i=0; i<m_nInput; i++) {
// TODO - hard coding size temporarily, really should be passed in
      icColorValue(m_pVal, 40, (icFloatNumber)m_GridAdr[i] / (m_GridPoints[i]-1) , m_csInput, i, bUseLegacy);

// TODO - this buffer handling is sloppy, but should be ok for now
// really needs to be refactored, needs some sort of error return if buffer overflows
// right now it will terminate the output when the buffer overflows
      ptr += snprintf(ptr, size_t(ptrEnd-ptr), " %s", m_pVal);
      if (ptr >= ptrEnd)
            return;
    }
    strcpy(ptr, "  ");
    ptr += 2;

    for (i=0; i<m_nOutput; i++) {
// TODO - hard coding size temporarily, really should be passed in
      icColorValue(m_pVal, 40, pData[i], m_csOutput, i, bUseLegacy);

      ptr += snprintf(ptr, size_t(ptrEnd-ptr), " %s", m_pVal);
      if (ptr >= ptrEnd)
            return;
    }
    strcpy(ptr, "\n");
    sDescription += (const icChar*)m_pOutText;

  }
}


/**
 ****************************************************************************
 * Name: CIccCLUT::Iterate
 * 
 * Purpose: Iterate through the CLUT to get the data and execute PixelOp 
 * 
 * Args: 
 *  pExec = pointer to the IIccCLUTExec object that implements the 
 *          IIccCLUTExec::Apply() function
 * 
 *****************************************************************************
 */
void CIccCLUT::Iterate(IIccCLUTExec* pExec)
{
  memset(&m_fGridAdr[0], 0, sizeof(m_fGridAdr));
  if (m_nInput==3) {
    int i,j,k;
    icUInt32Number index=0;
    for (i=0; i<m_GridPoints[0]; i++) {
      for (j=0; j<m_GridPoints[1]; j++) {
        for (k=0; k<m_GridPoints[2]; k++) {
          m_fGridAdr[2] = (icFloatNumber)k/(icFloatNumber)(m_GridPoints[2]-1);
          m_fGridAdr[1] = (icFloatNumber)j/(icFloatNumber)(m_GridPoints[1]-1);
          m_fGridAdr[0] = (icFloatNumber)i/(icFloatNumber)(m_GridPoints[0]-1);

          index = (m_DimSize[0]*i + m_DimSize[1]*j + m_DimSize[2]*k); 
          pExec->PixelOp(m_fGridAdr, &m_pData[index]);

        }
      }
    }
  }
  else if (m_nInput==4) {
    int i,j,k,l;
    icUInt32Number index=0;
    for (i=0; i<m_GridPoints[0]; i++) {
      for (j=0; j<m_GridPoints[1]; j++) {
        for (k=0; k<m_GridPoints[2]; k++) {
          for (l=0; l<m_GridPoints[3]; l++) {
            m_fGridAdr[3] = (icFloatNumber)l/(icFloatNumber)(m_GridPoints[3]-1);
            m_fGridAdr[2] = (icFloatNumber)k/(icFloatNumber)(m_GridPoints[2]-1);
            m_fGridAdr[1] = (icFloatNumber)j/(icFloatNumber)(m_GridPoints[1]-1);
            m_fGridAdr[0] = (icFloatNumber)i/(icFloatNumber)(m_GridPoints[0]-1);

            index = (m_DimSize[0]*i + m_DimSize[1]*j + 
                     m_DimSize[2]*k + m_DimSize[3]*l); 
            pExec->PixelOp(m_fGridAdr, &m_pData[index]);

          }
        }
      }
    }
  }
  else
    SubIterate(pExec, 0, 0);
}


/**
 ****************************************************************************
 * Name: CIccCLUT::SubIterate
 * 
 * Purpose: Iterate through the CLUT to get the data
 * 
 * Args: 
 *  pExec = pointer to the IIccCLUTExec object that implements the 
 *          IIccCLUTExec::Apply() function,
 *  nIndex = the channel number,
 *  nPos = the current position in the CLUT
 * 
 *****************************************************************************
 */
void CIccCLUT::SubIterate(IIccCLUTExec* pExec, icUInt8Number nIndex, icUInt32Number nPos)
{
  if (nIndex < m_nInput) {
    int i;
    for (i=0; i<m_GridPoints[nIndex]; i++) {
      m_fGridAdr[nIndex] = (icFloatNumber)i/(icFloatNumber)(m_GridPoints[nIndex]-1);
      SubIterate(pExec, nIndex+1, nPos);
      nPos += m_DimSize[nIndex];
    }
  }
  else
    pExec->PixelOp(m_fGridAdr, &m_pData[nPos]);
}

/**
 ****************************************************************************
 * Name: CIccCLUT::DumpLut
 * 
 * Purpose: Dump data associated with the tag to a string. 
 * 
 * Args: 
 *  sDescription = string to concatenate tag dump to,
 *  szName = name of the LUT to be printed,
 *  csInput = color space signature of the input data,
 *  csOutput = color space signature of the output data
 *  nVerboseness = how verbose the output is (bigger = more verbose)
 *****************************************************************************
 */
void CIccCLUT::DumpLut(std::string  &sDescription, const icChar *szName,
                       icColorSpaceSignature csInput, icColorSpaceSignature csOutput,
                       int nVerboseness, bool bUseLegacy)
{
  const size_t outSize = 200000;
  const size_t nameSize = 40;
  icChar szOutText[outSize], szColor[nameSize];
  int i;

  snprintf(szOutText, outSize, "BEGIN_LUT %s %d %d\n", szName, m_nInput, m_nOutput);
  sDescription += szOutText;

  if (nVerboseness > 75) {

    for (i=0; i<m_nInput; i++) {
      icColorIndexName(szColor, nameSize, csInput, i, m_nInput, "In");
      snprintf(szOutText, outSize, " %s=%d", szColor, m_GridPoints[i]);
      sDescription += szOutText;
    }

    sDescription += "  ";

    for (i=0; i<m_nOutput; i++) {
      icColorIndexName(szColor, nameSize, csOutput, i, m_nOutput, "Out");
      snprintf(szOutText, outSize, " %s", szColor);
      sDescription += szOutText;
    }

    sDescription += "\n";

    if (nVerboseness > 75) {
      size_t len = 0;
      for (i=0; i<m_nInput; i++) {
        icColorValue(szColor, nameSize, 1.0, csInput, i, bUseLegacy);
        len += strlen(szColor);
      }
      for (i=0; i<m_nOutput; i++) {
        icColorValue(szColor, nameSize, 1.0, csOutput, i, bUseLegacy);
        len += strlen(szColor);
      }
      len += m_nInput + m_nOutput + 6;

      sDescription.reserve(sDescription.size() + NumPoints()*len);

      //Initialize iteration member variables
      m_csInput = csInput;
      m_csOutput = csOutput;
      m_pOutText = szOutText;
      m_pVal = szColor;
      memset(m_GridAdr, 0, 16);

      Iterate(sDescription, 0, 0, outSize, bUseLegacy);
    }
  }
}



/**
 ****************************************************************************
 * Name: CIccCLUT::Begin
 * 
 * Purpose: Initializes the CLUT. Must be called before Apply().
 *
 *****************************************************************************
 */
void CIccCLUT::Begin()
{
  int i;
  for (i=0; i<m_nInput; i++) {
    m_MaxGridPoint[i] = m_GridPoints[i] - 1;
  }
  m_nNodes = (1<<m_nInput);

  if (m_nOffset)
    delete [] m_nOffset;

  m_nOffset = new icUInt32Number[m_nNodes];

  if (m_nInput==1) {
    m_nOffset[0] = n000 = 0;
    m_nOffset[1] = n001 = m_DimSize[0];
  }
  else if (m_nInput==2) {
    m_nOffset[0] = n000 = 0;
    m_nOffset[1] = n001 = m_DimSize[0];
    m_nOffset[2] = n010 = m_DimSize[1];
    m_nOffset[3] = n011 = n001 + n010;
  }
  else if (m_nInput==3) {
    m_nOffset[0] = n000 = 0;
    m_nOffset[1] = n001 = m_DimSize[0];
    m_nOffset[2] = n010 = m_DimSize[1];
    m_nOffset[3] = n011 = n001 + n010;
    m_nOffset[4] = n100 = m_DimSize[2];
    m_nOffset[5] = n101 = n100 + n001;
    m_nOffset[6] = n110 = n100 + n010;
    m_nOffset[7] = n111 = n110 + n001;
  }
  else if (m_nInput == 4) {
    m_nOffset[ 0] = 0;
    m_nOffset[ 1] = n001 = m_DimSize[ 0];
    m_nOffset[ 2] = n010 = m_DimSize[ 1];
    m_nOffset[ 3] = m_nOffset[ 2] + m_nOffset[ 1];
    m_nOffset[ 4] = n100 = m_DimSize[ 2];
    m_nOffset[ 5] = m_nOffset[ 4] + m_nOffset[ 1];
    m_nOffset[ 6] = m_nOffset[ 4] + m_nOffset[ 2];
    m_nOffset[ 7] = m_nOffset[ 4] + m_nOffset[ 3];
    m_nOffset[ 8] = n1000 = m_DimSize[ 3];
    m_nOffset[ 9] = m_nOffset[ 8] + m_nOffset[ 1];
    m_nOffset[10] = m_nOffset[ 8] + m_nOffset[ 2];
    m_nOffset[11] = m_nOffset[ 8] + m_nOffset[ 3];
    m_nOffset[12] = m_nOffset[ 8] + m_nOffset[ 4];
    m_nOffset[13] = m_nOffset[ 8] + m_nOffset[ 5];
    m_nOffset[14] = m_nOffset[ 8] + m_nOffset[ 6];
    m_nOffset[15] = m_nOffset[ 8] + m_nOffset[ 7];
  }
  else if (m_nInput == 5) {
    m_nOffset[ 0] = 0;
    m_nOffset[ 1] = n001 = m_DimSize[ 0];
    m_nOffset[ 2] = n010 = m_DimSize[ 1];
    m_nOffset[ 3] = m_nOffset[ 2] + m_nOffset[ 1];
    m_nOffset[ 4] = n100 = m_DimSize[ 2];
    m_nOffset[ 5] = m_nOffset[ 4] + m_nOffset[ 1];
    m_nOffset[ 6] = m_nOffset[ 4] + m_nOffset[ 2];
    m_nOffset[ 7] = m_nOffset[ 4] + m_nOffset[ 3];
    m_nOffset[ 8] = n1000 = m_DimSize[ 3];
    m_nOffset[ 9] = m_nOffset[ 8] + m_nOffset[ 1];
    m_nOffset[10] = m_nOffset[ 8] + m_nOffset[ 2];
    m_nOffset[11] = m_nOffset[ 8] + m_nOffset[ 3];
    m_nOffset[12] = m_nOffset[ 8] + m_nOffset[ 4];
    m_nOffset[13] = m_nOffset[ 8] + m_nOffset[ 5];
    m_nOffset[14] = m_nOffset[ 8] + m_nOffset[ 6];
    m_nOffset[15] = m_nOffset[ 8] + m_nOffset[ 7];
    m_nOffset[16] = n10000 = m_DimSize[ 4];
    m_nOffset[17] = m_nOffset[16] + m_nOffset[ 1];
    m_nOffset[18] = m_nOffset[16] + m_nOffset[ 2];
    m_nOffset[19] = m_nOffset[16] + m_nOffset[ 3];
    m_nOffset[20] = m_nOffset[16] + m_nOffset[ 4];
    m_nOffset[21] = m_nOffset[16] + m_nOffset[ 5];
    m_nOffset[22] = m_nOffset[16] + m_nOffset[ 6];
    m_nOffset[23] = m_nOffset[16] + m_nOffset[ 7];
    m_nOffset[24] = m_nOffset[16] + m_nOffset[ 8];
    m_nOffset[25] = m_nOffset[16] + m_nOffset[ 9];
    m_nOffset[26] = m_nOffset[16] + m_nOffset[10];
    m_nOffset[27] = m_nOffset[16] + m_nOffset[11];
    m_nOffset[28] = m_nOffset[16] + m_nOffset[12];
    m_nOffset[29] = m_nOffset[16] + m_nOffset[13];
    m_nOffset[30] = m_nOffset[16] + m_nOffset[14];
    m_nOffset[31] = m_nOffset[16] + m_nOffset[15];
  }
  else if (m_nInput == 6) {
    m_nOffset[ 0] = 0;
    m_nOffset[ 1] = n001 = m_DimSize[ 0];
    m_nOffset[ 2] = n010 = m_DimSize[ 1];
    m_nOffset[ 3] = m_nOffset[ 2] + m_nOffset[ 1];
    m_nOffset[ 4] = n100 = m_DimSize[ 2];
    m_nOffset[ 5] = m_nOffset[ 4] + m_nOffset[ 1];
    m_nOffset[ 6] = m_nOffset[ 4] + m_nOffset[ 2];
    m_nOffset[ 7] = m_nOffset[ 4] + m_nOffset[ 3];
    m_nOffset[ 8] = n1000 = m_DimSize[ 3];
    m_nOffset[ 9] = m_nOffset[ 8] + m_nOffset[ 1];
    m_nOffset[10] = m_nOffset[ 8] + m_nOffset[ 2];
    m_nOffset[11] = m_nOffset[ 8] + m_nOffset[ 3];
    m_nOffset[12] = m_nOffset[ 8] + m_nOffset[ 4];
    m_nOffset[13] = m_nOffset[ 8] + m_nOffset[ 5];
    m_nOffset[14] = m_nOffset[ 8] + m_nOffset[ 6];
    m_nOffset[15] = m_nOffset[ 8] + m_nOffset[ 7];
    m_nOffset[16] = n10000 = m_DimSize[ 4];
    m_nOffset[17] = m_nOffset[16] + m_nOffset[ 1];
    m_nOffset[18] = m_nOffset[16] + m_nOffset[ 2];
    m_nOffset[19] = m_nOffset[16] + m_nOffset[ 3];
    m_nOffset[20] = m_nOffset[16] + m_nOffset[ 4];
    m_nOffset[21] = m_nOffset[16] + m_nOffset[ 5];
    m_nOffset[22] = m_nOffset[16] + m_nOffset[ 6];
    m_nOffset[23] = m_nOffset[16] + m_nOffset[ 7];
    m_nOffset[24] = m_nOffset[16] + m_nOffset[ 8];
    m_nOffset[25] = m_nOffset[16] + m_nOffset[ 9];
    m_nOffset[26] = m_nOffset[16] + m_nOffset[10];
    m_nOffset[27] = m_nOffset[16] + m_nOffset[11];
    m_nOffset[28] = m_nOffset[16] + m_nOffset[12];
    m_nOffset[29] = m_nOffset[16] + m_nOffset[13];
    m_nOffset[30] = m_nOffset[16] + m_nOffset[14];
    m_nOffset[31] = m_nOffset[16] + m_nOffset[15];
    m_nOffset[32] = n100000 = m_DimSize[5];
    m_nOffset[33] = m_nOffset[32] + m_nOffset[ 1];
    m_nOffset[34] = m_nOffset[32] + m_nOffset[ 2];
    m_nOffset[35] = m_nOffset[32] + m_nOffset[ 3];
    m_nOffset[36] = m_nOffset[32] + m_nOffset[ 4];
    m_nOffset[37] = m_nOffset[32] + m_nOffset[ 5];
    m_nOffset[38] = m_nOffset[32] + m_nOffset[ 6];
    m_nOffset[39] = m_nOffset[32] + m_nOffset[ 7];
    m_nOffset[40] = m_nOffset[32] + m_nOffset[ 8];
    m_nOffset[41] = m_nOffset[32] + m_nOffset[ 9];
    m_nOffset[42] = m_nOffset[32] + m_nOffset[10];
    m_nOffset[43] = m_nOffset[32] + m_nOffset[11];
    m_nOffset[44] = m_nOffset[32] + m_nOffset[12];
    m_nOffset[45] = m_nOffset[32] + m_nOffset[13];
    m_nOffset[46] = m_nOffset[32] + m_nOffset[14];
    m_nOffset[47] = m_nOffset[32] + m_nOffset[15];
    m_nOffset[48] = m_nOffset[32] + m_nOffset[16];
    m_nOffset[49] = m_nOffset[32] + m_nOffset[17];
    m_nOffset[50] = m_nOffset[32] + m_nOffset[18];
    m_nOffset[51] = m_nOffset[32] + m_nOffset[19];
    m_nOffset[52] = m_nOffset[32] + m_nOffset[20];
    m_nOffset[53] = m_nOffset[32] + m_nOffset[21];
    m_nOffset[54] = m_nOffset[32] + m_nOffset[22];
    m_nOffset[55] = m_nOffset[32] + m_nOffset[23];
    m_nOffset[56] = m_nOffset[32] + m_nOffset[24];
    m_nOffset[57] = m_nOffset[32] + m_nOffset[25];
    m_nOffset[58] = m_nOffset[32] + m_nOffset[26];
    m_nOffset[59] = m_nOffset[32] + m_nOffset[27];
    m_nOffset[60] = m_nOffset[32] + m_nOffset[28];
    m_nOffset[61] = m_nOffset[32] + m_nOffset[29];
    m_nOffset[62] = m_nOffset[32] + m_nOffset[30];
    m_nOffset[63] = m_nOffset[32] + m_nOffset[31];
  }
  else {
    
    m_nOffset[0] = 0;
    int count, nFlag;
    icUInt32Number nPower[2];
    nPower[0] = 0;
    nPower[1] = 1;

    for (count=0; count<m_nInput; count++) {
      m_nPower[count] = (1<<(m_nInput-1-count));
    }

    count = 0;
    nFlag = 1;
    for (icUInt32Number j=1; j<m_nNodes; j++) {
      if (j == nPower[1]) {
        m_nOffset[j] = m_DimSize[count];
        nPower[0] = (1<<count);
        count++;
        nPower[1] = (1<<count);
        nFlag = 1;
      }
      else {
        m_nOffset[j] = m_nOffset[nPower[0]] + m_nOffset[nFlag];
        nFlag++;
      }
    }
  }
}


CIccApplyCLUT* CIccCLUT::GetNewApply()
{
  CIccApplyCLUT* rv = new CIccApplyCLUT();

  if (!rv)
    return NULL;

  if (!rv->Init(m_nInput, m_nNodes)) {
    delete rv;
    return NULL;
  }

  return rv;
}

/**
******************************************************************************
* Name: CIccCLUT::Interp1d
* 
* Purpose: One dimensional interpolation function
*
* Args:
*  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
*******************************************************************************
*/
void CIccCLUT::Interp1d(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number mx = m_MaxGridPoint[0];

  icFloatNumber x = UnitClip(srcPixel[0]) * mx;

  icUInt32Number ix = (icUInt32Number)x;

  icFloatNumber u = x - ix;

  if (ix==mx) {
    ix--;
    u = 1.0;
  }

  icFloatNumber nu = (icFloatNumber)(1.0 - u);

  int i;
  icFloatNumber *p = &m_pData[ix*n001];

  //Normalize grid units
  icFloatNumber dF0, dF1, pv;

  dF0 = nu;
  dF1 =  u;

  for (i=0; i<m_nOutput; i++, p++) {
    pv = p[n000]*dF0 + p[n001]*dF1;

    destPixel[i] = pv;
  }
}


/**
******************************************************************************
* Name: CIccCLUT::Interp2d
* 
* Purpose: Two dimensional interpolation function
*
* Args:
*  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
*******************************************************************************
*/
void CIccCLUT::Interp2d(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number mx = m_MaxGridPoint[0];
  icUInt8Number my = m_MaxGridPoint[1];

  // The UnitClip function pointer is calling "NoClip", but now removes NaN and Inf
  icFloatNumber x = UnitClip(srcPixel[0]) * mx;
  icFloatNumber y = UnitClip(srcPixel[1]) * my;
  
  if (x < 0.0)
    x = 0.0;
  if (y < 0.0)
    y = 0.0;

  icUInt32Number ix = (icUInt32Number)x;
  icUInt32Number iy = (icUInt32Number)y;

  icFloatNumber u = x - ix;
  icFloatNumber t = y - iy;
 
  if (ix == mx) {
    ix--;
    u = 1.0f;
  }
  if (iy == my) {
    iy--;
    t = 1.0f;
  }

  const icFloatNumber nt = 1.0f - t;
  const icFloatNumber nu = 1.0f - u;
  
  int offset = ix*n001 + iy*n010;

  // clip offset to valid data address, in case of bad inputs
  int maxDataSize = NumPoints()*m_nOutput;
  int maxDataOffset = maxDataSize - (m_nOutput + n011);
  if (offset > maxDataOffset)
    offset = maxDataOffset;

  const icFloatNumber *p = &m_pData[ offset ];

  // Normalize grid units
  const icFloatNumber dF0 = nt * nu;
  const icFloatNumber dF1 = nt *  u;
  const icFloatNumber dF2 =  t * nu;
  const icFloatNumber dF3 =  t *  u;

  for (int i=0; i<m_nOutput; i++) {
    icFloatNumber pv = p[n000 + i]*dF0 + p[n001 + i]*dF1 + p[n010 + i]*dF2 + p[n011 + i]*dF3;
    destPixel[i] = pv;
  }
}


/**
 ******************************************************************************
 * Name: CIccCLUT::Interp3dTetra
 * 
 * Purpose: Tetrahedral interpolation function
 *
 * Args:
 *  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
 *******************************************************************************
 */
void CIccCLUT::Interp3dTetra(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number mx = m_MaxGridPoint[0];
  icUInt8Number my = m_MaxGridPoint[1];
  icUInt8Number mz = m_MaxGridPoint[2];

  icFloatNumber x = UnitClip(srcPixel[0]) * mx;
  icFloatNumber y = UnitClip(srcPixel[1]) * my;
  icFloatNumber z = UnitClip(srcPixel[2]) * mz;

  icUInt32Number ix = (icUInt32Number)x;
  icUInt32Number iy = (icUInt32Number)y;
  icUInt32Number iz = (icUInt32Number)z;

  icFloatNumber v = x - ix;
  icFloatNumber u = y - iy;
  icFloatNumber t = z - iz;

  if (ix==mx) {
    ix--;
    v = 1.0f;
  }
  if (iy==my) {
    iy--;
    u = 1.0f;
  }
  if (iz==mz) {
    iz--;
    t = 1.0f;
  }

  int i;
  icFloatNumber *p = &m_pData[ix*n001 + iy*n010 + iz*n100];

  //Normalize grid units

  for (i=0; i<m_nOutput; i++, p++) {
    if (t<u) {
      if (t>v) {
        destPixel[i] = (p[n000] + t*(p[n110]-p[n010]) +
                                      u*(p[n010]-p[n000]) +
                                      v*(p[n111]-p[n110]));
      }
      else if (u<v) {
        destPixel[i] = (p[n000] + t*(p[n111]-p[n011]) + 
                                      u*(p[n011]-p[n001]) +
                                      v*(p[n001]-p[n000]));
      }
      else {
        destPixel[i] = (p[n000] + t*(p[n111]-p[n011]) +
                                      u*(p[n010]-p[n000]) +
                                      v*(p[n011]-p[n010]));
      }
    }
    else { 
      if (t<v) {
        destPixel[i] = (p[n000] + t*(p[n101]-p[n001]) + 
                                      u*(p[n111]-p[n101]) + 
                                      v*(p[n001]-p[n000]));
      }
      else if (u<v) {
        destPixel[i] = (p[n000] + t*(p[n100]-p[n000]) + 
                                      u*(p[n111]-p[n101]) + 
                                      v*(p[n101]-p[n100]));
      }
      else {
        destPixel[i] = (p[n000] + t*(p[n100]-p[n000]) + 
                                      u*(p[n110]-p[n100]) + 
                                      v*(p[n111]-p[n110]));
      }
    }
  }
}



/**
 ******************************************************************************
 * Name: CIccCLUT::Interp3d
 * 
 * Purpose: Three dimensional interpolation function
 *
 * Args:
 *  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
 *******************************************************************************
 */
void CIccCLUT::Interp3d(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number mx = m_MaxGridPoint[0];
  icUInt8Number my = m_MaxGridPoint[1];
  icUInt8Number mz = m_MaxGridPoint[2];

  icFloatNumber x = UnitClip(srcPixel[0]) * mx;
  icFloatNumber y = UnitClip(srcPixel[1]) * my;
  icFloatNumber z = UnitClip(srcPixel[2]) * mz;

  icUInt32Number ix = (icUInt32Number)x;
  icUInt32Number iy = (icUInt32Number)y;
  icUInt32Number iz = (icUInt32Number)z;

  icFloatNumber u = x - ix;
  icFloatNumber t = y - iy;
  icFloatNumber s = z - iz;

  if (ix==mx) {
    ix--;
    u = 1.0f;
  }
  if (iy==my) {
    iy--;
    t = 1.0f;
  }
  if (iz==mz) {
    iz--;
    s = 1.0f;
  }

  icFloatNumber ns = 1.0f - s;
  icFloatNumber nt = 1.0f - t;
  icFloatNumber nu = 1.0f - u;

  int i;
  icFloatNumber *p = &m_pData[ix*n001 + iy*n010 + iz*n100];

  //Normalize grid units
  icFloatNumber dF0, dF1, dF2, dF3, dF4, dF5, dF6, dF7, pv;

  dF0 = ns* nt* nu;
  dF1 = ns* nt*  u;
  dF2 = ns*  t* nu;
  dF3 = ns*  t*  u;
  dF4 =  s* nt* nu;
  dF5 =  s* nt*  u;
  dF6 =  s*  t* nu;
  dF7 =  s*  t*  u;

  for (i=0; i<m_nOutput; i++, p++) {
    pv = p[n000]*dF0 + p[n001]*dF1 + p[n010]*dF2 + p[n011]*dF3 +
         p[n100]*dF4 + p[n101]*dF5 + p[n110]*dF6 + p[n111]*dF7;

    destPixel[i] = pv;
  }
}



/**
 ******************************************************************************
 * Name: CIccCLUT::Interp4d
 * 
 * Purpose: Four dimensional interpolation function
 *
 * Args:
 *  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
 *******************************************************************************
 */
void CIccCLUT::Interp4d(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number mw = m_MaxGridPoint[0];
  icUInt8Number mx = m_MaxGridPoint[1];
  icUInt8Number my = m_MaxGridPoint[2];
  icUInt8Number mz = m_MaxGridPoint[3];

  icFloatNumber w = UnitClip(srcPixel[0]) * mw;
  icFloatNumber x = UnitClip(srcPixel[1]) * mx;
  icFloatNumber y = UnitClip(srcPixel[2]) * my;
  icFloatNumber z = UnitClip(srcPixel[3]) * mz;

  icUInt32Number iw = (icUInt32Number)w;
  icUInt32Number ix = (icUInt32Number)x;
  icUInt32Number iy = (icUInt32Number)y;
  icUInt32Number iz = (icUInt32Number)z;

  icFloatNumber v = w - iw;
  icFloatNumber u = x - ix;
  icFloatNumber t = y - iy;
  icFloatNumber s = z - iz;

  if (iw==mw) {
    iw--;
    v = 1.0f;
  }
  if (ix==mx) {
    ix--;
    u = 1.0f;
  }
  if (iy==my) {
    iy--;
    t = 1.0f;
  }
  if (iz==mz) {
    iz--;
    s = 1.0f;
  }

  icFloatNumber ns = 1.0f - s;
  icFloatNumber nt = 1.0f - t;
  icFloatNumber nu = 1.0f - u;
  icFloatNumber nv = 1.0f - v;

  int i, j;
  icFloatNumber *p = &m_pData[iw*n001 + ix*n010 + iy*n100 + iz*n1000];

  //Normalize grid units
  icFloatNumber dF[16], pv;

  dF[ 0] = ns* nt* nu* nv;
  dF[ 1] = ns* nt* nu*  v;
  dF[ 2] = ns* nt*  u* nv;
  dF[ 3] = ns* nt*  u*  v;
  dF[ 4] = ns*  t* nu* nv;
  dF[ 5] = ns*  t* nu*  v;
  dF[ 6] = ns*  t*  u* nv;
  dF[ 7] = ns*  t*  u*  v;
  dF[ 8] =  s* nt* nu* nv;
  dF[ 9] =  s* nt* nu*  v;
  dF[10] =  s* nt*  u* nv;
  dF[11] =  s* nt*  u*  v;
  dF[12] =  s*  t* nu* nv;
  dF[13] =  s*  t* nu*  v;
  dF[14] =  s*  t*  u* nv;
  dF[15] =  s*  t*  u*  v;

  for (i=0; i<m_nOutput; i++, p++) {
    for (pv=0, j=0; j<16; j++)
      pv += p[m_nOffset[j]] * dF[j];

    destPixel[i] = pv;
  }
}


/**
 ******************************************************************************
 * Name: CIccCLUT::Interp5d
 * 
 * Purpose: Five dimensional interpolation function
 *
 * Args:
 *  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
 *******************************************************************************
 */
void CIccCLUT::Interp5d(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number m0 = m_MaxGridPoint[0];
  icUInt8Number m1 = m_MaxGridPoint[1];
  icUInt8Number m2 = m_MaxGridPoint[2];
  icUInt8Number m3 = m_MaxGridPoint[3];
  icUInt8Number m4 = m_MaxGridPoint[4];

  icFloatNumber g0 = UnitClip(srcPixel[0]) * m0;
  icFloatNumber g1 = UnitClip(srcPixel[1]) * m1;
  icFloatNumber g2 = UnitClip(srcPixel[2]) * m2;
  icFloatNumber g3 = UnitClip(srcPixel[3]) * m3;
  icFloatNumber g4 = UnitClip(srcPixel[4]) * m4;

  icUInt32Number ig0 = (icUInt32Number)g0;
  icUInt32Number ig1 = (icUInt32Number)g1;
  icUInt32Number ig2 = (icUInt32Number)g2;
  icUInt32Number ig3 = (icUInt32Number)g3;
  icUInt32Number ig4 = (icUInt32Number)g4;

  icFloatNumber s4 = g0 - ig0;
  icFloatNumber s3 = g1 - ig1;
  icFloatNumber s2 = g2 - ig2;
  icFloatNumber s1 = g3 - ig3;
  icFloatNumber s0 = g4 - ig4;

  if (ig0==m0) {
    ig0--;
    s4 = 1.0f;
  }
  if (ig1==m1) {
    ig1--;
    s3 = 1.0f;
  }
  if (ig2==m2) {
    ig2--;
    s2 = 1.0f;
  }
  if (ig3==m3) {
    ig3--;
    s1 = 1.0f;
  }
  if (ig4==m4) {
    ig4--;
    s0 = 1.0f;
  }

  icFloatNumber ns0 = 1.0f - s0;
  icFloatNumber ns1 = 1.0f - s1;
  icFloatNumber ns2 = 1.0f - s2;
  icFloatNumber ns3 = 1.0f - s3;
  icFloatNumber ns4 = 1.0f - s4;

  int i, j;
  icFloatNumber *p = &m_pData[ig0*n001 + ig1*n010 + ig2*n100 + ig3*n1000 + ig4*n10000];

  //Normalize grid units
  icFloatNumber dF[32], pv;

  dF[ 0] = ns0 * ns1 * ns2 * ns3 * ns4;
  dF[ 1] = ns0 * ns1 * ns2 * ns3 *  s4;
  dF[ 2] = ns0 * ns1 * ns2 *  s3 * ns4;
  dF[ 3] = ns0 * ns1 * ns2 *  s3 *  s4;
  dF[ 4] = ns0 * ns1 *  s2 * ns3 * ns4;
  dF[ 5] = ns0 * ns1 *  s2 * ns3 *  s4;
  dF[ 6] = ns0 * ns1 *  s2 *  s3 * ns4;
  dF[ 7] = ns0 * ns1 *  s2 *  s3 *  s4;
  dF[ 8] = ns0 *  s1 * ns2 * ns3 * ns4;
  dF[ 9] = ns0 *  s1 * ns2 * ns3 *  s4;
  dF[10] = ns0 *  s1 * ns2 *  s3 * ns4;
  dF[11] = ns0 *  s1 * ns2 *  s3 *  s4;
  dF[12] = ns0 *  s1 *  s2 * ns3 * ns4;
  dF[13] = ns0 *  s1 *  s2 * ns3 *  s4;
  dF[14] = ns0 *  s1 *  s2 *  s3 * ns4;
  dF[15] = ns0 *  s1 *  s2 *  s3 *  s4;
  dF[16] =  s0 * ns1 * ns2 * ns3 * ns4;
  dF[17] =  s0 * ns1 * ns2 * ns3 *  s4;
  dF[18] =  s0 * ns1 * ns2 *  s3 * ns4;
  dF[19] =  s0 * ns1 * ns2 *  s3 *  s4;
  dF[20] =  s0 * ns1 *  s2 * ns3 * ns4;
  dF[21] =  s0 * ns1 *  s2 * ns3 *  s4;
  dF[22] =  s0 * ns1 *  s2 *  s3 * ns4;
  dF[23] =  s0 * ns1 *  s2 *  s3 *  s4;
  dF[24] =  s0 *  s1 * ns2 * ns3 * ns4;
  dF[25] =  s0 *  s1 * ns2 * ns3 *  s4;
  dF[26] =  s0 *  s1 * ns2 *  s3 * ns4;
  dF[27] =  s0 *  s1 * ns2 *  s3 *  s4;
  dF[28] =  s0 *  s1 *  s2 * ns3 * ns4;
  dF[29] =  s0 *  s1 *  s2 * ns3 *  s4;
  dF[30] =  s0 *  s1 *  s2 *  s3 * ns4;
  dF[31] =  s0 *  s1 *  s2 *  s3 *  s4;

  for (i=0; i<m_nOutput; i++, p++) {
    for (pv=0.0, j=0; j<32; j++)
      pv += p[m_nOffset[j]] * dF[j];

    destPixel[i] = pv;
  }
}



/**
 ******************************************************************************
 * Name: CIccCLUT::Interp6d
 * 
 * Purpose: Six dimensional interpolation function
 *
 * Args:
 *  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
 *******************************************************************************
 */
void CIccCLUT::Interp6d(icFloatNumber *destPixel, const icFloatNumber *srcPixel) const
{
  icUInt8Number m0 = m_MaxGridPoint[0];
  icUInt8Number m1 = m_MaxGridPoint[1];
  icUInt8Number m2 = m_MaxGridPoint[2];
  icUInt8Number m3 = m_MaxGridPoint[3];
  icUInt8Number m4 = m_MaxGridPoint[4];
  icUInt8Number m5 = m_MaxGridPoint[5];

  icFloatNumber g0 = UnitClip(srcPixel[0]) * m0;
  icFloatNumber g1 = UnitClip(srcPixel[1]) * m1;
  icFloatNumber g2 = UnitClip(srcPixel[2]) * m2;
  icFloatNumber g3 = UnitClip(srcPixel[3]) * m3;
  icFloatNumber g4 = UnitClip(srcPixel[4]) * m4;
  icFloatNumber g5 = UnitClip(srcPixel[5]) * m5;

  icUInt32Number ig0 = (icUInt32Number)g0;
  icUInt32Number ig1 = (icUInt32Number)g1;
  icUInt32Number ig2 = (icUInt32Number)g2;
  icUInt32Number ig3 = (icUInt32Number)g3;
  icUInt32Number ig4 = (icUInt32Number)g4;
  icUInt32Number ig5 = (icUInt32Number)g5;

  icFloatNumber s5 = g0 - ig0;
  icFloatNumber s4 = g1 - ig1;
  icFloatNumber s3 = g2 - ig2;
  icFloatNumber s2 = g3 - ig3;
  icFloatNumber s1 = g4 - ig4;
  icFloatNumber s0 = g5 - ig5;

  if (ig0==m0) {
    ig0--;
    s5 = 1.0f;
  }
  if (ig1==m1) {
    ig1--;
    s4 = 1.0f;
  }
  if (ig2==m2) {
    ig2--;
    s3 = 1.0f;
  }
  if (ig3==m3) {
    ig3--;
    s2 = 1.0f;
  }
  if (ig4==m4) {
    ig4--;
    s1 = 1.0f;
  }
  if (ig5==m5) {
    ig5--;
    s0 = 1.0f;
  }

  icFloatNumber ns0 = 1.0f - s0;
  icFloatNumber ns1 = 1.0f - s1;
  icFloatNumber ns2 = 1.0f - s2;
  icFloatNumber ns3 = 1.0f - s3;
  icFloatNumber ns4 = 1.0f - s4;
  icFloatNumber ns5 = 1.0f - s5;

  int i, j;
  icFloatNumber *p = &m_pData[ig0*n001 + ig1*n010 + ig2*n100 + ig3*n1000 + ig4*n10000 + ig5*n100000];

  //Normalize grid units
  icFloatNumber dF[64], pv;

  dF[ 0] = ns0 * ns1 * ns2 * ns3 * ns4 * ns5;
  dF[ 1] = ns0 * ns1 * ns2 * ns3 * ns4 *  s5;
  dF[ 2] = ns0 * ns1 * ns2 * ns3 *  s4 * ns5;
  dF[ 3] = ns0 * ns1 * ns2 * ns3 *  s4 *  s5;
  dF[ 4] = ns0 * ns1 * ns2 *  s3 * ns4 * ns5;
  dF[ 5] = ns0 * ns1 * ns2 *  s3 * ns4 *  s5;
  dF[ 6] = ns0 * ns1 * ns2 *  s3 *  s4 * ns5;
  dF[ 7] = ns0 * ns1 * ns2 *  s3 *  s4 *  s5;
  dF[ 8] = ns0 * ns1 *  s2 * ns3 * ns4 * ns5;
  dF[ 9] = ns0 * ns1 *  s2 * ns3 * ns4 *  s5;
  dF[10] = ns0 * ns1 *  s2 * ns3 *  s4 * ns5;
  dF[11] = ns0 * ns1 *  s2 * ns3 *  s4 *  s5;
  dF[12] = ns0 * ns1 *  s2 *  s3 * ns4 * ns5;
  dF[13] = ns0 * ns1 *  s2 *  s3 * ns4 *  s5;
  dF[14] = ns0 * ns1 *  s2 *  s3 *  s4 * ns5;
  dF[15] = ns0 * ns1 *  s2 *  s3 *  s4 *  s5;
  dF[16] = ns0 *  s1 * ns2 * ns3 * ns4 * ns5;
  dF[17] = ns0 *  s1 * ns2 * ns3 * ns4 *  s5;
  dF[18] = ns0 *  s1 * ns2 * ns3 *  s4 * ns5;
  dF[19] = ns0 *  s1 * ns2 * ns3 *  s4 *  s5;
  dF[20] = ns0 *  s1 * ns2 *  s3 * ns4 * ns5;
  dF[21] = ns0 *  s1 * ns2 *  s3 * ns4 *  s5;
  dF[22] = ns0 *  s1 * ns2 *  s3 *  s4 * ns5;
  dF[23] = ns0 *  s1 * ns2 *  s3 *  s4 *  s5;
  dF[24] = ns0 *  s1 *  s2 * ns3 * ns4 * ns5;
  dF[25] = ns0 *  s1 *  s2 * ns3 * ns4 *  s5;
  dF[26] = ns0 *  s1 *  s2 * ns3 *  s4 * ns5;
  dF[27] = ns0 *  s1 *  s2 * ns3 *  s4 *  s5;
  dF[28] = ns0 *  s1 *  s2 *  s3 * ns4 * ns5;
  dF[29] = ns0 *  s1 *  s2 *  s3 * ns4 *  s5;
  dF[30] = ns0 *  s1 *  s2 *  s3 *  s4 * ns5;
  dF[31] = ns0 *  s1 *  s2 *  s3 *  s4 *  s5;
  dF[32] =  s0 * ns1 * ns2 * ns3 * ns4 * ns5;
  dF[33] =  s0 * ns1 * ns2 * ns3 * ns4 *  s5;
  dF[34] =  s0 * ns1 * ns2 * ns3 *  s4 * ns5;
  dF[35] =  s0 * ns1 * ns2 * ns3 *  s4 *  s5;
  dF[36] =  s0 * ns1 * ns2 *  s3 * ns4 * ns5;
  dF[37] =  s0 * ns1 * ns2 *  s3 * ns4 *  s5;
  dF[38] =  s0 * ns1 * ns2 *  s3 *  s4 * ns5;
  dF[39] =  s0 * ns1 * ns2 *  s3 *  s4 *  s5;
  dF[40] =  s0 * ns1 *  s2 * ns3 * ns4 * ns5;
  dF[41] =  s0 * ns1 *  s2 * ns3 * ns4 *  s5;
  dF[42] =  s0 * ns1 *  s2 * ns3 *  s4 * ns5;
  dF[43] =  s0 * ns1 *  s2 * ns3 *  s4 *  s5;
  dF[44] =  s0 * ns1 *  s2 *  s3 * ns4 * ns5;
  dF[45] =  s0 * ns1 *  s2 *  s3 * ns4 *  s5;
  dF[46] =  s0 * ns1 *  s2 *  s3 *  s4 * ns5;
  dF[47] =  s0 * ns1 *  s2 *  s3 *  s4 *  s5;
  dF[48] =  s0 *  s1 * ns2 * ns3 * ns4 * ns5;
  dF[49] =  s0 *  s1 * ns2 * ns3 * ns4 *  s5;
  dF[50] =  s0 *  s1 * ns2 * ns3 *  s4 * ns5;
  dF[51] =  s0 *  s1 * ns2 * ns3 *  s4 *  s5;
  dF[52] =  s0 *  s1 * ns2 *  s3 * ns4 * ns5;
  dF[53] =  s0 *  s1 * ns2 *  s3 * ns4 *  s5;
  dF[54] =  s0 *  s1 * ns2 *  s3 *  s4 * ns5;
  dF[55] =  s0 *  s1 * ns2 *  s3 *  s4 *  s5;
  dF[56] =  s0 *  s1 *  s2 * ns3 * ns4 * ns5;
  dF[57] =  s0 *  s1 *  s2 * ns3 * ns4 *  s5;
  dF[58] =  s0 *  s1 *  s2 * ns3 *  s4 * ns5;
  dF[59] =  s0 *  s1 *  s2 * ns3 *  s4 *  s5;
  dF[60] =  s0 *  s1 *  s2 *  s3 * ns4 * ns5;
  dF[61] =  s0 *  s1 *  s2 *  s3 * ns4 *  s5;
  dF[62] =  s0 *  s1 *  s2 *  s3 *  s4 * ns5;
  dF[63] =  s0 *  s1 *  s2 *  s3 *  s4 *  s5;

  for (i=0; i<m_nOutput; i++, p++) {
    for (pv=0, j=0; j<64; j++)
      pv += p[m_nOffset[j]] * dF[j];

    destPixel[i] = pv;
  }
}


/**
 ******************************************************************************
 * Name: CIccCLUT::InterpND
 * 
 * Purpose: Generic N-dimensional interpolation function
 *
 * Args:
 *  Pixel = Pixel value to be found in the CLUT. Also used to store the result.
 *******************************************************************************
 */
void CIccCLUT::InterpND(icFloatNumber *destPixel, const icFloatNumber *srcPixel, CIccApplyCLUT *pApply) const
{
  icUInt32Number i,j, index = 0;
  icFloatNumber* df = pApply->m_df;
  icFloatNumber* g = pApply->m_g;
  icFloatNumber* s = pApply->m_s;
  icUInt32Number* ig = pApply->m_ig;

  for (i=0; i<m_nInput; i++) {
    g[i] = UnitClip(srcPixel[i]) * m_MaxGridPoint[i];
    ig[i] = (icUInt32Number)g[i];
    s[m_nInput-1-i] = g[i] - ig[i];
    if (ig[i]==m_MaxGridPoint[i]) {
      ig[i]--;
      s[m_nInput-1-i] = 1.0;      
    }
    index += ig[i]*m_DimSize[i];
  }

  icFloatNumber *p = &m_pData[index];
  icFloatNumber temp[2];
  icFloatNumber pv;
  int nFlag = 0;

  for (i=0; i<m_nNodes; i++) {
    df[i] = 1.0f;
  }


  for (i=0; i<m_nInput; i++) {
    temp[0] = 1.0f - s[i];
    temp[1] = s[i];
    index = m_nPower[i];
    for (j=0; j<m_nNodes; j++) {
      df[j] *= temp[nFlag];
      if ((j+1)%index == 0)
        nFlag = !nFlag;
    }
    nFlag = 0;
  }

  for (i=0; i<m_nOutput; i++, p++) {
    for (pv=0, j=0; j<m_nNodes; j++)
      pv += p[m_nOffset[j]] * df[j];

    destPixel[i] = pv;
  }

}


/**
******************************************************************************
* Name: CIccCLUT::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccCLUT::Validate(std::string sigPath, std::string &sReport, const CIccProfile* /* pProfile =NULL */) const
{
  icValidateStatus rv = icValidateOK;
  icSignature sig = icGetFirstSigPathSig(sigPath);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  if (m_nReserved2[0]!=0 || m_nReserved2[1]!=0 || m_nReserved2[2]!=0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Reserved Value must be zero.\n";

    rv = icValidateNonCompliant;
  }

  if (sig==icSigLutAtoBType || sig==icSigLutBtoAType) {
    const size_t tempSize = 256;
    char temp[tempSize];
    for (int i=0; i<m_nInput; i++) {
      if (m_GridPoints[i]<2) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        snprintf(temp, tempSize, " - CLUT: At least 2 grid points should be present in dimension %u.\n",i );
        sReport += temp;
        rv = icMaxStatus(rv, icValidateCriticalError);
      }
    }
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccMBB::CIccMBB
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccMBB::CIccMBB()
{
  m_nInput = 0;
  m_nOutput = 0;

  m_CurvesA = NULL;
  m_CLUT = NULL;
  m_Matrix = NULL;
  m_CurvesM = NULL;
  m_CurvesB = NULL;

  m_csInput  = icSigUnknownData;
  m_csOutput = icSigUnknownData;

  m_bInputMatrix = true;
  m_bUseMCurvesAsBCurves = false;
}


/**
 ****************************************************************************
 * Name: CIccMBB::CIccMBB
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  IMBB = The CIccMBB object to be copied
 *****************************************************************************
 */
CIccMBB::CIccMBB(const CIccMBB &IMBB)
{
 icUInt8Number nCurves;
 int i;
 
  m_bInputMatrix = IMBB.m_bInputMatrix;
  m_bUseMCurvesAsBCurves = IMBB.m_bUseMCurvesAsBCurves;
  m_nInput = IMBB.m_nInput;
  m_nOutput = IMBB.m_nOutput;
  m_csInput = IMBB.m_csInput;
  m_csOutput = IMBB.m_csOutput;

  if (IMBB.m_CLUT) {
    m_CLUT = new CIccCLUT(*IMBB.m_CLUT);
  }
  else
    m_CLUT = NULL;

  if (IMBB.m_CurvesA) {
    nCurves = !IsInputB() ? m_nInput : m_nOutput;

    m_CurvesA = new LPIccCurve[nCurves];
    for (i=0; i<nCurves; i++)
      m_CurvesA[i] = (CIccCurve*)(IMBB.m_CurvesA[i]->NewCopy());
  }
  else {
    m_CurvesA = NULL;
  }

  if (IMBB.m_CurvesM) {
    nCurves = IsInputMatrix() ? m_nInput : m_nOutput;

    m_CurvesM = new LPIccCurve[nCurves];
    for (i=0; i<nCurves; i++)
      m_CurvesM[i] = (CIccCurve*)IMBB.m_CurvesM[i]->NewCopy();
  }
  else {
    m_CurvesM = NULL;
  }

  if (IMBB.m_CurvesB) {
    nCurves = IsInputB() ? m_nInput : m_nOutput;

    m_CurvesB = new LPIccCurve[nCurves];
    for (i=0; i<nCurves; i++)
      m_CurvesB[i] = (CIccCurve*)IMBB.m_CurvesB[i]->NewCopy();
  }
  else {
    m_CurvesB = NULL;
  }

  if (IMBB.m_Matrix) {
    m_Matrix = new CIccMatrix(*IMBB.m_Matrix);
  }
  else {
    m_Matrix = NULL;
  }
}


/**
 ****************************************************************************
 * Name: CIccMBB::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  IMBB = The CIccMBB object to be copied
 *****************************************************************************
 */
CIccMBB &CIccMBB::operator=(const CIccMBB &IMBB)
{
  if (&IMBB == this)
    return *this;

  Cleanup();

  icUInt8Number nCurves;
  int i;

  m_bInputMatrix = IMBB.m_bInputMatrix;
  m_bUseMCurvesAsBCurves = IMBB.m_bUseMCurvesAsBCurves;
  m_nInput = IMBB.m_nInput;
  m_nOutput = IMBB.m_nOutput;
  m_csInput = IMBB.m_csInput;
  m_csOutput = IMBB.m_csOutput;

  if (IMBB.m_CLUT) {
    m_CLUT = new CIccCLUT(*IMBB.m_CLUT);
  }
  else
    m_CLUT = NULL;

  if (IMBB.m_CurvesA) {
    nCurves = !IsInputB() ? m_nInput : m_nOutput;

    m_CurvesA = new LPIccCurve[nCurves];
    for (i=0; i<nCurves; i++)
      m_CurvesA[i] = (CIccCurve*)IMBB.m_CurvesA[i]->NewCopy();
  }
  else {
    m_CurvesA = NULL;
  }

  if (IMBB.m_CurvesM) {
    nCurves = IsInputMatrix() ? m_nInput : m_nOutput;

    m_CurvesM = new LPIccCurve[nCurves];
    for (i=0; i<nCurves; i++)
      m_CurvesM[i] = (CIccCurve*)IMBB.m_CurvesM[i]->NewCopy();
  }
  else {
    m_CurvesM = NULL;
  }

  if (IMBB.m_CurvesB) {
    nCurves = IsInputB() ? m_nInput : m_nOutput;

    m_CurvesB = new LPIccCurve[nCurves];
    for (i=0; i<nCurves; i++)
      m_CurvesB[i] = (CIccCurve*)IMBB.m_CurvesB[i]->NewCopy();
  }
  else {
    m_CurvesB = NULL;
  }

  if (IMBB.m_Matrix) {
    m_Matrix = new CIccMatrix(*IMBB.m_Matrix);
  }
  else {
    m_Matrix = NULL;
  }

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccMBB::~CIccMBB
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccMBB::~CIccMBB()
{
  Cleanup();
}

/**
 ****************************************************************************
 * Name: CIccMBB::Cleanup
 * 
 * Purpose: Frees the memory allocated to the object
 * 
 *****************************************************************************
 */
void CIccMBB::Cleanup()
{
  int i;

  if (IsInputMatrix()) {
    if (m_CurvesB) {
      for (i=0; i<m_nInput; i++) 
        if (m_CurvesB[i])
          delete m_CurvesB[i];

      delete [] m_CurvesB;
      m_CurvesB = NULL;
    }

    if (m_CurvesM) {
      for (i=0; i<m_nInput; i++) 
        if (m_CurvesM[i])
          delete m_CurvesM[i];

      delete [] m_CurvesM;
      m_CurvesM = NULL;
    }


    if (m_CurvesA) {
      for (i=0; i<m_nOutput; i++) 
        if (m_CurvesA[i])
          delete m_CurvesA[i];

      delete [] m_CurvesA;
      m_CurvesA = NULL;
    }

  }
  else {
    if (m_CurvesA) {
      for (i=0; i<m_nInput; i++) 
        if (m_CurvesA[i])
          delete m_CurvesA[i];

      delete [] m_CurvesA;
      m_CurvesA = NULL;
    }

    if (m_CurvesM) {
      for (i=0; i<m_nOutput; i++) 
        if (m_CurvesM[i])
          delete m_CurvesM[i];

      delete [] m_CurvesM;
      m_CurvesM = NULL;
    }

    if (m_CurvesB) {
      for (i=0; i<m_nOutput; i++) 
        if (m_CurvesB[i])
          delete m_CurvesB[i];

      delete [] m_CurvesB;
      m_CurvesB = NULL;
    }
  }

  if (m_Matrix) {
    delete m_Matrix;
    m_Matrix = NULL;
  }

  if (m_CLUT) {
    delete m_CLUT;
    m_CLUT = NULL;
  }
}

/**
 ****************************************************************************
 * Name: CIccMBB::Init
 * 
 * Purpose: Cleans up any prior memory and Initializes the object.
 * 
 * Args:
 *  nInputChannels = number of input channels,
 *  nOutputChannels = number of output channels
 *****************************************************************************
 */
void CIccMBB::Init(icUInt8Number nInputChannels, icUInt8Number nOutputChannels)
{
  Cleanup();
  m_nInput = nInputChannels;
  m_nOutput = nOutputChannels;
}

/**
 ****************************************************************************
 * Name: CIccMBB::SetColorSpaces
 * 
 * Purpose: Sets the input and output color spaces
 *
 * Args:
 *  csInput = input color space signature,
 *  csOutput = output color space signature
 *****************************************************************************
 */
void CIccMBB::SetColorSpaces(icColorSpaceSignature csInput, icColorSpaceSignature csOutput)
{
   m_csInput = csInput;
   m_csOutput = csOutput;
}

/**
 ****************************************************************************
 * Name: CIccMBB::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccMBB::Describe(std::string &sDescription, int nVerboseness)
{
  int i;
  const size_t bufSize = 128;
  const size_t nameSize = 40;
  icChar buf[bufSize], color[nameSize];


  if (IsInputMatrix()) {
    if (m_CurvesB && !m_bUseMCurvesAsBCurves) {
      for (i=0; i<m_nInput; i++) {
        icColorIndexName(color, nameSize, m_csInput, i, m_nInput, "");
        snprintf(buf, bufSize, "B_Curve_%s", color);
        m_CurvesB[i]->DumpLut(sDescription, buf, m_csInput, i, nVerboseness);
      }
    }

    if (m_Matrix)
      m_Matrix->DumpLut(sDescription, "Matrix", nVerboseness);

    if (m_CurvesM) {
      for (i=0; i<m_nInput; i++) {
        icColorIndexName(color, nameSize, m_csInput, i, m_nInput, "");
        if (!m_bUseMCurvesAsBCurves)
          snprintf(buf, bufSize, "M_Curve_%s", color);
        else
          snprintf(buf, bufSize, "B_Curve_%s", color);
        m_CurvesM[i]->DumpLut(sDescription, buf, m_csInput, i, nVerboseness);
      }
    }

    if (m_CLUT)
      m_CLUT->DumpLut(sDescription, "CLUT", m_csInput, m_csOutput, nVerboseness, GetType()==icSigLut16Type);

    if (m_CurvesA) {
      for (i=0; i<m_nOutput; i++) {
        icColorIndexName(color, nameSize, m_csOutput, i, m_nOutput, "");
        snprintf(buf, bufSize, "A_Curve_%s", color);
        m_CurvesA[i]->DumpLut(sDescription, buf, m_csOutput, i, nVerboseness);
      }
    }
  }
  else {
    if (m_CurvesA) {
      for (i=0; i<m_nInput; i++) {
        icColorIndexName(color, nameSize, m_csInput, i, m_nInput, "");
        snprintf(buf, bufSize, "A_Curve_%s", color);
        m_CurvesA[i]->DumpLut(sDescription, buf, m_csInput, i, nVerboseness);
      }
    }

    if (m_CLUT)
      m_CLUT->DumpLut(sDescription, "CLUT", m_csInput, m_csOutput, nVerboseness);

    if (m_CurvesM && this->GetType()!=icSigLut8Type) {
      for (i=0; i<m_nOutput; i++) {
        icColorIndexName(color, nameSize, m_csOutput, i, m_nOutput, "");
        snprintf(buf, bufSize, "M_Curve_%s", color);
        m_CurvesM[i]->DumpLut(sDescription, buf, m_csOutput, i, nVerboseness);
      }
    }

    if (m_Matrix)
      m_Matrix->DumpLut(sDescription, "Matrix", nVerboseness);

    if (m_CurvesB) {
      for (i=0; i<m_nOutput; i++) {
        icColorIndexName(color, nameSize, m_csOutput, i, m_nOutput, "");
        snprintf(buf, bufSize, "B_Curve_%s", color);
        m_CurvesB[i]->DumpLut(sDescription, buf, m_csOutput, i, nVerboseness);
      }
    }
  }
}


/**
******************************************************************************
* Name: CIccMBB::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccMBB::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (!pProfile) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Tag validation incomplete: Pointer to profile unavailable.\n";
    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }
  icUInt32Number nInput, nOutput;

  //Check # of channels 
  switch(sig) {
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
  case icSigAToB3Tag:
    {
      nInput = icGetSpaceSamples(pProfile->m_Header.colorSpace);
      if (m_nInput!=nInput) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        sReport += " - Incorrect number of input channels.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

      nOutput = icGetSpaceSamples(pProfile->m_Header.pcs);
      if (m_nOutput!=nOutput) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        sReport += " - Incorrect number of output channels.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

      break;
    }
  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
  case icSigBToA3Tag:
    {
      nInput = icGetSpaceSamples(pProfile->m_Header.pcs);
      if (m_nInput!=nInput) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        sReport += " - Incorrect number of input channels.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

      nOutput = icGetSpaceSamples(pProfile->m_Header.colorSpace);
      if (m_nOutput!=nOutput) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        sReport += " - Incorrect number of output channels.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

      break;
    }
  case icSigHToS0Tag:
  case icSigHToS1Tag:
  case icSigHToS2Tag:
  case icSigHToS3Tag:
  {
    nInput = icGetSpaceSamples(pProfile->m_Header.pcs);
    if (m_nInput != nInput) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - Incorrect number of input channels.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    nOutput = icGetSpaceSamples(pProfile->m_Header.pcs);
    if (m_nOutput != nOutput) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - Incorrect number of output channels.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    break;
  }
  case icSigGamutTag:
    {
      nInput = icGetSpaceSamples(pProfile->m_Header.pcs);
      if (m_nInput!=nInput) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        sReport += " - Incorrect number of input channels.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

      nOutput = 1;
      if (m_nOutput!=nOutput) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        sReport += " - Incorrect number of output channels.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

      break;
    }
  default:
    {
      nInput = m_nInput;
      nOutput = m_nOutput;
    }
  }

  //CLUT check
  if (nInput!=nOutput) {
    if (!m_CLUT) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - CLUT must be present.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);          
    }
  }

  if (m_CLUT) {
    rv = icMaxStatus(rv, m_CLUT->Validate(sigPath, sReport, pProfile));
  }

  return rv;
}

/**
 ****************************************************************************
 * Name: CIccMBB::NewCurvesA
 * 
 * Purpose: Allocates memory for a new set of A-curves
 *
 * Return: Pointer to the LPIccCurve object
 *****************************************************************************
 */
LPIccCurve* CIccMBB::NewCurvesA()
{
  if (m_CurvesA)
    return m_CurvesA;

  icUInt8Number nCurves = !IsInputB() ? m_nInput : m_nOutput;

  m_CurvesA = new LPIccCurve[nCurves];
  memset(m_CurvesA, 0, nCurves * sizeof(LPIccCurve));

  return m_CurvesA;
}


/**
 ****************************************************************************
 * Name: CIccMBB::NewCurvesM
 * 
 * Purpose: Allocates memory for a new set of M-curves
 *
 * Return: Pointer to the LPIccCurve object
 *****************************************************************************
 */
LPIccCurve* CIccMBB::NewCurvesM()
{
  if (m_CurvesM)
    return m_CurvesM;

  icUInt8Number nCurves = IsInputMatrix() ? m_nInput : m_nOutput;

  m_CurvesM = new LPIccCurve[nCurves];
  memset(m_CurvesM, 0, nCurves * sizeof(LPIccCurve));

  return m_CurvesM;
}

/**
 ****************************************************************************
 * Name: CIccMBB::NewCurvesB
 * 
 * Purpose: Allocates memory for a new set of B-curves
 *
 * Return: Pointer to the LPIccCurve object
 *****************************************************************************
 */
LPIccCurve* CIccMBB::NewCurvesB()
{
  if (m_CurvesB)
    return m_CurvesB;

  icUInt8Number nCurves = IsInputB() ? m_nInput : m_nOutput;

  m_CurvesB = new LPIccCurve[nCurves];
  memset(m_CurvesB, 0, nCurves * sizeof(LPIccCurve));

  return m_CurvesB;
}

/**
 ****************************************************************************
 * Name: CIccMBB::NewMatrix
 * 
 * Purpose: Allocates memory for a new matrix
 *
 * Return: Pointer to the CIccMatrix object
 *****************************************************************************
 */
CIccMatrix* CIccMBB::NewMatrix()
{
  if (m_Matrix)
    return m_Matrix;

  m_Matrix = new CIccMatrix;

  return m_Matrix;
}

/**
 ****************************************************************************
 * Name: CIccMBB::NewCLUT
 * 
 * Purpose: Allocates memory for a new CLUT and initializes it
 *
 * Args:
 *  pGridPoints = number of grid points in the CLUT
 *
 * Return: Pointer to the CIccCLUT object
 *****************************************************************************
 */
CIccCLUT* CIccMBB::NewCLUT(icUInt8Number *pGridPoints, icUInt8Number nPrecision/*=2*/)
{
  if (m_CLUT)
    return m_CLUT;

  m_CLUT = new CIccCLUT(m_nInput, m_nOutput, nPrecision);

  m_CLUT->Init(pGridPoints);

  return m_CLUT;
}

/**
****************************************************************************
* Name: CIccMBB::SetCLUT
* 
* Purpose: Assignes CLUT connection to an initialized new CLUT
*
* Args:
*  clut = pointer to a previously allocated CLUT (Onwership is transfered to
*    CIccMBB object).
*
* Return: Pointer to the CIccCLUT object or NULL if clut is incompatible with
*  CIccMBB object.  If the clut is incompatible it is deleted.
*****************************************************************************
*/
CIccCLUT *CIccMBB::SetCLUT(CIccCLUT *clut)
{
  if (clut->GetInputDim() != m_nInput || clut->GetOutputChannels() != m_nOutput) {
    delete clut;
    return NULL;
  }

  if (m_CLUT) {
    delete m_CLUT;
  }

  m_CLUT = clut;
  return clut;
}

/**
 ****************************************************************************
 * Name: CIccMBB::NewCLUT
 * 
 * Purpose: Allocates memory for a new CLUT and initializes it
 *
 * Args:
 *  nGridPoints = number of grid points in the CLUT
 *
 * Return: Pointer to the CIccCLUT object
 *****************************************************************************
 */
CIccCLUT* CIccMBB::NewCLUT(icUInt8Number nGridPoints, icUInt8Number nPrecision/*=2*/)
{
  if (m_CLUT)
    return m_CLUT;

  m_CLUT = new CIccCLUT(m_nInput, m_nOutput, nPrecision);

  m_CLUT->Init(nGridPoints);

  return m_CLUT;
}


/**
 ****************************************************************************
 * Name: CIccTagLutAtoB::CIccTagLutAtoB
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagLutAtoB::CIccTagLutAtoB()
{
  m_bInputMatrix = false;
  m_nReservedWord = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagLutAtoB::CIccTagLutAtoB
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITLA2B = The CIccTagLutAtoB object to be copied
 *****************************************************************************
 */
CIccTagLutAtoB::CIccTagLutAtoB(const CIccTagLutAtoB &ITLA2B) : CIccMBB(ITLA2B)
{
  m_nReservedWord = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagLutAtoB::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITLA2B = The CIccTagLutAtoB object to be copied
 *****************************************************************************
 */
CIccTagLutAtoB &CIccTagLutAtoB::operator=(const CIccTagLutAtoB &ITLA2B)
{
  if (&ITLA2B == this)
    return *this;

  CIccMBB::operator=(ITLA2B);

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagLutAtoB::~CIccTagLutAtoB
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagLutAtoB::~CIccTagLutAtoB()
{
}


/**
 ****************************************************************************
 * Name: CIccTagLutAtoB::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagLutAtoB::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt32Number Offset[5];
  icUInt8Number nCurves, i;
  size_t nPos;

  if (size<8*sizeof(icUInt32Number) || !pIO) {
    return false;
  }

  size_t nStart = pIO->Tell();
  size_t nEnd = nStart + size;

  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read8(&m_nInput) ||
      !pIO->Read8(&m_nOutput) ||
      !pIO->Read16(&m_nReservedWord) ||
      pIO->Read32(Offset, 5)!=5)
    return false;

  if (sig!=GetType())
    return false;

  // must have at least 1 input and 1 output
  if (m_nInput < 1 || m_nOutput < 1)
    return false;

  //B Curves
  if (Offset[0]) {
    nCurves = IsInputB() ? m_nInput : m_nOutput;
    LPIccCurve *pCurves = NewCurvesB();

    if (pIO->Seek(nStart + Offset[0], icSeekSet)<0)
      return false;

    for (i=0; i<nCurves; i++) {
      nPos = pIO->Tell();

      if (!pIO->Read32(&sig))
        return false;

      if (pIO->Seek(nPos, icSeekSet)<0)
        return false;

      if (sig!=icSigCurveType &&
          sig!=icSigParametricCurveType)
        return false;

      pCurves[i] = (CIccCurve*)CIccTag::Create(sig);

      if (!pCurves[i]->Read( (icUInt32Number)(nEnd - pIO->Tell()), pIO))
        return false;

      if (!pIO->Sync32(Offset[0]))
        return false;
    }
  }

  //Matrix
  if (Offset[1]) {
    icS15Fixed16Number tmp;

    if (Offset[1] + 12*sizeof(icS15Fixed16Number) >size)
      return false;

    m_Matrix = new CIccMatrix();

    if (pIO->Seek(nStart + Offset[1], icSeekSet)<0)
      return false;

    for (i=0; i<12; i++) {
       if (pIO->Read32(&tmp, 1)!=1)
        return false;
      m_Matrix->m_e[i] = icFtoD(tmp);
    }
  }


  //M Curves
  if (Offset[2]) {
    nCurves = IsInputMatrix() ? m_nInput : m_nOutput;
    LPIccCurve *pCurves = NewCurvesM();

    if (pIO->Seek(nStart + Offset[2], icSeekSet)<0)
      return false;

    for (i=0; i<nCurves; i++) {
      nPos = pIO->Tell();

      if (!pIO->Read32(&sig))
        return false;

      if (pIO->Seek(nPos, icSeekSet)<0)
        return false;

      if (sig!=icSigCurveType &&
          sig!=icSigParametricCurveType)
        return false;

      pCurves[i] = (CIccCurve*)CIccTag::Create(sig);

      if (!pCurves[i]->Read( (icUInt32Number)(nEnd - pIO->Tell()), pIO))
        return false;

      if (!pIO->Sync32(Offset[2]))
        return false;
    }
  }

  //CLUT
  if (Offset[3]) {
    if (pIO->Seek(nStart + Offset[3], icSeekSet)<0)
      return false;

    m_CLUT = new CIccCLUT(m_nInput, m_nOutput);

    if (!m_CLUT->Read( (icUInt32Number)(nEnd - pIO->Tell()), pIO))
      return false;
  }

  //A Curves
  if (Offset[4]) {
    nCurves = !IsInputB() ? m_nInput : m_nOutput;
    LPIccCurve *pCurves = NewCurvesA();

    if (pIO->Seek(nStart + Offset[4], icSeekSet)<0)
      return false;

    for (i=0; i<nCurves; i++) {
      nPos = pIO->Tell();

      if (!pIO->Read32(&sig))
        return false;

      if (pIO->Seek(nPos, icSeekSet)<0)
        return false;

      if (sig!=icSigCurveType &&
          sig!=icSigParametricCurveType)
        return false;

      pCurves[i] = (CIccCurve*)CIccTag::Create(sig);

      if (!pCurves[i]->Read( (icUInt32Number)(nEnd - pIO->Tell()), pIO))
        return false;

      if (!pIO->Sync32(Offset[4]))
        return false;
    }
  }
  return true;
}



/**
 ****************************************************************************
 * Name: CIccTagLutAtoB::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagLutAtoB::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt32Number Offset[5];
  icUInt8Number nCurves, i;

  size_t nStart = pIO->Tell();
  memset(&Offset[0], 0, sizeof(Offset));
  
  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write8(&m_nInput) ||
      !pIO->Write8(&m_nOutput) ||
      !pIO->Write16(&m_nReservedWord))
    return false;

  size_t nOffsetPos = pIO->Tell();
  if (pIO->Write32(Offset, 5)!=5)
    return false;

  //B Curves
  if (m_CurvesB) {
    Offset[0] = (icUInt32Number)(pIO->Tell() - nStart);
    nCurves = IsInputB() ? m_nInput : m_nOutput;

    for (i=0; i<nCurves; i++) {
      if (!m_CurvesB[i])
        return false;

      if (!m_CurvesB[i]->Write(pIO))
        return false;

      if (!pIO->Align32())
        return false;
    }
  }

  //Matrix
  if (m_Matrix) {
    icS15Fixed16Number tmp;

    Offset[1] = (icUInt32Number)(pIO->Tell() - nStart);

    for (i=0; i<12; i++) {
      tmp = icDtoF(m_Matrix->m_e[i]);
      if (pIO->Write32(&tmp, 1)!=1)
        return false;
    }
  }


  //M Curves
  if (m_CurvesM) {
    Offset[2] = (icUInt32Number)(pIO->Tell() - nStart);
    nCurves = IsInputMatrix() ? m_nInput : m_nOutput;

    for (i=0; i<nCurves; i++) {
      if (!m_CurvesM[i])
        return false;

      if (!m_CurvesM[i]->Write(pIO))
        return false;

      if (!pIO->Align32())
        return false;
    }
  }

  //CLUT
  if (m_CLUT) {
    Offset[3] = (icUInt32Number)(pIO->Tell() - nStart);

    if (!m_CLUT->Write(pIO))
      return false;

    if (!pIO->Align32())
      return false;
  }

  //A Curves
  if (m_CurvesA) {
    Offset[4] = (icUInt32Number)(pIO->Tell() - nStart);
    nCurves = !IsInputB() ? m_nInput : m_nOutput;

    for (i=0; i<nCurves; i++) {
      if (!m_CurvesA[i])
        return false;

      if (!m_CurvesA[i]->Write(pIO))
        return false;

      if (!pIO->Align32())
        return false;
    }
  }

  size_t nEnd = pIO->Tell();

  if (!pIO->Seek(nOffsetPos, icSeekSet))
    return false;

  if (pIO->Write32(&Offset[0], 5)!=5)
    return false;

  return pIO->Seek(nEnd, icSeekSet)>=0;
}


/**
******************************************************************************
* Name: CIccTagLutAtoB::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagLutAtoB::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccMBB::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (!pProfile) {
    return rv;
  }

  switch(sig) {
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
    {
//      icUInt32Number nInput = icGetSpaceSamples(pProfile->m_Header.colorSpace);
//      icUInt32Number nOutput = icGetSpaceSamples(pProfile->m_Header.pcs);
// m_nInput should match nInput, and m_nOutput should match nOutput
// That is validated in CIccMBB::Validate
// Here we don't want to crash while validating the curves, even if the count of them is incorrect, so we use the same counts obtained from reading the LUT.

      icUInt8Number i;
      if (m_CurvesB) {
        icUInt32Number nCurves = IsInputB() ? m_nInput : m_nOutput;
        for (i=0; i<nCurves; i++) {
          if (m_CurvesB[i]) {           // crash on i = 1, m_CurvesB only allocated a single channel
            rv = icMaxStatus(rv, m_CurvesB[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of B-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
      }

      if (m_CurvesM) {
        icUInt32Number nCurves = IsInputMatrix() ? m_nInput : m_nOutput;
        for (i=0; i<nCurves; i++) {
          if (m_CurvesM[i]) {
            rv = icMaxStatus(rv, m_CurvesM[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of M-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
      }

      if (m_CurvesA) {
        if (!m_CLUT) {
          sReport += icMsgValidateNonCompliant;
          sReport += sSigPathName;
          sReport += " - CLUT must be present if using A-curves.\n";

          rv = icMaxStatus(rv, icValidateNonCompliant);
        }

        icUInt32Number nCurves = !IsInputB() ? m_nInput : m_nOutput;
        for (i=0; i<nCurves; i++) {
          if (m_CurvesA[i]) {
            rv = icMaxStatus(rv, m_CurvesA[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of A-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }

      }

      break;
    }
  default:
    {
    }
  }


  return rv;
}

/**
 ****************************************************************************
 * Name: CIccTagLutBtoA::CIccTagLutBtoA
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagLutBtoA::CIccTagLutBtoA()
{
  m_bInputMatrix = true;
}


/**
 ****************************************************************************
 * Name: CIccTagLutBtoA::CIccTagLutBtoA
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITLB2A = The CIccTagLutBtoA object to be copied
 *****************************************************************************
 */
CIccTagLutBtoA::CIccTagLutBtoA(const CIccTagLutBtoA &ITLB2A) : CIccTagLutAtoB(ITLB2A)
{
}


/**
 ****************************************************************************
 * Name: CIccTagLutBtoA::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITLB2A = The CIccTagLutBtoA object to be copied
 *****************************************************************************
 */
CIccTagLutBtoA &CIccTagLutBtoA::operator=(const CIccTagLutBtoA &ITLB2A)
{
  if (&ITLB2A == this)
    return *this;

  CIccMBB::operator=(ITLB2A);

  return *this;
}


/**
******************************************************************************
* Name: CIccTagLutBtoA::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagLutBtoA::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccMBB::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (!pProfile) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Tag validation incomplete: Pointer to profile unavailable.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
    return rv;
  }

  switch(sig) {
  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
  case icSigGamutTag:
    {
//      icUInt32Number nInput = icGetSpaceSamples(pProfile->m_Header.colorSpace);
//      icUInt32Number nOutput = icGetSpaceSamples(pProfile->m_Header.pcs);
// m_nInput should match nInput, and m_nOutput should match nOutput
// That is validated in CIccMBB::Validate
// Here we don't want to crash while validating the curves, even if the count of them is incorrect, so we use the same counts obtained from reading the LUT.

      icUInt32Number nInput = m_nInput;
      icUInt32Number nOutput = m_nOutput;

      if (sig==icSigGamutTag) {
        nOutput = 1;
      }

      icUInt8Number i;
      if (m_CurvesB) {
        for (i=0; i<nInput; i++) {
          if (m_CurvesB[i]) {
            rv = icMaxStatus(rv, m_CurvesB[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of B-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
      }

      if (m_CurvesM) {
        for (i=0; i<nInput; i++) {
          if (m_CurvesM[i]) {
            rv = icMaxStatus(rv, m_CurvesM[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of M-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
      }

      if (m_CurvesA) {
        if (!m_CLUT) {
          sReport += icMsgValidateNonCompliant;
          sReport += sSigPathName;
          sReport += " - CLUT must be present if using A-curves.\n";

          rv = icMaxStatus(rv, icValidateNonCompliant);
        }

        for (i=0; i<nOutput; i++) {
          if (m_CurvesA[i]) {
            rv = icMaxStatus(rv, m_CurvesA[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of A-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }

      }

      break;
    }
  default:
    {
    }
  }


  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::CIccTagLut8
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagLut8::CIccTagLut8()
{
  memset(m_XYZMatrix, 0, sizeof(m_XYZMatrix));
  m_XYZMatrix[0] = m_XYZMatrix[4] = m_XYZMatrix[8] = icDtoF(1.0);
  m_nReservedByte = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::CIccTagLut8
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITL = The CIccTagLut8 object to be copied
 *****************************************************************************
 */
CIccTagLut8::CIccTagLut8(const CIccTagLut8& ITL) : CIccMBB(ITL)
{
  memcpy(&m_XYZMatrix, &ITL.m_XYZMatrix, sizeof(m_XYZMatrix));
  m_nReservedByte = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITL = The CIccTagLut8 object to be copied
 *****************************************************************************
 */
CIccTagLut8 &CIccTagLut8::operator=(const CIccTagLut8 &ITL) 
{
  if (&ITL==this)
    return *this;

  CIccMBB::operator=(ITL);
  memcpy(&m_XYZMatrix, &ITL.m_XYZMatrix, sizeof(m_XYZMatrix));

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::~CIccTagLut8
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagLut8::~CIccTagLut8()
{
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagLut8::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt8Number i, nGrid;
  LPIccCurve *pCurves;
  CIccTagCurve *pCurve;

  if (size<13*sizeof(icUInt32Number) || !pIO) {
    return false;
  }

  size_t nStart = pIO->Tell();
  size_t nEnd = nStart + size;
 
  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read8(&m_nInput) ||
      !pIO->Read8(&m_nOutput) ||
      !pIO->Read8(&nGrid) ||
      !pIO->Read8(&m_nReservedByte) ||
      pIO->Read32(m_XYZMatrix, 9) != 9)
    return false;

  if (sig!=GetType())
    return false;

  // must have at least 1 input and 1 output
  if (m_nInput < 1 || m_nOutput < 1)
    return false;

  //B Curves
  pCurves = NewCurvesB();

  for (i=0; i<m_nInput; i++) {
    if (256 > nEnd - pIO->Tell())
      return false;

    pCurves[i] = pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);

    if (!pCurve->SetSize(256))
      return false;

    if (pIO->ReadUInt8Float(&(*pCurve)[0], 256) != 256)
      return false;                                                                       
  }

  //CLUT
  m_CLUT = new CIccCLUT(m_nInput, m_nOutput);
  if (m_CLUT == NULL)
    return false;

  if (!m_CLUT->Init(nGrid, (icUInt32Number)(nEnd - pIO->Tell()), 1))
    return false;

  if (!m_CLUT->ReadData( (icUInt32Number)(nEnd - pIO->Tell()), pIO, 1))
    return false;

  //A Curves
  pCurves = NewCurvesA();
  if (pCurves == NULL)
    return false;

  for (i=0; i<m_nOutput; i++) {
    if (256 > nEnd - pIO->Tell())
      return false;

    pCurves[i] = pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);

    if (!pCurve->SetSize(256))
      return false;

    if (pIO->ReadUInt8Float(&(*pCurve)[0], 256) != 256)
      return false;                                                                       
  }
  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::SetColorSpaces
 * 
 * Purpose: Sets the input and output color spaces
 * 
 * Args: 
 *  csInput = input color space signature,
 *  csOutput = output color space signature
 * 
 *****************************************************************************
 */
void CIccTagLut8::SetColorSpaces(icColorSpaceSignature csInput, icColorSpaceSignature csOutput)
{
  if (csInput==icSigXYZData) {
    int i;

    if (!m_CurvesM && IsInputMatrix()) { //Transfer ownership of curves
      m_CurvesM = m_CurvesB;
      m_CurvesB = NULL;

      LPIccCurve *pCurves = NewCurvesB();
      CIccTagCurve *pCurve;
      for (i=0; i<m_nInput; i++) {
        pCurves[i] = pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
        pCurve->SetSize(0);
      }

      m_bUseMCurvesAsBCurves = true;
    }
  
    if (!m_Matrix) {
      CIccMatrix *pMatrix = NewMatrix();
      for (i=0; i<9; i++) {
        pMatrix->m_e[i] = icFtoD(m_XYZMatrix[i]);
      }
 
      pMatrix->m_bUseConstants=false;
    }
  }
  else {
    m_XYZMatrix[0] = m_XYZMatrix[4] = m_XYZMatrix[8] = icDtoF(1.0);
    m_XYZMatrix[1] = m_XYZMatrix[2] = m_XYZMatrix[3] =
    m_XYZMatrix[5] = m_XYZMatrix[6] = m_XYZMatrix[7] = 0;
  }

  CIccMBB::SetColorSpaces(csInput, csOutput);
}


/**
 ****************************************************************************
 * Name: CIccTagLut8::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagLut8::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt8Number i, nGrid;
  icS15Fixed16Number XYZMatrix[9];
  //icUInt16Number nInputEntries;
  //icUInt16Number nOutputEntries;
  LPIccCurve *pCurves;
  CIccTagCurve *pCurve;
  icFloat32Number v;

  if (m_Matrix) {
    for (i=0; i<9; i++)
      XYZMatrix[i] = icDtoF(m_Matrix->m_e[i]);
  }
  else {
    memset(XYZMatrix, 0, 9*sizeof(icS15Fixed16Number));
    XYZMatrix[0] = XYZMatrix[4] = XYZMatrix[8] = icDtoF(1.0);
  }

  if (m_bUseMCurvesAsBCurves) {
    pCurves = m_CurvesM;
  }
  else {
    pCurves = m_CurvesB;
  }

  if (!pCurves || !m_CurvesA || !m_CLUT)
    return false;

  nGrid = m_CLUT->GridPoints();

  //nInputEntries = (icUInt16Number)(((CIccTagCurve*)pCurves[0])->GetSize());
  //nOutputEntries = (icUInt16Number)(((CIccTagCurve*)m_CurvesA[0])->GetSize());

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write8(&m_nInput) ||
      !pIO->Write8(&m_nOutput) ||
      !pIO->Write8(&nGrid) ||
      !pIO->Write8(&m_nReservedByte) ||
      pIO->Write32(XYZMatrix, 9) != 9)
    return false;

  //B Curves
  for (i=0; i<m_nInput; i++) {
    if (pCurves[i]->GetType()!=icSigCurveType)
      return false;

    pCurve = (CIccTagCurve*)pCurves[i];
    if (!pCurve)
      return false;

    if (pCurve->GetSize()!=256) {
      icUInt32Number j;

      for (j=0; j<256; j++) {
        v = pCurve->Apply((icFloat32Number)j / 255.0F);
        if (!pIO->WriteUInt8Float(&v, 1))
          return false;
      }
    }
    else {
      if (pIO->WriteUInt8Float(&(*pCurve)[0], 256)!=256)
        return false;
    }
  }

  //CLUT
  if (!m_CLUT->WriteData(pIO, 1))
    return false;

  //A Curves
  pCurves = m_CurvesA;

  for (i=0; i<m_nOutput; i++) {
    if (pCurves[i]->GetType()!=icSigCurveType)
      return false;

    pCurve = (CIccTagCurve*)pCurves[i];

    if (!pCurve)
      return false;

    if (pCurve->GetSize()!=256) {
      icUInt32Number j;

      for (j=0; j<256; j++) {
        v = pCurve->Apply((icFloat32Number)j / 255.0F);
        if (!pIO->WriteUInt8Float(&v, 1))
          return false;
      }
    }
    else {
      if (pIO->WriteUInt8Float(&(*pCurve)[0], 256)!=256)
        return false;
    }
  }
  return true;
}


/**
******************************************************************************
* Name: CIccTagLut8::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagLut8::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccMBB::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (!pProfile) {
    return rv;
  }

  switch(sig) {
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
  case icSigGamutTag:
    {
//      icUInt32Number nInput = icGetSpaceSamples(pProfile->m_Header.colorSpace);
//      icUInt32Number nOutput = icGetSpaceSamples(pProfile->m_Header.pcs);
// m_nInput should match nInput, and m_nOutput should match nOutput
// That is validated in CIccMBB::Validate
// Here we don't want to crash while validating the curves, even if the count of them is incorrect, so we use the same counts obtained from reading the LUT.

      icUInt32Number nInput = m_nInput;
      icUInt32Number nOutput = m_nOutput;

      if (sig==icSigGamutTag) {
        nOutput = 1;
      }

      if (m_CurvesB) {
        for (icUInt32Number i=0; i<nInput; i++) {
          if (m_CurvesB[i]) {
            rv = icMaxStatus(rv, m_CurvesB[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
            if (m_CurvesB[i]->GetType()==icSigCurveType) {
              CIccTagCurve *pTagCurve = (CIccTagCurve*)m_CurvesB[i];
              if (pTagCurve->GetSize()==1) {
                sReport += icMsgValidateCriticalError;
                sReport += sSigPathName;
                sReport += " - lut8Tags do not support single entry gamma curves.\n";
                rv = icMaxStatus(rv, icValidateCriticalError);
              }
            }
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of B-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
      }

      if (m_Matrix) {
        rv = icMaxStatus(rv, m_Matrix->Validate(sigPath + icGetSigPath(GetType()), sReport, pProfile));
      }
      else {
        const int s15dot16Unity = 65536;
        int sum = 0;
        for (icUInt32Number i=0; i<9; i++) {
          sum += m_XYZMatrix[i];
        }
        if (m_XYZMatrix[0]!=s15dot16Unity || m_XYZMatrix[4]!=s15dot16Unity || m_XYZMatrix[8]!=s15dot16Unity || sum!=3*s15dot16Unity) {
          sReport += icMsgValidateWarning;
          sReport += sSigPathName;
          sReport += " - Matrix must be identity.\n";
          rv = icMaxStatus(rv, icValidateWarning);
        }
      }

      if (m_CurvesA) {

        for (icUInt32Number i=0; i<nOutput; i++) {
          if (m_CurvesA[i]) {
            rv = icMaxStatus(rv, m_CurvesA[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
            if (m_CurvesA[i]->GetType()==icSigCurveType) {
              CIccTagCurve *pTagCurve = (CIccTagCurve*)m_CurvesA[i];
              if (pTagCurve->GetSize()==1) {
                sReport += icMsgValidateCriticalError;
                sReport += sSigPathName;
                sReport += " - lut8Tags do not support single entry gamma curves.\n";
                rv = icMaxStatus(rv, icValidateCriticalError);
              }
            }
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of A-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }

      }

      break;
    }
  default:
    {
    }
  }


  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::CIccTagLut16
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagLut16::CIccTagLut16()
{
  memset(m_XYZMatrix, 0, sizeof(m_XYZMatrix));
  m_XYZMatrix[0] = m_XYZMatrix[4] = m_XYZMatrix[8] = icDtoF(1.0);
  m_nReservedByte = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::CIccTagLut16
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITL = The CIccTagUnknown object to be copied
 *****************************************************************************
 */
CIccTagLut16::CIccTagLut16(const CIccTagLut16& ITL) : CIccMBB(ITL)
{
  memcpy(&m_XYZMatrix, &ITL.m_XYZMatrix, sizeof(m_XYZMatrix));
  m_nReservedByte = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITL = The CIccTagLut16 object to be copied
 *****************************************************************************
 */
CIccTagLut16 &CIccTagLut16::operator=(const CIccTagLut16 &ITL) 
{
  if (&ITL==this)
    return *this;

  CIccMBB::operator=(ITL);
  memcpy(&m_XYZMatrix, &ITL.m_XYZMatrix, sizeof(m_XYZMatrix));

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::~CIccTagLut16
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagLut16::~CIccTagLut16()
{
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagLut16::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt8Number i, nGrid;
  icUInt16Number nInputEntries, nOutputEntries;
  LPIccCurve *pCurves;
  CIccTagCurve *pCurve;

  if (size < 13*sizeof(icUInt32Number) || !pIO) {
    return false;
  }

  size_t nStart = pIO->Tell();
  size_t nEnd = nStart + size;
 
  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read8(&m_nInput) ||
      !pIO->Read8(&m_nOutput) ||
      !pIO->Read8(&nGrid) ||
      !pIO->Read8(&m_nReservedByte) ||
      pIO->Read32(m_XYZMatrix, 9) != 9 ||
      !pIO->Read16(&nInputEntries) ||
      !pIO->Read16(&nOutputEntries))
    return false;

  if (sig!=GetType())
    return false;

  // must have at least 1 input and 1 output
  if (m_nInput < 1 || m_nOutput < 1)
    return false;

  //B Curves
  pCurves = NewCurvesB();

  for (i=0; i<m_nInput; i++) {
    if (nInputEntries*sizeof(icUInt16Number) > nEnd - pIO->Tell())
      return false;

    pCurves[i] = pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);

    if (!pCurve->SetSize(nInputEntries))
      return false;

    if ((nInputEntries > 0) && pIO->ReadUInt16Float(&(*pCurve)[0], nInputEntries) != nInputEntries)
      return false;
  }

  //CLUT
  m_CLUT = new CIccCLUT(m_nInput, m_nOutput);

  if (!m_CLUT->Init(nGrid, (icUInt32Number)(nEnd - pIO->Tell()), 2))
    return false;

  if (!m_CLUT->ReadData( (icUInt32Number)(nEnd - pIO->Tell()), pIO, 2))
    return false;

  //A Curves
  pCurves = NewCurvesA();

  for (i=0; i<m_nOutput; i++) {
    if (nOutputEntries*sizeof(icUInt16Number) > nEnd - pIO->Tell())
      return false;

    pCurves[i] = pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);

    if (!pCurve->SetSize(nOutputEntries))
      return false;

    if ((nOutputEntries > 0) && pIO->ReadUInt16Float(&(*pCurve)[0], nOutputEntries) != nOutputEntries)
      return false;
  }
  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::SetColorSpaces
 * 
 * Purpose: Sets the input and output color spaces
 * 
 * Args: 
 *  csInput = input color space signature,
 *  csOutput = output color space signature
 * 
 *****************************************************************************
 */
void CIccTagLut16::SetColorSpaces(icColorSpaceSignature csInput, icColorSpaceSignature csOutput)
{
  if (csInput==icSigXYZData) {
    int i;

    if (!m_CurvesM && IsInputMatrix()) { //Transfer ownership of curves
      m_CurvesM = m_CurvesB;
      m_CurvesB = NULL;

      LPIccCurve *pCurves = NewCurvesB();
      CIccTagCurve *pCurve;
      for (i=0; i<m_nInput; i++) {
        pCurves[i] = pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
        pCurve->SetSize(0);
      }

      m_bUseMCurvesAsBCurves = true;
    }

    if (!m_Matrix) {
      CIccMatrix *pMatrix = NewMatrix();
      for (i=0; i<9; i++) {
        pMatrix->m_e[i] = icFtoD(m_XYZMatrix[i]);
      }

      pMatrix->m_bUseConstants=false;
    }
  }
  else {
    m_XYZMatrix[0] = m_XYZMatrix[4] = m_XYZMatrix[8] = icDtoF(1.0);
    m_XYZMatrix[1] = m_XYZMatrix[2] = m_XYZMatrix[3] =
    m_XYZMatrix[5] = m_XYZMatrix[6] = m_XYZMatrix[7] = 0;
  }

  CIccMBB::SetColorSpaces(csInput, csOutput);
}


/**
 ****************************************************************************
 * Name: CIccTagLut16::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagLut16::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt8Number i, nGrid;
  icS15Fixed16Number XYZMatrix[9];
  icUInt16Number nInputEntries, nOutputEntries;
  LPIccCurve *pCurves;
  CIccTagCurve *pCurve;

  if (m_Matrix) {
    for (i=0; i<9; i++) {
      XYZMatrix[i] = icDtoF(m_Matrix->m_e[i]);
    }
  }
  else {
    memset(XYZMatrix, 0, 9*sizeof(icS15Fixed16Number));
    XYZMatrix[0] = XYZMatrix[4] = XYZMatrix[8] = icDtoF(1.0);
  }

  if (m_bUseMCurvesAsBCurves) {
    pCurves = m_CurvesM;
  }
  else {
    pCurves = m_CurvesB;
  }

  if (!pCurves || !m_CurvesA || !m_CLUT)
    return false;

  nGrid = m_CLUT->GridPoints();

  nInputEntries = (icUInt16Number)(((CIccTagCurve*)pCurves[0])->GetSize());
  nOutputEntries = (icUInt16Number)(((CIccTagCurve*)m_CurvesA[0])->GetSize());

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write8(&m_nInput) ||
      !pIO->Write8(&m_nOutput) ||
      !pIO->Write8(&nGrid) ||
      !pIO->Write8(&m_nReservedByte) ||
      pIO->Write32(XYZMatrix, 9) != 9 ||
      !pIO->Write16(&nInputEntries) ||
      !pIO->Write16(&nOutputEntries))
    return false;

  //B Curves
  for (i=0; i<m_nInput; i++) {
    if (pCurves[i]->GetType()!=icSigCurveType)
      return false;

    pCurve = (CIccTagCurve*)pCurves[i];
    if (!pCurve)
      return false;

    if (pIO->WriteUInt16Float(&(*pCurve)[0], nInputEntries) != nInputEntries)
      return false;
  }

  //CLUT
  if (!m_CLUT->WriteData(pIO, 2))
    return false;

  //A Curves
  pCurves = m_CurvesA;

  for (i=0; i<m_nOutput; i++) {
    if (pCurves[i]->GetType()!=icSigCurveType)
      return false;

    pCurve = (CIccTagCurve*)pCurves[i];

    if (pIO->WriteUInt16Float(&(*pCurve)[0], nOutputEntries) != nOutputEntries)
      return false;
  }
  return true;
}


/**
******************************************************************************
* Name: CIccTagLut16::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagLut16::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccMBB::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (!pProfile) {
    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }

  switch(sig) {
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
  case icSigGamutTag:
    {
//      icUInt32Number nInput = icGetSpaceSamples(pProfile->m_Header.colorSpace);
//      icUInt32Number nOutput = icGetSpaceSamples(pProfile->m_Header.pcs);
// m_nInput should match nInput, and m_nOutput should match nOutput
// That is validated in CIccMBB::Validate
// Here we don't want to crash while validating the curves, even if the count of them is incorrect, so we use the same counts obtained from reading the LUT.

      icUInt32Number nInput = m_nInput;
      icUInt32Number nOutput = m_nOutput;

      if (sig==icSigGamutTag) {
        nOutput = 1;
      }

      if (m_CurvesB) {
        for (icUInt32Number i=0; i<nInput; i++) {
          if (m_CurvesB[i]) {
            rv = icMaxStatus(rv, m_CurvesB[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
            if (m_CurvesB[i]->GetType()==icSigCurveType) {
              CIccTagCurve *pTagCurve = (CIccTagCurve*)m_CurvesB[i];
              if (pTagCurve->GetSize()==1) {
                sReport += icMsgValidateCriticalError;
                sReport += sSigPathName;
                sReport += " - lut16Tags do not support single entry gamma curves.\n";
                rv = icMaxStatus(rv, icValidateCriticalError);
              }
            }
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of B-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
      }

      if (m_Matrix) {
        rv = icMaxStatus(rv, m_Matrix->Validate(sigPath + icGetSigPath(GetType()), sReport, pProfile));
      }
      else {
        const int s15dot16Unity = 65536;
        int sum = 0;
        for (icUInt32Number i=0; i<9; i++) {
          sum += m_XYZMatrix[i];
        }
        if (m_XYZMatrix[0]!=s15dot16Unity || m_XYZMatrix[4]!=s15dot16Unity || m_XYZMatrix[8]!=s15dot16Unity || sum!=3*s15dot16Unity) {
          sReport += icMsgValidateWarning;
          sReport += sSigPathName;
          sReport += " - Matrix must be identity.\n";
          rv = icMaxStatus(rv, icValidateWarning);
        }
      }

      if (m_CurvesA) {

        for (icUInt32Number i=0; i<nOutput; i++) {
          if (m_CurvesA[i]) {
            rv = icMaxStatus(rv, m_CurvesA[i]->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
            if (m_CurvesA[i]->GetType()==icSigCurveType) {
              CIccTagCurve *pTagCurve = (CIccTagCurve*)m_CurvesA[i];
              if (pTagCurve->GetSize()==1) {
                sReport += icMsgValidateCriticalError;
                sReport += sSigPathName;
                sReport += " - lut16Tags do not support single entry gamma curves.\n";
                rv = icMaxStatus(rv, icValidateCriticalError);
              }
            }
          }
          else {
            sReport += icMsgValidateCriticalError;
            sReport += sSigPathName;
            sReport += " - Incorrect number of A-curves.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }

      }

      break;
    }
  default:
    {
    }
  }


  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::CIccTagGamutBoundaryDescription
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */	
CIccTagGamutBoundaryDesc::CIccTagGamutBoundaryDesc()
{	
	m_NumberOfVertices = 0;
	m_NumberOfTriangles = 0;
	m_nPCSChannels = 0;
	m_nDeviceChannels = 0;
	m_PCSValues = NULL;
	m_DeviceValues = NULL;
	m_Triangles = NULL;
	
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::CIccTagGamutBoundaryDescription
 * 
 * Purpose: Constructor
 * 
 * Args: 
 *		nInputChannels = the number of PCS values, typically 3
 *		numberOfVertices = the number of vertices in the gamut boundary
 *		numberOfTriangles = the number of triangles that make up the gamut boundary
 *		nOutputChannels = the number of device values, can be zero if none are included
 *
 *****************************************************************************
 */		

CIccTagGamutBoundaryDesc::CIccTagGamutBoundaryDesc(icUInt8Number nInputChannels,icInt32Number numberOfVertices, icInt32Number numberOfTriangles, icUInt8Number nOutputChannels)
{
	
	m_NumberOfVertices = numberOfVertices;
	m_NumberOfTriangles = numberOfTriangles;
	m_nPCSChannels = nInputChannels;
	m_nDeviceChannels = nOutputChannels;
	m_PCSValues = new icFloatNumber[nInputChannels*m_NumberOfVertices];
	if (nOutputChannels > 0)
	{
		m_DeviceValues = new icFloatNumber[nOutputChannels*m_NumberOfVertices];
	}
	else
	{
		m_DeviceValues = NULL;
	}
	m_Triangles = new icGamutBoundaryTriangle[m_NumberOfTriangles];
	
	
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::CIccTagGamutBoundaryDescription
 * 
 * Purpose: Constructor
 *
 * Args:
 *	InGamutBoundaryTag = the CIccTagGamutBoundaryDescription object to copy
 * 
 *****************************************************************************
 */	
CIccTagGamutBoundaryDesc::CIccTagGamutBoundaryDesc(const CIccTagGamutBoundaryDesc &InGamutBoundaryTag)
{
	m_NumberOfVertices = InGamutBoundaryTag.m_NumberOfVertices;
	m_NumberOfTriangles = InGamutBoundaryTag.m_NumberOfTriangles;
	m_nPCSChannels = InGamutBoundaryTag.m_nPCSChannels;
	m_nDeviceChannels = InGamutBoundaryTag.m_nDeviceChannels;
	
	m_PCSValues = new icFloatNumber[m_nPCSChannels*m_NumberOfVertices];
	
	if (m_nDeviceChannels > 0)
	{
		m_DeviceValues = new icFloatNumber[m_nDeviceChannels*m_NumberOfVertices];
	}
	else
	{
		m_DeviceValues = NULL;
	}
	m_Triangles = new icGamutBoundaryTriangle[m_NumberOfTriangles];
	
	memcpy(m_PCSValues,InGamutBoundaryTag.m_PCSValues,m_nPCSChannels*m_NumberOfVertices*sizeof(icFloatNumber));
	if (m_DeviceValues)
		memcpy(m_DeviceValues,InGamutBoundaryTag.m_DeviceValues,m_nDeviceChannels*m_NumberOfVertices*sizeof(icFloatNumber));
	memcpy(m_Triangles,InGamutBoundaryTag.m_Triangles,m_NumberOfTriangles*sizeof(icGamutBoundaryTriangle));
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::operator=
 * 
 * Purpose: Constructor
 *
 * Args:
 *	InGamutBoundaryTag = the CIccTagGamutBoundaryDescription object to copy
 * 
 ******/	

CIccTagGamutBoundaryDesc &CIccTagGamutBoundaryDesc::operator=(const CIccTagGamutBoundaryDesc& InGamutBoundaryTag)
{
	if (&InGamutBoundaryTag == this)
		return *this;
	
	m_NumberOfVertices = InGamutBoundaryTag.m_NumberOfVertices;
	m_NumberOfTriangles = InGamutBoundaryTag.m_NumberOfTriangles;
	m_nPCSChannels = InGamutBoundaryTag.m_nPCSChannels;
	m_nDeviceChannels = InGamutBoundaryTag.m_nDeviceChannels;
	
	m_PCSValues = new icFloatNumber[m_nPCSChannels*m_NumberOfVertices];
	
	if (m_nDeviceChannels > 0)
	{
		m_DeviceValues = new icFloatNumber[m_nDeviceChannels*m_NumberOfVertices];
	}
	else
	{
		m_DeviceValues = NULL;
	}
	m_Triangles = new icGamutBoundaryTriangle[m_NumberOfTriangles];
	
	memcpy(m_PCSValues,InGamutBoundaryTag.m_PCSValues,m_nPCSChannels*m_NumberOfVertices*sizeof(icFloatNumber));
	if (m_DeviceValues)
		memcpy(m_DeviceValues,InGamutBoundaryTag.m_DeviceValues,m_nDeviceChannels*m_NumberOfVertices*sizeof(icFloatNumber));
	memcpy(m_Triangles,InGamutBoundaryTag.m_Triangles,m_NumberOfTriangles*sizeof(icGamutBoundaryTriangle));	
	
	return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::CIccTagGamutBoundaryDescription
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */	
CIccTagGamutBoundaryDesc::~CIccTagGamutBoundaryDesc()
{	
	if (m_PCSValues)
		delete [] m_PCSValues;
	
	if (m_DeviceValues)
		delete [] m_DeviceValues;
	
	if (m_Triangles)
		delete [] m_Triangles;		
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::Read
 * 
 * Purpose: Read in an gamut boundary tag type into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */	
bool CIccTagGamutBoundaryDesc::Read(icUInt32Number size, CIccIO *pIO)
{
	icTagTypeSignature sig;
	
	if (sizeof(icTagTypeSignature) + 
		sizeof(icUInt32Number)*3 +
		sizeof(icUInt16Number)*2 > size)
		return false;
	
	if (!pIO) {
		return false;
	}
	
	if (!pIO->Read32(&sig) ||
		!pIO->Read32(&m_nReserved))
		return false;
	
	if (!pIO->Read16(&m_nPCSChannels) ||
		!pIO->Read16(&m_nDeviceChannels))
		return false;
	
	if (!pIO->Read32(&m_NumberOfVertices) ||
		!pIO->Read32(&m_NumberOfTriangles))
		return false;	

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number)*3 +
      sizeof(icUInt16Number)*2 + 
      (icUInt64Number)m_NumberOfVertices*3*sizeof(icUInt32Number) +
      (icUInt64Number)m_NumberOfVertices*m_nPCSChannels*sizeof(icFloat32Number) +
      (icUInt64Number)m_NumberOfVertices*m_nDeviceChannels*sizeof(icFloat32Number) > size)
    return false;
	
	if (m_PCSValues)
		delete [] m_PCSValues;
	if (m_DeviceValues)
		delete [] m_DeviceValues;
	if (m_Triangles)
		delete [] m_Triangles;	
	
	m_PCSValues = new icFloatNumber[m_nPCSChannels*m_NumberOfVertices];

  if (!m_PCSValues)
    return false;
	
	if (m_nDeviceChannels > 0)
	{
		m_DeviceValues = new icFloatNumber[m_nDeviceChannels*m_NumberOfVertices];

    if (!m_DeviceValues)
      return false;
	}
	else
	{
		m_DeviceValues = NULL;
	}
	m_Triangles = new icGamutBoundaryTriangle[m_NumberOfTriangles];
	
	icUInt32Number nNum32 = m_NumberOfTriangles*3;
	
	if (pIO->Read32(m_Triangles, nNum32)!=nNum32)
		return false;		
	
	nNum32 = m_nPCSChannels*m_NumberOfVertices;
	
	if (pIO->ReadFloat32Float(m_PCSValues, nNum32)!=nNum32)
		return false;
	
	if (m_nDeviceChannels > 0)
	{
		nNum32 = m_nDeviceChannels*m_NumberOfVertices;
		
		if (pIO->ReadFloat32Float(m_DeviceValues, nNum32)!=nNum32)
			return false;
	}	
	
	return true;
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::Write
 * 
 * Purpose: Write a gamut boundary type tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */	
bool CIccTagGamutBoundaryDesc::Write(CIccIO *pIO)
{
	
	icTagTypeSignature sig = GetType();
	
	if (!pIO) {
		return false;
	}
	
	//icUInt32Number startPos = pIO->GetLength();   // unused, and no side effets
	
	if (!pIO->Write32(&sig) ||
		!pIO->Write32(&m_nReserved))
		return false;
	
	if (!pIO->Write16(&m_nPCSChannels) ||
		!pIO->Write16(&m_nDeviceChannels))
		return false;
	
	if (!pIO->Write32(&m_NumberOfVertices) ||
		!pIO->Write32(&m_NumberOfTriangles))
		return false;	
	
	icUInt32Number nNum32 = m_NumberOfTriangles*3;
	
	if (pIO->Write32(m_Triangles, nNum32)!=nNum32)
		return false;	
	
	
	nNum32 = m_nPCSChannels*m_NumberOfVertices;
	
	if (pIO->WriteFloat32Float(m_PCSValues, nNum32)!=nNum32)
		return false;
	
	if (m_nDeviceChannels > 0)
	{
		nNum32 = m_nDeviceChannels*m_NumberOfVertices;
		
		if (pIO->WriteFloat32Float(m_DeviceValues, nNum32)!=nNum32)
			return false;
	}
	
	return true;
	
}


/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::Describe
 * 
 * Purpose: Dump data associated with gamut boundary tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */	
void CIccTagGamutBoundaryDesc::Describe(std::string &sDescription, int nVerboseness)
{
    const size_t bufSize = 256;
	icChar buf[bufSize];
	
    snprintf(buf, bufSize, "Number Of Vertices = %d, Number of Triangles = %d\n",m_NumberOfVertices,m_NumberOfTriangles);
	sDescription += buf;
	
	snprintf(buf,bufSize, "Number Of Inputs = %d, Number of Outputs = %d\n",m_nPCSChannels,m_nDeviceChannels);
	sDescription += buf;
	
    if (nVerboseness > 75) {
	    int c = 0;
	    int d = 0;
	    for (int i=0; i<m_NumberOfVertices; i++)
	    {
            snprintf(buf,bufSize, "V = %d:\t",i);
		    sDescription += buf;
		    for (int j=0; j<m_nPCSChannels; j++)
		    {
			    snprintf(buf,bufSize, "%.4lf\t",m_PCSValues[c++]);
			    sDescription += buf;
		    }
		    if (m_nDeviceChannels > 0)
		    {
			    snprintf(buf,bufSize, ":\t");
			    sDescription += buf;
			
			    for (int j=0; j<m_nDeviceChannels; j++)
			    {
				    snprintf(buf,bufSize, "%.4lf\t",m_DeviceValues[d++]);
				    sDescription += buf;
			    }
		    }
		    snprintf(buf,bufSize, "\n");
		    sDescription += buf;
	    }
	
	    for (int i=0; i<m_NumberOfTriangles; i++)
	    {
            snprintf(buf,bufSize, "V1 = %u\tV2 = %u\tV3 = %u\n",m_Triangles[i].m_VertexNumbers[0],m_Triangles[i].m_VertexNumbers[1],m_Triangles[i].m_VertexNumbers[2]);
		        sDescription += buf;
	    }
    }
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::setVertex
 * 
 * Purpose: Dump data associated with gamut boundary tag to a string
 * 
 * Args: 
 *  vertexNumber = the number of the vertex to set
 *	pcsCoords = the pcs coordinates of the vertex
 *	deviceCoords = the device coordinates of the vertex
 *
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */		
bool CIccTagGamutBoundaryDesc::setVertex(icInt32Number vertexNumber,icFloatNumber* pcsCoords,icFloatNumber* deviceCoords)
{
	if ((vertexNumber < 0) || (vertexNumber >= m_NumberOfVertices))
		return false;
	
	memcpy(&m_PCSValues[vertexNumber*m_nPCSChannels],pcsCoords,sizeof(icFloatNumber)*m_nPCSChannels);
	if ((m_DeviceValues) && (deviceCoords))
		memcpy(&m_DeviceValues[vertexNumber*m_nDeviceChannels],deviceCoords,sizeof(icFloatNumber)*m_nDeviceChannels);
	
	return true;
}


/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::getVertexPCSCoord
 * 
 * Purpose: Dump data associated with gamut boundary tag to a string
 * 
 * Args: 
 *  vertexNumber = the number of the vertex to get
 *
 * Return: 
 *  NULL = failure, otherwise a pointer to the PCS coordinate
 *****************************************************************************
 */			
icFloatNumber*	CIccTagGamutBoundaryDesc::getVertexPCSCoord(icInt32Number vertexNumber)
{
	if ((vertexNumber < 0) || (vertexNumber >= m_NumberOfVertices))
		return NULL;
	
	return &m_PCSValues[vertexNumber*m_nPCSChannels];
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::getVertexDeviceCoord
 * 
 * Purpose: Dump data associated with gamut boundaty tag to a string
 * 
 * Args: 
 *  vertexNumber = the number of the vertex to get
 *
 * Return: 
 *  NULL = failure, otherwise a pointer to the device coordinate
 *****************************************************************************
 */		
icFloatNumber*	CIccTagGamutBoundaryDesc::getVertexDeviceCoord(icInt32Number vertexNumber)
{
	if ((vertexNumber < 0) || (vertexNumber >= m_NumberOfVertices))
		return NULL;
	
	if (!m_DeviceValues)
		return NULL;
	
	return &m_DeviceValues[vertexNumber*m_nDeviceChannels];
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::setTriangle
 * 
 * Purpose: Dump data associated with gamut boundary tag to a string
 * 
 * Args: 
 *  triangleNumber = the number of the triangle to set
 *	triangle = the triangle to copy into the gamut boundary
 *
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */		
bool CIccTagGamutBoundaryDesc::setTriangle(icInt32Number triangleNumber,icGamutBoundaryTriangle& triangle)
{
	if ((triangleNumber < 0) || (triangleNumber >= m_NumberOfTriangles))
		return false;
	
	memcpy(&m_Triangles[triangleNumber],&triangle,sizeof(icGamutBoundaryTriangle));
	
	return true;
}

/**
 ****************************************************************************
 * Name: CIccTagGamutBoundaryDescription::getTriangle
 * 
 * Purpose: Dump data associated with gamut boundary tag to a string
 * 
 * Args: 
 *  triangleNumber = the number of the triangle to set
 *	triangle = the triangle to copy from the gamut boundary
 *
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */		

bool CIccTagGamutBoundaryDesc::getTriangle(icInt32Number triangleNumber,icGamutBoundaryTriangle& triangle)	
{
	if ((triangleNumber < 0) || (triangleNumber >= m_NumberOfTriangles))
		return false;
	
	memcpy(&triangle,&m_Triangles[triangleNumber],sizeof(icGamutBoundaryTriangle));
	return true;
}	



/**
 ******************************************************************************
 * Name: CIccTagGamutBoundaryDescription::Validate
 * 
 * Purpose: Check tag data validity.
 * 
 * Args: 
 *  sig = signature of tag being validated,
 *  sReport = String to add report information to
 * 
 * Return: 
 *  icValidateStatusOK if valid, or other error status.
 ******************************************************************************
 */
icValidateStatus CIccTagGamutBoundaryDesc::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
	icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);
	
	CIccInfo Info;
	std::string sSigPathName = Info.GetSigPathName(sigPath);
	
	if ((m_NumberOfVertices == 0) || (m_NumberOfTriangles == 0) || (m_nPCSChannels < 3)) {
		sReport += icMsgValidateWarning;
		sReport += sSigPathName;
		sReport += " - Invalid tag.\n";
		
		rv = icMaxStatus(rv, icValidateWarning);
		return rv;
	}
	
	return rv;
}	



#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
