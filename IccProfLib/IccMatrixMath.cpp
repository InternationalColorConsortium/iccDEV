/** @file
    File:       IccMatrixMath.cpp

    Contains:   Implementation of matrix math operations

    Version:    V1

    Copyright:  See ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2015 The International Color Consortium. All rights
 * reserved.
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
// -Initial implementation by Max Derhak 5-15-2003
// -Added support for Monochrome ICC profile apply by Rohit Patil 12-03-2008
// -Integrated changes for PCS adjustment by George Pawle 12-09-2008
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#endif

#include "IccMatrixMath.h"
#include "IccUtil.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <new>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

static const size_t icMaxMatrixMathEntries = 0x1000000;

static bool icCanAllocateMatrixMath(icUInt16Number nRows, icUInt16Number nCols)
{
  return nRows && nCols && (size_t)nRows <= icMaxMatrixMathEntries / nCols;
}

static bool icIsValidMatrixSpectralRange(const icSpectralRange &range)
{
  icFloatNumber start = icF16toF(range.start);
  icFloatNumber end = icF16toF(range.end);

  return range.steps >= 2 &&
         std::isfinite(start) &&
         std::isfinite(end) &&
         end > start;
}

/**
**************************************************************************
* Name: CIccMatrixMath::CIccMatrixMath
*
* Purpose:
*  Constructor
**************************************************************************
*/
CIccMatrixMath::CIccMatrixMath(icUInt16Number nRows, icUInt16Number nCols, bool bInitIdentity/* =false */)
{
  size_t nTotal = (size_t)nRows * nCols;
  int nMin = nRows<nCols ? nRows : nCols;

  m_nRows = nRows;
  m_nCols = nCols;
  m_vals = icCanAllocateMatrixMath(nRows, nCols) ? new (std::nothrow) icFloatNumber[nTotal] : NULL;
  if (!m_vals) {
    m_nRows = 0;
    m_nCols = 0;
    return;
  }
  if (bInitIdentity) {
    memset(m_vals, 0, nTotal * sizeof(icFloatNumber));
    int i;
    for (i=0; i<nMin; i++) {
      icFloatNumber *row = entry(nRows-1-i);
      row[nCols-1-i] = 1.0;
    }
  }
}


/**
**************************************************************************
* Name: CIccMatrixMath::CIccMatrixMath
*
* Purpose:
*  Copy Constructor
**************************************************************************
*/
CIccMatrixMath::CIccMatrixMath(const CIccMatrixMath &matrix)
{
  size_t nTotal = (size_t)matrix.m_nRows * matrix.m_nCols;
  m_nRows = matrix.m_nRows;
  m_nCols = matrix.m_nCols;
  m_vals = matrix.m_vals && icCanAllocateMatrixMath(m_nRows, m_nCols) ? new (std::nothrow) icFloatNumber[nTotal] : NULL;
  if (!m_vals) {
    m_nRows = 0;
    m_nCols = 0;
    return;
  }
  memcpy(m_vals, matrix.m_vals, nTotal*sizeof(icFloatNumber));
}


/**
**************************************************************************
* Name: CIccMatrixMath::operator=
*
* Purpose:
*  Copy assignment (Rule of Two).  The copy constructor above deep-copies the
*  owned m_vals buffer and the destructor frees it; without this matching
*  operator the compiler-generated assignment would shallow-copy m_vals, so the
*  source and target would share one buffer and double-free it on destruction.
**************************************************************************
*/
CIccMatrixMath &CIccMatrixMath::operator=(const CIccMatrixMath &matrix)
{
  if (this == &matrix)
    return *this;

  if (!matrix.IsValid()) {
    delete[] m_vals;
    m_vals = NULL;
    m_nRows = 0;
    m_nCols = 0;
    return *this;
  }

  size_t nTotal = (size_t)matrix.m_nRows * matrix.m_nCols;
  // Allocate the replacement before releasing the old buffer so a failed
  // allocation leaves this object unchanged (strong exception guarantee).
  icFloatNumber *vals = matrix.m_vals && icCanAllocateMatrixMath(matrix.m_nRows, matrix.m_nCols) ? new (std::nothrow) icFloatNumber[nTotal] : NULL;
  if (!vals)
    return *this;
  memcpy(vals, matrix.m_vals, nTotal*sizeof(icFloatNumber));

  delete[] m_vals;
  m_vals = vals;
  m_nRows = matrix.m_nRows;
  m_nCols = matrix.m_nCols;

  return *this;
}


/**
**************************************************************************
* Name: CIccMatrixMath::~CIccMatrixMath
*
* Purpose:
*  Destructor
**************************************************************************
*/
CIccMatrixMath::~CIccMatrixMath()
{
  delete[] m_vals;
}


