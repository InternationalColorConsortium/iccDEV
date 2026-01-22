/** @file
    File:       IccMpeSpectral.cpp

    Contains:   Implementation of Basic Multi Processing Elements

    Version:    V1

    Copyright:  � see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2014 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 1-30-2006
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#endif

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
#include "IccIO.h"
#include <map>
#include "IccMatrixMath.h"
#include "IccUtil.h"
#include "IccCAM.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::CIccMpeSpectralMatrix
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralMatrix::CIccMpeSpectralMatrix()
{
  m_nReserved = 0;
  m_nReserved2 = 0;
  m_nInputChannels = m_nOutputChannels = 0;
  m_size = 0;
  m_pMatrix = NULL;
  m_pOffset = NULL;
  m_pWhite = NULL;

  m_Range.start=0;
  m_Range.end=0;
  m_Range.steps=0;

  m_pApplyMtx = NULL;
}


/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::CIccMpeSpectralMatrix
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralMatrix::CIccMpeSpectralMatrix(const CIccMpeSpectralMatrix &matrix)
{
  m_nReserved = matrix.m_nReserved;
  m_nReserved2 = matrix.m_nReserved2;

  m_nInputChannels = matrix.m_nInputChannels;
  m_nOutputChannels = matrix.m_nOutputChannels;

  m_Range = matrix.m_Range;

  m_size = matrix.m_size;
  if(matrix.m_pMatrix) {
    int num = m_size * sizeof(icFloatNumber);
    m_pMatrix = (icFloatNumber*)malloc(num);
    memcpy(m_pMatrix, matrix.m_pMatrix, num);
  }
  else
    m_pMatrix = NULL;

  if (matrix.m_pOffset) {
    int num = m_Range.steps * sizeof(icFloatNumber);
    m_pOffset = (icFloatNumber*)malloc(num);
    memcpy(m_pOffset, matrix.m_pOffset, num);
  }
  else
    m_pOffset = NULL;

  if (matrix.m_pWhite) {
    int num = m_Range.steps * sizeof(icFloatNumber);
    m_pWhite = (icFloatNumber*)malloc(num);
    memcpy(m_pWhite, matrix.m_pWhite, num);
  }
  else
    m_pWhite = NULL;

  m_pApplyMtx = NULL;
}

/**
 ******************************************************************************
 * Name: &CIccMpeSpectralMatrix::operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralMatrix::copyData(const CIccMpeSpectralMatrix &matrix)
{
  m_nReserved = matrix.m_nReserved;
  m_nReserved2 = matrix.m_nReserved2;

  m_nInputChannels = matrix.m_nInputChannels;
  m_nOutputChannels = matrix.m_nOutputChannels;

  m_Range = matrix.m_Range;

  if (m_pMatrix)
    free(m_pMatrix);

  m_size = matrix.m_size;
  if (matrix.m_pMatrix) {
    int num = m_size * sizeof(icFloatNumber);
    m_pMatrix = (icFloatNumber*)malloc(num);
    memcpy(m_pMatrix, matrix.m_pMatrix, num);
  }
  else
    m_pMatrix = NULL;

  if (m_pOffset)
    free(m_pOffset);

  if (matrix.m_pOffset) {
    int num = m_Range.steps * sizeof(icFloatNumber);
    m_pOffset = (icFloatNumber*)malloc(num);
    memcpy(m_pOffset, matrix.m_pOffset, num);
  }
  else
    m_pOffset = NULL;

  if (m_pWhite)
    free(m_pWhite);

  if (matrix.m_pWhite) {
    int num = m_Range.steps * sizeof(icFloatNumber);
    m_pWhite = (icFloatNumber*)malloc(num);
    memcpy(m_pWhite, matrix.m_pWhite, num);
  }
  else
    m_pWhite = NULL;

  m_pApplyMtx = NULL;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::~CIccMpeSpectralMatrix
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralMatrix::~CIccMpeSpectralMatrix()
{
  if (m_pMatrix)
    free(m_pMatrix);

  if (m_pOffset)
    free(m_pOffset);

  if (m_pWhite)
    free(m_pWhite);

  if (m_pApplyMtx)
    delete m_pApplyMtx;
}


/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::SetSize
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeSpectralMatrix::SetSize(icUInt16Number nInputChannels, icUInt16Number nOutputChannels, const icSpectralRange &range)
{
  if (m_pMatrix) {
    free(m_pMatrix);
    m_pMatrix = NULL;
  }

  if (m_pWhite) {
    free(m_pWhite);
    m_pWhite = NULL;
  }

  if (m_pOffset) {
    free(m_pOffset);
    m_pOffset = NULL;
  }

  if (m_pApplyMtx) {
    delete m_pApplyMtx;
    m_pApplyMtx = NULL;
  }

  m_nInputChannels = nInputChannels;
  m_nOutputChannels = nOutputChannels;
  m_Range = range;

  m_size = (icUInt32Number)numVectors() * range.steps;

  m_pMatrix = (icFloatNumber*)calloc(m_size, sizeof(icFloatNumber));
  m_pOffset = (icFloatNumber*)calloc(range.steps, sizeof(icFloatNumber));
  m_pWhite = (icFloatNumber*)calloc(range.steps, sizeof(icFloatNumber));

  if (!m_pMatrix || !m_pOffset || !m_pWhite) {
    m_size = 0;
    return false;
  }

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralMatrix::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 81;
  icChar buf[bufSize];
  int i, j;
  icFloatNumber *data = m_pMatrix;

  snprintf(buf, bufSize, "BEGIN_%s %d %d \n", GetDescribeName(), m_nInputChannels, m_nOutputChannels);
  sDescription += buf;

  snprintf(buf, bufSize, "RANGE %f %f %d\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  sDescription += buf;

  sDescription += "White\n";
  for (j=0; j<(int)m_Range.steps; j++) {
    if (j)
      sDescription += " ";
    snprintf(buf, bufSize, "%12.8lf", m_pWhite[j]);
    sDescription += buf;
  }
  sDescription += "\n";

  sDescription += "BLACK_OFFSET\n";
  for (j=0; j<(int)m_Range.steps; j++) {
    if (j)
      sDescription += " ";
    snprintf(buf, bufSize, "%12.8lf", m_pOffset[j]);
    sDescription += buf;
  }
  sDescription += "\n";

  if (data) {
    sDescription += "CHANNEL_DATA\n";
    for (j=0; j<m_nOutputChannels; j++) {
      for (i=0; i<(int)m_Range.steps; i++) {
        if (i)
          sDescription += " ";
        snprintf(buf, bufSize, "%12.8lf", data[i]);
        sDescription += buf;
      }
      sDescription += "\n";
      data += m_nInputChannels;
    }
  }

  snprintf(buf, bufSize, "END_%s\n", GetDescribeName());
  sDescription += buf;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::Read
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeSpectralMatrix::Read(icUInt32Number size, CIccIO *pIO)
{
  icElemTypeSignature sig;
  
  icUInt32Number headerSize = sizeof(icElemTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt16Number) + 
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  icUInt16Number nInputChannels, nOutputChannels;
  icSpectralRange range;

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read16(&nInputChannels))
    return false;

  if (!pIO->Read16(&nOutputChannels))
    return false;

  if (!pIO->Read16(&range.start))
    return false;

  if (!pIO->Read16(&range.end))
    return false;

  if (!pIO->Read16(&range.steps))
    return false;

  if (!pIO->Read16(&m_nReserved2))
    return false;

  SetSize(nInputChannels, nOutputChannels, range);
  if (!m_pWhite || !m_pMatrix || !m_pOffset)
    return false;

  if (size<headerSize + (int)range.steps*sizeof(icFloatNumber))
    return false;

  //Read White data
  if (pIO->ReadFloat32Float(m_pWhite, range.steps)!=range.steps)
    return false;

  if (size<headerSize + (int)range.steps*sizeof(icFloatNumber) + m_size * sizeof(icFloatNumber))
    return false;

  //Read Matrix data
  if (pIO->ReadFloat32Float(m_pMatrix, m_size)!= m_size)
    return false;

  if (size>=headerSize + 2*(int)range.steps*sizeof(icFloatNumber) + m_size * sizeof(icFloatNumber)) {
    if (pIO->ReadFloat32Float(m_pOffset, range.steps)!=range.steps)
      return false;
  }
  else {
    memset(m_pOffset, 0, (int)range.steps*sizeof(icFloatNumber));
  }
  
  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::Write
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeSpectralMatrix::Write(CIccIO *pIO)
{
  icElemTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write16(&m_nInputChannels))
    return false;

  if (!pIO->Write16(&m_nOutputChannels))
    return false;

  if (!pIO->Write16(&m_Range.start))
    return false;

  if (!pIO->Write16(&m_Range.end))
    return false;

  if (!pIO->Write16(&m_Range.steps))
    return false;

  if (!pIO->Write16(&m_nReserved2))
    return false;

  if (m_pWhite) {
    if (pIO->WriteFloat32Float(m_pWhite, m_Range.steps)!=m_Range.steps)
      return false;
  }
  else if (m_Range.steps) {
    return false;
  }

  if (m_pMatrix) {
    if (pIO->WriteFloat32Float(m_pMatrix, m_size)!= m_size)
      return false;
  }

  //Write Constant data
  if (m_pOffset) {
    if (pIO->WriteFloat32Float(m_pOffset, m_Range.steps)!=m_Range.steps)
      return false;
  }

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralMatrix::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccMpeSpectralMatrix::Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE/*=NULL*/, const CIccProfile *pProfile/*=NULL*/) const
{
  std::string mpeSigPath = sigPath + icGetSigPath(GetType());
  icValidateStatus rv = CIccMultiProcessElement::Validate(sigPath, sReport, pMPE, pProfile);
  
  if (!m_Range.steps) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Cannot have zero spectral range steps!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (m_nOutputChannels != 3) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Output Channels must be 3!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_pWhite) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has Empty White data!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_pMatrix) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has Empty Matrix data!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_pOffset) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has Empty Matrix Constant data!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (m_Range.start>= m_Range.end || !m_Range.steps) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has an invalid spectral range!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  return rv;
}

