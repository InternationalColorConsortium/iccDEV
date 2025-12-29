/*
    File:       IccUtil.cpp

    Contains:   Implementation of utility classes/functions

    Version:    V1

    Copyright:  � see ICC Software License
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
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#include "IccIO.h"
#include "IccUtil.h"
#include "IccTagFactory.h"
#include "IccStructFactory.h"
#include "IccArrayFactory.h"
#include "IccMpeFactory.h"
#include "IccDefs.h"
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define PI 3.1415926535897932384626433832795

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

ICCPROFLIB_API const char *icMsgValidateWarning = "Warning! - ";
ICCPROFLIB_API const char *icMsgValidateNonCompliant = "NonCompliant! - ";
ICCPROFLIB_API const char *icMsgValidateCriticalError = "Error! - ";
ICCPROFLIB_API const char* icMsgValidateInformation = "Information - ";


/**
 ******************************************************************************
* Name: icRealloc
* 
* Purpose: realloc memory allocated by malloc or calloc freeing old memory on
*  failure.
* 
* Args:
*  ptr - memory block to realloc
*  size - new size of memory block
* 
* Return: 
*  pointer to reallocated memory block or NULL on failure.  Old memory is
*  released on error.
******************************************************************************
*/void* icRealloc(void *ptr, size_t size)
{
  void *nptr;
  
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  
  if (ptr)
    nptr = realloc(ptr, size);
  else
    nptr = malloc(size);

  if (!nptr && ptr)
    free(ptr);

  return nptr;
}


/**
******************************************************************************
* Name: icIsNear
*
* Purpose: Checks if two numbers are close to each other in value
*
* Args:
*  v1 - first value
*  v2 - second value
*  range - 
*
* Return:
*  true if v1 is near v2 within range
******************************************************************************
*/
bool icIsNear(icFloatNumber v1, icFloatNumber v2, icFloatNumber nearRange /* = 1.0e-8 */)
{
  return fabs(v1 - v2) <= nearRange;
}


/**
******************************************************************************
* Name: icValidTagPos
*
* Purpose: Checks if pos is valid for data within a tag
*
* Args:
*  pos - first value
*  nTagHeaderSize - size of tag header
*  nTagSize - total size of tag data
*
* Return:
*  true pos is valid
******************************************************************************
*/
bool icValidTagPos(const icPositionNumber& pos, size_t nTagHeaderSize, size_t nTagSize, bool bAllowEmpty)
{
  if ((bAllowEmpty && !pos.size) || !pos.offset)
    return true;

  if (pos.offset < nTagHeaderSize)
    return false;
  
  if ( ((size_t)pos.offset + pos.size) > nTagSize)
    return false;

  if (!pos.size && !bAllowEmpty)
    return false;

  return true;
}


/**
******************************************************************************
* Name: icValidOverlap
*
* Purpose: Checks if pos overlap is acceptable
*
* Args:
*  pos1 - first positionNumber
*  pos2 - second positionNumber
*  bAllowSame - allow to positions to cover the same area
*
* Return:
*  true if v1 is near v2 within range
******************************************************************************
*/
bool icValidOverlap(const icPositionNumber& pos1, const icPositionNumber &pos2, bool bAllowSame)
{
  if ((icUInt64Number)pos1.offset + pos1.size <= (icUInt64Number)pos2.offset ||
      (icUInt64Number)pos1.offset >= (icUInt64Number)pos2.offset + pos2.size)
    return true;

// TODO - double check this logic!
  if ( (bAllowSame && pos1.offset == pos2.offset) || pos1.size == pos2.size)
    return true;

  return false;
}



/**
 ******************************************************************************
* Name: icRoundOffset
* 
* Purpose: Adds offset to floating point value for purposes of rounding
*  by casting to and integer based value
* 
* Args:
*  v - value to offset
* 
* Return: 
*  v with offset added - suitable for casting to some form of integer
******************************************************************************
*/
double icRoundOffset(double v)
{
  if (v < 0.0) 
    return v - 0.5;
  else
    return v + 0.5;
}


/**
 ******************************************************************************
 * Name: icMaxStatus
 * 
 * Purpose: return worst status 
 * 
 * Args:
 *  s1, s2
 * 
 * Return: 
 ******************************************************************************
 */
icValidateStatus icMaxStatus(icValidateStatus s1, icValidateStatus s2)
{
  if (s1>s2)
    return s1;
  return s2;
}

static icInt32Number icHexDigit(icChar digit)
{
  if (digit>='0' && digit<='9')
    return digit-'0';
  if (digit>='A' && digit<='F')
    return digit-'A'+10;
/*  if (digit>='a' && digit<='f')
    return digit-'a'+10;*/
  return -1;
}

bool icIsSpaceCLR(icColorSpaceSignature sig) 
{
  icChar szSig[5] = {0};
  szSig[0] = (icChar)(sig>>24);
  szSig[1] = (icChar)(sig>>16);
  szSig[2] = (icChar)(sig>>8);
  szSig[3] = (icChar)(sig);
  szSig[4] = '\0';

  if (szSig[0]=='n' && szSig[1]=='c')
    return true;
  else if (!strcmp(szSig+1, "CLR")) {
    icInt32Number d0 = icHexDigit(szSig[0]);
    if (d0>=1)
      return true;
  }
  else if (!strcmp(szSig+2, "CL")) {
    icInt32Number d0 = icHexDigit(szSig[0]);
    icInt32Number d1 = icHexDigit(szSig[1]);
    
    if (d0>=0 && d1>=0) {
      icInt32Number n = (d0<<4) + d1;

      if (n>0xf)
        return true;
    }
  }

  return false;
}

void icColorIndexName(icChar *szName, size_t nameSize, icColorSpaceSignature csSig,
                      int nIndex, int nColors, const icChar *szUnknown)
{
  icChar szSig[5] = {0};
  int i;

  if (csSig!=icSigUnknownData) {
    szSig[0] = (icChar)(csSig>>24);
    szSig[1] = (icChar)(csSig>>16);
    szSig[2] = (icChar)(csSig>>8);
    szSig[3] = (icChar)(csSig);
    szSig[4] = '\0';

    for (i=3; i>0; i--) {
      if (szSig[i]==' ')
        szSig[i]='\0';
    }
    if (nColors==1) {
      strcpy(szName, szSig);
    }
    else if ((size_t)nColors == strlen(szSig)) {
      snprintf(szName, nameSize, "%s_%c", szSig, szSig[nIndex]);
    }
    else {
      snprintf(szName, nameSize, "%s_%d", szSig, nIndex+1);
    }
  }
  else if (nColors==1) {
    strcpy(szName, szUnknown);
  }
  else {
    snprintf(szName, nameSize, "%s_%d", szUnknown, nIndex+1);
  }
}

void icColorValue(icChar *szValue, size_t nameSize, icFloatNumber nValue,
                  icColorSpaceSignature csSig, int nIndex,
                  bool bUseLegacy)
{
  if (csSig==icSigLabData) {
    if (!bUseLegacy) {
      if (!nIndex || nIndex>2)
        snprintf(szValue, nameSize, "%7.3lf", nValue * 100.0);
      else
        snprintf(szValue, nameSize, "%8.3lf", nValue * 255.0 - 128.0);
    }
    else {
      if (!nIndex || nIndex>2)
        snprintf(szValue, nameSize, "%7.3lf", nValue * 100.0 * 65535.0 / 65280.0);
      else
        snprintf(szValue, nameSize, "%8.3lf", nValue * 255.0 * 65535.0 / 65280.0 - 128.0);
    }
  }
  else if (csSig==icSigUnknownData) {
    snprintf(szValue, nameSize, "%8.5lf", nValue);
  }
  else {
    snprintf(szValue, nameSize, "%7.3lf", nValue * 100.0);
  }
}

static bool icIsS15Fixed16NumberNear(icS15Fixed16Number F, icFloatNumber D)
{
  //icFloatNumber v=icFtoD(F);

  return (icUInt32Number)(F*10000.0f + 0.5) == (icUInt32Number)(D*10000.0f + 0.5);
}

bool icIsIllumD50(icXYZNumber xyz)
{
  return icIsS15Fixed16NumberNear(xyz.X, 0.9642f) && 
         icIsS15Fixed16NumberNear(xyz.Y, 1.0000f) && 
         icIsS15Fixed16NumberNear(xyz.Z, 0.8249f);
}

