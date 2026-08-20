/** @file
    File:       IccSolve.cpp

    Contains:   Implementation of Matrix Solver

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.1
 *
 *
 * Copyright (c) 2003-2006 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 4-12-2016
//
//////////////////////////////////////////////////////////////////////

#include "IccSolve.h"
#include "IccUtil.h"
#include <cstring>

#ifdef ICC_USE_EIGEN_SOLVER
#include<Eigen/Core>
#include<Eigen/SVD>

template<typename _Matrix_Type_, typename _Vector_Type_>
_Vector_Type_ solve(const _Matrix_Type_ &a, const _Vector_Type_ y, icFloatNumber epsilon = std::numeric_limits<icFloatNumber>::epsilon())
{
  Eigen::JacobiSVD< _Matrix_Type_ > svd(a ,Eigen::ComputeThinU | Eigen::ComputeThinV);
  icFloatNumber tolerance = epsilon * std::max(a.cols(), a.rows()) *svd.singularValues().array().abs()(0);
  return svd.matrixV() *  (svd.singularValues().array().abs() > tolerance).select(svd.singularValues().array().inverse(), 0).matrix().asDiagonal() * svd.matrixU().adjoint() * y;
}


typedef Eigen::Matrix<icFloatNumber, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> MatrixXn;
typedef Eigen::Matrix<icFloatNumber, Eigen::Dynamic, 1> VectorXn;

class CIccEigenMatrixSolver : public IIccMatrixSolver
{
public:
  CIccEigenMatrixSolver() {}

  /**
  ****************************************************************************
  * Member Function: Solve
  * 
  * Purpose: Solve for x in the matrix/vector equation y=Mx
  * 
  * Parameters:
  *  -dMatrix is a matrix (M) with nRows x nCols entries in RowOrder
  *  -dYVector is a column vector (y) with nRows entries
  *  -dXVecotr is a column vector (x) with nCols entries
  *  
  * Return:
  *  true if solution was found or false if solution not possible.
  *****************************************************************************
  */
  virtual bool Solve(icFloatNumber *dXVector, const icFloatNumber *dMatrix, const icFloatNumber *dYVector, icUInt16Number nRows, icUInt16Number nCols)
  {
    if (nRows==3 && nCols==3) {
      icFloatNumber mtx[9];
      memcpy(mtx, dMatrix, 9*sizeof(icFloatNumber));
      bool bInvertible = icMatrixInvert3x3(mtx);
      if (bInvertible)
        icVectorApplyMatrix3x3(dXVector, mtx, dYVector);
      else
        memset(dXVector, 0, nCols*sizeof(icFloatNumber));

      return bInvertible;
    }

    VectorXn x;
    VectorXn y = Eigen::Map<VectorXn>((icFloatNumber*)dYVector, nRows);
    MatrixXn A = Eigen::Map<MatrixXn>((icFloatNumber*)dMatrix, nRows, nCols);

    x=solve(A,y);

    Eigen::Map<VectorXn>(dXVector, nCols) = x;

    return true;
  }
};

CIccEigenMatrixSolver g_EigenSolver;

//Define the global g_pIccMatrixSolver variable pointer
ICCPROFLIB_DATA_API IIccMatrixSolver *g_pIccMatrixSolver = &g_EigenSolver;

#else

class CIccSimpleMatrixSolver : public IIccMatrixSolver
{
public:
  CIccSimpleMatrixSolver() {}

  /**
  ****************************************************************************
  * Member Function: Solve
  * 
  * Purpose: Solve for x in the matrix/vector equation y=Mx when M is 3x3 matrix
  *  Otherwise return false.
  * 
  * Parameters:
  *  -dMatrix is a matrix (M) with nRows x nCols entries in RowOrder
  *  -dYVector is a column vector (y) with nRows entries
  *  -dXVecotr is a column vector (x) with nCols entries
  *  
  * Return:
  *  true if solution was found or false if solution not possible/supported.
  *****************************************************************************
  */
  virtual bool Solve(icFloatNumber *dXVector, const icFloatNumber *dMatrix, const icFloatNumber *dYVector, icUInt16Number nRows, icUInt16Number nCols)
  {
    if (nRows==3 && nCols==3) {
      icFloatNumber mtx[9];
      memcpy(mtx, dMatrix, 9*sizeof(icFloatNumber));
      bool bInvertible = icMatrixInvert3x3(mtx);
      if (bInvertible)
        icVectorApplyMatrix3x3(dXVector, mtx, dYVector);
      else
        memset(dXVector, 0, nCols*sizeof(icFloatNumber));

      return bInvertible;
    }

    return false;
  }
};

CIccSimpleMatrixSolver g_SimpleSolver;

//Define the global g_pIccMatrixSolver variable pointer
ICCPROFLIB_DATA_API IIccMatrixSolver *g_pIccMatrixSolver = &g_SimpleSolver;

#endif



/**
****************************************************************************
* Name:  IccSetMatrixSolver(IIccMatrixSolver *pSolver)
* 
* Purpose: 
*  Global function that can be used by a supporting application to
*  establish an implementation of a matrix solver object.  
*****************************************************************************
*/
void ICCPROFLIB_API IccSetMatrixSolver(IIccMatrixSolver *pIccMatrixSolver)
{
  g_pIccMatrixSolver = pIccMatrixSolver;
}


/**
****************************************************************************
* Name:  IccGetDefaultMatrixSolver()
* 
* Purpose: 
*  Global function that can be used by a supporting application to
*  determine the default matrix solver object.  
*****************************************************************************
*/
IIccMatrixSolver *IccGetDefaultMatrixSolver()
{
  return g_pIccMatrixSolver;
}

//TODO implement IIccMatrixInverter in terms of Eigen library

class CIccSimpleMatrixInverter : public IIccMatrixInverter
{
public:
  CIccSimpleMatrixInverter() {}

  /**
  ****************************************************************************
  * Member Function: Invert
  *
  * Purpose: Solve for x in the matrix/vector equation y=Mx when M is 3x3 matrix
  *  Otherwise return false.
  *
  * Parameters:
  *  -dMatrix is a matrix (M) with nRows x nCols entries in RowOrder
  *  -nRows is number of row entries
  *  -nCols is number of column entries
  *
  * Return:
  *  true if matrix was inverted or false if not possible/supported.
  *****************************************************************************
  */
  virtual bool Invert(icFloatNumber *dMatrix, icUInt16Number nRows, icUInt16Number nCols)
  {
    if (nRows == 3 && nCols == 3) {
      bool bInvertible = icMatrixInvert3x3(dMatrix);

      return bInvertible;
    }

    return false;
  }
};

CIccSimpleMatrixInverter g_SimpleInverter;

//Define the global g_pIccMatrixSolver variable pointer
ICCPROFLIB_DATA_API IIccMatrixInverter *g_pIccMatrixInverter = &g_SimpleInverter;



/**
****************************************************************************
* Name:  IccSetMatrixInverter(IIccMatrixInverter *pInverter)
*
* Purpose:
*  Global function that can be used by a supporting application to
*  establish an implementation of a matrix inverter object.
*****************************************************************************
*/
void ICCPROFLIB_API IccSetMatrixInverter(IIccMatrixInverter *pIccMatrixInverter)
{
  g_pIccMatrixInverter = pIccMatrixInverter;
}


/**
****************************************************************************
* Name:  IccGetDefaultMatrixInverter()
*
* Purpose:
*  Global function that can be used by a supporting application to
*  determine the default matrix inverter object.
*****************************************************************************
*/
IIccMatrixInverter *IccGetDefaultMatrixInverter()
{
  return g_pIccMatrixInverter;
}
