 /** @file
    File:       IccCAM.h

    Contains:   Implementation of the ICC CAM color transform

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights 
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
// -Initial implementation by Marc Mahy 5-15-2003
// -Refactoring to match IccProfLib by Max Derhak 5-31-2012
//
//////////////////////////////////////////////////////////////////////

#include "IccCAM.h"
#include "IccUtil.h"
#include <cmath>
#include <cstring>
#include <string>

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::Multiply_vect_by_mx (const icFloatNumber	*in,
											 icFloatNumber	*out,
											 icFloatNumber	m[3][3])
{
	out[0] = m[0][0] * in[0] + m[0][1] * in[1] + m[0][2] * in[2];
	out[1] = m[1][0] * in[0] + m[1][1] * in[1] + m[1][2] * in[2];
	out[2] = m[2][0] * in[0] + m[2][1] * in[1] + m[2][2] * in[2];
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::ReferenceConditions (icFloatNumber	*rgb,
											 icFloatNumber	*rgbC)
{
	rgbC[0] = (icFloatNumber)((m_D * m_WhitePoint[1] / m_rgbWhite[0] + 1.0 - m_D) * rgb[0]);
	rgbC[1] = (icFloatNumber)((m_D * m_WhitePoint[1] / m_rgbWhite[1] + 1.0 - m_D) * rgb[1]);
	rgbC[2] = (icFloatNumber)((m_D * m_WhitePoint[1] / m_rgbWhite[2] + 1.0 - m_D) * rgb[2]);
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::ReferenceConditionsInv (icFloatNumber *rgbC,
												 icFloatNumber *rgb)
{
	rgb[0] = (icFloatNumber)(rgbC[0] / (m_D * m_WhitePoint[1] / m_rgbWhite[0] + 1.0 - m_D));
	rgb[1] = (icFloatNumber)(rgbC[1] / (m_D * m_WhitePoint[1] / m_rgbWhite[1] + 1.0 - m_D));
	rgb[2] = (icFloatNumber)(rgbC[2] / (m_D * m_WhitePoint[1] / m_rgbWhite[2] + 1.0 - m_D));
}

/*---------------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::F_Function (icFloatNumber	x)
{
	icFloatNumber	y;
	y = (icFloatNumber) (400.0 * pow ((double)x, 0.42) / (27.13 + pow ((double)x, 0.42)));
	return y;
}

/*---------------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::H_Function (icFloatNumber	x)
{
	icFloatNumber	y;

	y = (icFloatNumber) (400.0 * pow ((double)x, (double)m_exp) / (27.13 + pow ((double)x, (double)m_exp)));

	return y;
}

/*---------------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::H_FunctionInv (icFloatNumber	y)
{
	icFloatNumber	x;

	x = (icFloatNumber) (pow (27.13*y / (400.0-y), 1.0/m_exp));

	return x;
}

/*---------------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::Hyperbolic (icFloatNumber	x)
{
	icFloatNumber	y;

	if (x < 0)
	{
		if (-x <= m_x0)
		{
			y = m_cc * x;
		}
		else
		{
			y = - ((1 + m_alfa) * H_Function (-x) / H_Function (m_Fl) - m_alfa) * F_Function (m_Fl);
		}
	}
	else
	{
		if (x <= m_x0)
		{
			y = m_cc * x;
		}
		else
		{
			y = ((1 + m_alfa) * H_Function (x) / H_Function (m_Fl) - m_alfa) * F_Function (m_Fl);
		}
	}

	return y;
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::HyperbolicInv (icFloatNumber y)
{
	icFloatNumber	x, h_y;

	if (y < 0)
	{
		if (-y <= (m_cc * m_x0))
		{
			x = y / m_cc;
		}
		else
		{
			h_y = (- y / F_Function (m_Fl) + m_alfa) / (1 + m_alfa) * H_Function (m_Fl);
			x = - H_FunctionInv (h_y);
		}
	}
	else
	{
		if (y <= (m_cc * m_x0))
		{
			x = y / m_cc;
		}
		else
		{
			h_y = (y / F_Function (m_Fl) + m_alfa) / (1 + m_alfa) * H_Function (m_Fl);
			x = H_FunctionInv (h_y);
		}
	}

	return x;
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::IccCam_e (icFloatNumber hue)
{
	int		i, j;
	icFloatNumber		offset;
	double	hdef[4] = {20.14, 90.00, 164.25, 237.53};
	double	edef[4] = {0.8, 0.7, 1.0, 1.2};

	for (i = 0; i < 4; i++)
		if (hue < hdef[i])
			break;
		
	j = i - 1;
	if (i == 0)
		j = 3;
	else if (i == 4)
		i = 0;
		
	if (i == 0)
	{
		offset = 360.0;

		if (hue <= hdef[0])
			hue += 360.0;
	}
	else
		offset = 0;

	return (icFloatNumber)(edef[j] + (edef[i] - edef[j]) * (hue - hdef[j]) / (offset + hdef[i] - hdef[j]));
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::CalcCoefficients ()
{
	icFloatNumber	k, La5, rgbC[3], rgbP[3];

	double epow = pow (2.7182818, (-((double)m_La + 42.0)/92.0) );
	m_D = (icFloatNumber) (m_F *(1.0 - (epow/ 3.6)) );

	k = 1.0f / (5.0f * m_La + 1.0f);
	k = (icFloatNumber)(pow ((double)k, 4.0));
	La5 = 5.0f * m_La;
	m_Fl = (icFloatNumber)(0.2 * k * La5 + 0.1 * (1 - k) * (1 - k) * pow ((double)La5, 0.333333));

	m_n = m_Yb / m_WhitePoint[1];

	m_factor = (icFloatNumber) pow (1.64 - pow (0.29, (double) m_n), 0.73);

	m_Nbb = (icFloatNumber)(0.725 / pow ((double)(m_n), 0.2));

	m_z = (icFloatNumber)(1.48 + sqrt (m_n));

	Multiply_vect_by_mx (m_WhitePoint, m_rgbWhite, m_mFor);

	ReferenceConditions (m_rgbWhite, rgbC);

	// because CIECAT02 == HPE
	rgbP[0] = rgbC[0];
	rgbP[1] = rgbC[1];
	rgbP[2] = rgbC[2];
 
    // Hyperbolic function uses the m_x0 and m_cc values, so they must be initialized first
//	m_x0 = (icFloatNumber) (m_Fl * 0.25 / 255.0);
// m_x0 = (icFloatNumber) (m_Fl * 1.00 / 255.0)
	m_x0 = (icFloatNumber) (m_Fl * 4.00 / 255.0);
	m_cc = ((1 + m_alfa) * H_Function (m_x0) / H_Function (m_Fl) - m_alfa) * F_Function (m_Fl) / m_x0;

	rgbP[0] = Hyperbolic (m_Fl * rgbP[0] / 100) + 0.1f;
	rgbP[1] = Hyperbolic (m_Fl * rgbP[1] / 100) + 0.1f;
	rgbP[2] = Hyperbolic (m_Fl * rgbP[2] / 100) + 0.1f;
	m_AWhite = (icFloatNumber)((2.0 * rgbP[0] + rgbP[1] + rgbP[2] / 20.0 - 0.305) * m_Nbb);
}


void CIccCamConverter::ab2chDeg(icFloatNumber a, icFloatNumber b, icFloatNumber &c, icFloatNumber &h)
{
  c = sqrt(a*a + b*b);
  h = (icFloatNumber)(atan2(b, a) * 180.0 / icPiNum);
  while (h<0.0)
    h+=360.0;
}


/* ---------------------------------------------------------------------------------*/

