#include "IccSparseMatrix.h"

#include <cstring>

CIccSparseMatrix::CIccSparseMatrix(void *pMatrix, size_t nSize, icSparseMatrixType nType, bool bInitFromData/*=false*/)
{
  m_pMatrix = (unsigned char*)pMatrix;
  m_nRawSize = nSize;
  m_nType = nType;
  m_Data = NULL;

  if (bInitFromData) {
    icUInt16Number nRows = *((icUInt16Number*)pMatrix);
    icUInt16Number nCols = *((icUInt16Number*)(m_pMatrix+sizeof(icUInt16Number)));
    Init(nRows, nCols);
  }
  else {
    m_nRows = 0;
    m_nCols = 0;
    m_nMaxEntries = 0;
    m_RowStart = NULL;
    m_ColumnIndices = NULL;
  }
}

CIccSparseMatrix::CIccSparseMatrix(const CIccSparseMatrix &mtx)
{
  m_pMatrix = mtx.m_pMatrix;
  m_nRawSize = mtx.m_nRawSize;
  m_nType = mtx.m_nType;
  m_Data = NULL;

  if (mtx.m_Data) {
    switch (m_nType) {
    case icSparseMatrixUInt8:
      m_Data = new CIccSparseMatrixUInt8();
      break;
    case icSparseMatrixUInt16:
      m_Data = new CIccSparseMatrixUInt16();
      break;
    case icSparseMatrixFloat16:
      m_Data = new CIccSparseMatrixFloat16();
      break;
    case icSparseMatrixFloat32:
      m_Data = new CIccSparseMatrixFloat32();
      break;
    case icSparseMatrixFloatNum:
      m_Data = new CIccSparseMatrixFloatNum();
      break;    // without this, we have a memory leak!
    default:
      m_Data = NULL;
    }
  }

  if (m_Data) {
    Init(mtx.m_nRows, mtx.m_nCols);  
  }
  else {
    m_nRows = 0;
    m_nCols = 0;
    m_nMaxEntries = 0;
    m_RowStart = NULL;
    m_ColumnIndices = NULL;
  }
}

CIccSparseMatrix::~CIccSparseMatrix(void)
{
  if (m_Data)
    delete m_Data;
}

CIccSparseMatrix &CIccSparseMatrix::operator=(const CIccSparseMatrix &mtx)
{
  m_pMatrix = mtx.m_pMatrix;
  m_nRawSize = mtx.m_nRawSize;
  m_nType = mtx.m_nType;

  if (mtx.m_Data) {
    if (m_Data)
      delete m_Data;

    switch (m_nType) {
    case icSparseMatrixUInt8:
      m_Data = new CIccSparseMatrixUInt8();
      break;
    case icSparseMatrixUInt16:
      m_Data = new CIccSparseMatrixUInt16();
      break;
    case icSparseMatrixFloat16:
      m_Data = new CIccSparseMatrixFloat16();
      break;
    case icSparseMatrixFloat32:
      m_Data = new CIccSparseMatrixFloat32();
      break;
    case icSparseMatrixFloatNum:
      m_Data = new CIccSparseMatrixFloatNum();
      break;    // without this, we have a memory leak!
    default:
      m_Data = NULL;
    }
  }

  if (m_Data) {
    Init(mtx.m_nRows, mtx.m_nCols);  
  }
  else {
    m_nRows = 0;
    m_nCols = 0;
    m_nMaxEntries = 0;
  }

  return *this;
}

bool CIccSparseMatrix::Reset(void *pMatrix, size_t nSize, icSparseMatrixType nType, bool bInitFromData/*=true*/)
{
  if (m_Data)
    delete m_Data;

  m_pMatrix = (unsigned char*)pMatrix;
  m_nRawSize = nSize;
  m_nType = nType;
  m_Data = NULL;

  if (bInitFromData) {
    icUInt16Number nRows = *((icUInt16Number*)pMatrix);
    icUInt16Number nCols = *((icUInt16Number*)(m_pMatrix+sizeof(icUInt16Number)));
    return Init(nRows, nCols);
  }
  else {
    m_nRows = 0;
    m_nCols = 0;
    m_nMaxEntries = 0;
    m_RowStart = NULL;
    m_ColumnIndices = NULL;
    return true;
  }

}


