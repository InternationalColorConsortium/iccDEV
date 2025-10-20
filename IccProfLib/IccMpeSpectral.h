/** @file
File:       IccMpeSpectral.h

Contains:   Header for implementation of Basic CIccTagMPE elements
            and supporting classes

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
// -Oct 23, 2015 
//  Initial CIccMpe Spectral development
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCELEMSPECTRAL_H
#define _ICCELEMSPECTRAL_H

#include "IccTagMPE.h"


//CIccFloatTag support
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//declare class for referencing by CIccMpeInvEmissionMatrix
class CIccMathMatrix;

/**
****************************************************************************
* Class: CIccMpeSpectralMatrix
* 
* Purpose: The Observed Emission Matrix Element
*****************************************************************************
*/
class CIccMpeSpectralMatrix : public CIccMultiProcessElement
{
public:
  CIccMpeSpectralMatrix();
  CIccMpeSpectralMatrix(const CIccMpeSpectralMatrix &ITPC);
  virtual ~CIccMpeSpectralMatrix();

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  const icSpectralRange &GetRange() { return m_Range;}

  bool SetSize(icUInt16Number nInputChannels, icUInt16Number nOutputChannels, const icSpectralRange &range );

  icFloatNumber *GetWhite() { return m_pWhite;}
  icFloatNumber *GetMatrix() {return m_pMatrix;}
  icFloatNumber *GetOffset() {return m_pOffset;}

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE) = 0;
  virtual void Apply(CIccApplyMpe *pApply, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const = 0;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE=NULL, const CIccProfile* pProfile = NULL) const;

  virtual bool IsLateBinding() const { return true; }

protected:
  void copyData(const CIccMpeSpectralMatrix &ITPC);
  virtual const char *GetDescribeName() const = 0;
  virtual icUInt16Number numVectors() const = 0;

  icSpectralRange m_Range;
  icUInt16Number m_nReserved2;

  icFloatNumber *m_pWhite;
  icFloatNumber *m_pMatrix;
  icFloatNumber *m_pOffset;
  icUInt32Number m_size;

  CIccMatrixMath *m_pApplyMtx;
  icFloatNumber m_xyzOffset[3];
};



/**
****************************************************************************
* Class: CIccMpeEmissionMatrix
* 
* Purpose: The Observed Emission Matrix Element
*****************************************************************************
*/
class CIccMpeEmissionMatrix : public CIccMpeSpectralMatrix
{
public:
  CIccMpeEmissionMatrix() : CIccMpeSpectralMatrix() {}
  CIccMpeEmissionMatrix(const CIccMpeEmissionMatrix &ITPC) : CIccMpeSpectralMatrix(ITPC) {}
  CIccMpeEmissionMatrix &operator=(const CIccMpeEmissionMatrix &ITPC) { copyData(ITPC); return *this; }
  virtual CIccMultiProcessElement *NewCopy() const { return new CIccMpeEmissionMatrix(*this);}
  virtual ~CIccMpeEmissionMatrix() {}

  virtual icElemTypeSignature GetType() const { return icSigEmissionMatrixElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeEmissionMatrix"; }

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);
  virtual void Apply(CIccApplyMpe *pApply, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const;

protected:
  virtual const char *GetDescribeName() const { return "ELEM_OBS_EMIS_MATRIX"; }
  virtual icUInt16Number numVectors() const { return m_nInputChannels; }
};

/**
****************************************************************************
* Class: CIccMpeInvEmissionMatrix
* 
* Purpose: The Inverse Observed Emission Matrix Element
*****************************************************************************
*/
class CIccMpeInvEmissionMatrix : public CIccMpeSpectralMatrix
{
public:
  CIccMpeInvEmissionMatrix() : CIccMpeSpectralMatrix() {}
  CIccMpeInvEmissionMatrix(const CIccMpeInvEmissionMatrix &ITPC) : CIccMpeSpectralMatrix(ITPC) {}
  CIccMpeInvEmissionMatrix &operator=(const CIccMpeInvEmissionMatrix &ITPC) { copyData(ITPC); return *this; }
  virtual CIccMultiProcessElement *NewCopy() const { return new CIccMpeInvEmissionMatrix(*this);}
  virtual ~CIccMpeInvEmissionMatrix() {}

