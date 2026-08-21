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
#include <condition_variable>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>


static const int kIccMaxCmmThreads = 256;
static const icUInt32Number kIccSmallApplyPixels = 1024;
static const icUInt32Number kIccSmallApplyPixelsPerThread = 256;
static const icUInt32Number kIccBulkApplyPixelsPerThread = 128;

static int icResolveCmmThreadCount(int nThreads)
{
  if (nThreads < 0 || nThreads > kIccMaxCmmThreads)
    return 0;

  int nActual = nThreads > 0 ? nThreads : (int)std::thread::hardware_concurrency();
  if (nActual <= 0)
    return 1;

  return std::min(nActual, kIccMaxCmmThreads);
}

struct CIccApplyThreadedCmmTask
{
  CIccApplyCmm *m_worker;
  icFloatNumber *m_dst;
  const icFloatNumber *m_src;
  icUInt32Number m_count;
  icStatusCMM m_status;
};

class CIccApplyThreadedCmmPool
{
public:
  CIccApplyThreadedCmmPool()
  {
    m_jobHead = 0;
    m_jobCount = 0;
    m_pending = 0;
    m_stop = false;
  }

  ~CIccApplyThreadedCmmPool()
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_stop = true;
    }
    m_jobReady.notify_all();

    for (std::thread &worker : m_threads) {
      if (worker.joinable())
        worker.join();
    }
  }

  bool Init(int nThreads)
  {
    try {
      m_tasks.resize((size_t)std::max(0, nThreads - 1));
      m_jobs.resize(m_tasks.size());
      m_threads.reserve((size_t)std::max(0, nThreads - 1));
    }
    catch (const std::bad_alloc &) {
      return false;
    }

    return true;
  }

  icStatusCMM Apply(std::vector<CIccApplyCmm*> &workers,
                    icFloatNumber *dstPixel, const icFloatNumber *srcPixel,
                    icUInt32Number nPixels, int nActive,
                    int nSrcSamples, int nDstSamples)
  {
    if (!EnsureWorkerThreads(nActive - 1))
      return icCmmStatAllocErr;

    icUInt32Number base = nPixels / (icUInt32Number)nActive;
    icUInt32Number extra = nPixels % (icUInt32Number)nActive;
    icUInt32Number offset = 0;

    for (int t = 0; t < nActive - 1; t++) {
      icUInt32Number count = base + (t < (int)extra ? 1u : 0u);
      CIccApplyThreadedCmmTask &task = m_tasks[(size_t)t];
      task.m_worker = workers[(size_t)t];
      task.m_dst = dstPixel + offset * nDstSamples;
      task.m_src = srcPixel + offset * nSrcSamples;
      task.m_count = count;
      task.m_status = icCmmStatOk;
      offset += count;
    }

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_jobHead = 0;
      m_jobCount = (size_t)nActive - 1;
      m_pending = m_jobCount;
      for (size_t i = 0; i < m_jobCount; i++)
        m_jobs[i] = i;
    }

    for (int i = 0; i < nActive - 1; i++)
      m_jobReady.notify_one();

    icStatusCMM rv = workers[(size_t)nActive - 1]->Apply(
      dstPixel + offset * nDstSamples,
      srcPixel + offset * nSrcSamples,
      nPixels - offset);

    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_jobDone.wait(lock, [this]() { return m_pending == 0; });
    }

    for (int i = 0; i < nActive - 1; i++) {
      const CIccApplyThreadedCmmTask &task = m_tasks[(size_t)i];
      if (rv == icCmmStatOk && task.m_status != icCmmStatOk)
        rv = task.m_status;
    }

    return rv;
  }