bool CIccSparseMatrix::Init(icUInt16Number nRows, icUInt16Number nCols, bool bSetData/*=false*/)
{
  if (!m_pMatrix)
    return false;

  icUInt16Number *Dim = (icUInt16Number*)m_pMatrix;

  if (m_Data)
    delete m_Data;

  switch (m_nType) {
    case icSparseMatrixUInt8:
      m_Data = new CIccSparseMatrixUInt8();
      break;
    case icSparseMatrixUInt16:
      m_Data = new CIccSparseMatrixUInt16();
      break;
    case icSparseMatrixFloat16:
      m_Data = new CIccSparseMatrixFloat16();
      break;
    case icSparseMatrixFloat32:
      m_Data = new CIccSparseMatrixFloat32();
      break;
    case icSparseMatrixFloatNum:
      m_Data = new CIccSparseMatrixFloatNum();
      break;
    default:
      m_nRows = 0;
      m_nCols = 0;
      if (bSetData) {
        Dim[0] = 0;
        Dim[1] = 0;
      }
      m_Data = NULL;
      m_RowStart = NULL;
      m_ColumnIndices = NULL;
      m_nMaxEntries = 0;
      return false;
  }
  m_nRows = nRows;
  m_nCols = nCols;
  if (bSetData) {
    Dim[0] = nRows;
    Dim[1] = nCols;
  }

  icUInt32Number coloffset = 2*sizeof(icUInt16Number) + (nRows+1)*sizeof(icUInt32Number);
  icUInt16Number nTypeSize = m_Data->size();

  if (coloffset+(nTypeSize-1) >m_nRawSize) {
    m_nRows = 0;
    m_nCols = 0;
    if (bSetData) {
      Dim[0] = 0;
      Dim[1] = 0;
    }
    m_Data = NULL;
    m_RowStart = NULL;
    m_ColumnIndices = NULL;
    m_nMaxEntries = 0;
    return false;
  }

  m_nMaxEntries = (icUInt32Number)((m_nRawSize - coloffset - (nTypeSize-1)) / (nTypeSize+sizeof(icUInt16Number)));

  icUInt32Number dataoffset = ((icUInt32Number)((coloffset + m_nMaxEntries*sizeof(icUInt16Number) + nTypeSize-1) / nTypeSize)) * nTypeSize;

  m_RowStart = (icUInt16Number*)(m_pMatrix + 2*sizeof(icUInt16Number));
  m_ColumnIndices = (icUInt16Number*)(m_pMatrix + (3+m_nRows)*sizeof(icUInt16Number));

  m_Data->init(m_pMatrix+dataoffset);

  return true;
}

void CIccSparseMatrix::Clear()
{
  memset(m_pMatrix + 2*sizeof(icUInt16Number), 0, m_nRawSize-2*sizeof(icUInt16Number));
}

bool CIccSparseMatrix::Copy(const CIccSparseMatrix &mtx)
{
  if (!mtx.m_Data || !mtx.m_pMatrix)
    return false;

  if (!Init(mtx.m_nRows, mtx.m_nCols))
    return false;

  // m_RowStart is a u16* pointing at (m_nRows+1) u16 slots; copying with
  // sizeof(icUInt32Number) stomps 2× the allocated region (into the
  // adjacent m_ColumnIndices tail).
  memcpy(m_RowStart, mtx.m_RowStart, (m_nRows+1)*sizeof(icUInt16Number));
  icUInt32Number nEntries = m_RowStart[m_nRows];
  icUInt32Number i;

  for (i=0; i<nEntries; i++) {
    m_Data->set(i, mtx.m_Data->get(i));
  }

  return true;
}

bool CIccSparseMatrix::FillFromFullMatrix(icFloatNumber *pData)
{
  if (!m_Data)
    return false;

  Clear();

  int r, c;
  icUInt32Number nEntry = 0;

  for (r=0; r<(int)m_nRows; r++) {
    m_RowStart[r] = (icUInt16Number)nEntry;
    for (c=0; c<(int)m_nCols; c++) {
      if (icNotZero(*pData)) {
        if (nEntry+1 > m_nMaxEntries)
          return false;
        m_ColumnIndices[nEntry] = c;
        m_Data->set(nEntry, *pData);
        nEntry++;
      }
      pData++;
    }
  }
  m_RowStart[r] = (icUInt16Number)nEntry;

  return true;
}

