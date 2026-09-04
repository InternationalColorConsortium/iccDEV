/** @file
    File:       IccProfileXml.cpp

    Contains:   Implementation Icc Profile XML format conversions

    Version:    V1

    Copyright:  (c) see ICC Software License
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
#include <cstdio>
#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccUtilXml.h"
#include "IccArrayBasic.h"
#include <set>
#include <cstring> /* C strings strcpy, memcpy ... */
#include <map>

typedef  std::map<icUInt32Number, icTagSignature> IccOffsetTagSigMap;

bool CIccProfileXml::ToXml(std::string &xml)
{
  CIccInfo info;

  xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  return ToXmlWithBlanks(xml, "");
}

bool CIccProfileXml::ToXmlWithBlanks(std::string &xml, std::string blanks)
{
  CIccInfo info;
  const size_t bufSize = 256;
  char line[bufSize];
  char buf[bufSize];
  std::string fix;
  size_t n;
  bool nonzero;

  xml += blanks + "<IccProfile>\n";
  xml += blanks + "  <Header>\n";
  // Guard every zero header signature the same way as the data colour space /
  // PCS below (and the JSON serializer): icGetSig*(0) emits the literal "NULL",
  // which icGetSigVal("NULL") reparses to 0x4E554C4C, corrupting a legitimately
  // zero PreferredCMMType on round-trip. Emit an empty element for zero instead.
  snprintf(line, bufSize, "    <PreferredCMMType>%s</PreferredCMMType>\n", m_Header.cmmId ? icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.cmmId)) : "");
  xml += blanks + line;
  snprintf(line, bufSize, "    <ProfileVersion>%s</ProfileVersion>\n", info.GetVersionName(m_Header.version));
  xml += blanks + line;
  if (m_Header.version & 0x0000ffff) {
    snprintf(line, bufSize,"    <ProfileSubClassVersion>%s</ProfileSubClassVersion>\n", info.GetSubClassVersionName(m_Header.version));
    xml += blanks + line;
  }
  // Guard the zero case as above so a zero device class round-trips as 0 rather
  // than being corrupted to 0x4E554C4C via the "NULL" text encoding.
  snprintf(line, bufSize, "    <ProfileDeviceClass>%s</ProfileDeviceClass>\n", m_Header.deviceClass ? icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.deviceClass)) : "");
  xml += blanks + line;

  if (m_Header.deviceSubClass) {
    snprintf(line, bufSize, "    <ProfileDeviceSubClass>%s</ProfileDeviceSubClass>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.deviceSubClass)));
    xml += blanks + line;
  }

  // Header colour-space signatures are serialized as four-character text. A zero
  // signature (0x00000000) must round-trip back to zero, but icGetColorSigStr(0)
  // returns the literal "NULL"; the inverse icGetSigVal("NULL") then packs the
  // ASCII bytes 'N','U','L','L' into 0x4E554C4C, corrupting the value on reparse
  // (see iccFromXml turning a NoData header into "Unknown 'NULL' = 4E554C4C").
  // Emit an empty element for a zero signature instead -- icXmlGetChildSigVal()
  // returns 0 for an empty element, restoring the original value. This mirrors the
  // JSON serializer (IccProfileJson.cpp), which already guards these fields the
  // same way. Note: per ICC.1 (v4.4.0.0) 7.2.6 / Table 19 a zero data colour
  // space is itself invalid for a v2/v4 profile; faithfully preserving the bytes
  // is a serialization concern, leaving the validator to flag the malformance.
  snprintf(line, bufSize, "    <DataColourSpace>%s</DataColourSpace>\n", m_Header.colorSpace ? icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.colorSpace)) : "");
  xml += blanks + line;
  snprintf(line, bufSize, "    <PCS>%s</PCS>\n",  m_Header.pcs ? icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.pcs)) : "");
  xml += blanks + line;

  snprintf(line, bufSize, "    <CreationDateTime>%d-%02d-%02dT%02d:%02d:%02d</CreationDateTime>\n",
										m_Header.date.year,
										m_Header.date.month,
										m_Header.date.day,
										m_Header.date.hours,
										m_Header.date.minutes,
										m_Header.date.seconds);
  xml += blanks + line;

 // if (m_Header.magic != 0){
 //	  sprintf(line, "    <Signature>%s</Signature>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.magic)));
 //	  xml += line;
 // }

  if (m_Header.platform != icSigUnknownPlatform){
	snprintf(line, bufSize, "    <PrimaryPlatform>%s</PrimaryPlatform>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.platform)));
	xml += blanks + line;
  }
 
  xml+= blanks + "    ";
  xml+= icGetHeaderFlagsName(m_Header.flags, m_Header.mcs!=0);

  if (m_Header.manufacturer != 0){
	  snprintf(line, bufSize, "    <DeviceManufacturer>%s</DeviceManufacturer>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.manufacturer)));
	  xml += blanks + line;
  }
  
  if (m_Header.model != 0){
	snprintf(line, bufSize, "    <DeviceModel>%s</DeviceModel>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.model)));
	xml += blanks + line;
  }

  xml+= "    ";
  xml += blanks + icGetDeviceAttrName(m_Header.attributes);

  snprintf(line, bufSize, "    <RenderingIntent>%s</RenderingIntent>\n", info.GetRenderingIntentName((icRenderingIntent)m_Header.renderingIntent, m_Header.version>=icVersionNumberV5));
  xml += blanks + line;
  snprintf(line, bufSize, "    <PCSIlluminant>\n%s      <XYZNumber X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n%s    </PCSIlluminant>\n", blanks.c_str(),
                                                             (float)icFtoD(m_Header.illuminant.X),
                                                             (float)icFtoD(m_Header.illuminant.Y),
                                                             (float)icFtoD(m_Header.illuminant.Z),
                                                             blanks.c_str());

  xml += blanks + line;
  
  // Guard the zero case as above so a zero creator round-trips as 0 rather than
  // being corrupted to 0x4E554C4C via the "NULL" text encoding.
  snprintf(line, bufSize, "    <ProfileCreator>%s</ProfileCreator>\n", m_Header.creator ? icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.creator)) : "");
  xml += blanks + line;

  if (m_Header.profileID.ID32[0] || m_Header.profileID.ID32[1] || 
      m_Header.profileID.ID32[2] || m_Header.profileID.ID32[3]) {
    xml += blanks;
    for (n=0; n<16; n++) {
      snprintf(buf + n*2, bufSize-n*2, "%02X", m_Header.profileID.ID8[n]);
    }
    buf[n*2]='\0';
    xml += "    <ProfileID>";
    xml += buf;
    xml += "</ProfileID>\n";
  }
  nonzero = false;

  if (m_Header.spectralPCS) {
    snprintf(line, bufSize, "    <SpectralPCS>%s</SpectralPCS>\n",  icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.spectralPCS)));
    xml += blanks + line;

    if (m_Header.spectralRange.steps) {
      xml += blanks + "    <SpectralRange>\n";
      snprintf(line, bufSize, "     <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n",
              icF16toF(m_Header.spectralRange.start), icF16toF(m_Header.spectralRange.end), m_Header.spectralRange.steps);
      xml += blanks + line;
      xml += blanks + "    </SpectralRange>\n";
    }
    if (m_Header.biSpectralRange.steps) {
      xml += blanks + "    <BiSpectralRange>\n";
      // The format string used to end "/>\n)", so a stray ')' was appended after
      // the newline and landed immediately before the closing tag, i.e. every
      // profile carrying a biSpectralRange was written out with a junk text node
      // inside the element:
      //
      //      <Wavelengths start="300.00000000" end="700.00000000" steps="41"/>
      //  )    </BiSpectralRange>
      //
      // It round-tripped only because the reader locates <Wavelengths> with
      // icXmlFindNode(), which walks past text nodes, so nothing ever looked at
      // it.  The SpectralRange writer directly above is the same statement
      // without the typo and is what this now matches.
      snprintf(line, bufSize, "     <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n",
              icF16toF(m_Header.biSpectralRange.start), icF16toF(m_Header.biSpectralRange.end), m_Header.biSpectralRange.steps);
      xml += blanks + line;
      xml += blanks + "    </BiSpectralRange>\n";
    }
  }

  if (m_Header.mcs) {
    snprintf(line, bufSize, "    <MCS>%s</MCS>\n",  icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.mcs)));
    xml += blanks + line;
  }

  for (n=0; n<sizeof(m_Header.reserved); n++) {
    if (m_Header.reserved[n])
      nonzero = true;
    snprintf(buf + n*2, bufSize-n*2, "%02X", m_Header.reserved[n]);
  }
  buf[n*2]='\0';
  if (nonzero) {
    xml += blanks + "    <Reserved>";
    xml += buf;
	  xml += "</Reserved>\n";
  }
  xml += blanks + "  </Header>\n";
  
  xml += blanks + "  <Tags>\n";
  TagEntryList::iterator i, j;
  std::set<icTagSignature> sigSet;
  CIccInfo Fmt;
  IccOffsetTagSigMap offsetTags;

  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if (sigSet.find(i->TagInfo.sig)==sigSet.end()) {
      CIccTag *pTag = FindTag(i->TagInfo.sig);

      if (pTag) {
        CIccTagXml *pTagXml = (CIccTagXml*)(pTag->GetExtension());
        if (pTagXml) {
          IccOffsetTagSigMap::iterator prevTag = offsetTags.find(i->TagInfo.offset);
          const icChar *tagName = Fmt.GetTagSigName(i->TagInfo.sig);
          if (prevTag == offsetTags.end()) {
            const icChar* tagSig = icGetTagSigTypeName(pTag->GetType());

            // Remember where this tag's markup begins.  The opening elements are
            // appended below *before* ToXml() is called, so if the tag turns out
            // not to be serializable the buffer has to be rewound to here -- see
            // the skip path after the ToXml() call.
            const size_t nTagStart = xml.size();

            if (tagName && strncmp(tagName, "Unknown ", 8)) {
              snprintf(line, bufSize, "    <%s> ", icFixXml(fix, tagName));
            }
            else {
              snprintf(line, bufSize, "    <PrivateTag TagSignature=\"%s\"> ", icFixXml(fix, icGetSigStr(buf, bufSize, i->TagInfo.sig)));
              tagName = "PrivateTag";
            }
            xml += blanks + line;
            // PrivateType - a type that does not belong to the list in the icc specs - custom for vendor.
            if (pTag->m_nReserved) {
              if (!strcmp("PrivateType", tagSig))
                snprintf(line, bufSize, "<PrivateType type=\"%s\" reserved=\"%08x\">\n", icFixXml(fix, icGetSigStr(buf, bufSize, pTag->GetType())), (unsigned int) pTag->m_nReserved);
              else
                snprintf(line, bufSize, "<%s reserved=\"%08x\">\n", tagSig, (unsigned int) pTag->m_nReserved); //parent node is the tag type
            }
            else {
              if (!strcmp("PrivateType", tagSig))
                snprintf(line, bufSize, "<PrivateType type=\"%s\">\n", icFixXml(fix, icGetSigStr(buf, bufSize, pTag->GetType())));
              else
                snprintf(line, bufSize, "<%s>\n", tagSig); //parent node is the tag type
            }

            xml += line;
            j = i;
#if 0
            // print out the tag signature (there is at least one)
            sprintf(line, "      <TagSignature>%s</TagSignature>\n", icFixXml(fix, icGetSigStr(buf, bufSize, i->TagInfo.sig)));
            xml += line;

            sigSet.insert(i->TagInfo.sig);

            // print out the rest of the tag signatures
            for (j++; j != m_Tags.end(); j++) {
              if (j->pTag == i->pTag || j->TagInfo.offset == i->TagInfo.offset) {
                sprintf(line, "      <TagSignature>%s</TagSignature>\n", icFixXml(fix, icGetSigStr(buf, bufSize, j->TagInfo.sig)));
                xml += line;
                sigSet.insert(j->TagInfo.sig);
              }
            }
#endif
            //convert the rest of the tag to xml
            if (!pTagXml->ToXml(xml, blanks + "      ")) {
              // Says "skipped" because this is no longer fatal: the conversion
              // continues and exits successfully, so an unqualified "Unable to
              // output ..." on stdout would now describe a run that in fact
              // succeeded, and any caller treating that text as a failure signal
              // would misread it (the #1394 regression test made exactly that
              // mistake).  The leading text is kept verbatim so existing searches
              // for this message still match.
              printf("Unable to output tag with type %s - tag skipped\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));

              // Skip just this tag rather than abandoning the whole profile.
              // Returning false here meant a single unserializable tag -- which
              // on a corrupt or fuzzed profile is common -- discarded the entire
              // document and iccToXml wrote no file at all, while iccToJson
              // dropped the one bad tag and still produced output (#1779).  The
              // JSON writer's top-level loop does exactly this: "if
              // (!pJsonTag->ToJson(tagData)) continue;" (IccProfileJson.cpp).
              //
              // ToXml() may have appended a partial fragment before failing, and
              // the opening elements were emitted above, so rewind to nTagStart
              // to keep the document well-formed, then leave a comment recording
              // what was dropped so the loss is visible rather than silent.
              // Top-level tags are keyed by signature, not by position, so
              // omitting one is representable -- the same reason the JSON writer
              // can drop a key here.
              std::string sigFix, typeFix;
              xml.resize(nTagStart);
              snprintf(line, bufSize, "    <!-- tag %s (type %s): unable to serialize, skipped -->\n\n",
                       icFixXmlComment(sigFix, icGetSigStr(buf, bufSize, i->TagInfo.sig)),
                       icFixXmlComment(typeFix, tagSig ? tagSig : ""));
              xml += blanks + line;

              // Deliberately not recorded in offsetTags: a later tag sharing this
              // offset must serialize itself in full rather than emit a SameAs
              // reference pointing at a tag that is no longer in the document.
              continue;
            }
            snprintf(line, bufSize, "    </%s> </%s>\n\n", tagSig, tagName);
            xml += blanks + line;
            offsetTags[i->TagInfo.offset] = i->TagInfo.sig;
          }
          else {
            const icChar *prevTagName = Fmt.GetTagSigName(prevTag->second);
            char nameBuf[200], fix2[200];
            if (!prevTagName || !strncmp(prevTagName, "Unknown ", 8)) {
              strcpy(nameBuf, "PrivateTag");
              prevTagName = nameBuf;
            }

            if (tagName && strncmp(tagName, "Unknown ", 8))
              snprintf(line, bufSize, "    <%s SameAs=\"%s\"", icFixXml(fix, tagName), icFixXml(fix2, prevTagName)); //parent node is the tag type
            else
              snprintf(line, bufSize, "    <PrivateTag TagSignature=\"%s\" SameAs=\"%s\"", icFixXml(fix2, icGetSigStr(buf, bufSize, i->TagInfo.sig)), icFixXml(fix, prevTagName));
            
            xml += blanks + line;
            if (prevTagName == nameBuf) {
              snprintf(line, bufSize, " SameAsSignature=\"%s\"", icFixXml(fix2, icGetSigStr(buf, bufSize, prevTag->second)));
              xml += blanks + line;
            }

            xml += "/>\n\n";
          }
        }
        else {
          printf("Non XML tag in list with tag %s!\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));
          return false;
        }
      }
      else {
        printf("Unable to find tag with tag %s!\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));
        return false;
      }
    }
  }
  xml += blanks + "  </Tags>\n";
  xml += blanks + "</IccProfile>\n";

  return true;
}

// Convert one decimal version component (0-99) into the BCD byte the ICC header
// stores.  Returns false for an empty component, a sign, a non-digit, or a value
// too large to fit one BCD byte.
//
// This used to be atoi(), which accepts a leading '-'.  The #1845 PoC carries
// <ProfileVersion>-12.</ProfileVersion>: atoi gave -12, ((val/10)%10)*16 +
// (val%10) then evaluated to -18, and storing that in an unsigned char produced
// 238 (0xEE) -- UBSan reports it as "implicit conversion from type 'int' of
// value -18 ... changed the value to 238".  0xE is not a valid BCD digit, so
// CIccInfo::GetVersionName can only render the result back as "Invalid BCD
// version", yet iccFromXml still wrote the profile out carrying that byte.
// Values above 99 were silently truncated by the same expression ("100" became
// 0x00), and atoi() is undefined outright on an input too large for an int.
//
// icXmlParseU32 is the existing strict attribute parser (IccUtilXml.h): it
// rejects the sign, non-digits and trailing garbage, and its max_value argument
// performs the range check.  This is the XML twin of the JSON defect fixed for
// #1830, whose icJsonParseBCDByte in IccProfileJson.cpp now carries the same
// contract.
static bool parseVersion(const std::string &sPart, unsigned long &rv)
{
  icUInt32Number v = 0;

  // Trim first: a component carries the element's literal text, so a
  // pretty-printed or hand-edited document indents it ("<ProfileVersion>\n
  // 5.10\n</ProfileVersion>") or leaves a trailing space. icXmlParseU32 requires
  // the whole string to be consumed, and strtoull skips leading blanks but not
  // trailing ones, so "5.10 " was rejected outright and the version reached the
  // header as 0 -- silently, since ParseBasic still returns true. That has been
  // true of <ProfileVersion> since the #1845 repair put this helper on that path;
  // routing <ProfileSubClassVersion> through the same helper would otherwise have
  // spread it to a field whose atoi() had tolerated the whitespace.
  std::string sTrimmed = sPart;
  const char *szBlank = " \t\r\n\f\v";
  std::string::size_type nFirst = sTrimmed.find_first_not_of(szBlank);
  if (nFirst == std::string::npos)
    return false;
  sTrimmed = sTrimmed.substr(nFirst, sTrimmed.find_last_not_of(szBlank) - nFirst + 1);

  if (!icXmlParseU32(sTrimmed.c_str(), v, 99))
    return false;

  rv = ((v / 10) % 10) * 16 + (v % 10);
  return true;
}

/**
*****************************************************************************
* Name: CIccProfileXml::ParseBasic
* 
* Purpose: Parse ICC header.
* 
* Args: 
*  pNode - pointer to xmlNode object to read data with
* 
* Return: 
*  true - valid ICC header, false - failure
******************************************************************************
*/
bool CIccProfileXml::ParseBasic(xmlNode *pNode, std::string &parseStr)
{  	  
  std::string temp;
  memset(&m_Header, 0, sizeof(m_Header));
  
  if (!pNode)
    return false;

  for (pNode=pNode->children; pNode; pNode=pNode->next) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp((const char*)pNode->name, "ProfileVersion")) {
        if (!pNode->children || !pNode->children->content) {
          parseStr += "Cannot parse ProfileVersion, no value specified\n";
          continue;
        }
      const char *szVer = (const char*)pNode->children->content;
      std::string ver;
      unsigned long verMajor=0, verMinor=0, verClassMajor=0, verClassMinor=0;
      // A malformed component now rejects the whole version rather than letting a
      // wrapped or partly-parsed value reach the header, matching the policy
      // icJsonParseBCDVersionStr applies on the JSON side (#1830).
      bool bVerOk = false;

      for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
        ver += *szVer;
      }
      bVerOk = parseVersion(ver, verMajor);
      ver.clear();
      if (*szVer)
        szVer++;

      if (bVerOk && *szVer) {
        for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
          ver += *szVer;
        }
        bVerOk = parseVersion(ver, verMinor);
        ver.clear();
        if (*szVer)
          szVer++;

        if (bVerOk && *szVer) {
          // Accumulate rather than assign.  889db62b repaired the major and
          // minor loops above but left these two, so every iteration overwrote
          // the previous digit and only the last one reached parseVersion:
          // "5.10.12.34" landed as 05 10 02 04.
          for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
            ver += *szVer;
          }
          bVerOk = parseVersion(ver, verClassMajor);
          ver.clear();
          if (*szVer)
            szVer++;

          if (bVerOk && *szVer) {
            for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
              ver += *szVer;
            }
            bVerOk = parseVersion(ver, verClassMinor);
            ver.clear();

            // The header holds exactly four components, so anything still
            // unconsumed is a fifth. Rejecting rather than silently truncating
            // is what icJsonParseBCDVersionStr does -- it hands the whole
            // remainder to a helper that refuses more than two digits.
            if (bVerOk && *szVer)
              bVerOk = false;
          }
        }
      }

      if (!bVerOk) {
        // Leave the memset-zeroed header version in place.  0 is what
        // GetVersionName renders as a plain "0.00", where the old wrapped byte
        // rendered as "Invalid BCD version" -- and the profile was still saved.
        parseStr += "Cannot parse ProfileVersion '";
        parseStr += (const char*)pNode->children->content;
        parseStr += "'\n";
      }
      else {
        m_Header.version = icUInt32Number( (verMajor << 24) | (verMinor << 16) | (verClassMajor << 8) | verClassMinor );
      }
    }
    else if (!icXmlStrCmp((const char*)pNode->name, "ProfileSubClassVersion")) {
      if (!pNode->children || !pNode->children->content) {
        parseStr += "Cannot parse ProfileSubClassVersion, no value specified\n";
        continue;
      }
      const char *szVer = (const char*)pNode->children->content;
      std::string ver;
      unsigned long verClassMajor = 0, verClassMinor = 0;
      // Three defects shared this block and they only cancel out together: the
      // loops assigned instead of appending, so only the last digit survived;
      // the major loop never stepped over the separator, so the minor loop ran
      // zero times and atoi("") supplied 0; and atoi() stored a raw decimal
      // where the header holds BCD.  Repairing only the first would have put
      // "12" in the byte as 0x0C, which GetSubClassVersionName then rejects as
      // "Invalid BCD subclass version".  parseVersion is the strict BCD helper
      // ProfileVersion already uses, and matches icJsonParseBCDVersionStr on
      // the JSON side.
      bool bVerOk = false;

      for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
        ver += *szVer;
      }
      bVerOk = parseVersion(ver, verClassMajor);
      ver.clear();
      if (*szVer)
        szVer++;

      // A missing minor component stays 0, so "12" means "12.00"; a present but
      // malformed one rejects the whole subclass version.
      if (bVerOk && *szVer) {
        for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
          ver += *szVer;
        }
        bVerOk = parseVersion(ver, verClassMinor);
        ver.clear();

        // As above: the sub-class version is two components, so a third is a
        // malformation rather than something to truncate away.
        if (bVerOk && *szVer)
          bVerOk = false;
      }

      if (!bVerOk) {
        // Malformed input already reached the header as zero via atoi(), so the
        // stored value is unchanged; what is new is the recorded reason. Note
        // iccFromXml prints parseStr only when LoadXml fails, and ParseBasic
        // returns true regardless, so this string is not yet surfaced on the
        // command line -- that suppression is #2387 finding 4 / #2384, which
        // needs a ruling on the tool's exit contract before it can move.
        parseStr += "Cannot parse ProfileSubClassVersion '";
        parseStr += (const char*)pNode->children->content;
        parseStr += "'\n";
        verClassMajor = verClassMinor = 0;
      }

      m_Header.version = (m_Header.version & 0xffff0000) | (((verClassMajor << 8) | verClassMinor) & 0x0000ffff);
    }
    else if (!icXmlStrCmp(pNode->name, "PreferredCMMType")) {
			m_Header.cmmId = icXmlGetChildSigVal(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "ProfileDeviceClass")) {
			m_Header.deviceClass = (icProfileClassSignature)icXmlGetChildSigVal(pNode);
		}
    else if (!icXmlStrCmp(pNode->name, "ProfileDeviceSubClass")) {
      m_Header.deviceSubClass = (icSignature)icXmlGetChildSigVal(pNode);
    }
    else if (!icXmlStrCmp(pNode->name, "DataColourSpace")) {
			m_Header.colorSpace = (icColorSpaceSignature)icXmlGetChildSigVal(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "PCS")) {
		  m_Header.pcs = (icColorSpaceSignature)icXmlGetChildSigVal(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "CreationDateTime")) {
      if (pNode && pNode->children && pNode->children->content) {
        const char *datetime = (const char*)pNode->children->content;
			  m_Header.date = icGetDateTimeValue(datetime);
      }
      else
        memset(&m_Header.date, 0, sizeof(m_Header.date));
		}
		else if (!icXmlStrCmp(pNode->name, "PrimaryPlatform")) {
			m_Header.platform = (icPlatformSignature)icXmlGetChildSigVal(pNode);
		}		
		else if (!icXmlStrCmp(pNode->name, "ProfileFlags")) {
			m_Header.flags = 0;			
      
      xmlAttr *attr = icXmlFindAttr(pNode, "EmbeddedInFile");
      if (attr && !strcmp(icXmlAttrValue(attr), "true")) {
		m_Header.flags |= icEmbeddedProfileTrue;
      }

        attr = icXmlFindAttr(pNode, "UseWithEmbeddedDataOnly");
      if (attr && !strcmp(icXmlAttrValue(attr), "true")) {
        m_Header.flags |= icUseWithEmbeddedDataOnly;
      }

      attr = icXmlFindAttr(pNode, "ExtendedRangePCS");
      if (attr && !strcmp(icXmlAttrValue(attr), "true")) {
        m_Header.flags |= icExtendedRangePCS;
      }

      attr = icXmlFindAttr(pNode, "MCSNeedsSubset");
      if (attr && !strcmp(icXmlAttrValue(attr), "true")) {
        m_Header.flags |= icMCSNeedsSubsetTrue;
      }

      attr = icXmlFindAttr(pNode, "VendorFlags");
      if (attr) {
        icUInt32Number vendor = 0;
        sscanf(icXmlAttrValue(attr), "%x", &vendor);
        m_Header.flags |= vendor;
      }
		}
		else if (!icXmlStrCmp(pNode->name, "DeviceManufacturer")) {
		  m_Header.manufacturer = icXmlGetChildSigVal(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "DeviceModel")) {
		  m_Header.model = icXmlGetChildSigVal(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "DeviceAttributes")) {
			m_Header.attributes = icGetDeviceAttrValue(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "RenderingIntent")) {
          if (!pNode->children || !pNode->children->content) {
            parseStr += "Cannot parse RenderingIntent, no value specified\n";
            continue;
          }
          if (!strcmp((const char*)pNode->children->content, "Perceptual"))
            m_Header.renderingIntent = icPerceptual;
          else if (!strcmp((const char*)pNode->children->content, "Relative Colorimetric") || !strcmp((const char*)pNode->children->content, "Relative"))
            m_Header.renderingIntent = icRelativeColorimetric;
          else if (!strcmp((const char*)pNode->children->content, "Saturation"))
            m_Header.renderingIntent = icSaturation;
          else if (!strcmp((const char*)pNode->children->content, "Absolute Colorimetric") || !strcmp((const char*)pNode->children->content, "Absolute"))
            m_Header.renderingIntent = icAbsoluteColorimetric;
		}
		else if (!icXmlStrCmp(pNode->name, "PCSIlluminant")) { 
			xmlNode *xyzNode = icXmlFindNode(pNode->children, "XYZNumber");

			xmlAttr *x = icXmlFindAttr(xyzNode, "X");
			xmlAttr *y = icXmlFindAttr(xyzNode, "Y");
			xmlAttr *z = icXmlFindAttr(xyzNode, "Z");

			if (x && y && z) {
			   m_Header.illuminant.X = icDtoF((icFloatNumber)atof(icXmlAttrValue(x)));
			   m_Header.illuminant.Y = icDtoF((icFloatNumber)atof(icXmlAttrValue(y)));
			   m_Header.illuminant.Z = icDtoF((icFloatNumber)atof(icXmlAttrValue(z)));
			}
		}
		else if (!icXmlStrCmp(pNode->name, "ProfileCreator")) {
		  m_Header.creator = icXmlGetChildSigVal(pNode);
		}
		else if (!icXmlStrCmp(pNode->name, "ProfileID")) {
      if (pNode->children && pNode->children->content) 
		    icXmlGetHexData(&m_Header.profileID.ID8, (const char*)pNode->children->content, sizeof(m_Header.profileID.ID8));
      else
        memset(&m_Header.profileID.ID8, 0, sizeof(m_Header.profileID.ID8));
		}
    else if (!icXmlStrCmp(pNode->name, "SpectralPCS")) {
      m_Header.spectralPCS = (icColorSpaceSignature)icXmlGetChildSigVal(pNode);
    }
    else if (!icXmlStrCmp(pNode->name, "SpectralRange")) {
      xmlNode *xyzNode = icXmlFindNode(pNode->children, "Wavelengths");

      xmlAttr *start = icXmlFindAttr(xyzNode, "start");
      xmlAttr *end = icXmlFindAttr(xyzNode, "end");
      xmlAttr *steps = icXmlFindAttr(xyzNode, "steps");

      if (start && end && steps) {
        m_Header.spectralRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(start)));
        m_Header.spectralRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(end)));

        // steps is the sample count for the whole spectral PCS, so narrowing it
        // modulo 65536 substitutes a different spectrum rather than failing:
        // steps="65567" was stored -- and written back out on save -- as 31,
        // while iccFromXml still reported "Profile parsed and saved correctly".
        // The explicit (icUInt16Number) cast made that silent to UBSan as well.
        // This is the header-level twin of what #1925 fixed in the tag readers
        // and #1908 in the MPE elements, and it feeds the very disagreement
        // #1932 had to reject downstream in CIccPcsXform::Connect -- a
        // spectralPCS channel count that does not match spectralRange.steps.
        if (!icXmlParseU16(icXmlAttrValue(steps), m_Header.spectralRange.steps)) {
          parseStr += "Invalid SpectralRange Wavelengths steps\n";
          return false;
        }
      }
    }
    else if (!icXmlStrCmp(pNode->name, "BiSpectralRange")) {
      xmlNode *xyzNode = icXmlFindNode(pNode->children, "Wavelengths");

      xmlAttr *start = icXmlFindAttr(xyzNode, "start");
      xmlAttr *end = icXmlFindAttr(xyzNode, "end");
      xmlAttr *steps = icXmlFindAttr(xyzNode, "steps");

      if (start && end && steps) {
        m_Header.biSpectralRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(start)));
        m_Header.biSpectralRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(end)));

        // Same defect and same reasoning as the spectralRange steps above; this
        // count sizes the bi-spectral (fluorescence) axis, which #1677 showed
        // must agree with the PCS steps or the mapping is built from a range the
        // profile does not actually carry.
        if (!icXmlParseU16(icXmlAttrValue(steps), m_Header.biSpectralRange.steps)) {
          parseStr += "Invalid BiSpectralRange Wavelengths steps\n";
          return false;
        }
      }
    }
    else if (!icXmlStrCmp(pNode->name, "MCS")) {
      m_Header.mcs = (icMultiplexColorSignature)icXmlGetChildSigVal(pNode);
    }
    else if (!icXmlStrCmp(pNode->name, "ProfileDeviceSubClass")) {
      m_Header.deviceSubClass = (icProfileClassSignature)icXmlGetChildSigVal(pNode);
    }
		else if (!icXmlStrCmp(pNode->name, "Reserved")) {
      if (pNode->children && pNode->children->content)
        icXmlGetHexData(&m_Header.reserved, (const char*)pNode->children->content, sizeof(m_Header.reserved));
      else
        memset(&m_Header.reserved, 0, sizeof(m_Header.reserved));
		}
		else {
		  parseStr += "Unknown Profile Header attribute: ";
		  parseStr += (const char*)pNode->name;
		  parseStr += "=\"";
      if (pNode->children && pNode->children->content)
		    parseStr += (const char*)pNode->children->content;
		  parseStr += "\"\n";
		}
	  }
  }

  m_Header.magic = icMagicNumber;  

  return true;
}


