/** @file
    File:       IccUtilJson.cpp

    Contains:   Implementation of Utilities used for ICC/JSON processing

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

#include "IccUtilJson.h"
#include "IccUtil.h"
#include "IccTagLut.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>

// Library-wide flag: off by default (library/service/WASM contexts).
// The iccFromJson CLI tool sets this to true after argument parsing so
// file-based `"imports"` and `"file"` directives work in that context.
bool g_IccJsonAllowFileImports = false;

// Safely narrow size_t to icUInt32Number; returns 0 on truncation
static inline icUInt32Number icJsonSafeU32(size_t n, bool *overflow = nullptr)
{
  if (n > (size_t)UINT32_MAX) {
    if (overflow) *overflow = true;
    return 0;
  }
  if (overflow) *overflow = false;
  return (icUInt32Number)n;
}

// ---------------------------------------------------------------------------
// Hex data helpers (self-contained, no IccXML dependency)
// ---------------------------------------------------------------------------

static int hexVal(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

icUInt32Number icJsonGetHexData(void *pBuf, const char *szText, icUInt32Number nBufSize)
{
  unsigned char *pDest = (unsigned char*)pBuf;
  icUInt32Number rv = 0;
  while (rv < nBufSize && *szText) {
    int c1 = hexVal(szText[0]);
    if (c1 >= 0 && szText[1]) {
      int c2 = hexVal(szText[1]);
      if (c2 >= 0) {
        *pDest++ = (unsigned char)(c1 * 16 + c2);
        szText += 2;
        rv++;
        continue;
      }
    }
    szText++;
  }
  return rv;
}

icUInt32Number icJsonGetHexDataSize(const char *szText)
{
  icUInt32Number rv = 0;
  while (*szText) {
    if (hexVal(szText[0]) >= 0 && szText[1] && hexVal(szText[1]) >= 0) {
      szText += 2;
      rv++;
    } else {
      szText++;
    }
  }
  return rv;
}

std::string icJsonDumpHexData(const void *pBuf, size_t nBufSize)
{
  const unsigned char *p = (const unsigned char*)pBuf;
  const size_t bufSize = 3;
  char buf[bufSize];
  std::string result;
  result.reserve(nBufSize * 2);
  for (size_t i = 0; i < nBufSize; i++) {
    snprintf(buf, bufSize, "%02x", p[i]);
    result += buf;
  }
  return result;
}

// ---------------------------------------------------------------------------
// CLUT serialization
// ---------------------------------------------------------------------------

bool icCLUTDataToJson(IccJson &j, CIccCLUT *pCLUT, icConvertType nType, bool bSaveGridPoints)
{
  if (!pCLUT)
    return false;

  icUInt8Number i;
  icUInt8Number nInput  = pCLUT->GetInputDim();
  icUInt16Number nOutput = pCLUT->GetOutputChannels();

  if (bSaveGridPoints) {
    IccJson gridPts = IccJson::array();
    for (i = 0; i < nInput; i++)
      gridPts.push_back((int)pCLUT->GridPoint(i));
    j["gridPoints"] = gridPts;
  }

  // Resolve variable precision the same way IccXML does
  if (nType == icConvertVariable)
    nType = (pCLUT->GetPrecision() == 1) ? icConvert8Bit : icConvert16Bit;

  // Record precision so the parser knows how to interpret the integers
  if (nType == icConvert8Bit)
    j["precision"] = 1;
  else if (nType == icConvert16Bit)
    j["precision"] = 2;

  IccJson data = IccJson::array();
  icUInt32Number nTotal = pCLUT->NumPoints() * (icUInt32Number)nOutput;
  icFloatNumber *pData  = pCLUT->GetData(0);

  for (icUInt32Number k = 0; k < nTotal; k++) {
    switch (nType) {
      case icConvert8Bit:
        data.push_back((int)(pData[k] * 255.0f + 0.5f));
        break;
      case icConvert16Bit:
        data.push_back((int)(pData[k] * 65535.0f + 0.5f));
        break;
      default:
        data.push_back((double)pData[k]);
        break;
    }
  }
  j["data"] = data;
  return true;
}

bool icCLUTToJson(IccJson &j, CIccCLUT *pCLUT, icConvertType nType,
                  bool bSaveGridPoints, const char *szName)
{
  IccJson clut;
  if (!icCLUTDataToJson(clut, pCLUT, nType, bSaveGridPoints))
    return false;
  j[szName] = clut;
  return true;
}

// ---------------------------------------------------------------------------
// Device attribute value (JSON-specific: reads numeric value from JSON node)
// ---------------------------------------------------------------------------

icUInt64Number icJsonGetDeviceAttrValue(const IccJson &j)
{
  if (j.is_number_unsigned())
    return j.get<icUInt64Number>();
  return 0;
}

IccJson icJsonGetDeviceAttr(icUInt64Number devAttr)
{
  IccJson j;
  j["ReflectiveOrTransparency"] = (devAttr & icTransparency)       ? "transparency"  : "reflective";
  j["GlossyOrMatte"]            = (devAttr & icMatte)              ? "matte"         : "glossy";
  j["MediaPolarity"]            = (devAttr & icMediaNegative)      ? "negative"      : "positive";
  j["MediaColour"]              = (devAttr & icMediaBlackAndWhite) ? "blackAndWhite"  : "colour";
  icUInt64Number knownBits = (icUInt64Number)(icTransparency | icMatte | icMediaNegative | icMediaBlackAndWhite);
  icUInt64Number other = devAttr & ~knownBits;
  if (other) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)other);
    j["VendorSpecific"] = buf;
  }
  return j;
}

icUInt64Number icJsonParseDeviceAttr(const IccJson &j)
{
  icUInt64Number attr = 0;
  std::string s;
  jGetString(j, "ReflectiveOrTransparency", s); if (s == "transparency")   attr |= icTransparency;
  s.clear(); jGetString(j, "GlossyOrMatte",  s); if (s == "matte")         attr |= icMatte;
  s.clear(); jGetString(j, "MediaPolarity",  s); if (s == "negative")      attr |= icMediaNegative;
  s.clear(); jGetString(j, "MediaColour",    s); if (s == "blackAndWhite") attr |= icMediaBlackAndWhite;
  s.clear(); jGetString(j, "VendorSpecific", s);
  if (!s.empty()) {
    unsigned long long vendor = 0;
    if (sscanf(s.c_str(), "%llx", &vendor) != 1) vendor = 0;
    attr |= (icUInt64Number)vendor;
  }
  return attr;
}

// ---------------------------------------------------------------------------
// MPE element type name / signature lookup
// ---------------------------------------------------------------------------

struct SIccElemTypeName {
  icElemTypeSignature sig;
  const icChar       *name;
};

static const SIccElemTypeName g_IccElemTypeNames[] = {
  { icSigCurveSetElemType,            "CurveSetElement"          },
  { icSigMatrixElemType,              "MatrixElement"            },
  { icSigCLutElemType,                "CLutElement"              },
  { icSigExtCLutElemType,             "ExtCLutElement"           },
  { icSigBAcsElemType,                "BAcsElement"              },
  { icSigEAcsElemType,                "EAcsElement"              },
  { icSigCalculatorElemType,          "CalculatorElement"        },
  { icSigXYZToJabElemType,            "XYZToJabElement"          },
  { icSigJabToXYZElemType,            "JabToXYZElement"          },
  { icSigTintArrayElemType,           "TintArrayElement"         },
  { icSigToneMapElemType,             "ToneMapElement"           },
  { icSigEmissionMatrixElemType,      "EmissionMatrixElement"    },
  { icSigInvEmissionMatrixElemType,   "InvEmissionMatrixElement" },
  { icSigEmissionCLUTElemType,        "EmissionCLutElement"      },
  { icSigReflectanceCLUTElemType,     "ReflectanceCLutElement"   },
  { icSigEmissionObserverElemType,    "EmissionObserverElement"  },
  { icSigReflectanceObserverElemType, "ReflectanceObserverElement" },
  { icSigSparseMatrixElemType,        "SparseMatrixElement"      },
  { icSigUnknownElemType,             "UnknownElement"           },
  { (icElemTypeSignature)0,           NULL                       }
};

const icChar* icJsonGetElemTypeName(icElemTypeSignature sig)
{
  for (int i = 0; g_IccElemTypeNames[i].name; i++) {
    if (g_IccElemTypeNames[i].sig == sig)
      return g_IccElemTypeNames[i].name;
  }
  return "UnknownElement";
}

icElemTypeSignature icJsonGetElemTypeNameSig(const icChar *szName)
{
  if (!szName)
    return icSigUnknownElemType;
  for (int i = 0; g_IccElemTypeNames[i].name; i++) {
    if (!strcmp(g_IccElemTypeNames[i].name, szName))
      return g_IccElemTypeNames[i].sig;
  }
  // Fall back to 4-char signature parsing
  return (icElemTypeSignature)icGetSigVal(szName);
}

// ---------------------------------------------------------------------------
// JSON value extraction helpers
// ---------------------------------------------------------------------------

template <typename T>
bool jsonToValue(const IccJson &j, T &value)
{
  try {
    if (j.is_number()) {
      value = j.get<T>();
      return true;
    }
  }
  catch (...) {
  }
  return false;
}

template <>
bool jsonToValue<bool>(const IccJson &j, bool &value)
{
  try {
    if (j.is_boolean())         { value = j.get<bool>();            return true; }
    if (j.is_number_integer())  { value = j.get<int>() != 0;       return true; }
    if (j.is_number_unsigned()) { value = j.get<unsigned>() != 0;  return true; }
    if (j.is_number_float())    { value = j.get<float>() > 0.5f;   return true; }
  }
  catch (...) {
  }
  return false;
}

bool jsonToString(const IccJson &j, std::string &value)
{
  try {
    if (j.is_string()) { value = j.get<std::string>(); return true; }
  }
  catch (...) {
  }
  return false;
}

template <>
bool jsonToValue<std::string>(const IccJson &j, std::string &value)
{
  return jsonToString(j, value);
}

template <typename T>
bool jsonToArray(const IccJson &j, T *vals, int n)
{
  if (!j.is_array()) return false;
  if (n < 0) return false;
  bool overflow = false;
  icUInt32Number nSize = icJsonSafeU32(j.size(), &overflow);
  if (overflow) return false;
  icUInt32Number nExpected = (icUInt32Number)n;
  icUInt32Number nLimit = std::min(nExpected, nSize);
  icUInt32Number nValid = 0;
  for (icUInt32Number i = 0; i < nLimit; i++) {
    if (jsonToValue(j[i], vals[i])) nValid++;
  }
  return nValid == nExpected;
}

template <typename T>
bool jsonToArray(const IccJson &j, std::vector<T> &vals)
{
  if (!j.is_array()) return false;
  bool overflow = false;
  icUInt32Number nSize = icJsonSafeU32(j.size(), &overflow);
  if (overflow) return false;
  vals.resize(nSize);
  icUInt32Number nValid = 0;
  for (icUInt32Number i = 0; i < nSize; i++) {
    if (jsonToValue(j[i], vals[i])) nValid++;
    else vals[i] = T(0);
  }
  return nValid == nSize;
}

bool jsonExistsField(const IccJson &j, const char *field)
{
  return j.is_object() && j.contains(field);
}

template <typename T>
bool jGetValue(const IccJson &j, const char *field, T &value)
{
  if (!j.is_object() || !j.contains(field)) return false;
  return jsonToValue(j[field], value);
}

bool jGetString(const IccJson &j, const char *field, std::string &value)
{
  if (!j.is_object() || !j.contains(field)) return false;
  return jsonToString(j[field], value);
}

template <typename T>
bool jGetArray(const IccJson &j, const char *field, T *vals, int n)
{
  if (!j.is_object() || !j.contains(field)) return false;
  return jsonToArray(j[field], vals, n);
}

// Explicit instantiations
// On Linux/macOS icUInt32Number is uint32_t (same as unsigned int), but on
// Windows/MSVC it is unsigned long. Instantiate unsigned int only on MSVC
// to avoid duplicate-symbol errors on other platforms.
template bool jsonToValue<int>(const IccJson&, int&);
template bool jsonToValue<icUInt8Number>(const IccJson&, icUInt8Number&);
template bool jsonToValue<icUInt16Number>(const IccJson&, icUInt16Number&);
template bool jsonToValue<icUInt32Number>(const IccJson&, icUInt32Number&);
template bool jsonToValue<icInt16Number>(const IccJson&, icInt16Number&);
template bool jsonToValue<icFloat32Number>(const IccJson&, icFloat32Number&);
template bool jsonToValue<icFloat64Number>(const IccJson&, icFloat64Number&);
#ifdef _MSC_VER
template bool jsonToValue<unsigned int>(const IccJson&, unsigned int&);
#endif

template bool jsonToArray<double>(const IccJson&, double*, int);
template bool jsonToArray<float>(const IccJson&, float*, int);
template bool jsonToArray<icUInt8Number>(const IccJson&, icUInt8Number*, int);
template bool jsonToArray<icUInt16Number>(const IccJson&, icUInt16Number*, int);
template bool jsonToArray<icUInt32Number>(const IccJson&, icUInt32Number*, int);
template bool jsonToArray<double>(const IccJson&, std::vector<double>&);
template bool jsonToArray<float>(const IccJson&, std::vector<float>&);

template bool jGetValue<int>(const IccJson&, const char*, int&);
template bool jGetValue<icUInt8Number>(const IccJson&, const char*, icUInt8Number&);
template bool jGetValue<icUInt16Number>(const IccJson&, const char*, icUInt16Number&);
template bool jGetValue<icUInt32Number>(const IccJson&, const char*, icUInt32Number&);
template bool jGetValue<icInt16Number>(const IccJson&, const char*, icInt16Number&);
template bool jGetValue<icFloat32Number>(const IccJson&, const char*, icFloat32Number&);
template bool jGetValue<icFloat64Number>(const IccJson&, const char*, icFloat64Number&);
template bool jGetValue<bool>(const IccJson&, const char*, bool&);
#ifdef _MSC_VER
template bool jGetValue<unsigned int>(const IccJson&, const char*, unsigned int&);
#endif

template bool jGetArray<double>(const IccJson&, const char*, double*, int);
template bool jGetArray<float>(const IccJson&, const char*, float*, int);
template bool jGetArray<icUInt8Number>(const IccJson&, const char*, icUInt8Number*, int);
template bool jGetArray<icUInt16Number>(const IccJson&, const char*, icUInt16Number*, int);
template bool jGetArray<icUInt32Number>(const IccJson&, const char*, icUInt32Number*, int);

// ---------------------------------------------------------------------------
// CIccJsonArrayType template implementations
// ---------------------------------------------------------------------------

template <class T, icTagTypeSignature Tsig>
CIccJsonArrayType<T, Tsig>::CIccJsonArrayType()
  : m_nSize(0), m_pBuf(NULL)
{}

template <class T, icTagTypeSignature Tsig>
CIccJsonArrayType<T, Tsig>::~CIccJsonArrayType()
{
  delete[] m_pBuf;
}

template <class T, icTagTypeSignature Tsig>
bool CIccJsonArrayType<T, Tsig>::SetSize(icUInt32Number nSize)
{
  if (m_pBuf) { delete[] m_pBuf; m_pBuf = NULL; }
  m_nSize = nSize;
  if (nSize) {
    m_pBuf = new T[nSize];
    if (!m_pBuf) { m_nSize = 0; return false; }
  }
  return true;
}

template <class T, icTagTypeSignature Tsig>
bool CIccJsonArrayType<T, Tsig>::DumpArray(IccJson &j, const T *buf,
                                            icUInt32Number nBufSize,
                                            icConvertType /*nType*/)
{
  IccJson arr = IccJson::array();
  for (icUInt32Number i = 0; i < nBufSize; i++)
    arr.push_back(buf[i]);
  j = arr;
  return true;
}

