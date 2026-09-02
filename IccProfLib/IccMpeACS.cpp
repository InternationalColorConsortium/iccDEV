/** @file
    File:       IccMpeACS.cpp

    Contains:   Implementation of ACS Elements

    Version:    V1

    Copyright:  (c) see ICC Software License
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
#include "IccMpeACS.h"
#include "IccIO.h"
#include <map>
#include "IccUtil.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


/**
******************************************************************************
* Name: CIccMpeAcs::CIccMpeACS
* 
* Purpose:
*  Base constructor (protected)
******************************************************************************/
CIccMpeAcs::CIccMpeAcs()
{
  m_pData = NULL;
  m_nDataSize = 0;
  m_signature = icSigAcsZero;
  m_nReserved = 0;
}

/**
******************************************************************************
* Name: CIccMpeAcs::~CIccMpeAcs
* 
* Purpose: 
*  Base destructor
******************************************************************************/
CIccMpeAcs::~CIccMpeAcs()
{
  free(m_pData);
}

/**
******************************************************************************
* Name: CIccMpeAcs::Describe
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
void CIccMpeAcs::Describe(std::string &sDescription, int nVerboseness)
{
  icChar sigBuf[30];

  // Label from the ELEMENT TYPE, not from the value of the ACS data signature.
  // GetBAcsSig() returns m_signature on a bACS element and the base class's
  // icSigAcsZero on everything else (IccTagMPE.h:177), so testing its truth
  // conflates "this is a bACS element" with "this element's signature is
  // non-zero".  Those two agree only by luck.  icSigAcsZero is a legitimate,
  // explicitly named signature -- CIccMpeXmlBAcs::ToXml goes out of its way to
  // round-trip it rather than writing the text "NULL" (#1843) -- and a bACS
  // element carrying it therefore described itself as ELEM_eACS (#2181).
  // GetType() is fixed per class (IccMpeACS.h:135 and :159) and cannot be
  // confused by the payload.  This is the only reader of GetBAcsSig() in the
  // tree; everything else discriminates on IsAcs(), which was always type-based.
  if (GetType() == icSigBAcsElemType)
    sDescription += "ELEM_bACS\n";
  else
    sDescription += "ELEM_eACS\n";

  icGetSig(sigBuf, 30, m_signature);
  sDescription += "  Signature = ";
  sDescription += sigBuf;
  sDescription += "\n";

  if ((m_pData) && (nVerboseness > 50)) {
    sDescription += "\nData Follows:\n";

    icMemDump(sDescription, m_pData, m_nDataSize);
  }
}

/**
******************************************************************************
* Name: CIccMpeAcs::Read
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeAcs::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt16Number) + 
    sizeof(icUInt16Number) + 
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

  if (!pIO->Read16(&m_nInputChannels))
    return false;

  if (!pIO->Read16(&m_nOutputChannels))
    return false;

  if (!pIO->Read32(&m_signature))
    return false;

  size_t dataSize = size - headerSize;

  if (!AllocData(dataSize))
    return false;

  if (dataSize) {
    if (pIO->Read8(m_pData, dataSize)!=dataSize)
      return false;
  }

  return true;
}

/**
******************************************************************************
* Name: CIccMpeAcs::Write
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeAcs::Write(CIccIO *pIO)
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

  if (!pIO->Write32(&m_signature))
    return false;

  if (m_pData && m_nDataSize) {
    // ERROR - sign and unsigned comparison, this should be fixed at a higher level
    // cast type to get it compiling for now
    if (pIO->Write8(m_pData, m_nDataSize) != m_nDataSize)
      return false;
  }

  return true;
}

/**
******************************************************************************
* Name: CIccMpeAcs::Begin
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeAcs::Begin(icElemInterp /* nInterp */, CIccTagMultiProcessElement * /* pMPE */)
{
  if (m_nInputChannels!=m_nOutputChannels)
    return false;

  return true;
}

/**
******************************************************************************
* Name: CIccMpeAcs::Apply
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
void CIccMpeAcs::Apply(CIccApplyMpe * /* pApply */, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const
{
  memcpy(dstPixel, srcPixel, m_nInputChannels*sizeof(icFloatNumber));
}

/**
******************************************************************************
* Name: CIccMpeAcs::Validate
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icValidateStatus CIccMpeAcs::Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE/*=NULL*/, const CIccProfile *pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccMultiProcessElement::Validate(sigPath, sReport, pMPE, pProfile);

  return rv;
}

