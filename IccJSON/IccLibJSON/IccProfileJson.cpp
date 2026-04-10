/** @file
    File:       IccProfileJson.cpp

    Contains:   Implementation of ICC Profile JSON format conversions

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

#include "IccProfileJson.h"
#include "IccTagJson.h"
#include "IccUtilJson.h"
#include "IccUtil.h"
#include "IccTagFactory.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>
#include <map>

typedef std::map<icUInt32Number, icTagSignature> IccOffsetTagSigMap;

// ---------------------------------------------------------------------------
// JSON-specific header flags / device attributes helpers
// ---------------------------------------------------------------------------

static IccJson icJsonGetHeaderFlags(icUInt32Number flags)
{
  IccJson j;
  j["EmbeddedInFile"]          = (bool)(flags & icEmbeddedProfileTrue);
  j["UseWithEmbeddedDataOnly"] = (bool)(flags & icUseWithEmbeddedDataOnly);
  if (flags & icExtendedRangePCS) j["ExtendedRangePCS"] = true;
  if (flags & icMCSNeedsSubsetTrue) j["MCSNeedsSubset"] = true;
  icUInt32Number other = flags & ~(icUInt32Number)(icEmbeddedProfileTrue | icUseWithEmbeddedDataOnly |
                                                   icExtendedRangePCS | icMCSNeedsSubsetTrue);
  if (other) {
    char buf[16]; snprintf(buf, sizeof(buf), "%08x", other);
    j["VendorFlags"] = buf;
  }
  return j;
}

static icUInt32Number icJsonParseHeaderFlags(const IccJson &j)
{
  icUInt32Number flags = 0;
  bool b = false;
  jGetValue(j, "EmbeddedInFile", b);          if (b) flags |= icEmbeddedProfileTrue;
  b = false; jGetValue(j, "UseWithEmbeddedDataOnly", b); if (b) flags |= icUseWithEmbeddedDataOnly;
  b = false; jGetValue(j, "ExtendedRangePCS", b);        if (b) flags |= icExtendedRangePCS;
  b = false; jGetValue(j, "MCSNeedsSubset", b);          if (b) flags |= icMCSNeedsSubsetTrue;
  std::string vendor;
  jGetString(j, "VendorFlags", vendor);
  if (!vendor.empty()) {
    unsigned v = 0;
    sscanf(vendor.c_str(), "%x", &v);
    flags |= v;
  }
  return flags;
}

// icJsonGetDeviceAttr and icJsonParseDeviceAttr are now in IccUtilJson

// ---------------------------------------------------------------------------
// Serialization: profile -> JSON object
// ---------------------------------------------------------------------------

bool CIccProfileJson::ToJson(IccJson &root)
{
  CIccInfo info;
  const size_t bufSize = 256;
  char buf[bufSize];

  // Header
  IccJson header;
  header["PreferredCMMType"]   = m_Header.cmmId      ? icGetSigStr(buf, bufSize, m_Header.cmmId)         : "";
  header["ProfileVersion"]     = info.GetVersionName(m_Header.version);

  if (m_Header.version & 0x0000ffff)
    header["ProfileSubClassVersion"] = info.GetSubClassVersionName(m_Header.version);

  header["ProfileDeviceClass"] = m_Header.deviceClass ? icGetSigStr(buf, bufSize, m_Header.deviceClass) : "";

  if (m_Header.deviceSubClass)
    header["ProfileDeviceSubClass"] = icGetSigStr(buf, bufSize, m_Header.deviceSubClass);

  header["DataColourSpace"]    = m_Header.colorSpace  ? icGetSigStr(buf, bufSize, m_Header.colorSpace)   : "";
  header["PCS"]                = m_Header.pcs         ? icGetSigStr(buf, bufSize, m_Header.pcs)          : "";

  // Creation date/time as ISO-8601
  char dt[64];
  snprintf(dt, sizeof(dt), "%d-%02d-%02dT%02d:%02d:%02d",
           m_Header.date.year, m_Header.date.month,  m_Header.date.day,
           m_Header.date.hours, m_Header.date.minutes, m_Header.date.seconds);
  header["CreationDateTime"] = dt;

  header["ProfileFileSignature"] = icGetSigStr(buf, bufSize, m_Header.magic);
  header["PrimaryPlatform"]    = m_Header.platform    ? icGetSigStr(buf, bufSize, m_Header.platform)     : "";
  header["CMMFlags"]             = icJsonGetHeaderFlags(m_Header.flags);
  header["DeviceManufacturer"] = m_Header.manufacturer ? icGetSigStr(buf, bufSize, m_Header.manufacturer) : "";
  header["DeviceModel"]        = m_Header.model        ? icGetSigStr(buf, bufSize, m_Header.model)        : "";
  header["DeviceAttributes"]     = icJsonGetDeviceAttr(m_Header.attributes);
  header["RenderingIntent"]      = info.GetRenderingIntentName((icRenderingIntent)m_Header.renderingIntent, m_Header.version >= icVersionNumberV5);
  header["PCSIlluminant"] = IccJson::array({
    icFtoD(m_Header.illuminant.X), icFtoD(m_Header.illuminant.Y), icFtoD(m_Header.illuminant.Z)
  });
  header["ProfileCreator"]     = m_Header.creator      ? icGetSigStr(buf, bufSize, m_Header.creator)      : "";

  // Profile ID (16-byte MD5) as hex
  bool nonzero = false;
  for (int i = 0; i < 16; i++) if (m_Header.profileID.ID8[i]) { nonzero = true; break; }
  if (nonzero)
    header["ProfileID"] = icJsonDumpHexData(m_Header.profileID.ID8, 16);

  if (m_Header.spectralPCS)
    header["SpectralPCS"] = icGetColorSigStr(buf, bufSize, m_Header.spectralPCS);

  if (m_Header.spectralRange.start || m_Header.spectralRange.end || m_Header.spectralRange.steps) {
    IccJson sr;
    sr["start"] = (double)icF16toF(m_Header.spectralRange.start);
    sr["end"]   = (double)icF16toF(m_Header.spectralRange.end);
    sr["steps"] = (int)m_Header.spectralRange.steps;
    header["SpectralRange"] = sr;
  }
  if (m_Header.biSpectralRange.start || m_Header.biSpectralRange.end || m_Header.biSpectralRange.steps) {
    IccJson bsr;
    bsr["start"] = (double)icF16toF(m_Header.biSpectralRange.start);
    bsr["end"]   = (double)icF16toF(m_Header.biSpectralRange.end);
    bsr["steps"] = (int)m_Header.biSpectralRange.steps;
    header["BiSpectralRange"] = bsr;
  }
  if (m_Header.mcs)
    header["MCS"] = icGetColorSigStr(buf, bufSize, m_Header.mcs);

  root["Header"] = header;

  // Tags -- stored as a JSON array to preserve tag order.
  // Each element is a single-member object: { "<tagName>": { ... } }
  // Known tags use their ICC name (e.g. "redMatrixColumnTag").
  // Private/unknown tags use "PrivateTag_N" with a "sig" member for the raw 4-char signature.
  // Tag data is in "data": { "type": "<TypeName>", ...fields... }.
  // Shared tags carry "SameAs": "<key of first occurrence>" instead of a "data" entry.
  IccJson tags = IccJson::array();
  std::set<icTagSignature> sigSet;
  std::map<CIccTag*, std::string> ptrToFirstKey;
  int privateTagCount = 0;

  for (auto i = m_Tags.begin(); i != m_Tags.end(); ++i) {
    if (sigSet.count(i->TagInfo.sig))
      continue;
    sigSet.insert(i->TagInfo.sig);

    CIccTag *pTag = FindTag(i->TagInfo.sig);
    if (!pTag)
      continue;

    // Determine the key name for this tag
    const icChar *tagSigName = CIccTagCreator::GetTagSigName(i->TagInfo.sig);
    bool isPrivateTag = !tagSigName;
    std::string key = isPrivateTag
        ? "PrivateTag_" + std::to_string(++privateTagCount)
        : std::string(tagSigName);

    IccJson tagObj;
    if (isPrivateTag)
      tagObj["sig"] = icGetSigStr(buf, bufSize, i->TagInfo.sig);

    // SameAs: tag object already serialized under another key
    auto prevIt = ptrToFirstKey.find(pTag);
    if (prevIt != ptrToFirstKey.end()) {
      tagObj["sameAs"] = prevIt->second;
      IccJson entry;
      entry[key] = tagObj;
      tags.push_back(entry);
      continue;
    }

    IIccExtensionTag *pExt = pTag->GetExtension();
    if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0)
      continue;

    CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);

    IccJson tagData;
    if (!pJsonTag->ToJson(tagData))
      continue;

    // Place the type entry in "data": { "type": "<TypeName>", ...fields... }
    const icChar *typeName = CIccTagCreator::GetTagTypeSigName(pTag->GetType());
    if (!typeName) typeName = "PrivateType";
    tagData["type"] = typeName;
    if (!strcmp(typeName, "PrivateType"))
      tagData["sig"] = icGetSigStr(buf, bufSize, pTag->GetType());

    if (pTag->m_nReserved)
      tagObj["Reserved"] = (unsigned int)pTag->m_nReserved;
    tagObj["data"] = tagData;

    IccJson entry;
    entry[key] = tagObj;
    tags.push_back(entry);
    ptrToFirstKey[pTag] = key;
  }
  root["Tags"] = tags;

  return true;
}

bool CIccProfileJson::ToJson(std::string &jsonString, int indent)
{
  IccJson root;
  IccJson profile;
  if (!ToJson(profile))
    return false;
  root["IccProfile"] = profile;
  jsonString = root.dump(indent);
  return true;
}

// ---------------------------------------------------------------------------
// Parsing: JSON object -> profile
// ---------------------------------------------------------------------------

static icUInt32Number icJsonParseBCDByte(const char *s)
{
  int v = atoi(s);
  return (icUInt32Number)(((v / 10) % 10) * 16 + (v % 10));
}

static icUInt32Number icJsonParseBCDVersionStr(const char *szVer)
{
  std::string part;
  for (; *szVer && *szVer != '.'; szVer++) part += *szVer;
  icUInt32Number hi = icJsonParseBCDByte(part.c_str()); part.clear();
  if (*szVer) szVer++;
  for (; *szVer; szVer++) part += *szVer;
  icUInt32Number lo = part.empty() ? 0 : icJsonParseBCDByte(part.c_str());
  return (hi << 8) | lo;
}

bool CIccProfileJson::ParseBasic(const IccJson &header, std::string & /*parseStr*/)
{
  CIccInfo info;

  memset(&m_Header, 0, sizeof(m_Header));
  m_Header.magic = icMagicNumber;

  std::string str;
  if (jGetString(header, "PreferredCMMType", str))
    m_Header.cmmId = (icCmmSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "ProfileVersion", str))
    m_Header.version = (m_Header.version & 0x0000ffff) | (icJsonParseBCDVersionStr(str.c_str()) << 16);

  if (jGetString(header, "ProfileSubClassVersion", str))
    m_Header.version = (m_Header.version & 0xffff0000) | icJsonParseBCDVersionStr(str.c_str());

  if (jGetString(header, "ProfileDeviceClass", str))
    m_Header.deviceClass = (icProfileClassSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "ProfileDeviceSubClass", str))
    m_Header.deviceSubClass = (icSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "DataColourSpace", str))
    m_Header.colorSpace = (icColorSpaceSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "PCS", str))
    m_Header.pcs = (icColorSpaceSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "CreationDateTime", str))
    m_Header.date = icGetDateTimeValue(str.c_str());

  if (jGetString(header, "PrimaryPlatform", str))
    m_Header.platform = (icPlatformSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "DeviceManufacturer", str))
    m_Header.manufacturer = (icSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "DeviceModel", str))
    m_Header.model = (icUInt32Number)icGetSigVal(str.c_str());

  if (jGetString(header, "ProfileCreator", str))
    m_Header.creator = (icSignature)icGetSigVal(str.c_str());

  if (header.contains("CMMFlags") && header["CMMFlags"].is_object())
    m_Header.flags = icJsonParseHeaderFlags(header["CMMFlags"]);

  if (header.contains("DeviceAttributes") && header["DeviceAttributes"].is_object())
    m_Header.attributes = icJsonParseDeviceAttr(header["DeviceAttributes"]);

  if (jGetString(header, "RenderingIntent", str))
    m_Header.renderingIntent = icGetRenderingIntentValue(str.c_str());

  double ill[3];
  if (jGetArray(header, "PCSIlluminant", ill, 3)) {
    m_Header.illuminant.X = icDtoF(ill[0]);
    m_Header.illuminant.Y = icDtoF(ill[1]);
    m_Header.illuminant.Z = icDtoF(ill[2]);
  }

  if (jGetString(header, "ProfileID", str))
    icJsonGetHexData(m_Header.profileID.ID8, str.c_str(), 16);

  if (jGetString(header, "SpectralPCS", str))
    m_Header.spectralPCS = (icColorSpaceSignature)icGetSigVal(str.c_str());

  auto parseSpectralRange = [&](const char *field, icSpectralRange &range) {
    if (header.contains(field) && header[field].is_object()) {
      const IccJson &sr = header[field];
      double start = 0, end = 0; int steps = 0;
      jGetValue(sr, "start", start);
      jGetValue(sr, "end",   end);
      jGetValue(sr, "steps", steps);
      range.start = icFtoF16((icFloat32Number)start);
      range.end   = icFtoF16((icFloat32Number)end);
      range.steps = (icUInt16Number)steps;
    }
  };
  parseSpectralRange("SpectralRange",   m_Header.spectralRange);
  parseSpectralRange("BiSpectralRange", m_Header.biSpectralRange);

  if (jGetString(header, "MCS", str))
    m_Header.mcs = (icMultiplexColorSignature)icGetSigVal(str.c_str());

  if (jGetString(header, "ProfileSubClass", str))
    m_Header.deviceSubClass = (icSignature)icGetSigVal(str.c_str());

  return true;
}