template <class T, icTagTypeSignature Tsig>
bool CIccJsonArrayType<T, Tsig>::ParseArray(T *buf, icUInt32Number nBufSize,
                                             const IccJson &j)
{
  if (!j.is_array()) return false;
  bool overflow = false;
  icUInt32Number n = icJsonSafeU32(j.size(), &overflow);
  if (overflow) return false;
  if (n > nBufSize) n = nBufSize;
  for (icUInt32Number i = 0; i < n; i++) {
    if (!jsonToValue(j[i], buf[i]))
      return false;
  }
  return true;
}

template <class T, icTagTypeSignature Tsig>
bool CIccJsonArrayType<T, Tsig>::ParseArray(const IccJson &j)
{
  if (!j.is_array()) return false;
  bool overflow = false;
  icUInt32Number n = icJsonSafeU32(j.size(), &overflow);
  if (overflow) return false;
  if (!SetSize(n)) return false;
  for (icUInt32Number i = 0; i < n; i++) {
    if (!jsonToValue(j[i], m_pBuf[i]))
      return false;
  }
  return true;
}

// Explicit instantiations for all used types
template class CIccJsonArrayType<icUInt8Number,   icSigUInt8ArrayType>;
template class CIccJsonArrayType<icUInt16Number,  icSigUInt16ArrayType>;
template class CIccJsonArrayType<icUInt32Number,  icSigUInt32ArrayType>;
template class CIccJsonArrayType<icUInt64Number,  icSigUInt64ArrayType>;
template class CIccJsonArrayType<icFloatNumber,   (icTagTypeSignature)0x66637420>;
template class CIccJsonArrayType<icFloat32Number, icSigFloat32ArrayType>;
template class CIccJsonArrayType<icFloat64Number, icSigFloat64ArrayType>;