/**
******************************************************************************
* Name: CIccMpeAcs::AllocData
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeAcs::AllocData(size_t size)
{
  // Clear both members before allocating, so the class invariant
  // "m_pData == NULL if and only if m_nDataSize == 0" holds on EVERY exit,
  // failure included.  It previously did not: on a failed malloc m_pData became
  // NULL while m_nDataSize kept the PREVIOUS allocation's size (#2181).
  //
  // That stale pair is not merely untidy, because the four copy paths below
  // guard the memcpy on the DESTINATION pointer and the SOURCE size:
  //
  //     AllocData(elemAcs.m_nDataSize);
  //     if (m_pData && elemAcs.m_nDataSize)
  //       memcpy(m_pData, elemAcs.m_pData, m_nDataSize);
  //
  // Copying an element left in the broken state allocates successfully from the
  // stale non-zero size, finds the source size non-zero, and reads from a source
  // pointer that is NULL.  Restoring the invariant at its origin fixes all four
  // sites -- and NewCopy() is one of them -- without four separate guards.
  free(m_pData);
  m_pData = NULL;
  m_nDataSize = 0;

  if (size) {
    m_pData = (icUInt8Number*)malloc(size);
    if (!m_pData)
      return false;
    m_nDataSize = size;
  }

  return true;
}


/**
******************************************************************************
* Name: CIccMpeBeginAcs::CIccMpeBeginAcs
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeBAcs::CIccMpeBAcs(icUInt16Number nChannels/* =0 */, icAcsSignature sig /* = icSigUnknownAcs */)
{
  m_signature = sig;

  m_nInputChannels = nChannels;
  m_nOutputChannels = nChannels;
}

/**
******************************************************************************
* Name: CIccMpeBeginAcs::CIccMpeBeginAcs
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeBAcs::CIccMpeBAcs(const CIccMpeBAcs &elemAcs)
{

  m_signature = elemAcs.m_signature;
  m_nReserved = elemAcs.m_nReserved;
  m_nInputChannels = elemAcs.m_nInputChannels;
  m_nOutputChannels = elemAcs.m_nOutputChannels;

  m_pData = NULL;
  m_nDataSize = 0;

  AllocData(elemAcs.m_nDataSize);
  if (m_pData && elemAcs.m_nDataSize) {
    memcpy(m_pData, elemAcs.m_pData, m_nDataSize);
  }

  m_nReserved = 0;
}

/**
******************************************************************************
* Name: &CIccMpeBeginAcs::operator=
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeBAcs &CIccMpeBAcs::operator=(const CIccMpeBAcs &elemAcs)
{
  m_signature = elemAcs.m_signature;
  m_nReserved = elemAcs.m_nReserved;
  m_nInputChannels = elemAcs.m_nInputChannels;
  m_nOutputChannels = elemAcs.m_nOutputChannels;

  AllocData(elemAcs.m_nDataSize);
  if (m_pData && elemAcs.m_nDataSize) {
    memcpy(m_pData, elemAcs.m_pData, m_nDataSize);
  }

  return *this;
}

/**
******************************************************************************
* Name: CIccMpeBeginAcs::~CIccMpeBeginAcs
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeBAcs::~CIccMpeBAcs()
{
}

/**
******************************************************************************
* Name: CIccMpeEndAcs::CIccMpeEndAcs
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeEAcs::CIccMpeEAcs(icUInt16Number nChannels/* =0 */, icAcsSignature sig /* = icSigUnknownAcs */)
{
  m_signature = sig;

  m_nInputChannels = nChannels;
  m_nOutputChannels = nChannels;
}

/**
******************************************************************************
* Name: CIccMpeEndAcs::CIccMpeEndAcs
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeEAcs::CIccMpeEAcs(const CIccMpeEAcs &elemAcs)
{
  m_signature = elemAcs.m_signature;
  m_nReserved = elemAcs.m_nReserved;

  m_nInputChannels = elemAcs.m_nInputChannels;
  m_nOutputChannels = elemAcs.m_nOutputChannels;

  AllocData(elemAcs.m_nDataSize);
  if (m_pData && elemAcs.m_nDataSize) {
    memcpy(m_pData, elemAcs.m_pData, m_nDataSize);
  }
}

/**
******************************************************************************
* Name: &CIccMpeEndAcs::operator=
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeEAcs &CIccMpeEAcs::operator=(const CIccMpeEAcs &elemAcs)
{
  m_signature = elemAcs.m_signature;
  m_nReserved = elemAcs.m_nReserved;
  m_nInputChannels = elemAcs.m_nInputChannels;
  m_nOutputChannels = elemAcs.m_nOutputChannels;

  AllocData(elemAcs.m_nDataSize);
  if (m_pData && elemAcs.m_nDataSize) {
    memcpy(m_pData, elemAcs.m_pData, m_nDataSize);
  }

  return *this;
}

/**
******************************************************************************
* Name: CIccMpeEndAcs::~CIccMpeEndAcs
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMpeEAcs::~CIccMpeEAcs()
{
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
