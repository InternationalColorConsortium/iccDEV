/*
    File:       IccConnect.h

    Contains:   Factory class for constructing initialized ICC CMM transforms
                from JSON-based configuration objects.

    Version:    V1

    Copyright:  (c) see below
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
 // -Initial implementation by Max Derhak 4-2026
 //
 //////////////////////////////////////////////////////////////////////

#ifndef _ICCCONNECT_H
#define _ICCCONNECT_H

#include "IccCmmConfig.h"
#include "IccCmm.h"
#include "IccCmmSearch.h"
#include <list>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

typedef std::list<CIccProfile*> IccProfilePtrList;
class CIccThreadedCmm;

/**
 * CIccConnectCmm provides factory methods for constructing fully initialized
 * ICC CMM transform objects from CIccCfg* configuration objects (which can
 * be loaded from JSON or command-line arguments).  Calling a factory method
 * handles profile loading, hint setup, PCC lifecycle management, and Begin().
 */
class CIccConnectCmm
{
public:
  virtual ~CIccConnectCmm();

  // Creates a named-color CMM from a profile sequence configuration.
  // srcSpace: hint for source color space (icSigUnknownData = auto-detect)
  // bInputProfile: true if the transform starts from a device (input) profile
  static CIccConnectCmm* CreateNamed(const CIccCfgProfileSequence& profiles,
                                      icColorSpaceSignature srcSpace = icSigUnknownData,
                                      bool bInputProfile = true);

  // Creates a standard CMM from a profile sequence configuration.
  // pEmbeddedData/nEmbeddedLen: when provided and the first profile config has
  // an empty m_iccFile, these embedded ICC profile bytes are used for the first xform.
  static CIccConnectCmm* CreateStandard(const CIccCfgProfileSequence& profiles,
                                          const unsigned char* pEmbeddedData = nullptr,
                                          unsigned int nEmbeddedLen = 0,
                                          int nThreads = 1);

  // Creates a spectral search CMM from a search apply configuration.
  static CIccConnectCmm* CreateSearch(const CIccCfgSearchApply& searchApply);

  // Wraps an already-initialized CMM (takes ownership).
  static CIccConnectCmm* Attach(CIccCmm* pCmm);

  CIccCmm*            GetCmm()       const { return m_pCmm; }
  CIccNamedColorCmm*  GetNamedCmm()  const;
  CIccCmmSearch*      GetSearchCmm() const;
  CIccThreadedCmm*    GetThreadedCmm() const;
  bool                IsThreaded() const { return GetThreadedCmm() != nullptr; }

  icColorSpaceSignature GetSourceSpace() const;
  icColorSpaceSignature GetDestSpace()   const;

protected:
  CIccConnectCmm();

  // Prepares hints and PCC from a profile config entry and calls
  // pCmm->AddXform() via the file path.  pccList holds the PCC profile
  // pointers until after Begin() is called.
  static icStatusCMM AddXformFromConfig(CIccCmm* pCmm,
                                         CIccCfgProfile* pCfg,
                                         IccProfilePtrList& pccList);

  static void ReleasePccList(IccProfilePtrList& pccList);

  CIccCmm* m_pCmm;
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif //_ICCCONNECT_H