/**
**************************************************************************
* Name: icMatrixInvert3x3
* 
* Purpose: 
*  Inversion of a 3x3 matrix using the Adjoint Cofactor and the determinant of
*  the 3x3 matrix.
*
*  Note: Matrix index positions:
*     0 1 2
*     3 4 5
*     6 7 8
* 
* Args: 
*  M = matrix to invert.
* 
* Return: 
*  true = matrix is invertible and stored back into M, false = matrix is not
*  invertible.
**************************************************************************
*/
bool icMatrixInvert3x3(icFloatNumber *M)
{
  const icFloatNumber epsilon = 1e-8f;

  icFloatNumber m48 = M[4]*M[8];
  icFloatNumber m75 = M[7]*M[5];
  icFloatNumber m38 = M[3]*M[8];
  icFloatNumber m65 = M[6]*M[5];
  icFloatNumber m37 = M[3]*M[7];
  icFloatNumber m64 = M[6]*M[4];

  icFloatNumber det = M[0]*(m48 - m75) - 
    M[1]*(m38 - m65) + 
    M[2]*(m37 - m64);

  if (det>-epsilon && det<epsilon)
    return false;

  icFloatNumber Co[9] = {0};

  Co[0] = +(m48 - m75);
  Co[1] = -(m38 - m65);
  Co[2] = +(m37 - m64);

  Co[3] = -(M[1]*M[8] - M[7]*M[2]);
  Co[4] = +(M[0]*M[8] - M[6]*M[2]);
  Co[5] = -(M[0]*M[7] - M[6]*M[1]);

  Co[6] = +(M[1]*M[5] - M[4]*M[2]);
  Co[7] = -(M[0]*M[5] - M[3]*M[2]);
  Co[8] = +(M[0]*M[4] - M[3]*M[1]);

  M[0] = Co[0] / det;
  M[1] = Co[3] / det;
  M[2] = Co[6] / det;

  M[3] = Co[1] / det;
  M[4] = Co[4] / det;
  M[5] = Co[7] / det;

  M[6] = Co[2] / det;
  M[7] = Co[5] / det;
  M[8] = Co[8] / det;

  return true;
}

/**
**************************************************************************
* Name: icMatrixMultiply3x3
* 
* Purpose: 
*  Multiply two 3x3 matricies resulting in a 3x3 matrix.
*
*  Note: Matrix index positions:
*     0 1 2
*     3 4 5
*     6 7 8
* 
* Args: 
*  result = matrix to recieve result.
*  l = left matrix to multiply (matrix multiplication is order dependent)
*  r = right matrix to multiply (matrix multiplicaiton is order dependent)
*
**************************************************************************
*/
void icMatrixMultiply3x3(icFloatNumber* result,
                         const icFloatNumber* l,
                         const icFloatNumber* r)
{
  const unsigned int e11 = 0;
  const unsigned int e12 = 1;
  const unsigned int e13 = 2;
  const unsigned int e21 = 3;
  const unsigned int e22 = 4;
  const unsigned int e23 = 5;
  const unsigned int e31 = 6;
  const unsigned int e32 = 7;
  const unsigned int e33 = 8;
  result[e11] = l[e11] * r[e11] + l[e12] * r[e21] + l[e13] * r[e31];
  result[e12] = l[e11] * r[e12] + l[e12] * r[e22] + l[e13] * r[e32];
  result[e13] = l[e11] * r[e13] + l[e12] * r[e23] + l[e13] * r[e33];
  result[e21] = l[e21] * r[e11] + l[e22] * r[e21] + l[e23] * r[e31];
  result[e22] = l[e21] * r[e12] + l[e22] * r[e22] + l[e23] * r[e32];
  result[e23] = l[e21] * r[e13] + l[e22] * r[e23] + l[e23] * r[e33];
  result[e31] = l[e31] * r[e11] + l[e32] * r[e21] + l[e33] * r[e31];
  result[e32] = l[e31] * r[e12] + l[e32] * r[e22] + l[e33] * r[e32];
  result[e33] = l[e31] * r[e13] + l[e32] * r[e23] + l[e33] * r[e33];
}

/**
**************************************************************************
* Name: icVectorApplyMatrix3x3
* 
* Purpose: 
*  Applies a 3x3 matrix to a 3 element column vector. 
*
*  Note: Matrix index positions:
*     0 1 2
*     3 4 5
*     6 7 8
* 
*  Note: result = m x v
*
* Args: 
*  result = vector to receive result.
*  m = matrix to multiply
*  v = vector to apply matrix to
*
**************************************************************************
*/
void icVectorApplyMatrix3x3(icFloatNumber* result,
                            const icFloatNumber* m,
                            const icFloatNumber* v)
{
  const unsigned int e11 = 0;
  const unsigned int e12 = 1;
  const unsigned int e13 = 2;
  const unsigned int e21 = 3;
  const unsigned int e22 = 4;
  const unsigned int e23 = 5;
  const unsigned int e31 = 6;
  const unsigned int e32 = 7;
  const unsigned int e33 = 8;
  result[0] = m[e11] * v[0] + m[e12] * v[1] + m[e13] * v[2];
  result[1] = m[e21] * v[0] + m[e22] * v[1] + m[e23] * v[2];
  result[2] = m[e31] * v[0] + m[e32] * v[1] + m[e33] * v[2];
}


static inline icFloatNumber icSq(icFloatNumber x)
{
  return x*x;
}


icFloatNumber icDeltaE(const icFloatNumber *lab1, const icFloatNumber *lab2)
{
  return (icFloatNumber)sqrt(icSq(lab1[0]-lab2[0]) + icSq(lab1[1]-lab2[1]) + icSq(lab1[2]-lab2[2]));
}

icFloatNumber icRmsDif(const icFloatNumber *v1, const icFloatNumber *v2, icUInt32Number nSample)
{
  icFloatNumber sum=0;
  icUInt32Number i;
  for (i=0; i<nSample; i++) {
    sum += icSq(v1[i] - v2[i]);
  }
  if (nSample)
    sum /= nSample;

  return (icFloatNumber)sqrt(sum);
}

icS15Fixed16Number icDtoF(icFloatNumber num)
{
  icS15Fixed16Number rv;

  if (num<-32768.0)
    num = -32768.0;
  else if (num>32767.0)
    num = 32767.0;

  rv = (icS15Fixed16Number)icRoundOffset((double)num*65536.0);

  return rv;
}

icFloatNumber icFtoD(icS15Fixed16Number num)
{
  icFloatNumber rv = (icFloatNumber)((double)num / 65536.0);

  return rv;
}

icU16Fixed16Number icDtoUF(icFloatNumber num)
{
  icU16Fixed16Number rv;

  if (num<0)
    num = 0;
  else if (num>65535.0)
    num = 65535.0;

  rv = (icU16Fixed16Number)icRoundOffset((double)num*65536.0);

  return rv;
}

icFloatNumber icUFtoD(icU16Fixed16Number num)
{
  icFloatNumber rv = (icFloatNumber)((double)num / 65536.0);

  return rv;
}

icU1Fixed15Number icDtoUSF(icFloatNumber num)
{
  icU1Fixed15Number rv;

  if (num<0)
    num = 0;
  else if (num>65535.0/32768.0)
    num = 65535.0/32768.0;

  rv = (icU1Fixed15Number)icRoundOffset(num*32768.0);

  return rv;
}

icFloatNumber icUSFtoD(icU1Fixed15Number num)
{
  icFloatNumber rv = (icFloatNumber)((icFloatNumber)num / 32768.0);

  return rv;
}

icU8Fixed8Number icDtoUCF(icFloatNumber num)
{
  icU8Fixed8Number rv;

  if (num<0)
    num = 0;
  else if (num>255.0)
    num = 255.0;

  rv = (icU8Fixed8Number)icRoundOffset(num*256.0);

  return rv;
}

icFloatNumber icUCFtoD(icU8Fixed8Number num)
{
  icFloatNumber rv = (icFloatNumber)((icFloatNumber)num / 256.0);

  return rv;
}

icFloatNumber ICCPROFLIB_API icF16toF(icFloat16Number num)
{
  icUInt16Number numsgn, numexp, nummnt;
  icUInt32Number rv, rvsgn, rvexp, rvmnt;
  icInt32Number tmpexp;
  icFloatNumber * rvfp, rvf;
  int exp;

  if (!(num & 0x7FFF)) {
    rv = ((icUInt32Number) num) << 16;
  } else {
    numsgn = num & 0x8000;
    numexp = num & 0x7C00;
    nummnt = num & 0x03FF;
    if (!numexp) {
      exp = -1;
      do {
        exp++;
        nummnt <<= 1;
      } while (!(nummnt & 0x0400)); // Shift until leading bit overflows into exponent bit
      rvsgn = ((icUInt32Number) numsgn) << 16;
      tmpexp = ((icUInt32Number) (numexp >> 10)) - 15 + 127 - exp;
      rvexp = (icUInt32Number) (tmpexp << 23);
      rvmnt = ((icUInt32Number) (nummnt & 0x03FFu)) << 13;
      rv = (rvsgn | rvexp | rvmnt);
    } else if (numexp == 0x7C00) {
      if (!nummnt) {
        rv = (((icUInt32Number) numsgn) << 16) | ((icUInt32Number) 0x7F800000);
      } else {
        rv = (icUInt32Number) 0xFFC00000;
      }
    } else {
      rvsgn = ((icUInt32Number) numsgn) << 16;
      tmpexp = ((icUInt32Number) (numexp >> 10)) - 15 + 127;
      rvexp = (icUInt32Number) (tmpexp << 23);
      rvmnt = ((icUInt32Number) nummnt) << 13;
      rv = (rvsgn | rvexp | rvmnt);
    }
  }
  rvfp = (icFloatNumber*)&rv;
  rvf = *rvfp;
  return rvf;
}

