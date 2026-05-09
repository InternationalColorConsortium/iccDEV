/** @file
    File:       IccCmmThread.cpp

    Contains:   Implementation of CIccThreadedCmm and CIccApplyThreadedCmm.

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

#include "IccCmmThread.h"
#include <algorithm>
#include <future>
#include <new>
#include <thread>


//===========================================================================
// CIccApplyThreadedCmm
//===========================================================================

/**
 **************************************************************************
 * Name: CIccApplyThreadedCmm::CIccApplyThreadedCmm
 *
 * Purpose: Constructor - called only by CIccThreadedCmm::GetNewApplyCmm.
 **************************************************************************
 */
CIccApplyThreadedCmm::CIccApplyThreadedCmm(CIccCmm *pCmm) : CIccApplyCmm(pCmm)
{
  m_nThreads = 1;
}


/**
 **************************************************************************
 * Name: CIccApplyThreadedCmm::~CIccApplyThreadedCmm
 **************************************************************************
 */
CIccApplyThreadedCmm::~CIccApplyThreadedCmm()
{
  for (CIccApplyCmm *p : m_workers)
    delete p;
}


/**
 **************************************************************************
 * Name: CIccApplyThreadedCmm::Init
 *
 * Purpose:
 *  Allocates one CIccApplyCmm per thread from the underlying (non-threaded)
 *  CMM.  Must be called immediately after construction.
 *
 * Args:
 *  pCmm     - the underlying CIccCmm (not the CIccThreadedCmm wrapper)
 *  nThreads - number of worker objects to allocate
 **************************************************************************
 */
bool CIccApplyThreadedCmm::Init(CIccCmm *pCmm, int nThreads)
{
  m_nThreads = nThreads;
  m_workers.resize(nThreads, NULL);

  for (int i = 0; i < nThreads; i++) {
    icStatusCMM status;
    m_workers[i] = pCmm->GetNewApplyCmm(status);
    if (!m_workers[i] || status != icCmmStatOk)
      return false;
  }
  return true;
}


/**
 **************************************************************************
 * Name: CIccApplyThreadedCmm::Apply (single pixel)
 *
 * Purpose: Forwards single-pixel apply to worker[0].
 **************************************************************************
 */
icStatusCMM CIccApplyThreadedCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel)
{
  return m_workers[0]->Apply(DstPixel, SrcPixel);
}


/**
 **************************************************************************
 * Name: CIccApplyThreadedCmm::Apply (multi-pixel)
 *
 * Purpose:
 *  Partitions nPixels into up to m_nThreads contiguous strips and
 *  processes each strip on a separate thread.  The final strip is run on
 *  the calling thread to overlap its work with the async launches.
 *
 *  Strips differ in size by at most one pixel (remainder pixels are
 *  distributed one each to the first strips).
 *
 * Args:
 *  DstPixel - output buffer (must be large enough for nPixels * dstSamples)
 *  SrcPixel - input buffer
 *  nPixels  - number of pixels to process
 **************************************************************************
 */
icStatusCMM CIccApplyThreadedCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel,
                                         icUInt32Number nPixels)
{
  if (!nPixels)
    return icCmmStatOk;

  int nSrcSamples = m_pCmm->GetSourceSamples();
  int nDstSamples = m_pCmm->GetDestSamples();

  // Cap active thread count to actual pixel count
  int nActive = std::min((int)nPixels, m_nThreads);

  if (nActive <= 1)
    return m_workers[0]->Apply(DstPixel, SrcPixel, nPixels);

  icUInt32Number base  = nPixels / nActive;
  icUInt32Number extra = nPixels % nActive;  // first `extra` strips get one more pixel

  // Launch nActive-1 async strips; the last strip runs on the calling thread.
  std::vector<std::future<icStatusCMM>> futures;
  futures.reserve(nActive - 1);

  icUInt32Number offset = 0;
  for (int t = 0; t < nActive - 1; t++) {
    icUInt32Number count      = base + (t < (int)extra ? 1u : 0u);
    icFloatNumber *pDst       = DstPixel + offset * nDstSamples;
    const icFloatNumber *pSrc = SrcPixel + offset * nSrcSamples;
    CIccApplyCmm *pWorker     = m_workers[t];

    futures.push_back(std::async(std::launch::async,
      [pWorker, pDst, pSrc, count]() {
        return pWorker->Apply(pDst, pSrc, count);
      }));

    offset += count;
  }

  // Last strip on calling thread using workers[nActive-1]
  icStatusCMM rv = m_workers[nActive - 1]->Apply(
    DstPixel + offset * nDstSamples,
    SrcPixel + offset * nSrcSamples,
    nPixels - offset);

  // Collect async results; preserve first non-OK status
  for (auto &f : futures) {
    icStatusCMM r = f.get();
    if (rv == icCmmStatOk && r != icCmmStatOk)
      rv = r;
  }

  return rv;
}


