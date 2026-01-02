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
#include "stdio.h"
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
  char fix[bufSize];
  size_t n;
  bool nonzero;

  xml += blanks + "<IccProfile>\n";
  xml += blanks + "  <Header>\n";
  snprintf(line, bufSize, "    <PreferredCMMType>%s</PreferredCMMType>\n", icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.cmmId)));
  xml += blanks + line;
  snprintf(line, bufSize, "    <ProfileVersion>%s</ProfileVersion>\n", info.GetVersionName(m_Header.version));
  xml += blanks + line;
  if (m_Header.version & 0x0000ffff) {
    snprintf(line, bufSize,"    <ProfileSubClassVersion>%s</ProfileSubClassVersion>\n", info.GetSubClassVersionName(m_Header.version));
    xml += blanks + line;
  }
  snprintf(line, bufSize, "    <ProfileDeviceClass>%s</ProfileDeviceClass>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.deviceClass)));
  xml += blanks + line;

  if (m_Header.deviceSubClass) {
    snprintf(line, bufSize, "    <ProfileDeviceSubClass>%s</ProfileDeviceSubClass>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.deviceSubClass)));
    xml += blanks + line;
  }

  snprintf(line, bufSize, "    <DataColourSpace>%s</DataColourSpace>\n", icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.colorSpace)));
  xml += blanks + line;
  snprintf(line, bufSize, "    <PCS>%s</PCS>\n",  icFixXml(fix, icGetColorSigStr(buf, bufSize, m_Header.pcs)));
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
 //	  sprintf(line, "    <Signature>%s</Signature>\n", icFixXml(fix, icGetSigStr(buf, m_Header.magic)));
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
  
  snprintf(line, bufSize, "    <ProfileCreator>%s</ProfileCreator>\n", icFixXml(fix, icGetSigStr(buf, bufSize, m_Header.creator)));
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
      snprintf(line, bufSize, "     <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n)",
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
                snprintf(line, bufSize, "<PrivateType type=\"%s\" reserved=\"%08x\">\n", icFixXml(fix, icGetSigStr(buf, bufSize, pTag->GetType())), pTag->m_nReserved);
              else
                snprintf(line, bufSize, "<%s reserved=\"%08x\">\n", tagSig, pTag->m_nReserved); //parent node is the tag type
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
            sprintf(line, "      <TagSignature>%s</TagSignature>\n", icFixXml(fix, icGetSigStr(buf, i->TagInfo.sig)));
            xml += line;

            sigSet.insert(i->TagInfo.sig);

            // print out the rest of the tag signatures
            for (j++; j != m_Tags.end(); j++) {
              if (j->pTag == i->pTag || j->TagInfo.offset == i->TagInfo.offset) {
                sprintf(line, "      <TagSignature>%s</TagSignature>\n", icFixXml(fix, icGetSigStr(buf, j->TagInfo.sig)));
                xml += line;
                sigSet.insert(j->TagInfo.sig);
              }
            }
#endif
            //convert the rest of the tag to xml
            if (!pTagXml->ToXml(xml, blanks + "      ")) {
              printf("Unable to output tag with type %s\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));
              return false;
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

static unsigned char parseVersion(const char *szVer)
{
  unsigned char rv;
  int val = atoi(szVer);
  rv = ((val / 10) % 10) * 16 + (val % 10);

  return rv;
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
      const char *szVer = (const char*)pNode->children->content;
      std::string ver;
      unsigned long verMajor=0, verMinor=0, verClassMajor=0, verClassMinor=0;

      for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
        ver += *szVer;
      }
      verMajor = parseVersion(ver.c_str());
      ver.clear();
      if (*szVer)
        szVer++;

      if (*szVer) {
        for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
          ver += *szVer;
        }
        verMinor = parseVersion(ver.c_str());
        ver.clear();
        if (*szVer)
          szVer++;

        if (*szVer) {
          for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
            ver = *szVer;
          }
          verClassMajor = parseVersion(ver.c_str());
          ver.clear();
          if (*szVer)
            szVer++;

          if (*szVer) {
            for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
              ver = *szVer;
            }
            verClassMinor = parseVersion(ver.c_str());
            ver.clear();
          }
        }
      }

      m_Header.version = icUInt32Number( (verMajor << 24) | (verMinor << 16) | (verClassMajor << 8) | verClassMinor );
		}
    else if (!icXmlStrCmp((const char*)pNode->name, "ProfileSubClassVersion")) {
      const char *szVer = (const char*)pNode->children->content;
      std::string ver;
      unsigned long verClassMajor = 0, verClassMinor = 0;

      if (szVer) {
        for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
          ver = *szVer;
        }
        verClassMajor = (unsigned char)atoi(ver.c_str());
        ver.clear();
        
        for (; *szVer && *szVer != '.' && *szVer != ','; szVer++) {
          ver = *szVer;
        }
        verClassMinor = (unsigned char)atoi(ver.c_str());
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
        icUInt32Number vendor;
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
          if (!pNode->children) {
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
        m_Header.spectralRange.steps = (icUInt16Number)atoi(icXmlAttrValue(steps));
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
        m_Header.biSpectralRange.steps = (icUInt16Number)atoi(icXmlAttrValue(steps));
      }
    }
    else if (!icXmlStrCmp(pNode->name, "MCS")) {
      m_Header.mcs = (icMaterialColorSignature)icXmlGetChildSigVal(pNode);
    }
    else if (!icXmlStrCmp(pNode->name, "ProfileDeviceSubClass")) {
      m_Header.deviceSubClass = (icProfileClassSignature)icXmlGetChildSigVal(pNode);
    }
		else if (!icXmlStrCmp(pNode->name, "Reserved")) {
      if (pNode->children && pNode->content)
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
        xmlAttr *attr = icXmlFindAttr(pTypeNode, "type");
        sigType = (icTagTypeSignature)icGetSigVal((icChar*)icXmlAttrValue(attr));
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
          AttachTag(sigTag, pTag);
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
        char str[100];
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
      xmlAttr *attr = icXmlFindAttr(pNode, "type");
      sigType = (icTagTypeSignature)icGetSigVal((icChar*)icXmlAttrValue(attr));
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

        for (xmlNode *tagSigNode = pNode->children; tagSigNode; tagSigNode = tagSigNode->next) {
          if (tagSigNode->type == XML_ELEMENT_NODE && !icXmlStrCmp(tagSigNode->name, "TagSignature")) {
            if ((const icChar*)tagSigNode->children != NULL) {
              sigTag = (icTagSignature)icGetSigVal((const icChar*)tagSigNode->children->content);
              AttachTag(sigTag, pTag);
            }
          }
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

  /*parse the file and get the DOM */
  doc = xmlReadFile(szFilename, NULL, 0);

  if (doc == NULL) 
    return NULL;

  if (szRelaxNGDir && szRelaxNGDir[0]) {
    xmlRelaxNGParserCtxt* rlxParser;
    
    rlxParser = xmlRelaxNGNewParserCtxt (szRelaxNGDir);

    //validate the xml file
    if (rlxParser){
	    xmlRelaxNG* relaxNG = xmlRelaxNGParse(rlxParser);
	    if (relaxNG){
		    xmlRelaxNGValidCtxt* validCtxt = xmlRelaxNGNewValidCtxt(relaxNG);
		    if (validCtxt){
			    int result = xmlRelaxNGValidateDoc(validCtxt, doc);
			    if (result != 0){
				    printf("\nError: %d: '%s' is an invalid XML file.\n", result, szFilename);
				    return false;
			    }
		    }
		    else
		      return false;
	    }
	    else
		  return false;		  
    }
    else
	    return false;  
  }
   
  std::string my_parseStr;

  if (!parseStr)
    parseStr = &my_parseStr;

  *parseStr = "";

  /*Get the root element node */
  root_element = xmlDocGetRootElement(doc);

  bool rv = ParseXml(root_element, *parseStr);

  xmlFreeDoc(doc);

  return rv;
}