icFloat16Number ICCPROFLIB_API icFtoF16(icFloat32Number num)
{
  icUInt16Number rv;
  icUInt16Number rvsgn, rvexp, rvmnt;
  icUInt32Number flt, *fltp, fltsgn, fltexp, fltmnt;
  int exp;

  fltp = (icUInt32Number*)&num;
  flt = *fltp;
  if (!(flt & 0x7FFFFFFF)) {
    rv = (icUInt16Number) (flt >> 16);
  } else { 
    fltsgn = flt & 0x80000000;
    fltexp = flt & 0x7F800000;
    fltmnt = flt & 0x007FFFFF;
    if (!fltexp) {
      rv = (icUInt16Number) (fltsgn >> 16);
    } else if (fltexp == 0x7F800000) {
      if (!fltmnt) {
        rv = (icUInt16Number) ((fltsgn >> 16) | 0x7C00); // Signed Inf
      } else {
        rv = (icUInt16Number) 0xFE00; // NaN
      }
    } else { // Normalized number
      rvsgn = (icUInt16Number) (fltsgn >> 16);
      exp = ((int)(fltexp >> 23)) - 127 + 15;
      if (exp >= 0x1F) {  // Overflow
        rv = (icUInt16Number) ((fltsgn >> 16) | 0x7C00); // Signed Inf
      } else if (exp <= 0) {  // Underflow
        if ((14 - exp) > 24) {
          rvmnt = (icUInt16Number) 0;  // Set mantissa to zero
        } else {
          fltmnt |= 0x00800000;  // Include hidden leading bit
          rvmnt = (icUInt16Number) (fltmnt >> (14 - exp));
          if ((fltmnt >> (13 - exp)) & 0x00000001) // Rounding?
            rvmnt += (icUInt16Number) 1;
        }
        rv = (rvsgn | rvmnt);
      } else {
        rvexp = (icUInt16Number) (exp << 10);
        rvmnt = (icUInt16Number) (fltmnt >> 13);
        if (fltmnt & 0x00001000) // Rounding?
          rv = (rvsgn | rvexp | rvmnt) + (icUInt16Number) 1; 
        else
          rv = (rvsgn | rvexp | rvmnt);
      }
    }
  }
  return rv;
}

icUInt8Number icFtoU8(icFloatNumber num)
{
  icUInt8Number rv;

  if (num<0)
    num = 0;
  else if (num>1.0)
    num = 1.0;

  rv = (icUInt8Number)icRoundOffset(num*255.0);

  return rv;
}

icFloatNumber icU8toF(icUInt8Number num)
{
  icFloatNumber rv = (icFloatNumber)((icFloatNumber)num / 255.0);

  return rv;
}

icUInt16Number icFtoU16(icFloatNumber num)
{
  icUInt16Number rv;

  if (num<0)
    num = 0;
  else if (num>1.0)
    num = 1.0;

  rv = (icUInt16Number)icRoundOffset(num*65535.0);

  return rv;
}

icFloatNumber icU16toF(icUInt16Number num)
{
  icFloatNumber rv = (icFloatNumber)((icFloatNumber)num / 65535.0);

  return rv;
}

icUInt8Number icABtoU8(icFloatNumber num)
{
  icFloatNumber v = num + 128.0f;
  if (v<0)
    v=0;
  else if (v>255)
    v=255;

  return (icUInt8Number)(v + 0.5);
}

icFloatNumber icU8toAB(icUInt8Number num)
{
  return (icFloatNumber)num - 128.0f;
}

ICCPROFLIB_API icFloatNumber icD50XYZ[3] = { 0.9642f, 1.0000f, 0.8249f };
ICCPROFLIB_API icFloatNumber icD50XYZxx[3] = { 96.42f, 100.00f, 82.49f };

void icNormXyz(icFloatNumber *XYZ, icFloatNumber *WhiteXYZ)
{
  if (!WhiteXYZ)
    WhiteXYZ = icD50XYZ;

  XYZ[0] = XYZ[0] / WhiteXYZ[0];
  XYZ[1] = XYZ[1] / WhiteXYZ[1];
  XYZ[2] = XYZ[2] / WhiteXYZ[2];
}

void icDeNormXyz(icFloatNumber *XYZ, icFloatNumber *WhiteXYZ)
{
  if (!WhiteXYZ)
    WhiteXYZ = icD50XYZ;

  XYZ[0] = XYZ[0] * WhiteXYZ[0];
  XYZ[1] = XYZ[1] * WhiteXYZ[1];
  XYZ[2] = XYZ[2] * WhiteXYZ[2];
}

icFloatNumber icCubeth(icFloatNumber v)
{
  if (v> 0.008856) {
    return (icFloatNumber)ICC_CBRTF(v);
  }
  else {
    return (icFloatNumber)(7.787037037037037037037037037037*v + 16.0/116.0);
  }
}

icFloatNumber icICubeth(icFloatNumber v)
{
  if (v > 0.20689303448275862068965517241379)
    return v*v*v;
  else 
#ifndef SAMPLEICC_NOCLIPLABTOXYZ
  if (v>16.0/116.0)
#endif
    return (icFloatNumber)((v - 16.0 / 116.0) / 7.787037037037037037037037037037);
#ifndef SAMPLEICC_NOCLIPLABTOXYZ
  else
    return 0.0;
#endif
}

void icLabtoXYZ(icFloatNumber *XYZ, const icFloatNumber *Lab /*=NULL*/, const icFloatNumber *WhiteXYZ /*=NULL*/)
{
  if (!Lab)
    Lab = XYZ;

  if (!WhiteXYZ)
    WhiteXYZ = icD50XYZ;

  icFloatNumber fy = (icFloatNumber)((Lab[0] + 16.0) / 116.0);

  XYZ[0] = icICubeth((icFloatNumber)(Lab[1]/500.0 + fy)) * WhiteXYZ[0];
  XYZ[1] = icICubeth(fy) * WhiteXYZ[1];
  XYZ[2] = icICubeth((icFloatNumber)(fy - Lab[2]/200.0)) * WhiteXYZ[2];

}

void icXYZtoLab(icFloatNumber *Lab, const icFloatNumber *XYZ /*=NULL*/, const icFloatNumber *WhiteXYZ /*=NULL*/)
{
  icFloatNumber Xn, Yn, Zn;
  
  if (!XYZ)
    XYZ = Lab;

  if (!WhiteXYZ)
    WhiteXYZ = icD50XYZ;

  Xn = icCubeth(XYZ[0] / WhiteXYZ[0]);
  Yn = icCubeth(XYZ[1] / WhiteXYZ[1]);
  Zn = icCubeth(XYZ[2] / WhiteXYZ[2]);

  Lab[0] = (icFloatNumber)(116.0 * Yn - 16.0);
  Lab[1] = (icFloatNumber)(500.0 * (Xn - Yn));
  Lab[2] = (icFloatNumber)(200.0 * (Yn - Zn));

}

void icLch2Lab(icFloatNumber *Lab, icFloatNumber *Lch /*=NULL*/)
{
  if (!Lch) {
    Lch = Lab;
  }
  else
    Lab[0] = Lch[0];

  icFloatNumber a = (icFloatNumber)(Lch[1] * cos(Lch[2] * PI / 180.0));
  icFloatNumber b = (icFloatNumber)(Lch[1] * sin(Lch[2] * PI / 180.0));

  Lab[1] = a;
  Lab[2] = b;
}

void icLab2Lch(icFloatNumber *Lch, icFloatNumber *Lab /*=NULL*/)
{
  if (!Lab) {
    Lab = Lch;
  }
  else
    Lch[0] = Lab[0];

  icFloatNumber c = (icFloatNumber)sqrt((double)Lab[1]*Lab[1] + (double)Lab[2]*Lab[2]);
  icFloatNumber h = (icFloatNumber)(atan2(Lab[2], Lab[1]) * 180.0 / PI);
  while (h<0.0)
    h+=360.0;

  Lch[1] = c;
  Lch[2] = h;
}

icFloatNumber icMin(icFloatNumber v1, icFloatNumber v2)
{
  return( v1 < v2 ? v1 : v2 );
}

icFloatNumber icMax(icFloatNumber v1, icFloatNumber v2)
{
  return( v1 > v2 ? v1 : v2 );
}

icUInt32Number icIntMin(icUInt32Number v1, icUInt32Number v2)
{
  return( v1 < v2 ? v1 : v2 );
}

icUInt32Number icIntMax(icUInt32Number v1, icUInt32Number v2)
{
  return( v1 > v2 ? v1 : v2 );
}


void icLabFromPcs(icFloatNumber *Lab)
{
  Lab[0] *= 100.0f;
  Lab[1] = Lab[1]*255.0f - 128.0f;
  Lab[2] = Lab[2]*255.0f - 128.0f;
}


