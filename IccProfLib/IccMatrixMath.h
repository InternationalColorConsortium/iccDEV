/** @file
    File:       IccCmm.h

    Contains:   Header file for implementation of the CIccCmm class.

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
// -Initial implementation by Max Derhak 5-15-2003
// -Added support for Monochrome ICC profile apply by Rohit Patil 12-03-2008
// -Integrate changes for PCS adjustment by George Pawle 12-09-2008
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCMATRIXMATH_H)
#define _ICCMATRIXMATH_H

#include "IccProfile.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

#define icNearRange 0.000001
#define ICCMTXSTEPDUMPFMT " %.8f"

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Object for performing matrix/vector math
**************************************************************************
*/
class ICCPROFLIB_API CIccMatrixMath
{
public:
  CIccMatrixMath(icUInt16Number nRows, icUInt16Number nCols, bool bInitIdentity=false);
  CIccMatrixMath(const CIccMatrixMath &mat);
  // Rule of Two: the copy constructor deep-copies the owned m_vals buffer and the
  // destructor deletes it, so a matching deep-copy assignment operator is required
  // -- the implicit one would shallow-copy m_vals and double-free on destruction.
  CIccMatrixMath &operator=(const CIccMatrixMath &mat);
  virtual ~CIccMatrixMath();

  virtual void VectorMult(icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetCols() const { return m_nCols; }
  virtual icUInt16Number GetRows() const { return m_nRows; }

  CIccMatrixMath *Mult(const CIccMatrixMath *matrix) const;

  icFloatNumber RowSum(icUInt16Number nRow) const;

  void VectorScale(const icFloatNumber *vec);
  void Scale(icFloatNumber v);

  icFloatNumber *entry(icUInt16Number nRow, icUInt16Number nCol=0) { return &m_vals[(int)nRow*m_nCols + nCol]; }
  const icFloatNumber *entry(icUInt16Number nRow, icUInt16Number nCol=0) const { return &m_vals[(int)nRow*m_nCols + nCol]; }

  virtual bool isIdentityMtx() const;

  virtual void dumpMtx(std::string &str) const;

  bool Invert();

  bool SetRange(const icSpectralRange &from, const icSpectralRange &to);

  static CIccMatrixMath* rangeMap(const icSpectralRange &from, const icSpectralRange &to);  //Caller is responsible for deleting returned matrix
  //As above, but sets *pFailed when NULL means "a conversion was needed and could not be built"
  //rather than "the ranges are identical". Callers that fall back to using one range's length
  //against the other array need this to tell those two cases apart.
  static CIccMatrixMath* rangeMap(const icSpectralRange &from, const icSpectralRange &to, bool *pFailed);

protected:
  icUInt16Number m_nRows, m_nCols;
  icFloatNumber *m_vals;
};

#ifdef USEICCDEVNAMESPACE
}
#endif


#endif //_ICCMATRIXMATH_H