/**
******************************************************************************
* Name: CIccProfileXml::ParseTag
* 
* Purpose: This will load from the indicated IO object and associate a tag
*  object to a tag directory entry.  Nothing happens if tag directory entry
*  is associated with a tag object.
* 
* Args: 
*  pNode - pointer to xmlNode object to parse from
* 
* Return: 
*  true - tag from node successfully parsed,
*  false - failure
*******************************************************************************
*/
bool CIccProfileXml::ParseTag(xmlNode *pNode, std::string &parseStr)
{
  xmlAttr *attr = NULL;
  
  if (!pNode) {
	  parseStr += "Invalid NULL Tag Node\n";
	  return false;
  }
  
  if (pNode->type != XML_ELEMENT_NODE) {// || icXmlStrCmp(pNode->name, "Tag")) {
	  parseStr += "Invalid Tag Node: ";
	  parseStr += (const char *)pNode->name;
	  parseStr += "\n";
	  return false;
  }

  CIccTag *pTag = NULL;
 
  std::string nodeName = (icChar*)pNode->name; 
  icTagSignature sigTag = icGetTagNameSig(nodeName.c_str());

  if (sigTag != icSigUnknownTag || nodeName == "PrivateTag") { //Parsing of XML tags by name
    if (nodeName == "PrivateTag") {
      const char *tagSig = icXmlAttrValue(pNode, "TagSignature", "");
      if (tagSig[0]) {
        sigTag = (icTagSignature)icGetSigVal(tagSig);
      }
      else {
        parseStr += "Invalid TagSignature for PrivateTag\n";
        return false;
      }
    }

    const char *sameAs = icXmlAttrValue(pNode, "SameAs", "");

    if (sameAs[0]) {
      icTagSignature sigParentTag = icGetTagNameSig(sameAs);
      if (!strcmp(sameAs, "PrivateTag") || sigParentTag == icSigUnknownTag) {
        const char *sameAsSig = icXmlAttrValue(pNode, "SameAsSignature", "");
        if (sameAsSig[0]) {
          sigParentTag = (icTagSignature)icGetSigVal(sameAsSig);
        }
        else {
          parseStr += "Invalid SameAsSignature for PrivateTag\n";
          return false;
        }
      }
      pTag = this->FindTag(sigParentTag);
      if (pTag) {
        AttachTag(sigTag, pTag);
      }
      else {
        parseStr += "SameAs tag ";
        parseStr += sameAs;
        parseStr += " for ";
        parseStr += nodeName + " does not exist\n";
        return false;
      }

      return true;
    }
    else { //Parse the type node as the first child
      xmlNode *pTypeNode;
      for (pTypeNode = pNode->children; pTypeNode; pTypeNode = pTypeNode->next) {
        if (pTypeNode->type == XML_ELEMENT_NODE) {
          break;
        }
      }

      if (!pTypeNode) {
        parseStr += "No tag type node defined for ";
        parseStr += nodeName;
        parseStr += "\n";
        return false;
      }

      // get the tag type signature
      icTagTypeSignature sigType = icGetTypeNameTagSig((const icChar*)pTypeNode->name);

      if (sigType == icSigUnknownType) {
        attr = icXmlFindAttr(pTypeNode, "type");
        const char *typeSig = icXmlAttrValue(attr);
        if (!typeSig[0]) {
          parseStr += "Invalid private tag type attribute for ";
          parseStr += nodeName;
          parseStr += "\n";
          return false;
        }
        sigType = (icTagTypeSignature)icGetSigVal(typeSig);
      }

      CIccInfo info;

      // create a tag based on the signature
      pTag = CIccTag::Create(sigType);

      IIccExtensionTag *pExt = NULL;
      const size_t strSize = 100;
      char str[strSize];
    
      if (pTag)
        pExt = pTag->GetExtension();

      if (pExt && !strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
        CIccTagXml* pXmlTag = (CIccTagXml*)pExt;

        if (pXmlTag && pXmlTag->ParseXml(pTypeNode->children, parseStr)) {
          if ((attr = icXmlFindAttr(pTypeNode, "reserved"))) {
            sscanf(icXmlAttrValue(attr), "%x", &pTag->m_nReserved);
          }
          //AttachTag refuses (and does not take ownership) when a tag with this
          //signature already exists; free pTag to avoid a leak on duplicates.
          if (!AttachTag(sigTag, pTag)) {
            parseStr += "Unable to attach duplicate tag \"";
            parseStr += nodeName;
            snprintf(str, strSize, "\" on line %d\n", pTypeNode->line);
            parseStr += str;
            delete pTag;
            return false;
          }
        }
        else {
          parseStr += "Unable to Parse \"";
          parseStr += (const char*)pTypeNode->name;
          parseStr += "\" (";
          parseStr += nodeName;
          snprintf(str, strSize, ") Tag on line %d\n", pTypeNode->line);
          parseStr += str;
          delete pTag;
          return false;
        }
      }
      else {
        parseStr += "Invalid tag extension for \"";
        parseStr += (const char*)pTypeNode->name;
        parseStr += "\" (";
        snprintf(str, strSize, ") Tag on line %d\n", pTypeNode->line);
        parseStr += str;
        delete pTag;
        return false;
      }
    }
  }
  else {  //Legacy parsing of XML tags by type
    sigTag = (icTagSignature)0;
    // get the tag type signature
    icTagTypeSignature sigType = icGetTypeNameTagSig(nodeName.c_str());

    if (sigType == icSigUnknownType) {
      attr = icXmlFindAttr(pNode, "type");
      const char *typeSig = icXmlAttrValue(attr);
      if (!typeSig[0]) {
        parseStr += "Invalid private tag type attribute for ";
        parseStr += nodeName;
        parseStr += "\n";
        return false;
      }
      sigType = (icTagTypeSignature)icGetSigVal(typeSig);
    }

    CIccInfo info;

    // create a tag based on the signature
    pTag = CIccTag::Create(sigType);

    IIccExtensionTag *pExt = NULL;
    const size_t strSize = 100;
    char str[strSize];
    
    if (pTag)
      pExt = pTag->GetExtension();

    if (pExt && !strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
      CIccTagXml* pXmlTag = (CIccTagXml*)pExt;

      if (pXmlTag && pXmlTag->ParseXml(pNode->children, parseStr)) {
        if ((attr = icXmlFindAttr(pNode, "reserved"))) {
          sscanf(icXmlAttrValue(attr), "%u", &pTag->m_nReserved);
        }

        bool bAttached = false;
        for (xmlNode *tagSigNode = pNode->children; tagSigNode; tagSigNode = tagSigNode->next) {
          if (tagSigNode->type == XML_ELEMENT_NODE && !icXmlStrCmp(tagSigNode->name, "TagSignature")) {
            if ((const icChar*)tagSigNode->children != NULL) {
              sigTag = (icTagSignature)icGetSigVal((const icChar*)tagSigNode->children->content);
              //Only flag ownership transfer when AttachTag actually accepts pTag.
              //It refuses duplicate signatures without taking ownership, so a
              //blanket bAttached=true here would leak pTag on a duplicate tag.
              if (AttachTag(sigTag, pTag))
                bAttached = true;
            }
          }
        }

        //No TagSignature node claimed ownership of pTag; free it to avoid a leak
        if (!bAttached) {
          delete pTag;
          return false;
        }
      }
      else {
        parseStr += "Unable to Parse \"";
        parseStr += info.GetTagTypeSigName(sigType);
        parseStr += "\" (";
        parseStr += nodeName;
        snprintf(str, strSize, ") Tag on line %d\n", pNode->line);
        parseStr += str;
        delete pTag;
        return false;
      }
    }
    else {
      parseStr += "Invalid tag extension for \"";
      parseStr += info.GetTagTypeSigName(sigType);
      parseStr += "\" (";
      parseStr += nodeName;
      snprintf(str, strSize, ") Tag on line %d\n", pNode->line);
      parseStr += str;
      delete pTag;
      return false;
    }
  }

  switch(sigTag) {
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
  case icSigAToB3Tag:
    if (pTag && pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.colorSpace, m_Header.pcs);
    break;

  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
  case icSigBToA3Tag:
    if (pTag && pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.pcs, m_Header.colorSpace);
    break;

  case icSigHToS0Tag:
  case icSigHToS1Tag:
  case icSigHToS2Tag:
  case icSigHToS3Tag:
    if (pTag && pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.pcs, m_Header.pcs);
    break;

  case icSigGamutTag:
    if (pTag && pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.pcs, icSigGamutData);
    break;

  case icSigNamedColor2Tag:
    {
      if (pTag && pTag->GetType()==icSigNamedColor2Type) {
        ((CIccTagNamedColor2*)pTag)->SetColorSpaces(m_Header.pcs, m_Header.colorSpace);
      }
      else if (pTag && pTag->GetTagArrayType()==icSigNamedColorArray) {
        CIccArrayNamedColor *pAry = (CIccArrayNamedColor*)icGetTagArrayHandler(pTag);

        if (pAry) {
          pAry->SetColorSpaces(m_Header.pcs, m_Header.colorSpace, 
                               m_Header.spectralPCS, 
                               &m_Header.spectralRange, 
                               &m_Header.biSpectralRange);
        }
      }
    }
    break;
  default:
      break;
  }

  return true;
}


