/*
    File:       IccCmmConfig.cpp

    Contains:   Cmm application configuraiton objects

    Version:    V1

    Copyright:  (c) see below
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
 // -Initial implementation by Max Derhak 1-11-2024
 //
 //////////////////////////////////////////////////////////////////////


#include "IccCmmConfig.h"
#include <cstdio>
#include <fstream>
#include <cstring>
#include <new>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

static bool icWriteString(FILE* f, const std::string& out)
{
  return out.empty() || fwrite(out.c_str(), 1, out.size(), f) == out.size();
}

static FILE* icOpenWriteTextFile(const char* filename)
{
  if (!filename || !filename[0])
    return stdout;

#if defined(_WIN32)
  return fopen(filename, "wt");
#else
  struct stat st;
  if (stat(filename, &st) == 0 && !S_ISREG(st.st_mode))
    return nullptr;

  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0)
    return nullptr;

  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    return nullptr;
  }

  FILE* f = fdopen(fd, "w");
  if (!f)
    close(fd);

  return f;
#endif
}

static bool icCloseWriteTextFile(FILE* f)
{
  if (!f)
    return false;

  bool failed = (fflush(f) != 0) || (ferror(f) != 0);

  if (f == stdout)
    return !failed;

  if (fclose(f) != 0)
    failed = true;

  return !failed;
}

static bool icFormatFloatValue(char* buf, size_t bufSize, int nDigits, int nPrecision, icFloatNumber v)
{
  int n;
  if (!nDigits)
    n = snprintf(buf, bufSize, " %.*f", nPrecision, v);
  else
    n = snprintf(buf, bufSize, " %*.*f", nDigits, nPrecision, v);

  return n >= 0 && (size_t)n < bufSize;
}

static const icChar* icGetJsonFloatColorEncoding(icFloatColorEncoding val)
{
  switch (val) {

  case icEncodeValue:
    return "value";

  case icEncodeFloat:
    return "float";

  case icEncodeUnitFloat:
    return "unitFloat";

  case icEncodePercent:
    return "percent";

  case icEncode8Bit:
    return "8Bit";

  case icEncode16Bit:
    return "16Bit";

  case icEncode16BitV2:
    return "16BitV2";

  default:
    return "unknown";
  }
}

static const char* clrEncNames[] = { "value", "float", "unitFloat", "percent",
                                     "8Bit", "16Bit", "16BitV2", nullptr };
static icFloatColorEncoding clrEncValues[] = { icEncodeValue, icEncodeFloat, icEncodeUnitFloat, icEncodePercent,
                                               icEncode8Bit, icEncode16Bit, icEncode16BitV2, icEncodeUnknown };

icFloatColorEncoding icSetJsonColorEncoding(const char* szEncode)
{

  int i;
  for (i = 0; clrEncNames[i]; i++) {
    if (!stricmp(szEncode, clrEncNames[i]))
      break;
  }

  return clrEncValues[i];
}

bool jsonToValue(const json& j, icFloatColorEncoding& v)
{
  std::string str;
  if (jsonToValue(j, str)) {
    v = icSetJsonColorEncoding(str.c_str());
    return true;
  }

  return false;
}

const char* icGetJsonColorEncoding(icFloatColorEncoding v)
{
  int i;
  for (i = 0; clrEncNames[i]; i++) {
    if (v==clrEncValues[i])
      break;
  }

  return clrEncNames[i] ? clrEncNames[i] : clrEncNames[0];
}

static const char* fileEncNames[] = { "8Bit", "16Bit", "float", "sameAsSource", nullptr};
static icFloatColorEncoding fileEncValues[] = { icEncode8Bit, icEncode16Bit, icEncodeFloat, icEncodeUnknown, icEncode8Bit };

icFloatColorEncoding icSetJsonFileEncoding(const char* szEncode)
{

  int i;
  for (i = 0; fileEncNames[i]; i++) {
    if (!stricmp(szEncode, fileEncNames[i]))
      break;
  }

  return fileEncValues[i];
}

const char* icGetJsonFileEncoding(icFloatColorEncoding v)
{

  int i;
  for (i = 0; fileEncNames[i]; i++) {
    if (v == fileEncValues[i])
      break;
  }

  return fileEncNames[i] ? fileEncNames[i] : clrEncNames[0];
}



CIccCfgDataApply::CIccCfgDataApply()
{
  reset();
}

void CIccCfgDataApply::reset()
{
  m_debugCalc = false;
  m_srcType = icCfgColorData;
  m_srcSpace = icSigUnknownData;
  m_srcFile.clear();
  m_dstType = icCfgColorData;
  m_dstFile.clear();
  m_dstEncoding = icEncodeValue;
  m_dstDigits = 9;
  m_dstPrecision = 4;
}

bool jsonToValue(const json& j, icCfgDataType& v)
{
  std::string str;
  if (jsonToValue(j, str)) {
    if (str == "colorData")
      v = icCfgColorData;
    else if (str == "legacy")
      v = icCfgLegacy;
    else if (str == "it8")
      v = icCfgIt8;
    else
      return false;
  }
  else
    return false;

  return true;
}

void jsonSetValue(json& j, const char* szName, icCfgDataType v)
{
  if (v == icCfgColorData)
    j[szName] = "colorData";
  else if (v == icCfgLegacy)
    j[szName] = "legacy";
  else if (v == icCfgIt8)
    j[szName] = "it8";
}



int CIccCfgDataApply::fromArgs(const char** args, int nArg, bool bReset)
{
  if (nArg < 2) {
    return 0;
  }

  if (bReset)
    reset();

  m_srcType = icCfgLegacy;
  m_srcFile = args[0];

  m_dstType = icCfgLegacy;
  m_dstFile.clear();

  //Setup destination encoding
  m_dstEncoding = (icFloatColorEncoding)atoi(args[1]);

  const char *colon = strchr(args[1], ':');
  if (colon) {
    colon++;
    m_dstPrecision = (icUInt8Number)atoi(colon);
    m_dstDigits = 5 + m_dstPrecision;
    colon = strchr(colon, ':');
    if (colon) {
      m_dstDigits = atoi(colon);
    }
  }
  
  return 2;
}

bool CIccCfgDataApply::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  jsonToValue(j["debugCalc"], m_debugCalc);

  jsonToValue(j["srcType"], m_srcType);
  if (j.find("srcSpace")!=j.end())
    jsonToColorSpace(j["srcSpace"], m_srcSpace);
  jsonToValue(j["srcFile"], m_srcFile);

  jsonToValue(j["dstType"], m_dstType);
  jsonToValue(j["dstFile"], m_dstFile);
  jsonToValue(j["dstEncoding"], m_dstEncoding);

  jsonToValue(j["dstDigits"], m_dstDigits);
  jsonToValue(j["dstPrecision"], m_dstPrecision);

  return true;
}

void CIccCfgDataApply::toJson(json& j) const
{
  if (m_debugCalc)
    j["debugCalc"] = m_debugCalc;

  jsonSetValue(j, "srcType", m_srcType);
  if (m_srcSpace != icSigUnknownData) {
    char buf[30];
    j["srcSpace"] = icGetColorSigStr(buf, 30, m_srcSpace);
  }

  if (m_srcFile.size() != size_t(0))
    j["srcFile"] = m_srcFile;

  jsonSetValue(j, "dstType", m_dstType);
  if (m_dstFile.size() != size_t(0))
    j["dstFile"] = m_dstFile;

  if (m_dstEncoding != icEncodeValue)
    j["dstEncoding"] = icGetJsonColorEncoding(m_dstEncoding);

  if (m_dstDigits != 9)
    j["dstDigits"] = m_dstDigits;

  if (m_dstPrecision != 4)
    j["dstPrecision"] = m_dstPrecision;
}

CIccCfgImageApply::CIccCfgImageApply()
{
  reset();
}

bool jsonToValue(const json& j, icDstBool& v)
{
  if (j.is_boolean())
    v = j.get<bool>() ? icDstBoolTrue : icDstBoolFalse;
  else if (j.is_string()) {
    std::string str = j.get<std::string>();
    if (str == "true")
      v = icDstBoolTrue;
    else if (str == "false")
      v = icDstBoolFalse;
    else if (str == "sameAsSource")
      v = icDstBoolFromSrc;
    else
      return false;
  }
  else
    return false;

  return true;
}

void CIccCfgImageApply::reset()
{
  m_srcImgFile.clear();
  m_dstImgFile.clear();
  m_dstEncoding = icEncode8Bit;
  m_dstCompression = icDstBoolTrue;
  m_dstPlanar = icDstBoolFalse;
  m_dstEmbedIcc = icDstBoolTrue;
}

int CIccCfgImageApply::fromArgs(const char** args, int nArg, bool bReset)
{
  if (nArg < 6) {
    return 0;
  }

  if (bReset)
    reset();

  m_srcImgFile = args[0];
  m_dstImgFile = args[1];

  int n = atoi(args[2]);
  switch (n)
  {
    case 0:
      m_dstEncoding = icEncodeUnknown;
      break;

    default:
    case 1:
      m_dstEncoding = icEncode8Bit;
      break;

    case 2:
      m_dstEncoding = icEncode16Bit;
      break;

    case 3:
      m_dstEncoding = icEncodeFloat;
      break;
  }

  m_dstCompression = atoi(args[3]) != 0 ? icDstBoolTrue : icDstBoolFalse;
  m_dstPlanar = atoi(args[4]) != 0 ? icDstBoolTrue : icDstBoolFalse;
  m_dstEmbedIcc = atoi(args[5]) != 0 ? icDstBoolTrue : icDstBoolFalse;

  return 6;
}

static void setDstBool(json &j, const char *szName, icDstBool v)
{
  if (v == icDstBoolTrue)
    j[szName] = true;
  else if (v == icDstBoolFalse)
    j[szName] = false;
  else if (v == icDstBoolFromSrc) // Compare, don't assign
    j[szName] = "sameAsSource";
  else
    j[szName] = nullptr; //  handle unexpected values
}

bool CIccCfgImageApply::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  jsonToValue(j["srcImageFile"], m_srcImgFile);
  jsonToValue(j["dstImageFile"], m_dstImgFile);
  
  std::string str;
  if (jsonToValue(j["dstEncoding"], str))
    m_dstEncoding = icSetJsonFileEncoding(str.c_str());

  jsonToValue(j["dstCompression"], m_dstCompression);
  jsonToValue(j["dstPlanar"], m_dstPlanar);
  jsonToValue(j["dstEmbedIcc"], m_dstEmbedIcc);

  return true;
}

void CIccCfgImageApply::toJson(json& j) const
{
  if (m_srcImgFile.size() != size_t(0))
    j["srcImageFile"] = m_srcImgFile;
  if (m_dstImgFile.size() != size_t(0))
    j["dstImageFile"] = m_dstImgFile;

  if (m_dstEncoding != icEncode8Bit)
    j["dstEncoding"] = icGetJsonFileEncoding(m_dstEncoding);

  setDstBool(j, "dstCompression", m_dstCompression);
  setDstBool(j, "dstPlanar", m_dstPlanar);
  setDstBool(j, "dstEmbedIcc", m_dstEmbedIcc);
}

CIccCfgConnectOptions::CIccCfgConnectOptions()
{
  reset();
}

void CIccCfgConnectOptions::reset()
{
  m_nThreads = 1;
}

bool CIccCfgConnectOptions::fromJson(json j, bool bReset)
{
  if (j.is_null())
    return true;

  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  if (j.find("threads") != j.end()) {
    int nThreads = m_nThreads;
    if (!jsonToValue(j["threads"], nThreads) || nThreads < 0)
      return false;
    m_nThreads = nThreads;
  }

  return true;
}

void CIccCfgConnectOptions::toJson(json& j) const
{
  if (m_nThreads != 1)
    j["threads"] = m_nThreads;
}

CIccCfgCreateLink::CIccCfgCreateLink()
{
  reset();
}

void CIccCfgCreateLink::reset()
{
  m_linkFile.clear();
  m_linkType = icCfgCreateIccLinkV4;
  m_linkGridSize = 19;
  m_linkPrecision = 4;
  m_linkTitle.clear();
  m_linkMinRange = (icFloatNumber)0.0;
  m_linkMaxRange = (icFloatNumber)1.0;
  m_useSourceTransform = true;
}

int CIccCfgCreateLink::fromArgs(const char** args, int nArg, bool bReset)
{
  if (nArg < 8) {
    return 0;
  }

  if (bReset)
    reset();

  m_linkFile = args[0];

  int n = atoi(args[1]);
  m_linkGridSize = atoi(args[2]);
  int o = atoi(args[3]);

  switch (n)
  {
  case 0:
    m_linkType = o==0 ? icCfgCreateIccLinkV4 : icCfgCreateIccLinkV5;
    break;

  case 1:
    m_linkType = icCfgCreateTextCube;
    m_linkPrecision = (icUInt8Number)o;
    break;

  default:
    m_linkType = icCfgCreateIccLinkV4;
    break;
  }

  m_linkTitle = args[4];

  m_linkMinRange = (icFloatNumber)atof(args[5]);
  m_linkMaxRange = (icFloatNumber)atof(args[6]);

  m_useSourceTransform = atoi(args[7]) != 0;

  return 8;
}

bool CIccCfgCreateLink::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  jsonToValue(j["linkFile"], m_linkFile);

  std::string linkType;
  jsonToValue(j["linkType"], linkType);
  if (!stricmp(linkType.c_str(), "cubeFile"))
    m_linkType = icCfgCreateTextCube;
  else if (!stricmp(linkType.c_str(), "iccDeviceLinkV5"))
    m_linkType = icCfgCreateIccLinkV5;
  else /*!stricmp(linkType.c_str(), "iccDeviceLinkV4")*/
    m_linkType = icCfgCreateIccLinkV4;

  jsonToValue(j["linkGridSize"], m_linkGridSize);
  jsonToValue(j["linkPrecision"], m_linkPrecision);
  jsonToValue(j["linkTitle"], m_linkTitle);
  jsonToValue(j["linkMinRange"], m_linkMinRange);
  jsonToValue(j["linkMaxRange"], m_linkMaxRange);
  jsonToValue(j["useSourceTransform"], m_useSourceTransform);

  return true;
}

