/** @file
File:       IccTagEmbedIcc.h

Contains:   Header for implementation of the CIccTagEmbedProfile class
and inherited classes

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
// -Initial implementation by Max Derhak 3-9-2018
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCTAGEMBEDICC_H)
#define _ICCTAGEMBEDICC_H

#include "IccTagBasic.h"
#include "IccDefs.h"
#include "IccProfile.h"
/**
****************************************************************************
* Class: IccTagEmbeddedProfile
*
* Purpose: This "private" tag is used to embed a profile within a profile
* allowing for v5 profiles to be embedded in v2/v4 profiles
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagEmbeddedProfile : public CIccTag
{
public:
  CIccTagEmbeddedProfile();
  CIccTagEmbeddedProfile(const CIccTagEmbeddedProfile &ITU);
  CIccTagEmbeddedProfile &operator=(const CIccTagEmbeddedProfile &UnknownTag);
  virtual CIccTag* NewCopy() const { return new CIccTagEmbeddedProfile(*this); }
  virtual ~CIccTagEmbeddedProfile();

  virtual bool IsSupported() { return true; }

  virtual icTagTypeSignature GetType() const { return icSigEmbeddedProfileType; }
  virtual const icChar *GetClassName() const { return "CIccTagEmbeddedProfile"; }

  virtual CIccProfile *GetProfile() const { return m_pProfile; }
  virtual void SetProfile(CIccProfile *pProfile); //The tag will assume ownership of pProfile

  virtual bool Read(icUInt32Number size, CIccIO *pIO) { return Read(size, pIO, NULL); }
  virtual bool Read(icUInt32Number size, CIccIO *pIO, CIccProfile *pProfile);

  virtual bool ReadAll();

  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

  //The m_pProfile is owned by this tag
  CIccProfile *m_pProfile;
};

#endif
