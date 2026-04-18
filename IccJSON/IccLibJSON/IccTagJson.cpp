/** @file
    File:       IccTagJson.cpp

    Contains:   Implementation of ICC tag JSON format conversions

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

#include "IccTagJson.h"
#include "IccMpeJson.h"
#include "IccIoJson.h"
#include "IccUtil.h"
#include "IccUtilJson.h"
#include "IccSparseMatrix.h"
#include "IccProfileJson.h"
#include "IccStructFactory.h"
#include "IccArrayFactory.h"
#include "IccMpeFactory.h"
#include <cstdlib>
#include <cstring>
#include <set>
#include <map>
#include <sstream>
#include <iomanip>
#include <climits>
#include <new>

// Safely narrow size_t to icUInt32Number; returns 0 and sets overflow flag on truncation
static inline icUInt32Number icJsonSafeU32(size_t n, bool *overflow = nullptr)
{
  if (n > (size_t)UINT32_MAX) {
    if (overflow) *overflow = true;
    return 0;
  }
  if (overflow) *overflow = false;
  return (icUInt32Number)n;
}

// Safely narrow size_t to icUInt16Number; returns 0 on truncation
static inline icUInt16Number icJsonSafeU16(size_t n)
{
  if (n > (size_t)0xFFFF)
    return 0;
  return (icUInt16Number)n;
}

typedef std::map<icUInt32Number, icTagSignature> IccOffsetTagSigMap;

// Forward declarations for helpers defined later in this file
static IccJson dictLocalizedToJson(const CIccTagMultiLocalizedUnicode *pTag);
static CIccTagMultiLocalizedUnicode *dictLocalizedFromJson(const IccJson &arr);

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

// ---------------------------------------------------------------------------
// Helper: 4-char signature to string
// ---------------------------------------------------------------------------
static std::string sigToStr(icUInt32Number sig)
{
  char buf[32];
  icGetSigStr(buf, sizeof(buf), sig);
  return std::string(buf);
}

// ===========================================================================
// CIccTagJsonUnknown
// ===========================================================================

bool CIccTagJsonUnknown::ToJson(IccJson &j)
{
  j["unknownData"] = icJsonDumpHexData(m_pData, m_nSize);
  return true;
}

bool CIccTagJsonUnknown::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string hex;
  if (jGetString(j, "unknownData", hex)) {
    m_nSize = icJsonGetHexDataSize(hex.c_str());
    if (m_pData) { delete[] m_pData; m_pData = NULL; }
    if (m_nSize) {
      m_pData = new(std::nothrow) icUInt8Number[m_nSize];
      if (!m_pData) { m_nSize = 0; return false; }
      icJsonGetHexData(m_pData, hex.c_str(), m_nSize);
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonText
// ===========================================================================

bool CIccTagJsonText::ToJson(IccJson &j)
{
  j["text"] = m_szText ? m_szText : "";
  return true;
}

bool CIccTagJsonText::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string text;
  if (jGetString(j, "text", text))
    SetText(text.c_str());
  return true;
}

// ===========================================================================
// CIccTagJsonUtf8Text
// ===========================================================================

bool CIccTagJsonUtf8Text::ToJson(IccJson &j)
{
  if (m_szText)
    j["text"] = std::string((const char*)m_szText);
  else
    j["text"] = "";
  return true;
}

bool CIccTagJsonUtf8Text::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string s;
  if (jGetString(j, "text", s))
    SetText(s.c_str());
  return true;
}

// ===========================================================================
// CIccTagJsonZipUtf8Text
// ===========================================================================

bool CIccTagJsonZipUtf8Text::ToJson(IccJson &j)
{
  j["compressedData"] = icJsonDumpHexData(m_pZipBuf, m_nBufSize);
  return true;
}

bool CIccTagJsonZipUtf8Text::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string hex;
  if (jGetString(j, "compressedData", hex)) {
    icUInt32Number sz = icJsonGetHexDataSize(hex.c_str());
    icUChar *pBuf = AllocBuffer(sz);
    if (!pBuf) return false;
    icJsonGetHexData(pBuf, hex.c_str(), sz);
  }
  return true;
}

// ===========================================================================
// CIccTagJsonZipXml
// ===========================================================================

bool CIccTagJsonZipXml::ToJson(IccJson &j)
{
  j["compressedData"] = icJsonDumpHexData(m_pZipBuf, m_nBufSize);
  return true;
}

bool CIccTagJsonZipXml::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string hex;
  if (jGetString(j, "compressedData", hex)) {
    icUInt32Number sz = icJsonGetHexDataSize(hex.c_str());
    icUChar *pBuf = AllocBuffer(sz);
    if (!pBuf) return false;
    icJsonGetHexData(pBuf, hex.c_str(), sz);
  }
  return true;
}

// ===========================================================================
// CIccTagJsonUtf16Text
// ===========================================================================

bool CIccTagJsonUtf16Text::ToJson(IccJson &j)
{
  // Convert UTF-16 to UTF-8 for JSON
  std::string utf8;
  if (m_szText) {
    const icUInt16Number *p = m_szText;
    // Simple 2-byte to string conversion (ASCII range only for skeleton)
    while (*p) {
      if (*p < 128) utf8 += (char)(*p);
      else utf8 += '?';
      p++;
    }
  }
  j["text"] = utf8;
  return true;
}

bool CIccTagJsonUtf16Text::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string s;
  if (jGetString(j, "text", s))
    SetText(s.c_str());
  return true;
}

// ===========================================================================
// CIccTagJsonTextDescription
// ===========================================================================

bool CIccTagJsonTextDescription::ToJson(IccJson &j)
{
  j["description"] = m_szText ? m_szText : "";
  return true;
}

bool CIccTagJsonTextDescription::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string desc;
  if (jGetString(j, "description", desc))
    SetText(desc.c_str());
  return true;
}

// ===========================================================================
// CIccTagJsonSignature
// ===========================================================================

bool CIccTagJsonSignature::ToJson(IccJson &j)
{
  j["signature"] = sigToStr(m_nSig);
  return true;
}

bool CIccTagJsonSignature::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string sig;
  jGetString(j, "signature", sig);
  if (!sig.empty())
    m_nSig = (icSignature)icGetSigVal(sig.c_str());
  return true;
}

// ===========================================================================
// CIccTagJsonDateTime
// ===========================================================================

bool CIccTagJsonDateTime::ToJson(IccJson &j)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%d-%02d-%02dT%02d:%02d:%02d",
           m_DateTime.year, m_DateTime.month,   m_DateTime.day,
           m_DateTime.hours, m_DateTime.minutes, m_DateTime.seconds);
  j["dateTime"] = buf;
  return true;
}

bool CIccTagJsonDateTime::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string dt;
  jGetString(j, "dateTime", dt);
  m_DateTime = icGetDateTimeValue(dt.c_str());
  return true;
}

// ===========================================================================
// CIccTagJsonXYZ
// ===========================================================================

bool CIccTagJsonXYZ::ToJson(IccJson &j)
{
  IccJson arr = IccJson::array();
  if (m_XYZ) {
    for (icUInt32Number i = 0; i < m_nSize; i++) {
      arr.push_back(IccJson::array({
        (double)icFtoD(m_XYZ[i].X),
        (double)icFtoD(m_XYZ[i].Y),
        (double)icFtoD(m_XYZ[i].Z)
      }));
    }
  }
  j["XYZ"] = arr;
  return true;
}

bool CIccTagJsonXYZ::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (!jsonExistsField(j, "XYZ") || !j["XYZ"].is_array()) return false;
  const IccJson &arr = j["XYZ"];
  if (!SetSize(icJsonSafeU32(arr.size()))) return false;
  for (size_t i = 0; i < arr.size(); i++) {
    if (arr[i].is_array() && arr[i].size() >= 3) {
      double xyz[3] = {0, 0, 0};
      jsonToArray(arr[i], xyz, 3);
      m_XYZ[i].X = icDtoF(xyz[0]);
      m_XYZ[i].Y = icDtoF(xyz[1]);
      m_XYZ[i].Z = icDtoF(xyz[2]);
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonChromaticity
// ===========================================================================

bool CIccTagJsonChromaticity::ToJson(IccJson &j)
{
  j["colorantType"] = (int)m_nColorantType;
  IccJson channels = IccJson::array();
  for (icUInt16Number i = 0; i < m_nChannels; i++)
    channels.push_back(IccJson::array({ icUFtoD(m_xy[i].x), icUFtoD(m_xy[i].y) }));
  j["channels"] = channels;
  return true;
}

bool CIccTagJsonChromaticity::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  int colorantType = 0;
  jGetValue(j, "colorantType", colorantType);
  m_nColorantType = (icColorantEncoding)colorantType;
  if (jsonExistsField(j, "channels") && j["channels"].is_array()) {
    const IccJson &ch = j["channels"];
    icUInt16Number nCh = icJsonSafeU16(ch.size());
    if (!nCh && ch.size()) return false;
    if (!SetSize(nCh)) return false;
    for (icUInt16Number i = 0; i < nCh; i++) {
      if (ch[i].is_array() && ch[i].size() >= 2) {
        double xy[2] = {0, 0};
        jsonToArray(ch[i], xy, 2);
        m_xy[i].x = icDtoUF(xy[0]);
        m_xy[i].y = icDtoUF(xy[1]);
      }
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonCicp
// ===========================================================================

bool CIccTagJsonCicp::ToJson(IccJson &j)
{
  j["colourPrimaries"]         = (int)m_nColorPrimaries;
  j["transferCharacteristics"] = (int)m_nTransferCharacteristics;
  j["matrixCoefficients"]      = (int)m_nMatrixCoefficients;
  j["videoFullRangeFlag"]      = (int)m_nVideoFullRangeFlag;
  return true;
}

bool CIccTagJsonCicp::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  m_nColorPrimaries = 0;          jGetValue(j, "colourPrimaries",         m_nColorPrimaries);
  m_nTransferCharacteristics = 0; jGetValue(j, "transferCharacteristics", m_nTransferCharacteristics);
  m_nMatrixCoefficients = 0;      jGetValue(j, "matrixCoefficients",      m_nMatrixCoefficients);
  m_nVideoFullRangeFlag = 0;      jGetValue(j, "videoFullRangeFlag",      m_nVideoFullRangeFlag);
  return true;
}

// ===========================================================================
// CIccTagJsonMeasurement
// ===========================================================================

bool CIccTagJsonMeasurement::ToJson(IccJson &j)
{
  CIccInfo info;
  j["standardObserver"]  = (int)m_Data.stdObserver;
  j["backing"]           = IccJson::array({ icFtoD(m_Data.backing.X), icFtoD(m_Data.backing.Y), icFtoD(m_Data.backing.Z) });
  j["geometry"]          = (int)m_Data.geometry;
  j["flare"]             = info.GetMeasurementFlareName(m_Data.flare);
  j["illuminant"]        = (int)m_Data.illuminant;
  return true;
}

bool CIccTagJsonMeasurement::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  int stdObserver = 0, geometry = 0, illuminant = 0;
  std::string flare;
  double backing[3] = {0, 0, 0};
  jGetValue(j, "standardObserver", stdObserver);
  jGetArray(j, "backing", backing, 3);
  jGetValue(j, "geometry",  geometry);
  jGetString(j, "flare",    flare);
  jGetValue(j, "illuminant",illuminant);
  m_Data.stdObserver = (icStandardObserver)stdObserver;
  m_Data.backing.X   = icDtoF(backing[0]);
  m_Data.backing.Y   = icDtoF(backing[1]);
  m_Data.backing.Z   = icDtoF(backing[2]);
  m_Data.geometry    = (icMeasurementGeometry)geometry;
  if (flare == "Flare 100")
    m_Data.flare = icFlare100;
  else
    m_Data.flare = icFlare0;
  m_Data.illuminant  = (icIlluminant)illuminant;
  return true;
}

// ===========================================================================
// CIccTagJsonViewingConditions
// ===========================================================================

bool CIccTagJsonViewingConditions::ToJson(IccJson &j)
{
  j["illuminant"]  = IccJson::array({ icFtoD(m_XYZIllum.X), icFtoD(m_XYZIllum.Y), icFtoD(m_XYZIllum.Z) });
  j["surround"]    = IccJson::array({ icFtoD(m_XYZSurround.X), icFtoD(m_XYZSurround.Y), icFtoD(m_XYZSurround.Z) });
  j["illuminantType"] = (int)m_illumType;
  return true;
}

bool CIccTagJsonViewingConditions::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  double illum[3] = {0, 0, 0};
  double surround[3] = {0, 0, 0};
  int illuminantType = 0;
  jGetArray(j, "illuminant", illum, 3);
  jGetArray(j, "surround", surround, 3);
  jGetValue(j, "illuminantType", illuminantType);
  m_XYZIllum.X   = icDtoF(illum[0]);
  m_XYZIllum.Y   = icDtoF(illum[1]);
  m_XYZIllum.Z   = icDtoF(illum[2]);
  m_XYZSurround.X = icDtoF(surround[0]);
  m_XYZSurround.Y = icDtoF(surround[1]);
  m_XYZSurround.Z = icDtoF(surround[2]);
  m_illumType     = (icIlluminant)illuminantType;
  return true;
}

// ===========================================================================
// CIccTagJsonSpectralDataInfo
// ===========================================================================

bool CIccTagJsonSpectralDataInfo::ToJson(IccJson &j)
{
  j["spectralColorSig"] = sigToStr(m_nSig);
  j["spectralRange"]    = IccJson::array({ icF16toF(m_spectralRange.start), icF16toF(m_spectralRange.end), (int)m_spectralRange.steps });
  j["biSpectralRange"]  = IccJson::array({ icF16toF(m_biSpectralRange.start), icF16toF(m_biSpectralRange.end), (int)m_biSpectralRange.steps });
  return true;
}

bool CIccTagJsonSpectralDataInfo::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string sig;
  jGetString(j, "spectralColorSig", sig);
  m_nSig = (icSignature)icGetSigVal(sig.c_str());
  if (jsonExistsField(j, "spectralRange") && j["spectralRange"].is_array() && j["spectralRange"].size() >= 3) {
    double r[3] = {0, 0, 0};
    jsonToArray(j["spectralRange"], r, 3);
    m_spectralRange.start = icFtoF16((icFloat32Number)r[0]);
    m_spectralRange.end   = icFtoF16((icFloat32Number)r[1]);
    m_spectralRange.steps = (icUInt16Number)r[2];
  }
  if (jsonExistsField(j, "biSpectralRange") && j["biSpectralRange"].is_array() && j["biSpectralRange"].size() >= 3) {
    double r[3] = {0, 0, 0};
    jsonToArray(j["biSpectralRange"], r, 3);
    m_biSpectralRange.start = icFtoF16((icFloat32Number)r[0]);
    m_biSpectralRange.end   = icFtoF16((icFloat32Number)r[1]);
    m_biSpectralRange.steps = (icUInt16Number)r[2];
  }
  return true;
}

// ===========================================================================
// CIccTagJsonSpectralViewingConditions
// ===========================================================================

bool CIccTagJsonSpectralViewingConditions::ToJson(IccJson &j)
{
  CIccInfo info;
  j["StdObserver"]  = info.GetStandardObserverName(m_stdObserver);
  j["IlluminantXYZ"] = IccJson::array({ (double)m_illuminantXYZ.X,
                                        (double)m_illuminantXYZ.Y,
                                        (double)m_illuminantXYZ.Z });

  if (m_observer && m_observerRange.steps) {
    IccJson obs;
    obs["start"] = (double)icF16toF(m_observerRange.start);
    obs["end"]   = (double)icF16toF(m_observerRange.end);
    obs["steps"] = (int)m_observerRange.steps;
    if (m_reserved2)
      obs["Reserved"] = (int)m_reserved2;
    IccJson data = IccJson::array();
    icUInt32Number nTotal = (icUInt32Number)m_observerRange.steps * 3;
    for (icUInt32Number i = 0; i < nTotal; i++)
      data.push_back((double)m_observer[i]);
    obs["data"] = data;
    j["ObserverFuncs"] = obs;
  }

  j["StdIlluminant"]    = info.GetIlluminantName(m_stdIlluminant);
  j["ColorTemperature"] = (double)m_colorTemperature;

  if (m_illuminant && m_illuminantRange.steps) {
    IccJson illum;
    illum["start"] = (double)icF16toF(m_illuminantRange.start);
    illum["end"]   = (double)icF16toF(m_illuminantRange.end);
    illum["steps"] = (int)m_illuminantRange.steps;
    if (m_reserved3)
      illum["Reserved"] = (int)m_reserved3;
    IccJson data = IccJson::array();
    for (int i = 0; i < (int)m_illuminantRange.steps; i++)
      data.push_back((double)m_illuminant[i]);
    illum["data"] = data;
    j["IlluminantSPD"] = illum;
  }

  j["SurroundXYZ"] = IccJson::array({ (double)m_surroundXYZ.X,
                                      (double)m_surroundXYZ.Y,
                                      (double)m_surroundXYZ.Z });
  return true;
}

bool CIccTagJsonSpectralViewingConditions::ParseJson(const IccJson &j, std::string &parseStr)
{
  memset(&m_illuminantXYZ, 0, sizeof(m_illuminantXYZ));
  memset(&m_surroundXYZ,   0, sizeof(m_surroundXYZ));
  m_stdObserver      = icStdObs1931TwoDegrees;
  m_stdIlluminant    = icIlluminantD50;
  m_colorTemperature = 5000.0f;
  m_reserved2        = 0;
  m_reserved3        = 0;

  std::string obsStr;
  jGetString(j, "StdObserver", obsStr);
  m_stdObserver = obsStr.empty() ? icStdObs1931TwoDegrees
                                 : icGetNamedStandardObserverValue(obsStr.c_str());

  icFloatNumber xyz[3] = { 0.0f, 0.0f, 0.0f };
  jGetArray(j, "IlluminantXYZ", xyz, 3);
  m_illuminantXYZ.X = xyz[0];
  m_illuminantXYZ.Y = xyz[1];
  m_illuminantXYZ.Z = xyz[2];

  // Optional ObserverFuncs
  if (jsonExistsField(j, "ObserverFuncs") && j["ObserverFuncs"].is_object()) {
    const IccJson &obs = j["ObserverFuncs"];
    icFloat32Number start = 0.0f, end = 0.0f;
    int steps = 0;
    jGetValue(obs, "start", start);
    jGetValue(obs, "end",   end);
    jGetValue(obs, "steps", steps);
    unsigned int res2 = 0;
    jGetValue(obs, "Reserved", res2);
    m_observerRange.start = icFtoF16(start);
    m_observerRange.end   = icFtoF16(end);
    m_observerRange.steps = (icUInt16Number)steps;
    m_reserved2           = (icUInt16Number)res2;

    if (jsonExistsField(obs, "data") && obs["data"].is_array()) {
      const IccJson &data = obs["data"];
      icUInt32Number nExpected = (icUInt32Number)steps * 3;
      if (icJsonSafeU32(data.size()) != nExpected) {
        parseStr += "ObserverFuncs data size mismatch\n";
        return false;
      }
      if (m_observer) { delete[] m_observer; }
      m_observer = new(std::nothrow) icFloatNumber[nExpected];
      if (!m_observer) { parseStr += "Allocation failed for observer\n"; return false; }
      for (icUInt32Number i = 0; i < nExpected; i++)
        m_observer[i] = (icFloatNumber)data[i].get<double>();
    }
    else {
      setObserver(m_stdObserver, m_observerRange, NULL);
    }
  }

  std::string illumStr;
  jGetString(j, "StdIlluminant", illumStr);
  m_stdIlluminant = illumStr.empty() ? icIlluminantD50
                                     : icGetIlluminantValue(illumStr.c_str());

  jGetValue(j, "ColorTemperature", m_colorTemperature);

  // Optional IlluminantSPD
  if (jsonExistsField(j, "IlluminantSPD") && j["IlluminantSPD"].is_object()) {
    const IccJson &illum = j["IlluminantSPD"];
    icFloat32Number start = 0.0f, end = 0.0f;
    int steps = 0;
    jGetValue(illum, "start", start);
    jGetValue(illum, "end",   end);
    jGetValue(illum, "steps", steps);
    unsigned int res3 = 0;
    jGetValue(illum, "Reserved", res3);
    m_illuminantRange.start = icFtoF16(start);
    m_illuminantRange.end   = icFtoF16(end);
    m_illuminantRange.steps = (icUInt16Number)steps;
    m_reserved3             = (icUInt16Number)res3;

    if (jsonExistsField(illum, "data") && illum["data"].is_array()) {
      const IccJson &data = illum["data"];
      if ((int)data.size() != steps) {
        parseStr += "IlluminantSPD data size mismatch\n";
        return false;
      }
      if (m_illuminant) { delete[] m_illuminant; }
      m_illuminant = new(std::nothrow) icFloatNumber[steps];
      if (!m_illuminant) { parseStr += "Allocation failed for illuminant\n"; return false; }
      for (int i = 0; i < steps; i++)
        m_illuminant[i] = (icFloatNumber)data[i].get<double>();
    }
    else {
      setIlluminant(m_stdIlluminant, m_illuminantRange, NULL, m_colorTemperature);
    }
  }

  icFloatNumber sxyz[3] = { 0.0f, 0.0f, 0.0f };
  jGetArray(j, "SurroundXYZ", sxyz, 3);
  m_surroundXYZ.X = sxyz[0];
  m_surroundXYZ.Y = sxyz[1];
  m_surroundXYZ.Z = sxyz[2];

  return true;
}

// ===========================================================================
// CIccTagJsonNamedColor2
// ===========================================================================

bool CIccTagJsonNamedColor2::ToJson(IccJson &j)
{
  j["vendorFlag"]  = (int)m_nVendorFlags;
  j["countOfDeviceCoords"] = (int)m_nDeviceCoords;
  j["colorantPrefix"] = m_szPrefix[0] ? m_szPrefix : "";
  j["colorantSuffix"] = m_szSufix[0]  ? m_szSufix  : "";

  IccJson colors = IccJson::array();
  if (m_NamedColor) {
    for (icUInt32Number i = 0; i < m_nSize; i++) {
      IccJson c;
      c["name"] = m_NamedColor[i].rootName;
      c["pcsCoords"] = IccJson::array({ m_NamedColor[i].pcsCoords[0], m_NamedColor[i].pcsCoords[1], m_NamedColor[i].pcsCoords[2] });
      IccJson dev = IccJson::array();
      for (icUInt32Number d = 0; d < m_nDeviceCoords; d++)
        dev.push_back((int)(m_NamedColor[i].deviceCoords[d] * 65535.0f + 0.5f));
      c["deviceCoords"] = dev;
      colors.push_back(c);
    }
  }
  j["colors"] = colors;
  return true;
}

bool CIccTagJsonNamedColor2::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  icUInt32Number nDevCoords = 0;
  jGetValue(j, "countOfDeviceCoords", nDevCoords);
  icUInt32Number nColors = (jsonExistsField(j, "colors") && j["colors"].is_array())
                           ? icJsonSafeU32(j["colors"].size()) : 0;

  if (!SetSize(nColors, nDevCoords)) return false;

  jGetValue(j, "vendorFlag", m_nVendorFlags);
  std::string prefix, suffix;
  if (jGetString(j, "colorantPrefix", prefix))
    strncpy(m_szPrefix, prefix.c_str(), sizeof(m_szPrefix)-1);
  if (jGetString(j, "colorantSuffix", suffix))
    strncpy(m_szSufix,  suffix.c_str(), sizeof(m_szSufix)-1);

  if (jsonExistsField(j, "colors") && j["colors"].is_array()) {
    const IccJson &colors = j["colors"];
    for (icUInt32Number i = 0; i < nColors && i < icJsonSafeU32(colors.size()); i++) {
      const IccJson &c = colors[i];
      std::string name;
      if (jGetString(c, "name", name))
        strncpy(m_NamedColor[i].rootName, name.c_str(), sizeof(m_NamedColor[i].rootName)-1);
      if (jsonExistsField(c, "pcsCoords") && c["pcsCoords"].is_array() && c["pcsCoords"].size() >= 3) {
        jGetArray(c, "pcsCoords", m_NamedColor[i].pcsCoords, 3);
      }
      if (jsonExistsField(c, "deviceCoords") && c["deviceCoords"].is_array()) {
        for (icUInt32Number d = 0; d < nDevCoords && d < icJsonSafeU32(c["deviceCoords"].size()); d++)
          m_NamedColor[i].deviceCoords[d] = (icFloatNumber)c["deviceCoords"][d].get<int>() / 65535.0f;
      }
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonColorantOrder
// ===========================================================================

bool CIccTagJsonColorantOrder::ToJson(IccJson &j)
{
  IccJson arr = IccJson::array();
  for (icUInt32Number i = 0; i < m_nCount; i++)
    arr.push_back((int)m_pData[i]);
  j["colorantOrder"] = arr;
  return true;
}

bool CIccTagJsonColorantOrder::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (jsonExistsField(j, "colorantOrder") && j["colorantOrder"].is_array()) {
    const IccJson &arr = j["colorantOrder"];
    if (!SetSize(icJsonSafeU32(arr.size()))) return false;
    for (size_t i = 0; i < arr.size(); i++)
      m_pData[i] = (icUInt8Number)arr[i].get<int>();
  }
  return true;
}

// ===========================================================================
// CIccTagJsonColorantTable
// ===========================================================================

bool CIccTagJsonColorantTable::ToJson(IccJson &j)
{
  j["pcsEncoding"] = "Lab";
  IccJson arr = IccJson::array();
  for (icUInt32Number i = 0; i < m_nCount; i++) {
    icFloatNumber pcs[3];
    pcs[0] = icU16toF(m_pData[i].data[0]);
    pcs[1] = icU16toF(m_pData[i].data[1]);
    pcs[2] = icU16toF(m_pData[i].data[2]);
    icLabFromPcs(pcs);
    IccJson c;
    c["name"] = m_pData[i].name;
    c["pcs"]  = IccJson::array({ pcs[0], pcs[1], pcs[2] });
    arr.push_back(c);
  }
  j["colorantTable"] = arr;
  return true;
}

bool CIccTagJsonColorantTable::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  std::string pcsEncoding = "Lab";
  jGetString(j, "pcsEncoding", pcsEncoding);

  if (jsonExistsField(j, "colorantTable") && j["colorantTable"].is_array()) {
    const IccJson &arr = j["colorantTable"];
    if (!SetSize(icJsonSafeU32(arr.size()))) return false;
    for (size_t i = 0; i < arr.size(); i++) {
      const IccJson &c = arr[i];
      std::string name;
      if (jGetString(c, "name", name))
        strncpy(m_pData[i].name, name.c_str(), sizeof(m_pData[i].name)-1);
      if (jsonExistsField(c, "pcs") && c["pcs"].is_array() && c["pcs"].size() >= 3) {
        if (pcsEncoding == "16bit") {
          jGetArray(c, "pcs", m_pData[i].data, 3);
        } else {
          icFloatNumber pcs[3];
          jGetArray(c, "pcs", pcs, 3);
          if (pcsEncoding == "XYZ")
            icXyzToPcs(pcs);
          else  // "Lab" (default)
            icLabToPcs(pcs);
          m_pData[i].data[0] = icFtoU16(pcs[0]);
          m_pData[i].data[1] = icFtoU16(pcs[1]);
          m_pData[i].data[2] = icFtoU16(pcs[2]);
        }
      }
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonSparseMatrixArray
// ===========================================================================

bool CIccTagJsonSparseMatrixArray::ToJson(IccJson &j)
{
  j["outputChannels"] = (int)m_nChannelsPerMatrix;
  j["matrixType"]     = (int)m_nMatrixType;

  CIccSparseMatrix mtx;
  icUInt32Number bytesPerMatrix = GetBytesPerMatrix();
  IccJson matrices = IccJson::array();

  for (icUInt32Number i = 0; i < m_nSize; i++) {
    mtx.Reset(m_RawData + i * bytesPerMatrix, bytesPerMatrix, icSparseMatrixFloatNum, true);

    IccJson jMtx;
    jMtx["rows"] = (int)mtx.Rows();
    jMtx["cols"] = (int)mtx.Cols();

    IccJson jRows = IccJson::array();
    for (int r = 0; r < (int)mtx.Rows(); r++) {
      int n = mtx.GetNumRowColumns(r);
      IccJson jRow;
      IccJson jIdx  = IccJson::array();
      IccJson jData = IccJson::array();

      icUInt16Number *cols = mtx.GetColumnsForRow(r);
      icFloatNumber  *data = (icFloatNumber*)mtx.GetData()->getPtr(mtx.GetRowOffset(r));
      for (int k = 0; k < n; k++) {
        jIdx.push_back(cols[k]);
        jData.push_back(data[k]);
      }
      jRow["colIndices"] = jIdx;
      jRow["colData"]    = jData;
      jRows.push_back(jRow);
    }
    jMtx["sparseRows"] = jRows;
    matrices.push_back(jMtx);
  }
  j["matrices"] = matrices;
  return true;
}

bool CIccTagJsonSparseMatrixArray::ParseJson(const IccJson &j, std::string &parseStr)
{
  if (!j.contains("outputChannels") || !j.contains("matrixType") || !j.contains("matrices")) {
    parseStr += "Missing outputChannels, matrixType, or matrices in SparseMatrixArray\n";
    return false;
  }

  icUInt16Number nChannels = (icUInt16Number)j["outputChannels"].get<int>();
  icSparseMatrixType nMatrixType = (icSparseMatrixType)j["matrixType"].get<int>();
  const IccJson &jMats = j["matrices"];

  if (!jMats.is_array()) {
    parseStr += "matrices must be a JSON array\n";
    return false;
  }

  icUInt32Number n = icJsonSafeU32(jMats.size());
  if (!Reset(n, nChannels)) {
    parseStr += "Failed to reset SparseMatrixArray\n";
    return false;
  }
  m_nMatrixType = nMatrixType;

  icUInt32Number bytesPerMatrix = GetBytesPerMatrix();
  CIccSparseMatrix mtx;

  for (icUInt32Number i = 0; i < n; i++) {
    const IccJson &jMtx = jMats[i];
    if (!jMtx.contains("rows") || !jMtx.contains("cols") || !jMtx.contains("sparseRows")) {
      parseStr += "SparseMatrix missing rows, cols, or sparseRows\n";
      return false;
    }

    icUInt16Number nRows = (icUInt16Number)jMtx["rows"].get<int>();
    icUInt16Number nCols = (icUInt16Number)jMtx["cols"].get<int>();

    mtx.Reset(m_RawData + i * bytesPerMatrix, bytesPerMatrix, icSparseMatrixFloatNum, false);
    mtx.Init(nRows, nCols, true);

    icUInt16Number *rowstart  = mtx.GetRowStart();
    icUInt32Number  nMaxEntries = mtx.GetMaxEntries();
    const IccJson  &jRows = jMtx["sparseRows"];
    icUInt32Number  pos = 0;

    for (int r = 0; r < (int)nRows && r < (int)jRows.size(); r++) {
      const IccJson &jRow = jRows[r];
      rowstart[r] = (icUInt16Number)pos;

      if (!jRow.contains("colIndices") || !jRow.contains("colData")) continue;

      const IccJson &jIdx  = jRow["colIndices"];
      const IccJson &jData = jRow["colData"];

      if (!jIdx.is_array() || !jData.is_array() || jIdx.size() != jData.size()) {
        parseStr += "SparseRow colIndices/colData mismatch or not arrays\n";
        return false;
      }

      icUInt32Number cnt = icJsonSafeU32(jIdx.size());
      if (pos + cnt > nMaxEntries) {
        parseStr += "Exceeded maximum number of sparse matrix entries\n";
        return false;
      }

      icUInt16Number *dstCols = mtx.GetColumnsForRow(r);
      icFloatNumber  *dstData = (icFloatNumber*)mtx.GetData()->getPtr(pos);
      for (icUInt32Number k = 0; k < cnt; k++) {
        dstCols[k] = jIdx[k].get<icUInt16Number>();
        dstData[k] = jData[k].get<icFloatNumber>();
      }
      pos += cnt;
    }
    // fill remaining row-start sentinels (including end sentinel at nRows)
    for (int r = (int)jRows.size(); r <= (int)nRows; r++)
      rowstart[r] = (icUInt16Number)pos;
  }
  return true;
}

// ===========================================================================
// CIccTagJsonFixedNum template
// ===========================================================================

template <class T, icTagTypeSignature Tsig>
const char* CIccTagJsonFixedNum<T, Tsig>::GetClassName() const
{
  if (Tsig == icSigS15Fixed16ArrayType) return "CIccTagJsonS15Fixed16";
  return "CIccTagJsonU16Fixed16";
}

template <class T, icTagTypeSignature Tsig>
bool CIccTagJsonFixedNum<T, Tsig>::ToJson(IccJson &j)
{
  IccJson arr = IccJson::array();
  for (icUInt32Number i = 0; i < this->m_nSize; i++)
    arr.push_back(icFtoD(this->m_Num[i]));
  j["values"] = arr;
  return true;
}

template <class T, icTagTypeSignature Tsig>
bool CIccTagJsonFixedNum<T, Tsig>::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (jsonExistsField(j, "values") && j["values"].is_array()) {
    const IccJson &arr = j["values"];
    if (!this->SetSize(icJsonSafeU32(arr.size()))) return false;
    for (size_t i = 0; i < arr.size(); i++)
      this->m_Num[i] = icDtoF(arr[i].get<double>());
  }
  return true;
}

template class CIccTagJsonFixedNum<icS15Fixed16Number, icSigS15Fixed16ArrayType>;

// ===========================================================================
// CIccTagJsonNum template
// ===========================================================================

template <class T, class A, icTagTypeSignature Tsig>
const char* CIccTagJsonNum<T, A, Tsig>::GetClassName() const
{
  if (Tsig == icSigUInt8ArrayType)  return "CIccTagJsonUInt8";
  if (Tsig == icSigUInt16ArrayType) return "CIccTagJsonUInt16";
  if (Tsig == icSigUInt32ArrayType) return "CIccTagJsonUInt32";
  return "CIccTagJsonUInt64";
}

template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagJsonNum<T, A, Tsig>::ToJson(IccJson &j)
{
  IccJson arr = IccJson::array();
  for (icUInt32Number i = 0; i < this->m_nSize; i++)
    arr.push_back(this->m_Num[i]);
  j["values"] = arr;
  return true;
}

template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagJsonNum<T, A, Tsig>::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (jsonExistsField(j, "values") && j["values"].is_array()) {
    const IccJson &arr = j["values"];
    if (!this->SetSize(icJsonSafeU32(arr.size()))) return false;
    for (size_t i = 0; i < arr.size(); i++)
      this->m_Num[i] = arr[i].get<T>();
  }
  return true;
}

template class CIccTagJsonNum<icUInt8Number,  CIccJsonArrayType<icUInt8Number,  icSigUInt8ArrayType>,  icSigUInt8ArrayType>;
template class CIccTagJsonNum<icUInt16Number, CIccJsonArrayType<icUInt16Number, icSigUInt16ArrayType>, icSigUInt16ArrayType>;
template class CIccTagJsonNum<icUInt32Number, CIccJsonArrayType<icUInt32Number, icSigUInt32ArrayType>, icSigUInt32ArrayType>;
template class CIccTagJsonNum<icUInt64Number, CIccJsonArrayType<icUInt64Number, icSigUInt64ArrayType>, icSigUInt64ArrayType>;

// ===========================================================================
// CIccTagJsonFloatNum template
// ===========================================================================

template <class T, class A, icTagTypeSignature Tsig>
const char* CIccTagJsonFloatNum<T, A, Tsig>::GetClassName() const
{
  if (Tsig == icSigFloat16ArrayType) return "CIccTagJsonFloat16";
  if (Tsig == icSigFloat32ArrayType) return "CIccTagJsonFloat32";
  return "CIccTagJsonFloat64";
}

template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagJsonFloatNum<T, A, Tsig>::ToJson(IccJson &j)
{
  IccJson arr = IccJson::array();
  for (icUInt32Number i = 0; i < this->m_nSize; i++)
    arr.push_back((double)this->m_Num[i]);
  j["values"] = arr;
  return true;
}

template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagJsonFloatNum<T, A, Tsig>::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (jsonExistsField(j, "values") && j["values"].is_array()) {
    const IccJson &arr = j["values"];
    if (!this->SetSize(icJsonSafeU32(arr.size()))) return false;
    for (size_t i = 0; i < arr.size(); i++)
      this->m_Num[i] = (T)arr[i].get<double>();
  }
  return true;
}

template class CIccTagJsonFloatNum<icFloat32Number, CIccJsonArrayType<icFloat32Number, icSigFloat32ArrayType>, icSigFloat16ArrayType>;
template class CIccTagJsonFloatNum<icFloat32Number, CIccJsonArrayType<icFloat32Number, icSigFloat32ArrayType>, icSigFloat32ArrayType>;
template class CIccTagJsonFloatNum<icFloat64Number, CIccJsonArrayType<icFloat64Number, icSigFloat64ArrayType>, icSigFloat64ArrayType>;

// ===========================================================================
// CIccTagJsonMultiLocalizedUnicode
// ===========================================================================

bool CIccTagJsonMultiLocalizedUnicode::ToJson(IccJson &j)
{
  IccJson locales = IccJson::array();
  if (m_Strings) {
    CIccMultiLocalizedUnicode::iterator i;
    for (i = m_Strings->begin(); i != m_Strings->end(); i++) {
      IccJson loc;
      char lang[3] = {0}, country[3] = {0};
      lang[0]    = (char)(i->m_nLanguageCode >> 8);
      lang[1]    = (char)(i->m_nLanguageCode & 0xFF);
      country[0] = (char)(i->m_nCountryCode >> 8);
      country[1] = (char)(i->m_nCountryCode & 0xFF);
      loc["language"] = lang;
      loc["country"]  = country;
      std::string text;
      if (i->GetBuf() && i->GetLength() > 0)
        icUtf16ToUtf8(text, i->GetBuf(), (int)i->GetLength());
      loc["text"] = text;
      locales.push_back(loc);
    }
  }
  j["localizedStrings"] = locales;
  return true;
}

bool CIccTagJsonMultiLocalizedUnicode::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (!jsonExistsField(j, "localizedStrings") || !j["localizedStrings"].is_array())
    return false;
  for (const auto &loc : j["localizedStrings"]) {
    std::string lang = "  ", country = "  ", text;
    jGetString(loc, "language", lang);
    jGetString(loc, "country",  country);
    jGetString(loc, "text",     text);

    while (lang.size()    < 2) lang    += ' ';
    while (country.size() < 2) country += ' ';

    icLanguageCode  langCode = (icLanguageCode) ((lang[0] << 8)    | lang[1]);
    icCountryCode countryCode= (icCountryCode) ((country[0] << 8) | country[1]);

    CIccUTF16String wstr(text.c_str());
    SetText(wstr.c_str(), langCode, countryCode);
  }
  return true;
}

// ===========================================================================
// CIccTagJsonTagData
// ===========================================================================

bool CIccTagJsonTagData::ToJson(IccJson &j)
{
  j["dataFlag"] = (int)m_nDataFlag;
  j["data"]     = icJsonDumpHexData(m_pData, m_nSize);
  return true;
}

bool CIccTagJsonTagData::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  int dataFlag = 0;
  if (jGetValue(j, "dataFlag", dataFlag))
    m_nDataFlag = (icDataBlockType)dataFlag;
  std::string hex;
  if (jGetString(j, "data", hex)) {
    icUInt32Number sz = icJsonGetHexDataSize(hex.c_str());
    if (!SetSize(sz)) return false;
    icJsonGetHexData(m_pData, hex.c_str(), sz);
  }
  return true;
}

// ===========================================================================
// CIccTagJsonProfileSeqDesc helpers
// ===========================================================================

static bool icProfDescToJson(IccJson &jDesc, const CIccProfileDescStruct &p)
{
  const size_t bufSize = 16;
  char buf[bufSize];

  jDesc["deviceManufacturerSignature"] = icGetSigStr(buf, bufSize, p.m_deviceMfg);
  jDesc["deviceModelSignature"]        = icGetSigStr(buf, bufSize, p.m_deviceModel);
  jDesc["deviceAttributes"]            = icJsonGetDeviceAttr(p.m_attributes);
  jDesc["technology"]                  = icGetSigStr(buf, bufSize, p.m_technology);

  auto serializeDescText = [](IccJson &jField, const CIccProfileDescText &descText) -> bool {
    CIccTag *pTag = descText.GetTag();
    if (!pTag)
      return true; // optional
    IIccExtensionTag *pExt = pTag->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0)
      return false;
    CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);
    IccJson tagData;
    if (!pJsonTag->ToJson(tagData))
      return false;
    const icChar *typeName = CIccTagCreator::GetTagTypeSigName(pTag->GetType());
    if (!typeName) typeName = "PrivateType";
    jField["type"] = typeName;
    jField.update(tagData);
    return true;
  };

  IccJson jMfgDesc, jModDesc;
  if (!serializeDescText(jMfgDesc, p.m_deviceMfgDesc))   return false;
  if (!serializeDescText(jModDesc, p.m_deviceModelDesc))  return false;
  if (!jMfgDesc.empty()) jDesc["deviceManufacturer"] = jMfgDesc;
  if (!jModDesc.empty()) jDesc["deviceModel"]         = jModDesc;

  return true;
}

static bool icJsonParseProfDesc(const IccJson &jDesc, CIccProfileDescStruct &p, std::string &parseStr)
{
  std::string sig;
  jGetString(jDesc, "deviceManufacturerSignature", sig);
  p.m_deviceMfg = sig.empty() ? 0 : (icSignature)icGetSigVal(sig.c_str());

  sig.clear();
  jGetString(jDesc, "deviceModelSignature", sig);
  p.m_deviceModel = sig.empty() ? 0 : (icSignature)icGetSigVal(sig.c_str());

  if (jsonExistsField(jDesc, "deviceAttributes") && jDesc["deviceAttributes"].is_object())
    p.m_attributes = icJsonParseDeviceAttr(jDesc["deviceAttributes"]);
  else
    p.m_attributes = 0;

  sig.clear();
  jGetString(jDesc, "technology", sig);
  p.m_technology = sig.empty() ? (icTechnologySignature)0
                               : (icTechnologySignature)icGetSigVal(sig.c_str());

  auto parseDescText = [&parseStr](const IccJson &jField, CIccProfileDescText &descText) -> bool {
    if (!jField.is_object() || jField.empty())
      return true;
    std::string typeName;
    jGetString(jField, "type", typeName);
    icTagTypeSignature typeSig = CIccTagCreator::GetTagTypeNameSig(typeName.c_str());
    if (typeSig == icSigUnknownType)
      return false;
    if (!descText.SetType(typeSig))
      return false;
    CIccTag *pTag = descText.GetTag();
    if (!pTag)
      return false;
    IIccExtensionTag *pExt = pTag->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0)
      return false;
    return static_cast<CIccTagJson*>(pExt)->ParseJson(jField, parseStr);
  };

  if (jsonExistsField(jDesc, "deviceManufacturer") && jDesc["deviceManufacturer"].is_object())
    if (!parseDescText(jDesc["deviceManufacturer"], p.m_deviceMfgDesc)) return false;

  if (jsonExistsField(jDesc, "deviceModel") && jDesc["deviceModel"].is_object())
    if (!parseDescText(jDesc["deviceModel"], p.m_deviceModelDesc)) return false;

  return true;
}

// ===========================================================================
// CIccTagJsonProfileSeqDesc
// ===========================================================================

bool CIccTagJsonProfileSeqDesc::ToJson(IccJson &j)
{
  if (!m_Descriptions)
    return false;

  IccJson seq = IccJson::array();
  for (auto &desc : *m_Descriptions) {
    IccJson jDesc;
    if (!icProfDescToJson(jDesc, desc))
      return false;
    seq.push_back(jDesc);
  }
  j["profileSequence"] = seq;
  return true;
}

bool CIccTagJsonProfileSeqDesc::ParseJson(const IccJson &j, std::string &parseStr)
{
  if (!m_Descriptions)
    return false;

  m_Descriptions->clear();

  if (!jsonExistsField(j, "profileSequence") || !j["profileSequence"].is_array())
    return true; // empty sequence is valid

  for (const IccJson &jDesc : j["profileSequence"]) {
    CIccProfileDescStruct desc;
    if (!icJsonParseProfDesc(jDesc, desc, parseStr))
      return false;
    m_Descriptions->push_back(desc);
  }
  return true;
}

// ===========================================================================
// CIccTagJsonProfileSequenceId
// ===========================================================================

bool CIccTagJsonProfileSequenceId::ToJson(IccJson &j)
{
  if (!m_list)
    return false;

  IccJson seq = IccJson::array();
  for (auto &pid : *m_list) {
    IccJson entry;

    // 16-byte profile ID serialized as 32-character uppercase hex string
    char buf[33];
    for (int n = 0; n < 16; n++)
      snprintf(buf + n * 2, 33 - n * 2, "%02X", pid.m_profileID.ID8[n]);
    buf[32] = '\0';
    entry["profileId"] = buf;

    entry["localizedStrings"] = dictLocalizedToJson(&pid.m_desc);

    seq.push_back(entry);
  }
  j["profileSequenceId"] = seq;
  return true;
}

bool CIccTagJsonProfileSequenceId::ParseJson(const IccJson &j, std::string & /*parseStr*/)
{
  if (!m_list)
    return false;

  m_list->clear();

  if (!jsonExistsField(j, "profileSequenceId") || !j["profileSequenceId"].is_array())
    return true;

  for (const IccJson &entry : j["profileSequenceId"]) {
    CIccProfileIdDesc desc;

    std::string idHex;
    if (jGetString(entry, "profileId", idHex) && !idHex.empty())
      icJsonGetHexData(&desc.m_profileID, idHex.c_str(), sizeof(desc.m_profileID));

    if (jsonExistsField(entry, "localizedStrings") && entry["localizedStrings"].is_array()) {
      for (const IccJson &loc : entry["localizedStrings"]) {
        std::string lang = "  ", country = "  ", text;
        jGetString(loc, "language", lang);
        jGetString(loc, "country",  country);
        jGetString(loc, "text",     text);

        while (lang.size()    < 2) lang    += ' ';
        while (country.size() < 2) country += ' ';

        icLanguageCode langCode    = (icLanguageCode)((lang[0]    << 8) | lang[1]);
        icCountryCode  countryCode = (icCountryCode) ((country[0] << 8) | country[1]);

        CIccUTF16String wstr(text.c_str());
        desc.m_desc.SetText(wstr.c_str(), langCode, countryCode);
      }
    }

    m_list->push_back(desc);
  }
  return true;
}