void icLabToPcs(icFloatNumber *Lab)
{
  Lab[0] /= 100.0f;
  Lab[1] = (Lab[1] + 128.0f) / 255.0f;
  Lab[2] = (Lab[2] + 128.0f) / 255.0f;
}

void icXyzFromPcs(icFloatNumber *XYZ)
{
  XYZ[0] = (icFloatNumber)(XYZ[0] * 65535.0 / 32768.0);
  XYZ[1] = (icFloatNumber)(XYZ[1] * 65535.0 / 32768.0);
  XYZ[2] = (icFloatNumber)(XYZ[2] * 65535.0 / 32768.0);
}

void icXyzToPcs(icFloatNumber *XYZ)
{
  XYZ[0] = (icFloatNumber)(XYZ[0] * 32768.0 / 65535.0);
  XYZ[1] = (icFloatNumber)(XYZ[1] * 32768.0 / 65535.0);
  XYZ[2] = (icFloatNumber)(XYZ[2] * 32768.0 / 65535.0);
}


#define DUMPBYTESPERLINE 16

// This assumes that nNum is only 8 hex digits, 4 bytes, 32 bit value
void icMemDump(std::string &sDump, void *pBuf, size_t nNum)
{
  icUInt8Number *pData = (icUInt8Number *)pBuf;
  const size_t bufSize = 80;
  const size_t numSize = 10;
  icChar buf[bufSize] = {0};
  icChar num[numSize] = {0};

  icInt32Number j;
  icUInt8Number c;

  size_t lines = (nNum + DUMPBYTESPERLINE - 1)/DUMPBYTESPERLINE;
  sDump.reserve(sDump.size() + lines*79);

  for (size_t i=0; i<nNum; i++, pData++) {
    j=i%DUMPBYTESPERLINE;
    if (!j) {
      if (i) {
        sDump += (const icChar*)buf;
      }
      memset(buf, ' ', 76);
      buf[76] = ' ';
      buf[77] = '\n';
      buf[78] = '\0';
      snprintf(num, numSize, "%08X:", uint32_t(i));    // NOTE - this formatting will have to change if we want to move beyond 4 Gig.
      strncpy(buf, num, 9);
    }

    snprintf(num, numSize, "%02X", *pData);
    strncpy(buf+10+j*3, num, 2);

    c=*pData;
    if (!isprint(c) || c > 126)
      c='.';
    buf[10+16*3 + 1 + j] = c;
  }
  sDump += buf;
}

void icMatrixDump(std::string &sDump, icS15Fixed16Number *pMatrix)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf\n", icFtoD(pMatrix[0]), icFtoD(pMatrix[1]), icFtoD(pMatrix[2]));
  sDump += buf;
  snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf\n", icFtoD(pMatrix[3]), icFtoD(pMatrix[4]), icFtoD(pMatrix[5]));
  sDump += buf;
  snprintf(buf, bufSize, "%8.4lf %8.4lf %8.4lf\n", icFtoD(pMatrix[6]), icFtoD(pMatrix[7]), icFtoD(pMatrix[8]));
  sDump += buf;
}

const icChar* icGet16bitSig(icChar* pBuf, size_t bufSize, icUInt16Number nSig, bool bGetHexVal)
{
    icUInt16Number sig = nSig;
    icUInt8Number c;
// TODO - really need to check bufSize for minimum limits

    if (!nSig) {
        strcpy(pBuf, "NULL");
        return pBuf;
    }

    pBuf[0] = '\'';
    c = (icUInt8Number)(sig >> 8);
    if (!isprint(c) || c > 126)
        c = '?';
    pBuf[1] = c;
    c = (icUInt8Number)(sig & 0x00FF);
    if (!isprint(c) || c > 126)
        c = '?';
    pBuf[2] = c;

    if (bGetHexVal)
        snprintf(pBuf + 3, bufSize-3, "' = %04X", nSig);
    else
        snprintf(pBuf + 3, bufSize-3, "'");

    return pBuf;
}

const icChar *icGetSig(icChar *pBuf, size_t bufSize, icUInt32Number nSig, bool bGetHexVal)
{
  int i;
  icUInt32Number sig=nSig;
  icUInt8Number c;
// TODO - really need to check bufSize for minimum limits

  if (!nSig) {
    strcpy(pBuf, "NULL");
    return pBuf;
  }

  pBuf[0] = '\'';
  
  // 126 is ~, and 127 is DEL, over 127 is undefined and depends on local code page
  // isprint() lies about values > 127 on MacOS and Linux, haven't tested Windows
  for (i=1; i<5; i++) {
    c=(icUInt8Number)(sig>>24);
    if (!isprint(c) || c > 126) {
      c='?';
      bGetHexVal = true;
    }
    pBuf[i]=c;
    sig <<=8;
  }

  if (bGetHexVal)
    snprintf(pBuf+5, bufSize-5, "' = %08X", nSig);  // 17 characcter plus NULL
  else
    snprintf(pBuf+5, bufSize-5, "'");   // 6 characters plus NULL

  return pBuf;
}

const icChar *icGetSigStr(icChar *pBuf, size_t bufSize, icUInt32Number nSig)
{
  int i, j=-1;
  icUInt32Number sig=nSig;
  icUInt8Number c;
  bool bGetHexVal = false;
// TODO - really need to check bufSize for minimum limits

  for (i=0; i<4; i++) {
    c=(icUInt8Number)(sig>>24);
    if (!c) {
      j=i;
    }
    else if (j!=-1) {
      bGetHexVal = true;
    }
    else if (!isprint(c) || c==':' || c > 126) {
      c='?';
      bGetHexVal = true;
    }
    pBuf[i]=c;
    sig <<=8;
  }

  if (bGetHexVal)
    snprintf(pBuf, bufSize, "%08Xh", nSig);
  else
    pBuf[4] = '\0';

  return pBuf;
}


const icChar *icGetColorSig(icChar *pBuf, size_t bufSize, icUInt32Number nSig, bool bGetHexVal)
{
  icUInt32Number sig=nSig;
// TODO - really need to check bufSize for minimum limits

  switch (icGetColorSpaceType(nSig)) {
    case icSigNChannelData:
    case icSigReflectanceSpectralData:
    case icSigTransmisionSpectralData:
    case icSigRadiantSpectralData:
    case icSigBiSpectralReflectanceData:
    case icSigSparseMatrixReflectanceData:

      pBuf[0]='\"';
      pBuf[1] = (icUInt8Number)(sig>>24);
      sig<<=8;
      pBuf[2] = (icUInt8Number)(sig>>24);
      snprintf(pBuf+3, bufSize-3, "%04X\"", icNumColorSpaceChannels(nSig));
      return pBuf;
    
    default:
    {

      int i;
      icUInt8Number c;
      bool bNeedHexVal = false;

      pBuf[0] = '\'';
      for (i=1; i<5; i++) {
        c=(icUInt8Number)(sig>>24);
        if (!isprint(c) || c > 126) {
          c = '?';
          bNeedHexVal = true;
        }
        pBuf[i]=c;
        sig <<=8;
      }

      if (bGetHexVal)
        snprintf(pBuf+5, bufSize-5, "' = %08X", nSig);
      else if (bNeedHexVal) {
        snprintf(pBuf, bufSize, "%08Xh", nSig);
      }
      else
        snprintf(pBuf+5, bufSize-5, "'");
    }
  }

  return pBuf;
}


const icChar *icGetColorSigStr(icChar *pBuf, size_t bufSize, icUInt32Number nSig)
{
  icUInt32Number sig=nSig;
// TODO - really need to check bufSize for minimum limits

  switch (icGetColorSpaceType(nSig)) {
    case icSigNChannelData:
    case icSigReflectanceSpectralData:
    case icSigTransmisionSpectralData:
    case icSigRadiantSpectralData:
    case icSigBiSpectralReflectanceData:
    case icSigSparseMatrixReflectanceData:

      pBuf[0] = (icUInt8Number)(sig>>24);
      sig<<=8;
      pBuf[1] = (icUInt8Number)(sig>>24);
      snprintf(pBuf+2, bufSize-2, "%04X", icNumColorSpaceChannels(nSig));
      return pBuf;

    default:
      {

        int i, j=-1;
        icUInt8Number c;
        bool bGetHexVal = false;

        for (i=0; i<4; i++) {
          c=(icUInt8Number)(sig>>24);
          if (!c) {
            j=i;
          }
          else if (j!=-1) {
            bGetHexVal = true;
          }
          else if (!isprint(c) || c==':' || c > 126) {
            c='?';
            bGetHexVal = true;
          }
          pBuf[i]=c;
          sig <<=8;
        }

        if (bGetHexVal)
          snprintf(pBuf, bufSize, "%08Xh", nSig);
        else
          pBuf[4] = '\0';
      }
  }

  return pBuf;
}


std::string icGetSigPath(icUInt32Number nSig)
{
  char buf[20];
  std::string rv = ":";

  rv += icGetSigStr(buf, 20, nSig);

  return rv;
}

