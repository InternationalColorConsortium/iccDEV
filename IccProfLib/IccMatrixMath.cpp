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

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

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
  int nTotal = nRows * nCols;
  int nMin = nRows<nCols ? nRows : nCols;

  m_nRows = nRows;
  m_nCols = nCols;
  m_vals = new icFloatNumber[nTotal];
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
  int nTotal = matrix.m_nRows * matrix.m_nCols;
  m_nRows = matrix.m_nRows;
  m_nCols = matrix.m_nCols;
  m_vals = new icFloatNumber[nTotal];
  memcpy(m_vals, matrix.m_vals, nTotal*sizeof(icFloatNumber));
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
  if (m_vals)
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
  char buf[80];
  int i, j;
  const icFloatNumber *row = entry(0);
  for (j=0; j<m_nRows; j++) {
    for (i=0; i<m_nCols; i++) {
      sprintf(buf, ICCMTXSTEPDUMPFMT, row[i]);
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

  CIccMatrixMath *pNew = new CIccMatrixMath(mRows, m_nCols);

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
  if (m_nRows==3 && m_nCols==3) {
    icMatrixInvert3x3(m_vals);
    return true;
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
  if (m_nCols!=m_nRows)
    return false;

  int i, j;
  for (j=0; j<m_nRows; j++) {
    for (i=0; i<m_nCols; i++) {
      icFloatNumber v = *(entry(j, i));
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
  if (m_nRows != dstRange.steps || m_nCols != srcRange.steps)
    return false;

  icUInt16Number d;
  icFloatNumber srcStart = icF16toF(srcRange.start);
  icFloatNumber srcEnd = icF16toF(srcRange.end);
  icFloatNumber dstStart = icF16toF(dstRange.start);
  icFloatNumber dstEnd = icF16toF(dstRange.end);
  icFloatNumber srcDiff = srcEnd - srcStart;
  icFloatNumber dstDiff = dstEnd - dstStart;
  icFloatNumber srcScale = (srcEnd - srcStart) / (srcRange.steps-1);
  icFloatNumber dstScale = (dstEnd - dstStart ) / (dstRange.steps - 1);

  icFloatNumber *data=entry(0);
  memset(data, 0, dstRange.steps*srcRange.steps*sizeof(icFloatNumber));

  for (d=0; d<dstRange.steps; d++) {
    icFloatNumber *r = entry(d);
    icFloatNumber w = dstStart + (icFloatNumber)d * dstScale;
    if (w<srcStart) {
      r[0] = 1.0;
    }
    else if (w>=srcEnd) {
      r[srcRange.steps-1] = 1.0;
    }
    else {
      icUInt16Number p = (icUInt16Number)((w - srcStart) / srcScale);
      icFloatNumber p2 = (w - (srcStart + p * srcScale)) / srcScale;

      if (p2<0.00001) {
        r[p] = 1.0f;
      }
      else if (p2>0.99999) {
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
  if (srcRange.steps != dstRange.steps ||
      srcRange.start != dstRange.start ||
      srcRange.end != dstRange.end) {
    CIccMatrixMath *mtx = new CIccMatrixMath(dstRange.steps, srcRange.steps);
    mtx->SetRange(srcRange, dstRange);

    return mtx;
  }

  return NULL;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
