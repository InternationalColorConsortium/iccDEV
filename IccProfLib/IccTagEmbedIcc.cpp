/** @file
File:       IccTagEmbedIcc.cpp

Contains:   Implementation of the CIccTagEmbedProfile class

Version:    V1

Copyright:  ? see ICC Software License
*/

/*
* The ICC Software License, Version 0.2
*
*
* Copyright (c) 2003-2018 The International Color Consortium. All rights
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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#include <windows.h>
#endif
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "IccTagEmbedIcc.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include "IccTagFactory.h"
#include "IccIO.h"
#include "IccDefs.h"

/**
****************************************************************************
* Name: CIccTagEmbedProfile::CIccTagEmbedProfile
*
* Purpose: Constructor
*
*****************************************************************************
*/
CIccTagEmbeddedProfile::CIccTagEmbeddedProfile()
{
  m_pProfile = NULL;
}

/**
****************************************************************************
* Name: CIccTagEmbedProfile::CIccTagEmbedProfile
*
* Purpose: Copy Constructor
*
* Args:
*  ITU = The CIccTagEmbedProfile object to be copied
*****************************************************************************
*/
CIccTagEmbeddedProfile::CIccTagEmbeddedProfile(const CIccTagEmbeddedProfile &ITEP)
{
  if (ITEP.m_pProfile) {
    m_pProfile = ITEP.m_pProfile->NewCopy();
  }
  else
    m_pProfile = NULL;
}

/**
****************************************************************************
* Name: CIccTagEmbedProfile::operator=
*
* Purpose: Copy Operator
*
* Args:
*  UnknownTag = The CIccTagEmbedProfile object to be copied
*****************************************************************************
*/
CIccTagEmbeddedProfile &CIccTagEmbeddedProfile::operator=(const CIccTagEmbeddedProfile &ITEP)
{
  if (&ITEP == this)
    return *this;

  if (m_pProfile)
    delete m_pProfile;

  if (ITEP.m_pProfile)
    m_pProfile = ITEP.m_pProfile->NewCopy();
  else
    m_pProfile = NULL;

  return *this;
}

/**
****************************************************************************
* Name: CIccTagEmbedProfile::~CIccTagEmbedProfile
*
* Purpose: Destructor
*****************************************************************************
*/
CIccTagEmbeddedProfile::~CIccTagEmbeddedProfile()
{
  if (m_pProfile)
    delete m_pProfile;
}


/**
****************************************************************************
* Name: CIccTagEmbedProfile::SetProfile
*
* Purpose: Associate a profile object with the tag
*****************************************************************************
*/
void CIccTagEmbeddedProfile::SetProfile(CIccProfile* pProfile)
{
  //delete old profile as appropriate
  if (pProfile != m_pProfile && m_pProfile) {
    delete m_pProfile;
    m_pProfile = NULL;
  }
  m_pProfile = pProfile;
}