icSignature icGetFirstSigPathSig(std::string sigPath)
{
  icSignature rv = 0;
  const char *ptr = sigPath.c_str();
  while(*ptr==':') {
    std::string sigStr;
    ptr++;
    while(*ptr && *ptr!=':') {
      sigStr += *ptr;
      ptr++;
    }
    rv = icGetSigVal(sigStr.c_str());
    //Skip embedded tag path entry
    if (rv != icSigEmbeddedV5ProfileTag)
      break;
  }
  return rv;
}

icSignature icGetLastSigPathSig(std::string sigPath)
{
  size_t n = sigPath.length();
  if (!n)
    return icGetSigVal(sigPath.c_str());

  const char *sig = sigPath.c_str();

  for (n = n - 1; n > 0; n--) {
    if (sig[n] == ':') {
      break;
    }
  }
  if (/* n >= 0 && */ sig[n] == ':')
    n++;
  return icGetSigVal(sig+n);
}

icSignature icGetSecondSigPathSig(std::string sigPath)
{
  icSignature rv = 0;
  const char *ptr = sigPath.c_str();
  if(*ptr==':') {
    std::string sigStr;
    ptr++;
    ptr = strchr(ptr+1, ':');
    if (ptr) {
      ptr++;
      while(*ptr && *ptr!=':') {
        sigStr += *ptr;
        ptr++;
      }
      rv = icGetSigVal(sigStr.c_str());
    }
  }
  return rv;
}

icUInt32Number icGetSigVal(const icChar *pBuf)
{
  icUInt32Number v;

  switch(strlen(pBuf)) {
    case 0:
      return 0;

    case 1:
      return (icUInt32Number)((((unsigned long)pBuf[0])<<24) +
                              0x202020);

    case 2:
      return (icUInt32Number)((((unsigned long)pBuf[0])<<24) +
                              (((unsigned long)pBuf[1])<<16) +
                              0x2020);

    case 3:
      return (icUInt32Number)((((unsigned long)pBuf[0])<<24) +
                              (((unsigned long)pBuf[1])<<16) +
                              (((unsigned long)pBuf[2])<<8) +
                              0x20);

    case 4:
    default:
      return (icUInt32Number)((((unsigned long)pBuf[0])<<24) +
                              (((unsigned long)pBuf[1])<<16) +
                              (((unsigned long)pBuf[2])<<8) +
                              (((unsigned long)pBuf[3])));

    case 6:  //Channel based color signatures
      sscanf(pBuf+2, "%x", &v);

      return (icUInt32Number)((((unsigned long)pBuf[0])<<24) +
                              (((unsigned long)pBuf[1])<<16) +
                              v);

    case 8:
    case 9:
      sscanf(pBuf, "%x", &v);
      return v;
  }
}


icUInt32Number icGetSpaceSamples(icColorSpaceSignature sig)
{
  switch(sig) {
  case icSigGrayData:
  case icSigGamutData:
    return 1;

  case icSig2colorData:
    return 2;

  case icSigXYZData:
  case icSigLabData:
  case icSigLuvData:
  case icSigYCbCrData:
  case icSigYxyData:
  case icSigRgbData:
  case icSigHsvData:
  case icSigHlsData:
  case icSigCmyData:
  case icSig3colorData:
  case icSigDevLabData:
  case icSigDevXYZData:
    return 3;

  case icSigCmykData:
  case icSig4colorData:
    return 4;

  default:
    switch (icGetColorSpaceType(sig)) {
      case icSigNChannelData:
      case icSigReflectanceSpectralData:
      case icSigTransmisionSpectralData:
      case icSigRadiantSpectralData:
      case icSigBiSpectralReflectanceData:
      case icSigSparseMatrixReflectanceData:
      case icSigSrcMCSChannelData:
        return icNumColorSpaceChannels(sig);

      default:
      {
        icChar szSig[5] = {0};
        szSig[0] = (icChar)(sig>>24);
        szSig[1] = (icChar)(sig>>16);
        szSig[2] = (icChar)(sig>>8);
        szSig[3] = (icChar)(sig);
        szSig[4] = '\0';

        if (!strcmp(szSig+1, "CLR")) {
          int d0 = icHexDigit(szSig[0]);
          if (d0>=0)
            return d0;
        }
        else if (!strncmp(szSig, "MCH", 3)) {  //Litte CMS multi-channel notation (not part of ICC specification)
          int d0 = icHexDigit(szSig[3]);
          if (d0>=0)
            return d0;
        }
      }
      return 0;
    }
  }
}

icUInt32Number icGetSpectralSpaceSamples(const icHeader *pHdr)
{
  if (pHdr->spectralPCS)
    return icGetSpaceSamples((icColorSpaceSignature)pHdr->spectralPCS);
  else
    return icGetSpaceSamples(pHdr->pcs);
}

icUInt8Number icGetStorageTypeBytes(icUInt16Number nStorageType)
{
  switch (nStorageType) {
  case icValueTypeUInt8:
    return 1;
  case icValueTypeUInt16:
  case icValueTypeFloat16:
    return 2;
  case icValueTypeFloat32:
    return 4;
  default:
    return 0;
  }
}


icUInt32Number icGetMaterialColorSpaceSamples(icMaterialColorSignature sig)
{
  if (icGetColorSpaceType((icColorSpaceSignature)sig)!=icSigSrcMCSChannelData)
    return 0;

  return icNumColorSpaceChannels(sig);
}

bool icSameSpectralRange(const icSpectralRange &rng1, const icSpectralRange &rng2)
{
  return (rng1.start == rng2.start &&
          rng1.end == rng2.end &&
          rng1.steps == rng2.steps);
}

CIccInfo::CIccInfo() : m_szStr{}, m_szSigStr{}
{
  m_str = new std::string;
}

CIccInfo::~CIccInfo()
{
  delete m_str;
}

const icChar *CIccInfo::GetUnknownName(icUInt32Number val)
{
  icChar buf[24];
  // icGetSig() will print 'NULL' if val is 0x00000000 which is easier to post-process
  // if (!val)
  //  return "Unknown";

  snprintf(m_szStr, m_bufSize, "Unknown %s", icGetSig(buf, 24, val));

  return m_szStr;
}

const icChar *CIccInfo::GetVersionName(icUInt32Number val)
{
  icFloatNumber ver = (icFloatNumber)(((val>>28)&0xf)*10.0 + ((val>>24)&0xf) +
                                      ((val>>20)&0xf)/10.0 + ((val>>16)&0xf)/100.0);

  snprintf(m_szStr, m_bufSize, "%.2lf", ver);

  return m_szStr;
}

const icChar *CIccInfo::GetSubClassVersionName(icUInt32Number val)
{
  icFloatNumber ver = (icFloatNumber)(((val >> 12) & 0xf)*10.0 + ((val >> 8) & 0xf) +
    ((val >> 4) & 0xf) / 10.0 + (val & 0xf) / 100.0);

  snprintf(m_szStr, m_bufSize, "%.2lf", ver);

  return m_szStr;
}

const icChar *CIccInfo::GetDeviceAttrName(icUInt64Number val)
{
  if (val & icTransparency)
    strcpy(m_szStr, "Transparency");
  else
    strcpy(m_szStr, "Reflective");

  int l=(int)strlen(m_szStr);

  if (val & icMatte)
    strcpy(m_szStr+l, " | Matte");
  else
    strcpy(m_szStr+l, " | Glossy");

  return m_szStr;
}

const icChar *CIccInfo::GetProfileFlagsName(icUInt32Number val, bool bCheckMCS/*=false*/)
{
  if (val & icEmbeddedProfileTrue) 
    strcpy(m_szStr, "EmbeddedProfileTrue");
  else
    strcpy(m_szStr, "EmbeddedProfileFalse");

  int l=(int)strlen(m_szStr);

  if (val & icUseWithEmbeddedDataOnly)
    strcpy(m_szStr+l, " | UseWithEmbeddedDataOnly");
  else
    strcpy(m_szStr+l, " | UseAnywhere");

  if (bCheckMCS) {
    l=(int)strlen(m_szStr);

    if (val & icMCSNeedsSubsetTrue)
      strcpy(m_szStr+l, " | MCSNeedsSubset");
    else
      strcpy(m_szStr+l, " | MCSConnectAny");
  }

  if (val & icExtendedRangePCS)
    strcpy(m_szStr + l, " | ExtendedRangePCS");


  return m_szStr;
}

const icChar *CIccInfo::GetTagSigName(icTagSignature sig)
{
  const icChar *rv = CIccTagCreator::GetTagSigName(sig);
  if (rv) {
    return rv;
  }
  return GetUnknownName(sig);
}

const icChar *CIccInfo::GetStructSigName(icStructSignature sig)
{
  if (CIccStructCreator::GetStructSigName(*m_str, sig)) {
    return m_str->c_str();
  }
  return GetUnknownName(sig);
}

const icChar *CIccInfo::GetArraySigName(icArraySignature sig)
{
  if (CIccArrayCreator::GetArraySigName(*m_str, sig)) {
    return m_str->c_str();
  }
  return GetUnknownName(sig);
}