// ===========================================================================
// CIccTagJsonResponseCurveSet16
// ===========================================================================

bool CIccTagJsonResponseCurveSet16::ToJson(IccJson &j)
{
  CIccInfo info;
  j["CountOfChannels"] = (int)m_nChannels;

  IccJson curves = IccJson::array();
  CIccResponseCurveStruct *pCurve = GetFirstCurves();
  while (pCurve) {
    IccJson jCurve;
    jCurve["MeasurementUnit"] = info.GetMeasurementUnit((icSignature)pCurve->GetMeasurementType());

    IccJson channels = IccJson::array();
    for (icUInt16Number i = 0; i < pCurve->GetNumChannels(); i++) {
      IccJson jChan;
      icXYZNumber *pXYZ = pCurve->GetXYZ(i);
      jChan["MaxColorantXYZ"] = IccJson::array({ icFtoD(pXYZ->X), icFtoD(pXYZ->Y), icFtoD(pXYZ->Z) });

      IccJson measurements = IccJson::array();
      CIccResponse16List *pList = pCurve->GetResponseList(i);
      for (auto m = pList->begin(); m != pList->end(); ++m) {
        IccJson jm;
        jm["DeviceCode"] = (int)m->deviceCode;
        jm["MeasValue"]  = icFtoD(m->measurementValue);
        if (m->reserved)
          jm["Reserved"] = (int)m->reserved;
        measurements.push_back(jm);
      }
      jChan["Measurements"] = measurements;
      channels.push_back(jChan);
    }
    jCurve["Channels"] = channels;
    curves.push_back(jCurve);
    pCurve = GetNextCurves();
  }
  j["ResponseCurves"] = curves;
  return true;
}