//===========================================================================
// CIccThreadedCmm
//===========================================================================

/**
 **************************************************************************
 * Name: CIccThreadedCmm::CIccThreadedCmm
 *
 * Purpose: Private constructor - use Attach() to create instances.
 **************************************************************************
 */
CIccThreadedCmm::CIccThreadedCmm() : CIccCmm()
{
  m_pCmm       = NULL;
  m_nThreads   = 1;
  m_bDeleteCmm = false;
}


/**
 **************************************************************************
 * Name: CIccThreadedCmm::~CIccThreadedCmm
 **************************************************************************
 */
CIccThreadedCmm::~CIccThreadedCmm()
{
  if (m_pCmm && m_bDeleteCmm)
    delete m_pCmm;
}


/**
 **************************************************************************
 * Name: CIccThreadedCmm::Attach
 *
 * Purpose:
 *  Creates a CIccThreadedCmm that wraps a Begin()-ed CIccCmm.  The
 *  wrapped CMM's Begin() must have already succeeded before calling Attach.
 *
 * Args:
 *  pCmm       - pointer to the source CMM; must be valid (Begin() succeeded)
 *  nThreads   - thread count; 0 means std::thread::hardware_concurrency()
 *  bDeleteCmm - if true, pCmm is owned and deleted with this object
 *               (and also deleted on failure)
 *
 * Return:
 *  New CIccThreadedCmm on success, NULL on failure.
 **************************************************************************
 */
CIccThreadedCmm* CIccThreadedCmm::Attach(CIccCmm *pCmm, int nThreads, bool bDeleteCmm)
{
  if (!pCmm || !pCmm->Valid()) {
    if (bDeleteCmm)
      delete pCmm;
    return NULL;
  }

  int nActual = nThreads > 0 ? nThreads : (int)std::thread::hardware_concurrency();
  if (nActual < 1)
    nActual = 1;

  CIccThreadedCmm *rv = new CIccThreadedCmm();
  rv->m_pCmm       = pCmm;
  rv->m_nThreads   = nActual;
  rv->m_bDeleteCmm = bDeleteCmm;

  // Copy color-space fields into the base so GetSourceSamples/GetDestSamples work.
  rv->m_nSrcSpace   = pCmm->GetSourceSpace();
  rv->m_nDestSpace  = pCmm->GetDestSpace();
  rv->m_nLastSpace  = pCmm->GetLastSpace();
  rv->m_nLastIntent = pCmm->GetLastIntent();

  // Allocate the default apply object (used by CIccCmm::Apply() forwarders).
  icStatusCMM status;
  rv->m_pApply = rv->GetNewApplyCmm(status);
  if (!rv->m_pApply || status != icCmmStatOk) {
    delete rv;   // bDeleteCmm controls pCmm deletion inside ~CIccThreadedCmm
    return NULL;
  }
  rv->m_bValid = true;

  return rv;
}


/**
 **************************************************************************
 * Name: CIccThreadedCmm::GetNewApplyCmm
 *
 * Purpose:
 *  Allocates a CIccApplyThreadedCmm with m_nThreads worker apply objects.
 *  Multiple instances can be used concurrently against this CMM.
 **************************************************************************
 */
CIccApplyCmm *CIccThreadedCmm::GetNewApplyCmm(icStatusCMM &status)
{
  CIccApplyThreadedCmm *pApply = new (std::nothrow) CIccApplyThreadedCmm(this);
  if (!pApply) {
    status = icCmmStatAllocErr;
    return NULL;
  }

  // Init allocates worker apply objects from the underlying (non-threaded) CMM.
  if (!pApply->Init(m_pCmm, m_nThreads)) {
    delete pApply;
    status = icCmmStatAllocErr;
    return NULL;
  }

  status = icCmmStatOk;
  return pApply;
}