const icChar *CIccInfo::GetTechnologySigName(icTechnologySignature sig)
{
  switch(sig) {
  case icSigDigitalCamera:
    return "DigitalCamera";

  case icSigFilmScanner:
    return "FilmScanner";

  case icSigReflectiveScanner:
    return "ReflectiveScanner";

  case icSigInkJetPrinter:
    return "InkJetPrinter";

  case icSigThermalWaxPrinter:
    return "ThermalWaxPrinter";

  case icSigElectrophotographicPrinter:
    return "ElectrophotographicPrinter";

  case icSigElectrostaticPrinter:
    return "ElectrostaticPrinter";

  case icSigDyeSublimationPrinter:
    return "DyeSublimationPrinter";

  case icSigPhotographicPaperPrinter:
    return "PhotographicPaperPrinter";

  case icSigFilmWriter:
    return "FilmWriter";

  case icSigVideoMonitor:
    return "VideoMonitor";

  case icSigVideoCamera:
    return "VideoCamera";

  case icSigProjectionTelevision:
    return "ProjectionTelevision";

  case icSigCRTDisplay:
    return "CRTDisplay";

  case icSigPMDisplay:
    return "PMDisplay";

  case icSigAMDisplay:
    return "AMDisplay";

  case icSigPhotoCD:
    return "PhotoCD";

  case icSigPhotoImageSetter:
    return "PhotoImageSetter";

  case icSigGravure:
    return "Gravure";

  case icSigOffsetLithography:
    return "OffsetLithography";

  case icSigSilkscreen:
    return "Silkscreen";

  case icSigFlexography:
    return "Flexography";

  default:
    return GetUnknownName(sig);
  }
}

const icChar *CIccInfo::GetTagTypeSigName(icTagTypeSignature sig)
{
  const icChar *rv = CIccTagCreator::GetTagTypeSigName(sig);
  if (rv) {
    return rv;
  }

  return GetUnknownName(sig);
}

const icChar *CIccInfo::GetElementTypeSigName(icElemTypeSignature sig)
{

  if (CIccMpeCreator::GetElementSigName(*m_str, sig)) {
    return m_str->c_str();
  }

  return GetUnknownName(sig);
}

std::string CIccInfo::GetSigPathName(std::string sigPath)
{
  std::string rv;
  const char *ptr = sigPath.c_str();
  int n =0;
  while(*ptr==':') {
    std::string sigStr;
    ptr++;
    while(*ptr && *ptr!=':') {
      sigStr += *ptr;
      ptr++;
    }
    icSignature sig = icGetSigVal(sigStr.c_str());
    if (n!=0) {
      rv += ">";
    }
    rv += GetPathEntrySigName(sig);
    n++;
  }
  if (n > 0)
    rv += ":";

  return rv;
}


const icChar *CIccInfo::GetColorSpaceSigName(icColorSpaceSignature sig)
{
  switch (sig) {
  case icSigNoColorData:
    return "NoData";

  case icSigXYZData:
  case icSigDevXYZData:
    return "XYZData";

  case icSigLabData:
  case icSigDevLabData:
    return "LabData";

  case icSigLuvData:
    return "LuvData";

  case icSigYCbCrData:
    return "YCbCrData";

  case icSigYxyData:
    return "YxyData";

  case icSigRgbData:
    return "RgbData";

  case icSigGrayData:
    return "GrayData";

  case icSigHsvData:
    return "HsvData";

  case icSigHlsData:
    return "HlsData";

  case icSigCmykData:
    return "CmykData";

  case icSigCmyData:
    return "CmyData";

  case icSigMCH1Data:
    return "MCH1Data/1ColorData";

  case icSigMCH2Data:
    return "MCH2Data/2ColorData";

  case icSigMCH3Data:
    return "MCH3Data/3ColorData";

  case icSigMCH4Data:
    return "MCH4Data/4ColorData";

  case icSigMCH5Data:
    return "MCH5Data/5ColorData";

  case icSigMCH6Data:
    return "MCH6Data/6ColorData";

  case icSigMCH7Data:
    return "MCH7Data/7ColorData";

  case icSigMCH8Data:
    return "MCH8Data/8ColorData";

  case icSigMCH9Data:
    return "MCH9Data/9ColorData";

  case icSigMCHAData:
    return "MCHAData/10ColorData";

  case icSigMCHBData:
    return "MCHBData/11ColorData";

  case icSigMCHCData:
    return "MCHCData/12ColorData";

  case icSigMCHDData:
    return "MCHDData/13ColorData";

  case icSigMCHEData:
    return "MCHEData/14ColorData";

  case icSigMCHFData:
    return "MCHFData/15ColorData";

  case icSigGamutData:
    return "GamutData";
    
  case icSigNamedData:
    return "NamedData";

  default:
    switch(icGetColorSpaceType(sig)) {
    case icSigNChannelData:
      snprintf(m_szStr, m_bufSize, "0x%04XChannelData", icNumColorSpaceChannels(sig));
      return m_szStr;

    case icSigReflectanceSpectralData:
      snprintf(m_szStr, m_bufSize, "0x%04XChannelReflectanceData", icNumColorSpaceChannels(sig));
      return m_szStr;

    case icSigTransmisionSpectralData:
      snprintf(m_szStr, m_bufSize, "0x%04XChannelTransmissionData", icNumColorSpaceChannels(sig));
      return m_szStr;

    case icSigRadiantSpectralData:
      snprintf(m_szStr, m_bufSize, "0x%04XChannelRadiantData", icNumColorSpaceChannels(sig));
      return m_szStr;

    case icSigBiSpectralReflectanceData:
      snprintf(m_szStr, m_bufSize, "0x%04XChannelBiDirReflectanceData", icNumColorSpaceChannels(sig));
      return m_szStr;

    case icSigSparseMatrixReflectanceData:
      snprintf(m_szStr, m_bufSize, "0x%04XChannelSparseMatrixReflectanceData", icNumColorSpaceChannels(sig));
      return m_szStr;

    default:
      icUInt32Number nChan = icGetSpaceSamples(sig);
      if (nChan>0) {
        snprintf(m_szStr, m_bufSize, "0x%XColorData", nChan);
        return m_szStr;
      }
      return GetUnknownName(sig);
    }
  }
}

const icChar *CIccInfo::GetSpectralColorSigName(icColorSpaceSignature sig)
{
  switch(icGetColorSpaceType(sig)) {
  case icSigNoSpectralData:
    return "NoSpectralData";

   case icSigNChannelData:
     snprintf(m_szStr, m_bufSize, "0x%04XChannelData", icNumColorSpaceChannels(sig));
     return m_szStr;

   case icSigReflectanceSpectralData:
     snprintf(m_szStr, m_bufSize, "0x%04XChannelReflectanceData", icNumColorSpaceChannels(sig));
     return m_szStr;

   case icSigTransmisionSpectralData:
     snprintf(m_szStr, m_bufSize, "0x%04XChannelTransmissionData", icNumColorSpaceChannels(sig));
     return m_szStr;

   case icSigRadiantSpectralData:
     snprintf(m_szStr, m_bufSize, "0x%04XChannelRadiantData", icNumColorSpaceChannels(sig));
     return m_szStr;

   case icSigBiSpectralReflectanceData:
     snprintf(m_szStr, m_bufSize, "0x%04XChannelBiSpectralReflectanceData", icNumColorSpaceChannels(sig));
     return m_szStr;

   case icSigSparseMatrixReflectanceData:
     snprintf(m_szStr, m_bufSize, "0x%04XChannelSparseMatrixReflectanceData", icNumColorSpaceChannels(sig));
     return m_szStr;

  default:
    return GetUnknownName(sig);
  }
}

const icChar *CIccInfo::GetProfileClassSigName(icProfileClassSignature sig)
{
  switch (sig) {
  case icSigInputClass:
    return "InputClass";

  case icSigDisplayClass:
    return "DisplayClass";

  case icSigOutputClass:
    return "OutputClass";

  case icSigLinkClass:
    return "LinkClass";

  case icSigAbstractClass:
    return "AbstractClass";

  case icSigColorSpaceClass:
    return "ColorSpaceClass";

  case icSigNamedColorClass:
    return "NamedColorClass";

  case icSigColorEncodingClass:
    return "ColorEncodingClass";

  case icSigMaterialIdentificationClass:
    return "MaterialIdentificationClass";

  case icSigMaterialVisualizationClass:
    return "MaterialVisualizationClass";

  case icSigMaterialLinkClass:
    return "MaterialLinkClass";

  default:
    return GetUnknownName(sig);
  }
}

const icChar *CIccInfo::GetPlatformSigName(icPlatformSignature sig)
{
  switch (sig) {
  case icSigMacintosh:
    return "Macintosh";

  case icSigMicrosoft:
    return "Microsoft";

  case icSigSolaris:
    return "Solaris";

  case icSigSGI:
    return "SGI";

  case icSigTaligent:
    return "Taligent";

  case icSigUnknownPlatform:
    return "Unknown";

  default:
    return GetUnknownName(sig);
  }
}


