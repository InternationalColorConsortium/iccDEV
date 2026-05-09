/** @file
    File:       IccCmmThread.h

    Contains:   Header for threaded CMM apply support via CIccThreadedCmm.

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
// -Initial implementation by Max Derhak
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCCMMTHREAD_H)
#define _ICCCMMTHREAD_H

#include "IccCmm.h"
#include <vector>

#ifdef __cplusplus

// Forward reference
class CIccThreadedCmm;

/**
 **************************************************************************
 * Type: Class
 *
 * Purpose:
 *  Apply object for CIccThreadedCmm. Partitions a multi-pixel Apply call
 *  across N worker threads, each owning a private CIccApplyCmm so that
 *  no CIccXform state is shared between threads.
 *
 *  Single-pixel Apply() is forwarded to the first worker, which is also
 *  used by the calling thread for the last strip in multi-pixel calls.
 *
 *  A single CIccApplyThreadedCmm instance must NOT be called concurrently
 *  from more than one thread; obtain separate instances via
 *  CIccThreadedCmm::GetNewApplyCmm() for concurrent use.
 **************************************************************************
 */
class ICCPROFLIB_API CIccApplyThreadedCmm : public CIccApplyCmm
{
  friend class CIccThreadedCmm;
public:
  virtual ~CIccApplyThreadedCmm();

  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel);
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels);

protected:
  CIccApplyThreadedCmm(CIccCmm *pCmm);
  bool Init(CIccCmm *pCmm, int nThreads);

  std::vector<CIccApplyCmm*> m_workers;
  int m_nThreads;
};


/**
 **************************************************************************
 * Type: Class
 *
 * Purpose:
 *  Decorator CMM that applies multi-pixel transforms in parallel using a
 *  thread-per-strip strategy.  Each strip is processed by a private
 *  CIccApplyCmm so transforms never share mutable state.
 *
 *  The wrapped CMM must already have had Begin() called successfully.
 *  Use the static Attach() factory to create instances.
 *
 *  Typical usage:
 *    CIccCmm *cmm = new CIccCmm(...);
 *    cmm->AddXform(...);
 *    if (cmm->Begin() != icCmmStatOk) { ... }
 *
 *    // nThreads=0 -> std::thread::hardware_concurrency()
 *    CIccThreadedCmm *tcmm = CIccThreadedCmm::Attach(cmm);
 *    if (!tcmm) { ... }   // cmm deleted on failure
 *
 *    tcmm->Apply(dst, src, nPixels);
 *    delete tcmm;
 *
 *  For concurrent apply from multiple call sites, each caller should
 *  obtain its own CIccApplyThreadedCmm via GetNewApplyCmm().
 **************************************************************************
 */
class ICCPROFLIB_API CIccThreadedCmm : public CIccCmm
{
  friend class CIccApplyThreadedCmm;
private:
  CIccThreadedCmm();
public:
  virtual ~CIccThreadedCmm();

  // Attach to a Begin()-ed CMM.  nThreads=0 uses hardware_concurrency.
  // Returns NULL on failure; pCmm is deleted on failure when bDeleteCmm=true.
  static CIccThreadedCmm* Attach(CIccCmm *pCmm, int nThreads=0, bool bDeleteCmm=true);

  // AddXform/Begin are disabled - setup must be done on the wrapped CMM before Attach().
  virtual icStatusCMM AddXform(const icChar *, icRenderingIntent, icXformInterp,
                               IIccProfileConnectionConditions*, icXformLutType,
                               bool, CIccCreateXformHintManager*, bool)     { return icCmmStatBad; }
  virtual icStatusCMM AddXform(icUInt8Number *, icUInt32Number, icRenderingIntent,
                               icXformInterp, IIccProfileConnectionConditions*,
                               icXformLutType, bool, CIccCreateXformHintManager*, bool) { return icCmmStatBad; }
  virtual icStatusCMM AddXform(CIccProfile *, icRenderingIntent, icXformInterp,
                               IIccProfileConnectionConditions*, icXformLutType,
                               bool, CIccCreateXformHintManager*)           { return icCmmStatBad; }
  virtual icStatusCMM AddXform(CIccProfile &, icRenderingIntent, icXformInterp,
                               IIccProfileConnectionConditions*, icXformLutType,
                               bool, CIccCreateXformHintManager*)           { return icCmmStatBad; }
  virtual icStatusCMM AddXform(CIccProfile *, CIccTag *, icRenderingIntent,
                               icXformInterp, IIccProfileConnectionConditions*,
                               bool, CIccCreateXformHintManager*)           { return icCmmStatBad; }
  virtual icStatusCMM AddXform(CIccXform *)                                 { return icCmmStatBad; }

  virtual CIccApplyCmm *GetNewApplyCmm(icStatusCMM &status);

  virtual icStatusCMM RemoveAllIO()             { return m_pCmm->RemoveAllIO(); }
  virtual icUInt32Number GetNumXforms() const   { return m_pCmm->GetNumXforms(); }
  virtual icColorSpaceSignature GetFirstXformSource() { return m_pCmm->GetFirstXformSource(); }
  virtual icColorSpaceSignature GetLastXformDest()    { return m_pCmm->GetLastXformDest(); }

  int GetNumThreads() const { return m_nThreads; }

protected:
  CIccCmm *m_pCmm;
  int      m_nThreads;
  bool     m_bDeleteCmm;
};

#endif // __cplusplus
#endif // !defined(_ICCCMMTHREAD_H)
