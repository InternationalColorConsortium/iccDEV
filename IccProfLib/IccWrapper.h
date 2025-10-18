/** @file
File:       IccWrapper.h

Contains:   Header file for C Wrapper interface functions for CIccProfile 
            and CIccCmm classes.

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

#ifndef _ICC_WRAPPER_H
#define _ICC_WRAPPER_H

#include "IccDefs.h"
#include "IccProfile.h"
#include "IccCmm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void CIccCmmHandle;
typedef void CIccApplyHandle;
typedef void CIccProfileHandle;

typedef struct {
  icColorSpaceSignature srcSpace;
  icColorSpaceSignature dstSpace;
  icUInt16Number srcSamples;
  icUInt16Number dstSamples;
} SIccCmmStruct;

typedef struct {
  icUInt32Number nVars;
  icSigCmmEnvVar *sigs;
  icFloat32Number *vals;
} SIccCmmEnvVars;

typedef unsigned char icBoolean;

ICCPROFLIB_API CIccCmmHandle* IccCmmCreate(icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace, icBoolean bFirstIsInput);
ICCPROFLIB_API icStatusCMM IccCmmAttachProfileFile(CIccCmmHandle *pCmm,
                                                   const char *szFname,
                                                   icRenderingIntent nIntent,
                                                   icXformInterp nInterp,
                                                   CIccProfileHandle *pPcc, //may be NULL (ownership not taken)
                                                   icXformLutType nLutType,
                                                   icBoolean bUseDtoBTags,
                                                   icBoolean bUseBPC,
                                                   SIccCmmEnvVars *pVars); //may be NULL (ownership not taken)
ICCPROFLIB_API icStatusCMM IccCmmAttachProfileHandle(CIccCmmHandle *pCmm,
                                                     CIccProfileHandle *pProfile, // CMM assumes ownership of profile
                                                     icRenderingIntent nIntent,
                                                     icXformInterp nInterp,
                                                     CIccProfileHandle *pPcc,
                                                     icXformLutType nLutType,
                                                     icBoolean bUseDtoBTags,
                                                     icBoolean bUseBPC,
                                                     SIccCmmEnvVars *pVars);
ICCPROFLIB_API icStatusCMM IccCmmBegin(CIccCmmHandle *pCmm);
ICCPROFLIB_API CIccApplyHandle* IccCmmGetApply(CIccCmmHandle *pCmm);
ICCPROFLIB_API icStatusCMM IccCmmGetInfo(CIccCmmHandle *pCmm, SIccCmmStruct *pCmmInfo);
ICCPROFLIB_API icStatusCMM IccCmmApplyFloat(CIccCmmHandle *pCmm, icFloatNumber *pTo, icFloatNumber *pFrom);
ICCPROFLIB_API icStatusCMM IccCmmApplyFloatMulti(CIccCmmHandle *pCmm, icFloatNumber *pTo, icFloatNumber *pFrom, icUInt32Number nPixels);
ICCPROFLIB_API void IccCmmFree(CIccCmmHandle *pCmm);

ICCPROFLIB_API icStatusCMM IccApplyApplyFloat(CIccApplyHandle *pApply, icFloatNumber *pTo, icFloatNumber *pFrom);
ICCPROFLIB_API icStatusCMM IccApplyApplyFloatMulti(CIccApplyHandle *pApply, icFloatNumber *pTo, icFloatNumber *pFrom, icUInt32Number nPixels);
ICCPROFLIB_API void IccApplyFree(CIccApplyHandle *pApply);

ICCPROFLIB_API CIccProfileHandle* IccProfileReadHandle(const char *szFname);
ICCPROFLIB_API CIccProfileHandle* IccProfileOpenHandle(const char *szFname);
ICCPROFLIB_API icBoolean IccProfileGetHeader(CIccProfileHandle *pProfile, icHeader *pHeader);
ICCPROFLIB_API void IccProfileFree(CIccProfileHandle *pProfile);

#ifdef __cplusplus
}
#endif

#endif //_ICC_WRAPPER_H