bool CIccTagJsonResponseCurveSet16::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nChannels = 0;
  if (!jGetValue(j, "CountOfChannels", nChannels) || nChannels <= 0) {
    parseStr += "Missing or invalid CountOfChannels in ResponseCurveSet16\n";
    return false;
  }
  SetNumChannels((icUInt16Number)nChannels);

  if (!m_ResponseCurves)
    return false;
  m_ResponseCurves->clear();

  if (!jsonExistsField(j, "ResponseCurves") || !j["ResponseCurves"].is_array())
    return true;

  for (const IccJson &jCurve : j["ResponseCurves"]) {
    std::string measUnit;
    jGetString(jCurve, "MeasurementUnit", measUnit);
    icMeasurementUnitSig sig = measUnit.empty() ? icSigStatusA
                                                 : icGetMeasurementValue(measUnit.c_str());

    CIccResponseCurveStruct curves(sig, (icUInt16Number)nChannels);

    if (!jsonExistsField(jCurve, "Channels") || !jCurve["Channels"].is_array()) {
      parseStr += "Missing Channels in ResponseCurve entry\n";
      return false;
    }

    int i = 0;
    for (const IccJson &jChan : jCurve["Channels"]) {
      if (i >= nChannels) break;

      icXYZNumber *pXYZ = curves.GetXYZ(i);
      pXYZ->X = pXYZ->Y = pXYZ->Z = 0;
      if (jsonExistsField(jChan, "MaxColorantXYZ") && jChan["MaxColorantXYZ"].is_array()
          && jChan["MaxColorantXYZ"].size() >= 3) {
        pXYZ->X = icDtoF((icFloatNumber)jChan["MaxColorantXYZ"][0].get<double>());
        pXYZ->Y = icDtoF((icFloatNumber)jChan["MaxColorantXYZ"][1].get<double>());
        pXYZ->Z = icDtoF((icFloatNumber)jChan["MaxColorantXYZ"][2].get<double>());
      }

      CIccResponse16List *pList = curves.GetResponseList(i);
      if (jsonExistsField(jChan, "Measurements") && jChan["Measurements"].is_array()) {
        for (const IccJson &jm : jChan["Measurements"]) {
          icResponse16Number resp = {};
          int dc = 0, res = 0;
          double mv = 0.0;
          jGetValue(jm, "DeviceCode", dc);
          jGetValue(jm, "MeasValue",  mv);
          jGetValue(jm, "Reserved",   res);
          resp.deviceCode       = (icUInt16Number)dc;
          resp.measurementValue = icDtoF((icFloatNumber)mv);
          resp.reserved         = (icUInt16Number)res;
          pList->push_back(resp);
        }
      }
      i++;
    }
    m_ResponseCurves->push_back(curves);
  }
  return true;
}