/**
****************************************************************************
* Name: CIccTagEmbedProfile::Read
*
* Purpose: Read in an unknown tag type into a data block
*
* Args:
*  size - # of bytes in tag,
*  pIO - IO object to read tag from
*  pProfile - pProfile object calling read
*
* Return:
*  true = successful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedProfile::Read(icUInt32Number size, CIccIO *pIO, CIccProfile *pProfile)
{
  if (size<sizeof(icTagTypeSignature) || !pIO) {
    return false;
  }

  icTagTypeSignature nType;

  if (!pIO->Read32(&nType))
    return false;
  
  if (nType != GetType())
    return false;

  size = size - sizeof(icTagTypeSignature);

  if (size < sizeof(m_nReserved))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  size = size - sizeof(m_nReserved);

  CIccEmbedIO *pEmbedIO = new CIccEmbedIO();

  if (!pEmbedIO->Attach(pIO, size)) {
    delete pEmbedIO;
    return false;
  }

  if (m_pProfile)
    delete m_pProfile;

  m_pProfile = pProfile ? pProfile->NewProfile() : new CIccProfile();

  if (pProfile && pProfile->HasIO()) {
    if (!m_pProfile->Attach(pEmbedIO)) {
      delete pEmbedIO;
      delete m_pProfile;
      m_pProfile = NULL;

      return false;
    }
  }
  else {
    bool stat = m_pProfile->Read(pEmbedIO);
    delete pEmbedIO;

    if (!stat) {
      delete m_pProfile;
      m_pProfile = NULL;

      return false;
    }

    return stat;
  }

  return true;
}

/**
****************************************************************************
* Name: CIccTagEmbedProfile::ReadAll
*
* Purpose: Read all tags associated with the embedded profile
*
*
* Return:
*  true = successful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedProfile::ReadAll()
{
  if (!m_pProfile)
    return true;

  return m_pProfile->ReadTags(m_pProfile);
}


/**
****************************************************************************
* Name: CIccTagEmbedProfile::Write
*
* Purpose: Write an unknown tag to a file
*
* Args:
*  pIO - The IO object to write tag to.
*
* Return:
*  true = succesful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedProfile::Write(CIccIO *pIO)
{
  if (!pIO)
    return false;

  icTagTypeSignature nType = GetType();

  if (!pIO->Write32(&nType))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;


  if (m_pProfile) {
    CIccEmbedIO embedIO;
    if (!embedIO.Attach(pIO))
      return false;

    icProfileIDSaveMethod bWriteId = (m_pProfile->m_Header.profileID.ID32[0] ||
                                      m_pProfile->m_Header.profileID.ID32[1] ||
                                      m_pProfile->m_Header.profileID.ID32[2] ||
                                      m_pProfile->m_Header.profileID.ID32[3]) ? icAlwaysWriteID : icVersionBasedID;
    bool rv = m_pProfile->Write(&embedIO, bWriteId);
    return rv;
  }

  return true;
}


static std::string fillColumns(std::string tag, std::string id, std::string offset, std::string size, std::string pad)
{
  char buf[200];
  memset(buf, ' ', 199);
  buf[199] = 0;
  strncpy(buf, tag.c_str(), tag.size());
  strncpy(buf + 30, id.c_str(), id.size());
  strncpy(buf + 40, offset.c_str(), offset.size());
  strncpy(buf + 48, size.c_str(), size.size());
  strcpy(buf + 56, pad.c_str());

  return buf;
}


/**
****************************************************************************
* Name: CIccTagEmbedProfile::Describe
*
* Purpose: Dump data associated with unknown tag to a string
*
* Args:
*  sDescription - string to concatenate tag dump to
*****************************************************************************
*/
void CIccTagEmbeddedProfile::Describe(std::string& sDescription, int /* nVerboseness */)
{
  if (m_pProfile) {
    sDescription += "Embedded profile in Tag\n";

    // Code below is copied from iccDumpProfile.cpp
    icHeader* pHdr = &m_pProfile->m_Header;
    CIccInfo Fmt;
    const size_t bufSize = 180;
    char buf[bufSize], sigbuf[180], buf2[bufSize];

    if (Fmt.IsProfileIDCalculated(&pHdr->profileID)) {
      sDescription += "Profile ID:         ";
      sDescription += Fmt.GetProfileID(&pHdr->profileID);
      sDescription += "\n";
    }
    else
      sDescription += "Profile ID:         Profile ID not calculated.\n";
    sDescription += "Size:               ";
    snprintf(buf, bufSize, "%d(0x%x) bytes\n", pHdr->size, pHdr->size);
    sDescription += buf;
    sDescription += "\nHeader\n";
    sDescription += "------\n";
    snprintf(buf, bufSize, "Attributes:         %s\n", Fmt.GetDeviceAttrName(pHdr->attributes));
    sDescription += buf;
    snprintf(buf, bufSize, "Cmm:                %s\n", Fmt.GetCmmSigName((icCmmSignature)(pHdr->cmmId)));
    sDescription += buf;
    snprintf(buf, bufSize, "Creation Date:      %d/%d/%d  %02u:%02u:%02u\n",
      pHdr->date.month, pHdr->date.day, pHdr->date.year,
      pHdr->date.hours, pHdr->date.minutes, pHdr->date.seconds);
    sDescription += buf;
    snprintf(buf, bufSize, "Creator:            %s\n", icGetSig(buf2, bufSize, pHdr->creator));
    sDescription += buf;
    snprintf(buf, bufSize, "Data Color Space:   %s\n", Fmt.GetColorSpaceSigName(pHdr->colorSpace));
    sDescription += buf;
    snprintf(buf, bufSize, "Flags               %s\n", Fmt.GetProfileFlagsName(pHdr->flags));
    sDescription += buf;
    snprintf(buf, bufSize, "PCS Color Space:    %s\n", Fmt.GetColorSpaceSigName(pHdr->pcs));
    sDescription += buf;
    snprintf(buf, bufSize, "Platform:           %s\n", Fmt.GetPlatformSigName(pHdr->platform));
    sDescription += buf;
    snprintf(buf, bufSize, "Rendering Intent:   %s\n", Fmt.GetRenderingIntentName((icRenderingIntent)(pHdr->renderingIntent)));
    sDescription += buf;
    snprintf(buf, bufSize, "Profile Class:      %s\n", Fmt.GetProfileClassSigName(pHdr->deviceClass));
    sDescription += buf;
    if (pHdr->deviceSubClass) {
      snprintf(buf, bufSize, "Profile SubClass:   %s\n", icGetSig(buf2, bufSize, pHdr->deviceSubClass));
      sDescription += buf;
    }
    else
      sDescription += "Profile SubClass:   Not Defined\n";
    snprintf(buf, bufSize, "Version:            %s\n", Fmt.GetVersionName(pHdr->version));
    sDescription += buf;
    if (pHdr->version >= icVersionNumberV5 && pHdr->deviceSubClass) {
      snprintf(buf, bufSize, "SubClass Version:   %s\n", Fmt.GetSubClassVersionName(pHdr->version));
      sDescription += buf;
    }
    snprintf(buf, bufSize, "Illuminant:         X=%.4lf, Y=%.4lf, Z=%.4lf\n",
      icFtoD(pHdr->illuminant.X),
      icFtoD(pHdr->illuminant.Y),
      icFtoD(pHdr->illuminant.Z));
    sDescription += buf;
    snprintf(buf, bufSize, "Spectral PCS:       %s\n", Fmt.GetSpectralColorSigName(pHdr->spectralPCS));
    sDescription += buf;
    if (pHdr->spectralRange.start || pHdr->spectralRange.end || pHdr->spectralRange.steps) {
      snprintf(buf, bufSize, "Spectral PCS Range: start=%.1fnm, end=%.1fnm, steps=%d\n",
        icF16toF(pHdr->spectralRange.start),
        icF16toF(pHdr->spectralRange.end),
        pHdr->spectralRange.steps);
      sDescription += buf;
    }
    else {
      sDescription += "Spectral PCS Range: Not Defined\n";
    }

    if (pHdr->biSpectralRange.start || pHdr->biSpectralRange.end || pHdr->biSpectralRange.steps) {
      snprintf(buf, bufSize, "BiSpectral Range:     start=%.1fnm, end=%.1fnm, steps=%d\n",
        icF16toF(pHdr->biSpectralRange.start),
        icF16toF(pHdr->biSpectralRange.end),
        pHdr->biSpectralRange.steps);
      sDescription += buf;
    }
    else {
      sDescription += "BiSpectral Range:   Not Defined\n";
    }
    if (pHdr->mcs) {
      snprintf(buf, bufSize, "MCS Color Space:    %s\n", Fmt.GetColorSpaceSigName((icColorSpaceSignature)pHdr->mcs));
      sDescription += buf;
    }
    else {
      sDescription += "MCS Color Space:    Not Defined\n";
    }

    sDescription += "\nProfile Tags\n";
    sDescription += "------------\n";

    sDescription += fillColumns("Tag", "ID", "Offset", "Size", "Pad") + "\n";
    sDescription += fillColumns("---", "--", "------", "----", "---") + "\n";

    int n, closest, pad;
    TagEntryList::iterator i, j;

    // n is number of Tags in Tag Table
    for (n = 0, i = m_pProfile->m_Tags.begin(); i != m_pProfile->m_Tags.end(); i++, n++) {
      // Find closest tag after this tag, by scanning all offsets of other tags 
      closest = pHdr->size;
      for (j = m_pProfile->m_Tags.begin(); j != m_pProfile->m_Tags.end(); j++) {
        if ((i != j) && (j->TagInfo.offset >= i->TagInfo.offset + i->TagInfo.size) && ((int)j->TagInfo.offset <= closest)) {
          closest = j->TagInfo.offset;
        }
      }
      // Number of actual padding bytes between this tag and closest neighbour (or EOF)
      // Should be 0-3 if compliant. Negative number if tags overlap!
      pad = closest - i->TagInfo.offset - i->TagInfo.size;

      const size_t tempSize = 20;
      char sOffset[tempSize], sSize[tempSize], sPad[tempSize];
      snprintf(sOffset, tempSize, "%u", i->TagInfo.offset);
      snprintf(sSize, tempSize, "%u", i->TagInfo.size);
      snprintf(sPad, tempSize, "%u", pad);
      sDescription += fillColumns(Fmt.GetTagSigName(i->TagInfo.sig), icGetSig(sigbuf, bufSize, i->TagInfo.sig, false), sOffset, sSize, sPad) + "\n";
    }
  }
  else {
    sDescription += "No profile embedded in Tag\n";
  }
}

