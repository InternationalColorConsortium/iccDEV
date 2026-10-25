/** @file
File:       IccApplyBPC.h

Contains:   Header file for implementation of Black Point Compensation calculations.

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
// -Initial implementation by Rohit Patil 12-8-2008
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCAPPLYBPC_H)
#define _ICCAPPLYBPC_H

#include "IccCmm.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*		Interface and hint for creating a BPC xform object
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyBPCHint : public CIccCreateAdjustPCSXformHint
{
public:
	virtual const char *GetAdjustPCSType() const { return "CIccApplyBPCHint"; }
	virtual IIccAdjustPCSXform* GetNewAdjustPCSXform() const;
};

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: This is the hint for applying black point compensation.
 *					Also does the calculations to setup actual application of BPC.
 * 
 **************************************************************************
*/
class ICCPROFLIB_API CIccApplyBPC : public IIccAdjustPCSXform
{
public: 
	// virtual IIccAdjustPCSXform functions
	// does all the calculations for BPC and returns the scale and offset in the arguments passed
	virtual bool CalcFactors(const CIccProfile* pProfile, const CIccXform* pXfm, icFloatNumber* Scale, icFloatNumber* Offset) const;

private:
	// utility functions
	void lab2pcs(icFloatNumber* pixel, const CIccProfile* pProfile) const;
	void pcs2lab(icFloatNumber* pixel, const CIccProfile* pProfile) const;
	icFloatNumber calcsum(icFloatNumber* x, icFloatNumber* y, int n, int j, int k) const;
	icFloatNumber calcQuadraticVertex(icFloatNumber* x, icFloatNumber* y, int n) const;

	// worker functions
	bool calcBlackPoint(const CIccProfile* pProfile, const CIccXform* pXform, icFloatNumber* XYZb) const;
	bool calcSrcBlackPoint(const CIccProfile* pProfile, const CIccXform* pXform, icFloatNumber* XYZb) const;
	bool calcDstBlackPoint(const CIccProfile* pProfile, const CIccXform* pXform, icFloatNumber* XYZb) const;

	// bUseD2BTags selects the same tag family the xform being adjusted was built
	// from (CIccXform::UseD2BTags()).  Estimating black through the other family
	// yields a black point from a pipeline the transform never applies.
	// nDstSize and nSrcSize are the capacities, in samples, of DstPixel and
	// SrcPixel.  A transform that would read or write more fails.
	bool pixelXfm(icFloatNumber *DstPixel, icUInt32Number nDstSize,
								icFloatNumber *SrcPixel, icUInt32Number nSrcSize,
								icColorSpaceSignature SrcSpace, icRenderingIntent nIntent,
								const CIccProfile *pProfile, bool bUseD2BTags) const;

	// PCS -> PCS round trip transform, always uses relative intent on the device -> pcs transform
	CIccCmm* getBlackXfm(icRenderingIntent nIntent, const CIccProfile *pProfile, bool bUseD2BTags) const;
};

#ifdef USEICCDEVNAMESPACE
}; //namespace iccDEV
#endif

#endif // _ICCAPPLYBPC_H