// ===========================================================================
// Curve tags
// ===========================================================================

bool CIccTagJsonCurve::ToJson(IccJson &j)
{
  return ToJson(j, icConvertFloat);
}

bool CIccTagJsonCurve::ToJson(IccJson &j, icConvertType nType)
{
  if (m_nSize == 0) {
    j["curveType"] = "identity";
  } else if (m_nSize == 1) {
    j["curveType"] = "gamma";
    j["gamma"] = (double)(m_Curve[0] * 65535.0 / 256.0);
  } else {
    // Check whether the table is a sampled identity (linear ramp 0..1).
    // Tolerance of 0.5/65535 covers 16-bit quantisation rounding.
    bool isIdentity = true;
    const icFloatNumber tol = 0.5f / 65535.0f;
    for (icUInt32Number i = 0; i < m_nSize && isIdentity; i++) {
      icFloatNumber expected = (icFloatNumber)i / (icFloatNumber)(m_nSize - 1);
      if (fabsf(m_Curve[i] - expected) > tol)
        isIdentity = false;
    }
    if (isIdentity) {
      j["curveType"] = "identity";
      j["size"] = (int)m_nSize;
    } else {
      j["curveType"] = "table";
      IccJson arr = IccJson::array();
      for (icUInt32Number i = 0; i < m_nSize; i++) {
        switch (nType) {
          case icConvert8Bit:
            arr.push_back((int)(m_Curve[i] * 255.0f + 0.5f)); break;
          case icConvert16Bit:
          case icConvertVariable:
            arr.push_back((int)(m_Curve[i] * 65535.0f + 0.5f)); break;
          default:
            arr.push_back((double)m_Curve[i]); break;
        }
      }
      if (nType == icConvert8Bit)
        j["precision"] = 1;
      else if (nType == icConvert16Bit || nType == icConvertVariable)
        j["precision"] = 2;
      j["table"] = arr;
    }
  }
  return true;
}

bool CIccTagJsonCurve::ParseJson(const IccJson &j, std::string &parseStr)
{
  return ParseJson(j, icConvertFloat, parseStr);
}

