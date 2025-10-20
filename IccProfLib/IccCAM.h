/** @file
    File:       IccCAM.h

    Contains:   Implementation of the ICC CAM color transform

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
// -Initial implementation by Marc Mahy 5-15-2012
// -Refactoring to match IccProfLib by Max Derhak 5-31-2012
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCCAM_H)
#define _ICCCAM_H

#include "IccDefs.h"
#include "IccIO.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//: ICCCAM transformation Object definition

class ICCPROFLIB_API CIccCamConverter
{
private:
	// parameters color space
	icFloatNumber								m_WhitePoint[3];
	icFloatNumber								m_La;	/* Absolute luminance of adapting field in cd/m^2 */
	icFloatNumber								m_Yb;	/* relative luminance of background in cd/m^2 */
	icFloatNumber								m_c;	/* impact surround */
	icFloatNumber								m_Nc;	/* chromatic induction factor */
	icFloatNumber								m_F;	/* degree of adaptation factor */
	icFloatNumber								m_mFor[3][3];
	icFloatNumber								m_mInv[3][3];
	icFloatNumber								m_factor;

  //Additional non-provided parameters
  //Yw is relative luminance of adapting field in cd/m^2
  //LW is absolute luminance of the reference white in cd/m^2

	// parameters precalculations
	icFloatNumber								m_rgbWhite[3];
	icFloatNumber								m_D;
	icFloatNumber								m_Fl;
	icFloatNumber								m_n;
	icFloatNumber								m_Nbb;
	icFloatNumber								m_z;
	icFloatNumber								m_AWhite;

	// parameters for corrected hyperbolic function
	icFloatNumber								m_x0;
	icFloatNumber								m_cc;
	icFloatNumber								m_alfa;
	icFloatNumber								m_exp;

	// helper functions
	void	Multiply_vect_by_mx (const icFloatNumber	*in, icFloatNumber *out, icFloatNumber m[3][3]);

	icFloatNumber	F_Function (icFloatNumber	x);
	icFloatNumber	H_Function (icFloatNumber	x);
	icFloatNumber	H_FunctionInv (icFloatNumber y);

	icFloatNumber	IccCam_e (icFloatNumber hue);
	icFloatNumber	Hyperbolic (icFloatNumber	x);
	icFloatNumber	HyperbolicInv (icFloatNumber	x);
	void	ReferenceConditions (icFloatNumber* rgb, icFloatNumber* rgbC);
	void	ReferenceConditionsInv (icFloatNumber* rgbC, icFloatNumber* rgb);

  void ab2chDeg(icFloatNumber a, icFloatNumber b, icFloatNumber &c, icFloatNumber &h);

	void	CalcCoefficients ();

public:
	CIccCamConverter();
	~CIccCamConverter();
  CIccCamConverter& operator=(const CIccCamConverter &camcon);
  CIccCamConverter* NewCopy() const;

	void	JabToXYZ (const icFloatNumber* jab, icFloatNumber* xyz, int nbr);
	void	XYZToJab (const icFloatNumber* xyz, icFloatNumber* jab, int nbr);

	void	SetParameter_WhitePoint (icFloatNumber*	whitePoint);
  //Set absolute luminance of adapting field La
	void	SetParameter_La (icFloatNumber	La);
  //Set relative luminance of background Yb
	void	SetParameter_Yb (icFloatNumber	YB);
  //Set impact of surround
	void	SetParameter_C (icFloatNumber	c);
	//Set chromatic induction factor
  void	SetParameter_Nc (icFloatNumber	Nc);
	//Set factor determining degree of adaptation
  void	SetParameter_F (icFloatNumber	F);
	void	GetParameter_WhitePoint (icFloatNumber*	whitePoint);
	icFloatNumber	GetParameter_La ();
	icFloatNumber	GetParameter_Yb ();
	icFloatNumber	GetParameter_C ();
	icFloatNumber	GetParameter_Nc ();
	icFloatNumber	GetParameter_F ();
};


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCCAM_H)