void CIccCfgCreateLink::toJson(json& j) const
{
  if (m_linkFile.size() != size_t(0))
    j["linkFile"] = m_linkFile;

  switch (m_linkType) {
  case icCfgCreateTextCube:
    j["linkType"] = "cubeFile";
    j["linkPrecision"] = m_linkPrecision;
    break;
  default:
  case icCfgCreateIccLinkV4:
    j["linkType"] = "iccDeviceLinkV4";
    break;
  case icCfgCreateIccLinkV5:
    j["linkType"] = "iccDeviceLinkV5";
    break;
  }

  j["linkTitle"] = m_linkTitle;
  j["linkMinRange"] = m_linkMinRange;
  j["linkMaxRange"] = m_linkMaxRange;
  j["linkGridSize"] = m_linkGridSize;
  j["useSourceTransform"] = m_useSourceTransform;
}

CIccCfgProfile::CIccCfgProfile()
{
  reset();
}

void CIccCfgProfile::reset()
{
  m_useEmbedded = false;
  m_iccFile.clear();
  m_intent = icUnknownIntent;
  m_transform = icXformLutColor;
  m_iccEnvVars.clear();
  m_pccFile.clear();
  m_pccEnvVars.clear();
  m_adjustPcsLuminance = false;
  m_useD2BxB2Dx = false;
  m_useBPC = false;
  m_useHToS = false;
  m_useV5SubProfile = false;
  m_interpolation = icInterpTetrahedral;
  m_nOverprint = icNamedColorOverWhite;
}

static const char* icIntentNames[] = { "perceptual", "relative", "saturation", "absolute" };

static bool icGetJsonRenderingIntent(const json& j, int& v)
{
  if (j.is_string()) {
    std::string str = j.get<std::string>();
    if (str == icIntentNames[icPerceptual])
      v = icPerceptual;
    else if (str == icIntentNames[icRelative])
      v = icRelative;
    else if (str == icIntentNames[icSaturation])
      v = icSaturation;
    else if (str == icIntentNames[icAbsolute])
      v = icAbsolute;
    else
      v = icUnknownIntent;
  }
  else if (j.is_number_integer()) {
    v = j.get<int>();
  }
  else
    return false;

  return true;
}

static const char* icGetRenderingIntentName(int nIntent)
{
  if (nIntent >= (int)icPerceptual && nIntent <= (int)icAbsolute)
    return icIntentNames[nIntent];

  return "unknown";
}

// Transform names exposed by JSON and the (transform, overprint) pair they
// resolve to.  The "namedOnBlack" / "namedOnGray" variants pick alternate
// spectral array members on a v5 NamedColor profile; the overprint value is
// ignored for non-named transforms.  Keep all three arrays in lock-step.
static const char* icTranNames[] = { "default",
                                     "named", "namedOnBlack", "namedOnGray",
                                     "colorimetric", "spectral",
                                     "MCS", "preview", "gamut", "brdfParam",
                                     "brdfDirect", "brdfMcsParam" , nullptr };