bool CIccTagJsonCurve::ParseJson(const IccJson &j, icConvertType /*nType*/, std::string & /*parseStr*/)
{
  std::string curveType;
  if (!jGetString(j, "curveType", curveType)) return false;
  if (curveType == "identity") {
    int size = 0;
    jGetValue(j, "size", size);
    if (size >= 2) {
      if (!SetSize((icUInt32Number)size)) return false;
      for (int i = 0; i < size; i++)
        m_Curve[i] = (icFloatNumber)i / (icFloatNumber)(size - 1);
    } else {
      SetSize(0);
    }
  } else if (curveType == "gamma") {
    SetSize(1);
    double gamma = 1.0;
    jGetValue(j, "gamma", gamma);
    SetGamma((icFloatNumber)gamma);
  } else {
    if (!jsonExistsField(j, "table") || !j["table"].is_array()) return false;
    const IccJson &arr = j["table"];
    if (!SetSize(icJsonSafeU32(arr.size()))) return false;
    int precision = 0;
    jGetValue(j, "precision", precision);
    for (size_t i = 0; i < arr.size(); i++) {
      if (precision == 1)
        m_Curve[i] = (icFloatNumber)arr[i].get<int>() / 255.0f;
      else if (precision == 2)
        m_Curve[i] = (icFloatNumber)arr[i].get<int>() / 65535.0f;
      else
        m_Curve[i] = (icFloatNumber)arr[i].get<double>();
    }
  }
  return true;
}

// ---------------------------------------------------------------------------

bool CIccTagJsonParametricCurve::ToJson(IccJson &j)
{
  return ToJson(j, icConvertFloat);
}

bool CIccTagJsonParametricCurve::ToJson(IccJson &j, icConvertType /*nType*/)
{
  j["functionType"] = (int)m_nFunctionType;
  IccJson params = IccJson::array();
  for (icUInt16Number i = 0; i < m_nNumParam; i++)
    params.push_back((double)m_dParam[i]);
  j["params"] = params;
  return true;
}

bool CIccTagJsonParametricCurve::ParseJson(const IccJson &j, std::string &parseStr)
{
  return ParseJson(j, icConvertFloat, parseStr);
}

bool CIccTagJsonParametricCurve::ParseJson(const IccJson &j, icConvertType /*nType*/, std::string & /*parseStr*/)
{
  int funcType = 0;
  if (!jGetValue(j, "functionType", funcType)) return false;
  icUInt8Number nParam = 0;
  // Determine param count from function type per ICC spec
  switch (funcType) { case 0: nParam=1; break; case 1: nParam=3; break; case 2: nParam=4; break; case 3: nParam=5; break; case 4: nParam=7; break; default: nParam=0; }
  if (!SetFunctionType((icUInt16Number)funcType)) return false;
  if (jsonExistsField(j, "params") && j["params"].is_array()) {
    const IccJson &params = j["params"];
    for (icUInt8Number i = 0; i < nParam && i < (icUInt8Number)params.size(); i++)
      m_dParam[i] = (icFloatNumber)params[i].get<double>();
  }
  return true;
}

// ---------------------------------------------------------------------------

bool CIccTagJsonSegmentedCurve::ToJson(IccJson &j)
{
  return ToJson(j, icConvertFloat);
}

bool CIccTagJsonSegmentedCurve::ToJson(IccJson &j, icConvertType /*nType*/)
{
  if (!m_pCurve)
    return true;
  CIccSegmentedCurveJson jsonCurve(m_pCurve);
  return jsonCurve.ToJson(j);
}

bool CIccTagJsonSegmentedCurve::ParseJson(const IccJson &j, std::string &parseStr)
{
  return ParseJson(j, icConvertFloat, parseStr);
}

bool CIccTagJsonSegmentedCurve::ParseJson(const IccJson &j, icConvertType /*nType*/, std::string &parseStr)
{
  CIccSegmentedCurveJson *pCurve = new(std::nothrow) CIccSegmentedCurveJson();
  if (!pCurve) return false;
  if (!pCurve->ParseJson(j, parseStr)) {
    delete pCurve;
    return false;
  }
  SetCurve(pCurve);
  return true;
}

// ===========================================================================
// MBB (LUT tag) JSON helpers
// ===========================================================================

// Curve type name for JSON "type" discriminator
static const char *icJsonCurveTypeName(icTagTypeSignature sig)
{
  switch (sig) {
    case icSigCurveType:           return "Curve";
    case icSigParametricCurveType: return "ParametricCurve";
    case icSigSegmentedCurveType:  return "SegmentedCurve";
    default:                        return nullptr;
  }
}

// Serialize a curve array into a JSON array
static bool icMBBCurvesToJson(IccJson &arr, LPIccCurve *pCurves, int nChannels, icConvertType nType)
{
  arr = IccJson::array();
  for (int i = 0; i < nChannels; i++) {
    CIccCurve *pCurve = pCurves[i];
    if (!pCurve) return false;
    IIccExtensionTag *pExt = pCurve->GetExtension();
    if (!pExt || strcmp(pExt->GetExtDerivedClassName(), "CIccCurveJson") != 0)
      return false;
    CIccCurveJson *pCurveJson = (CIccCurveJson *)pExt;
    IccJson jCurve;
    const char *typeName = icJsonCurveTypeName(pCurve->GetType());
    if (!typeName) return false;
    jCurve["type"] = typeName;
    if (!pCurveJson->ToJson(jCurve, nType))
      return false;
    arr.push_back(jCurve);
  }
  return true;
}

// Deserialize a JSON array into a curve array
static bool icMBBCurvesFromJson(LPIccCurve *pCurves, int nChannels,
                                 const IccJson &arr, icConvertType nType, std::string &parseStr)
{
  if (!arr.is_array() || (int)arr.size() != nChannels) {
    parseStr += "Curve array length does not match channel count\n";
    return false;
  }
  for (int i = 0; i < nChannels; i++) {
    const IccJson &jCurve = arr[i];
    std::string type;
    jGetString(jCurve, "type", type);

    CIccCurve     *pTag       = nullptr;
    CIccCurveJson *pCurveJson = nullptr;
    if (type == "Curve") {
      auto *p = new(std::nothrow) CIccTagJsonCurve();          pTag = p; pCurveJson = p;
    } else if (type == "ParametricCurve") {
      auto *p = new(std::nothrow) CIccTagJsonParametricCurve(); pTag = p; pCurveJson = p;
    } else if (type == "SegmentedCurve") {
      auto *p = new(std::nothrow) CIccTagJsonSegmentedCurve();  pTag = p; pCurveJson = p;
    } else {
      parseStr += "Unknown curve type: " + type + "\n";
      return false;
    }
    if (!pTag) { parseStr += "Allocation failed for curve\n"; return false; }
    if (!pCurveJson->ParseJson(jCurve, nType, parseStr)) {
      parseStr += "Failed to parse curve " + std::to_string(i) + "\n";
      delete pTag;
      return false;
    }
    pCurves[i] = pTag;
  }
  return true;
}

// Serialize a CIccMatrix into j["matrix"]
static void icMBBMatrixToJson(IccJson &j, CIccMatrix *pMatrix)
{
  IccJson e = IccJson::array();
  int n = pMatrix->m_bUseConstants ? 12 : 9;
  for (int i = 0; i < n; i++)
    e.push_back((double)pMatrix->m_e[i]);
  j["matrix"]["e"] = e;
}

// Deserialize j["matrix"]["e"] into a CIccMatrix
static bool icMBBMatrixFromJson(CIccMatrix *pMatrix, const IccJson &j)
{
  memset(pMatrix->m_e, 0, sizeof(pMatrix->m_e));
  pMatrix->m_bUseConstants = false;
  if (!jsonExistsField(j, "e") || !j["e"].is_array()) return false;
  const IccJson &e = j["e"];
  int n = std::min((int)e.size(), 12);
  for (int i = 0; i < n; i++)
    pMatrix->m_e[i] = (icFloatNumber)e[i].get<double>();
  if (n > 9)
    pMatrix->m_bUseConstants = true;
  return true;
}

// Parse whitespace/comma-separated numbers from a text buffer into a CLUT.
// Returns the number of values read.
static icUInt32Number icParseCLUTTextBuf(const char *buf, size_t bufLen,
                                          icFloatNumber *dst, icUInt32Number maxVals,
                                          icConvertType nType)
{
  const char *p   = buf;
  const char *end = buf + bufLen;
  icUInt32Number n = 0;
  while (p < end && n < maxVals) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ','))
      p++;
    if (p >= end) break;
    char *next = nullptr;
    double v = strtod(p, &next);
    if (!next || next == p) break;
    switch (nType) {
      case icConvert8Bit:  dst[n++] = (icFloatNumber)(v / 255.0);   break;
      case icConvert16Bit: dst[n++] = (icFloatNumber)(v / 65535.0); break;
      default:             dst[n++] = (icFloatNumber)v;              break;
    }
    p = next;
  }
  return n;
}