/**
 ******************************************************************************
 * Name: CIccMpeEmissionMatrix::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeEmissionMatrix::Begin(icElemInterp /* nInterp */, CIccTagMultiProcessElement *pMPE)
{
  if (!m_pOffset ||!pMPE || !m_pMatrix || m_nOutputChannels != 3)
    return false;

  IIccProfileConnectionConditions *pAppliedPCC = pMPE->GetAppliedPCC();
  if (!pAppliedPCC)
    return false;
 
  const CIccTagSpectralViewingConditions *pSVC = pAppliedPCC->getPccViewingConditions();
  if (!pSVC)
    return false;

  CIccMatrixMath observer(3, m_Range.steps);
  icFloat32Number *pSrc, *pMtx;

  if (!pAppliedPCC->getEmissiveObserver(m_Range, m_pWhite, observer.entry(0)))
    return false;

  //convert m_Matrix emission values to a matrix of XYZ column vectors
  m_pApplyMtx = new CIccMatrixMath(3,m_nInputChannels);

  if (!m_pApplyMtx)
    return false;

  icFloatNumber xyz[3];
  int i;

  pSrc = m_pMatrix;
  pMtx = m_pApplyMtx->entry(0);
  for (i=0; i<m_nInputChannels; i++) {
    observer.VectorMult(xyz, pSrc);
    pSrc += m_Range.steps;

    pMtx[0] = xyz[0];
    pMtx[m_nInputChannels] = xyz[1];
    pMtx[2*m_nInputChannels] = xyz[2];
    pMtx++;
  }

  //Now convert offset emission to XYZ offset
  observer.VectorMult(m_xyzOffset, m_pOffset);

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeEmissionMatrix::Apply
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeEmissionMatrix::Apply(CIccApplyMpe * /* pApply */, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const
{
  if (m_pApplyMtx) {
    m_pApplyMtx->VectorMult(dstPixel, srcPixel);
    dstPixel[0] += m_xyzOffset[0];
    dstPixel[1] += m_xyzOffset[1];
    dstPixel[2] += m_xyzOffset[2];
  }
  else {
    dstPixel[0] = 0;
    dstPixel[1] = 0;
    dstPixel[2] = 0;
  }
}


/**
 ******************************************************************************
 * Name: CIccMpeInvEmissionMatrix::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeInvEmissionMatrix::Begin(icElemInterp /* nInterp */, CIccTagMultiProcessElement *pMPE)
{
  if (!m_pOffset ||!pMPE || !m_pMatrix || m_nInputChannels != 3 || m_nOutputChannels != 3)
    return false;

  IIccProfileConnectionConditions *pAppliedPCC = pMPE->GetAppliedPCC();
  if (!pAppliedPCC)
    return false;
 
  const CIccTagSpectralViewingConditions *pSVC = pAppliedPCC->getPccViewingConditions();
  if (!pSVC)
    return false;

  CIccMatrixMath observer(3, m_Range.steps);
  icFloat32Number *pSrc, *pMtx;

  if (!pAppliedPCC->getEmissiveObserver(m_Range, m_pWhite, observer.entry(0)))
    return false;

  observer.VectorMult(m_xyzOffset, m_pOffset);

  //convert m_Matrix emission values to a matrix of XYZ column vectors
  m_pApplyMtx = new CIccMatrixMath(3,m_nInputChannels);

  if (!m_pApplyMtx)
    return false;

  icFloatNumber xyz[3];
  int i;

  pSrc = m_pMatrix;
  pMtx = m_pApplyMtx->entry(0);
  for (i=0; i<m_nInputChannels; i++) {
    observer.VectorMult(xyz, pSrc);
    pSrc += m_Range.steps;

    pMtx[0] = xyz[0];
    pMtx[m_nInputChannels] = xyz[1];
    pMtx[2*m_nInputChannels] = xyz[2];
    pMtx++;
  }

  m_pApplyMtx->Invert();

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeInvEmissionMatrix::Apply
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeInvEmissionMatrix::Apply(CIccApplyMpe * /* pApply */, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const
{
  if (m_pApplyMtx) {
    icFloatNumber xyz[3];
    xyz[0] = srcPixel[0] - m_xyzOffset[0];
    xyz[1] = srcPixel[1] - m_xyzOffset[1];
    xyz[2] = srcPixel[2] - m_xyzOffset[2];
    m_pApplyMtx->VectorMult(dstPixel, xyz);
  }
  else {
    dstPixel[0] = 0;
    dstPixel[1] = 0;
    dstPixel[2] = 0;
  }
}

/**
 ******************************************************************************
 * Name: CIccMpeInvEmissionMatrix::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccMpeInvEmissionMatrix::Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE/*=NULL*/, const CIccProfile *pProfile/*=NULL*/) const
{
  std::string mpeSigPath = sigPath + icGetSigPath(GetType());
  icValidateStatus rv = CIccMpeSpectralMatrix::Validate(sigPath, sReport, pMPE, pProfile);

  if (m_nInputChannels != 3) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Input Channels must be 3!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  return rv;
}

static icFloatNumber NoClip(icFloatNumber v)
{
  return v;
}



/**
 ******************************************************************************
 * Name: CIccMpeSpectralCLUT::CIccMpeSpectralCLUT
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralCLUT::CIccMpeSpectralCLUT()
{
  m_nInputChannels = 0;
  m_nOutputChannels = 0;

  m_nReserved = 0;

  m_Range.start = 0;
  m_Range.end = 0;
  m_Range.steps = 0;

  m_nStorageType = icValueTypeFloat32;
  m_flags = 0;

  m_pCLUT = NULL;
  m_pApplyCLUT = NULL;
  m_pWhite = NULL;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralCLUT::CIccMpeSpectralCLUT
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralCLUT::CIccMpeSpectralCLUT(const CIccMpeSpectralCLUT &clut)
{
  if (clut.m_pCLUT)
    m_pCLUT = new CIccCLUT(*clut.m_pCLUT);
  else
    m_pCLUT = NULL;

  if (clut.m_pApplyCLUT)
    m_pApplyCLUT = new CIccCLUT(*clut.m_pApplyCLUT);
  else
    m_pApplyCLUT = NULL;

  if (clut.m_pWhite) {
    m_pWhite = (icFloatNumber *)malloc((int)clut.m_Range.steps*sizeof(icFloatNumber));
    memcpy(m_pWhite, clut.m_pWhite, clut.m_Range.steps*sizeof(icFloatNumber));
  }
  else
    m_pWhite = NULL;

  m_nReserved = clut.m_nReserved;
  m_nInputChannels = clut.m_nInputChannels;
  m_nOutputChannels = clut.m_nOutputChannels;

  m_Range = clut.m_Range;
  m_nStorageType = clut.m_nStorageType;
  m_flags = clut.m_flags;
}

/**
 ******************************************************************************
 * Name: &CIccMpeSpectralCLUT::operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralCLUT::copyData(const CIccMpeSpectralCLUT &clut)
{
  if (m_pCLUT)
    delete m_pCLUT;

  if (m_pApplyCLUT)
    delete m_pApplyCLUT;

  if (m_pWhite)
    free(m_pWhite);

  if (clut.m_pCLUT)
    m_pCLUT = new CIccCLUT(*clut.m_pCLUT);
  else
    m_pCLUT = NULL;

  if (clut.m_pApplyCLUT)
    m_pApplyCLUT = new CIccCLUT(*clut.m_pApplyCLUT);
  else
    m_pApplyCLUT = NULL;

  if (clut.m_pWhite) {
    m_pWhite = (icFloatNumber *)malloc((int)clut.m_Range.steps*sizeof(icFloatNumber));
    memcpy(m_pWhite, clut.m_pWhite, clut.m_Range.steps*sizeof(icFloatNumber));
  }
  else
    m_pWhite = NULL;

  m_nReserved = clut.m_nReserved;
  m_nInputChannels = clut.m_nInputChannels;
  m_nOutputChannels = clut.m_nOutputChannels;

  m_Range = clut.m_Range;
  m_nStorageType = clut.m_nStorageType;
  m_flags = clut.m_flags;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralCLUT::~CIccMpeSpectralCLUT
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralCLUT::~CIccMpeSpectralCLUT()
{
  if (m_pCLUT)
    delete m_pCLUT;

  if (m_pApplyCLUT)
    delete m_pApplyCLUT;

  if (m_pWhite)
    free(m_pWhite);

}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralCLUT::SetCLUT
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralCLUT::SetData(CIccCLUT *pCLUT, icUInt16Number nStorageType, 
                                  const icSpectralRange &range, icFloatNumber *pWhite,
                                  icUInt16Number nOutputChannels)
{
  if (m_pCLUT)
    delete m_pCLUT;

  m_pCLUT = pCLUT;
  if (pCLUT) {
    pCLUT->SetClipFunc(NoClip);
    m_nInputChannels = pCLUT->GetInputDim();
    m_nOutputChannels = nOutputChannels;
  }

  m_nStorageType = nStorageType;

  if (m_pApplyCLUT) {
    delete m_pApplyCLUT;
    m_pApplyCLUT = NULL;
  }

  m_Range = range;

  if (m_pWhite)
    free(m_pWhite);

  m_pWhite = pWhite;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralCLUT::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralCLUT::Describe(std::string &sDescription, int nVerboseness)
{
  if (m_pCLUT) {
    m_pCLUT->DumpLut(sDescription, GetDescribeName(), icSigUnknownData, icSigUnknownData, nVerboseness);
  }
}


/**
******************************************************************************
* Name: CIccMpeSpectralCLUT::Read
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeSpectralCLUT::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt16Number) + 
    sizeof(icUInt16Number) + 
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    16 * sizeof(icUInt8Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read16(&m_nInputChannels))
    return false;

  if (!pIO->Read16(&m_nOutputChannels))
    return false;

  if (!pIO->Read32(&m_flags))
    return false;

  if (!pIO->Read16(&m_Range.start))
    return false;

  if (!pIO->Read16(&m_Range.end))
    return false;

  if (!pIO->Read16(&m_Range.steps))
    return false;

  if (!pIO->Read16(&m_nStorageType))
    return false;

  icUInt8Number gridPoints[16];

  if (pIO->Read8(gridPoints, 16)!=16) {
    return false;
  }

  m_pCLUT = new CIccCLUT((icUInt8Number)m_nInputChannels, (icUInt16Number)m_Range.steps, 4);

  if (!m_pCLUT)
    return false;

  m_pCLUT->SetClipFunc(NoClip);
  icUInt32Number nBytesPerPoint = icGetStorageTypeBytes(m_nStorageType) * m_Range.steps;
  
  if (!nBytesPerPoint)
    return false;

  m_pCLUT->Init(gridPoints, size - headerSize, (icUInt8Number)nBytesPerPoint);

  icFloatNumber *pData = m_pCLUT->GetData(0);

  if (!pData)
    return false;

  size_t nPoints = m_pCLUT->NumPoints()*(int)m_Range.steps;

  switch(m_nStorageType) {
    case icValueTypeUInt8:
      if (pIO->ReadUInt8Float(pData,nPoints)!= nPoints)
        return false;
      break;

    case icValueTypeUInt16:
      if (pIO->ReadUInt16Float(pData,nPoints)!= nPoints)
        return false;
      break;

    case icValueTypeFloat16:
      if (pIO->ReadFloat16Float(pData,nPoints)!= nPoints)
        return false;
      break;

    case icValueTypeFloat32:
      if (pIO->ReadFloat32Float(pData,nPoints)!= nPoints)
        return false;
      break;

    default:
      return false;
  }

  if (m_Range.steps *nBytesPerPoint > size - headerSize - nPoints*nBytesPerPoint)
    return false;

  m_pWhite = (icFloatNumber *)malloc((int)m_Range.steps*sizeof(icFloatNumber));
  if (!m_pWhite)
    return false;

  switch(m_nStorageType) {
    case icValueTypeUInt8:
      if (pIO->ReadUInt8Float(m_pWhite,m_Range.steps)!= m_Range.steps)
        return false;
      break;

    case icValueTypeUInt16:
      if (pIO->ReadUInt16Float(m_pWhite,m_Range.steps)!= m_Range.steps)
        return false;
      break;

    case icValueTypeFloat16:
      if (pIO->ReadFloat16Float(m_pWhite,m_Range.steps)!= m_Range.steps)
        return false;
      break;

    case icValueTypeFloat32:
      if (pIO->ReadFloat32Float(m_pWhite,m_Range.steps)!= m_Range.steps)
        return false;
      break;

    default:
      return false;
  }


  return true;
}

/**
******************************************************************************
* Name: CIccMpeSpectralCLUT::Write
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeSpectralCLUT::Write(CIccIO *pIO)
{
  icElemTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write16(&m_nInputChannels))
    return false;

  if (!pIO->Write16(&m_nOutputChannels))
    return false;

  if (!pIO->Write32(&m_flags))
    return false;

  if (!pIO->Write16(&m_Range.start))
    return false;

  if (!pIO->Write16(&m_Range.end))
    return false;

  if (!pIO->Write16(&m_Range.steps))
    return false;

  if (!pIO->Write16(&m_nStorageType))
    return false;

  if (m_pCLUT) {
    icUInt8Number gridPoints[16];
    int i;

    for (i=0; i<16; i++)
      gridPoints[i] = m_pCLUT->GridPoint(i);

    if (pIO->Write8(gridPoints, 16)!=16)
      return false;

    icFloatNumber *pData = m_pCLUT->GetData(0);
    size_t nPoints = m_pCLUT->NumPoints()*(int)m_Range.steps;

    switch(m_nStorageType) {
    case icValueTypeUInt8:
      if (pIO->WriteUInt8Float(pData,nPoints)!= nPoints)
        return false;
      break;

    case icValueTypeUInt16:
      if (pIO->WriteUInt16Float(pData,nPoints)!= nPoints)
        return false;
      break;

    case icValueTypeFloat16:
      if (pIO->WriteFloat16Float(pData,nPoints)!= nPoints)
        return false;
      break;

    case icValueTypeFloat32:
      if (pIO->WriteFloat32Float(pData,nPoints)!= nPoints)
        return false;
      break;

    default:
      return false;
    }
  }

  if (m_pWhite) {
    switch(m_nStorageType) {
    case icValueTypeUInt8:
      if (pIO->WriteUInt8Float(m_pWhite, m_Range.steps)!= m_Range.steps)
        return false;
      break;

    case icValueTypeUInt16:
      if (pIO->WriteUInt16Float(m_pWhite, m_Range.steps)!= m_Range.steps)
        return false;
      break;

    case icValueTypeFloat16:
      if (pIO->WriteFloat16Float(m_pWhite, m_Range.steps)!= m_Range.steps)
        return false;
      break;

    case icValueTypeFloat32:
      if (pIO->WriteFloat32Float(m_pWhite, m_Range.steps)!= m_Range.steps)
        return false;
      break;

    default:
      return false;
    }
  }
  else if (m_Range.steps) {
    return false;
  }

  return true;
}

/**
******************************************************************************
* Name: CIccMpeSpectralCLUT::GetNewApply
*
* Purpose:
*
* Args:
*
* Return:
******************************************************************************/
CIccApplyMpe* CIccMpeSpectralCLUT::GetNewApply(CIccApplyTagMpe* /* pApplyTag */)
{
  if (!m_pCLUT) {
    return NULL;
  }

  CIccApplyCLUT* pApply = m_pCLUT->GetNewApply();
  if (!pApply)
    return NULL;

  CIccApplyMpeSpectralCLUT* rv = new CIccApplyMpeSpectralCLUT(this, pApply);
  if (!rv)
    delete pApply;

  return rv;
}

/**
 ******************************************************************************
 * Name: CIccMpeEmissionCLUT::Apply
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralCLUT::Apply(CIccApplyMpe *pApply, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const
{
  const CIccCLUT *pCLUT = m_pApplyCLUT;

  switch(m_interpType) {
  case ic1dInterp:
    pCLUT->Interp1d(dstPixel, srcPixel);
    break;
  case ic2dInterp:
    pCLUT->Interp2d(dstPixel, srcPixel);
    break;
  case ic3dInterpTetra:
    pCLUT->Interp3dTetra(dstPixel, srcPixel);
    break;
  case ic3dInterp:
    pCLUT->Interp3d(dstPixel, srcPixel);
    break;
  case ic4dInterp:
    pCLUT->Interp4d(dstPixel, srcPixel);
    break;
  case ic5dInterp:
    pCLUT->Interp5d(dstPixel, srcPixel);
    break;
  case ic6dInterp:
    pCLUT->Interp6d(dstPixel, srcPixel);
    break;
  case icNdInterp:
    CIccApplyMpeSpectralCLUT* pClutApply = (CIccApplyMpeSpectralCLUT*)pApply;
    pCLUT->InterpND(dstPixel, srcPixel, pClutApply->m_pApply);
    break;
  }
}


/**
 ******************************************************************************
 * Name: CIccMpeSpectralCLUT::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccMpeSpectralCLUT::Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE/*=NULL*/, const CIccProfile *pProfile/*=NULL*/) const
{
  std::string mpeSigPath = sigPath + icGetSigPath(GetType());
  icValidateStatus rv = CIccMultiProcessElement::Validate(sigPath, sReport, pMPE, pProfile);

  if (m_nStorageType>icMaxValueType) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Invalid storageType value!\n";
    return icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_pCLUT) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has No CLUT!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_pWhite) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has Empty White data!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_Range.steps) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Cannot have zero spectral range steps!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (m_Range.start>= m_Range.end || !m_Range.steps) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has an invalid spectral range!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  return rv;
}

