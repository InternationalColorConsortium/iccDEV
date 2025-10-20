/** @file
    File:       IccUtil.h

    Contains:   Implementation of utility classes/functions

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
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCUTIL_H
#define _ICCUTIL_H

#include "IccDefs.h"
#include "IccProfLibConf.h"
#include <string>
#include <limits>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

ICCPROFLIB_API void* icRealloc(void *ptr, size_t size);

#define icPiNum 3.14159265358979323846
#define icPosInfinity (std::numeric_limits<icFloatNumber>::infinity()) 
#define icNegInfinity (-std::numeric_limits<icFloatNumber>::infinity())
#define icNotANumber (std::numeric_limits<icFloatNumber>::quiet_NaN())
#define icNotZero(v) ((v)>1.0e-8 || (v)<-1.0e-8)

ICCPROFLIB_API bool icIsNear(icFloatNumber v1, icFloatNumber v2, icFloatNumber nearRange = 1.0e-8);

ICCPROFLIB_API double icRoundOffset(double v);

ICCPROFLIB_API icValidateStatus icMaxStatus(icValidateStatus s1, icValidateStatus s2);
ICCPROFLIB_API bool  icIsSpaceCLR(icColorSpaceSignature sig);

ICCPROFLIB_API void icColorIndexName(icChar *szName, icColorSpaceSignature csSig,
                      int nIndex, int nColors, const icChar *szUnknown);
ICCPROFLIB_API void icColorValue(icChar *szValue, icFloatNumber nValue,
                  icColorSpaceSignature csSig, int nIndex, bool bUseLegacy=false);

ICCPROFLIB_API bool icIsIllumD50(icXYZNumber xyz);

ICCPROFLIB_API bool icMatrixInvert3x3(icFloatNumber *matrix);
ICCPROFLIB_API void icMatrixMultiply3x3(icFloatNumber *result,
                                        const icFloatNumber *l,
                                        const icFloatNumber *r);
ICCPROFLIB_API void icVectorApplyMatrix3x3(icFloatNumber *result,
                                           const icFloatNumber *m,
                                           const icFloatNumber *v);

ICCPROFLIB_API icS15Fixed16Number icDtoF(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icFtoD(icS15Fixed16Number num);

ICCPROFLIB_API icU16Fixed16Number icDtoUF(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icUFtoD(icU16Fixed16Number num);

ICCPROFLIB_API icU1Fixed15Number icDtoUSF(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icUSFtoD(icU1Fixed15Number num);

ICCPROFLIB_API icU8Fixed8Number icDtoUCF(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icUCFtoD(icU8Fixed8Number num);

ICCPROFLIB_API icFloat32Number icF16toF(icFloat16Number num);
ICCPROFLIB_API icFloat16Number icFtoF16(icFloat32Number num);

/*0 to 255 <-> 0.0 to 1.0*/
ICCPROFLIB_API icUInt8Number icFtoU8(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icU8toF(icUInt8Number num);

/*0 to 65535 <-> 0.0 to 1.0*/
ICCPROFLIB_API icUInt16Number icFtoU16(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icU16toF(icUInt16Number num);

/*0 to 255 <-> -128.0 to 127.0*/
ICCPROFLIB_API icUInt8Number icABtoU8(icFloatNumber num);
ICCPROFLIB_API icFloatNumber icU8toAB(icUInt8Number num);

ICCPROFLIB_API extern icFloatNumber icD50XYZ[3];
ICCPROFLIB_API extern icFloatNumber icD50XYZxx[3];

ICCPROFLIB_API void icNormXYZ(icFloatNumber *XYZ, icFloatNumber *WhiteXYZ=NULL);
ICCPROFLIB_API void icDeNormXYZ(icFloatNumber *XYZ, icFloatNumber *WhiteXYZ=NULL);

ICCPROFLIB_API icFloatNumber icCubeth(icFloatNumber v);
ICCPROFLIB_API icFloatNumber icICubeth(icFloatNumber v);

ICCPROFLIB_API void icXYZtoLab(icFloatNumber *Lab, const icFloatNumber *XYZ=NULL, const icFloatNumber *WhiteXYZ=NULL);
ICCPROFLIB_API void icLabtoXYZ(icFloatNumber *XYZ, const icFloatNumber *Lab=NULL, const icFloatNumber *WhiteXYZ=NULL);

ICCPROFLIB_API void icLab2Lch(icFloatNumber *Lch, icFloatNumber *Lab=NULL);
ICCPROFLIB_API void icLch2Lab(icFloatNumber *Lab, icFloatNumber *Lch=NULL);

ICCPROFLIB_API icFloatNumber icMin(icFloatNumber v1, icFloatNumber v2);
ICCPROFLIB_API icFloatNumber icMax(icFloatNumber v1, icFloatNumber v2);

ICCPROFLIB_API icUInt32Number icIntMin(icUInt32Number v1, icUInt32Number v2);
ICCPROFLIB_API icUInt32Number icIntMax(icUInt32Number v1, icUInt32Number v2);

ICCPROFLIB_API icFloatNumber icDeltaE(const icFloatNumber *Lab1, const icFloatNumber *Lab2);

ICCPROFLIB_API icFloatNumber icRmsDif(const icFloatNumber *v1, const icFloatNumber *v2, icUInt32Number nSample);

ICCPROFLIB_API bool icValidTagPos(const icPositionNumber& pos, icUInt32Number nTagHeaderSize, icUInt32Number nTagSize, bool bAllowEmpty=false);
ICCPROFLIB_API bool icValidOverlap(const icPositionNumber& pos1, const icPositionNumber& pos2, bool bAllowSame=true);

/**Floating point encoding of Lab in PCS is in range 0.0 to 1.0 */
///Here are some conversion routines to convert to regular Lab encoding
ICCPROFLIB_API void icLabFromPcs(icFloatNumber *Lab);
ICCPROFLIB_API void icLabToPcs(icFloatNumber *Lab);

/** Floating point encoding of XYZ in PCS is in range 0.0 to 1.0
 (Note: X=1.0 is encoded as about 0.5)*/
///Here are some conversion routines to convert to regular XYZ encoding
ICCPROFLIB_API void icXyzFromPcs(icFloatNumber *XYZ);
ICCPROFLIB_API void icXyzToPcs(icFloatNumber *XYZ);


ICCPROFLIB_API void icMemDump(std::string &sDump, void *pBuf, icUInt32Number nNum);
ICCPROFLIB_API void icMatrixDump(std::string &sDump, icS15Fixed16Number *pMatrix);
ICCPROFLIB_API const icChar* icGetSig(icChar *pBuf, icUInt32Number sig, bool bGetHexVal=true);
ICCPROFLIB_API const icChar* icGet16bitSig(icChar* pBuf, icUInt16Number sig, bool bGetHexVal=true);
ICCPROFLIB_API const icChar* icGetSigStr(icChar *pBuf, icUInt32Number nSig);
ICCPROFLIB_API const icChar* icGetColorSig(icChar *pBuf, icUInt32Number sig, bool bGetHexVal=true);
ICCPROFLIB_API const icChar *icGetColorSigStr(icChar *pBuf, icUInt32Number nSig);

#define icUtf8StrCmp(x, y) strcmp((const char*)x, (const char*)y)

ICCPROFLIB_API std::string icGetSigPath(icUInt32Number sig);
ICCPROFLIB_API icSignature icGetFirstSigPathSig(std::string sigPath);
ICCPROFLIB_API icSignature icGetSecondSigPathSig(std::string sigPath);
ICCPROFLIB_API icSignature icGetLastSigPathSig(std::string sigPath);

ICCPROFLIB_API icUInt32Number icGetSigVal(const icChar *pBuf);
ICCPROFLIB_API icUInt32Number icGetSpaceSamples(icColorSpaceSignature sig);
ICCPROFLIB_API icUInt32Number icGetSpectralSpaceSamples(const icHeader *pHdr);
ICCPROFLIB_API icUInt32Number icGetMaterialColorSpaceSamples(icMaterialColorSignature sig);

bool ICCPROFLIB_API icSameSpectralRange(const icSpectralRange &rng1, const icSpectralRange &rng2);

ICCPROFLIB_API icUInt8Number icGetStorageTypeBytes(icUInt16Number nStorageType);

ICCPROFLIB_API extern const char *icMsgValidateWarning;
ICCPROFLIB_API extern const char *icMsgValidateNonCompliant;
ICCPROFLIB_API extern const char *icMsgValidateCriticalError;
ICCPROFLIB_API extern const char* icMsgValidateInformation;

#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
inline void icSwab16Ptr(void *pVoid)
{
  icUInt8Number *ptr = (icUInt8Number*)pVoid;
  icUInt8Number tmp;

  tmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = tmp;
}

inline void icSwab16Array(void *pVoid, int num)
{
  icUInt8Number *ptr = (icUInt8Number*)pVoid;
  icUInt8Number tmp;

  while (num>0) {
    tmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = tmp;
    ptr += 2;
    num--;
  }
}

inline void icSwab32Ptr(void *pVoid)
{
  icUInt8Number *ptr = (icUInt8Number*)pVoid;
  icUInt8Number tmp;

  tmp = ptr[0]; ptr[0] = ptr[3]; ptr[3] = tmp;
  tmp = ptr[1]; ptr[1] = ptr[2]; ptr[2] = tmp;
}

inline void icSwab32Array(void *pVoid, int num)
{
  icUInt8Number *ptr = (icUInt8Number*)pVoid;
  icUInt8Number tmp;

  while (num>0) {
    tmp = ptr[0]; ptr[0] = ptr[3]; ptr[3] = tmp;
    tmp = ptr[1]; ptr[1] = ptr[2]; ptr[2] = tmp;
    ptr += 4;
    num--;
  }

}

inline void icSwab64Ptr(void *pVoid)
{
  icUInt8Number *ptr = (icUInt8Number*)pVoid;
  icUInt8Number tmp;

  tmp = ptr[0]; ptr[0] = ptr[7]; ptr[7] = tmp;
  tmp = ptr[1]; ptr[1] = ptr[6]; ptr[6] = tmp;
  tmp = ptr[2]; ptr[2] = ptr[5]; ptr[5] = tmp;
  tmp = ptr[3]; ptr[3] = ptr[4]; ptr[4] = tmp;
}

inline void icSwab64Array(void *pVoid, int num)
{
  icUInt8Number *ptr = (icUInt8Number*)pVoid;
  icUInt8Number tmp;

  while (num>0) {
    tmp = ptr[0]; ptr[0] = ptr[7]; ptr[7] = tmp;
    tmp = ptr[1]; ptr[1] = ptr[6]; ptr[6] = tmp;
    tmp = ptr[2]; ptr[2] = ptr[5]; ptr[5] = tmp;
    tmp = ptr[3]; ptr[3] = ptr[4]; ptr[4] = tmp;
    ptr += 8;
    num--;
  }

}
#else //!ICC_BYTE_ORDER_LITTLE_ENDIAN
#define icSwab16Ptr(flt)
#define icSwab16Array(flt, n)
#define icSwab32Ptr(flt)
#define icSwab32Array(flt, n)
#define icSwab64Ptr(flt)
#define icSwab64Array(flt, n)
#endif

#define icSwab16(flt) icSwab16Ptr(&flt)
#define icSwab32(flt) icSwab32Ptr(&flt)
#define icSwab64(flt) icSwab64Ptr(&flt)


/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: 
 *  This is a utility class which can be used to get profile info 
 *  for printing. The member functions are used to convert signatures
 *  and other enum values to character strings for printing.
 **************************************************************************
 */
class ICCPROFLIB_API  CIccInfo {
public:
  CIccInfo();
  virtual ~CIccInfo();
  //Signature values
  const icChar *GetVersionName(icUInt32Number val);
  const icChar *GetSubClassVersionName(icUInt32Number val);
  const icChar *GetDeviceAttrName(icUInt64Number val);
  const icChar *GetProfileFlagsName(icUInt32Number val, bool bCheckMCS=false);

  const icChar *GetTagSigName(icTagSignature sig);
  const icChar *GetTechnologySigName(icTechnologySignature sig);
  const icChar *GetTagTypeSigName(icTagTypeSignature sig);
  const icChar *GetStructSigName(icStructSignature sig);
  const icChar *GetArraySigName(icArraySignature sig);
  const icChar *GetColorSpaceSigName(icColorSpaceSignature sig);
  const icChar *GetProfileClassSigName(icProfileClassSignature sig);
  const icChar *GetPlatformSigName(icPlatformSignature sig);
  const icChar *GetCmmSigName(icCmmSignature sig);
  const icChar *GetReferenceMediumGamutSigNameName(icReferenceMediumGamutSignature sig);
  const icChar *GetColorimetricIntentImageStateName(icColorimetricIntentImageStateSignature sig);
  const icChar *GetSpectralColorSigName(icSpectralColorSignature sig);
  const icChar *GetElementTypeSigName(icElemTypeSignature sig);

  const icChar *GetSigName(icUInt32Number val);

  const icChar *GetPathEntrySigName(icUInt32Number val);

  std::string GetSigPathName(std::string sigPath);

  //Other values
  const icChar *GetMeasurementFlareName(icMeasurementFlare val);
  const icChar *GetMeasurementGeometryName(icMeasurementGeometry val);
  const icChar *GetRenderingIntentName(icRenderingIntent val, bool bIsV5=false);
  const icChar *GetSpotShapeName(icSpotShape val);
  const icChar *GetStandardObserverName(icStandardObserver val);
  const icChar *GetIlluminantName(icIlluminant val);

  const icChar *GetUnknownName(icUInt32Number val);
  const icChar *GetMeasurementUnit(icSignature sig);
  const icChar *GetProfileID(icProfileID *profileID);
  const icChar *GetColorantEncoding(icColorantEncoding colorant);
  
  bool IsProfileIDCalculated(icProfileID *profileID);
  icValidateStatus CheckData(std::string &sReport, const icDateTimeNumber &dateTime, std::string sDesc="");
  icValidateStatus CheckData(std::string &sReport, const icXYZNumber &XYZ, std::string sDesc="");
  icValidateStatus CheckData(std::string &sReport, const icFloatXYZNumber &XYZ, std::string sDesc="");
  icValidateStatus CheckData(std::string &sReport, const icSpectralRange &range, std::string sDesc="");

  icValidateStatus CheckLuminance(std::string &sReport, const icFloatXYZNumber &XYZ, std::string sDesc="");

  bool IsValidSpace(icColorSpaceSignature sig);
  bool IsValidSpectralSpace(icColorSpaceSignature sig);

protected:
  icChar m_szStr[128];
  icChar m_szSigStr[128];
  std::string *m_str;
};

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: 
 *  This is a utility class which can be for pixel sample storage.
 *  It features constant allocation access time when the pixel buffer size
 *  is less thatn icDefaultPixelBufSize
 **************************************************************************
 */
#define icDefaultPixelBufSize 100
class ICCPROFLIB_API CIccPixelBuf
{
public:
  CIccPixelBuf(int nChan=icDefaultPixelBufSize);
  ~CIccPixelBuf();
  icFloatNumber &operator[](int nPos) { return m_pixel[nPos]; }
  icFloatNumber *get() { return m_pixel;}
  
  operator icFloatNumber *() { return m_pixel; }
  operator void *() { return m_pixel; }

protected:
  icFloatNumber m_buf[icDefaultPixelBufSize];
  icFloatNumber *m_pixel;
};



/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Interface for performing Cmm Environment Variable Lookup
**************************************************************************
*/
class ICCPROFLIB_API IIccCmmEnvVarLookup
{
public:
  virtual ~IIccCmmEnvVarLookup() {}

  virtual bool GetEnvVar(icSigCmmEnvVar sig, icFloatNumber &val)=0;
  virtual bool IndexedEnvVar(icUInt32Number nIndex, icSigCmmEnvVar &sig, icFloatNumber &val)=0;
};




extern ICCPROFLIB_API CIccInfo icInfo;

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif
