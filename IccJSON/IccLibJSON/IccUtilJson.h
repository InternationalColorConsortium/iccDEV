/** @file
    File:       IccUtilJson.h

    Contains:   Header for Utilities used for ICC/JSON processing

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

#ifndef _ICCUTILJSON_H
#define _ICCUTILJSON_H

#include "IccUtil.h"
#include "IccTag.h"
#include "IccJsonConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using IccJson = nlohmann::json;

// ---------------------------------------------------------------------------
// Hex data helpers (binary blobs stored as hex strings in JSON)
// ---------------------------------------------------------------------------
icUInt32Number icJsonGetHexData(void *pBuf, const char *szText, icUInt32Number nBufSize);
icUInt32Number icJsonGetHexDataSize(const char *szText);
std::string    icJsonDumpHexData(const void *pBuf, size_t nBufSize);

// ---------------------------------------------------------------------------
// CLUT serialization
// ---------------------------------------------------------------------------
bool      icCLUTDataToJson(IccJson &j, CIccCLUT *pCLUT, icConvertType nType, bool bSaveGridPoints = false);
bool      icCLUTToJson(IccJson &j, CIccCLUT *pCLUT, icConvertType nType,
                       bool bSaveGridPoints = false, const char *szName = "CLUT");
CIccCLUT *icCLUTFromJson(const IccJson &j, int nIn, int nOut,
                         icConvertType nType, std::string &parseStr);

// ---------------------------------------------------------------------------
// Device attribute helpers
// ---------------------------------------------------------------------------
icUInt64Number icJsonGetDeviceAttrValue(const IccJson &j);
IccJson        icJsonGetDeviceAttr(icUInt64Number devAttr);
icUInt64Number icJsonParseDeviceAttr(const IccJson &j);

// ---------------------------------------------------------------------------
// MPE element type name / signature lookup
// ---------------------------------------------------------------------------
const icChar*        icJsonGetElemTypeName(icElemTypeSignature elemTypeSig);
icElemTypeSignature  icJsonGetElemTypeNameSig(const icChar *szElemType);

// ---------------------------------------------------------------------------
// JSON value extraction helpers (ported from IccCommon/IccJsonUtil)
// ---------------------------------------------------------------------------

// Extract a typed numeric value from a JSON node.
// Returns true if the node has a compatible type and value was assigned.
template <typename T>
bool jsonToValue(const IccJson &j, T &value);

template <>
bool jsonToValue<bool>(const IccJson &j, bool &value);

template <>
bool jsonToValue<std::string>(const IccJson &j, std::string &value);

// Extract a string value from a JSON node.
bool jsonToString(const IccJson &j, std::string &value);

// Extract a fixed-size C array from a JSON array node.
// Returns true only when all n elements were found and numeric.
template <typename T>
bool jsonToArray(const IccJson &j, T *vals, int n);

// Extract a variable-length array into a std::vector.
template <typename T>
bool jsonToArray(const IccJson &j, std::vector<T> &vals);

// True when a named field exists inside a JSON object.
bool jsonExistsField(const IccJson &j, const char *field);

// Field-level convenience: combines j.contains(field) with jsonToValue.
// Returns true if the field was present and the value was successfully assigned.
template <typename T>
bool jGetValue(const IccJson &j, const char *field, T &value);

// Field-level convenience for strings.
bool jGetString(const IccJson &j, const char *field, std::string &value);

// Field-level convenience for fixed-size arrays.
template <typename T>
bool jGetArray(const IccJson &j, const char *field, T *vals, int n);

// ---------------------------------------------------------------------------
// Typed array helpers (parallel to CIccXmlArrayType)
// ---------------------------------------------------------------------------
template <class T, icTagTypeSignature Tsig>
class CIccJsonArrayType
{
public:
  CIccJsonArrayType();
  ~CIccJsonArrayType();

  static bool DumpArray(IccJson &j, const T *buf, icUInt32Number nBufSize,
                        icConvertType nType);
  static bool ParseArray(T *buf, icUInt32Number nBufSize, const IccJson &j);

  bool ParseArray(const IccJson &j);
  bool SetSize(icUInt32Number nSize);

  T              *GetBuf()  { return m_pBuf; }
  icUInt32Number  GetSize() { return m_nSize; }

protected:
  icUInt32Number  m_nSize;
  T              *m_pBuf;
};

#define icSigJsonFloatArrayType ((icTagTypeSignature)0x66637420)  /* 'flt ' */

typedef CIccJsonArrayType<icUInt8Number,  icSigUInt8ArrayType>   CIccJsonUInt8Array;
typedef CIccJsonArrayType<icUInt16Number, icSigUInt16ArrayType>  CIccJsonUInt16Array;
typedef CIccJsonArrayType<icUInt32Number, icSigUInt32ArrayType>  CIccJsonUInt32Array;
typedef CIccJsonArrayType<icUInt64Number, icSigUInt64ArrayType>  CIccJsonUInt64Array;
typedef CIccJsonArrayType<icFloatNumber,  icSigJsonFloatArrayType> CIccJsonFloatArray;
typedef CIccJsonArrayType<icFloat32Number,icSigFloat32ArrayType> CIccJsonFloat32Array;
typedef CIccJsonArrayType<icFloat64Number,icSigFloat64ArrayType> CIccJsonFloat64Array;

#endif // _ICCUTILJSON_H