/**
**************************************************************************
* Name: CIccApplyMpeSpectralCLUT::CIccApplyMpeSpectralCLUT
*
* Purpose:
*  Constructor
**************************************************************************
*/
CIccApplyMpeSpectralCLUT::CIccApplyMpeSpectralCLUT(CIccMultiProcessElement* pElem, CIccApplyCLUT* pApply) : CIccApplyMpe(pElem)
{
  m_pApply = pApply;
}


/**
**************************************************************************
* Name: CIccApplyMpeSpectralCLUT::~CIccApplyMpeSpectralCLUT
*
* Purpose:
*  Destructor
**************************************************************************
*/
CIccApplyMpeSpectralCLUT::~CIccApplyMpeSpectralCLUT()
{
  if (m_pApply)
    delete m_pApply;
}




/**
 ******************************************************************************
 * Name: CIccMpeEmissionCLUT::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeEmissionCLUT::Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE)
{
  if (!m_pCLUT || !m_pWhite || m_pCLUT->GetOutputChannels()!=m_Range.steps || m_nOutputChannels!=3)
    return false;

  switch (m_nInputChannels) {
  case 1:
    m_interpType = ic1dInterp;
    break;
  case 2:
    m_interpType = ic2dInterp;
    break;
  case 3:
    if (nInterp==icElemInterpTetra)
      m_interpType = ic3dInterpTetra;
    else
      m_interpType = ic3dInterp;
    break;
  case 4:
    m_interpType = ic4dInterp;
    break;
  case 5:
    m_interpType = ic5dInterp;
    break;
  case 6:
    m_interpType = ic6dInterp;
    break;
  default:
    m_interpType = icNdInterp;
    break;
  }

  IIccProfileConnectionConditions *pAppliedPCC = pMPE->GetAppliedPCC();
  if (!pAppliedPCC)
    return false;

  const CIccTagSpectralViewingConditions *pSVC = pAppliedPCC->getPccViewingConditions();
  if (!pSVC)
    return false;

  CIccMatrixMath observer(3, m_Range.steps);

  if (!pAppliedPCC->getEmissiveObserver(m_Range, m_pWhite, observer.entry(0)))
    return false;

  if (m_pApplyCLUT)
    delete m_pApplyCLUT;

  m_pApplyCLUT = new CIccCLUT((icUInt8Number)m_nInputChannels, (icUInt16Number)m_nOutputChannels, 4);

  if (!m_pApplyCLUT) {
    return false;
  }

  m_pApplyCLUT->SetClipFunc(NoClip);
  m_pApplyCLUT->Init(m_pCLUT->GridPointArray());

  icFloatNumber *pSrc = m_pCLUT->GetData(0);
  icFloatNumber *pDst = m_pApplyCLUT->GetData(0);

  icFloatNumber xyzW[3];
  icUInt32Number i;

  observer.VectorMult(xyzW, m_pWhite);

  //bool bUseAbsolute = (m_flags & icRelativeSpectralData)!=0;
  bool bLab = (m_flags & icLabSpectralData) != 0;

  for (i=0; i<m_pCLUT->NumPoints(); i++) {
    observer.VectorMult(pDst, pSrc);
    if (bLab) {
      icXYZtoLab(pDst, pDst, xyzW);
//      icLabToPcs(pDst);
    }
    else {
//      icXyzToPcs(pDst);
    }
    pSrc += m_Range.steps;
    pDst += m_nOutputChannels;
  }

  m_pApplyCLUT->Begin();

  return true;
}


/**
 ******************************************************************************
 * Name: CIccMpeReflectanceCLUT::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeReflectanceCLUT::Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE)
{
  if (!m_pCLUT || !m_pWhite || m_pCLUT->GetOutputChannels()!=m_Range.steps || m_nOutputChannels!=3)
    return false;

  switch (m_nInputChannels) {
  case 1:
    m_interpType = ic1dInterp;
    break;
  case 2:
    m_interpType = ic2dInterp;
    break;
  case 3:
    if (nInterp==icElemInterpTetra)
      m_interpType = ic3dInterpTetra;
    else
      m_interpType = ic3dInterp;
    break;
  case 4:
    m_interpType = ic4dInterp;
    break;
  case 5:
    m_interpType = ic5dInterp;
    break;
  case 6:
    m_interpType = ic6dInterp;
    break;
  default:
    m_interpType = icNdInterp;
    break;
  }

  IIccProfileConnectionConditions *pAppliedPCC = pMPE->GetAppliedPCC();
  if (!pAppliedPCC)
    return false;

  const CIccTagSpectralViewingConditions *pSVC = pAppliedPCC->getPccViewingConditions();
  if (!pSVC)
    return false;

  CIccMatrixMath observer(3, m_Range.steps);
  icSpectralRange illumRange;
  const icFloatNumber *illum = pSVC->getIlluminant(illumRange);

  if (!pAppliedPCC->getEmissiveObserver(illumRange, illum, observer.entry(0)))
    return false;

  //
  icFloatNumber xyzi[3];

  //apply illuminant to observer and calculate XYZ of illuminant
  icFloatNumber *pObs = observer.entry(0);
  int i, j;
  for (i=0; i<3; i++) {
    xyzi[i] = 0.0;
    for (j=0; j<illumRange.steps; j++) {
      *pObs *= illum[j];
      xyzi[i] += *pObs;
      pObs++;
    }
  }

  //concatenate reflectance range mapping to observer+illuminant
  CIccMatrixMath *rangeRef = CIccMatrixMath::rangeMap(m_Range, illumRange);
  CIccMatrixMath *pApplyMtx;
  if (!rangeRef) 
    pApplyMtx = &observer;
  else
    pApplyMtx = rangeRef->Mult(&observer);

  if (m_pApplyCLUT)
    delete m_pApplyCLUT;

  m_pApplyCLUT = new CIccCLUT((icUInt8Number)m_nInputChannels, (icUInt16Number)m_nOutputChannels, 4);

  if (!m_pApplyCLUT) {
    if (pApplyMtx!=&observer)
      delete pApplyMtx;
    return false;
  }

  m_pApplyCLUT->SetClipFunc(NoClip);
  m_pApplyCLUT->Init(m_pCLUT->GridPointArray());

  icFloatNumber *pSrc = m_pCLUT->GetData(0);
  icFloatNumber *pDst = m_pApplyCLUT->GetData(0);

  icFloatNumber xyzW[3];

  pApplyMtx->VectorMult(xyzW, m_pWhite);

  bool bUseAbsolute = (m_flags & icRelativeSpectralData)!=0;
  bool bLab = (m_flags & icLabSpectralData) != 0;

  icFloatNumber xyzscale[3];
  if (!bUseAbsolute) {
    xyzscale[0] = xyzi[0] / xyzW[0];
    xyzscale[1] = xyzi[1] / xyzW[1];
    xyzscale[2] = xyzi[2] / xyzW[2];
  }

  for (i=0; i<(int)m_pCLUT->NumPoints(); i++) {
    pApplyMtx->VectorMult(pDst, pSrc);

    if (!bUseAbsolute) {
      pDst[0] *= xyzscale[0];
      pDst[1] *= xyzscale[1];
      pDst[2] *= xyzscale[2];
    }

    if (bLab) {
      icXYZtoLab(pDst, pDst, xyzi);
      //      icLabToPcs(pDst);
    }
    else {
      //      icXyzToPcs(pDst);
    }
    pSrc += m_Range.steps;
    pDst += m_nOutputChannels;
  }

  if (pApplyMtx!=&observer)
    delete pApplyMtx;

  m_pApplyCLUT->Begin();

  return true;
}


/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::CIccMpeSpectralObserver
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralObserver::CIccMpeSpectralObserver()
{
  m_nReserved = 0;
  m_nInputChannels = m_nOutputChannels = 0;
  m_pWhite = NULL;

  m_Range.start=0;
  m_Range.end=0;
  m_Range.steps=0;

  m_pApplyMtx = NULL;
}


/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::CIccMpeSpectralObserver
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralObserver::CIccMpeSpectralObserver(const CIccMpeSpectralObserver &matrix)
{
  m_nReserved = matrix.m_nReserved;

  m_nInputChannels = matrix.m_nInputChannels;
  m_nOutputChannels = matrix.m_nOutputChannels;

  m_Range = matrix.m_Range;

  if (matrix.m_pWhite) {
    int num = m_Range.steps*sizeof(icFloatNumber);
    m_pWhite = (icFloatNumber*)malloc(num);
    memcpy(m_pWhite, matrix.m_pWhite, num);
  }
  else
    m_pWhite = NULL;

  m_pApplyMtx = NULL;
}

/**
 ******************************************************************************
 * Name: &CIccMpeSpectralObserver::operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralObserver::copyData(const CIccMpeSpectralObserver &matrix)
{
  m_nReserved = matrix.m_nReserved;

  m_nInputChannels = matrix.m_nInputChannels;
  m_nOutputChannels = matrix.m_nOutputChannels;

  m_Range = matrix.m_Range;

  if (m_pWhite)
    free(m_pWhite);

  if (matrix.m_pWhite) {
    int num = m_Range.steps*sizeof(icFloatNumber);
    m_pWhite = (icFloatNumber*)malloc(num);
    memcpy(m_pWhite, matrix.m_pWhite, num);
  }
  else
    m_pWhite = NULL;

  m_pApplyMtx = NULL;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::~CIccMpeSpectralObserver
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeSpectralObserver::~CIccMpeSpectralObserver()
{
  if (m_pWhite)
    free(m_pWhite);

  if (m_pApplyMtx)
    delete m_pApplyMtx;
}


/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::SetSize
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeSpectralObserver::SetSize(icUInt16Number nInputChannels, icUInt16Number nOutputChannels, const icSpectralRange &range)
{
  if (m_pWhite) {
    free(m_pWhite);
    m_pWhite = NULL;
  }

  if (m_pApplyMtx) {
    delete m_pApplyMtx;
    m_pApplyMtx = NULL;
  }

  m_nInputChannels = nInputChannels;
  m_nOutputChannels = nOutputChannels;
  m_Range = range;

  m_pWhite = (icFloatNumber*)calloc(range.steps, sizeof(icFloatNumber));

  if (!m_pWhite)
    return false;

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeSpectralObserver::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 81;
  icChar buf[bufSize];
  int j;

  snprintf(buf, bufSize, "BEGIN_%s %d %d \n", GetDescribeName(), m_nInputChannels, m_nOutputChannels);
  sDescription += buf;

  snprintf(buf, bufSize, "RANGE %f %f %d\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  sDescription += buf;

  sDescription += "White\n";
  for (j=0; j<(int)m_Range.steps; j++) {
    if (j)
      sDescription += " ";
    snprintf(buf, bufSize, "%12.8lf", m_pWhite[j]);
    sDescription += buf;
  }
  sDescription += "\n";

  snprintf(buf, bufSize, "END_%s\n", GetDescribeName());
  sDescription += buf;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::Read
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeSpectralObserver::Read(icUInt32Number size, CIccIO *pIO)
{
  icElemTypeSignature sig;
  
  icUInt32Number headerSize = sizeof(icElemTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt16Number) + 
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number) +
    sizeof(icUInt16Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  icUInt16Number nInputChannels, nOutputChannels;
  icSpectralRange range;

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read16(&nInputChannels))
    return false;

  if (!pIO->Read16(&nOutputChannels))
    return false;

  if (!pIO->Read16(&range.start))
    return false;

  if (!pIO->Read16(&range.end))
    return false;

  if (!pIO->Read16(&range.steps))
    return false;

  if (!pIO->Read16(&m_flags))
    return false;

  if (!SetSize(nInputChannels, nOutputChannels, range))
    return false;

  if (!m_pWhite )
    return false;

  if (size<headerSize + (int)range.steps*sizeof(icFloatNumber))
    return false;

  //Read White data
  if (pIO->ReadFloat32Float(m_pWhite, range.steps)!=range.steps)
    return false;

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::Write
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeSpectralObserver::Write(CIccIO *pIO)
{
  icElemTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write16(&m_nInputChannels))
    return false;

  if (!pIO->Write16(&m_nOutputChannels))
    return false;

  if (!pIO->Write16(&m_Range.start))
    return false;

  if (!pIO->Write16(&m_Range.end))
    return false;

  if (!pIO->Write16(&m_Range.steps))
    return false;

  if (!pIO->Write16(&m_flags))
    return false;

  if (m_pWhite) {
    if (pIO->WriteFloat32Float(m_pWhite, m_Range.steps)!=m_Range.steps)
      return false;
  }
  else if (m_Range.steps) {
    return false;
  }

  return true;
}

void CIccMpeSpectralObserver::Apply(CIccApplyMpe * /* pApply */, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const
{
  if (m_pApplyMtx) {
    icFloatNumber xyz[3];
    m_pApplyMtx->VectorMult(xyz, srcPixel);

    bool bUseAbsolute = (m_flags & icRelativeSpectralData)!=0;
    bool bLab = (m_flags & icLabSpectralData) != 0;

    if (!bUseAbsolute) {
      xyz[0] *= m_xyzscale[0];
      xyz[1] *= m_xyzscale[1];
      xyz[2] *= m_xyzscale[2];
    }

    if (bLab) {
      icXYZtoLab(dstPixel, xyz, m_xyzw);
      //      icLabToPcs(dstPixel);
    }
    else {
      memcpy(dstPixel, xyz, 3*sizeof(icFloatNumber));
      //      icXyzToPcs(dstPixel);
    }
  }
}

/**
 ******************************************************************************
 * Name: CIccMpeSpectralObserver::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccMpeSpectralObserver::Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE/*=NULL*/, const CIccProfile *pProfile/*=NULL*/) const
{
  std::string mpeSigPath = sigPath + icGetSigPath(GetType());
  icValidateStatus rv = CIccMultiProcessElement::Validate(sigPath, sReport, pMPE, pProfile);
  
  if (!m_Range.steps) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Cannot have zero spectral range steps!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (m_nOutputChannels != 3) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Output Channels must be 3!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (!m_pWhite) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has Empty White data!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }


  if (m_Range.start>= m_Range.end || !m_Range.steps) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(mpeSigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has an invalid spectral range!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  return rv;
}


/**
 ******************************************************************************
 * Name: CIccMpeEmissionObserver::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeEmissionObserver::Begin(icElemInterp /* nInterp */, CIccTagMultiProcessElement *pMPE)
{
  if (!m_pWhite || m_nInputChannels!=m_Range.steps || m_nOutputChannels!=3)
    return false;

  IIccProfileConnectionConditions *pAppliedPCC = pMPE->GetAppliedPCC();
  if (!pAppliedPCC)
    return false;

  const CIccTagSpectralViewingConditions *pSVC = pAppliedPCC->getPccViewingConditions();
  if (!pSVC)
    return false;

  m_pApplyMtx = new CIccMatrixMath(3, m_Range.steps);

  if (!m_pApplyMtx)
    return false;

  if (!pAppliedPCC->getEmissiveObserver(m_Range, m_pWhite, m_pApplyMtx->entry(0)))
    return false;

  m_pApplyMtx->VectorMult(m_xyzw, m_pWhite);

  m_xyzscale[0] = 1.0;
  m_xyzscale[0] = 1.0;
  m_xyzscale[0] = 1.0;

  return true;
}