  virtual icElemTypeSignature GetType() const { return icSigInvEmissionMatrixElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeEmissionMatrix"; }

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);
  virtual void Apply(CIccApplyMpe *pApply, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE=NULL, const CIccProfile* pProfile = NULL) const;

protected:
  virtual const char *GetDescribeName() const { return "ELEM_INV_OBS_EMIS_MATRIX"; }
  virtual icUInt16Number numVectors() const { return m_nOutputChannels; }
};

/**
****************************************************************************
* Class: CIccMpeSpectralCLUT
* 
* Purpose: The float Color LookUp Table tag
*****************************************************************************
*/
class CIccMpeSpectralCLUT : public CIccMultiProcessElement
{
public:
  CIccMpeSpectralCLUT();
  CIccMpeSpectralCLUT(const CIccMpeSpectralCLUT &ITPC);
  virtual ~CIccMpeSpectralCLUT();

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual CIccApplyMpe* GetNewApply(CIccApplyTagMpe* pApplyTag);

  virtual void Apply(CIccApplyMpe *pApply, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE=NULL, const CIccProfile* pProfile = NULL) const;

  void SetData(CIccCLUT *pCLUT, icUInt16Number nStorageType, 
               const icSpectralRange &range, icFloatNumber *pWhite,
               icUInt16Number nOutputChannels=3);
  CIccCLUT *GetCLUT() { return m_pCLUT; }
  CIccCLUT *GetApplyCLUT() { return m_pApplyCLUT; }
  icFloatNumber *GetWhite() { return m_pWhite; }

  virtual bool IsLateBinding() const { return true; }

protected:
  void copyData(const CIccMpeSpectralCLUT &ITPC);
  virtual const char *GetDescribeName() const = 0;

  icSpectralRange m_Range;
  icUInt16Number m_nStorageType;
  icUInt32Number m_flags;

  icFloatNumber *m_pWhite;

  CIccCLUT *m_pCLUT;
  icCLUTElemType m_interpType;

  CIccCLUT *m_pApplyCLUT;
};

/**
****************************************************************************
*Class: CIccApplyMpeSpectralCLUT
*
*Purpose : The SpectralCLUT element apply data
*****************************************************************************
*/
class CIccApplyMpeSpectralCLUT : public CIccApplyMpe
{
  friend class CIccMpeSpectralCLUT;
public:
  virtual ~CIccApplyMpeSpectralCLUT();

  virtual icElemTypeSignature GetType() const { return m_pElem->GetType(); }
  virtual const icChar* GetClassName() const { return "CIccApplyMpeSpectralCLUT"; }

protected:
  CIccApplyMpeSpectralCLUT(CIccMultiProcessElement* pElem, CIccApplyCLUT *pApply);

  CIccApplyCLUT* m_pApply;
};



/**
****************************************************************************
* Class: CIccMpeEmissionCLUT
* 
* Purpose: The Emission CLUT Element
*****************************************************************************
*/
class CIccMpeEmissionCLUT : public CIccMpeSpectralCLUT
{
public:
  CIccMpeEmissionCLUT() : CIccMpeSpectralCLUT() {}
  CIccMpeEmissionCLUT(const CIccMpeEmissionCLUT &ITPC) : CIccMpeSpectralCLUT(ITPC) {}
  CIccMpeEmissionCLUT &operator=(const CIccMpeEmissionCLUT &ITPC) { copyData(ITPC); return *this; }
  virtual CIccMultiProcessElement *NewCopy() const { return new CIccMpeEmissionCLUT(*this);}
  virtual ~CIccMpeEmissionCLUT() {}

  virtual icElemTypeSignature GetType() const { return icSigEmissionCLUTElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeEmissionCLUT"; }

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);

protected:
  virtual const char *GetDescribeName() const { return "ELEM_EMISSION_CLUT"; }
};


/**
****************************************************************************
* Class: CIccMpeReflectanceCLUT
* 
* Purpose: The Reflectance CLUT Element
*****************************************************************************
*/
class CIccMpeReflectanceCLUT : public CIccMpeSpectralCLUT
{
public:
  CIccMpeReflectanceCLUT() : CIccMpeSpectralCLUT() {}
  CIccMpeReflectanceCLUT(const CIccMpeReflectanceCLUT &ITPC) : CIccMpeSpectralCLUT(ITPC) {}
  CIccMpeReflectanceCLUT &operator=(const CIccMpeReflectanceCLUT &ITPC) { copyData(ITPC); return *this; }
  virtual CIccMpeReflectanceCLUT *NewCopy() const { return new CIccMpeReflectanceCLUT(*this);}
  virtual ~CIccMpeReflectanceCLUT() {}