/**
**************************************************************************
* Name: CIccMatrixMath::VectorMult
*
* Purpose:
*  Multiplies pSrc vector passed by a matrix resulting in a pDst vector
**************************************************************************
*/
void CIccMatrixMath::VectorMult(icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  if (!IsValid() || !pDst || !pSrc)
    return;

  int i, j;
  const icFloatNumber *row = entry(0);
  for (j=0; j<m_nRows; j++) {
    pDst[j] = 0.0f;
    for (i=0; i<m_nCols; i++) {
      if (row[i]!=0.0)
        pDst[j] += row[i] * pSrc[i];
    }
    row = &row[m_nCols];
  }
}


/**
**************************************************************************
* Name: CIccMatrixMath::dump
*
* Purpose:
*  dumps the context of the step
**************************************************************************
*/
void CIccMatrixMath::dumpMtx(std::string &str) const
{
  if (!IsValid())
    return;

  const size_t bufSize = 80;
  char buf[bufSize];
  int i, j;
  const icFloatNumber *row = entry(0);
  for (j=0; j<m_nRows; j++) {
    for (i=0; i<m_nCols; i++) {
      snprintf(buf, bufSize, ICCMTXSTEPDUMPFMT, row[i]);
      str += buf;
    }
    str += "\n";
    row = &row[m_nCols];
  }
}


/**
**************************************************************************
* Name: CIccMatrixMath::Mult
*
* Purpose:
*  Creates a new CIccMatrixMath that is the result of concatentating
*  another matrix with this matrix. (IE result = matrix * this).
**************************************************************************
*/
CIccMatrixMath *CIccMatrixMath::Mult(const CIccMatrixMath *matrix) const
{
  icUInt16Number mCols = matrix->m_nCols;
  icUInt16Number mRows = matrix->m_nRows;

  if (m_nRows != mCols)
    return NULL;

  if (!IsValid() || !matrix->IsValid())
    return NULL;

  CIccMatrixMath *pNew = new (std::nothrow) CIccMatrixMath(mRows, m_nCols);
  if (!pNew || !pNew->IsValid()) {
    delete pNew;
    return NULL;
  }

  int i, j, k;
  for (j=0; j<mRows; j++) {
    const icFloatNumber *row = matrix->entry(j);
    for (i=0; i<m_nCols; i++) {
      icFloatNumber *to = pNew->entry(j, i);
      const icFloatNumber *from = entry(0, i);

      *to = 0.0f;
      for (k=0; k<m_nRows; k++) {
        *to += row[k] * (*from);
        from += m_nCols;
      }
    }
  }

  return pNew;
}

/**
**************************************************************************
* Name: CIccMatrixMath::VectorScale
*
* Purpose:
*  Multiplies each row by values of vector passed in
**************************************************************************
*/
void CIccMatrixMath::VectorScale(const icFloatNumber *vec)
{
  if (!IsValid() || !vec)
    return;

  int i, j;
  for (j=0; j<m_nRows; j++) {
    icFloatNumber *row = entry(j);
    for (i=0; i<m_nCols; i++) {
      row[i] *= vec[i];
    }
  }
}

/**
**************************************************************************
* Name: CIccMatrixMath::Scale
*
* Purpose:
*  Multiplies all values in matrix by a single scale factor
**************************************************************************
*/
void CIccMatrixMath::Scale(icFloatNumber v)
{
  if (!IsValid())
    return;

  int i, j;
  for (j=0; j<m_nRows; j++) {
    icFloatNumber *row = entry(j);
    for (i=0; i<m_nCols; i++) {
      row[i] *= v;
    }
  }
}

/**
**************************************************************************
* Name: CIccMatrixMath::Invert
*
* Purpose:
*  Inverts the matrix
**************************************************************************
*/
bool CIccMatrixMath::Invert()
{
  if (IsValid() && m_nRows==3 && m_nCols==3) {
    return icMatrixInvert3x3(m_vals);
  }

  return false;
}



/**
**************************************************************************
* Name: CIccMatrixMath::RowSum
*
* Purpose:
*  Creates a new CIccMatrixMath step that is the result of multiplying the
*  matrix of this object to the scale of another object.
**************************************************************************
*/
icFloatNumber CIccMatrixMath::RowSum(icUInt16Number nRow) const
{
  if (!IsValid() || nRow >= m_nRows)
    return 0.0f;

  icFloatNumber rv=0;
  int i;
  const icFloatNumber *row = entry(nRow);

  for (i=0; i<m_nCols; i++) {
    rv += row[i];
  }

  return rv;
}



