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
#include <string>

// Issue #1856. The build system is the source of truth for this bound (CMake
// cache variable ICC_JSON_MAX_FILE_MB); this fallback only keeps builds that do
// not go through Build/Cmake compiling, and must stay finite for the same
// reason the cap exists at all.
#ifndef ICC_JSON_MAX_FILE_BYTES
#define ICC_JSON_MAX_FILE_BYTES (128ULL * 1024 * 1024)
#endif

typedef std::map<icUInt32Number, icTagSignature> IccOffsetTagSigMap;

CIccProfileJson::~CIccProfileJson()
{
}

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
    char buf[16]; snprintf(buf, sizeof(buf), "%08x", (unsigned int) other);
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
    if (sscanf(vendor.c_str(), "%x", &v) == 1) flags |= v;
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

  root["Header"] = std::move(header);

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

    CIccTag *pTag = FindTag(*i);        // performance improvement to make this linear instead of N^2
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
      entry[key] = std::move(tagObj);
      tags.push_back(std::move(entry));
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
    // Each of these hand-offs used to deep-copy the whole tag payload and then
    // destroy the source. For a tag carrying a large CLUT that is the dominant
    // cost of serialization, and it happened three times per tag on the way up
    // to the root: tagData -> tagObj -> entry -> tags.
    tagObj["data"] = std::move(tagData);

    IccJson entry;
    entry[key] = std::move(tagObj);
    tags.push_back(std::move(entry));
    ptrToFirstKey[pTag] = key;
  }
  root["Tags"] = std::move(tags);

  return true;
}

bool CIccProfileJson::ToJson(std::string &jsonString, int indent)
{
  IccJson root;
  IccJson profile;
  if (!ToJson(profile))
    return false;
  // profile is the entire document; copying it here doubled peak memory for the
  // whole serialized profile immediately before the dump.
  root["IccProfile"] = std::move(profile);
  
  // dump the json data to string, but replace bad text/utf8 data with valid data instead of throwing exceptions
  jsonString = root.dump( indent,' ',true, nlohmann::detail::error_handler_t::replace );
  return true;
}

// ---------------------------------------------------------------------------
// Parsing: JSON object -> profile
// ---------------------------------------------------------------------------

// Convert one decimal version component (0-99) to the BCD byte the ICC header
// stores. Returns false for an empty component, more than two digits, or any
// character that is not a digit -- notably a sign (#1830). This used to be
// atoi(), which accepts a leading '-': "-1" produced v == -1, and
// ((v/10)%10)*16 + (v%10) then evaluated to -1, which reinterpreted as
// icUInt32Number is 0xFFFFFFFF. That value went on to wrap both shifts below,
// which UBSan reported as "left shift of 4294967295 by 8 places cannot be
// represented in type icUInt32Number". atoi() is also undefined on an
// out-of-range input, which a digit-by-digit parse of at most two digits cannot
// hit.
// bFractional marks the minor component, whose digits are positional: "5.1" is
// five-and-one-TENTH, so the digit belongs in the high nibble as 0x10, matching
// the canonical "5.10" that CIccInfo::GetVersionName writes.  Encoding it as the
// integer 1 gave 0x01 -- 5.01 -- which is #2383.  The XML twin (parseVersion in
// IccProfileXml.cpp) carries the identical SCALING rule and the same one-or-two
// digit contract, so a version the two both accept encodes to the same header
// word.  That is the guarantee -- NOT that the two accept the same inputs.  The
// XML side is deliberately the more tolerant of the pair, and measurably so:
//   - it trims surrounding whitespace, because element text is subject to
//     pretty-printing ("<ProfileVersion>\n  5.10\n</ProfileVersion>");
//   - it accepts ',' as a component separator, so "5,1" parses there and not here;
//   - it accepts third and fourth components ("5.10.12.34"), which this parser
//     hands to a single byte helper that refuses more than two characters.
// Each of those yields 0 here, which GetVersionName renders as a plain "0.00".
static bool icJsonParseBCDByte(const std::string &sPart, icUInt32Number &nOut,
                               bool bFractional)
{
  if (sPart.empty() || sPart.size() > 2)
    return false;

  unsigned int v = 0;
  for (size_t i = 0; i < sPart.size(); i++) {
    if (sPart[i] < '0' || sPart[i] > '9')
      return false;
    v = v * 10 + (unsigned int)(sPart[i] - '0');
  }

  // Keyed on the digit COUNT, not the value, so an explicit "05" still means
  // five hundredths and stays 0x05 while a bare "5" becomes 0x50.
  if (bFractional && sPart.size() == 1)
    v *= 10;

  nOut = (icUInt32Number)(((v / 10) % 10) * 16 + (v % 10));
  return true;
}