//The following signatures come from the signature registry
//Return the Description (minus CMM).
const icChar *CIccInfo::GetCmmSigName(icCmmSignature sig)
{
  switch (sig) {
  case icSigAdobe:
    return "Adobe";

  case icSigAgfa:
    return "Agfa";

  case icSigApple:
    return "Apple";

  case icSigColorGear:
    return "ColorGear";

  case icSigColorGearLite:
    return "ColorGear Lite";

  case icSigColorGearC:
    return "ColorGear C";

  case icSigEFI:
    return "EFI";

  case icSigExactScan:
    return "ExactScan";

  case icSigFujiFilm:
    return "Fuji Film";

  case icSigHarlequinRIP:
    return "Harlequin RIP";

  case icSigArgyllCMS:
    return "Argyll CMS";

  case icSigLogoSync:
    return "LogoSync";

  case icSigHeidelberg:
    return "Heidelberg";
  
  case icSigLinoColor:
    return "LinoColor";
  
  case icSigMonaco:
    return "Monaco";

  case icSigLittleCMS:
    return "Little CMS";

  case icSigKodak:
    return "Kodak";

  case icSigKonicaMinolta:
    return "Konica Minolta";

  case icSigMutoh:
    return "Mutoh";

  case icSigRefIccMAX:
    return "RefIccMAX";

  case icSigDemoIccMAX:
    return "DemoIccMAX";

  case icSigIccDEV:
	  return "iccDEV";

  case icSigRolfGierling:
    return "Rolf Gierling Multitools";

  case icSigSampleICC:
    return "SampleIcc";

  case icSigToshiba:
    return "Toshiba";

  case icSigTheImagingFactory:
    return "the imaging factory";

  case icSigVivo:
    return "Vivo";

  case icSigWareToGo:
    return "Ware To Go";

  case icSigMicrosoftCMM:
    return "Windows Color System";

  case icSigZoran:
    return "Zoran";

  default:
    return GetUnknownName(sig);
  }
}


const icChar *CIccInfo::GetReferenceMediumGamutSigNameName(icReferenceMediumGamutSignature sig)
{
  switch (sig) {
  case icSigPerceptualReferenceMediumGamut:
    return "perceptualReferenceMediumGamut";

  default:
    return GetUnknownName(sig);
  }
}


const icChar *CIccInfo::GetColorimetricIntentImageStateName(icColorimetricIntentImageStateSignature sig)
{
  switch (sig) {
  case icSigSceneColorimetryEstimates:
    return "Scene Colorimetry Estimates";

  case icSigSceneAppearanceEstimates:
    return "Scene Appearance Estimates";

  case icSigFocalPlaneColorimetryEstimates:
    return "Focal Plane Colorimetry Estimates";

  case icSigReflectionHardcopyOriginalColorimetry:
    return "Reflection Hardcopy Original Colorimetry";

  case icSigReflectionPrintOutputColorimetry:
    return "Reflection Print Output Colorimetry";

  default:
    return GetUnknownName(sig);
  }
}