/**
 ******************************************************************************
 * Name: CIccMpeReflectanceObserver::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeReflectanceObserver::Begin(icElemInterp /* nInterp */, CIccTagMultiProcessElement *pMPE)
{
  if (!m_pWhite || m_nInputChannels!=m_Range.steps || m_nOutputChannels!=3)
    return false;

  IIccProfileConnectionConditions *pAppliedPCC = pMPE->GetAppliedPCC();
  if (!pAppliedPCC)
    return false;

  const CIccTagSpectralViewingConditions *pSVC = pAppliedPCC->getPccViewingConditions();
  if (!pSVC)
    return false;

  CIccMatrixMath observer(3, m_Range.steps);
  icSpectralRange illumRange;
  const icFloatNumber *illum = pSVC->getIlluminant(illumRange);

  if (!pAppliedPCC->getEmissiveObserver(illumRange, illum, observer.entry(0)))
    return false;

  //
  //apply illuminant to observer and calculate XYZ of illuminant
  icFloatNumber *pObs = observer.entry(0);
  int i, j;
  for (i=0; i<3; i++) {
    m_xyzw[i] = 0.0;
    for (j=0; j<illumRange.steps; j++) {
      *pObs *= illum[j];
      m_xyzw[i] += *pObs;
      pObs++;
    }
  }

  //concatenate reflectance range mapping to observer+illuminant
  CIccMatrixMath *rangeRef = CIccMatrixMath::rangeMap(m_Range, illumRange);
  if (!rangeRef) 
    m_pApplyMtx = new CIccMatrixMath(observer);
  else
    m_pApplyMtx = rangeRef->Mult(&observer);

  icFloatNumber xyzm[3];

  m_pApplyMtx->VectorMult(xyzm, m_pWhite);

  bool bUseAbsolute = (m_flags & icRelativeSpectralData)!=0;
  //bool bLab = (m_flags & icLabSpectralData) != 0;

  if (!bUseAbsolute) {
    m_xyzscale[0] = m_xyzw[0] / xyzm[0];
    m_xyzscale[1] = m_xyzw[1] / xyzm[1];
    m_xyzscale[2] = m_xyzw[2] / xyzm[2];
  }

  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