private:
  bool EnsureWorkerThreads(int nThreads)
  {
    try {
      while ((int)m_threads.size() < nThreads)
        m_threads.push_back(std::thread(&CIccApplyThreadedCmmPool::WorkerLoop, this));
    }
    catch (const std::system_error &) {
      return false;
    }

    return true;
  }

  void WorkerLoop()
  {
    while (true) {
      size_t job = 0;
      CIccApplyThreadedCmmTask *task = NULL;

      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_jobReady.wait(lock, [this]() { return m_stop || m_jobCount != 0; });
        if (m_stop)
          return;

        job = m_jobs[m_jobHead];
        m_jobHead++;
        m_jobCount--;
        task = &m_tasks[job];
      }

      icStatusCMM status = task->m_worker->Apply(task->m_dst, task->m_src,
                                                 task->m_count);

      {
        std::lock_guard<std::mutex> lock(m_mutex);
        task->m_status = status;
        m_pending--;
        if (!m_pending)
          m_jobDone.notify_one();
      }
    }
  }

  std::vector<std::thread> m_threads;
  std::vector<CIccApplyThreadedCmmTask> m_tasks;
  std::vector<size_t> m_jobs;
  size_t m_jobHead;
  size_t m_jobCount;
  size_t m_pending;
  bool m_stop;
  std::mutex m_mutex;
  std::condition_variable m_jobReady;
  std::condition_variable m_jobDone;
};


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
  m_pool = NULL;
  m_baseCmm = NULL;
  m_nThreads = 1;
}


/**
 **************************************************************************
 * Name: CIccApplyThreadedCmm::~CIccApplyThreadedCmm
 **************************************************************************
 */
CIccApplyThreadedCmm::~CIccApplyThreadedCmm()
{
  delete m_pool;

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
  m_nThreads = icResolveCmmThreadCount(nThreads);
  if (m_nThreads <= 0 || m_nThreads > kIccMaxCmmThreads) {
    return false;
  }

  m_baseCmm = pCmm;
  try {
    m_workers.clear();
    m_workers.reserve((size_t)m_nThreads);
  }
  catch (const std::bad_alloc &) {
    return false;
  }

  if (!EnsureWorkers(1))
    return false;

  m_pool = new (std::nothrow) CIccApplyThreadedCmmPool();
  if (!m_pool || !m_pool->Init(m_nThreads)) {
    delete m_pool;
    m_pool = NULL;
    return false;
  }

  return true;
}

bool CIccApplyThreadedCmm::EnsureWorkers(int nWorkers)
{
  while ((int)m_workers.size() < nWorkers) {
    icStatusCMM status;
    CIccApplyCmm *worker = m_baseCmm->GetNewApplyCmm(status);
    if (!worker || status != icCmmStatOk) {
      delete worker;
      return false;
    }
    m_workers.push_back(worker);
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
 *  Partitions nPixels into contiguous strips and processes them through
 *  persistent worker threads. The final strip runs on the calling thread.
 *
 *  The requested thread count is a maximum. Short calls use coarser strips
 *  than bulk calls so synchronization does not overwhelm transform work.
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

  icUInt32Number pixelsPerThread = nPixels < kIccSmallApplyPixels
                                    ? kIccSmallApplyPixelsPerThread
                                    : kIccBulkApplyPixelsPerThread;
  icUInt32Number usefulThreads = nPixels / pixelsPerThread;
  if (nPixels % pixelsPerThread)
    usefulThreads++;
  int nActive = std::min(m_nThreads, (int)usefulThreads);

  if (nActive <= 1)
    return m_workers[0]->Apply(DstPixel, SrcPixel, nPixels);

  if (!EnsureWorkers(nActive))
    return icCmmStatAllocErr;

  return m_pool->Apply(m_workers, DstPixel, SrcPixel, nPixels, nActive,
                       nSrcSamples, nDstSamples);
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
  if (m_bDeleteCmm)
    delete m_pCmm;
}


int CIccThreadedCmm::GetMaxThreads()
{
  return kIccMaxCmmThreads;
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

  int nActual = icResolveCmmThreadCount(nThreads);
  if (nActual <= 0) {
    if (bDeleteCmm)
      delete pCmm;
    return NULL;
  }

  CIccThreadedCmm *rv = new CIccThreadedCmm();
  // The caller transfers ownership, or retains pCmm until rv is destroyed.
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
