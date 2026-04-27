/** @file
    File:       IccMpeJson.cpp

    Contains:   Implementation of ICC multi-process element JSON format conversions

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

#include "IccMpeJson.h"
#include "IccTagJson.h"
#include "IccIoJson.h"
#include "IccUtilJson.h"
#include "IccUtil.h"
#include "IccSolve.h"
#include "IccCAM.h"
#include "IccMpeFactory.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>

static inline icUInt32Number icJsonSafeU32(size_t n, bool *overflow = nullptr)
{
  if (n > 0xFFFFFFFFUL) {
    if (overflow) *overflow = true;
    return 0;
  }
  if (overflow) *overflow = false;
  return (icUInt32Number)n;
}

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

// ===========================================================================
// CIccMpeJsonUnknown
// ===========================================================================

bool CIccMpeJsonUnknown::ToJson(IccJson &j)
{
  j["unknownData"] = icJsonDumpHexData(m_pData, m_nSize);
  return true;
}

bool CIccMpeJsonUnknown::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  m_nInputChannels  = (icUInt16Number)nIn;
  m_nOutputChannels = (icUInt16Number)nOut;
  if (j.contains("unknownData") && j["unknownData"].is_string()) {
    std::string hex = j["unknownData"].get<std::string>();
    m_nSize = icJsonGetHexDataSize(hex.c_str());
    if (m_pData) { free(m_pData); m_pData = nullptr; }
    if (m_nSize) {
      m_pData = (icUInt8Number*)malloc(m_nSize);
      if (!m_pData) { m_nSize = 0; return false; }
      icJsonGetHexData(m_pData, hex.c_str(), m_nSize);
    }
  }
  return true;
}

// ===========================================================================
// Segment position helpers (JSON has no native infinity -- use strings)
// ===========================================================================

// Emit pos as a JSON number for finite values, or a string for +/-infinity.
static void icJsonSetSegPos(IccJson &j, const char *field, icFloatNumber pos)
{
  if (pos == icMinFloat32Number)
    j[field] = "-infinity";
  else if (pos == icMaxFloat32Number)
    j[field] = "+infinity";
  else
    j[field] = (double)pos;
}

static icFloatNumber icJsonSegPosFromStr(const std::string &s)
{
  if (s.size() >= 4 && s.substr(s.size()-3) == "inf") return icMaxFloat32Number;
  if (!s.empty() && s[0] == '-' && s.size() >= 4)     return icMinFloat32Number;
  return (icFloatNumber)atof(s.c_str());
}

static icFloatNumber icJsonSegPosFromJson(const IccJson &jv)
{
  if (jv.is_string()) return icJsonSegPosFromStr(jv.get<std::string>());
  if (jv.is_number()) return (icFloatNumber)jv.get<double>();
  return 0.0f;
}

static int icFormulaCurveSegParamCount(int funcType)
{
  switch (funcType) {
    case 0:                         return 4;
    case 1: case 2: case 3: case 4: return 5;
    case 5: case 7:                 return 6;
    case 6:                         return 7;
    default:                        return -1;
  }
}

// ---------------------------------------------------------------------------
// Local JSON-aware derived segment classes (need access to protected members)
// ---------------------------------------------------------------------------

class CIccFormulaCurveSegmentJson : public CIccFormulaCurveSegment
{
public:
  CIccFormulaCurveSegmentJson(icFloatNumber start, icFloatNumber end)
    : CIccFormulaCurveSegment(start, end) {}
  CIccFormulaCurveSegmentJson(const CIccFormulaCurveSegment *p)
    : CIccFormulaCurveSegment(*p) {}

  bool ToJson(IccJson &j) const {
    j["type"]         = "FormulaSegment";
    j["functionType"] = (int)m_nFunctionType;
    if (m_nReserved)
      j["reserved"]   = (int)m_nReserved;
    if (m_nReserved2)
      j["reserved2"]  = (int)m_nReserved2;
    IccJson params = IccJson::array();
    for (int i = 0; i < m_nParameters; i++)
      params.push_back((double)m_params[i]);
    j["parameters"] = params;
    return true;
  }

  bool ParseJson(const IccJson &j, std::string &parseStr) {
    int funcType = 0, reserved = 0, reserved2 = 0;
    jGetValue(j, "functionType", funcType);
    jGetValue(j, "reserved",     reserved);
    jGetValue(j, "reserved2",    reserved2);
    m_nFunctionType = (icUInt16Number)funcType;
    m_nReserved     = (icUInt32Number)reserved;
    m_nReserved2    = (icUInt16Number)reserved2;

    int nParams = icFormulaCurveSegParamCount(funcType);
    if (nParams < 0) {
      parseStr += "Unsupported FunctionType in FormulaSegment\n";
      return false;
    }
    m_nParameters = (icUInt8Number)nParams;

    if (m_params) { free(m_params); m_params = nullptr; }
    if (nParams > 0) {
      m_params = (icFloatNumber*)malloc(nParams * sizeof(icFloatNumber));
      if (!m_params) return false;
      for (int i = 0; i < nParams; i++) m_params[i] = 0.0f;
      if (jsonExistsField(j, "parameters") && j["parameters"].is_array()) {
        int cnt = std::min(nParams, (int)j["parameters"].size());
        for (int i = 0; i < cnt; i++)
          m_params[i] = (icFloatNumber)j["parameters"][i].get<double>();
      }
    }
    return true;
  }
};


class CIccSampledCurveSegmentJson : public CIccSampledCurveSegment
{
public:
  CIccSampledCurveSegmentJson(icFloatNumber start, icFloatNumber end)
    : CIccSampledCurveSegment(start, end) {}
  CIccSampledCurveSegmentJson(const CIccSampledCurveSegment *p)
    : CIccSampledCurveSegment(*p) {}

  bool ToJson(IccJson &j) const {
    j["type"] = "SampledSegment";
    IccJson samples = IccJson::array();
    for (icUInt32Number i = 0; i < m_nCount; i++)
      samples.push_back((double)m_pSamples[i]);
    j["samples"] = samples;
    return true;
  }

  bool ParseJson(const IccJson &j, std::string &parseStr) {
    if (!jsonExistsField(j, "samples") || !j["samples"].is_array()) {
      parseStr += "Missing samples in SampledSegment\n";
      return false;
    }
    const IccJson &arr = j["samples"];
    if (!SetSize(icJsonSafeU32(arr.size())))
      return false;
    for (icUInt32Number i = 0; i < icJsonSafeU32(arr.size()); i++)
      m_pSamples[i] = (icFloatNumber)arr[i].get<double>();
    return true;
  }
};

// ===========================================================================
// CIccSegmentedCurveJson
// ===========================================================================

bool CIccSegmentedCurveJson::ToJson(IccJson &j)
{
  IccJson segs = IccJson::array();
  for (CIccCurveSegment *pSeg : *m_list) {
    if (!pSeg) continue;
    IccJson jSeg;
    icJsonSetSegPos(jSeg, "start", pSeg->StartPoint());
    icJsonSetSegPos(jSeg, "end",   pSeg->EndPoint());

    if (pSeg->GetType() == icSigFormulaCurveSeg) {
      CIccFormulaCurveSegmentJson js(static_cast<CIccFormulaCurveSegment*>(pSeg));
      if (!js.ToJson(jSeg)) return false;
    }
    else if (pSeg->GetType() == icSigSampledCurveSeg) {
      CIccSampledCurveSegmentJson js(static_cast<CIccSampledCurveSegment*>(pSeg));
      if (!js.ToJson(jSeg)) return false;
    }
    else {
      return false;
    }
    segs.push_back(jSeg);
  }
  j["segments"] = segs;
  return true;
}

bool CIccSegmentedCurveJson::ParseJson(const IccJson &j, std::string &parseStr)
{
  m_list->clear();

  if (!jsonExistsField(j, "segments") || !j["segments"].is_array())
    return true;

  for (const IccJson &jSeg : j["segments"]) {
    std::string type;
    jGetString(jSeg, "type",  type);
    // start/end may be strings ("-infinity"/"+infinity") or numbers
    icFloatNumber start = jSeg.contains("start") ? icJsonSegPosFromJson(jSeg["start"]) : 0.0f;
    icFloatNumber end   = jSeg.contains("end")   ? icJsonSegPosFromJson(jSeg["end"])   : 0.0f;

    if (type == "FormulaSegment") {
      CIccFormulaCurveSegmentJson *pSeg = new(std::nothrow) CIccFormulaCurveSegmentJson(start, end);
      if (!pSeg) { parseStr += "Allocation failed\n"; return false; }
      if (!pSeg->ParseJson(jSeg, parseStr)) { delete pSeg; return false; }
      m_list->push_back(pSeg);
    }
    else if (type == "SampledSegment") {
      CIccSampledCurveSegmentJson *pSeg = new(std::nothrow) CIccSampledCurveSegmentJson(start, end);
      if (!pSeg) { parseStr += "Allocation failed\n"; return false; }
      if (!pSeg->ParseJson(jSeg, parseStr)) { delete pSeg; return false; }
      m_list->push_back(pSeg);
    }
    else {
      parseStr += "Unknown segment type: " + type + "\n";
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Local JSON-aware derived curve classes (need access to protected members)
// ---------------------------------------------------------------------------

class CIccSingleSampledCurveJson : public CIccSingleSampledCurve
{
public:
  CIccSingleSampledCurveJson(icFloatNumber first = 0.0f, icFloatNumber last = 1.0f)
    : CIccSingleSampledCurve(first, last) {}
  CIccSingleSampledCurveJson(const CIccSingleSampledCurve *p)
    : CIccSingleSampledCurve(*p) {}

  bool ToJson(IccJson &j) const {
    j["type"]          = "SingleSampledCurve";
    j["firstEntry"]    = (double)m_firstEntry;
    j["lastEntry"]     = (double)m_lastEntry;
    j["storageType"]   = (int)m_storageType;
    j["extensionType"] = (int)m_extensionType;
    if (m_nReserved)
      j["reserved"] = (int)m_nReserved;
    IccJson samples = IccJson::array();
    for (icUInt32Number i = 0; i < m_nCount; i++)
      samples.push_back((double)m_pSamples[i]);
    j["samples"] = samples;
    return true;
  }

  bool ParseJson(const IccJson &j, std::string &parseStr) {
    icFloatNumber first = j.contains("firstEntry") ? (icFloatNumber)j["firstEntry"].get<double>() : 0.0f;
    icFloatNumber last  = j.contains("lastEntry")  ? (icFloatNumber)j["lastEntry"].get<double>()  : 1.0f;
    SetRange(first, last);

    int storageType   = (int)icValueTypeFloat32;
    int extensionType = (int)icClipSingleSampledCurve;
    int reserved      = 0;
    jGetValue(j, "storageType",   storageType);
    jGetValue(j, "extensionType", extensionType);
    jGetValue(j, "reserved",      reserved);
    SetStorageType((icUInt16Number)storageType);
    SetExtensionType((icUInt16Number)extensionType);
    m_nReserved = (icUInt32Number)reserved;

    if (!j.contains("samples") || !j["samples"].is_array()) {
      parseStr += "Missing samples in SingleSampledCurve\n";
      return false;
    }
    const IccJson &arr = j["samples"];
    if (!SetSize(icJsonSafeU32(arr.size())))
      return false;
    for (icUInt32Number i = 0; i < icJsonSafeU32(arr.size()); i++)
      m_pSamples[i] = (icFloatNumber)arr[i].get<double>();
    return true;
  }
};


class CIccSampledCalculatorCurveJson : public CIccSampledCalculatorCurve
{
public:
  CIccSampledCalculatorCurveJson(icFloatNumber first = 0.0f, icFloatNumber last = 1.0f)
    : CIccSampledCalculatorCurve(first, last) {}
  CIccSampledCalculatorCurveJson(const CIccSampledCalculatorCurve *p)
    : CIccSampledCalculatorCurve(*p) {}

  bool ToJson(IccJson &j) const {
    j["type"]          = "SampledCalculatorCurve";
    j["firstEntry"]    = (double)m_firstEntry;
    j["lastEntry"]     = (double)m_lastEntry;
    j["extensionType"] = (int)m_extensionType;
    j["desiredSize"]   = (int)m_nDesiredSize;
    if (m_nReserved2)
      j["reserved2"]   = (int)m_nReserved2;
    if (m_pCalc) {
      IIccExtensionMpe *pExt = m_pCalc->GetExtension();
      if (pExt && !strcmp(pExt->GetExtClassName(), "CIccMpeJson")) {
        IccJson jCalc;
        jCalc["inputChannels"]  = (int)m_pCalc->NumInputChannels();
        jCalc["outputChannels"] = (int)m_pCalc->NumOutputChannels();
        static_cast<CIccMpeJson*>(pExt)->ToJson(jCalc);
        j["calculator"] = jCalc;
      }
      else {
        return false;
      }
    }
    return true;
  }

  bool ParseJson(const IccJson &j, std::string &parseStr) {
    icFloatNumber first = j.contains("firstEntry") ? (icFloatNumber)j["firstEntry"].get<double>() : 0.0f;
    icFloatNumber last  = j.contains("lastEntry")  ? (icFloatNumber)j["lastEntry"].get<double>()  : 1.0f;
    SetRange(first, last);

    int extensionType = 0, desiredSize = 256, reserved2 = 0;
    jGetValue(j, "extensionType", extensionType);
    jGetValue(j, "desiredSize",   desiredSize);
    jGetValue(j, "reserved2",     reserved2);
    SetExtensionType((icUInt16Number)extensionType);
    SetRecommendedSize((icUInt32Number)desiredSize);
    m_nReserved2 = (icUInt16Number)reserved2;

    if (j.contains("calculator") && j["calculator"].is_object()) {
      CIccMpeJsonCalculator *pCalc = new(std::nothrow) CIccMpeJsonCalculator();
      if (!pCalc) { parseStr += "Allocation failed\n"; return false; }
      if (!pCalc->ParseJson(j["calculator"], parseStr)) {
        delete pCalc;
        return false;
      }
      if (!SetCalculator(pCalc)) {
        delete pCalc;
        parseStr += "SetCalculator failed in SampledCalculatorCurve\n";
        return false;
      }
    }
    return true;
  }
};


// Dispatch a single CurveSet curve to JSON
static bool icToJsonCurve(IccJson &j, icCurveSetCurvePtr pCurve)
{
  if (!pCurve)
    return false;

  if (pCurve->GetType() == icSigSegmentedCurve) {
    j["type"] = "SegmentedCurve";
    CIccSegmentedCurveJson scJson(static_cast<CIccSegmentedCurve *>(pCurve));
    return scJson.ToJson(j);
  }
  else if (pCurve->GetType() == icSigSingleSampledCurve) {
    CIccSingleSampledCurveJson sscJson(static_cast<CIccSingleSampledCurve *>(pCurve));
    return sscJson.ToJson(j);
  }
  else if (pCurve->GetType() == icSigSampledCalculatorCurve) {
    CIccSampledCalculatorCurveJson sccJson(static_cast<CIccSampledCalculatorCurve *>(pCurve));
    return sccJson.ToJson(j);
  }
  return false;
}

// Create a CurveSet curve from a JSON object
static icCurveSetCurvePtr icFromJsonCurve(const IccJson &jCurve, std::string &parseStr)
{
  std::string type;
  jGetString(jCurve, "type", type);

  if (type == "SegmentedCurve") {
    CIccSegmentedCurveJson *pCurve = new(std::nothrow) CIccSegmentedCurveJson();
    if (!pCurve) return nullptr;
    if (pCurve->ParseJson(jCurve, parseStr))
      return pCurve;
    delete pCurve;
  }
  else if (type == "SingleSampledCurve") {
    CIccSingleSampledCurveJson *pCurve = new(std::nothrow) CIccSingleSampledCurveJson();
    if (!pCurve) return nullptr;
    if (pCurve->ParseJson(jCurve, parseStr))
      return pCurve;
    delete pCurve;
  }
  else if (type == "SampledCalculatorCurve") {
    CIccSampledCalculatorCurveJson *pCurve = new(std::nothrow) CIccSampledCalculatorCurveJson();
    if (!pCurve) return nullptr;
    if (pCurve->ParseJson(jCurve, parseStr))
      return pCurve;
    delete pCurve;
  }
  else {
    parseStr += "Unknown curve type: " + type + "\n";
  }
  return nullptr;
}


// ===========================================================================
// CIccMpeJsonCurveSet
// ===========================================================================

bool CIccMpeJsonCurveSet::ToJson(IccJson &j)
{
  int nChannels = NumInputChannels();
  j["inputChannels"]  = nChannels;
  j["outputChannels"] = NumOutputChannels();

  IccJson curves = IccJson::array();
  for (int i = 0; i < nChannels; i++) {
    // Check for duplicate (shared pointer with an earlier slot)
    int dupIdx = -1;
    for (int k = 0; k < i; k++) {
      if (m_curve[i] == m_curve[k]) { dupIdx = k; break; }
    }

    IccJson jCurve;
    if (dupIdx >= 0) {
      jCurve["type"]  = "DuplicateCurve";
      jCurve["index"] = dupIdx;
    }
    else if (!icToJsonCurve(jCurve, m_curve[i])) {
      return false;
    }
    curves.push_back(jCurve);
  }
  j["curves"] = curves;
  return true;
}

bool CIccMpeJsonCurveSet::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);

  if (!nIn || nIn != nOut) {
    parseStr += "Invalid inputChannels or outputChannels in CurveSetElement\n";
    return false;
  }

  icUInt16Number nChannels = (icUInt16Number)nIn;
  SetSize(nChannels);

  if (!j.contains("curves") || !j["curves"].is_array()) {
    parseStr += "Missing curves array in CurveSetElement\n";
    return false;
  }

  const IccJson &jCurves = j["curves"];
  int nIndex = 0;
  for (const IccJson &jCurve : jCurves) {
    if (nIndex >= nChannels) {
      parseStr += "Too many curves in CurveSetElement\n";
      return false;
    }

    std::string type;
    jGetString(jCurve, "type", type);

    if (type == "DuplicateCurve") {
      int dupIdx = -1;
      jGetValue(jCurve, "index", dupIdx);
      if (dupIdx < 0 || dupIdx >= nIndex) {
        parseStr += "Invalid index for DuplicateCurve\n";
        return false;
      }
      m_curve[nIndex] = m_curve[dupIdx];
    }
    else {
      icCurveSetCurvePtr pCurve = icFromJsonCurve(jCurve, parseStr);
      if (!pCurve)
        return false;
      if (!SetCurve(nIndex, pCurve))
        return false;
    }
    nIndex++;
  }

  return (nIndex == nChannels);
}

// ===========================================================================
// CIccMpeJsonTintArray
// ===========================================================================

bool CIccMpeJsonTintArray::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;

  if (m_Array) {
    const icChar *typeName = CIccTagCreator::GetTagTypeSigName(m_Array->GetType());
    if (typeName) {
      j["arrayType"] = typeName;
    }
    else {
      char sigBuf[32];
      icGetSigStr(sigBuf, sizeof(sigBuf), m_Array->GetType());
      j["arrayType"] = sigBuf;
    }

    IIccExtensionTag *pExt = m_Array->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0)
      return false;
    if (!static_cast<CIccTagJson*>(pExt)->ToJson(j))
      return false;
  }
  return true;
}

bool CIccMpeJsonTintArray::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  m_nInputChannels  = (icUInt16Number)nIn;
  m_nOutputChannels = (icUInt16Number)nOut;

  // Determine array element type (default: float32)
  std::string arrayTypeName;
  jGetString(j, "arrayType", arrayTypeName);
  icTagTypeSignature typeSig = arrayTypeName.empty()
    ? icSigFloat32ArrayType
    : CIccTagCreator::GetTagTypeNameSig(arrayTypeName.c_str());
  if (typeSig == icSigUnknownType)
    typeSig = icSigFloat32ArrayType;

  // Create JSON-aware tag for the array data
  CIccTag *pTag = CIccTagCreator::CreateTag(typeSig);
  if (!pTag || !pTag->IsNumArrayType()) {
    parseStr += "Invalid arrayType for TintArrayElement\n";
    delete pTag;
    return false;
  }
  IIccExtensionTag *pExt = pTag->GetExtension();
  if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0) {
    parseStr += "arrayType does not support JSON for TintArrayElement\n";
    delete pTag;
    return false;
  }
  CIccTagJson *pTagJson = static_cast<CIccTagJson*>(pExt);

  // External file
  if (j.contains("file") && j["file"].is_string()) {
    std::string filename = j["file"].get<std::string>();
    std::string format   = "text";
    jGetString(j, "format", format);

    CIccIO *pIO = IccJsonOpenFileIO(filename.c_str(), "rb");
    if (!pIO) {
      parseStr += "Cannot open TintArray file: " + filename + "\n";
      delete pTag;
      return false;
    }

    if (format == "text") {
      size_t fileLen = pIO->GetLength();
      char *buf = new(std::nothrow) char[fileLen + 1];
      if (!buf) { delete pIO; parseStr += "Out of memory\n"; delete pTag; return false; }
      icUInt32Number nRead = pIO->Read8(buf, (icUInt32Number)fileLen);
      buf[nRead] = '\0';
      delete pIO;

      IccJson jArr = IccJson::array();
      const char *p   = buf;
      const char *end = buf + nRead;
      while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ','))
          p++;
        if (p >= end) break;
        char *next = nullptr;
        double v = strtod(p, &next);
        if (!next || next == p) break;
        jArr.push_back(v);
        p = next;
      }
      delete[] buf;

      IccJson jValues;
      jValues["values"] = jArr;
      if (!pTagJson->ParseJson(jValues, parseStr)) { delete pTag; return false; }
    }
    else if (format == "binary") {
      size_t fileLen       = pIO->GetLength();
      icUInt32Number nVals = (icUInt32Number)(fileLen / sizeof(icFloat32Number));
      IccJson jArr = IccJson::array();
      for (icUInt32Number i = 0; i < nVals; i++) {
        icFloat32Number v = 0.0f;
        if (!pIO->ReadFloat32Float(&v)) break;
        jArr.push_back((double)v);
      }
      delete pIO;

      IccJson jValues;
      jValues["values"] = jArr;
      if (!pTagJson->ParseJson(jValues, parseStr)) { delete pTag; return false; }
    }
    else {
      delete pIO;
      parseStr += "Unsupported format '" + format + "' for TintArrayElement file\n";
      delete pTag;
      return false;
    }
  }
  else {
    // Inline: the tag's ParseJson reads "values" from j
    if (!pTagJson->ParseJson(j, parseStr)) { delete pTag; return false; }
  }

  SetArray(static_cast<CIccTagNumArray*>(pTag));
  return true;
}

// ===========================================================================
// CIccJsonToneMapFunc
// ===========================================================================

CIccToneMapFunc* CIccJsonToneMapFunc::NewCopy() const
{
  return new CIccJsonToneMapFunc(*this);
}

bool CIccJsonToneMapFunc::ToJson(IccJson &j)
{
  j["functionType"] = (int)m_nFunctionType;
  if (m_nReserved2)
    j["reserved2"] = (int)m_nReserved2;
  IccJson params = IccJson::array();
  for (icUInt8Number i = 0; i < m_nParameters; i++)
    params.push_back((double)m_params[i]);
  j["parameters"] = params;
  return true;
}

bool CIccJsonToneMapFunc::ParseJson(const IccJson &j, std::string &parseStr)
{
  int funcType = 0, reserved2 = 0;
  jGetValue(j, "functionType", funcType);
  jGetValue(j, "reserved2",    reserved2);
  m_nFunctionType = (icUInt16Number)funcType;
  m_nReserved2    = (icUInt16Number)reserved2;

  switch (m_nFunctionType) {
    case 0: m_nParameters = 3; break;
    default:
      parseStr += "Unsupported functionType in ToneMapFunction\n";
      return false;
  }

  if (m_params) { free(m_params); m_params = nullptr; }
  m_params = (icFloatNumber*)malloc(m_nParameters * sizeof(icFloatNumber));
  if (!m_params) return false;
  for (int i = 0; i < m_nParameters; i++) m_params[i] = 0.0f;

  if (j.contains("parameters") && j["parameters"].is_array()) {
    int cnt = std::min((int)m_nParameters, (int)j["parameters"].size());
    for (int i = 0; i < cnt; i++)
      m_params[i] = (icFloatNumber)j["parameters"][i].get<double>();
  }
  return true;
}

// ===========================================================================
// CIccMpeJsonToneMap
// ===========================================================================

bool CIccMpeJsonToneMap::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;

  // Luminance curve
  if (m_pLumCurve) {
    IccJson jLum;
    if (!icToJsonCurve(jLum, m_pLumCurve))
      return false;
    j["luminanceCurve"] = jLum;
  }

  // Tone map functions (with duplicate detection)
  IccJson funcs = IccJson::array();
  for (int i = 0; i < (int)m_nOutputChannels; i++) {
    int dupIdx = -1;
    for (int k = 0; k < i; k++) {
      if (m_pToneFuncs[i] == m_pToneFuncs[k]) { dupIdx = k; break; }
    }

    IccJson jFunc;
    if (dupIdx >= 0) {
      jFunc["type"]  = "DuplicateFunction";
      jFunc["index"] = dupIdx;
    }
    else {
      if (!m_pToneFuncs[i] || strcmp(m_pToneFuncs[i]->GetClassName(), "CIccJsonToneMapFunc") != 0)
        return false;
      if (!static_cast<CIccJsonToneMapFunc*>(m_pToneFuncs[i])->ToJson(jFunc))
        return false;
    }
    funcs.push_back(jFunc);
  }
  j["toneMapFunctions"] = funcs;
  return true;
}

bool CIccMpeJsonToneMap::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);

  if (!nIn || !nOut || nIn != nOut + 1) {
    parseStr += "Invalid inputChannels or outputChannels in ToneMapElement\n";
    return false;
  }
  m_nInputChannels = (icUInt16Number)nIn;
  SetNumOutputChannels((icUInt16Number)nOut);

  if (!m_pToneFuncs) {
    parseStr += "Unable to allocate ToneMapFunctions\n";
    return false;
  }

  // Luminance curve
  if (!j.contains("luminanceCurve") || !j["luminanceCurve"].is_object()) {
    parseStr += "Missing luminanceCurve in ToneMapElement\n";
    return false;
  }
  icCurveSetCurvePtr pLumCurve = icFromJsonCurve(j["luminanceCurve"], parseStr);
  if (!pLumCurve) {
    parseStr += "Unable to parse luminanceCurve in ToneMapElement\n";
    return false;
  }
  SetLumCurve(pLumCurve);

  // Tone map functions
  if (!j.contains("toneMapFunctions") || !j["toneMapFunctions"].is_array()) {
    parseStr += "Missing toneMapFunctions in ToneMapElement\n";
    return false;
  }
  const IccJson &jFuncs = j["toneMapFunctions"];
  int nIndex = 0;
  for (const IccJson &jFunc : jFuncs) {
    if (nIndex >= nOut) {
      parseStr += "Too many toneMapFunctions in ToneMapElement\n";
      return false;
    }

    std::string type;
    jGetString(jFunc, "type", type);

    if (type == "DuplicateFunction") {
      int dupIdx = -1;
      jGetValue(jFunc, "index", dupIdx);
      if (dupIdx < 0 || dupIdx >= nIndex) {
        parseStr += "Invalid index for DuplicateFunction in ToneMapElement\n";
        return false;
      }
      if (m_nFunc >= (icUInt16Number)nOut) {
        parseStr += "Too many ToneMapFunctions\n";
        return false;
      }
      m_pToneFuncs[m_nFunc++] = m_pToneFuncs[dupIdx];
    }
    else {
      CIccJsonToneMapFunc *pFunc = new(std::nothrow) CIccJsonToneMapFunc();
      if (!pFunc) { parseStr += "Allocation failed\n"; return false; }
      if (!pFunc->ParseJson(jFunc, parseStr)) { delete pFunc; return false; }
      if (!Insert(pFunc)) { delete pFunc; return false; }
    }
    nIndex++;
  }

  return (nIndex == nOut);
}

// ===========================================================================
// CIccMpeJsonMatrix
// ===========================================================================

bool CIccMpeJsonMatrix::ToJson(IccJson &j)
{
  if (m_pMatrix) {
    IccJson matrix = IccJson::array();
    icUInt64Number nEntries64 = (icUInt64Number)m_nInputChannels * m_nOutputChannels;
    if (nEntries64 > 0xFFFFFFFFUL) return false;
    icUInt32Number nEntries = (icUInt32Number)nEntries64;
    for (icUInt32Number i = 0; i < nEntries; i++)
      matrix.push_back((double)m_pMatrix[i]);
    j["matrix"] = matrix;
  }
  if (m_pConstants && m_bApplyConstants) {
    IccJson constants = IccJson::array();
    for (icUInt16Number i = 0; i < m_nOutputChannels; i++)
      constants.push_back((double)m_pConstants[i]);
    j["constants"] = constants;
  }
  return true;
}

bool CIccMpeJsonMatrix::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nInInt = 0, nOutInt = 0;
  jGetValue(j, "inputChannels",  nInInt);
  jGetValue(j, "outputChannels", nOutInt);
  icUInt16Number nIn  = (icUInt16Number)nInInt;
  icUInt16Number nOut = (icUInt16Number)nOutInt;

  bool bHasConstants = j.contains("constants") && j["constants"].is_array();
  if (!SetSize(nIn, nOut, bHasConstants)) return false;

  if (j.contains("matrix") && j["matrix"].is_array()) {
    const IccJson &mat = j["matrix"];
    icUInt32Number nEntries = nIn * nOut;
    for (icUInt32Number i = 0; i < nEntries && i < icJsonSafeU32(mat.size()); i++)
      m_pMatrix[i] = (icFloatNumber)mat[i].get<double>();

    bool bInvert = j.contains("invertMatrix") && j["invertMatrix"].is_boolean()
                   ? j["invertMatrix"].get<bool>() : false;
    if (bInvert) {
      if (nIn != nOut) {
        parseStr += "Inversion of matrix requires square matrix\n";
        return false;
      }
      IIccMatrixInverter *pInverter = IccGetDefaultMatrixInverter();
      if (!pInverter || !pInverter->Invert(m_pMatrix, nOut, nIn)) {
        parseStr += "Unable to invert matrix!\n";
        return false;
      }
    }
  }
  if (bHasConstants) {
    const IccJson &con = j["constants"];
    for (icUInt16Number i = 0; i < nOut && i < con.size(); i++)
      m_pConstants[i] = (icFloatNumber)con[i].get<double>();
  }
  return true;
}

// ===========================================================================
// CIccMpeJsonCLUT
// ===========================================================================

bool CIccMpeJsonCLUT::ToJson(IccJson &j)
{
  if (m_pCLUT)
    icCLUTToJson(j, m_pCLUT, icConvertFloat, true, "clut");
  return true;
}

bool CIccMpeJsonCLUT::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);

  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in CLutElement\n";
    return false;
  }
  m_nInputChannels  = (icUInt16Number)nIn;
  m_nOutputChannels = (icUInt16Number)nOut;

  if (!j.contains("clut") || !j["clut"].is_object()) {
    parseStr += "Missing clut data in CLutElement\n";
    return false;
  }

  CIccCLUT *pCLUT = icCLUTFromJson(j["clut"], nIn, nOut, icConvertFloat, parseStr);
  if (!pCLUT) return false;

  SetCLUT(pCLUT);
  return m_pCLUT != nullptr;
}

// ===========================================================================
// CIccMpeJsonExtCLUT
// ===========================================================================

bool CIccMpeJsonExtCLUT::ToJson(IccJson &j)
{
  j["storageType"] = (int)m_storageType;
  if (m_nReserved2)
    j["reserved2"] = (int)m_nReserved2;
  if (m_pCLUT)
    icCLUTToJson(j, m_pCLUT, icConvertFloat, true, "clut");
  return true;
}

bool CIccMpeJsonExtCLUT::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0, storageType = 0, reserved2 = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  jGetValue(j, "storageType",    storageType);
  jGetValue(j, "reserved2",      reserved2);

  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in ExtCLutElement\n";
    return false;
  }
  m_nInputChannels  = (icUInt16Number)nIn;
  m_nOutputChannels = (icUInt16Number)nOut;
  m_storageType     = (icUInt16Number)storageType;
  m_nReserved2      = (icUInt16Number)reserved2;

  if (!j.contains("clut") || !j["clut"].is_object()) {
    parseStr += "Missing clut data in ExtCLutElement\n";
    return false;
  }

  CIccCLUT *pCLUT = icCLUTFromJson(j["clut"], nIn, nOut, icConvertFloat, parseStr);
  if (!pCLUT) return false;

  SetCLUT(pCLUT);
  return m_pCLUT != nullptr;
}

// ===========================================================================
// CIccMpeJsonBAcs / EAcs
// ===========================================================================

bool CIccMpeJsonBAcs::ToJson(IccJson &j)
{
  char buf[32]; icGetSigStr(buf, sizeof(buf), m_signature);
  j["signature"] = buf;
  j["data"] = icJsonDumpHexData(m_pData, m_nDataSize);
  return true;
}

bool CIccMpeJsonBAcs::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string sig, hex;
  jGetString(j, "signature", sig);
  jGetString(j, "data", hex);
  if (!sig.empty())
    m_signature = (icAcsSignature)icGetSigVal(sig.c_str());
  m_nDataSize = icJsonGetHexDataSize(hex.c_str());
  if (m_pData) { free(m_pData); m_pData = nullptr; }
  if (m_nDataSize) {
    m_pData = (icUInt8Number*)malloc(m_nDataSize);
    if (!m_pData) { m_nDataSize = 0; return false; }
    icJsonGetHexData(m_pData, hex.c_str(), m_nDataSize);
  }
  return true;
}

bool CIccMpeJsonEAcs::ToJson(IccJson &j)
{
  char buf[32]; icGetSigStr(buf, sizeof(buf), m_signature);
  j["signature"] = buf;
  j["data"] = icJsonDumpHexData(m_pData, m_nDataSize);
  return true;
}

bool CIccMpeJsonEAcs::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string sig, hex;
  jGetString(j, "signature", sig);
  jGetString(j, "data", hex);
  if (!sig.empty())
    m_signature = (icAcsSignature)icGetSigVal(sig.c_str());
  m_nDataSize = icJsonGetHexDataSize(hex.c_str());
  if (m_pData) { free(m_pData); m_pData = nullptr; }
  if (m_nDataSize) {
    m_pData = (icUInt8Number*)malloc(m_nDataSize);
    if (!m_pData) { m_nDataSize = 0; return false; }
    icJsonGetHexData(m_pData, hex.c_str(), m_nDataSize);
  }
  return true;
}

// ===========================================================================
// CIccMpeJsonJabToXYZ / XYZToJab  (shared CAM parameter helpers)
// ===========================================================================

static bool icCAMParamsToJson(IccJson &j, CIccCamConverter *pCAM)
{
  if (!pCAM)
    return false;

  IccJson jCAM;
  icFloatNumber xyz[3];
  pCAM->GetParameter_WhitePoint(xyz);
  jCAM["whitePoint"]               = IccJson::array({ (double)xyz[0], (double)xyz[1], (double)xyz[2] });
  jCAM["luminance"]                = (double)pCAM->GetParameter_La();
  jCAM["backgroundLuminance"]      = (double)pCAM->GetParameter_Yb();
  jCAM["impactSurround"]           = (double)pCAM->GetParameter_C();
  jCAM["chromaticInductionFactor"] = (double)pCAM->GetParameter_Nc();
  jCAM["adaptationFactor"]         = (double)pCAM->GetParameter_F();
  j["colorAppearanceParams"] = jCAM;
  return true;
}

static bool icCAMParamsFromJson(const IccJson &j, std::string &parseStr, CIccCamConverter *pCAM)
{
  if (!j.contains("colorAppearanceParams") || !j["colorAppearanceParams"].is_object()) {
    parseStr += "Missing colorAppearanceParams in Jab element\n";
    return false;
  }
  const IccJson &jCAM = j["colorAppearanceParams"];

  if (!jCAM.contains("whitePoint") || !jCAM["whitePoint"].is_array() || jCAM["whitePoint"].size() < 3) {
    parseStr += "Invalid whitePoint in colorAppearanceParams\n";
    return false;
  }
  icFloatNumber xyz[3];
  xyz[0] = (icFloatNumber)jCAM["whitePoint"][0].get<double>();
  xyz[1] = (icFloatNumber)jCAM["whitePoint"][1].get<double>();
  xyz[2] = (icFloatNumber)jCAM["whitePoint"][2].get<double>();
  pCAM->SetParameter_WhitePoint(xyz);

  if (!jCAM.contains("luminance")) {
    parseStr += "Missing luminance in colorAppearanceParams\n";
    return false;
  }
  pCAM->SetParameter_La((icFloatNumber)jCAM["luminance"].get<double>());

  if (!jCAM.contains("backgroundLuminance")) {
    parseStr += "Missing backgroundLuminance in colorAppearanceParams\n";
    return false;
  }
  pCAM->SetParameter_Yb((icFloatNumber)jCAM["backgroundLuminance"].get<double>());

  if (!jCAM.contains("impactSurround")) {
    parseStr += "Missing impactSurround in colorAppearanceParams\n";
    return false;
  }
  pCAM->SetParameter_C((icFloatNumber)jCAM["impactSurround"].get<double>());

  if (!jCAM.contains("chromaticInductionFactor")) {
    parseStr += "Missing chromaticInductionFactor in colorAppearanceParams\n";
    return false;
  }
  pCAM->SetParameter_Nc((icFloatNumber)jCAM["chromaticInductionFactor"].get<double>());

  if (!jCAM.contains("adaptationFactor")) {
    parseStr += "Missing adaptationFactor in colorAppearanceParams\n";
    return false;
  }
  pCAM->SetParameter_F((icFloatNumber)jCAM["adaptationFactor"].get<double>());

  return true;
}

bool CIccMpeJsonJabToXYZ::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;
  return icCAMParamsToJson(j, m_pCAM);
}

bool CIccMpeJsonJabToXYZ::ParseJson(const IccJson &j, std::string &parseStr)
{
  m_nInputChannels = m_nOutputChannels = 3;

  CIccCamConverter *pCAM = new(std::nothrow) CIccCamConverter();
  if (!pCAM) return false;
  if (!icCAMParamsFromJson(j, parseStr, pCAM)) {
    delete pCAM;
    return false;
  }
  SetCAM(pCAM);
  return true;
}

bool CIccMpeJsonXYZToJab::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;
  return icCAMParamsToJson(j, m_pCAM);
}

bool CIccMpeJsonXYZToJab::ParseJson(const IccJson &j, std::string &parseStr)
{
  m_nInputChannels = m_nOutputChannels = 3;

  CIccCamConverter *pCAM = new(std::nothrow) CIccCamConverter();
  if (!pCAM) return false;
  if (!icCAMParamsFromJson(j, parseStr, pCAM)) {
    delete pCAM;
    return false;
  }
  SetCAM(pCAM);
  return true;
}

// ===========================================================================
// CIccMpeJsonCalculator
// ===========================================================================

void CIccMpeJsonCalculator::clean()
{
  m_sImport = "*";
  m_declVarMap.clear();
  m_varMap.clear();
  m_macroMap.clear();
  m_macroLocalMap.clear();

  for (auto &mp : m_mpeList) {
    if (mp.m_ptr) { delete mp.m_ptr; mp.m_ptr = nullptr; }
  }
  m_mpeList.clear();

  for (auto &kv : m_mpeMap) {
    if (kv.second.m_ptr) { delete kv.second.m_ptr; kv.second.m_ptr = nullptr; }
  }
  m_mpeMap.clear();

  m_nNextVar = 0;
  m_nNextMpe = 0;
}

bool CIccMpeJsonCalculator::validNameChar(char c, bool bFirst)
{
  if (bFirst && !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'))
    return false;
  if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '_'))
    return false;
  return true;
}

bool CIccMpeJsonCalculator::validName(const char *szName)
{
  if (!szName || !*szName)
    return false;
  for (const char *p = szName; *p; p++) {
    if (!validNameChar(*p, p == szName))
      return false;
  }
  return true;
}

bool CIccMpeJsonCalculator::ParseChanMap(ChanVarMap &chanMap, const char *szNames, int nChannels)
{
  chanMap.clear();
  if (!szNames)
    return false;

  int i = 0, size = 1;
  std::string name;
  IndexSizePair isp;

  for (const char *p = szNames; *p && i < nChannels; p++) {
    bool bFirst = name.empty();
    if (*p == ' ') {
      if (!bFirst) {
        isp.first = i; isp.second = size;
        chanMap[name] = isp;
        name.clear(); i += size; size = 1;
      }
    }
    else if (*p == '[' || *p == '(') {
      size = atoi(p + 1);
      if (size < 0 || i + size > nChannels) return false;
      for (; *p && *p != ']' && *p != ')'; p++);
      if (!*p) p--;
    }
    else if (validNameChar(*p, bFirst)) {
      name += *p;
    }
    else {
      return false;
    }
  }
  if (!name.empty() && i < nChannels) {
    isp.first = i; isp.second = size;
    chanMap[name] = isp;
  }
  return true;
}

bool CIccMpeJsonCalculator::ValidMacroCalls(const char *szMacroText,
                                             std::string macroStack,
                                             std::string &parseStr) const
{
  // Macros can be called as call{name} or #name
  auto FindNextMacro = [](const char *text) -> const char* {
    const char *a = strstr(text, "call{");
    const char *b = strchr(text, '#');
    if (b && (!a || b < a)) return b;
    return a;
  };

  for (const char *ptr = FindNextMacro(szMacroText); ptr; ptr = FindNextMacro(ptr)) {
    bool isHash = (ptr[0] == '#');
    if (isHash) ptr++;
    CIccFuncTokenizer parse(ptr, true);
    parse.GetNext();
    std::string name = isHash ? parse.GetName() : parse.GetReference();

    MacroMap::const_iterator m = m_macroMap.find(name);
    if (m == m_macroMap.end()) {
      parseStr += "Call to undefined macro '" + name + "'\n";
      return false;
    }
    std::string sm = "*" + name + "*";
    if (strstr(macroStack.c_str(), sm.c_str())) {
      parseStr += "Macro recursion detected in call to '" + name + "'\n";
      return false;
    }
    if (!ValidMacroCalls(m->second.c_str(), macroStack + name + "*", parseStr))
      return false;
    ptr++;
  }
  return true;
}

bool CIccMpeJsonCalculator::ValidateMacroCalls(std::string &parseStr) const
{
  for (auto &kv : m_macroMap) {
    if (!ValidMacroCalls(kv.second.c_str(), std::string("*") + kv.first + "*", parseStr))
      return false;
  }
  return true;
}

bool CIccMpeJsonCalculator::Flatten(std::string &flatStr, std::string macroName,
                                     const char *szFunc, std::string &parseStr,
                                     icUInt32Number nLocalsOffset,
                                     icUInt32Number nDepth)
{
  // CWE-674: hard cap on macro-call recursion depth (defense in depth;
  // ValidateMacroCalls catches direct cycles, this catches deep non-cyclic
  // chains and any path that bypasses validation).
  if (nDepth >= kMaxFlattenDepth) {
    parseStr += "Macro recursion depth limit exceeded in '" + macroName + "'\n";
    return false;
  }

  CIccFuncTokenizer parse(szFunc, true);

  while (parse.GetNext()) {
    std::string token = parse.GetLast();
    const char *tok = token.c_str();

    // -- Macro call --------------------------------------------------------
    if (!strncmp(tok, "call{", 5) || tok[0] == '#') {
      std::string nameIter, name;
      if (tok[0] == '#')
        nameIter = token.substr(1);
      else
        nameIter = parse.GetReference();

      for (const char *p = nameIter.c_str(); *p && *p != '[' && *p != '('; p++)
        name += *p;

      MacroMap::iterator m = m_macroMap.find(name);
      if (m == m_macroMap.end()) {
        parseStr += "Call to undefined macro '" + name + "'\n";
        return false;
      }
      icUInt16Number nLocalsSize = 0;
      TempDeclVarMap::iterator locals = m_macroLocalMap.find(macroName);
      if (locals != m_macroLocalMap.end())
        nLocalsSize = locals->second.m_size;

      if (!Flatten(flatStr, name, m->second.c_str(), parseStr, nLocalsOffset + nLocalsSize, nDepth + 1))
        return false;
    }

    // -- Input/output channel references -----------------------------------
    else if (!strncmp(tok, "in{", 3) || !strncmp(tok, "out{", 4)) {
      std::string op  = parse.GetName();
      std::string ref = parse.GetReference();
      std::string refroot;
      for (const char *p = ref.c_str(); *p && *p != ',' && *p != '(' && *p != '['; p++)
        refroot += *p;

      ChanVarMap *pMap = (op == "in") ? &m_inputMap : &m_outputMap;
      ChanVarMap::iterator ci = pMap->find(refroot);
      if (ci == pMap->end()) {
        parseStr += "Invalid '" + op + "' channel reference '" + refroot + "'\n";
        return false;
      }

      char index[80];
      if (refroot != ref) {
        std::string select = ref.substr(refroot.size());
        int offset = 0, sz = 1;
        if (select[0] == '[' || select[0] == '(') {
          const char *p;
          offset = atoi(select.c_str() + 1);
          for (p = select.c_str() + 1; *p && *p != ')' && *p != ']'; p++);
          select = p + 1;
        }
        if (select[0] == ',') sz = atoi(select.c_str() + 1);
        if (sz < 0 || offset < 0 ||
            ci->second.first + offset + sz > (int)m_nInputChannels) {
          parseStr += "Invalid '" + op + "' channel offset/size '" + refroot + "'\n";
          return false;
        }
        snprintf(index, sizeof(index), "(%d,%d)", ci->second.first + offset, sz);
      }
      else if (ci->second.second > 1) {
        snprintf(index, sizeof(index), "(%d,%d)", ci->second.first, ci->second.second);
      }
      else {
        snprintf(index, sizeof(index), "(%d)", ci->second.first);
      }
      flatStr += op + index + " ";
    }

    // -- Temporary variable operations -------------------------------------
    else if (!strncmp(tok, "tget{", 5) || !strncmp(tok, "tput{", 5) ||
             !strncmp(tok, "tsav{", 5)) {
      std::string op  = parse.GetName();
      std::string ref = parse.GetReference();
      std::string refroot;
      for (const char *p = ref.c_str(); *p && *p != '[' && *p != '('; p++)
        refroot += *p;

      // Macro local variables (prefixed with @)
      if (!macroName.empty() && !refroot.empty() && refroot[0] == '@') {
        std::string localName = refroot.substr(1);
        TempDeclVarMap::iterator locals = m_macroLocalMap.find(macroName);
        if (locals == m_macroLocalMap.end()) {
          parseStr += "Reference to undeclared local '" + localName + "' in macro '" + macroName + "'\n";
          return false;
        }
        bool localFound = false;
        unsigned long nLocalOffset = 0, nLocalSize = 0;
        for (auto &m : locals->second.m_members) {
          if (m.m_name == localName) {
            nLocalOffset = m.m_pos; nLocalSize = m.m_size; localFound = true; break;
          }
        }
        if (!localFound) {
          parseStr += "Reference to undeclared local '" + localName + "' in macro '" + macroName + "'\n";
          return false;
        }
        int voffset = 0, vsize = (int)nLocalSize;
        if (ref != refroot) {
          CIccFuncTokenizer p2(ref.c_str());
          p2.GetNext();
          icUInt16Number vo = 0, vs = 1;
          p2.GetIndex(vo, vs, 0, 1);
          voffset = vo; vsize = vs + 1;
        }
        if (voffset + vsize > (int)nLocalSize) {
          parseStr += "Out of bounds indexing local '" + refroot + "' in macro '" + macroName + "'\n";
          return false;
        }
        if (nLocalsOffset + nLocalOffset + voffset + vsize > 65536) {
          parseStr += "Temporary variable addressing out of bounds\n";
          return false;
        }
        char idx[80];
        if (vsize == 1)
          snprintf(idx, sizeof(idx), "[%lu]", nLocalsOffset + nLocalOffset + voffset);
        else
          snprintf(idx, sizeof(idx), "[%lu,%d]", nLocalsOffset + nLocalOffset + voffset, vsize);
        flatStr += "l" + op.substr(1) + idx + " ";
      }
      else {
        // Named temporary variable
        TempVarMap::iterator var = m_varMap.find(refroot);
        if (var == m_varMap.end()) {
          // Try to allocate from declVarMap
          std::string root;
          for (const char *p = refroot.c_str(); *p && *p != '.'; p++) root += *p;

          TempDeclVarMap::iterator decl = m_declVarMap.find(root);
          if (decl == m_declVarMap.end()) {
            parseStr += "Reference to undeclared variable '" + ref + "'\n";
            return false;
          }
          if (decl->second.m_pos < 0) {
            m_varMap[root] = CIccTempVar(root, m_nNextVar, decl->second.m_size);
            decl->second.m_pos = m_nNextVar;
            if (refroot.find('.') != std::string::npos) {
              for (auto &m : decl->second.m_members) {
                m_varMap[root + "." + m.m_name] =
                    CIccTempVar(root + "." + m.m_name, m_nNextVar + m.m_pos, m.m_size);
              }
            }
            if (m_nNextVar + decl->second.m_size > 65536) {
              parseStr += "Temporary variable addressing out of bounds\n";
              return false;
            }
            m_nNextVar += decl->second.m_size;
          }
          else {
            m_varMap[root] = CIccTempVar(root, decl->second.m_pos, decl->second.m_size);
            if (refroot.find('.') != std::string::npos) {
              for (auto &m : decl->second.m_members) {
                m_varMap[root + "." + m.m_name] =
                    CIccTempVar(root + "." + m.m_name, decl->second.m_pos + m.m_pos, m.m_size);
              }
            }
          }
          var = m_varMap.find(refroot);
          if (var == m_varMap.end()) {
            parseStr += "Reference to undeclared variable '" + refroot + "'\n";
            return false;
          }
        }
        int voffset = 0, vsize = var->second.m_size;
        if (ref != refroot) {
          CIccFuncTokenizer p2(ref.c_str());
          p2.GetNext();
          icUInt16Number vo = 0, vs = 1;
          p2.GetIndex(vo, vs, 0, 1);
          voffset = vo; vsize = vs + 1;
        }
        if (voffset + vsize > var->second.m_size) {
          parseStr += "Out of bounds indexing '" + refroot + "'\n";
          return false;
        }
        if (var->second.m_pos + voffset + vsize > 65536) {
          parseStr += "Temporary variable addressing out of bounds\n";
          return false;
        }
        char idx[80];
        if (vsize == 1)
          snprintf(idx, sizeof(idx), "[%d]", var->second.m_pos + voffset);
        else
          snprintf(idx, sizeof(idx), "[%d,%d]", var->second.m_pos + voffset, vsize);
        flatStr += op + idx + " ";
      }
    }

    // -- Sub-element references ---------------------------------------------
    else if (!strncmp(tok, "elem{", 5) || !strncmp(tok, "curv{", 5) ||
             !strncmp(tok, "clut{", 5) || !strncmp(tok, "mtx{",  4) ||
             !strncmp(tok, "fJab{", 5) || !strncmp(tok, "tJab{", 5) ||
             !strncmp(tok, "calc{", 5) || !strncmp(tok, "tint{", 5)) {
      std::string op  = parse.GetName();
      std::string ref = parse.GetReference();
      MpePtrMap::iterator e = m_mpeMap.find(ref);
      if (e == m_mpeMap.end()) {
        parseStr += "Unknown sub-element reference: " + token + "\n";
        return false;
      }
      if (e->second.m_nIndex < 0) {
        if (e->second.m_ptr) {
          m_mpeList.push_back(CIccMpePtr(e->second.m_ptr, m_nNextMpe));
          e->second.m_ptr = nullptr;
          e->second.m_nIndex = m_nNextMpe;
          m_nNextMpe++;
        }
        else {
          parseStr += "Invalid sub-element reference: " + token + "\n";
          return false;
        }
      }
      char idx[80];
      snprintf(idx, sizeof(idx), "(%d)", e->second.m_nIndex);
      flatStr += op + idx + " ";
    }

    // -- Pass through any other token --------------------------------------
    else {
      flatStr += tok;
      flatStr += " ";
    }
  }
  return true;
}

bool CIccMpeJsonCalculator::UpdateLocals(std::string &func, std::string sFunc,
                                          std::string &parseStr, int nLocalsOffset)
{
  func.clear();
  CIccFuncTokenizer parse(sFunc.c_str(), true);

  while (parse.GetNext()) {
    std::string token = parse.GetLast();
    const char *tok = token.c_str();

    if (!strncmp(tok, "lget[", 5) || !strncmp(tok, "lput[", 5) ||
        !strncmp(tok, "lsav[", 5)) {
      std::string op = parse.GetName();
      CIccFuncTokenizer p2(tok + 4);
      icUInt16Number vo = 0, vs = 1;
      p2.GetIndex(vo, vs, 0, 1);
      int voffset = vo + nLocalsOffset;
      int vsize   = vs + 1;
      if (voffset + vsize > 65535) {
        parseStr += "Local variable out of bounds - too many variables.\n";
        return false;
      }
      char idx[80];
      if (vsize == 1) snprintf(idx, sizeof(idx), "[%d]", voffset);
      else            snprintf(idx, sizeof(idx), "[%d,%d]", voffset, vsize);
      func += "t" + op.substr(1) + idx + " ";
    }
    else {
      func += tok;
      func += " ";
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// ParseImport -- recursively load variables, macros, and sub-elements from
// either the top-level JSON (importPath=="*") or an imported JSON file.
// ---------------------------------------------------------------------------
bool CIccMpeJsonCalculator::ParseImport(const IccJson &j, std::string importPath,
                                         std::string &parseStr)
{
  // Process imports (nested JSON files)
  if (j.contains("imports") && j["imports"].is_array()) {
    for (const IccJson &imp : j["imports"]) {
      std::string file;
      jGetString(imp, "file", file);
      if (file.empty()) {
        parseStr += "Missing file in import entry\n";
        return false;
      }
      // File-based imports are useful for the iccFromJson CLI but
      // dangerous when IccLibJSON is embedded in a library/service/WASM
      // context where the JSON input is attacker-controlled. Gate them
      // behind an opt-in (see IccJsonConfig.h), and reject anything
      // that looks like a path escape even when allowed.
      if (!g_IccJsonAllowFileImports) {
        parseStr += "File imports disabled; ignored '" + file + "'\n";
        continue;
      }
      if (file.find('/')  != std::string::npos ||
          file.find('\\') != std::string::npos ||
          file.find("..") != std::string::npos) {
        parseStr += "Unsafe import path '" + file + "'\n";
        return false;
      }
      std::string look = "*" + file + "*";
      if (strstr(importPath.c_str(), look.c_str()))
        continue;  // Already imported -- skip

      std::ifstream ifs(file);
      if (!ifs.is_open()) {
        parseStr += "Unable to open import file '" + file + "'\n";
        return false;
      }
      IccJson jImport;
      try {
        ifs >> jImport;
      }
      catch (...) {
        parseStr += "Invalid JSON in import file '" + file + "'\n";
        return false;
      }

      if (!ParseImport(jImport, importPath + file + "*", parseStr))
        return false;
    }
  }

  // Process variable declarations
  if (j.contains("variables") && j["variables"].is_array()) {
    for (const IccJson &jVar : j["variables"]) {
      std::string name;
      jGetString(jVar, "name", name);
      if (!validName(name.c_str())) {
        parseStr += "Invalid variable name '" + name + "'\n";
        return false;
      }
      if (m_declVarMap.find(name) != m_declVarMap.end()) {
        parseStr += "Variable '" + name + "' was previously declared\n";
        return false;
      }

      int offset = -1, size = 1;
      jGetValue(jVar, "position", offset);
      jGetValue(jVar, "size", size);
      if (size < 1) size = 1;

      if (offset >= 0 && importPath != "*") {
        parseStr += "Position cannot be specified for imported variables\n";
        return false;
      }

      CIccTempDeclVar var(name, offset, (icUInt16Number)size);

      // Optional members string: "m1 m2[2] m3"
      std::string members;
      jGetString(jVar, "members", members);
      if (!members.empty()) {
        CIccFuncTokenizer parse(members.c_str());
        int off = 0;
        while (parse.GetNext()) {
          std::string mname = parse.GetName();
          if (!validName(mname.c_str())) {
            parseStr += "Invalid member name '" + mname + "' for variable '" + name + "'\n";
            return false;
          }
          icUInt16Number msize = 0, extra = 0;
          parse.GetIndex(msize, extra, 1, 0);
          msize++;
          if (msize < 1) msize = 1;
          var.m_members.push_back(CIccTempVar(mname, off, msize));
          off += msize;
        }
        if (var.m_size < off) var.m_size = off;
      }
      m_declVarMap[name] = var;
    }
  }

  // Process macro definitions
  if (j.contains("macros") && j["macros"].is_array()) {
    for (const IccJson &jMacro : j["macros"]) {
      std::string name, body;
      jGetString(jMacro, "name", name);
      jGetString(jMacro, "body", body);

      if (!validName(name.c_str())) {
        parseStr += "Invalid macro name '" + name + "'\n";
        return false;
      }
      if (body.empty()) {
        parseStr += "Missing body for macro '" + name + "'\n";
        return false;
      }

      MacroMap::iterator m = m_macroMap.find(name);
      if (m != m_macroMap.end()) {
        if (m->second == body) continue;
        parseStr += "Macro '" + name + "' was previously defined differently\n";
        return false;
      }
      m_macroMap[name] = body;

      // Optional local variable declarations: "loc1 loc2[2]"
      std::string locals;
      jGetString(jMacro, "locals", locals);
      if (!locals.empty()) {
        CIccTempDeclVar var(name, 0, 0);
        CIccFuncTokenizer parse(locals.c_str());
        int off = 0;
        while (parse.GetNext()) {
          std::string lname = parse.GetName();
          if (!validName(lname.c_str())) {
            parseStr += "Invalid local name '" + lname + "' for macro '" + name + "'\n";
            return false;
          }
          icUInt16Number lsize = 0, extra = 0;
          parse.GetIndex(lsize, extra, 1, 0);
          lsize++;
          if (lsize < 1) lsize = 1;
          var.m_members.push_back(CIccTempVar(lname, off, lsize));
          off += lsize;
        }
        if (var.m_size < off) var.m_size = off;
        m_macroLocalMap[name] = var;
      }
    }
  }

  // Process sub-elements
  if (j.contains("subElements") && j["subElements"].is_array()) {
    for (const IccJson &jElem : j["subElements"]) {
      std::string name;
      jGetString(jElem, "name", name);

      if (!name.empty()) {
        if (!validName(name.c_str())) {
          parseStr += "Invalid SubElement name '" + name + "'\n";
          return false;
        }
        if (m_mpeMap.find(name) != m_mpeMap.end()) {
          parseStr += "Duplicate SubElement '" + name + "'\n";
          return false;
        }
      }
      else if (importPath != "*") {
        parseStr += "Imported SubElements must be named\n";
        return false;
      }

      // Read the "type" discriminator field
      std::string typeName;
      jGetString(jElem, "type", typeName);
      if (typeName.empty()) {
        parseStr += "Missing 'type' field in subElements entry\n";
        return false;
      }

      icElemTypeSignature sig = icJsonGetElemTypeNameSig(typeName.c_str());
      CIccMultiProcessElement *pMpe = CIccMpeCreator::CreateElement(sig);
      if (!pMpe) {
        parseStr += "Unknown SubElement type '" + typeName + "'\n";
        return false;
      }

      // Propagate import path to nested calculators
      if (!strcmp(pMpe->GetClassName(), "CIccMpeJsonCalculator")) {
        static_cast<CIccMpeJsonCalculator*>(pMpe)->m_sImport = importPath;
      }

      IIccExtensionMpe *pExt = pMpe->GetExtension();
      if (!pExt || strcmp(pExt->GetExtClassName(), "CIccMpeJson") != 0) {
        parseStr += std::string(pMpe->GetClassName()) + " is not a CIccMpeJson\n";
        delete pMpe;
        return false;
      }

      CIccMpeJson *pJsonMpe = static_cast<CIccMpeJson*>(pExt);
      if (!pJsonMpe->ParseJson(jElem, parseStr)) {
        parseStr += "Unable to parse SubElement '" + typeName + "'\n";
        delete pMpe;
        return false;
      }

      if (name.empty()) {
        m_mpeList.push_back(CIccMpePtr(pMpe, m_nNextMpe));
        m_nNextMpe++;
      }
      else {
        m_mpeMap[name] = CIccMpePtr(pMpe);
      }
    }
  }

  return true;
}

bool CIccMpeJsonCalculator::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)NumInputChannels();
  j["outputChannels"] = (int)NumOutputChannels();

  // Emit sub-elements (anonymous, in order)
  if (m_SubElem && m_nSubElem) {
    IccJson elems = IccJson::array();
    for (icUInt32Number i = 0; i < m_nSubElem; i++) {
      if (!m_SubElem[i]) return false;
      IIccExtensionMpe *pExt = m_SubElem[i]->GetExtension();
      if (!pExt || strcmp(pExt->GetExtClassName(), "CIccMpeJson") != 0)
        return false;
      CIccMpeJson *pJsonMpe = static_cast<CIccMpeJson*>(pExt);
      IccJson elemObj;
      elemObj["type"]           = icJsonGetElemTypeName(m_SubElem[i]->GetType());
      elemObj["inputChannels"]  = (int)m_SubElem[i]->NumInputChannels();
      elemObj["outputChannels"] = (int)m_SubElem[i]->NumOutputChannels();
      if (m_SubElem[i]->m_nReserved)
        elemObj["Reserved"] = (int)m_SubElem[i]->m_nReserved;
      pJsonMpe->ToJson(elemObj);
      elems.push_back(elemObj);
    }
    j["subElements"] = elems;
  }

  // Emit main function bytecode as text
  if (m_calcFunc) {
    std::string desc;
    m_calcFunc->Describe(desc, 100, 0);
    j["mainFunction"] = desc;
  }

  return true;
}

bool CIccMpeJsonCalculator::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in CalculatorElement\n";
    return false;
  }

  SetSize((icUInt16Number)nIn, (icUInt16Number)nOut);
  clean();

  // Named input/output channels
  std::string inputNames, outputNames;
  jGetString(j, "inputNames",  inputNames);
  jGetString(j, "outputNames", outputNames);

  if (!ParseChanMap(m_inputMap,  inputNames.c_str(),  m_nInputChannels)) {
    parseStr += "Invalid name for inputChannels\n";
    return false;
  }
  if (!ParseChanMap(m_outputMap, outputNames.c_str(), m_nOutputChannels)) {
    parseStr += "Invalid name for outputChannels\n";
    return false;
  }

  // Load variables, macros, and sub-elements (including any imports)
  if (!ParseImport(j, "*", parseStr))
    return false;

  if (!ValidateMacroCalls(parseStr))
    return false;

  // Main function string
  std::string mainFunc;
  jGetString(j, "mainFunction", mainFunc);
  if (mainFunc.empty()) {
    parseStr += "Missing mainFunction in CalculatorElement\n";
    return false;
  }

  // Advance m_nNextVar past any fixed-position variables
  for (auto &kv : m_declVarMap) {
    if (kv.second.m_pos >= 0) {
      int next = kv.second.m_pos + kv.second.m_size;
      if (next > m_nNextVar) m_nNextVar = next;
    }
  }

  // Expand macros, resolve variable / channel / element references
  std::string flatFunc;
  if (!Flatten(flatFunc, "", mainFunc.c_str(), parseStr, 0))
    return false;

  // Resolve lget/lput/lsav local variable offsets if macros used locals
  if (!m_macroLocalMap.empty()) {
    std::string localFunc;
    if (!UpdateLocals(localFunc, flatFunc, parseStr, m_nNextVar))
      return false;
    flatFunc = localFunc;
  }

  // Transfer sub-elements referenced from the function to the compiled array
  int n = 0;
  for (auto &mp : m_mpeList) {
    SetSubElem(n++, mp.m_ptr);
    mp.m_ptr = nullptr;
  }

  icFuncParseStatus stat = SetCalcFunc(flatFunc.c_str(), parseStr);
  if (stat != icFuncParseNoError) {
    char buf[65];
    int len = std::min(64, (int)flatFunc.size());
    strncpy(buf, flatFunc.c_str(), len);
    buf[len] = 0;
    switch (stat) {
      case icFuncParseSyntaxError:
        parseStr += "Syntax error in CalculatorElement MainFunction: \""; break;
      case icFuncParseInvalidOperation:
        parseStr += "Invalid operation in CalculatorElement MainFunction: \""; break;
      case icFuncParseStackUnderflow:
        parseStr += "Stack underflow in CalculatorElement MainFunction: \""; break;
      case icFuncParseInvalidChannel:
        parseStr += "Invalid channel in CalculatorElement MainFunction: \""; break;
      default:
        parseStr += "Unable to parse CalculatorElement MainFunction: \"";
    }
    parseStr += buf;
    parseStr += "\"\n";
    return false;
  }

  clean();  // Free all parse-time state; compiled data is in base class
  return true;
}

// ===========================================================================
// Spectral elements -- shared helpers
// ===========================================================================

static IccJson icSpectralRangeToJson(const icSpectralRange &range)
{
  IccJson j;
  j["start"] = (double)icF16toF(range.start);
  j["end"]   = (double)icF16toF(range.end);
  j["steps"] = (int)range.steps;
  return j;
}

static bool icSpectralRangeFromJson(const IccJson &j, icSpectralRange &range, std::string &parseStr)
{
  if (!j.contains("wavelengths") || !j["wavelengths"].is_object()) {
    parseStr += "Missing wavelengths in spectral element\n";
    return false;
  }
  const IccJson &jw = j["wavelengths"];
  double dStart = 0.0, dEnd = 0.0;
  int nSteps = 0;
  jGetValue(jw, "start", dStart);
  jGetValue(jw, "end",   dEnd);
  jGetValue(jw, "steps", nSteps);
  if (!nSteps) {
    parseStr += "Invalid spectral range (steps==0)\n";
    return false;
  }
  range.start = icFtoF16((icFloat32Number)dStart);
  range.end   = icFtoF16((icFloat32Number)dEnd);
  range.steps = (icUInt16Number)nSteps;
  return true;
}

static IccJson icFloatArrayToJson(const icFloatNumber *buf, int n)
{
  IccJson arr = IccJson::array();
  for (int i = 0; i < n; i++)
    arr.push_back((double)buf[i]);
  return arr;
}

static bool icFloatArrayFromJson(const IccJson &j, const char *field,
                                  icFloatNumber *buf, int n, std::string &parseStr,
                                  bool bRequired = true)
{
  if (!j.contains(field)) {
    if (bRequired)
      parseStr += std::string("Missing ") + field + " in spectral element\n";
    return !bRequired;
  }
  if (!j[field].is_array()) {
    parseStr += std::string(field) + " must be an array in spectral element\n";
    return false;
  }
  const IccJson &arr = j[field];
  if ((int)arr.size() != n) {
    parseStr += std::string(field) + " count does not match spectral element size\n";
    return false;
  }
  for (int i = 0; i < n; i++) {
    if (!arr[i].is_number()) {
      parseStr += std::string(field) + " contains non-numeric value in spectral element\n";
      return false;
    }
    buf[i] = (icFloatNumber)arr[i].get<double>();
  }
  return true;
}

// ===========================================================================
// CIccMpeJsonEmissionMatrix / CIccMpeJsonInvEmissionMatrix
// ===========================================================================

// numVectors() is protected -- helpers take it as an explicit parameter
static bool icSpectralMatrixToJson(IccJson &j, CIccMpeSpectralMatrix *pMtx, int nVectors)
{
  j["inputChannels"]  = (int)pMtx->NumInputChannels();
  j["outputChannels"] = (int)pMtx->NumOutputChannels();
  j["wavelengths"]    = icSpectralRangeToJson(pMtx->GetRange());

  if (pMtx->GetWhite())
    j["whiteData"]  = icFloatArrayToJson(pMtx->GetWhite(), pMtx->GetRange().steps);
  if (pMtx->GetMatrix())
    j["matrixData"] = icFloatArrayToJson(pMtx->GetMatrix(), nVectors * pMtx->GetRange().steps);
  if (pMtx->GetOffset())
    j["offsetData"] = icFloatArrayToJson(pMtx->GetOffset(), pMtx->GetRange().steps);
  return true;
}

static bool icSpectralMatrixFromJson(const IccJson &j, CIccMpeSpectralMatrix *pMtx,
                                      int nVectors, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in spectral matrix element\n";
    return false;
  }

  icSpectralRange range{};
  if (!icSpectralRangeFromJson(j, range, parseStr))
    return false;

  if (!pMtx->SetSize((icUInt16Number)nIn, (icUInt16Number)nOut, range)) {
    parseStr += "Unable to SetSize in spectral matrix element\n";
    return false;
  }

  if (!icFloatArrayFromJson(j, "whiteData", pMtx->GetWhite(), range.steps, parseStr))
    return false;
  if (!icFloatArrayFromJson(j, "matrixData", pMtx->GetMatrix(), nVectors * range.steps, parseStr))
    return false;
  // offsetData optional -- zero-fill if absent
  if (j.contains("offsetData")) {
    if (!icFloatArrayFromJson(j, "offsetData", pMtx->GetOffset(), range.steps, parseStr, false))
      return false;
  }
  else {
    memset(pMtx->GetOffset(), 0, range.steps * sizeof(icFloatNumber));
  }
  return true;
}

bool CIccMpeJsonEmissionMatrix::ToJson(IccJson &j)
{
  return icSpectralMatrixToJson(j, this, numVectors());
}

bool CIccMpeJsonEmissionMatrix::ParseJson(const IccJson &j, std::string &parseStr)
{
  // For EmissionMatrix numVectors() == m_nInputChannels; parse nIn first then pass it
  int nIn = 0;
  jGetValue(j, "inputChannels", nIn);
  return icSpectralMatrixFromJson(j, this, nIn, parseStr);
}

bool CIccMpeJsonInvEmissionMatrix::ToJson(IccJson &j)
{
  return icSpectralMatrixToJson(j, this, numVectors());
}

bool CIccMpeJsonInvEmissionMatrix::ParseJson(const IccJson &j, std::string &parseStr)
{
  // For InvEmissionMatrix numVectors() == m_nOutputChannels; parse nOut first then pass it
  int nOut = 0;
  jGetValue(j, "outputChannels", nOut);
  return icSpectralMatrixFromJson(j, this, nOut, parseStr);
}

// ===========================================================================
// CIccMpeJsonEmissionCLUT / CIccMpeJsonReflectanceCLUT
// (m_flags, m_nStorageType, m_Range are protected -- accessed from derived member functions)
// ===========================================================================

// Shared parse helper: fills out-params; caller owns pWhite and pCLUT on success.
static bool icSpectralCLUTParseJson(const IccJson &j, std::string &parseStr,
                                     int &nIn, int &nOut,
                                     icUInt32Number &flags, icUInt16Number &storageType,
                                     icSpectralRange &range,
                                     icFloatNumber *&pWhite, CIccCLUT *&pCLUT)
{
  int iFlags = 0, iStorageType = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  jGetValue(j, "flags",          iFlags);
  jGetValue(j, "storageType",    iStorageType);
  flags       = (icUInt32Number)iFlags;
  storageType = (icUInt16Number)iStorageType;

  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in spectral CLUT element\n";
    return false;
  }

  if (!icSpectralRangeFromJson(j, range, parseStr))
    return false;

  pWhite = (icFloatNumber*)malloc(range.steps * sizeof(icFloatNumber));
  if (!pWhite) {
    parseStr += "White buffer memory error in spectral CLUT element\n";
    return false;
  }
  if (!icFloatArrayFromJson(j, "whiteData", pWhite, range.steps, parseStr)) {
    free(pWhite); pWhite = nullptr;
    return false;
  }

  if (!j.contains("clut") || !j["clut"].is_object()) {
    parseStr += "Missing clut data in spectral CLUT element\n";
    free(pWhite); pWhite = nullptr;
    return false;
  }

  // CLUT output channels = spectral steps, not the element's final output channels
  pCLUT = icCLUTFromJson(j["clut"], nIn, range.steps, icConvertFloat, parseStr);
  if (!pCLUT) {
    free(pWhite); pWhite = nullptr;
    return false;
  }
  return true;
}

bool CIccMpeJsonEmissionCLUT::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;
  j["flags"]          = (int)m_flags;
  j["storageType"]    = (int)m_nStorageType;
  j["wavelengths"]    = icSpectralRangeToJson(m_Range);
  if (m_pWhite)
    j["whiteData"] = icFloatArrayToJson(m_pWhite, m_Range.steps);
  if (m_pCLUT)
    icCLUTToJson(j, m_pCLUT, icConvertFloat, true, "clut");
  return true;
}

bool CIccMpeJsonEmissionCLUT::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  icUInt32Number flags = 0;
  icUInt16Number storageType = 0;
  icSpectralRange range{};
  icFloatNumber *pWhite = nullptr;
  CIccCLUT *pCLUT = nullptr;

  if (!icSpectralCLUTParseJson(j, parseStr, nIn, nOut, flags, storageType, range, pWhite, pCLUT))
    return false;

  SetData(pCLUT, storageType, range, pWhite, (icUInt16Number)nOut);
  m_flags = flags;
  return true;
}

bool CIccMpeJsonReflectanceCLUT::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;
  j["flags"]          = (int)m_flags;
  j["storageType"]    = (int)m_nStorageType;
  j["wavelengths"]    = icSpectralRangeToJson(m_Range);
  if (m_pWhite)
    j["whiteData"] = icFloatArrayToJson(m_pWhite, m_Range.steps);
  if (m_pCLUT)
    icCLUTToJson(j, m_pCLUT, icConvertFloat, true, "clut");
  return true;
}

bool CIccMpeJsonReflectanceCLUT::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  icUInt32Number flags = 0;
  icUInt16Number storageType = 0;
  icSpectralRange range{};
  icFloatNumber *pWhite = nullptr;
  CIccCLUT *pCLUT = nullptr;

  if (!icSpectralCLUTParseJson(j, parseStr, nIn, nOut, flags, storageType, range, pWhite, pCLUT))
    return false;

  SetData(pCLUT, storageType, range, pWhite, (icUInt16Number)nOut);
  m_flags = flags;
  return true;
}

// ===========================================================================
// CIccMpeJsonEmissionObserver / CIccMpeJsonReflectanceObserver
// (m_flags and m_Range are protected -- accessed from derived member functions)
// ===========================================================================

bool CIccMpeJsonEmissionObserver::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;
  j["flags"]          = (int)m_flags;
  j["wavelengths"]    = icSpectralRangeToJson(m_Range);
  if (m_pWhite)
    j["whiteData"] = icFloatArrayToJson(m_pWhite, m_Range.steps);
  return true;
}

bool CIccMpeJsonEmissionObserver::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0, flags = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  jGetValue(j, "flags",          flags);
  m_flags = (icUInt16Number)flags;

  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in EmissionObserverElement\n";
    return false;
  }

  icSpectralRange range{};
  if (!icSpectralRangeFromJson(j, range, parseStr))
    return false;

  if (range.steps != (icUInt16Number)nIn) {
    parseStr += "Spectral observer wavelength steps must equal inputChannels\n";
    return false;
  }

  if (!SetSize((icUInt16Number)nIn, (icUInt16Number)nOut, range)) {
    parseStr += "Unable to SetSize in EmissionObserverElement\n";
    return false;
  }

  return icFloatArrayFromJson(j, "whiteData", m_pWhite, range.steps, parseStr);
}

bool CIccMpeJsonReflectanceObserver::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;
  j["flags"]          = (int)m_flags;
  j["wavelengths"]    = icSpectralRangeToJson(m_Range);
  if (m_pWhite)
    j["whiteData"] = icFloatArrayToJson(m_pWhite, m_Range.steps);
  return true;
}

bool CIccMpeJsonReflectanceObserver::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0, flags = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  jGetValue(j, "flags",          flags);
  m_flags = (icUInt16Number)flags;

  if (!nIn || !nOut) {
    parseStr += "Invalid inputChannels or outputChannels in ReflectanceObserverElement\n";
    return false;
  }

  icSpectralRange range{};
  if (!icSpectralRangeFromJson(j, range, parseStr))
    return false;

  if (range.steps != (icUInt16Number)nIn) {
    parseStr += "Spectral observer wavelength steps must equal inputChannels\n";
    return false;
  }

  if (!SetSize((icUInt16Number)nIn, (icUInt16Number)nOut, range)) {
    parseStr += "Unable to SetSize in ReflectanceObserverElement\n";
    return false;
  }

  return icFloatArrayFromJson(j, "whiteData", m_pWhite, range.steps, parseStr);
}

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif
