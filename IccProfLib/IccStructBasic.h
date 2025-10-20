/** @file
    File:       IccStructBasic.h

    Contains:   Header for implementation of basic IccStruct handlers

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

#if !defined(_ICCSTRUCTBASIC_H)
#define _ICCSTRUCTBASIC_H

#include <list>
#include <string>
#include "IccDefs.h"
#include "IccTagComposite.h"
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

struct ICCPROFLIB_API SIccElemNameSig
{
  icSignature nSig;
  const icChar *szName;
};

/**
****************************************************************************
* Class: CIccStructUnknown
* 
* Purpose: The Unknown struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructUnknown : public IIccStruct
{
public:
  CIccStructUnknown(CIccTagStruct *pTagStruct=NULL);
  virtual ~CIccStructUnknown();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual void Describe(std::string &sDescription, int nVerboseness) const;

  virtual const icChar *GetClassName() const { return "CIccStructUnknown"; }
  virtual const icChar *GetDisplayName() const { return "privateStruct"; }

  virtual std::string GetElemName(icSignature sigElem) const;
  virtual icSignature GetElemSig(const icChar *szElemName) const;

  CIccTag *GetElem(icSignature sigElem) const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

protected:
  SIccElemNameSig *m_pElemNameSigTable;
  
};

/**
****************************************************************************
* Class: CIccStructNamedColor
* 
* Purpose: The Named Color struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructNamedColor : public CIccStructUnknown
{
public:
  CIccStructNamedColor(CIccTagStruct *pTagStruct=NULL);
  virtual ~CIccStructNamedColor();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructNamedColor"; }
  virtual const icChar *GetDisplayName() const { return "namedColorStructure"; }

  std::string getName() const;

  bool GetTint(icFloatNumber *dstColor,
               icFloatNumber tint,
               CIccTagNumArray *pZero,
               icSignature sigElem,
               icUInt32Number nSamples) const;

  CIccTagNumArray *GetNumArray(icSignature sigElem) const;

  icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/* = NULL*/) const;

protected:

};


/**
****************************************************************************
* Class: CIccStructBRDF
* 
* Purpose: The BRDF struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructBRDF : public CIccStructUnknown
{
public:
  CIccStructBRDF(CIccTagStruct *pTagStruct=NULL);
  virtual ~CIccStructBRDF();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructBRDF"; }
  virtual const icChar *GetDisplayName() const { return "brdfTransformStructure"; }

  virtual icSigBRDFType GetBRDFType() const;

  virtual icSigBRDFFunction GetBRDFFunction() const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

protected:

};


/**
****************************************************************************
* Class: CIccStructColorEncodingParams
* 
* Purpose: The colorEncodingParamsStructure handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructColorEncodingParams : public CIccStructUnknown
{
public:
  CIccStructColorEncodingParams(CIccTagStruct *pTagStruct=NULL);
  virtual ~CIccStructColorEncodingParams();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructColorEncodingParams"; }
  virtual const icChar *GetDisplayName() const { return "colorEncodingParamsStructure"; }

protected:

};


/**
****************************************************************************
* Class: CIccStructColorantInfo
*
* Purpose: The Named Color struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructColorantInfo : public CIccStructUnknown
{
public:
  CIccStructColorantInfo(CIccTagStruct *pTagStruct = NULL);
  virtual ~CIccStructColorantInfo();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructColorantInfo"; }
  virtual const icChar *GetDisplayName() const { return "colorantInfoStructure"; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

protected:

};


/**
****************************************************************************
* Class: CIccStructMeasurementInfo
*
* Purpose: The Named Color struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructMeasurementInfo : public CIccStructUnknown
{
public:
  CIccStructMeasurementInfo(CIccTagStruct *pTagStruct = NULL);
  virtual ~CIccStructMeasurementInfo();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructMeasurementInfo"; }
  virtual const icChar *GetDisplayName() const { return "measurementInfoStructure"; }

protected:

};


/**
****************************************************************************
* Class: CIccStructProfileInfo
*
* Purpose: The Named Color struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructProfileInfo : public CIccStructUnknown
{
public:
  CIccStructProfileInfo(CIccTagStruct *pTagStruct = NULL);
  virtual ~CIccStructProfileInfo();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructProfileInfo"; }
  virtual const icChar *GetDisplayName() const { return "profileInfoStructure"; }

protected:

};


/**
****************************************************************************
* Class: CIccStructTintZero
*
* Purpose: The Named Color struct handler
*****************************************************************************
*/
class ICCPROFLIB_API CIccStructTintZero : public CIccStructNamedColor
{
public:
  CIccStructTintZero(CIccTagStruct *pTagStruct = NULL);
  virtual ~CIccStructTintZero();
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const;

  virtual const icChar *GetClassName() const { return "CIccStructTintZero"; }
  virtual const icChar *GetDisplayName() const { return "tintZeroStructure"; }

protected:

};


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCSTRUCTBASIC_H)