bool CIccProfileJson::ParseTag(const std::string &key, const IccJson &tagValue,
                               std::map<std::string, icTagSignature> &keyToSig,
                               std::string &parseStr)
{
  // Determine tag signature from the key name
  icTagSignature sig = CIccTagCreator::GetTagNameSig(key.c_str());
  if (sig == icSigUnknownTag) {
    // Private tag -- raw sig stored in "sig" member
    if (!tagValue.contains("sig")) {
      parseStr += "Private tag '" + key + "' missing 'sig'\n";
      return false;
    }
    sig = (icTagSignature)icGetSigVal(tagValue["sig"].get<std::string>().c_str());
  }

  // sameAs: re-attach an already-parsed tag under this signature
  if (tagValue.contains("sameAs")) {
    std::string refKey = tagValue["sameAs"].get<std::string>();
    auto it = keyToSig.find(refKey);
    if (it == keyToSig.end()) {
      parseStr += "sameAs references unknown tag '" + refKey + "' for '" + key + "'\n";
      return false;
    }
    CIccTag *pRefTag = FindTag(it->second);
    if (!pRefTag) {
      parseStr += "sameAs target not found: '" + refKey + "'\n";
      return false;
    }
    if (!AttachTag(sig, pRefTag)) {
      parseStr += "Unable to attach sameAs tag '" + key + "'\n";
      return false;
    }
    keyToSig[key] = sig;
    return true;
  }

  // "data" member of the tag holds a flat object: { "type": "<TypeName>", ...fields... }
  if (!tagValue.contains("data") || !tagValue["data"].is_object()) {
    parseStr += "Tag '" + key + "' missing 'data' object\n";
    return false;
  }
  const IccJson &tagData = tagValue["data"];

  // Read the "type" discriminator field
  std::string typeName;
  jGetString(tagData, "type", typeName);
  if (typeName.empty()) {
    parseStr += "Tag '" + key + "' has no 'type' field in 'data'\n";
    return false;
  }

  // Resolve type sig from name; for "PrivateType" use the "sig" field
  icTagTypeSignature typeSig = CIccTagCreator::GetTagTypeNameSig(typeName.c_str());
  if (typeSig == icSigUnknownType) {
    if (typeName == "PrivateType" && tagData.contains("sig"))
      typeSig = (icTagTypeSignature)icGetSigVal(tagData["sig"].get<std::string>().c_str());
    else
      typeSig = (icTagTypeSignature)icGetSigVal(typeName.c_str());
  }

  CIccTag *pTag = CIccTagCreator::CreateTag(typeSig);
  if (!pTag) {
    parseStr += "Unable to create type '" + typeName + "' for tag '" + key + "'\n";
    return false;
  }

  IIccExtensionTag *pExt = pTag->GetExtension();
  if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0) {
    delete pTag;
    parseStr += "Type '" + typeName + "' does not support JSON\n";
    return false;
  }

  CIccTagJson *pJsonTag = static_cast<CIccTagJson*>(pExt);
  if (!pJsonTag->ParseJson(tagData, parseStr)) {
    delete pTag;
    return false;
  }

  if (tagValue.contains("Reserved"))
    pTag->m_nReserved = (icUInt32Number)tagValue["Reserved"].get<unsigned int>();

  if (!AttachTag(sig, pTag)) {
    delete pTag;
    parseStr += "Unable to attach tag '" + key + "'\n";
    return false;
  }

  keyToSig[key] = sig;
  return true;
}