  virtual icElemTypeSignature GetType() const { return icSigReflectanceCLUTElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeReflectanceCLUT"; }

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);

  virtual bool IsLateBindingReflectance() const { return true; }

protected:
  virtual const char *GetDescribeName() const { return "ELEM_REFLECTANCE_CLUT"; }
};


/**
****************************************************************************
* Class: CIccMpeSpectralObserver
* 
* Purpose: The Spectral Observer Element
*****************************************************************************
*/
class CIccMpeSpectralObserver : public CIccMultiProcessElement
{
public:
  CIccMpeSpectralObserver();
  CIccMpeSpectralObserver(const CIccMpeSpectralObserver &ITPC);
  virtual ~CIccMpeSpectralObserver();

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  const icSpectralRange &GetRange() { return m_Range;}

  bool SetSize(icUInt16Number nInputChannels, icUInt16Number nOutputChannels, const icSpectralRange &range );

  icFloatNumber *GetWhite() { return m_pWhite;}

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE) = 0;
  virtual void Apply(CIccApplyMpe *pApply, icFloatNumber *dstPixel, const icFloatNumber *srcPixel) const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE=NULL, const CIccProfile* pProfile = NULL) const;

  virtual bool IsLateBinding() const { return true; }

protected:
  void copyData(const CIccMpeSpectralObserver &ITPC);
  virtual const char *GetDescribeName() const = 0;

  icSpectralRange m_Range;
  icUInt16Number m_flags;

  icFloatNumber *m_pWhite;

  CIccMatrixMath *m_pApplyMtx;
  icFloatNumber m_xyzw[3];
  icFloatNumber m_xyzscale[3];
};


/**
****************************************************************************
* Class: CIccMpeEmissionObserver
* 
* Purpose: The Emission Observer Element
*****************************************************************************
*/
class CIccMpeEmissionObserver : public CIccMpeSpectralObserver
{
public:
  CIccMpeEmissionObserver() : CIccMpeSpectralObserver() {}
  CIccMpeEmissionObserver(const CIccMpeEmissionObserver &ITPC) : CIccMpeSpectralObserver(ITPC) {}
  CIccMpeEmissionObserver &operator=(const CIccMpeEmissionObserver &ITPC) { copyData(ITPC); return *this; }
  virtual CIccMultiProcessElement *NewCopy() const { return new CIccMpeEmissionObserver(*this);}
  virtual ~CIccMpeEmissionObserver() {}

  virtual icElemTypeSignature GetType() const { return icSigEmissionObserverElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeEmissionObserver"; }

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);

protected:
  virtual const char *GetDescribeName() const { return "ELEM_EMISSION_OBSERVER"; }
};


/**
****************************************************************************
* Class: CIccMpeReflectanceObserver
* 
* Purpose: The Reflectance Observer Element
*****************************************************************************
*/
class CIccMpeReflectanceObserver : public CIccMpeSpectralObserver
{
public:
  CIccMpeReflectanceObserver() : CIccMpeSpectralObserver() {}
  CIccMpeReflectanceObserver(const CIccMpeReflectanceObserver &ITPC) : CIccMpeSpectralObserver(ITPC) {}
  CIccMpeReflectanceObserver &operator=(const CIccMpeReflectanceObserver &ITPC) { copyData(ITPC); return *this; }
  virtual CIccMpeReflectanceObserver *NewCopy() const { return new CIccMpeReflectanceObserver(*this);}
  virtual ~CIccMpeReflectanceObserver() {}

  virtual icElemTypeSignature GetType() const { return icSigReflectanceObserverElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeReflectanceObserver"; }

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);

  virtual bool IsLateBindingReflectance() const { return true; }

protected:
  virtual const char *GetDescribeName() const { return "ELEM_REFLECTANCE_OBSERVER"; }
};


//CIccMPElements support
#ifdef USEICCDEVNAMESPACE
}
#endif



#endif //_ICCELEMSPECTRAL_H