bool CIccSparseMatrix::MultiplyVector(icFloatNumber *pResult, const icFloatNumber *pVector) const
{
  if (!m_Data)
    return false;

  int r;
  icUInt32Number e, le;
  icUInt16Number *ci=m_ColumnIndices;

  for (r=0, e=0; r<(int)m_nRows; r++) {
    icFloatNumber v=0.0f;

    le = m_RowStart[r+1];

    for (; e<le; e++, ci++) {
      v += m_Data->get(e)*pVector[*ci];
    }
    pResult[r]=v;
  }
  return true;
}


bool CIccSparseMatrix::Interp(icFloatNumber d1, const CIccSparseMatrix &mtx1, icFloatNumber d2, const CIccSparseMatrix &mtx2)
{
  if (mtx1.m_nRows != mtx2.m_nRows || mtx1.m_nCols != mtx2.m_nCols)
    return false;

  if (mtx1.m_nRows != m_nRows || mtx2.m_nCols != m_nCols) {
    if (!Init(mtx1.m_nRows, mtx2.m_nCols))
      return false;
  }

  icUInt32Number pos=0;
  int r=0, i, j;
  int fA, fB, nA, nB, offA, offB, iA, iB;

  for (r=0; r<(int)m_nRows; r++) {
    fA = (int)mtx1.m_RowStart[r];
    fB = (int)mtx2.m_RowStart[r];
    nA = (int)(mtx1.m_RowStart[r+1] - fA);
    nB = (int)(mtx2.m_RowStart[r+1] - fA);

    m_RowStart[r] = (icUInt16Number)pos;

    if (nA && nB) {
      i=j=0;

      while(i<nA || j<nB) {
        if (pos >= m_nMaxEntries)
          return false;

        if (i<nA && j<nB) {
          offA = fA+i;
          offB = fB+j;
          iA = mtx1.m_ColumnIndices[offA];
          iB = mtx2.m_ColumnIndices[offB];

          if (iA<iB) {
            m_ColumnIndices[pos] = iA;
            m_Data->set(pos, d1*mtx1.m_Data->get(offA));
            i++;
          }
          else if (iB<iA) {
            m_ColumnIndices[pos] = iB;
            m_Data->set(pos, d2*mtx2.m_Data->get(offB));
            j++;
          }
          else {
            m_ColumnIndices[pos] = iA;
            m_Data->set(pos, d1*mtx1.m_Data->get(offA) + d2*mtx2.m_Data->get(offB));
            i++;
            j++;
          }
          pos++;
        }
        else if (i<nA) {
          offA = fA+i;
          iA = mtx1.m_ColumnIndices[offA];

          m_ColumnIndices[pos] = iA;
          m_Data->set(pos, d1 * mtx1.m_Data->get(offA));
        }
        else {
          offB = fB+j;
          iB = mtx2.m_ColumnIndices[offB];

          m_ColumnIndices[pos] = iB;
          m_Data->set(pos, d2 * mtx2.m_Data->get(offB));
        }
      }
    }
    else if (nA) {
      if (pos+nA >= m_nMaxEntries)
        return false;

      memcpy(&m_ColumnIndices[pos], &mtx1.m_ColumnIndices[fA], nA);
      
      for (i=0; i<nA; i++) {
        m_Data->set(pos++, d1 * mtx1.m_Data->get(fA+i));
      }
    }
    else if (nB) {
      if (pos+nB >= m_nMaxEntries)
        return false;

      memcpy(&m_ColumnIndices[pos], &mtx2.m_ColumnIndices[fB], nB);

      for (i=0; i<nB; i++) {
        m_Data->set(pos++, d2 * mtx2.m_Data->get(fB+i));
      }
    }
    m_RowStart[r]=(icUInt16Number)pos;
  }

  return true;
}


bool CIccSparseMatrix::Union(const CIccSparseMatrix &mtx1, const  CIccSparseMatrix &mtx2)
{
  if (mtx1.m_nRows != mtx2.m_nRows || mtx1.m_nCols != mtx2.m_nCols)
    return false;

  if (mtx1.m_nRows != m_nRows || mtx2.m_nCols != m_nCols) {
    if (!Init(mtx1.m_nRows, mtx2.m_nCols))
      return false;
  }

  icUInt32Number pos=0;
  int r=0, i, j;
  int fA, fB, nA, nB, offA, offB, iA, iB;

  for (r=0; r<(int)m_nRows; r++) {
    fA = (int)mtx1.m_RowStart[r];
    fB = (int)mtx2.m_RowStart[r];
    nA = (int)(mtx1.m_RowStart[r+1] - fA);
    nB = (int)(mtx2.m_RowStart[r+1] - fA);

    m_RowStart[r] = (icUInt16Number)pos;

    if (nA && nB) {
      i=j=0;

      while(i<nA || j<nB) {
        if (pos >= m_nMaxEntries)
          return false;

        if (i<nA && j<nB) {
          offA = fA+i;
          offB = fB+j;
          iA = mtx1.m_ColumnIndices[offA];
          iB = mtx2.m_ColumnIndices[offB];

          if (iA<iB) {
            m_ColumnIndices[pos] = iA;
            m_Data->set(pos, 1.0f);
            i++;
          }
          else if (iB<iA) {
            m_ColumnIndices[pos] = iB;
            m_Data->set(pos, 1.0f);
            j++;
          }
          else {
            m_ColumnIndices[pos] = iA;
            m_Data->set(pos, 1.0f);
            i++;
            j++;
          }
          pos++;
        }
        else if (i<nA) {
          offA = fA+i;
          iA = mtx1.m_ColumnIndices[offA];

          m_ColumnIndices[pos] = iA;
          m_Data->set(pos, 1.0f);
        }
        else {
          offB = fB+j;
          iB = mtx2.m_ColumnIndices[offB];

          m_ColumnIndices[pos] = iB;
          m_Data->set(pos, 1.0f);
        }
      }
    }
    else if (nA) {
      if (pos+nA >= m_nMaxEntries)
        return false;

      memcpy(&m_ColumnIndices[pos], &mtx1.m_ColumnIndices[fA], nA);

      for (i=0; i<nA; i++) {
        m_Data->set(pos++, 1.0f);
      }
    }
    else if (nB) {
      if (pos+nB >= m_nMaxEntries)
        return false;

      memcpy(&m_ColumnIndices[pos], &mtx2.m_ColumnIndices[fB], nB);

      for (i=0; i<nB; i++) {
        m_Data->set(pos++, 1.0f);
      }
    }
    m_RowStart[r]=(icUInt16Number)pos;
  }

  return true;
}


bool CIccSparseMatrix::IsValid()
{
  if (!m_Data || !m_RowStart || !m_ColumnIndices)
    return false;

  int r, i;
  for (r=0; r<(int)m_nRows; r++) {
    if (m_RowStart[r]>m_RowStart[r+1])
      return false;

    for (i=m_RowStart[r]; i<(int)m_RowStart[r+1]-1; i++) {
      if (m_ColumnIndices[i]>=m_ColumnIndices[i+1] || m_ColumnIndices[i]>m_nCols)
        return false;
    }
    if (m_ColumnIndices[i]>m_nCols)
      return false;
  }
  return true;
}

icUInt32Number CIccSparseMatrix::MaxEntries(icUInt32Number nMemSize, icUInt16Number nRows, icUInt8Number nTypeSize)
{
  return (nMemSize - 4 - 4*(nRows+1) - (nTypeSize-1)) / (nTypeSize+2);
}

icUInt32Number CIccSparseMatrix::MemSize(icUInt32Number nMaxEntries, icUInt16Number nRows, icUInt8Number nTypeSize)
{
  icUInt32Number off = ((4 + 4*(nRows+1) + 2*nMaxEntries + (nTypeSize-1))/nTypeSize)*nTypeSize;
  return off + nTypeSize*nMaxEntries;
}

icUInt8Number CIccSparseMatrix::EntrySize(icSparseMatrixType nType)
{
  switch(nType) {
    case icSparseMatrixUInt8:
      return sizeof(icUInt8Number);

    case icSparseMatrixUInt16:
      return sizeof(icUInt16Number);

    case icSparseMatrixFloat16:
      return sizeof(icFloat16Number);

    case icSparseMatrixFloat32:
      return sizeof(icFloat16Number);

    case icSparseMatrixFloatNum:
      return sizeof(icFloatNumber);

    default:
      break;
  }

  return 0;
}