// Parse "<major>.<minor>" (as written by CIccInfo::GetVersionName, e.g. "4.30")
// into the packed BCD pair 0xMMmm. A malformed string yields 0, which is the
// version the zeroed header already carries, and which GetVersionName renders
// back as a plain "0.00" rather than a wrapped value. Because each component is
// now guaranteed to be a valid BCD byte (<= 0x99), the result is at most 0x9999
// and the caller's further "<< 16" cannot overflow either.
static icUInt32Number icJsonParseBCDVersionStr(const char *szVer)
{
  if (!szVer)
    return 0;

  std::string part;
  for (; *szVer && *szVer != '.'; szVer++) part += *szVer;

  icUInt32Number hi = 0, lo = 0;
  if (!icJsonParseBCDByte(part, hi, false))
    return 0;

  part.clear();
  if (*szVer) szVer++;
  for (; *szVer; szVer++) part += *szVer;

  // A missing minor component stays 0 ("4" == "4.0"), matching the previous
  // behaviour; a present but malformed one rejects the whole version.
  if (!part.empty() && !icJsonParseBCDByte(part, lo, true))
    return 0;

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
                               KeyToSignatureMap &keyToSig,
                               std::string &parseStr)
{
  if (key.empty()) {
    parseStr += "Tag entry has empty name\n";
    return false;
  }

  // Determine tag signature from the key name
  icTagSignature sig = CIccTagCreator::GetTagNameSig(key.c_str());
  if (sig == icSigUnknownTag) {
    // Private tag -- raw sig stored in "sig" member
    if (!tagValue.contains("sig")) {
      parseStr += "Private tag '" + key + "' missing 'sig'\n";
      return false;
    }
    if (!tagValue["sig"].is_string()) {
      parseStr += "Private tag '" + key + "' has non-string 'sig'\n";
      return false;
    }
    std::string sigStr;
    jGetString(tagValue, "sig", sigStr);
    sig = (icTagSignature)icGetSigVal(sigStr.c_str());
  }

  // sameAs: re-attach an already-parsed tag under this signature
  if (tagValue.contains("sameAs")) {
    if (!tagValue["sameAs"].is_string()) {
      parseStr += "sameAs for tag '" + key + "' must be a string\n";
      return false;
    }
    std::string refKey;
    jGetString(tagValue, "sameAs", refKey);
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
    if (typeName == "PrivateType" && tagData.contains("sig")) {
      if (!tagData["sig"].is_string()) {
        parseStr += "Tag '" + key + "' has non-string private type 'sig'\n";
        return false;
      }
      std::string typeSigStr;
      jGetString(tagData, "sig", typeSigStr);
      typeSig = (icTagTypeSignature)icGetSigVal(typeSigStr.c_str());
    }
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

  if (tagValue.contains("Reserved") && !jGetValue(tagValue, "Reserved", pTag->m_nReserved)) {
    delete pTag;
    parseStr += "Tag '" + key + "' has invalid Reserved value\n";
    return false;
  }

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
  // Wrap the whole parse body in a try/catch so nlohmann's
  // type_error / out_of_range exceptions (thrown from raw typed JSON
  // access sites in IccTagJson / IccMpeJson) can't
  // propagate out to std::terminate. LoadJson() has its own wrap,
  // but other callers (wasm wrappers, custom embedders) calling
  // ParseJson() directly would otherwise abort the host.
  try {
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

    KeyToSignatureMap keyToSig;
    for (const auto &entry : tags) {
      if (!entry.is_object() || entry.size() != 1) {
        parseStr += "Warning: tag entry must be a single-member object, skipping\n";
        continue;
      }
      auto it = entry.begin();
      const std::string &key   = it.key();
      const IccJson     &value = it.value();
      if (!ParseTag(key, value, keyToSig, parseStr))
        return false;
    }
    return true;
  }
  catch (const nlohmann::json::exception &e) {
    parseStr += std::string("JSON type/range error during parse: ") + e.what() + "\n";
    return false;
  }
  catch (const std::exception &e) {
    parseStr += std::string("Unexpected error during JSON parse: ") + e.what() + "\n";
    return false;
  }
}

bool CIccProfileJson::LoadJson(const char *szFilename, std::string *parseStr)
{
  std::ifstream f(szFilename, std::ios::binary | std::ios::ate);
  if (!f.is_open()) {
    if (parseStr) *parseStr += std::string("Unable to open file: ") + szFilename + "\n";
    return false;
  }

  // Cap raw JSON size. nlohmann's default nesting + size limits are
  // effectively "everything fits in memory", which is a DoS primitive
  // when the JSON comes from an untrusted source, so the bound stays.
  //
  // Issue #1856: the previous fixed 64 MiB was chosen against an estimate
  // that "even big iccMAX spectral profiles top out around 5 MB", which the
  // tracked corpus does not support - the profile built from
  // Testing/hybrid/CMYK-W_Overprint_Profile.xml emits 55.5 MiB at the default
  // -indent=2 and 92.9 MiB at the documented -indent=4, so iccToJson could
  // write a document iccFromJson then refused.
  // The bound is now a build-time knob (ICC_JSON_MAX_FILE_MB) defaulting to
  // 128 MiB, which clears every tracked fixture through -indent=5. It does not
  // remove the asymmetry entirely, since -indent accepts up to 20 (392.7 MiB on
  // the same profile), and a byte count is only a proxy for the memory this
  // guards: indentation inflates the file without inflating the parsed
  // document, so the cost per admitted byte varies by an order of magnitude
  // with document shape.
  static const std::streamsize kMaxJsonFileBytes =
      static_cast<std::streamsize>(ICC_JSON_MAX_FILE_BYTES);
  auto sz = f.tellg();
  if (sz < 0 || sz > kMaxJsonFileBytes) {
    if (parseStr) {
      // Report the bound that was actually compiled in, so a build that
      // configured a different limit does not print a misleading number.
      *parseStr += "JSON file exceeds " +
                   std::to_string(kMaxJsonFileBytes / (1024 * 1024)) +
                   " MiB limit\n";
    }
    return false;
  }
  f.seekg(0, std::ios::beg);

  IccJson root;
  try {
    f >> root;
  }
  catch (const std::exception &e) {
    if (parseStr) *parseStr += std::string("JSON parse error: ") + e.what() + "\n";
    return false;
  }

  std::string reason;
  bool ok = false;
  try {
    ok = ParseJson(root, reason);
  }
  catch (const std::exception &e) {
    reason += std::string("JSON semantic error: ") + e.what() + "\n";
    if (parseStr) *parseStr += reason;
    return false;
  }

  if (parseStr) *parseStr += reason;
  return ok;
}