const icChar *CIccInfo::GetSigName(icUInt32Number sig)
{
  const icChar *rv;

  rv = GetTagSigName((icTagSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetTechnologySigName((icTechnologySignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetTagTypeSigName((icTagTypeSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetColorSpaceSigName((icColorSpaceSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetProfileClassSigName((icProfileClassSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetPlatformSigName((icPlatformSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetReferenceMediumGamutSigNameName((icReferenceMediumGamutSignature)sig);
  if (rv != m_szStr)
    return rv;

  return GetColorimetricIntentImageStateName((icColorimetricIntentImageStateSignature)sig);
}

const icChar *CIccInfo::GetPathEntrySigName(icUInt32Number sig)
{
  const icChar *rv;

  rv = GetTagSigName((icTagSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetTagTypeSigName((icTagTypeSignature)sig);
  if (rv != m_szStr)
    return rv;

  rv = GetElementTypeSigName((icElemTypeSignature)sig);
  if (rv != m_szStr)
    return rv;

  return GetUnknownName(sig);
}


const icChar *CIccInfo::GetMeasurementFlareName(icMeasurementFlare val)
{
  switch (val) { // Directly switch on the enum without casting to int
  case icFlare0:
    return "Flare 0"; // Properly handle the 0% flare

  case icFlare100:
    return "Flare 100"; // Properly handle the 100% flare

  default:
    // Handle icMaxEnumFlare explicitly here if needed
    if (val == icMaxEnumFlare) {
      return "Max Flare"; // Special handling for the max sentinel value
    }
    // General default case for truly unexpected values
          std::snprintf(m_szStr, sizeof(m_szStr), "Unknown Flare '%d'", (int)val);
    return m_szStr;
  }
}


const icChar *CIccInfo::GetMeasurementGeometryName(icMeasurementGeometry val)
{
  switch (val) {  // Directly use enum type, casting is not needed
  case icGeometryUnknown:
    return "Geometry Unknown";
  case icGeometry045or450:
    return "Geometry 0-45 or 45-0";
  case icGeometry0dord0:
    return "Geometry 0-d or d-0";
  default:
          std::snprintf(m_szStr, sizeof(m_szStr), "Unknown Geometry '%d'", (int)val);
    return m_szStr;
  }
}

const icChar *CIccInfo::GetRenderingIntentName(icRenderingIntent val, bool bIsV5)
{
  switch (val) {
  case icPerceptual:
    return "Perceptual";

  case icRelativeColorimetric:
    if (bIsV5)
      return "Relative";
    else
      return "Relative Colorimetric";

  case icSaturation:
    return "Saturation";

  case icAbsoluteColorimetric:
    if (bIsV5)
      return "Absolute";
    else
      return "Absolute Colorimetric";

  default:
    snprintf(m_szStr, m_bufSize, "Unknown Intent '%d", val);
    return m_szStr;
  }
}

const icChar *CIccInfo::GetSpotShapeName(icSpotShape val)
{
  switch (val) {
  case icSpotShapeUnknown:
    return "Spot Shape Unknown";

  case icSpotShapePrinterDefault:
    return "Spot Shape Printer Default";

  case icSpotShapeRound:
    return "Spot Shape Round";

  case icSpotShapeDiamond:
    return "Spot Shape Diamond";

  case icSpotShapeEllipse:
    return "Spot Shape Ellipse";

  case icSpotShapeLine:
    return "Spot Shape Line";

  case icSpotShapeSquare:
    return "Spot Shape Square";

  case icSpotShapeCross:
    return "Spot Shape Cross";

  default:
    snprintf(m_szStr, m_bufSize, "Unknown Spot Shape '%d", val);
    return m_szStr;
  }
}

const icChar *CIccInfo::GetStandardObserverName(icStandardObserver val)
{
  switch (val) {
  case icStdObsUnknown:
    return "Unknown observer";

  case icStdObs1931TwoDegrees:
    return "CIE 1931 (two degree) standard observer";

  case icStdObs1964TenDegrees:
    return "CIE 1964 (ten degree) standard observer";

  default:
    snprintf(m_szStr, m_bufSize, "Unknown Observer '%d", val);
    return m_szStr;
  }
}

const icChar *CIccInfo::GetIlluminantName(icIlluminant val)
{
  switch (val) {
  case icIlluminantUnknown:
    return "Illuminant Unknown";

  case icIlluminantD50:
    return "Illuminant D50";

  case icIlluminantD65:
    return "Illuminant D65";

  case icIlluminantD93:
    return "Illuminant D93";

  case icIlluminantF2:
    return "Illuminant F2";

  case icIlluminantD55:
    return "Illuminant D55";

  case icIlluminantA:
    return "Illuminant A";

  case icIlluminantEquiPowerE:
    return "Illuminant EquiPowerE";

  case icIlluminantF8:
    return "Illuminant F8";

  case icIlluminantBlackBody:
    return "Illuminant Black Body";

  case icIlluminantDaylight:
    return "Illuminant Daylight";

  case icIlluminantB:
    return "Illuminant B";

  case icIlluminantC:
    return "Illuminant C";

  case icIlluminantF1:
    return "Illuminant F1";

  case icIlluminantF3:
    return "Illuminant F3";

  case icIlluminantF4:
    return "Illuminant F4";

  case icIlluminantF5:
    return "Illuminant F5";

  case icIlluminantF6:
    return "Illuminant F6";

  case icIlluminantF7:
    return "Illuminant F7";

  case icIlluminantF9:
    return "Illuminant F9";

  case icIlluminantF10:
    return "Illuminant F10";

  case icIlluminantF11:
    return "Illuminant F11";

  case icIlluminantF12:
    return "Illuminant F12";

  default:
    snprintf(m_szStr, m_bufSize, "Unknown Illuminant '%d", val);
    return m_szStr;
  }
}

const icChar *CIccInfo::GetMeasurementUnit(icSignature sig)
{
  switch (sig) {
    case icSigStatusA:
      return "Status A";

    case icSigStatusE:
      return "Status E";

    case icSigStatusI:
      return "Status I";

    case icSigStatusT:
      return "Status T";

    case icSigStatusM:
      return "Status M";

    case icSigDN:
      return "DIN with no polarizing filter";

    case icSigDNP:
      return "DIN with polarizing filter";

    case icSigDNN:
      return "Narrow band DIN with no polarizing filter";

    case icSigDNNP:
      return "Narrow band DIN with polarizing filter";

    default:
    {
      char buf[10] = {0};
      buf[0] = (char)(sig>>24);
      buf[1] = (char)(sig>>16);
      buf[2] = (char)(sig>>8);
      buf[3] = (char)(sig);
      buf[4] = '\0';

      snprintf(m_szStr, m_bufSize, "Unknown Measurement Type '%s'", buf);
      return m_szStr;
    }
  }
}


const icChar *CIccInfo::GetProfileID(icProfileID *profileID)
{
  char *ptr = m_szStr;
  int i;

  for (i=0; i<16; i++, ptr+=2) {
    snprintf(ptr, m_bufSize-i*2, "%02x", profileID->ID8[i]);
  }

  return m_szStr;
}

bool CIccInfo::IsProfileIDCalculated(icProfileID *profileID)
{
  int i;

  for (i=0; i<16; i++) {
    if (profileID->ID8[i])
      break;
  }

  return i<16;
}

const icChar *CIccInfo::GetColorantEncoding(icColorantEncoding colorant)
{
  switch(colorant) {
    case icColorantITU:
      return "ITU-R BT.709";

    case icColorantSMPTE:
      return "SMPTE RP145-1994";

    case icColorantEBU:
      return "EBU Tech.3213-E";

    case icColorantP22:
      return "P22";

    default:
      return "Customized Encoding";
  }
}

icValidateStatus CIccInfo::CheckData(std::string &sReport, const icXYZNumber &XYZ, std::string sDesc/*=""*/)
{
  icValidateStatus rv = icValidateOK;

  if (XYZ.X < 0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - XYZNumber: Negative X value!\n";
    rv = icValidateNonCompliant;
  }

  if (XYZ.Y < 0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - XYZNumber: Negative Y value!\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (XYZ.Z < 0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - XYZNumber: Negative Z value!\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}

icValidateStatus CIccInfo::CheckData(std::string &sReport, const icFloatXYZNumber &XYZ, std::string sDesc/*=""*/)
{
  icValidateStatus rv = icValidateOK;

  if (XYZ.X < 0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - FloatXYZNumber: Negative X value!\n";
    rv = icValidateNonCompliant;
  }

  if (XYZ.Y < 0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - FloatXYZNumber: Negative Y value!\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (XYZ.Z < 0) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - FloatXYZNumber: Negative Z value!\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}

icValidateStatus CIccInfo::CheckData(std::string &sReport, const icSpectralRange &range, std::string sDesc/*=""*/)
{
  icValidateStatus rv = icValidateOK;

  if (icF16toF(range.end)<=icF16toF(range.start)) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - spectralRange: end wavelength must be greater than start wavelength!\n";
    rv = icValidateNonCompliant;
  }

  if (range.steps<2) {
    sReport += icMsgValidateNonCompliant;
    sReport += sDesc;
    sReport += " - spectralRange: wavelength range must have at least two steps!\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}

icValidateStatus CIccInfo::CheckData(std::string &sReport, const icDateTimeNumber &dateTime, std::string sDesc/*=""*/)
{
  icValidateStatus rv = icValidateOK;

  struct tm *newtime;
  time_t long_time;

  time( &long_time );                /* Get time as long integer. */
  newtime = localtime( &long_time );

  const size_t bufSize = 128;
  icChar buf[bufSize];
  if (dateTime.year<1992) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    snprintf(buf, bufSize, " - %u: Invalid year!\n",dateTime.year);
    sReport += buf;
    rv = icValidateWarning;
  }

  int year = newtime->tm_year+1900;
  if (newtime->tm_mon==11 && newtime->tm_mday==31) {
    if (dateTime.year>(year+1)) {
      sReport += icMsgValidateWarning;
      sReport += sDesc;
      snprintf(buf, bufSize, " - %u: Invalid year!\n",dateTime.year);
      sReport += buf;
      rv = icMaxStatus(rv, icValidateWarning);
    }
  }
  else {
    if (dateTime.year>year) {
      sReport += icMsgValidateWarning;
      sReport += sDesc;
      snprintf(buf, bufSize, " - %u: Invalid year!\n",dateTime.year);
      sReport += buf;
      rv = icMaxStatus(rv, icValidateWarning);
    }
  }

  if (dateTime.month<1 || dateTime.month>12) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    snprintf(buf, bufSize, " - %u: Invalid month!\n",dateTime.month);
    sReport += buf;
    rv = icMaxStatus(rv, icValidateWarning);
  }

  if (dateTime.day<1 || dateTime.day>31) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    snprintf(buf, bufSize, " - %u: Invalid day!\n",dateTime.day);
    sReport += buf;
    rv = icMaxStatus(rv, icValidateWarning);
  }

  if (dateTime.month==2) {
    if (dateTime.day>29) {
      sReport += icMsgValidateWarning;
      sReport += sDesc;
      snprintf(buf, bufSize, " - %u: Invalid day for February!\n",dateTime.day);
      sReport += buf;
      rv = icMaxStatus(rv, icValidateWarning);
    }

    if (dateTime.day==29) {
      if ((dateTime.year%4)!=0) {
        sReport += icMsgValidateWarning;
        sReport += sDesc;
        snprintf(buf, bufSize, " - %u: Invalid day for February, year is not a leap year(%u)!\n",dateTime.day, dateTime.year);
        sReport += buf;
        rv = icMaxStatus(rv, icValidateWarning);
      }
    }
  }

  if (dateTime.hours>23) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    snprintf(buf, bufSize, " - %u: Invalid hour!\n",dateTime.hours);
    sReport += buf;
    rv = icMaxStatus(rv, icValidateWarning);
  }

  if (dateTime.minutes>59) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    snprintf(buf, bufSize, " - %u: Invalid minutes!\n",dateTime.minutes);
    sReport += buf;
    rv = icMaxStatus(rv, icValidateWarning);
  }

  if (dateTime.seconds>59) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    snprintf(buf, bufSize, " - %u: Invalid seconds!\n",dateTime.hours);
    sReport += buf;
    rv = icMaxStatus(rv, icValidateWarning);
  }

  return rv;
}

icValidateStatus CIccInfo::CheckLuminance(std::string &sReport, const icFloatXYZNumber &XYZ, std::string sDesc/*=""*/)
{
  icValidateStatus rv = icValidateOK;

  if (fabs(XYZ.Y - 1.0) < 0.01) {
    sReport += icMsgValidateWarning;
    sReport += sDesc;
    sReport += " - XYZNumber appears to be normalized! Y value should reflect absolute luminance.\n";
    rv = icValidateWarning;
  }

  return rv;
}


bool CIccInfo::IsValidSpace(icColorSpaceSignature sig)
{
  bool rv = true;

  switch(sig) {
  case icSigXYZData:
  case icSigLabData:
  case icSigLuvData:
  case icSigYCbCrData:
  case icSigYxyData:
  case icSigRgbData:
  case icSigGrayData:
  case icSigHsvData:
  case icSigHlsData:
  case icSigCmykData:
  case icSigCmyData:
  case icSigMCH1Data:
  case icSigNamedData:
  case icSigGamutData:
  case icSig2colorData:
  case icSig3colorData:
  case icSig4colorData: 
  case icSig5colorData:
  case icSig6colorData:
  case icSig7colorData:
  case icSig8colorData:
  case icSig9colorData:
  case icSig10colorData:
  case icSig11colorData:
  case icSig12colorData:
  case icSig13colorData:
  case icSig14colorData:
  case icSig15colorData:
    break;

  default:
    switch(icGetColorSpaceType(sig)) {
    case icSigNChannelData:
      break;
    default:
      rv = false;
    }
  }

  return rv;
}


bool CIccInfo::IsValidSpectralSpace(icColorSpaceSignature sig)
{
  bool rv = true;
  switch (icGetColorSpaceType(sig)) {
    case icSigReflectanceSpectralData:
    case icSigTransmisionSpectralData:
    case icSigRadiantSpectralData:
    case icSigBiSpectralReflectanceData:
    case icSigSparseMatrixReflectanceData:
      break;

    default:
      rv = false;
  }
  return rv;
}

CIccPixelBuf::CIccPixelBuf(int nChan/* =icDefaultPixelBufSize */) : m_buf{}
{
  if (nChan>icDefaultPixelBufSize) {
    m_pixel = new icFloatNumber[nChan];
  }
  else {
    m_pixel = m_buf;
  }
}

CIccPixelBuf::~CIccPixelBuf()
{
  if (m_pixel && m_pixel!=m_buf)
    delete [] m_pixel;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