static icXformLutType icTranValues[] = { icXformLutColor,
                                         icXformLutNamedColor, icXformLutNamedColor, icXformLutNamedColor,
                                         icXformLutColorimetric, icXformLutSpectral,
                                         icXformLutMCS, icXformLutPreview, icXformLutGamut, icXformLutBRDFParam,
                                         icXformLutBRDFDirect, icXformLutBRDFMcsParam, icXformLutColor };

static icNamedColorOverprintType icTranOverprint[] = { icNamedColorOverWhite,
                                                       icNamedColorOverWhite, icNamedColorOverBlack, icNamedColorOverGray,
                                                       icNamedColorOverWhite, icNamedColorOverWhite,
                                                       icNamedColorOverWhite, icNamedColorOverWhite, icNamedColorOverWhite, icNamedColorOverWhite,
                                                       icNamedColorOverWhite, icNamedColorOverWhite, icNamedColorOverWhite };

static const char* icGetTransformName(int nTransform,
                                      icNamedColorOverprintType nOverprint = icNamedColorOverWhite)
{
  // Two-key lookup: for non-named transforms nOverprint is OverWhite by
  // construction so the row matches the same way it did before this field
  // existed.
  for (int i = 0; icTranNames[i]; i++) {
    if (nTransform == icTranValues[i] && nOverprint == icTranOverprint[i])
      return icTranNames[i];
  }
  // Fallback: match transform alone (for safety if a caller passes an
  // unexpected overprint value with a non-named transform).
  for (int i = 0; icTranNames[i]; i++) {
    if (nTransform == icTranValues[i])
      return icTranNames[i];
  }
  return icTranNames[0];
}

static const char* icInterpNames[] = { "linear", "tetrahedral", nullptr };

static icXformInterp icInterpValues[] = { icInterpLinear, icInterpTetrahedral, icInterpTetrahedral };

bool jsonToValue(const json& j, icCmmEnvSigMap& v)
{
  if (!j.is_array())
    return false;

  icCmmEnvSigMap envVars;
  for (auto e = j.begin(); e != j.end(); e++) {
    if (e->is_object()) {
      auto nameIt = e->find("name");
      auto valueIt = e->find("value");
      if (nameIt == e->end() || valueIt == e->end())
        return false;

      std::string name;
      icFloatNumber value;
      if (jsonToValue(*nameIt, name) && jsonToValue(*valueIt, value)) {
        icColorSpaceSignature sig = (icColorSpaceSignature)icGetSigVal(name.c_str());
        envVars[sig]=value;
      }
      else
        return false;
    }
    else
      return false;
  }
  v = envVars;
  return true;
}

bool CIccCfgProfile::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  CIccCfgProfile parsed;
  if (!bReset)
    parsed = *this;

  jsonToValue(j["iccFile"], parsed.m_iccFile);

  icGetJsonRenderingIntent(j["intent"], parsed.m_intent);

  std::string str;
  if (jsonToValue(j["transform"], str)) {
    int i;
    for (i = 0; icTranNames[i]; i++) {
      if (str == icTranNames[i])
        break;
    }
    parsed.m_transform = (icXformLutType)icTranValues[i];
    // Pull the overprint variant from the same row.  For non-named names
    // this is always icNamedColorOverWhite, so behaviour is unchanged
    // for existing configs.
    parsed.m_nOverprint = icTranOverprint[i];
  }

  if (j.contains("iccEnvVars") && !jsonToValue(j["iccEnvVars"], parsed.m_iccEnvVars))
    return false;

  jsonToValue(j["pccFile"], parsed.m_pccFile);
  if (j.contains("pccEnvVars") && !jsonToValue(j["pccEnvVars"], parsed.m_pccEnvVars))
    return false;

  jsonToValue(j["adjustPcsLuminance"], parsed.m_adjustPcsLuminance);
  jsonToValue(j["useBPC"], parsed.m_useBPC);
  jsonToValue(j["useHToS"], parsed.m_useHToS);
  jsonToValue(j["useV5SubProfile"], parsed.m_useV5SubProfile);
  jsonToValue(j["useD2BxB2Dx"], parsed.m_useD2BxB2Dx);

  if (jsonToValue(j["interpolation"], str)) {
    int i;
    for (i = 0; icInterpNames[i]; i++) {
      if (str == icInterpNames[i])
        break;
    }
    parsed.m_interpolation = icInterpValues[i];
  }

  *this = parsed;
  return true;
}

static bool jsonFromEnvMap(json& j, const icCmmEnvSigMap& map)
{
  char buf[30];
  j.clear();
  for (auto e = map.begin(); e != map.end(); e++) {
    json var;
    var["name"] = icGetSigStr(buf, 30, e->first);
    var["value"] = e->second;
    j.push_back(var);
  }
  return j.is_array() && j.size() > 0;
}

void CIccCfgProfile::toJson(json& j) const
{
  if (m_iccFile.size() != size_t(0))
    j["iccFile"] = m_iccFile;
  else
    j["iccFile"] = nullptr;

  j["intent"] = icGetRenderingIntentName(m_intent);
  // Pass the overprint so "namedOnBlack"/"namedOnGray" round-trip; for
  // non-named transforms the overprint is OverWhite and the writer picks
  // the same row it would have without this argument.
  j["transform"] = icGetTransformName(m_transform, m_nOverprint);
  json iccMap;
  if (jsonFromEnvMap(iccMap, m_iccEnvVars))
    j["iccEnvVars"] = iccMap;
  if (m_pccFile.size() != size_t(0))
    j["pccFile"] = m_pccFile;
  json pccMap;
  if (jsonFromEnvMap(pccMap, m_pccEnvVars))
    j["pccEnvVars"] = pccMap;
  if (m_adjustPcsLuminance)
    j["adjustPcsLuminance"] = m_adjustPcsLuminance;
  if (m_useBPC)
    j["useBPC"] = m_useBPC;
  if (m_useHToS)
    j["useHToS"] = m_useHToS;
  if (m_useV5SubProfile)
    j["useV5SubProfile"] = m_useV5SubProfile;
  if (m_useD2BxB2Dx)
    j["useD2BxB2Dx"] = m_useD2BxB2Dx;
  int i;
  for (i = 0; icInterpNames[i]; i++)
    if (icInterpValues[i] == m_interpolation)
      break;
  if (icInterpNames[i] && icInterpValues[i] == icInterpLinear)
    j["interpolation"] = icInterpNames[i];
}

CIccCfgProfileSequence::CIccCfgProfileSequence()
{
}

void CIccCfgProfileSequence::reset()
{
  m_profiles.clear();
}

int CIccCfgProfileSequence::fromArgs(const char** args, int nArg, bool bReset)
{
  int nUsed = 0;
  if (bReset)
    reset();

  icXformInterp interpolation = icInterpTetrahedral;

  if (nArg > 1) {
    int nInterpVal = atoi(args[0]);
    interpolation = (nInterpVal == 0) ? icInterpLinear : icInterpTetrahedral;
    args++;
    nArg--;
    nUsed++;
  }
  
  bool bFirst = true;
  while (nArg >= 2) {
    CIccCfgProfilePtr pProf = CIccCfgProfilePtr(new CIccCfgProfile());

    while (nArg >= 2 && !strnicmp(args[0], "-ENV:", 5)) {  //check for -ENV: to allow for Cmm Environment variables to be defined for next transform
      icSignature sig = icGetSigVal(args[0] + 5);
      icFloatNumber val = (icFloatNumber)atof(args[1]);
      args += 2;
      nArg -= 2;
      nUsed += 2;

      pProf->m_iccEnvVars[sig] = val;
    }

    if (nArg >= 2) {

      pProf->m_iccFile = args[0];
      if (bFirst) {
        if (!stricmp(args[0], "-embedded")) {
          pProf->m_iccFile.clear();
        }
        bFirst = false;
      }
      int nType;
      int nIntent = atoi(args[1]);

      pProf->m_useD2BxB2Dx = true;
      // Overprint variant for NamedColor xforms is encoded as the millions
      // digit of the intent code:
      //   +1000000 -> icNamedColorOverBlack (spcb)
      //   +2000000 -> icNamedColorOverGray  (spcg)
      // Anything else leaves the default icNamedColorOverWhite (spec).
      // Strip the field before the existing decimal-coded flags are read.
      {
        int overprintCode = nIntent / 1000000;
        if (overprintCode == 1)
          pProf->m_nOverprint = icNamedColorOverBlack;
        else if (overprintCode == 2)
          pProf->m_nOverprint = icNamedColorOverGray;
        else
          pProf->m_nOverprint = icNamedColorOverWhite;
        nIntent = nIntent % 1000000;
      }
      pProf->m_useHToS = (nIntent / 100000) != 0;
      pProf->m_useV5SubProfile = (nIntent / 10000) != 0;
      nIntent = nIntent % 10000;
      pProf->m_adjustPcsLuminance = nIntent / 1000 != 0;
      nIntent = nIntent % 1000;
      nType = abs(nIntent) / 10;
      nIntent = nIntent % 10;

      switch (nType) {
      case 1:
        pProf->m_useD2BxB2Dx = false;
        nType = icXformLutColor;
        break;
      case 3:
        nType = icXformLutGamut;
        break;
      case 4:
        pProf->m_useBPC = true;
        nType = icXformLutColor;
        break;
      default:
        break;
      }

      pProf->m_intent = (icRenderingIntent)nIntent;
      pProf->m_transform = (icXformLutType)nType;
      pProf->m_interpolation = interpolation;

      args += 2;
      nArg -= 2;
      nUsed += 2;
    }

    if (nArg >= 2 && !stricmp(args[0], "-PCC")) {
      pProf->m_pccFile = args[1];
      args += 2;
      nArg -= 2;
      nUsed += 2;
    }
    m_profiles.push_back(pProf);
  }

  return m_profiles.size()>0 ? nUsed : 0;
}

