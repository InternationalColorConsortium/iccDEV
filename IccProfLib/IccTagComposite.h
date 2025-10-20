/** @file
File:       IccTagComposite.h

Contains:   Header for implementation of CIccTagStruct and CIccTagArray
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
// -June 8, 2011 
//  Initial prototype development
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCTAGCOMPOSITE_H
#define _ICCTAGCOMPOSITE_H

#include "IccProfile.h"
#include "IccTag.h"
#include "IccDefs.h"
#include <memory>
#include <list>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccTagStruct;
class CIccTagArray;

/**
****************************************************************************
* Class: IIccStruct
* 
* Purpose: The interface for Tag Struct handler
*****************************************************************************
*/
class ICCPROFLIB_API IIccStruct
{
public:
  virtual ~IIccStruct() {}
  virtual IIccStruct* NewCopy(CIccTagStruct *pNewTagStruct) const = 0;

  virtual void Describe(std::string &sDescription, int nVerboseness) const;

  virtual const icChar *GetClassName() const = 0;
  virtual const icChar *GetDisplayName() const = 0;

  CIccTagStruct* GetTagStruct() { return m_pTagStruct; }

  virtual std::string GetElemName(icSignature sigElem) const = 0;
  virtual icSignature GetElemSig(const icChar *szElemName) const = 0;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const = 0;

protected:

  TagEntryList *getTagEntries() const;

  CIccTagStruct *m_pTagStruct;
};


/**
****************************************************************************
* Class: IIccArray
* 
* Purpose: The interface for Tag Array handler
*****************************************************************************
*/
class ICCPROFLIB_API IIccArray
{
public:
  virtual ~IIccArray() {}
  virtual IIccArray* NewCopy(CIccTagArray *pNewTagArray) const = 0;

  virtual void Describe(std::string &sDescription, int nVerboseness) const { }

  virtual const icChar *GetClassName() const = 0;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const = 0; 

  CIccTagArray* GetTagArray() { return m_pTagArray; }

protected:
  CIccTagArray *m_pTagArray;
};


/**
****************************************************************************
* Class: CIccTagStruct
* 
* Purpose: The general purpose Tag Struct type tag 
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagStruct : public CIccTag
{
  friend class IIccStruct;
public:
  CIccTagStruct();
  CIccTagStruct(const CIccTagStruct &lut);
  CIccTagStruct &operator=(const CIccTagStruct &lut);
  virtual CIccTag *NewCopy() const { return new CIccTagStruct(*this);}
  virtual ~CIccTagStruct();

  static CIccTagStruct *ParseMem(icUInt8Number *pMem, icUInt32Number size);

  virtual icTagTypeSignature GetType() const { return icSigTagStructType; }
  virtual icStructSignature GetTagStructType() const { return m_sigStructType; }

  virtual bool SetTagStructType(icStructSignature sig);

  virtual const icChar *GetClassName() const { return "CIccTagStruct"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  TagEntryList *GetElemList() { return m_ElemEntries; }

  CIccTag* FindElem(icSignature sig);
  CIccTag* FindElemOfType(icSignature sig, icTagTypeSignature sigType);

  icFloatNumber GetElemNumberValue(icSignature sig, icFloatNumber defaultValue=0);

  bool AttachElem(icSignature sig, CIccTag *pTag);
  bool DeleteElem(icSignature sig);

  bool AreElemsUnique() const;

  static std::string GetElemName(icTagSignature sig, icStructSignature sigThis);

  IIccStruct *GetStructHandler();

protected:
  IIccStruct *m_pStruct; //Note: The CIccTagStruct will delete the m_pStruct in destructor

  void Cleanup();
  IccTagEntry* GetElem(icSignature sig) const;
  IccTagEntry* GetElem(CIccTag *pTag) const;

  bool LoadElem(IccTagEntry *pTagEntry, CIccIO *pIO);
  bool DetachElem(CIccTag *pTag);
  
  icStructSignature m_sigStructType;

  icUInt32Number m_tagStart;
  icUInt32Number m_tagSize;
  
  TagEntryList *m_ElemEntries;

  TagPtrList *m_ElemVals;
};


/**
****************************************************************************
* Class: CIccTagArray
* 
* Purpose: The Tag Array tag 
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagArray : public CIccTag
{
public:
  CIccTagArray();
  CIccTagArray(icArraySignature sigArrayType);
  CIccTagArray(const CIccTagArray &lut);
  CIccTagArray &operator=(const CIccTagArray &lut);
  virtual CIccTag *NewCopy() const { return new CIccTagArray(*this);}
  virtual ~CIccTagArray();

  static CIccTagArray *ParseMem(icUInt8Number *pMem, icUInt32Number size);

  virtual icTagTypeSignature GetType() const { return icSigTagArrayType; }
  virtual icArraySignature GetTagArrayType() const { return m_sigArrayType; }

  virtual bool SetTagArrayType(icArraySignature sig);

  bool AreAllOfType(icTagTypeSignature sigTagType);

  virtual const icChar *GetClassName() const { return "CIccTagArray"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  bool AttachTag(icUInt32Number nIndex, CIccTag *pTag);
  CIccTag * DetachTag(icUInt32Number nIndex, bool bDeleteFlag=false);

  CIccTag *GetIndex(icUInt32Number index) const;
  CIccTag *operator[](icUInt32Number index) const { return GetIndex(index); }

  /// Returns the size of the data array
  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize);

  IIccArray* GetArrayHandler();

protected:
  IIccArray *m_pArray;  //Note: The CIccTagArray will delete the m_pArray in destructor

  void Cleanup();
  
  icArraySignature m_sigArrayType;

  icUInt32Number m_nSize;
  IccTagPtr *m_TagVals;
};

ICCPROFLIB_API IIccStruct* icGetTagStructHandler(CIccTag* pTag);
ICCPROFLIB_API IIccStruct* icGetTagStructHandlerOfType(CIccTag* pTag, icStructSignature sig);
ICCPROFLIB_API IIccArray* icGetTagArrayHandler(CIccTag* pTag);
ICCPROFLIB_API IIccArray* icGetTagArrayHandlerOfType(CIccTag* pTag, icStructSignature sig);


#ifdef USEICCDEVNAMESPACE
}
#endif

#endif //_ICCTAGCOMPOSITE_H
