/** @file
File:       IccSparseMatrix.h

Contains:   Header for implementation of Sparse Matrices

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
// -Dec 30, 2012 
//  Initial Sparse Matrix development
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCSPARSEMATRIX_H
#define _ICCSPARSEMATRIX_H

#include "IccDefs.h"
#include "IccUtil.h"


class IIccSparseMatrixEntry
{
public:
  virtual ~IIccSparseMatrixEntry(){}
  virtual void init(void *pData)=0;
  
  virtual icFloatNumber get(int index) const=0;
  virtual void set(int index, icFloatNumber value)=0;

  virtual icUInt8Number size() const=0;
  virtual icUInt8Number *getPtr(int index=0) const=0;
};

template <class T>
class CIccSparseMatrixEntry : public IIccSparseMatrixEntry
{
public:
  CIccSparseMatrixEntry<T>() {m_pData = 0;}
  void init(void *pData) {m_pData = (T*)pData;}

  virtual icUInt8Number size() const { return sizeof(T); }
  virtual icUInt8Number *getPtr(int index) const {return (icUInt8Number*)(&m_pData[index]); }

protected:
  T *m_pData;
};

class CIccSparseMatrixUInt8 : public CIccSparseMatrixEntry<icUInt8Number>
{
public:
  CIccSparseMatrixUInt8() {}

  virtual icFloatNumber get(int index) const {return (icFloatNumber)m_pData[index]/255.0f;}
  virtual void set(int index, icFloatNumber value) {m_pData[index] = value<0.0 ? 0 : (value > 1.0 ? 255 : (icUInt8Number)(value*255.0f+0.5f));}

};

class CIccSparseMatrixUInt16 : public CIccSparseMatrixEntry<icUInt16Number>
{
public:
  CIccSparseMatrixUInt16() {}

  virtual icFloatNumber get(int index) const {return (icFloatNumber)m_pData[index]/65535.0f;}
  virtual void set(int index, icFloatNumber value) {m_pData[index] = value<0.0 ? 0 : (value > 1.0 ? 65535 : (icUInt8Number)(value*65535.0f+0.5f));}

};

class CIccSparseMatrixFloat16 : public CIccSparseMatrixEntry<icFloat16Number>
{
public:
  CIccSparseMatrixFloat16() {}

  virtual icFloatNumber get(int index) const {return icF16toF(m_pData[index]);}
  virtual void set(int index, icFloatNumber value) {m_pData[index] = icFtoF16(value);}
};

class CIccSparseMatrixFloat32 : public CIccSparseMatrixEntry<icFloat32Number>
{
public:
  CIccSparseMatrixFloat32() {}

  virtual icFloatNumber get(int index) const {return (icFloatNumber)m_pData[index];}
  virtual void set(int index, icFloatNumber value) {m_pData[index] = (icFloat32Number)value;}
};

class CIccSparseMatrixFloatNum : public CIccSparseMatrixEntry<icFloatNumber>
{
public:
  CIccSparseMatrixFloatNum() {}

  virtual icFloatNumber get(int index) const {return m_pData[index];}
  virtual void set(int index, icFloatNumber value) {m_pData[index] = value;}
};

class ICCPROFLIB_API CIccSparseMatrix
{
public:
  CIccSparseMatrix(void *pMatrix=NULL, icUInt32Number nSize=0, icSparseMatrixType nType=icSparseMatrixFloatNum, bool bInitFromData=false);
  CIccSparseMatrix(const CIccSparseMatrix &mtx);

  virtual ~CIccSparseMatrix(void);

  CIccSparseMatrix &operator=(const CIccSparseMatrix &mtx);

  icUInt16Number Rows() const { return m_nRows; }
  icUInt16Number Cols() const { return m_nCols; }

  void Reset(void *pMatrix, icUInt32Number nSize, icSparseMatrixType nType, bool bInitFromData=true);
  bool Init(icUInt16Number nRows, icUInt16Number nCols, bool bSetData=false);
  void Clear();
  bool Copy(const CIccSparseMatrix &mtx);
  bool FillFromFullMatrix(icFloatNumber *pData);

  bool MultiplyVector(icFloatNumber *pResult, const icFloatNumber *pVector) const;
  bool Interp(icFloatNumber d1, const CIccSparseMatrix &mtx1, icFloatNumber d2, const  CIccSparseMatrix &mtx2);
  bool Union(const CIccSparseMatrix &mtx1, const CIccSparseMatrix &mtx2);

  bool IsValid();

  icUInt16Number GetNumEntries() const {return m_RowStart ? m_RowStart[m_nRows] : 0;}
  icUInt16Number* GetRowStart() const { return m_RowStart; }
  icUInt16Number GetRowOffset(icUInt16Number nRow=0) { return m_RowStart[nRow]; }

  icUInt16Number* GetColumnsForRow(icUInt16Number nRow=0) const { return &m_ColumnIndices[m_RowStart[nRow]]; }
  icUInt16Number GetNumRowColumns(icUInt16Number nRow) const { return (icUInt16Number)(m_RowStart[nRow+1] - m_RowStart[nRow]); }
  IIccSparseMatrixEntry* GetData() const { return m_Data; }

  icUInt32Number GetMaxEntries() { return m_nMaxEntries; }

  static icUInt32Number MaxEntries(icUInt32Number nMemSize, icUInt16Number nRows, icUInt8Number nTypeSize);
  static icUInt32Number MaxEntries(icUInt32Number nMemSize, icUInt16Number nRows, icSparseMatrixType nType) {return MaxEntries(nMemSize, nRows, EntrySize(nType));}
  static icUInt32Number MemSize(icUInt32Number nMaxEntries, icUInt16Number nRows, icUInt8Number nTypeSize);
  static icUInt32Number MemSize(icUInt32Number nMaxEntries, icUInt16Number nRows, icSparseMatrixType nType) {return MemSize(nMaxEntries, nRows, EntrySize(nType));}
  static icUInt8Number EntrySize(icSparseMatrixType nType);

protected:
  icUInt8Number *m_pMatrix;
  icUInt32Number m_nRawSize;
  icSparseMatrixType m_nType;

  icUInt16Number m_nRows;
  icUInt16Number m_nCols;

  icUInt16Number *m_RowStart;
  icUInt16Number *m_ColumnIndices;

  IIccSparseMatrixEntry *m_Data;

  icUInt32Number m_nMaxEntries;
};



#endif //_ICCSPARSEMATRIX_H