bool CIccCfgProfileSequence::fromJson(json j, bool bReset)
{
  if (!j.is_array())
    return false;

  CIccCfgProfileArray profiles = bReset ? CIccCfgProfileArray() : m_profiles;

  for (auto p = j.begin(); p != j.end(); p++) {
    if (p->is_object()) {
      CIccCfgProfilePtr pProf(new CIccCfgProfile);
      if (pProf->fromJson(*p)) {
        profiles.push_back(pProf);
      }
      else {
        return false;
      }
    }
    else
      return false;
  }

  m_profiles.swap(profiles);
  return true;
}

void CIccCfgProfileSequence::toJson(json& obj) const
{
  for (auto p = m_profiles.begin(); p != m_profiles.end(); p++) {
    const CIccCfgProfile* pProf = p->get();
    if (!pProf)
      continue;
    json prof;
    pProf->toJson(prof);
    obj.push_back(prof);
  }
}

CIccCfgPccWeight::CIccCfgPccWeight()
{
  reset();
}

CIccCfgPccWeight::~CIccCfgPccWeight() {

}

void CIccCfgPccWeight::reset()
{
  m_pccPath.clear();
  m_dWeight = 0.0f;
}

int CIccCfgPccWeight::fromArgs(const char** args, int nArg, bool bReset)
{
  int nUsed = 0;

  if (bReset)
    reset();

  if (nArg >= 2) {
    m_pccPath = args[0];
    m_dWeight = (icFloatNumber)atof(args[1]);

//    args += 2;    // static analysis says value unread
    nUsed += 2;
  }

  return nUsed;
}

bool CIccCfgPccWeight::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  jsonToValue(j["pccFile"], m_pccPath);
  jsonToValue(j["weight"], m_dWeight);
  
  return true;
}

void CIccCfgPccWeight::toJson(json& obj) const
{
  obj["pccFile"] = m_pccPath;
  obj["weight"] = m_dWeight;
}

CIccCfgSearchApply::CIccCfgSearchApply()
{
  reset();
}

void CIccCfgSearchApply::reset()
{
  m_bInitialized = false;
  m_intentInitial = icUnknownIntent;
  m_transformInitial = icXformLutColor;
  m_useD2BxB2DxInitial = false;
  m_adjustPcsLuminanceInitial = false;
  m_useV5SubProfileInitial = false;
  m_interpolationInitial = icInterpTetrahedral;
  m_pccWeights.clear();
  m_profiles.clear();
}

int CIccCfgSearchApply::fromArgs(const char** args, int nArg, bool bReset)
{
  int nUsed = 0;
  if (bReset)
    reset();

  icXformInterp interpolation = icInterpTetrahedral;

  if (nArg > 1) {
    int nInterpVal = atoi(args[0]);
    interpolation = (nInterpVal == 0) ? icInterpLinear : icInterpTetrahedral;
    args++;
    nArg--;
    nUsed++;
  }

  bool bFirst = true;
  while (nArg >= 2) {

    if (!stricmp(args[0], "-INIT"))
      break;

    CIccCfgProfilePtr pProf = CIccCfgProfilePtr(new CIccCfgProfile());

    while (nArg >= 2 && !strnicmp(args[0], "-ENV:", 5)) {  //check for -ENV: to allow for Cmm Environment variables to be defined for next transform
      icSignature sig = icGetSigVal(args[0] + 5);
      icFloatNumber val = (icFloatNumber)atof(args[1]);
      args += 2;
      nArg -= 2;
      nUsed += 2;

      pProf->m_iccEnvVars[sig] = val;
    }

    if (nArg >= 2) {

      pProf->m_iccFile = args[0];
      if (bFirst) {
        if (!stricmp(args[0], "-embedded")) {
          pProf->m_iccFile.clear();
        }
        bFirst = false;
      }
      int nType;
      int nIntent = atoi(args[1]);

      pProf->m_useD2BxB2Dx = true;
      // Overprint variant for NamedColor xforms is encoded as the millions
      // digit of the intent code:
      //   +1000000 -> icNamedColorOverBlack (spcb)
      //   +2000000 -> icNamedColorOverGray  (spcg)
      // Anything else leaves the default icNamedColorOverWhite (spec).
      // Strip the field before the existing decimal-coded flags are read.
      {
        int overprintCode = nIntent / 1000000;
        if (overprintCode == 1)
          pProf->m_nOverprint = icNamedColorOverBlack;
        else if (overprintCode == 2)
          pProf->m_nOverprint = icNamedColorOverGray;
        else
          pProf->m_nOverprint = icNamedColorOverWhite;
        nIntent = nIntent % 1000000;
      }
      pProf->m_useHToS = (nIntent / 100000) != 0;
      pProf->m_useV5SubProfile = (nIntent / 10000) != 0;
      nIntent = nIntent % 10000;
      pProf->m_adjustPcsLuminance = nIntent / 1000 != 0;
      nIntent = nIntent % 1000;
      nType = abs(nIntent) / 10;
      nIntent = nIntent % 10;

      if (nType == 1) {
        pProf->m_useD2BxB2Dx = false;
        nType = icXformLutColor;
      }
      else if (nType == 4) {
        pProf->m_useBPC = true;
        nType = icXformLutColor;
      }

      pProf->m_intent = (icRenderingIntent)nIntent;
      pProf->m_transform = (icXformLutType)nType;
      pProf->m_interpolation = interpolation;

      args += 2;
      nArg -= 2;
      nUsed += 2;
    }

    if (nArg >= 2 && !stricmp(args[0], "-PCC")) {
      pProf->m_pccFile = args[1];
      args += 2;
      nArg -= 2;
      nUsed += 2;
    }
    m_profiles.push_back(pProf);
  }

  if (nArg >= 2 && !strcmp(args[0], "-INIT")) {
    int nIntent = atoi(args[1]);
    int nType;

    m_bInitialized = true;

    m_useD2BxB2DxInitial = true;
    nIntent = nIntent % 100000;
    m_useV5SubProfileInitial = (nIntent / 10000) != 0;
    nIntent = nIntent % 10000;
    m_adjustPcsLuminanceInitial = nIntent / 1000 != 0;
    nIntent = nIntent % 1000;
    nType = abs(nIntent) / 10;
    nIntent = nIntent % 10;

    if (nType == 1) {
      m_useD2BxB2DxInitial = false;
      nType = icXformLutColor;
    }
    else if (nType == 4) {
      nType = icXformLutColor;
    }

    m_intentInitial = (icRenderingIntent)nIntent;
    m_transformInitial = (icXformLutType)nType;
    m_interpolationInitial = interpolation;

    args += 2;
    nArg -= 2;
    nUsed += 2;
  }
  
  while (nArg >= 2) {
    CIccCfgPccWeightPtr pPccWeight = CIccCfgPccWeightPtr(new CIccCfgPccWeight());
    pPccWeight->m_pccPath = args[0];
    pPccWeight->m_dWeight = (icFloatNumber)atof(args[1]);

    m_pccWeights.push_back(pPccWeight);

    args += 2;
    nArg -= 2;
    nUsed += 2;
  }

  return m_profiles.size() > 0 ? nUsed : 0;
}

bool CIccCfgSearchApply::fromJsonProfiles(json j)
{
  if (!j.is_array())
    return false;

  for (auto p = j.begin(); p != j.end(); p++) {
    if (p->is_object()) {
      CIccCfgProfilePtr pProf(new CIccCfgProfile);
      if (pProf->fromJson(*p)) {
        m_profiles.push_back(pProf);
      }
      else {
        pProf.reset();
      }
    }
  }

  return true;
}

