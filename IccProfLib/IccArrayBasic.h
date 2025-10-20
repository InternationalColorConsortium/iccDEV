/** @file
    File:       IccArrayBasic.h

    Contains:   Header for implementation of basic IccArray handlers

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
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
// -Initial implementation by Max Derhak 7-27-2011
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCTARRAYBASIC_H)
#define _ICCTARRAYBASIC_H

#include <list>
#include <string>
#include <map>
#include "IccDefs.h"
#include "IccTagComposite.h"
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
****************************************************************************
* Class: CIccArrayUnknown
* 
* Purpose: The Unknown array handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccArrayUnknown : public IIccArray
{
public:
  CIccArrayUnknown(CIccTagArray *pTagArray = NULL, icArraySignature sigArray = (icArraySignature)0);
  virtual ~CIccArrayUnknown();
  virtual IIccArray* NewCopy(CIccTagArray *pTagArray) const;

  virtual void Describe(std::string &sDescription, int nVerboseness) const;

  virtual const icChar *GetClassName() const { return "CIccArrayUnknown"; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  CIccTagArray *m_pTag;
  icArraySignature m_sig;

};

/**
****************************************************************************
* Class: CIccArrayColorantInfo
*
* Purpose: The ColorantInfo array handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccArrayColorantInfo : public CIccArrayUnknown
{
public:
  CIccArrayColorantInfo(CIccTagArray *pTagArray = NULL);
  virtual ~CIccArrayColorantInfo();
  virtual IIccArray* NewCopy(CIccTagArray *pTagArray) const;

  virtual void Describe(std::string &sDescription, int nVerboseness) const;

  virtual const icChar *GetClassName() const { return "CIccArrayColorantInfo"; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

};


class CIccStructNamedColor;

typedef std::map<std::string, CIccStructNamedColor*> icNamedColorStructList;

/**
****************************************************************************
* Class: CIccArrayNamedColor
* 
* Purpose: The Named Color array handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccArrayNamedColor : public CIccArrayUnknown
{
public:
  CIccArrayNamedColor(CIccTagArray *pTagArray=NULL);
  virtual ~CIccArrayNamedColor();
  virtual IIccArray* NewCopy(CIccTagArray *pTagArray) const;

  virtual void Describe(std::string &sDescription, int nVerboseness) const;

  virtual const icChar *GetClassName() const { return "CIccArrayNamedColor"; }

  void SetColorSpaces(icColorSpaceSignature csPcs, icColorSpaceSignature csDevice, 
                      icSpectralColorSignature csSpectralPCS=icSigNoSpectralData,
                      const icSpectralRange *pSpectralRange = NULL,
                      const icSpectralRange *pBiSPectralRange = NULL);

  icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  bool Begin();

  CIccStructNamedColor* FindColor(const icChar *szColor) const;
  CIccStructNamedColor* FindDeviceColor(const icFloatNumber *pDevColor) const;
  CIccStructNamedColor* FindPcsColor(const icFloatNumber *pPCS, icFloatNumber dMinDE=1000.0) const;
  CIccStructNamedColor* FindSpectralColor(const icFloatNumber *pSpec, icFloatNumber dMinRms=1000.0) const;

  bool GetDeviceTint(icFloatNumber *dstColor,
                     const CIccStructNamedColor *pColor, 
                     icFloatNumber tint=1.0f,
                     icNamedColorlMemberSignature sig=icSigNmclDeviceDataMbr) const;
  bool GetPcsTint(icFloatNumber *dstColor,
                  const CIccStructNamedColor *pColor, 
                  icFloatNumber tint=1.0f,
                  icNamedColorlMemberSignature sig=icSigNmclPcsDataMbr) const;
  bool GetSpectralTint(icFloatNumber *dstColor,
                       const CIccStructNamedColor *pColor, 
                       icFloatNumber tint=1.0f,
                       icNamedColorlMemberSignature sig=icSigNmclSpectralDataMbr) const;

  icUInt32Number GetDeviceSamples() { return m_nDeviceSamples; }
  icUInt32Number GetPcsSamples() { return m_nPcsSamples; }
  icUInt32Number GetSpectralSamples() { return m_nSpectralSamples; }

protected:
  bool GetTint(icFloatNumber *dstColor,
               CIccTagNumArray *pZero,
               CIccTagNumArray *pData,
               CIccTagNumArray *pTint,
               icFloatNumber tint,
               icUInt32Number nSamples);

  icColorSpaceSignature m_csPcs;
  icColorSpaceSignature m_csDevice;
  icSpectralColorSignature m_csSpectralPcs;
  icSpectralRange m_spectralRange;
  icSpectralRange m_biSpectralRange;

  CIccStructNamedColor *m_pZeroTint;

  icNamedColorStructList *m_list;

  icUInt32Number m_nDeviceSamples;
  icUInt32Number m_nPcsSamples;
  icUInt32Number m_nSpectralSamples;
};



#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCARRAYBASIC_H)