// Construct and initialize a CIccCLUT from a JSON object.
// Supports inline "data" array or external "file" (text or binary).
//
// External file JSON keys:
//   "file"     : path to data file (triggers file-based loading)
//   "format"   : "text" (default) or "binary"
//   "encoding" : "int8", "int16", or "float" (default: matches nType)
//   "endian"   : "little" or "big" (binary only; default "big")
CIccCLUT *icCLUTFromJson(const IccJson &j, int nIn, int nOut,
                          icConvertType nType, std::string &parseStr)
{
  // Resolve variable type from stored precision field (mirrors icConvertVariable in XML)
  if (nType == icConvertVariable) {
    int prec = 2;
    jGetValue(j, "precision", prec);
    nType = (prec == 1) ? icConvert8Bit : icConvert16Bit;
  }
  int nPrecision = (nType == icConvert8Bit) ? 1 : 2;
  CIccCLUT *pCLUT = new(std::nothrow) CIccCLUT((icUInt8Number)nIn, (icUInt16Number)nOut, nPrecision);
  if (!pCLUT) return nullptr;

  // Initialize grid dimensions
  if (jsonExistsField(j, "gridPoints") && j["gridPoints"].is_array()) {
    const IccJson &gp = j["gridPoints"];
    icUInt8Number pts[16] = {};
    int cnt = std::min((int)gp.size(), nIn);
    for (int i = 0; i < cnt; i++)
      pts[i] = (icUInt8Number)gp[i].get<int>();
    if (!pCLUT->Init(pts)) {
      parseStr += "Failed to initialize CLUT from gridPoints array\n";
      delete pCLUT; return nullptr;
    }
  } else if (jsonExistsField(j, "gridPoints") && j["gridPoints"].is_number()) {
    icUInt8Number gran = (icUInt8Number)j["gridPoints"].get<int>();
    if (!pCLUT->Init(gran)) {
      parseStr += "Failed to initialize CLUT from gridPoints scalar\n";
      delete pCLUT; return nullptr;
    }
  } else {
    parseStr += "Missing gridPoints in CLUT\n";
    delete pCLUT; return nullptr;
  }

  icUInt64Number nTotal64 = (icUInt64Number)pCLUT->NumPoints() * (icUInt64Number)nOut;
  if (nTotal64 > 0xFFFFFFFFULL) {
    parseStr += "CLUT size overflow\n";
    delete pCLUT; return nullptr;
  }
  icUInt32Number nTotal = (icUInt32Number)nTotal64;
  icFloatNumber *pData  = pCLUT->GetData(0);
  if (!pData) { delete pCLUT; return nullptr; }

  // -----------------------------------------------------------------------
  // External file
  // -----------------------------------------------------------------------
  if (jsonExistsField(j, "file") && j["file"].is_string()) {
    std::string filename = j["file"].get<std::string>();
    std::string format   = "text";
    std::string encoding;
    std::string endian   = "big";
    jGetString(j, "format",   format);
    jGetString(j, "encoding", encoding);
    jGetString(j, "endian",   endian);

    // Resolve encoding -> effective convert type
    icConvertType fileType = nType;
    if      (encoding == "int8")  fileType = icConvert8Bit;
    else if (encoding == "int16") fileType = icConvert16Bit;
    else if (encoding == "float") fileType = icConvertFloat;

    CIccIO *file = IccJsonOpenFileIO(filename.c_str(), "rb");
    if (!file) {
      parseStr += "Error! - File '" + filename + "' not found.\n";
      delete pCLUT; return nullptr;
    }

    bool ok = false;

    if (format == "text") {
      size_t num = file->GetLength();
      char *buf = (char *)malloc(num + 1);
      if (!buf) {
        parseStr += "Memory error reading '" + filename + "'\n";
        delete file; delete pCLUT; return nullptr;
      }
      if (file->Read8(buf, num) != num) {
        parseStr += "Read error on '" + filename + "'\n";
        free(buf); delete file; delete pCLUT; return nullptr;
      }
      buf[num] = '\0';
      icUInt32Number n = icParseCLUTTextBuf(buf, num, pData, nTotal, fileType);
      free(buf);
      if (n != nTotal) {
        parseStr += "Error! - Entry count in '" + filename + "' does not match CLUT size.\n";
        delete file; delete pCLUT; return nullptr;
      }
      ok = true;
    }
    else if (format == "binary") {
      bool little_endian = (endian == "little");

      if (fileType == icConvert8Bit) {
        size_t num = file->GetLength();
        if (num != nTotal) {
          parseStr += "Error! - Binary file '" + filename + "' size does not match CLUT.\n";
          delete file; delete pCLUT; return nullptr;
        }
        icUInt8Number v;
        for (icUInt32Number k = 0; k < nTotal; k++) {
          if (!file->Read8(&v)) {
            parseStr += "Read error on '" + filename + "'\n";
            delete file; delete pCLUT; return nullptr;
          }
          pData[k] = (icFloatNumber)v / 255.0f;
        }
        ok = true;
      }
      else if (fileType == icConvert16Bit) {
        size_t num = file->GetLength() / sizeof(icUInt16Number);
        if (num < nTotal) {
          parseStr += "Error! - Binary file '" + filename + "' too small for CLUT.\n";
          delete file; delete pCLUT; return nullptr;
        }
        icUInt16Number v;
        icUInt8Number *ptr = (icUInt8Number *)&v;
        for (icUInt32Number k = 0; k < nTotal; k++) {
          if (!file->Read16(&v)) {
            parseStr += "Read error on '" + filename + "'\n";
            delete file; delete pCLUT; return nullptr;
          }
#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
          if (little_endian) {
#else
          if (!little_endian) {
#endif
            icUInt8Number t = ptr[0]; ptr[0] = ptr[1]; ptr[1] = t;
          }
          pData[k] = (icFloatNumber)v / 65535.0f;
        }
        ok = true;
      }
      else { // float32
        size_t num = file->GetLength() / sizeof(icFloat32Number);
        if (num < nTotal) {
          parseStr += "Error! - Binary file '" + filename + "' too small for CLUT.\n";
          delete file; delete pCLUT; return nullptr;
        }
        icFloat32Number v;
        icUInt8Number *ptr = (icUInt8Number *)&v;
        for (icUInt32Number k = 0; k < nTotal; k++) {
          if (!file->ReadFloat32Float(&v)) {
            parseStr += "Read error on '" + filename + "'\n";
            delete file; delete pCLUT; return nullptr;
          }
#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
          if (little_endian) {
#else
          if (!little_endian) {
#endif
            icUInt8Number tmp;
            tmp = ptr[0]; ptr[0] = ptr[3]; ptr[3] = tmp;
            tmp = ptr[1]; ptr[1] = ptr[2]; ptr[2] = tmp;
          }
          pData[k] = v;
        }
        ok = true;
      }
    }
    else {
      parseStr += "Unknown CLUT file format '" + format + "' (expected 'text' or 'binary')\n";
    }

    delete file;
    if (!ok) { delete pCLUT; return nullptr; }
    return pCLUT;
  }

  // -----------------------------------------------------------------------
  // Inline data array
  // -----------------------------------------------------------------------
  if (!jsonExistsField(j, "data") || !j["data"].is_array()) {
    parseStr += "Missing 'data' or 'file' in CLUT\n";
    delete pCLUT; return nullptr;
  }
  const IccJson &data = j["data"];
  for (icUInt32Number k = 0; k < nTotal && k < icJsonSafeU32(data.size()); k++) {
    double v = data[k].get<double>();
    switch (nType) {
      case icConvert8Bit:  pData[k] = (icFloatNumber)(v / 255.0);   break;
      case icConvert16Bit: pData[k] = (icFloatNumber)(v / 65535.0); break;
      default:             pData[k] = (icFloatNumber)v;              break;
    }
  }
  return pCLUT;
}

// Serialize an MBB to JSON
static bool icMBBToJson(IccJson &j, CIccMBB *pMBB, icConvertType nType, bool bSaveGridPoints)
{
  j["inputChannels"]  = (int)pMBB->InputChannels();
  j["outputChannels"] = (int)pMBB->OutputChannels();

  int nIn  = pMBB->InputChannels();
  int nOut = pMBB->OutputChannels();

  auto emitCurves = [&](const char *name, LPIccCurve *pCurves, int n) -> bool {
    IccJson arr;
    if (!icMBBCurvesToJson(arr, pCurves, n, nType)) return false;
    j[name] = arr;
    return true;
  };

  if (pMBB->IsInputMatrix()) {
    if (pMBB->SwapMBCurves()) {
      if (pMBB->GetMatrix())   icMBBMatrixToJson(j, pMBB->GetMatrix());
      if (pMBB->GetCurvesB() && !emitCurves("bCurves", pMBB->GetCurvesM(), nIn)) return false;
    } else {
      if (pMBB->GetCurvesB() && !emitCurves("bCurves", pMBB->GetCurvesB(), nIn)) return false;
      if (pMBB->GetMatrix())   icMBBMatrixToJson(j, pMBB->GetMatrix());
      if (pMBB->GetCurvesM() && !emitCurves("mCurves", pMBB->GetCurvesM(), nIn)) return false;
    }
    if (pMBB->GetCLUT() && !icCLUTToJson(j, pMBB->GetCLUT(), nType, bSaveGridPoints, "clut")) return false;
    if (pMBB->GetCurvesA() && !emitCurves("aCurves", pMBB->GetCurvesA(), nOut)) return false;
  } else {
    if (pMBB->GetCurvesA() && !emitCurves("aCurves", pMBB->GetCurvesA(), nIn))  return false;
    if (pMBB->GetCLUT() && !icCLUTToJson(j, pMBB->GetCLUT(), nType, bSaveGridPoints, "clut")) return false;
    if (pMBB->GetCurvesM() && !emitCurves("mCurves", pMBB->GetCurvesM(), nOut)) return false;
    if (pMBB->GetMatrix())   icMBBMatrixToJson(j, pMBB->GetMatrix());
    if (pMBB->GetCurvesB() && !emitCurves("bCurves", pMBB->GetCurvesB(), nOut)) return false;
  }
  return true;
}

// Deserialize an MBB from JSON
static bool icMBBFromJson(CIccMBB *pMBB, const IccJson &j, icConvertType nType, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  if (!jGetValue(j, "inputChannels", nIn) || !jGetValue(j, "outputChannels", nOut)
      || nIn < 1 || nOut < 1 || nIn > 15 || nOut > 15) {
    parseStr += "Missing or invalid inputChannels/outputChannels in LUT\n";
    return false;
  }
  pMBB->Init((icUInt8Number)nIn, (icUInt8Number)nOut);

  // aCurves
  if (jsonExistsField(j, "aCurves") && j["aCurves"].is_array()) {
    int nCh = !pMBB->IsInputMatrix() ? nIn : nOut;
    if (!icMBBCurvesFromJson(pMBB->NewCurvesA(), nCh, j["aCurves"], nType, parseStr)) {
      parseStr += "Error parsing aCurves\n"; return false;
    }
  }
  // bCurves
  if (jsonExistsField(j, "bCurves") && j["bCurves"].is_array()) {
    int nCh = pMBB->IsInputMatrix() ? nIn : nOut;
    if (!icMBBCurvesFromJson(pMBB->NewCurvesB(), nCh, j["bCurves"], nType, parseStr)) {
      parseStr += "Error parsing bCurves\n"; return false;
    }
  }
  // mCurves
  if (jsonExistsField(j, "mCurves") && j["mCurves"].is_array()) {
    int nCh = pMBB->IsInputMatrix() ? nIn : nOut;
    if (!icMBBCurvesFromJson(pMBB->NewCurvesM(), nCh, j["mCurves"], nType, parseStr)) {
      parseStr += "Error parsing mCurves\n"; return false;
    }
  }
  // matrix
  if (jsonExistsField(j, "matrix") && j["matrix"].is_object()) {
    if (!icMBBMatrixFromJson(pMBB->NewMatrix(), j["matrix"])) {
      parseStr += "Error parsing matrix\n"; return false;
    }
  }
  // clut
  if (jsonExistsField(j, "clut") && j["clut"].is_object()) {
    CIccCLUT *pCLUT = icCLUTFromJson(j["clut"], nIn, nOut, nType, parseStr);
    if (!pCLUT || !pMBB->SetCLUT(pCLUT)) {
      delete pCLUT;
      parseStr += "Error parsing clut\n"; return false;
    }
  }
  return true;
}

// ===========================================================================
// LUT tags
// ===========================================================================

bool CIccTagJsonLutAtoB::ToJson(IccJson &j)
{
  return icMBBToJson(j, this, icConvertVariable, true);
}
bool CIccTagJsonLutAtoB::ParseJson(const IccJson &j, std::string &parseStr)
{
  return icMBBFromJson(this, j, icConvertVariable, parseStr);
}

bool CIccTagJsonLutBtoA::ToJson(IccJson &j)
{
  return icMBBToJson(j, this, icConvertVariable, true);
}
bool CIccTagJsonLutBtoA::ParseJson(const IccJson &j, std::string &parseStr)
{
  return icMBBFromJson(this, j, icConvertVariable, parseStr);
}

bool CIccTagJsonLut8::ToJson(IccJson &j)
{
  return icMBBToJson(j, this, icConvert8Bit, true);
}
bool CIccTagJsonLut8::ParseJson(const IccJson &j, std::string &parseStr)
{
  return icMBBFromJson(this, j, icConvert8Bit, parseStr);
}

bool CIccTagJsonLut16::ToJson(IccJson &j)
{
  return icMBBToJson(j, this, icConvert16Bit, true);
}
bool CIccTagJsonLut16::ParseJson(const IccJson &j, std::string &parseStr)
{
  return icMBBFromJson(this, j, icConvert16Bit, parseStr);
}

// ===========================================================================
// CIccTagJsonMultiProcessElement
// ===========================================================================

bool CIccTagJsonMultiProcessElement::ToJson(IccJson &j)
{
  j["inputChannels"]  = (int)m_nInputChannels;
  j["outputChannels"] = (int)m_nOutputChannels;

  IccJson elems = IccJson::array();
  if (m_list) {
    CIccMultiProcessElementList::iterator it;
    for (it = m_list->begin(); it != m_list->end(); it++) {
      CIccMultiProcessElement *pElem = it->ptr;
      IIccExtensionMpe *pExt = pElem ? pElem->GetExtension() : NULL;
      if (!pExt || strcmp(pExt->GetExtClassName(), "CIccMpeJson") != 0)
        continue;
      CIccMpeJson *pJsonElem = static_cast<CIccMpeJson*>(pExt);
      IccJson elemObj;
      elemObj["type"]           = icJsonGetElemTypeName(pElem->GetType());
      elemObj["inputChannels"]  = (int)pElem->NumInputChannels();
      elemObj["outputChannels"] = (int)pElem->NumOutputChannels();
      if (pElem->m_nReserved)
        elemObj["Reserved"] = (int)pElem->m_nReserved;
      pJsonElem->ToJson(elemObj);
      elems.push_back(elemObj);
    }
  }
  j["elements"] = elems;
  return true;
}

bool CIccTagJsonMultiProcessElement::ParseJson(const IccJson &j, std::string &parseStr)
{
  int nIn = 0, nOut = 0;
  jGetValue(j, "inputChannels",  nIn);
  jGetValue(j, "outputChannels", nOut);
  m_nInputChannels  = (icUInt16Number)nIn;
  m_nOutputChannels = (icUInt16Number)nOut;

  if (!jsonExistsField(j, "elements") || !j["elements"].is_array()) return true;

  for (const auto &elemObj : j["elements"]) {
    if (!elemObj.is_object() || elemObj.empty()) continue;
    std::string typeName;
    jGetString(elemObj, "type", typeName);
    if (typeName.empty()) continue;

    icElemTypeSignature sig = icJsonGetElemTypeNameSig(typeName.c_str());
    CIccMultiProcessElement *pElem = CIccMpeCreator::CreateElement(sig);
    if (!pElem) continue;

    int nReserved = 0;
    if (jGetValue(elemObj, "Reserved", nReserved))
      pElem->m_nReserved = (icUInt32Number)nReserved;

    IIccExtensionMpe *pExt = pElem->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccMpeJson") != 0) { delete pElem; continue; }

    CIccMpeJson *pJsonElem = static_cast<CIccMpeJson*>(pExt);
    pJsonElem->ParseJson(elemObj, parseStr);

    Attach(pElem);
  }
  return true;
}

CIccMultiProcessElement* CIccTagJsonMultiProcessElement::CreateElement(const icChar *szElementName)
{
  icElemTypeSignature sig = icJsonGetElemTypeNameSig(szElementName);
  return CIccMpeCreator::CreateElement(sig);
}

// ===========================================================================
// CIccTagJsonDict
// ===========================================================================

// Helper: serialize a CIccTagMultiLocalizedUnicode into a JSON array of
// { language, country, text } objects -- reuses the same shape as
// CIccTagJsonMultiLocalizedUnicode::ToJson.
static IccJson dictLocalizedToJson(const CIccTagMultiLocalizedUnicode *pTag)
{
  IccJson arr = IccJson::array();
  if (!pTag || !pTag->m_Strings)
    return arr;
  for (auto i = pTag->m_Strings->begin(); i != pTag->m_Strings->end(); ++i) {
    char lang[3] = {0}, country[3] = {0};
    lang[0]    = (char)(i->m_nLanguageCode >> 8);
    lang[1]    = (char)(i->m_nLanguageCode & 0xFF);
    country[0] = (char)(i->m_nCountryCode  >> 8);
    country[1] = (char)(i->m_nCountryCode  & 0xFF);
    std::string text;
    if (i->GetBuf() && i->GetLength() > 0)
      icUtf16ToUtf8(text, i->GetBuf(), (int)i->GetLength());
    IccJson loc;
    loc["language"] = lang;
    loc["country"]  = country;
    loc["text"]     = text;
    arr.push_back(loc);
  }
  return arr;
}

// Helper: parse a JSON localized array into a new CIccTagMultiLocalizedUnicode.
// Returns nullptr on empty input (caller should not attach the tag).
static CIccTagMultiLocalizedUnicode *dictLocalizedFromJson(const IccJson &arr)
{
  if (!arr.is_array() || arr.empty())
    return nullptr;
  CIccTagMultiLocalizedUnicode *pTag = new(std::nothrow) CIccTagMultiLocalizedUnicode();
  if (!pTag) return nullptr;
  for (const auto &loc : arr) {
    std::string lang    = loc.contains("language") ? loc["language"].get<std::string>() : "  ";
    std::string country = loc.contains("country")  ? loc["country"].get<std::string>()  : "  ";
    std::string text    = loc.contains("text")     ? loc["text"].get<std::string>()     : "";
    while (lang.size()    < 2) lang    += ' ';
    while (country.size() < 2) country += ' ';
    icLanguageCode langCode    = (icLanguageCode)((lang[0]    << 8) | lang[1]);
    icCountryCode  countryCode = (icCountryCode) ((country[0] << 8) | country[1]);
    CIccUTF16String wstr(text.c_str());
    pTag->SetText(wstr.c_str(), langCode, countryCode);
  }
  return pTag;
}

bool CIccTagJsonDict::ToJson(IccJson &j)
{
  IccJson entries = IccJson::array();

  CIccNameValueDict::iterator nvp;
  for (nvp = m_Dict->begin(); nvp != m_Dict->end(); ++nvp) {
    CIccDictEntry *nv = nvp->ptr;
    if (!nv)
      continue;

    IccJson entry;

    // Name (stored as wstring / UTF-16 on Windows)
    std::string name;
    const std::wstring &wname = nv->GetName();
    icUtf16ToUtf8(name, (const icUInt16Number*)wname.c_str(), (int)wname.size());
    entry["name"] = name;

    // Optional value
    if (nv->IsValueSet()) {
      std::string value;
      const std::wstring wval = nv->GetValue();
      icUtf16ToUtf8(value, (const icUInt16Number*)wval.c_str(), (int)wval.size());
      entry["value"] = value;
    }

    // Optional localized name
    if (nv->GetNameLocalized())
      entry["localizedNames"] = dictLocalizedToJson(nv->GetNameLocalized());

    // Optional localized value
    if (nv->GetValueLocalized())
      entry["localizedValues"] = dictLocalizedToJson(nv->GetValueLocalized());

    entries.push_back(entry);
  }

  j["entries"] = entries;
  return true;
}

bool CIccTagJsonDict::ParseJson(const IccJson &j, std::string &parseStr)
{
  if (!jsonExistsField(j, "entries") || !j["entries"].is_array()) {
    parseStr += "Missing or invalid 'entries' array in Dict tag\n";
    return false;
  }

  m_Dict->clear();

  for (const auto &entry : j["entries"]) {
    std::string name;
    if (!jGetString(entry, "name", name)) {
      parseStr += "Dict entry missing 'name'\n";
      return false;
    }

    CIccDictEntry *pDesc = new(std::nothrow) CIccDictEntry();
    if (!pDesc)
      return false;

    // Name: UTF-8 -> UTF-16 -> wstring
    CIccUTF16String wname(name.c_str());
    wname.ToWString(pDesc->GetName());

    // Optional value
    std::string val;
    if (jGetString(entry, "value", val)) {
      CIccUTF16String wval(val.c_str());
      std::wstring wvalStr;
      wval.ToWString(wvalStr);
      pDesc->SetValue(wvalStr);
    }

    // Optional localized name
    if (jsonExistsField(entry, "localizedNames")) {
      CIccTagMultiLocalizedUnicode *pTag = dictLocalizedFromJson(entry["localizedNames"]);
      if (pTag)
        pDesc->SetNameLocalized(pTag);
    }

    // Optional localized value
    if (jsonExistsField(entry, "localizedValues")) {
      CIccTagMultiLocalizedUnicode *pTag = dictLocalizedFromJson(entry["localizedValues"]);
      if (pTag)
        pDesc->SetValueLocalized(pTag);
    }

    CIccDictEntryPtr ptr;
    ptr.ptr = pDesc;
    m_Dict->push_back(ptr);
  }

  return true;
}

// ===========================================================================
// CIccTagJsonStruct
// ===========================================================================

bool CIccTagJsonStruct::ToJson(IccJson &j)
{
  const size_t bufSize = 256;
  char buf[bufSize];

  IIccStruct *pStruct = GetStructHandler();

  // Emit the struct type name so round-trip parsing can restore the struct type
  std::string structName;
  if (pStruct && strcmp(pStruct->GetDisplayName(), "privateStruct")) {
    structName = pStruct->GetDisplayName();
  } else {
    structName = "privateStruct";
    j["structureSignature"] = icGetSigStr(buf, bufSize, m_sigStructType);
  }
  j["structureType"] = structName;

  // Member tags -- array of single-key objects: { "<elemName>": { "type": ..., ...fields... } }
  IccJson memberTags = IccJson::array();
  std::set<icTagSignature> sigSet;
  std::map<CIccTag*, std::string> ptrToFirstKey;
  int privateElemCount = 0;

  for (auto i = m_ElemEntries->begin(); i != m_ElemEntries->end(); ++i) {
    if (sigSet.count(i->TagInfo.sig))
      continue;
    sigSet.insert(i->TagInfo.sig);

    CIccTag *pTag = FindElem(i->TagInfo.sig);
    if (!pTag)
      continue;

    // Determine element name
    std::string elemName = pStruct ? pStruct->GetElemName((icSignature)i->TagInfo.sig) : "";
    bool isPrivate = elemName.empty() || !elemName.compare(0, 13, "PrivateSubTag");
    std::string key = isPrivate
        ? "PrivateSubTag_" + std::to_string(++privateElemCount)
        : elemName;

    IccJson elemObj;
    if (isPrivate)
      elemObj["sig"] = icGetSigStr(buf, bufSize, i->TagInfo.sig);

    // SameAs: already serialized element
    auto prevIt = ptrToFirstKey.find(pTag);
    if (prevIt != ptrToFirstKey.end()) {
      elemObj["sameAs"] = prevIt->second;
      IccJson entry;
      entry[key] = elemObj;
      memberTags.push_back(entry);
      continue;
    }

    IIccExtensionTag *pExt = pTag->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0)
      continue;

    CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);
    IccJson tagData;
    if (!pJsonTag->ToJson(tagData))
      continue;

    const icChar *typeName = CIccTagCreator::GetTagTypeSigName(pTag->GetType());
    if (!typeName) typeName = "PrivateType";
    elemObj["type"] = typeName;
    if (!strcmp(typeName, "PrivateType"))
      elemObj["typeSig"] = icGetSigStr(buf, bufSize, pTag->GetType());
    if (pTag->m_nReserved)
      elemObj["Reserved"] = (unsigned int)pTag->m_nReserved;
    elemObj.update(tagData);

    IccJson entry;
    entry[key] = elemObj;
    memberTags.push_back(entry);
    ptrToFirstKey[pTag] = key;
  }
  j["memberTags"] = memberTags;
  return true;
}