// entry function for converting xml to icc
bool CIccProfileXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{  
  if (icXmlStrCmp(pNode->name, "IccProfile")) {
    return false;
  }

  xmlNode *hdrNode = icXmlFindNode(pNode->children, "Header");

  // parse header
  if (!hdrNode || !ParseBasic(hdrNode, parseStr))
    return false;

  // parse each tag
  xmlNode *tagNode = icXmlFindNode(pNode->children, "Tags");
  if (!tagNode)
    return false;

  for (tagNode = tagNode->children; tagNode; tagNode = tagNode->next) {
    if (tagNode->type == XML_ELEMENT_NODE) {
      if (!ParseTag(tagNode, parseStr))
        return false;
    }
  }

  return true;
}

// entry function for converting icc to xml
bool CIccProfileXml::LoadXml(const char *szFilename, const char *szRelaxNGDir, std::string *parseStr)
{  
  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;

  std::string my_parseStr;

  if (!parseStr)
    parseStr = &my_parseStr;

  *parseStr = "";

  /* Parse the file and get the DOM.
   *
   * Still no XML_PARSE_HUGE: it disables the nesting-depth cap and the
   * name-length cap for every input, and it is not needed to read back a
   * large CLUT - icXmlReadFileBounded parses from one contiguous buffer,
   * which avoids the streaming text-node cap that refused iccToXml's own
   * output in issue #1856, and bounds the document instead. Keep
   * XML_PARSE_NONET (no network), do not set XML_PARSE_NOENT (it substitutes
   * entity references and may load local external entities), and do not set
   * XML_PARSE_DTDLOAD. ICC profiles never use DTD entities legitimately.
   *
   * parseStr is set up above rather than after the parse so that a rejection
   * on size reaches the caller as a reason instead of a bare parse failure.
   */
  doc = icXmlReadFileBounded(szFilename, XML_PARSE_NONET, parseStr);

  if (doc == NULL)
    return false;

  if (szRelaxNGDir && szRelaxNGDir[0]) {
    /* #1999: each libxml2 object created below has its own free function, and
     * none of them used to be called on any path: this block was four bare
     * "return false" statements and a fall-through that freed nothing, so a
     * schema-validating run leaked the parser context, the compiled schema
     * and the validation context whether validation passed or failed.
     * bValid records the outcome so the cleanup runs once on the way out
     * instead of being repeated at each exit.
     *
     * Ordering matters: the schema returned by xmlRelaxNGParse outlives its
     * parser context, and the validation context refers to the schema, so
     * they are released innermost-first.
     */
    bool bValid = false;
    xmlRelaxNGParserCtxt* rlxParser = xmlRelaxNGNewParserCtxt(szRelaxNGDir);

    //validate the xml file
    if (rlxParser) {
      xmlRelaxNG* relaxNG = xmlRelaxNGParse(rlxParser);

      if (relaxNG) {
        xmlRelaxNGValidCtxt* validCtxt = xmlRelaxNGNewValidCtxt(relaxNG);

        if (validCtxt) {
          int result = xmlRelaxNGValidateDoc(validCtxt, doc);

          if (result != 0)
            printf("\nError: %d: '%s' is an invalid XML file.\n", result, szFilename);
          else
            bValid = true;

          xmlRelaxNGFreeValidCtxt(validCtxt);
        }

        xmlRelaxNGFree(relaxNG);
      }

      xmlRelaxNGFreeParserCtxt(rlxParser);
    }

    /* Same rejection as before for a schema that will not load, will not
     * compile, or that the document fails - only now the document goes back
     * with it. icXmlReadFileBounded hands ownership of doc to this function
     * (see IccUtilXml.h) and the sole xmlFreeDoc was past the tail of the
     * function, so these exits used to walk away from it. LSan does not
     * flag that one as leaked - the block stays reachable from libxml2 - so
     * this is an ownership fix rather than a measured leak, unlike the
     * RelaxNG objects above. */
    if (!bValid) {
      xmlFreeDoc(doc);
      return false;
    }
  }
   
  /* parseStr was bound and cleared before the parse above, so that a size
   * rejection could be reported; nothing has written to it on the path that
   * reaches here. */

  /*Get the root element node */
  root_element = xmlDocGetRootElement(doc);

  bool rv = ParseXml(root_element, *parseStr);

  xmlFreeDoc(doc);

  return rv;
}