void CIccCfgSearchApply::toJsonProfiles(json& obj) const
{
  for (auto p = m_profiles.begin(); p != m_profiles.end(); p++) {
    const CIccCfgProfile* pProf = p->get();
    if (!pProf)
      continue;
    json prof;
    pProf->toJson(prof);
    obj.push_back(prof);
  }
}

bool CIccCfgSearchApply::fromJsonPccWeights(json j)
{
  if (j.is_null())
    return true;

  if (!j.is_array())
    return false;

  for (auto p = j.begin(); p != j.end(); p++) {
    if (p->is_object()) {
      CIccCfgPccWeightPtr pPccWeight(new CIccCfgPccWeight);
      if (pPccWeight->fromJson(*p)) {
        m_pccWeights.push_back(pPccWeight);
      }
      else {
        pPccWeight.reset();
      }
    }
  }

  return true;
}

void CIccCfgSearchApply::toJsonPccWeights(json& obj) const
{
  for (auto p = m_pccWeights.begin(); p != m_pccWeights.end(); p++) {
    const CIccCfgPccWeight* pPccWeight = p->get();
    if (!pPccWeight)
      continue;
    json prof;
    pPccWeight->toJson(prof);
    obj.push_back(prof);
  }
}

void CIccCfgSearchApply::toJsonInit(json &j) const
{
  j["intent"] = icGetRenderingIntentName(m_intentInitial);
  if (m_adjustPcsLuminanceInitial)
    j["adjustPcsLuminance"] = m_adjustPcsLuminanceInitial;
  if (m_useV5SubProfileInitial)
    j["useV5SubProfile"] = m_useV5SubProfileInitial;
  int i;
  for (i = 0; icTranNames[i]; i++)
    if (icTranValues[i] == m_transformInitial)
      break;
  if (icTranNames[i])
    j["transform"] = icTranNames[i];
  for (i = 0; icInterpNames[i]; i++)
    if (icInterpValues[i] == m_interpolationInitial)
      break;
  if (icInterpNames[i])
    j["interpolation"] = icInterpNames[i];

}

bool CIccCfgSearchApply::fromJsonInit(json j)
{
  if (!j.is_object())
    return false;

  m_bInitialized = true;

  int intent = icUnknownIntent; // just incase json parsing fails
  icGetJsonRenderingIntent(j["intent"], intent);
  m_intentInitial = (icRenderingIntent)intent;

  std::string str;
  if (jsonToValue(j["transform"], str)) {
    int i;
    for (i = 0; icTranNames[i]; i++) {
      if (str == icTranNames[i])
        break;
    }
    if (icTranNames[i])
      m_transformInitial = (icXformLutType)icTranValues[i];
  }

  jsonToValue(j["adjustPcsLuminance"], m_adjustPcsLuminanceInitial);
  jsonToValue(j["useV5SubProfile"], m_useV5SubProfileInitial);

  if (jsonToValue(j["interpolation"], str)) {
    int i;
    for (i = 0; icInterpNames[i]; i++) {
      if (str == icInterpNames[i])
        break;
    }
    if (icInterpNames[i])
      m_interpolationInitial = icInterpValues[i];
  }

  return true;
}

void CIccCfgSearchApply::toJson(json& j) const
{
  if (m_profiles.size() != size_t(0))
    toJsonProfiles(j["profileSequence"]);
  if (m_bInitialized)
    toJsonInit(j["initial"]);
  if (m_pccWeights.size() != size_t(0))
    toJsonPccWeights(j["pccWeights"]);
}

bool CIccCfgSearchApply::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  return fromJsonProfiles(j["profileSequence"]) && 
         fromJsonInit(j["initial"]) &&
         fromJsonPccWeights(j["pccWeights"]);
}



class CIccIt8Parser
{
public:
  CIccIt8Parser() { m_f = nullptr; }
  ~CIccIt8Parser() { delete m_f; }

  bool open(const char* szFilename) {
    delete m_f;
    m_f = new (std::nothrow) std::ifstream(szFilename);
    if (!m_f || !m_f->is_open()) {
      delete m_f;
      m_f = nullptr;
      return false;
    }
    return true;
  }
  bool isEOF() { return m_f->eof(); }
  bool parseLine(std::vector<std::string>& line) {
    line.clear();
    if (isEOF())
      return false;
    std::string str;
    bool bHasField = false;

    while (!isEOF()) {
      int c = m_f->get();
      if (c < 0) {
        if (str.size() != size_t(0))
          line.push_back(str);
        if (line.size() == size_t(0))
          return false;
        break;
      }
      else if (c == '\n' || c == '\r') {
        if (c == '\r' && m_f->peek() == '\n')
          m_f->get();
        if ((str.size() != size_t(0)) || bHasField)
          line.push_back(str);
        break;
      }
      else if (c == '\t') {
        line.push_back(str);
        str.clear();
        bHasField = true;
      }
      else {
        str += c;
      }
    }
    return true;
  }

  bool parseNextLine(std::vector<std::string>& line) {
    while (parseLine(line)) {
      if (line.size() != size_t(0))
        return true;
    }
    return false;
  }

  bool findTokenLine(std::vector<std::string>& line, const char* szToken) {
    while (parseNextLine(line)) {
      if (line[0] == szToken)
        return true;
    }
    return false;
  }

protected:
  std::ifstream* m_f;
};

CIccCfgColorData::CIccCfgColorData()
{
  reset();
}

void CIccCfgColorData::reset()
{
  m_space = icSigUnknownData;
  m_encoding = icEncodeValue;

  m_srcSpace = icSigUnknownData;
  m_srcEncoding = icEncodeValue;

  m_data.clear();
}

static bool ParseNumbers(icFloatNumber* pData, icChar* pString, icUInt32Number nSamples)
{
  icUInt32Number nNumbersRead = 0;

  icChar* ptr = pString;

  while (*ptr && nNumbersRead < nSamples) {
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')
      ptr++;
    if (sscanf(ptr, ICFLOATFMT, &pData[nNumbersRead]) == 1)
      nNumbersRead++;
    else
      break;
    while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r')
      ptr++;
  }

  if (nNumbersRead != nSamples) {
    return false;
  }

  return true;
}

static bool ParseNextNumber(icFloatNumber& num, icChar** text)
{
  if (!text)
    return false;

  icChar* ptr = *text;
  while (*ptr == ' ') ptr++;
  if ((*ptr >= '0' && *ptr <= '9') || *ptr == '.') {
    num = (icFloatNumber)atof(ptr);

    while (*ptr && *ptr != ' ') ptr++;

    *text = ptr;

    return true;
  }
  return false;
}

//===================================================

static bool ParseName(icChar* pName, icChar* pString)
{
  if (strncmp(pString, "{ \"", 3))
    return false;

  const icChar* ptr = strstr(pString, "\" }");

  if (!ptr)
    return false;

  icUInt32Number nNameLen = (icUInt32Number)(ptr - (pString + 3));

  if (!nNameLen)
    return false;

  strncpy(pName, pString + 3, nNameLen);
  pName[nNameLen] = '\0';

  return true;
}


bool CIccCfgColorData::fromLegacy(const char* filename, bool bReset)
{
  if (bReset)
    reset();

  std::ifstream InputData(filename);

  if (!InputData) {
    return false;
  }

  int tempBufSize = 20000;
  icChar ColorSig[7], *tempBuf = new (std::nothrow) icChar[tempBufSize];
  if (!tempBuf)
    return false;
  InputData.getline(tempBuf, tempBufSize);

  int i;
  for (i = 0; (i < 4 || tempBuf[i + 1] != '\'') && i < 6; i++) {
    ColorSig[i] = tempBuf[i + 1];
  }
  for (; i < 7; i++)
    ColorSig[i] = '\0';

  //Init source number of samples from color signature is source data file
  m_srcSpace = (icColorSpaceSignature)icGetSigVal(ColorSig);
  int nSamples = icGetSpaceSamples(m_srcSpace);
  if (m_srcSpace != icSigNamedData) {
    if (!nSamples) {
      delete[] tempBuf;
      return false;
    }
  }

  InputData.getline(tempBuf, tempBufSize);
  {
    char encodeBuf[20000];
    sscanf(tempBuf, "%19999s", encodeBuf);
    strncpy(tempBuf, encodeBuf, tempBufSize - 1);
    tempBuf[tempBufSize - 1] = '\0';
  }

  //Setup source encoding
  m_encoding = CIccCmm::GetFloatColorEncoding(tempBuf);
  if (m_encoding == icEncodeUnknown) {
    delete[] tempBuf;
    return false;
  }
  char SrcNameBuf[256];
  int nSrcSamples = icGetSpaceSamples(m_srcSpace);
  CIccPixelBuf Pixel(nSrcSamples + 16);

  while (!InputData.eof()) {
    CIccCfgDataEntryPtr data(new CIccCfgDataEntry());

    //Are names coming is as an input?
    if (m_srcSpace == icSigNamedData) {
      InputData.getline(tempBuf, tempBufSize);
      if (!ParseName(SrcNameBuf, tempBuf))
        continue;

      data->m_name = SrcNameBuf;

      icChar* numptr = strstr(tempBuf, "\" }");
      if (numptr)
        numptr += 3;

      icFloatNumber tint;
      if (!ParseNextNumber(tint, &numptr))
        tint = 1.0;
      data->m_values.push_back(tint);
    }
    else { //pixel sample data coming in as input

      InputData.getline(tempBuf, tempBufSize);
      if (!ParseNumbers(Pixel, tempBuf, nSamples))
        continue;

      for (int n = 0; n < nSamples; n++) {
        data->m_values.push_back(Pixel[n]);
      }
    }

    m_data.push_back(data);
  }
  delete[] tempBuf;
  return true;
}