bool CIccProfileJson::ParseJson(const IccJson &root, std::string &parseStr)
{
  // The root may be the IccProfile object directly or wrapped in {"IccProfile": ...}
  const IccJson *pProfile = &root;
  IccJson unwrapped;
  if (root.contains("IccProfile")) {
    unwrapped  = root["IccProfile"];
    pProfile   = &unwrapped;
  }

  if (!pProfile->contains("Header") || !pProfile->contains("Tags")) {
    parseStr += "Missing Header or Tags in JSON profile\n";
    return false;
  }

  if (!ParseBasic((*pProfile)["Header"], parseStr))
    return false;

  const IccJson &tags = (*pProfile)["Tags"];
  if (!tags.is_array()) {
    parseStr += "Tags must be a JSON array\n";
    return false;
  }

  std::map<std::string, icTagSignature> keyToSig;
  for (const auto &entry : tags) {
    if (!entry.is_object() || entry.size() != 1) {
      parseStr += "Warning: tag entry must be a single-member object, skipping\n";
      continue;
    }
    auto it = entry.begin();
    const std::string &key   = it.key();
    const IccJson     &value = it.value();
    if (!ParseTag(key, value, keyToSig, parseStr))
      parseStr += "Warning: skipped tag '" + key + "'\n";
  }
  return true;
}

bool CIccProfileJson::LoadJson(const char *szFilename, std::string *parseStr)
{
  std::ifstream f(szFilename);
  if (!f.is_open()) {
    if (parseStr) *parseStr += std::string("Unable to open file: ") + szFilename + "\n";
    return false;
  }

  IccJson root;
  try {
    f >> root;
  }
  catch (const std::exception &e) {
    if (parseStr) *parseStr += std::string("JSON parse error: ") + e.what() + "\n";
    return false;
  }

  std::string reason;
  bool ok = ParseJson(root, reason);
  if (parseStr) *parseStr += reason;
  return ok;
}