CIccCamConverter::CIccCamConverter ()
{
//	for XYZwhite = D50, La = 500, Yb = 20, average surround

	/* the initializations are based on values for an average surround */
	m_La = (icFloatNumber) 500.0;
	m_Yb = (icFloatNumber)  20.0;

  m_WhitePoint[0] = icD50XYZ[0];
  m_WhitePoint[1] = icD50XYZ[1];
  m_WhitePoint[2] = icD50XYZ[2];

	/* the initializations are based on values for an average surround */
	m_c  = (icFloatNumber) 0.69;	/* impact surround */
	m_Nc = (icFloatNumber) 1.00;	/* chromatic induction factor */
	m_F  = (icFloatNumber) 1.00;	/* degree of adaptation factor */

	// ciecat02 = HPE
	m_mFor[0][0] =  0.38970f;	m_mFor[0][1] = 0.68898f;	m_mFor[0][2] = -0.07868f;
	m_mFor[1][0] = -0.22981f;	m_mFor[1][1] = 1.18340f;	m_mFor[1][2] =  0.04641f;
	m_mFor[2][0] =  0.00000f;	m_mFor[2][1] = 0.00000f;	m_mFor[2][2] =  1.00000f;

	m_mInv[0][0] = 1.910197f;	m_mInv[0][1] = -1.1121240f;	m_mInv[0][2] =  0.201908f;
	m_mInv[1][0] = 0.370950f;	m_mInv[1][1] =  0.6290540f;	m_mInv[1][2] =  0.000008f;
	m_mInv[2][0] = 0.000000f;	m_mInv[2][1] =  0.0000000f;	m_mInv[2][2] =  1.000000f;

	/* precalculated values */
// m_x0 = 0.25 / 255
//	m_alfa = 0.041747f;
//	m_exp = 0.396f;

// m_x0 = 1 / 255
//	m_alfa = 0.089914f;
//	m_exp = 0.372f;

//	m_x0 = 4 / 255
//	m_alfa = 0.2307f;
//	m_exp = 0.3169f;

	m_alfa = 0.2307f;
	m_exp = 0.3169f;

	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

CIccCamConverter::~CIccCamConverter ()
{
}


CIccCamConverter* CIccCamConverter::NewCopy() const
{
  CIccCamConverter *rv = new CIccCamConverter();
  *rv = *this;
  return rv;
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::SetParameter_WhitePoint (icFloatNumber*	whitePoint)
{
	m_WhitePoint[0] = whitePoint[0];
	m_WhitePoint[1] = whitePoint[1];
	m_WhitePoint[2] = whitePoint[2];
	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::SetParameter_La (icFloatNumber	La)
{
	m_La = La;
	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::SetParameter_Yb (icFloatNumber	Yb)
{
	m_Yb = Yb;
	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::SetParameter_C (icFloatNumber	c)
{
	m_c = c;
	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::SetParameter_Nc (icFloatNumber	Nc)
{
	m_Nc = Nc;
	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::SetParameter_F (icFloatNumber	F)
{
	m_F = F;
	CalcCoefficients ();
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::GetParameter_WhitePoint (icFloatNumber*	whitePoint)
{
	whitePoint[0] = m_WhitePoint[0];
	whitePoint[1] = m_WhitePoint[1];
	whitePoint[2] = m_WhitePoint[2];
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::GetParameter_La ()
{
	return m_La;
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::GetParameter_Yb ()
{
	return m_Yb;
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::GetParameter_C ()
{
	return m_c;
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::GetParameter_Nc ()
{
	return m_Nc;
}

/*---------------------------------------------------------------------------------*/

icFloatNumber
CIccCamConverter::GetParameter_F ()
{
	return m_F;
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::XYZToJab (const icFloatNumber*	xyz,
							  icFloatNumber*	jab,
							  int		nbr)
{
	int		k;
	icFloatNumber		rgb[3], rgbC[3], rgbP[3];
	icFloatNumber		la, lb, lc, lchroma, lh, lhr, A, J, et, t, C, ac, bc;
	const icFloatNumber*	h_xyz;
	icFloatNumber*	h_jab;

	h_xyz = xyz;
	h_jab = jab;

	for (k=0;k<nbr;k++)
	{
		Multiply_vect_by_mx (h_xyz, rgb, m_mFor);

		// clipping to the HPE triangle
		if (rgb[0] < 0) rgb[0] = 0.0f;
		if (rgb[1] < 0) rgb[1] = 0.0f;
		if (rgb[2] < 0) rgb[2] = 0.0f;

		ReferenceConditions (rgb, rgbC);

		rgbP[0] = Hyperbolic (m_Fl * rgbC[0] / 100) + 0.1f;
		rgbP[1] = Hyperbolic (m_Fl * rgbC[1] / 100) + 0.1f;
		rgbP[2] = Hyperbolic (m_Fl * rgbC[2] / 100) + 0.1f;

		la = (icFloatNumber)(rgbP[0] - 12.0 * rgbP[1] / 11.0 + rgbP[2] / 11.0);
		lb = (icFloatNumber)((rgbP[0] + rgbP[1] - 2.0 * rgbP[2] ) / 9.0);
		lchroma = (icFloatNumber)(sqrt (la * la + lb * lb));

		ab2chDeg (la, lb, lc, lh);
		lhr = lh * (icFloatNumber)icPiNum / 180.0f;

		A = (icFloatNumber)((2.0 * rgbP[0] + rgbP[1] + rgbP[2] / 20.0 - 0.305) * m_Nbb);

		J = (icFloatNumber)(100.0 * pow (A / m_AWhite, m_c * m_z));

		et = (icFloatNumber)((cos(lhr + 2.0) + 3.8) / 4.0);
		t = (icFloatNumber)(50.0 * lchroma * 100 * et  * 10.0/13.0 * m_Nc * m_Nbb / (rgbP[0]+rgbP[1]+21.0/20.0*rgbP[2]));
		C = (icFloatNumber)(pow((double)t, 0.9) * sqrt ((double)(J/100.0)) * m_factor);

		ac = (icFloatNumber)(C * cos(lhr));
		bc = (icFloatNumber)(C * sin(lhr));

		h_jab[0] = J;
		h_jab[1] = ac;
		h_jab[2] = bc;

		h_xyz += 3;
		h_jab += 3;
	}
}

/*---------------------------------------------------------------------------------*/

void
CIccCamConverter::JabToXYZ (const icFloatNumber*	jab,
							  icFloatNumber*	xyz,
							  int		nbr)
{
	int		i;
	icFloatNumber		rgb[3], rgbP[3], rgbC[3];
	icFloatNumber		lc, lh, lhr, et, p1, p2, p4, p5, p3, numerator;
	icFloatNumber		value, a, b, cotan, tanValue;
	double	exp = 1.0 / 0.9;
	double	A, ratio, C, t;
	const icFloatNumber*	h_jab;
	icFloatNumber*	h_xyz;

	h_jab = jab;
	h_xyz = xyz;

	for (i=0;i<nbr;i++)
	{
		if (h_jab[0] < 1.0e-5)
		{
			rgbP[0] = 0.1f;
			rgbP[1] = 0.1f;
			rgbP[2] = 0.1f;
		}
		else
		{
			C = sqrt (h_jab[1] * h_jab[1] + h_jab[2] * h_jab[2]);

			ratio = C / (sqrt (h_jab[0]/100.0) * m_factor);
			t = pow (ratio, exp);
			A = m_AWhite * pow (h_jab[0]/100.0, 1.0/(m_c*m_z));

			p2 = (icFloatNumber) ((A / m_Nbb + 0.305f) * 460.0 / 1403.0);

			if (t < 1.0e-5)
			{
				rgbP[0] = p2;
				rgbP[1] = p2;
				rgbP[2] = p2;
			}
			else
			{
				ab2chDeg (h_jab[1], h_jab[2], lc, lh);
				lhr = lh * (icFloatNumber)icPiNum / 180.0f;

				et = (icFloatNumber)((cos ((double)lhr + 2.0) + 3.8) / 4.0);

				p1 = (icFloatNumber)((50000.0f / 13.0f) * m_Nc * m_Nbb * et / t);
				p3 = 21.0f / 20.0f;

				numerator = p2 * (2.0f + p3);

				if ((lh <= 45) || (lh >= 315) || ((lh > 135) && (lh < 225))) /* |a| > |b| */
				{
					tanValue = (icFloatNumber)(tan ((double)lhr));

					if ((lh > 90) && (lh < 270))
						value = (icFloatNumber)(- pow (1 + pow ((double)tanValue, 2.0), 0.5));
					else
						value = (icFloatNumber)(pow (1 + pow ((double)tanValue, 2.0), 0.5));

					p5 = (icFloatNumber)(p1 * value);

					a = (icFloatNumber)(numerator / (p5 + (2.0f+p3)*(220.0f/1403.0f) - (27.0f/1403.0f - p3*(6300.0f/1403.0f)) * tanValue));
					b = a * tanValue;
				}
				else /* |b| > |a| */
				{
					if (fabs (lh - 90) < 1.0e-5)
						cotan = 0.0f;
					else if (fabs (lh - 270) < 1.0e-5)
						cotan = 0.0f;
					else
						cotan = (icFloatNumber)(1 / tan ((double)lhr));

					if (lh > 180)
						value = (icFloatNumber)(- pow (1 + pow ((double)cotan , 2.0), 0.5));
					else
						value = (icFloatNumber)(pow (1 + pow ((double)cotan, 2.0), 0.5));

					p4 = (icFloatNumber)(p1 * value);

					b = (icFloatNumber)(numerator / (p4 + (2.0f+p3)*(220.0f/1403.0f) * cotan - 27.0f/1403.0f + p3*(6300.0f/1403.0f)));
					a = b * cotan;
				}

				rgbP[0] = (icFloatNumber)(p2 + ( 451.0 * a +  288.0 * b) / 1403.0);
				rgbP[1] = (icFloatNumber)(p2 + (-891.0 * a -  261.0 * b) / 1403.0);
				rgbP[2] = (icFloatNumber)(p2 + (-220.0 * a - 6300.0 * b) / 1403.0);
			}
		}

		if (rgbP[0] < 0) rgbP[0] = 0.0f;
		if (rgbP[1] < 0) rgbP[1] = 0.0f;
		if (rgbP[2] < 0) rgbP[2] = 0.0f;

		rgbC[0] = 100 * HyperbolicInv (rgbP[0] - 0.1f) / m_Fl;
		rgbC[1] = 100 * HyperbolicInv (rgbP[1] - 0.1f) / m_Fl;
		rgbC[2] = 100 * HyperbolicInv (rgbP[2] - 0.1f) / m_Fl;

		ReferenceConditionsInv (rgbC, rgb);
		Multiply_vect_by_mx (rgb, h_xyz, m_mInv);

		h_jab += 3;
		h_xyz += 3;
	}
}

CIccCamConverter& CIccCamConverter::operator=(const CIccCamConverter &camcon)
{
  memcpy(&m_WhitePoint, &camcon.m_WhitePoint, sizeof(m_WhitePoint));
  m_La=camcon.m_La;	/* Absolute luminance of adapting field in cd/m^2 */
  m_Yb=camcon.m_Yb;	/* relative luminance of background in cd/m^2 */
  m_c=camcon.m_c;	/* impact surround */
  m_Nc=camcon.m_Nc;	/* chromatic induction factor */
  m_F=camcon.m_F;	/* degree of adaptation factor */
  memcpy(&m_mFor, &camcon.m_mFor, sizeof(m_mFor));
  memcpy(&m_mInv, &camcon.m_mInv, sizeof(m_mInv));
  m_factor=camcon.m_factor;

  //Additional non-provided parameters
  //Yw is relative luminance of adapting field in cd/m^2
  //LW is absolute luminance of the reference white in cd/m^2

  // parameters precalculations
  memcpy(m_rgbWhite, camcon.m_rgbWhite, sizeof(m_rgbWhite));
  m_D=camcon.m_D;
  m_Fl=camcon.m_Fl;
  m_n=camcon.m_n;
  m_Nbb=camcon.m_Nbb;
  m_z=camcon.m_z;
  m_AWhite=camcon.m_AWhite;

  // parameters for corrected hyperbolic function
  m_x0=camcon.m_x0;
  m_cc=camcon.m_cc;
  m_alfa=camcon.m_alfa;
  m_exp=camcon.m_exp;

  return *this;
}

/*---------------------------------------------------------------------------------*/