class CIccIndexValue
{
public:
  CIccIndexValue() {
    nIndex = -1;
    nValue = 0;
    space = icSigUnknownData;
  }

  int nIndex;
  icFloatNumber nValue;
  icColorSpaceSignature space;
};

typedef std::pair<int, std::string> icIndexName;
typedef std::vector<CIccIndexValue> icValueVector;

static void setSampleIndex(std::vector<icValueVector>& samples, int index, const char* szFmt, const char** szChannels)
{
  if (samples.size() < 1)
    return;
  size_t nPos = samples.size() - 1;
  for (size_t i = 0; i < samples[nPos].size(); i++) {
    if (!strcmp(szFmt, szChannels[i])) {
      samples[nPos][i].nIndex = index;
    }
  }
}

bool CIccCfgColorData::fromIt8(const char* filename, bool bReset)
{
  if (bReset)
    reset();

  CIccIt8Parser f;

  if (!f.open(filename))
    return false;

  std::vector<std::string> line;
  if (!f.findTokenLine(line, "CGATS.17"))
    return false;
  if (!f.findTokenLine(line, "NUMBER_OF_FIELDS"))
    return false;
  size_t nFields = 0;
  if (line.size() >= 2) {
    nFields = (size_t)atoi(line[1].c_str());
  }
  if (!f.findTokenLine(line, "BEGIN_DATA_FORMAT"))
    return false;
  if (!f.parseNextLine(line)) {
    return false;
  }
  if (line.size() != nFields)
    return false;

  std::string lastSpace;
  std::vector<icValueVector> samples;
  std::vector<std::string> spaces;
  std::vector<icIndexName> names;

  int nId = -1;
  int nLabel = -1;

  int index = 0;
  for (auto fmt = line.begin(); fmt != line.end(); fmt++, index++) {
    if (*fmt == "SAMPLE_ID") {
      nId = index;
    }
    else if (*fmt == "SAMPLE_NAME") {
      nLabel = index;
    }
    else {
      std::string space;
      const char* szFmt = fmt->c_str();
      if (!strncmp(szFmt, "SRC_", 4)) {
        space += "SRC_";
        szFmt += 4;
      }
      if (!strcmp(szFmt, "NAME")) {
        space += "NAME";
        names.push_back(icIndexName(index, space));
      }
      else if (!strncmp(szFmt, "TINT", 4)) {
        space += "TINT";
        if (space != lastSpace) {
          icValueVector val(1);
          val[0].nIndex = index;
          val[0].space = icSigNamedData;
          samples.push_back(val);
          lastSpace = "";
        }
      }
      else if (!strncmp(szFmt, "RGB_", 4)) {
        space += "RGB";
        szFmt += 4;
        if (space != lastSpace) {
          lastSpace = space;
          icValueVector val(3);
          val[0].space = icSigRgbData;
          samples.push_back(val);
          spaces.push_back(space);
        }
        const char* szChannels[] = { "R", "G", "B" };
        setSampleIndex(samples, index, szFmt, &szChannels[0]);
      }
      else if (!strncmp(szFmt, "CMYK_", 5)) {
        space += "CMYK";
        szFmt += 5;
        if (space != lastSpace) {
          lastSpace = space;
          icValueVector val(4);
          val[0].space = icSigCmykData;
          samples.push_back(val);
          spaces.push_back(space);
        }
        const char* szChannels[] = { "C", "M", "Y", "K" };
        setSampleIndex(samples, index, szFmt, &szChannels[0]);
      }
      else if (!strncmp(szFmt, "LAB_", 4)) {
      space += "LAB";
      szFmt += 4;
      if (space != lastSpace) {
        lastSpace = space;
        icValueVector val(4);
        val[0].space = icSigLabData;
        samples.push_back(val);
        spaces.push_back(space);
      }
      const char* szChannels[] = { "L", "A", "B" };
      setSampleIndex(samples, index, szFmt, &szChannels[0]);
      }
      else if (!strncmp(szFmt, "XYZ_", 4)) {
      space += "XYZ";
      szFmt += 4;
      if (space != lastSpace) {
        lastSpace = space;
        icValueVector val(4);
        val[0].space = icSigXYZData;
        samples.push_back(val);
        spaces.push_back(space);
      }
      const char* szChannels[] = { "X", "Y", "Z" };
      setSampleIndex(samples, index, szFmt, &szChannels[0]);
      }
      else {
      int nColor = -1;
      if ( (sscanf(szFmt, "%dCOLOR_", &nColor) == 1) && nColor >= 1) {  // sscanf can also return EOF(-1)
        const size_t bufSize = 30;
        char buf[bufSize];
        snprintf(buf, bufSize, "%dCOLOR", nColor);
        space += buf;

        if (space != lastSpace) {
          lastSpace = space;
          icValueVector val(nColor);
          val[0].space = (icColorSpaceSignature)(icSigNChannelData + (nColor & 0xffff));
          samples.push_back(val);
          spaces.push_back(space);
        }

        szFmt = strchr(szFmt, '_');
        if (szFmt) {
          szFmt++;
          size_t nChannel = (size_t)atoi(szFmt);
          if (samples.size() > 1) {
            size_t last = samples.size() - 1;
            if (nChannel > 0 && samples[last].size() >= nChannel) {
              samples[samples.size() - 1][nChannel - 1].nIndex = index;
            }
          }
        }
      }
      }
    }
  }

  if (!f.findTokenLine(line, "END_DATA_FORMAT"))
    return false;

  if (!f.findTokenLine(line, "NUMBER_OF_SETS"))
    return false;

  size_t nSets = 0;
  if (line.size() > 1) {
    nSets = (size_t)atoi(line[1].c_str());
  }

  if (!f.findTokenLine(line, "BEGIN_DATA"))
    return false;

  do {
    if (!f.parseNextLine(line) || line[0] == "END_DATA")
      break;
    CIccCfgDataEntryPtr pData(new CIccCfgDataEntry);

    if (nId >= 0 && (size_t)nId < line.size()) {
      pData->m_index = atoi(line[nId].c_str());
    }
    else if (nLabel >= 0 && (size_t)nLabel < line.size()) {
      pData->m_label = line[nLabel];
    }

    if (names.size() > 0) {
      if (names[0].second == "NAME") {
        if (names[0].first >= 0)
          pData->m_name = line[names[0].first];
        if (names.size() > 1) {
          if (names[1].first >= 0)
            pData->m_srcName = line[names[1].first];
        }
      }
      else if (names[0].second == "SRC_NAME") {
        if (names[0].first >= 0)
          pData->m_srcName = line[names[0].first];
        if (names.size() > 1) {
          if (names[1].first >= 0)
            pData->m_name = line[names[1].first];
        }
      }
    }

    size_t nValueIdx;
    for (nValueIdx = 0; nValueIdx < samples.size(); nValueIdx++) {
      if (samples[nValueIdx][0].space == m_space)
        break;
    }
    if (nValueIdx != samples.size()) {
      for (size_t i = 0; i < samples[nValueIdx].size(); i++) {
        size_t nPos = samples[nValueIdx][i].nIndex;
        if (nPos < line.size()) {
          pData->m_values.push_back((icFloatNumber)atof(line[nPos].c_str()));
        }
      }
    }
    else {
      for (nValueIdx = 0; nValueIdx < spaces.size(); nValueIdx++) {
        if (strncmp(spaces[nValueIdx].c_str(), "SRC_", 4))
          break;
      }
      if (nValueIdx != spaces.size() && nValueIdx < samples.size()) {
        m_space = samples[nValueIdx][0].space;
        for (size_t j = 0; j < samples[nValueIdx].size(); j++) {
          size_t nPos = samples[nValueIdx][j].nIndex;
          if (nPos < line.size()) {
            pData->m_values.push_back((icFloatNumber)atof(line[nPos].c_str()));
          }
        }
      }
    }

    size_t nSrcIndex;
    for (nSrcIndex = 0; nSrcIndex < samples.size(); nSrcIndex++) {
      if (samples[nSrcIndex][0].space == m_srcSpace)
        break;
    }
    if (nSrcIndex != samples.size() && nSrcIndex != nValueIdx) {
      for (size_t i = 0; i < samples[nSrcIndex].size(); i++) {
        size_t nPos = samples[nSrcIndex][i].nIndex;
        if (nPos < line.size()) {
          pData->m_values.push_back((icFloatNumber)atof(line[nPos].c_str()));
        }
      }
    }
    else {
      for (nSrcIndex = 0; nSrcIndex < spaces.size(); nSrcIndex++) {
        if (!strncmp(spaces[nValueIdx].c_str(), "SRC_", 4))
          break;
      }
      if (nSrcIndex != spaces.size() && nSrcIndex < samples.size()) {
        m_srcSpace = samples[nSrcIndex][0].space;
        for (size_t j = 0; j < samples[nValueIdx].size(); j++) {
          size_t nPos = samples[nValueIdx][j].nIndex;
          if (nPos < line.size()) {
            pData->m_srcValues.push_back((icFloatNumber)atof(line[nPos].c_str()));
          }
        }
      }
      else {
        for (nSrcIndex = 0; nSrcIndex < samples.size(); nSrcIndex++) {
          if (nSrcIndex != nValueIdx)
            break;
        }
        if (nSrcIndex < samples.size()) {
          m_srcSpace = samples[nSrcIndex][0].space;
          for (size_t j = 0; j < samples[nValueIdx].size(); j++) {
            size_t nPos = samples[nValueIdx][j].nIndex;
            if (nPos < line.size()) {
              pData->m_srcValues.push_back((icFloatNumber)atof(line[nPos].c_str()));
            }
          }
        }
      }
    }

    if (   pData->m_values.size() != size_t(0)
        || pData->m_srcValues.size() != size_t(0)
        || pData->m_name.size() != size_t(0)
        || pData->m_srcName.size() != size_t(0))
      m_data.push_back(pData);
    else
      pData.reset();

  } while (!f.isEOF());

  return m_data.size() == nSets;
}