/**
**************************************************************************
* Name: CIccMatrixMath::isIdentityMtx
*
* Purpose:
*  Determines if applying this step will result in negligible change in data
**************************************************************************
*/
bool CIccMatrixMath::isIdentityMtx() const
{
  if (!IsValid() || m_nCols!=m_nRows)
    return false;

  int i, j;
  for (j=0; j<m_nRows; j++) {
    const icFloatNumber *row = &m_vals[(size_t)j * m_nCols];
    for (i=0; i<m_nCols; i++) {
      icFloatNumber v = row[i];
      if (i==j) {
        if (v<1.0f-icNearRange || v>1.0f+icNearRange)
          return false;
      }
      else {
        if (v<-icNearRange ||v>icNearRange)
          return false;
      }
    }
  }

  return true;
}


/**
**************************************************************************
* Name: CIccMatrixMath::SetRange
*
* Purpose:
*  Fills a matrix math object that can be used to convert
*  spectral vectors from one spectral range to another using linear interpolation.
**************************************************************************
*/
bool CIccMatrixMath::SetRange(const icSpectralRange &srcRange, const icSpectralRange &dstRange)
{
  if (!IsValid() || m_nRows != dstRange.steps || m_nCols != srcRange.steps)
    return false;

  if (!icIsValidMatrixSpectralRange(srcRange) || !icIsValidMatrixSpectralRange(dstRange))
    return false;

  icUInt16Number d;
  icFloatNumber srcStart = icF16toF(srcRange.start);
  icFloatNumber srcEnd = icF16toF(srcRange.end);
  icFloatNumber dstStart = icF16toF(dstRange.start);
  icFloatNumber dstEnd = icF16toF(dstRange.end);

  icFloatNumber srcScale = (srcEnd - srcStart) / (srcRange.steps-1);
  icFloatNumber dstScale = (dstEnd - dstStart ) / (dstRange.steps - 1);

  if (!std::isfinite(srcScale) || !std::isfinite(dstScale))
    return false;

  icFloatNumber *data=entry(0);
  size_t dataSize = (size_t)dstRange.steps * srcRange.steps * sizeof(icFloatNumber);
  memset(data, 0, dataSize);

  for (d=0; d<dstRange.steps; d++) {
    icFloatNumber *r = entry(d);
    icFloatNumber w = dstStart + (icFloatNumber)d * dstScale;
    if (w<srcStart) {
      r[0] = 1.0f;
    }
    else if (w>=srcEnd) {
      r[srcRange.steps-1] = 1.0f;
    }
    else {
      icFloatNumber temp = (w - srcStart) / srcScale;
      if (temp < 0.0f)
        temp = 0.0f;
      if (temp > 65535.0f)
        temp = 65535.0f;
      icUInt16Number p = (icUInt16Number)temp;
      icFloatNumber p2 = (w - (srcStart + p * srcScale)) / srcScale;

      if (p2<0.00001f) {
        r[p] = 1.0f;
      }
      else if (p2>0.99999f) {
        r[p+1] = 1.0f;
      }
      else {
        r[p] = 1.0f - p2;
        r[p+1] = p2;
      }
    }
  }

  return true;
}

/**
 **************************************************************************
 * Name: CIccMatrixMath::rangeMap
 *
 * Purpose:
 *  This helper function generates a matrix math object that can be used to convert
 *  spectral vectors from one spectral range to another using linear interpolation.
 **************************************************************************
 */
CIccMatrixMath *CIccMatrixMath::rangeMap(const icSpectralRange &srcRange, const icSpectralRange &dstRange)
{
  return rangeMap(srcRange, dstRange, NULL);
}

CIccMatrixMath *CIccMatrixMath::rangeMap(const icSpectralRange &srcRange, const icSpectralRange &dstRange, bool *pFailed)
{
  if (pFailed)
    *pFailed = false;

  if (!icIsValidMatrixSpectralRange(srcRange) || !icIsValidMatrixSpectralRange(dstRange)) {
    if (pFailed)
      *pFailed = true;
    return NULL;
  }

  if (srcRange.steps != dstRange.steps ||
      srcRange.start != dstRange.start ||
      srcRange.end != dstRange.end) {
    CIccMatrixMath *mtx = new (std::nothrow) CIccMatrixMath(dstRange.steps, srcRange.steps);
    if (!mtx || !mtx->IsValid() || !mtx->SetRange(srcRange, dstRange)) {
      delete mtx;
      if (pFailed)
        *pFailed = true;
      return NULL;
    }

    return mtx;
  }

  return NULL;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
