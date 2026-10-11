/** @file
File:       IccTagProfSeqId.h

Contains:   Header for implementation of CIccTagProfSeqId
and supporting classes

Version:    V1

Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Jun 3, 2007 
//  Initial CIccTagProfSeqId development
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCTAGPROFSEQID_H
#define _ICCTAGPROFSEQID_H

#include "IccProfile.h"
#include "IccTag.h"
#include <memory>
#include <list>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class ICCPROFLIB_API CIccProfileIdDesc
{
public:
  CIccProfileIdDesc();
  CIccProfileIdDesc(CIccProfile &profile);
  CIccProfileIdDesc(icProfileID id, CIccTagMultiLocalizedUnicode desc);
  CIccProfileIdDesc(const CIccProfileIdDesc &pid);
  CIccProfileIdDesc &operator=(const CIccProfileIdDesc &pid);

  void Describe(std::string &sDescription, int nVerboseness);

  bool Read(icUInt32Number size, CIccIO *pIO);
  bool Write(CIccIO *pIO);

  icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  CIccTagMultiLocalizedUnicode m_desc;
  icProfileID m_profileID;
};

typedef std::list<CIccProfileIdDesc> CIccProfileIdDescList;

/**
****************************************************************************
* Class: CIccTagProfileSequenceId
* 
* Purpose: The ProfileSequenceId tag 
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagProfileSequenceId : public CIccTag
{
public:
  CIccTagProfileSequenceId();
  CIccTagProfileSequenceId(const CIccTagProfileSequenceId &lut);
  CIccTagProfileSequenceId &operator=(const CIccTagProfileSequenceId &lut);
  virtual CIccTag *NewCopy() const { return new CIccTagProfileSequenceId(*this);}
  virtual ~CIccTagProfileSequenceId();

  static CIccTagProfileSequenceId *ParseMem(icUInt8Number *pMem, icUInt32Number size);

  virtual icTagTypeSignature GetType() const { return icSigProfileSequceIdType; }
  virtual const icChar *GetClassName() const { return "CIccTagProfileSequenceId"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  bool AddProfileDescription(CIccProfile &profile) { return AddProfileDescription(CIccProfileIdDesc(profile)); }
  bool AddProfileDescription(const CIccProfileIdDesc &profileDesc);

  CIccProfileIdDesc *GetFirst();
  CIccProfileIdDesc *GetLast();

  CIccProfileIdDescList::iterator begin() { return m_list->begin(); }
  CIccProfileIdDescList::iterator end() { return m_list->end(); }

protected:
  void Cleanup();

  CIccProfileIdDescList *m_list;
};



//CIccTagProfSeq support
#ifdef USEICCDEVNAMESPACE
}
#endif

#endif //_ICCTAGPROFSEQID_H