bool CIccCfgColorData::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  std::string str;

  jsonToColorSpace(j["space"], m_space);
  if (jsonToValue(j["encoding"], str))
    m_encoding = icSetJsonColorEncoding(str.c_str());

  jsonToColorSpace(j["srcSpace"], m_srcSpace);
  if (jsonToValue(j["srcEncoding"], str))
    m_srcEncoding = icSetJsonColorEncoding(str.c_str());

  if (j.find("data")!=j.end()) {
    json data = j["data"];
    if (data.is_array()) {
      for (auto d = data.begin(); d != data.end(); d++) {
        if (d->is_object()) {
          CIccCfgDataEntryPtr entry(new CIccCfgDataEntry);
          if (entry->fromJson(*d)) {
            m_data.push_back(entry);
          }
          else
            entry.reset();
        }
      }
    }
  }

  return true;
}

bool CIccCfgColorData::toLegacy(const char* filename, const CIccCfgProfileArray &profiles, icUInt8Number nDigits, icUInt8Number nPrecision, bool bShowDebug)
{
  FILE* f;
  const size_t tempSize = 256;
  char tempBuf[tempSize];
  char tempBuf2[tempSize];
  if (nDigits > 20) nDigits = 20;
  if (nPrecision > 20) nPrecision = 20;

  f = icOpenWriteTextFile(filename);

  if (!f)
    return false;

  auto writeFloat = [&](icFloatNumber v)->bool {
    return icFormatFloatValue(tempBuf, tempSize, nDigits, nPrecision, v) &&
           icWriteString(f, tempBuf);
  };

  std::string out;
  snprintf(tempBuf, tempSize, "%s\t; ", icGetColorSig(tempBuf2, tempSize, m_space, false));
  out = tempBuf;
  out += "Data Format\n";
  if (!icWriteString(f, out)) { if (f != stdout) fclose(f); return false; }

  snprintf(tempBuf, tempSize, "%s\t; ", CIccCmm::GetFloatColorEncoding(m_encoding));
  out = tempBuf;
  out += "Encoding\n\n";
  if (!icWriteString(f, out)) { if (f != stdout) fclose(f); return false; }

  out = ";Source Data Format: ";
  snprintf(tempBuf, tempSize, "%s\n", icGetColorSig(tempBuf2, tempSize, m_srcSpace, false));
  out += tempBuf;
  if (!icWriteString(f, out)) { if (f != stdout) fclose(f); return false; }

  out = ";Source Data Encoding: ";
  snprintf(tempBuf, tempSize, "%s\n", CIccCmm::GetFloatColorEncoding(m_srcEncoding));
  out += tempBuf;
  if (!icWriteString(f, out)) { if (f != stdout) fclose(f); return false; }

  fprintf(f, ";Source data is after semicolon\n");
  
  fprintf(f, "\n;Profiles applied\n");
  for (auto pIter = profiles.begin(); pIter != profiles.end(); pIter++) {
    CIccCfgProfile* pProf = pIter->get();
    if (!pProf)
      continue;
    if (pProf->m_pccFile.size() != size_t(0)) {
      fprintf(f, "; %s -PCC %s\n", pProf->m_iccFile.c_str(), pProf->m_pccFile.c_str());
    }
    else {
      fprintf(f, "; %s\n", pProf->m_iccFile.c_str());
    }
  }
fprintf(f, "\n");

  for (auto dIter = m_data.begin(); dIter != m_data.end(); dIter++) {
    CIccCfgDataEntry* pData = dIter->get();
    if (!pData)
      continue;

    if (bShowDebug && pData->m_debugInfo.size() != size_t(0)) {
      for (auto l = pData->m_debugInfo.begin(); l != pData->m_debugInfo.end(); l++) {
        fprintf(f, "; %s\n", l->c_str());
      }
    }

    if (pData->m_name.size() != size_t(0)) {
      fprintf(f, "{ \"%s\" }\t;", pData->m_name.c_str());
    }
    else {
      for (size_t i = 0; i < pData->m_values.size(); i++) {
        if (!writeFloat(pData->m_values[i])) { if (f != stdout) fclose(f); return false; }
      }
      fprintf(f, "\t;");
    }

    if (pData->m_srcName.size() != size_t(0)) {
      fprintf(f,"{ \"%s\" }", pData->m_srcName.c_str());
      if (pData->m_srcValues.size() > 0 && pData->m_srcValues[0] != 1.0) {
        if (!writeFloat(pData->m_srcValues[0])) { if (f != stdout) fclose(f); return false; }
      }
    }
    else {
      for (size_t i = 0; i < pData->m_srcValues.size(); i++) {
        if (!writeFloat(pData->m_srcValues[i])) { if (f != stdout) fclose(f); return false; }
      }
    }
    fprintf(f, "\n");
  }

  if (!icCloseWriteTextFile(f))
    return false;

  return true;
}

std::string CIccCfgColorData::spaceName(icColorSpaceSignature sig)
{
  switch (sig) {
    case icSigRgbData:
      return "RGB";
    case icSigCmyData:
      return "CMY";
    case icSigCmykData:
      return "CMYK";
    case icSigDevXYZData:
    case icSigXYZData:
      return "XYZ";
    case icSigDevLabData:
    case icSigLabData:
      return "LAB";
    case icSigNamedData:
      return "TINT";
    default:
    {
      int nSamples = icGetSpaceSamples(sig);
      const size_t bufSize = 32;
      char buf[bufSize];
      snprintf(buf, bufSize, "%dCOLOR", nSamples);
      return buf;
    }
  }
}

void CIccCfgColorData::addFields(std::string& dataFormat, int& nFields, int& nSamples, icColorSpaceSignature sig, const std::string& prefix)
{
  std::string tabStr = "\t";
  const size_t bufSize = 32;
  char buf[32];

  switch (sig) {
    case icSigRgbData:
      nSamples = 3;
      if (nFields) dataFormat += tabStr;
      dataFormat += prefix + "RGB_R";
      dataFormat += tabStr + prefix + "RGB_G";
      dataFormat += tabStr + prefix + "RGB_B";
      nFields += nSamples;
      return;
    case icSigCmyData:
      nSamples = 3;
      if (nFields) dataFormat += tabStr;
      dataFormat += prefix + "CMY_C";
      dataFormat += tabStr + prefix + "CMY_M";
      dataFormat += tabStr + prefix + "CMY_Y";
      nFields += nSamples;
      return;
    case icSigCmykData:
      nSamples = 4;
      if (nFields) dataFormat += tabStr;
      dataFormat += prefix + "CMYK_C";
      dataFormat += tabStr + prefix + "CMYK_M";
      dataFormat += tabStr + prefix + "CMYK_Y";
      dataFormat += tabStr + prefix + "CMYK_K";
      nFields += nSamples;
      return;
    case icSigDevLabData:
    case icSigLabData:
      nSamples += 3;
      if (nFields) dataFormat += tabStr;
      dataFormat += prefix + "LAB_L";
      dataFormat += tabStr + prefix + "LAB_A";
      dataFormat += tabStr + prefix + "LAB_B";
      nFields += nSamples;
      return;
    case icSigDevXYZData:
    case icSigXYZData:
      nSamples += 3;
      if (nFields) dataFormat += tabStr;
      dataFormat += prefix + "XYZ_X";
      dataFormat += tabStr + prefix + "XYZ_Y";
      dataFormat += tabStr + prefix + "XYZ_Z";
      nFields += 3;
      return;
    case icSigNamedData:
      nSamples += 1;
      if (nFields) dataFormat += tabStr;
      dataFormat += prefix + "TINT";
      nFields += nSamples;
      return;
    default:
      nSamples = icGetSpaceSamples(sig);
      if (nFields) dataFormat += tabStr;
      for (int i = 0; i < nSamples; i++) {
        snprintf(buf, bufSize, "%dCOLOR_%d", nSamples, i + 1);
        if (i)
          dataFormat += tabStr;
        dataFormat += buf;
      }
      nFields += nSamples;
      return;
  }
}

bool CIccCfgColorData::toIt8(const char* filename, icUInt8Number nDigits, icUInt8Number nPrecision)
{
  if (m_data.size() == size_t(0))
    return false;

  FILE* f;
  if (nDigits > 20) nDigits = 20;
  if (nPrecision > 20) nPrecision = 20;

  auto first = m_data.begin();
  CIccCfgDataEntry* pEntry = first->get();

  std::string dataFormat;
  int nFields = 0;
  bool bShowIndex = false;
  bool bShowLabel = false;
  bool bShowSrcName = false;
  bool bShowSrcValues = false;
  bool bShowName = false;
  bool bShowValues = false;
  
  int nSrcSamples = 0, nDstSamples = 0;

  if (pEntry->m_index >= 0) {
    dataFormat = "INDEX";
    nFields = 1;
    bShowIndex = true;
  }

  if (pEntry->m_label.size() != size_t(0)) {
    if (nFields) dataFormat+="\t";
    dataFormat += "SAMPLE_ID";
    nFields++;
    bShowLabel = true;
  }

  bool bSameSpace = spaceName(m_space) == spaceName(m_srcSpace);

  if (pEntry->m_name.size() != size_t(0)) {
    if (nFields) dataFormat += "\t";
    dataFormat += "NAME";
    nFields++;
    bShowName = true;
  }

  if (m_space != icSigUnknownData) {
    addFields(dataFormat, nFields, nDstSamples, m_space, "");
    if (nDstSamples)
      bShowValues = true;
  }

  if (pEntry->m_srcName.size() != size_t(0)) {
    if (nFields) dataFormat += "\t";
    if (bSameSpace)
      dataFormat += "SRC_";
    dataFormat += "NAME";
    nFields++;
    bShowSrcName = true;
  }

  if (m_srcSpace != icSigUnknownData) {
    addFields(dataFormat, nFields, nSrcSamples, m_srcSpace, bSameSpace ? "SRC_" : "");
    if (nSrcSamples)
      bShowSrcValues = true;
  }

  if (!nFields)
    return false;

  f = icOpenWriteTextFile(filename);

  if (!f)
    return false;

  fprintf(f, "CGATS.17\n");
  fprintf(f, "ORIGINATOR\t\"iccDEV\"\n");
  fprintf(f, "FILE_DESCRIPTOR\t\"Color Data\"\n");

  fprintf(f, "NUMBER_OF_FIELDS\t%d\n", nFields);
  fprintf(f, "BEGIN_DATA_FORMAT\n");
  fprintf(f, "%s\n", dataFormat.c_str());
  fprintf(f, "END_DATA_FORMAT\n");
  fprintf(f, "NUMBER_OF_SETS\t%zu\n", m_data.size());

  CIccCfgDataEntry blank;
  const size_t bufSize = 256;
  char buf[bufSize];

  fprintf(f, "BEGIN_DATA\n");
  for (auto e = m_data.begin(); e != m_data.end(); e++) {
    std::string line;

    CIccCfgDataEntry* pCurEntry = e->get();
    if (!pCurEntry)
      pCurEntry = &blank;

    if (bShowIndex) {
      snprintf(buf, bufSize, "%d", pCurEntry->m_index);
      line += buf;
    }

    if (bShowLabel) {
      if (line.size() != size_t(0)) line += "\t";
      if (pCurEntry->m_label.size() == 0)
        line += "\"\"";
      else
        line += pCurEntry->m_label;
    }

    if (bShowName) {
      if (line.size() != size_t(0)) line += "\t";
      if (pCurEntry->m_name.size() == 0)
        line += "\"\"";
      else
        line += pCurEntry->m_name;
    }

    if (bShowValues) {
      if (line.size() != size_t(0)) line += "\t";
      for (size_t i = 0; i < (size_t)nDstSamples; i++) {
        icFloatNumber v = (i >= pCurEntry->m_values.size()) ? 0 : pCurEntry->m_values[i];
        if (!icFormatFloatValue(buf, bufSize, nDigits, nPrecision, v)) {
          if (f != stdout) fclose(f);
          return false;
        }
        if (i)
          line += "\t";
        line += buf;
      }
    }

    if (bShowSrcName) {
      if (line.size() != size_t(0)) line += "\t";
      if (pCurEntry->m_srcName.size() == 0)
        line += "\"\"";
      else
        line += pCurEntry->m_srcName;
    }

    if (bShowSrcValues) {
      if (line.size() != size_t(0)) line += "\t";
      for (size_t i = 0; i < (size_t)nSrcSamples; i++) {
        icFloatNumber v = (i >= pCurEntry->m_srcValues.size()) ? 0 : pCurEntry->m_srcValues[i];
        if (!icFormatFloatValue(buf, bufSize, nDigits, nPrecision, v)) {
          if (f != stdout) fclose(f);
          return false;
        }
        if (i)
          line += "\t";
        line += buf;
      }
    }

    fprintf(f, "%s\n", line.c_str());
  }
  fprintf(f, "END_DATA\n");

  if (!icCloseWriteTextFile(f))
    return false;

  return true;
}

void CIccCfgColorData::toJson(json& obj) const
{
  char buf[32];
  if (m_space != icSigUnknownData)
    obj["space"] = icGetColorSigStr(buf, 32, m_space);
  obj["encoding"] = icGetJsonFloatColorEncoding(m_encoding);
  obj["srcSpace"] = icGetColorSigStr(buf, 32, m_srcSpace);
  obj["srcEncoding"] = icGetJsonFloatColorEncoding(m_srcEncoding);

  json data;
  for (auto e = m_data.begin(); e != m_data.end(); e++) {
    CIccCfgDataEntry *pData = e->get();
    if (!pData)
      continue;

    json entry;
    pData->toJson(entry);
    if (entry.is_object())
      data.push_back(entry);
  }
  if (data.is_array() && data.size() != size_t(0)) {
    obj["data"] = data;
  }
}

CIccCfgDataEntry::CIccCfgDataEntry()
{
  reset();
}

void CIccCfgDataEntry::reset()
{
  m_name.clear();
  m_values.clear();
  m_srcName.clear();
  m_srcValues.clear();
  m_index = -1;
  m_label.clear();
  m_debugInfo.clear();
}

bool CIccCfgDataEntry::fromJson(json j, bool bReset)
{
  if (!j.is_object())
    return false;

  if (bReset)
    reset();

  jsonToValue(j["n"], m_name);
  jsonToArray(j["v"], m_values);

  jsonToValue(j["sn"], m_srcName);
  jsonToArray(j["sv"], m_srcValues);

  jsonToValue(j["i"], m_index);
  jsonToValue(j["l"], m_label);

  jsonToList(j["d"], m_debugInfo);

  return true;
}

void CIccCfgDataEntry::toJson(json& obj)
{
  if (m_name.size() != size_t(0))
    obj["n"] = m_name;
  if (m_values.size() != size_t(0))
    obj["v"] = m_values;

  if (m_srcName.size() != size_t(0))
    obj["sn"] = m_srcName;
  if (m_srcValues.size() != size_t(0))
    obj["sv"] = m_srcValues;

  if (m_label.size() != size_t(0))
    obj["l"] = m_label;

  if (m_index >= 0)
    obj["i"] = m_index;

  if (m_debugInfo.size() != size_t(0))
    obj["d"] = m_debugInfo;
}


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