bool CIccTagJsonStruct::ParseTag(const IccJson &elemEntry, std::string &parseStr)
{
  if (!elemEntry.is_object() || elemEntry.size() != 1) {
    parseStr += "MemberTag entry must be a single-member object\n";
    return false;
  }
  auto it = elemEntry.begin();
  const std::string &key     = it.key();
  const IccJson     &elemObj = it.value();

  // Determine element signature
  IIccStruct *pStruct = GetStructHandler();
  icSignature sigElem = pStruct ? pStruct->GetElemSig(key.c_str()) : (icSignature)0;
  if (!sigElem) {
    // Private sub-tag -- must carry a "sig" member
    std::string sigStr;
    jGetString(elemObj, "sig", sigStr);
    if (sigStr.empty()) {
      parseStr += "PrivateSubTag '" + key + "' missing 'sig'\n";
      return false;
    }
    sigElem = (icSignature)icGetSigVal(sigStr.c_str());
  }

  // SameAs: re-attach an already-parsed element
  if (elemObj.contains("sameAs")) {
    std::string refKey = elemObj["sameAs"].get<std::string>();
    icSignature refSig = pStruct ? pStruct->GetElemSig(refKey.c_str()) : 0;
    if (!refSig) {
      // refKey may be a "PrivateSubTag_N" -- look up by walking existing entries
      for (auto ei = m_ElemEntries->begin(); ei != m_ElemEntries->end(); ++ei) {
        std::string n = pStruct ? pStruct->GetElemName((icSignature)ei->TagInfo.sig) : "";
        if (n == refKey) { refSig = ei->TagInfo.sig; break; }
      }
    }
    CIccTag *pRefTag = refSig ? FindElem(refSig) : nullptr;
    if (!pRefTag) {
      parseStr += "SameAs '" + refKey + "' not found for '" + key + "'\n";
      return false;
    }
    if (!AttachElem(sigElem, pRefTag)) {
      parseStr += "Unable to attach SameAs element '" + key + "'\n";
      return false;
    }
    return true;
  }

  // Flat object with "type" discriminator
  std::string typeName;
  jGetString(elemObj, "type", typeName);
  if (typeName.empty()) {
    parseStr += "MemberTag '" + key + "' missing 'type' field\n";
    return false;
  }

  icTagTypeSignature typeSig = CIccTagCreator::GetTagTypeNameSig(typeName.c_str());
  if (typeSig == icSigUnknownType) {
    if (typeName == "PrivateType" && elemObj.contains("typeSig"))
      typeSig = (icTagTypeSignature)icGetSigVal(elemObj["typeSig"].get<std::string>().c_str());
    else
      typeSig = (icTagTypeSignature)icGetSigVal(typeName.c_str());
  }

  CIccTag *pTag = CIccTagCreator::CreateTag(typeSig);
  if (!pTag) {
    parseStr += "Unable to create type '" + typeName + "' for member '" + key + "'\n";
    return false;
  }

  IIccExtensionTag *pExt = pTag->GetExtension();
  if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0) {
    delete pTag;
    parseStr += "Type '" + typeName + "' does not support JSON\n";
    return false;
  }

  CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);
  if (!pJsonTag->ParseJson(elemObj, parseStr)) {
    delete pTag;
    return false;
  }

  if (elemObj.contains("Reserved"))
    pTag->m_nReserved = (icUInt32Number)elemObj["Reserved"].get<unsigned int>();

  if (!AttachElem(sigElem, pTag)) {
    delete pTag;
    parseStr += "Unable to attach member '" + key + "'\n";
    return false;
  }
  return true;
}

bool CIccTagJsonStruct::ParseJson(const IccJson &j, std::string &parseStr)
{
  // Resolve struct type
  std::string structTypeName;
  jGetString(j, "structureType", structTypeName);

  icStructSignature sigStruct = (icStructSignature)0;
  if (!structTypeName.empty() && structTypeName != "privateStruct") {
    sigStruct = CIccStructCreator::GetStructSig(structTypeName.c_str());
  }
  if (!sigStruct) {
    // Fall back to explicit signature field
    std::string sigStr;
    jGetString(j, "structureSignature", sigStr);
    if (!sigStr.empty())
      sigStruct = (icStructSignature)icGetSigVal(sigStr.c_str());
  }
  if (sigStruct)
    SetTagStructType(sigStruct);

  // Parse memberTags array
  if (!j.contains("memberTags") || !j["memberTags"].is_array()) {
    parseStr += "Struct tag missing 'memberTags' array\n";
    return false;
  }
  for (const auto &entry : j["memberTags"]) {
    if (!ParseTag(entry, parseStr))
      parseStr += "Warning: skipped member tag\n";
  }
  return true;
}

// ===========================================================================
// CIccTagJsonArray
// ===========================================================================

bool CIccTagJsonArray::ToJson(IccJson &j)
{
  const size_t bufSize = 256;
  char buf[bufSize];

  // Emit array type so round-trip can restore it
  std::string arrayName;
  if (CIccArrayCreator::GetArraySigName(arrayName, m_sigArrayType, false)) {
    j["arrayType"] = arrayName;
  } else {
    j["arrayType"]      = "privateArray";
    j["arraySignature"] = icGetSigStr(buf, bufSize, m_sigArrayType);
  }

  // Elements -- flat objects with "type" discriminator field
  IccJson elems = IccJson::array();
  for (icUInt32Number i = 0; i < m_nSize; i++) {
    CIccTag *pTag = m_TagVals[i].ptr;
    IccJson elemObj;
    if (!pTag) {
      elemObj["type"] = nullptr;
      elems.push_back(elemObj);
      continue;
    }

    IIccExtensionTag *pExt = pTag->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0) {
      elemObj["type"] = nullptr;
      elems.push_back(elemObj);
      continue;
    }

    CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);
    IccJson tagData;
    if (!pJsonTag->ToJson(tagData)) {
      elemObj["type"] = nullptr;
      elems.push_back(elemObj);
      continue;
    }

    const icChar *typeName = CIccTagCreator::GetTagTypeSigName(pTag->GetType());
    if (!typeName) typeName = "PrivateType";
    elemObj["type"] = typeName;
    if (!strcmp(typeName, "PrivateType"))
      elemObj["sig"] = icGetSigStr(buf, bufSize, pTag->GetType());
    if (pTag->m_nReserved)
      elemObj["Reserved"] = (unsigned int)pTag->m_nReserved;
    elemObj.update(tagData);

    elems.push_back(elemObj);
  }
  j["arrayTags"] = elems;
  return true;
}