/**
******************************************************************************
* Name: CIccTagEmbedProfile::Validate
*
* Purpose: Check tag data validity.  In base class we only look at the
*  tag's reserved data value
*
* Args:
*  sig = signature of tag being validated,
*  sReport = String to add report information to
*
* Return:
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagEmbeddedProfile::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;

  if (!m_pProfile) {
    sReport += icMsgValidateWarning;
    sReport += Info.GetSigPathName(sigPath);
    sReport += " - No Profile defined for embedded profile tag.\n";

    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }

  rv = icMaxStatus(rv, m_pProfile->Validate(sReport, sigPath, m_pProfile));

  if (pProfile) {
    if (icGetSpaceSamples(m_pProfile->m_Header.colorSpace) < icGetSpaceSamples(pProfile->m_Header.colorSpace)) {
      sReport += icMsgValidateCriticalError;
      sReport += Info.GetSigPathName(sigPath);
      sReport += " - Embedded profile has fewer device channels that parent profile.\n";

      rv = icMaxStatus(rv, icValidateCriticalError);
    }
    if (m_pProfile->m_Header.deviceClass != pProfile->m_Header.deviceClass) {
      sReport += icMsgValidateCriticalError;
      sReport += Info.GetSigPathName(sigPath);
      sReport += " - device class does not match for embedded profile.\n";

      rv = icMaxStatus(rv, icValidateCriticalError);
    }
  }
  
  return rv;
}
