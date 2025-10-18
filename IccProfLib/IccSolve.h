/** @file
File:       IccSolve.h

Contains:   Header for implementation Matrix Solve functionality

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
// -April 12, 2016 
//  Initial CIccSolve development
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCSOLVE_H
#define _ICCSOLVE_H

#include "IccDefs.h"

/**
****************************************************************************
* Structure: IIccMatrixSolver
* 
* Purpose: Define an interface for allowing external implementation of 
* matrix inversion
*****************************************************************************
*/
class IIccMatrixSolver
{
public:
  /**
  ****************************************************************************
  * Member Function: Solve
  * 
  * Purpose: Solve for x in the matrix/vector equation y=Mx
  * 
  * Parameters:
  *  -dMatrix is a matrix (M) with nRows x nCols entries
  *  -dYVector is a column vector (y) with nRows entries
  *  -dXVecotr is a column vector (x) with nCols entries
  *  
  * Return:
  *  true if solution was found or false if solution not possible.
  *****************************************************************************
  */
  virtual bool Solve(icFloatNumber *dXVector, const icFloatNumber *dMatrix, const icFloatNumber *dYVector, icUInt16Number nRows, icUInt16Number nCols)=0;
};

/**
****************************************************************************
* Global Variable: g_pIccMatrixSolver
* 
* Purpose: Keep tracks of pointer to matrix solver object.  This object
* should be instantiated before making calls to CIccMpeCalculator.  If this
* variable is not set then the SOLV operator will return a zero vector with
* with the status of false.
*****************************************************************************
*/
ICCPROFLIB_API extern IIccMatrixSolver *g_pIccMatrixSolver;

/**
****************************************************************************
* Function: IccSetMatrixSolver(IIccMatrixSolver *pSolver)
* 
* Purpose:
*  Global function that can be used by a supporting application to
*  establish an implementation of a matrix solver object.  
*****************************************************************************
*/
void ICCPROFLIB_API IccSetMatrixSolver(IIccMatrixSolver *pSolver);

/**
****************************************************************************
* Function: icGetDefaultSolver(IIccMatrixSolver *pSolver)
* 
* Purpose:
*  Global function that can be used by a supporting application to
*  determine the matrix solver object.  
*****************************************************************************
*/
IIccMatrixSolver ICCPROFLIB_API *IccGetDefaultMatrixSolver();

/**
****************************************************************************
* Structure: IIccMatrixInverter
*
* Purpose: Define an interface for allowing external implementation of
* matrix inversion
*****************************************************************************
*/
class IIccMatrixInverter
{
public:
  /**
  ****************************************************************************
  * Member Function: Invert
  *
  * Purpose: Invert a square matrix
  *
  * Parameters:
  *  -dMatrix is a matrix (M) with nRows x nCols entries
  *  -nRows is number of rows in matrix
  *  -nCols is number of columns in matrix
  *
  * Return:
  *  true if matrix was inverted or false if not possible.
  *****************************************************************************
  */
  virtual bool Invert(icFloatNumber *dMatrix, icUInt16Number nRows, icUInt16Number nCols) = 0;
};

/**
****************************************************************************
* Global Variable: g_pIccMatrixInverter
*
* Purpose: Keep tracks of pointer to matrix inverter object.  
*****************************************************************************
*/
ICCPROFLIB_API extern IIccMatrixInverter *g_pIccMatrixInverter;

/**
****************************************************************************
* Function: IccSetMatrixInverter(IIccMatrixInverter *pInverter)
*
* Purpose:
*  Global function that can be used by a supporting application to
*  establish an implementation of a matrix inverter object.
*****************************************************************************
*/
void ICCPROFLIB_API IccSetMatrixInverter(IIccMatrixInverter *pInverter);

/**
****************************************************************************
* Function: icGetDefaultInverter(IIccMatrixInverter *pInverter)
*
* Purpose:
*  Global function that can be used by a supporting application to
*  determine the matrix solver object.
*****************************************************************************
*/
IIccMatrixInverter ICCPROFLIB_API *IccGetDefaultMatrixInverter();

#endif //_ICCSOLVE_H