bool CIccTagJsonArray::ParseJson(const IccJson &j, std::string &parseStr)
{
  // Resolve array type
  std::string arrayTypeName;
  jGetString(j, "arrayType", arrayTypeName);

  icArraySignature sigArray = (icArraySignature)0;
  if (!arrayTypeName.empty() && arrayTypeName != "privateArray") {
    sigArray = CIccArrayCreator::GetArraySig(arrayTypeName.c_str());
  }
  if (!sigArray) {
    std::string sigStr;
    jGetString(j, "arraySignature", sigStr);
    if (!sigStr.empty())
      sigArray = (icArraySignature)icGetSigVal(sigStr.c_str());
  }
  if (sigArray)
    SetTagArrayType(sigArray);

  // Parse arrayTags
  if (!j.contains("arrayTags") || !j["arrayTags"].is_array()) {
    parseStr += "Array tag missing 'arrayTags' array\n";
    return false;
  }
  const IccJson &elems = j["arrayTags"];
  icUInt32Number nSize = icJsonSafeU32(elems.size());
  if (!SetSize(nSize)) {
    parseStr += "Unable to allocate array of size " + std::to_string(nSize) + "\n";
    return false;
  }

  for (icUInt32Number i = 0; i < nSize; i++) {
    const IccJson &elemObj = elems[i];

    // Flat object with "type" discriminator
    std::string typeName;
    jGetString(elemObj, "type", typeName);
    if (typeName.empty()) {
      parseStr += "Warning: array element " + std::to_string(i) + " has no type, skipping\n";
      continue;
    }

    icTagTypeSignature typeSig = CIccTagCreator::GetTagTypeNameSig(typeName.c_str());
    if (typeSig == icSigUnknownType) {
      if (typeName == "PrivateType" && elemObj.contains("sig"))
        typeSig = (icTagTypeSignature)icGetSigVal(elemObj["sig"].get<std::string>().c_str());
      else
        typeSig = (icTagTypeSignature)icGetSigVal(typeName.c_str());
    }

    CIccTag *pTag = CIccTagCreator::CreateTag(typeSig);
    if (!pTag) {
      parseStr += "Unable to create type '" + typeName + "' for array element " + std::to_string(i) + "\n";
      return false;
    }

    IIccExtensionTag *pExt = pTag->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0) {
      delete pTag;
      parseStr += "Type '" + typeName + "' does not support JSON\n";
      return false;
    }

    CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);
    if (!pJsonTag->ParseJson(elemObj, parseStr)) {
      delete pTag;
      return false;
    }

    if (elemObj.contains("Reserved"))
      pTag->m_nReserved = (icUInt32Number)elemObj["Reserved"].get<unsigned int>();

    if (!AttachTag(i, pTag)) {
      delete pTag;
      parseStr += "Unable to attach array element " + std::to_string(i) + "\n";
      return false;
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonGamutBoundaryDesc
// ===========================================================================

bool CIccTagJsonGamutBoundaryDesc::ToJson(IccJson &j)
{
  if (m_NumberOfVertices > 0 && (m_PCSValues || m_DeviceValues)) {
    IccJson vertices;
    if (m_PCSValues) {
      vertices["PCSChannels"] = (int)m_nPCSChannels;
      IccJson pcsArr = IccJson::array();
      icInt32Number nPCS = m_NumberOfVertices * m_nPCSChannels;
      for (icInt32Number i = 0; i < nPCS; i++)
        pcsArr.push_back((double)m_PCSValues[i]);
      vertices["PCSValues"] = pcsArr;
    }
    if (m_DeviceValues) {
      vertices["DeviceChannels"] = (int)m_nDeviceChannels;
      IccJson devArr = IccJson::array();
      icInt32Number nDev = m_NumberOfVertices * m_nDeviceChannels;
      for (icInt32Number i = 0; i < nDev; i++)
        devArr.push_back((double)m_DeviceValues[i]);
      vertices["DeviceValues"] = devArr;
    }
    j["Vertices"] = vertices;
  }

  if (m_Triangles && m_NumberOfTriangles > 0) {
    IccJson triArr = IccJson::array();
    for (icInt32Number i = 0; i < m_NumberOfTriangles; i++) {
      IccJson tri = IccJson::array();
      tri.push_back((icUInt32Number)m_Triangles[i].m_VertexNumbers[0]);
      tri.push_back((icUInt32Number)m_Triangles[i].m_VertexNumbers[1]);
      tri.push_back((icUInt32Number)m_Triangles[i].m_VertexNumbers[2]);
      triArr.push_back(tri);
    }
    j["Triangles"] = triArr;
  }

  return true;
}

bool CIccTagJsonGamutBoundaryDesc::ParseJson(const IccJson &j, std::string &parseStr)
{
  if (!j.contains("Vertices") || !j["Vertices"].is_object()) {
    parseStr += "Cannot find Vertices\n";
    return false;
  }
  const IccJson &verts = j["Vertices"];

  // Parse PCS values
  if (!verts.contains("PCSChannels") || !verts.contains("PCSValues") || !verts["PCSValues"].is_array()) {
    parseStr += "Cannot find PCSValues\n";
    return false;
  }
  int nPCSCh = verts["PCSChannels"].get<int>();
  if (nPCSCh <= 0 || nPCSCh > 32767) {
    parseStr += "Bad PCSValues channels\n";
    return false;
  }
  m_nPCSChannels = (icInt16Number)nPCSCh;
  const IccJson &pcsArr = verts["PCSValues"];
  icInt32Number nPCSTotal = (icInt32Number)pcsArr.size();
  m_NumberOfVertices = nPCSTotal / m_nPCSChannels;
  if (m_NumberOfVertices < 4) {
    parseStr += "Must have at least 4 PCSValues vertices\n";
    return false;
  }
  if (m_PCSValues) { delete[] m_PCSValues; }
  icUInt64Number nPCSAlloc = (icUInt64Number)m_NumberOfVertices * m_nPCSChannels;
  if (nPCSAlloc > 0x7FFFFFFF) { parseStr += "PCSValues allocation overflow\n"; return false; }
  m_PCSValues = new(std::nothrow) icFloatNumber[(size_t)nPCSAlloc];
  if (!m_PCSValues) { parseStr += "Allocation failed for PCSValues\n"; return false; }
  for (icInt32Number i = 0; i < nPCSTotal; i++)
    m_PCSValues[i] = (icFloatNumber)pcsArr[i].get<double>();

  // Parse optional Device values
  if (verts.contains("DeviceChannels") && verts.contains("DeviceValues") && verts["DeviceValues"].is_array()) {
    m_nDeviceChannels = (icInt16Number)verts["DeviceChannels"].get<int>();
    if (m_nDeviceChannels <= 0) {
      parseStr += "Bad DeviceValues channels\n";
      return false;
    }
    const IccJson &devArr = verts["DeviceValues"];
    icInt32Number nDevTotal = (icInt32Number)devArr.size();
    icInt32Number nDevVerts = nDevTotal / m_nDeviceChannels;
    if (nDevVerts != m_NumberOfVertices) {
      parseStr += "Number of Device vertices doesn't match PCS vertices\n";
      return false;
    }
    if (m_DeviceValues) { delete[] m_DeviceValues; }
    icUInt64Number nDevAlloc = (icUInt64Number)m_NumberOfVertices * m_nDeviceChannels;
    if (nDevAlloc > 0x7FFFFFFF) { parseStr += "DeviceValues allocation overflow\n"; return false; }
    m_DeviceValues = new(std::nothrow) icFloatNumber[(size_t)nDevAlloc];
    if (!m_DeviceValues) { parseStr += "Allocation failed for DeviceValues\n"; return false; }
    for (icInt32Number i = 0; i < nDevTotal; i++)
      m_DeviceValues[i] = (icFloatNumber)devArr[i].get<double>();
  }

  // Parse triangles
  if (!j.contains("Triangles") || !j["Triangles"].is_array()) {
    parseStr += "Cannot find Triangles\n";
    return false;
  }
  const IccJson &triArr = j["Triangles"];
  m_NumberOfTriangles = (icInt32Number)triArr.size();
  if (m_Triangles) { delete[] m_Triangles; }
  m_Triangles = new(std::nothrow) icGamutBoundaryTriangle[m_NumberOfTriangles];
  if (!m_Triangles) { parseStr += "Allocation failed for Triangles\n"; return false; }
  for (icInt32Number i = 0; i < m_NumberOfTriangles; i++) {
    const IccJson &tri = triArr[i];
    if (!tri.is_array() || tri.size() != 3) {
      parseStr += "Invalid Triangle entry\n";
      return false;
    }
    m_Triangles[i].m_VertexNumbers[0] = tri[0].get<icUInt32Number>();
    m_Triangles[i].m_VertexNumbers[1] = tri[1].get<icUInt32Number>();
    m_Triangles[i].m_VertexNumbers[2] = tri[2].get<icUInt32Number>();
  }

  return true;
}

// ===========================================================================
// CIccTagJsonEmbeddedHeightImage
// ===========================================================================

bool CIccTagJsonEmbeddedHeightImage::ToJson(IccJson &j)
{
  j["SeamlessIndicator"] = (unsigned int)m_nSeamlesIndicator;
  j["EncodingFormat"]    = (unsigned int)m_nEncodingFormat;
  j["MetersMinPixelValue"] = (double)m_fMetersMinPixelValue;
  j["MetersMaxPixelValue"] = (double)m_fMetersMaxPixelValue;
  if (m_pData && m_nSize)
    j["ImageData"] = icJsonDumpHexData(m_pData, m_nSize);
  return true;
}

bool CIccTagJsonEmbeddedHeightImage::ParseJson(const IccJson &j, std::string &parseStr)
{
  m_nSeamlesIndicator  = 0;
  m_nEncodingFormat    = icPngImageType;
  m_fMetersMinPixelValue = 0.0f;
  m_fMetersMaxPixelValue = 0.0f;

  jGetValue(j, "SeamlessIndicator",   m_nSeamlesIndicator);
  unsigned int nEncFmt = (unsigned int)icPngImageType;
  jGetValue(j, "EncodingFormat", nEncFmt);
  m_nEncodingFormat = (icImageEncodingType)nEncFmt;
  jGetValue(j, "MetersMinPixelValue", m_fMetersMinPixelValue);
  jGetValue(j, "MetersMaxPixelValue", m_fMetersMaxPixelValue);

  std::string hex;
  if (!jGetString(j, "ImageData", hex)) {
    parseStr += "Cannot find ImageData in HeightImage\n";
    return false;
  }
  icUInt32Number nSize = icJsonGetHexDataSize(hex.c_str());
  if (nSize) {
    SetSize(nSize);
    if (icJsonGetHexData(m_pData, hex.c_str(), m_nSize) != m_nSize) {
      parseStr += "Failed to decode HeightImage ImageData\n";
      return false;
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonEmbeddedNormalImage
// ===========================================================================

bool CIccTagJsonEmbeddedNormalImage::ToJson(IccJson &j)
{
  j["SeamlessIndicator"] = (unsigned int)m_nSeamlesIndicator;
  j["EncodingFormat"]    = (unsigned int)m_nEncodingFormat;
  if (m_pData && m_nSize)
    j["ImageData"] = icJsonDumpHexData(m_pData, m_nSize);
  return true;
}

bool CIccTagJsonEmbeddedNormalImage::ParseJson(const IccJson &j, std::string &parseStr)
{
  m_nSeamlesIndicator = 0;
  m_nEncodingFormat   = icPngImageType;

  jGetValue(j, "SeamlessIndicator", m_nSeamlesIndicator);
  unsigned int nEncFmt = (unsigned int)icPngImageType;
  jGetValue(j, "EncodingFormat", nEncFmt);
  m_nEncodingFormat = (icImageEncodingType)nEncFmt;

  std::string hex;
  if (!jGetString(j, "ImageData", hex)) {
    parseStr += "Cannot find ImageData in NormalImage\n";
    return false;
  }
  icUInt32Number nSize = icJsonGetHexDataSize(hex.c_str());
  if (nSize) {
    SetSize(nSize);
    if (icJsonGetHexData(m_pData, hex.c_str(), m_nSize) != m_nSize) {
      parseStr += "Failed to decode NormalImage ImageData\n";
      return false;
    }
  }
  return true;
}

// ===========================================================================
// CIccTagJsonEmbeddedProfile
// ===========================================================================

bool CIccTagJsonEmbeddedProfile::ToJson(IccJson &j)
{
  if (!m_pProfile)
    return false;

  if (strcmp(m_pProfile->GetClassName(), "CIccProfileJson") != 0)
    return false;

  CIccProfileJson *pProfile = static_cast<CIccProfileJson*>(m_pProfile);
  return pProfile->ToJson(j["IccProfile"]);
}

bool CIccTagJsonEmbeddedProfile::ParseJson(const IccJson &j, std::string &parseStr)
{
  if (!j.contains("IccProfile") || !j["IccProfile"].is_object()) {
    parseStr += "Cannot find IccProfile in embedded profile tag\n";
    return false;
  }

  CIccProfileJson *pProfile = new(std::nothrow) CIccProfileJson();
  if (!pProfile) { parseStr += "Allocation failed for embedded profile\n"; return false; }
  if (!pProfile->ParseJson(j["IccProfile"], parseStr)) {
    delete pProfile;
    return false;
  }

  SetProfile(pProfile);
  return true;
}

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif
