/** @file
File:       IccTagEmbedIcc.cpp

Contains:   Implementation of the CIccTagEmbedProfile class

Version:    V1

Copyright:  ? see ICC Software License
*/

/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <climits>
#include "IccTagEmbedIcc.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include "IccTagFactory.h"
#include "IccIO.h"
#include "IccDefs.h"

// Wrap this translation unit in the iccDEV namespace under the library-wide
// USEICCDEVNAMESPACE convention (see IccArrayFactory.cpp / IccTagFactory.cpp).
// This localizes the file-local anonymous namespace below so it documents as
// iccDEV::anonymous_namespace{IccTagEmbedIcc.cpp} instead of a top-level
// anonymous_namespace{} (issue #1425). The macro is off in the default build,
// so this is compiled away there and only the Doxygen pass / namespace-enabled
// builds see the wrapper; behaviour is unchanged either way.
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

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
  if (pProfile != m_pProfile) {
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
// Guard against attacker-crafted profiles that nest embedded-profile tags
// arbitrarily deep. Each level adds a C++ stack frame through
// CIccProfile::Read -> ReadTags/LoadTag -> CIccTag::Create(icSigEmbeddedProfileTag)
// -> CIccTagEmbeddedProfile::Read -> CIccProfile::Read (recursion). On
// Emscripten's ~1 MB stack this aborts at ~50-150 levels; on native ~500-1500.
// 8 nesting levels is far beyond any legitimate use.
namespace {
  static thread_local int s_embeddedProfileDepth = 0;
  static const int kMaxEmbeddedProfileDepth = 8;

  struct EmbedDepthGuard {
    EmbedDepthGuard()  { ++s_embeddedProfileDepth; }
    ~EmbedDepthGuard() { --s_embeddedProfileDepth; }
    bool TooDeep() const { return s_embeddedProfileDepth > kMaxEmbeddedProfileDepth; }
  };
}

bool CIccTagEmbeddedProfile::Read(icUInt32Number size, CIccIO *pIO, CIccProfile *pProfile)
{
  EmbedDepthGuard depthGuard;
  if (depthGuard.TooDeep())
    return false;

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

  delete m_pProfile;

  m_pProfile = pProfile ? pProfile->NewProfile() : new CIccProfile();

  // ICC.2:2023 7.2.4 requires the profile-size field to equal the exact
  // profile extent. The ICC5 tag's payload boundary is the embedded profile's
  // file boundary, so accepting a different declared size hides trailing or
  // missing bytes from nested-profile validation.
  if (pProfile && pProfile->HasIO()) {
    if (!m_pProfile->Attach(pEmbedIO)) {
      delete pEmbedIO;
      delete m_pProfile;
      m_pProfile = NULL;

      return false;
    }
    if (m_pProfile->m_Header.size != size) {
      delete m_pProfile;
      m_pProfile = NULL;

      return false;
    }
    std::string layoutReport;
    icValidateStatus layoutStatus = m_pProfile->CheckTagLayout(pEmbedIO, layoutReport);
    if (layoutStatus == icValidateNonCompliant ||
        layoutStatus == icValidateCriticalError) {
      delete m_pProfile;
      m_pProfile = NULL;

      return false;
    }
  }
  else {
    bool stat = m_pProfile->Read(pEmbedIO);
    if (stat && m_pProfile->m_Header.size != size)
      stat = false;
    if (stat) {
      std::string layoutReport;
      icValidateStatus layoutStatus = m_pProfile->CheckTagLayout(pEmbedIO, layoutReport);
      if (layoutStatus == icValidateNonCompliant ||
          layoutStatus == icValidateCriticalError)
        stat = false;
    }
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
* Name: CIccTagEmbedProfile::DetachIO
*
* Purpose: Releases the inner profile's hold on its parent's IO once this
*  tag's owning profile detaches from its own IO.
*
* CIccTagEmbeddedProfile::Read() (above) can leave m_pProfile attached to a
* CIccEmbedIO that wraps the IO object passed in by the profile that read
* this tag (the "parent"). CIccProfile::Detach() (IccProfile.cpp) calls
* DetachIO() on every one of its loaded tags before deleting its own IO; the
* base CIccTag::DetachIO() is an empty virtual, so without this override the
* inner profile kept reporting HasIO()==true over an IO the parent had just
* freed. This override calls the inner profile's own Detach(), which is safe
* precisely because the caller (CIccProfile::Detach()) runs it before
* deleting the parent's IO -- the inner's CIccEmbedIO is released while what
* it wraps is still alive.
*
* Cascade and its recursion depth: CIccProfile::Detach() loops over *its*
* loaded tags calling DetachIO(), so if m_pProfile itself holds a loaded
* CIccTagEmbeddedProfile (a nested embedded profile), that tag's DetachIO()
* runs too, and so on. The recursion depth this can reach is bounded by how
* deep the *loaded* tag tree is, not by how deeply the file nests embedded
* profiles: a lazily-opened profile has nothing loaded below the tag entries
* it was asked for (e.g. via FindTag()/FindAllTags()), so a one-level load
* only ever recurses one level here. Building a deeply loaded tree at all
* requires a caller who already paid ReadTags()' recursive cost to build it,
* and ~CIccTagEmbeddedProfile's `delete m_pProfile` already recurses the same
* way when that tree is torn down -- this does not introduce any new
* unbounded recursion.
*
* This is deliberately destructive to anything the inner profile had not yet
* loaded: once its IO is gone, tags it has not read can never be read. That
* is what detaching means -- a caller that wants the inner profile's contents
* must load them first (e.g. via FindAllTags()) before detaching the outer
* profile. DetachIO() must not read anything to compensate; doing so would
* reintroduce the unbounded ReadAll()-style descent this branch removed.
*
* Note this detaches the inner profile whatever the origin of its IO, not only
* a parent-derived CIccEmbedIO. A profile handed to SetProfile() with its own
* independently opened IO is detached too. That is safe -- such a profile owns
* that IO and Detach() frees it correctly -- and it is consistent with this tag
* owning m_pProfile outright, but it does mean a caller cannot keep a
* separately-attached inner profile readable across the owning profile's
* Detach().
*****************************************************************************
*/
void CIccTagEmbeddedProfile::DetachIO()
{
  if (m_pProfile)
    m_pProfile->Detach();
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
    snprintf(buf, bufSize, "%u(0x%x) bytes\n", (unsigned int) pHdr->size, (unsigned int) pHdr->size);
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
    snprintf(buf, bufSize, "Data Colour Space:  %s\n", Fmt.GetColorSpaceSigName(pHdr->colorSpace));
    sDescription += buf;
    snprintf(buf, bufSize, "Flags               %s\n", Fmt.GetProfileFlagsName(pHdr->flags));
    sDescription += buf;
    snprintf(buf, bufSize, "PCS Colour Space:   %s\n", Fmt.GetColorSpaceSigName(pHdr->pcs));
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
      snprintf(buf, bufSize, "MCS Colour Space:   %s\n", Fmt.GetColorSpaceSigName((icColorSpaceSignature)pHdr->mcs));
      sDescription += buf;
    }
    else {
      sDescription += "MCS Colour Space:   Not Defined\n";
    }

    sDescription += "\nProfile Tags\n";
    sDescription += "------------\n";

    sDescription += fillColumns("Tag", "ID", "Offset", "Size", "Pad") + "\n";
    sDescription += fillColumns("---", "--", "------", "----", "---") + "\n";

    int n, pad;
    icUInt32Number closest;
    TagEntryList::iterator i, j;

    // n is number of Tags in Tag Table
    for (n = 0, i = m_pProfile->m_Tags.begin(); i != m_pProfile->m_Tags.end(); i++, n++) {
      // Find closest tag after this tag, by scanning all offsets of other tags
      // NOTE - if this O(N^2) search is a performance problem, copy algorithm from iccDumpProfile
      closest = pHdr->size;
      for (j = m_pProfile->m_Tags.begin(); j != m_pProfile->m_Tags.end(); j++) {
        if ((i != j) && (i->TagInfo.size <= (0xFFFFFFFF - i->TagInfo.offset)) && (j->TagInfo.offset >= i->TagInfo.offset + i->TagInfo.size) && (j->TagInfo.offset <= closest)) {
          closest = j->TagInfo.offset;
        }
      }
      
      // Number of actual padding bytes between this tag and closest neighbour (or EOF)
      // Should be 0-3 if compliant. Negative number if tags overlap!
      int64_t temp = (int64_t)closest - i->TagInfo.offset - i->TagInfo.size;
      if (temp > (int64_t)INT_MAX)
        pad = INT_MAX;
      else if (temp < (int64_t)INT_MIN)
        pad = INT_MIN;
      else
        pad = (int)temp;

      const size_t tempSize = 20;
      char sOffset[tempSize], sSize[tempSize], sPad[tempSize];
      snprintf(sOffset, tempSize, "%u", (unsigned int)i->TagInfo.offset);
      snprintf(sSize, tempSize, "%u", (unsigned int)i->TagInfo.size);
      snprintf(sPad, tempSize, "%d", pad);
      sDescription += fillColumns(Fmt.GetTagSigName(i->TagInfo.sig), icGetSig(sigbuf, bufSize, i->TagInfo.sig, false), sOffset, sSize, sPad) + "\n";
    }
  }
  else {
    sDescription += "No profile embedded in Tag\n";
  }
}

/* A colorant table entry's name, which need not be NUL-terminated. */
static std::string icColorantEntryName(const icColorantTableEntry *pEntry)
{
  size_t n = 0;
  while (n < sizeof(pEntry->name) && pEntry->name[n])
    n++;
  return std::string((const char*)pEntry->name, n);
}

/**
******************************************************************************
* Name: icCheckSupersetColorantOrder
*
* Purpose: Check that an embedded profile whose device space is a superset of
*  its parent's begins with the parent's channels in the same order (ICC.2
*  annex on embedding).  Only colorant names can show this, so the check is
*  made when both profiles have a loaded colorantTableTag, and is otherwise
*  silent.  The first channel whose name differs is reported.
*
* Return:
*  icValidateOK, or icValidateNonCompliant when the order differs.
******************************************************************************
*/
static icValidateStatus icCheckSupersetColorantOrder(const CIccProfile *pChild, const CIccProfile *pParent,
                                                     const std::string &sigPath, std::string &sReport)
{
  const CIccTag *pChildTag = pChild->FindTagConst(icSigColorantTableTag);
  const CIccTag *pParentTag = pParent->FindTagConst(icSigColorantTableTag);

  if (!pChildTag || !pParentTag ||
      pChildTag->GetType() != icSigColorantTableType || pParentTag->GetType() != icSigColorantTableType)
    return icValidateOK;

  // GetEntry() has no const overload; the entries are only read here.
  CIccTagColorantTable *pChildTable = const_cast<CIccTagColorantTable*>(static_cast<const CIccTagColorantTable*>(pChildTag));
  CIccTagColorantTable *pParentTable = const_cast<CIccTagColorantTable*>(static_cast<const CIccTagColorantTable*>(pParentTag));
  icUInt32Number nParent = pParentTable->GetSize();
  icUInt32Number nChild = pChildTable->GetSize();

  for (icUInt32Number i = 0; i < nParent; i++) {
    std::string sParentName = icColorantEntryName(pParentTable->GetEntry(i));
    std::string sChildName = i < nChild ? icColorantEntryName(pChildTable->GetEntry(i)) : std::string();

    if (i >= nChild || sChildName != sParentName) {
      CIccInfo Info;
      char szIndex[16];
      snprintf(szIndex, sizeof(szIndex), "%u", (unsigned)(i + 1));

      sReport += icMsgValidateNonCompliant;
      sReport += Info.GetSigPathName(sigPath);
      sReport += " - Embedded profile channel ";
      sReport += szIndex;
      if (i < nChild) {
        sReport += " is colorant \"";
        sReport += sChildName;
        sReport += "\"";
      }
      else {
        sReport += " has no colorant table entry";
      }
      sReport += " but the parent profile's channel ";
      sReport += szIndex;
      sReport += " is \"";
      sReport += sParentName;
      sReport += "\"; a superset device space shall begin with the parent's channels in the same order.\n";

      return icValidateNonCompliant;
    }
  }

  return icValidateOK;
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

  // The proposed ICC.2 annex on embedding: "Redundant tags are not required
  // in the embedded iccMAX profile as long as the redundant tags are not
  // required for the correct processing of other tags in the embedded iccMAX
  // profile".  The embedded profile is validated with the encapsulating
  // profile as its parent, so a required tag the parent holds satisfies the
  // embedded profile's requirement unless the embedded profile's own tags
  // depend on it.  This used to pass the embedded profile as its own parent,
  // so the parent's tags never counted.
  rv = icMaxStatus(rv, m_pProfile->Validate(sReport, sigPath, pProfile));

  // The annex's header requirements: "Bit position 0 shall be set to 1 to
  // indicate that the profile is embedded in another file", and "Only
  // iccMAX profiles containing 0 in bit position 1 should be embedded in
  // other profiles".  Technical Note 04-2018 said "should" for both (#2693);
  // the annex makes bit 0 a requirement, so it is non-compliance, and bit 1
  // stays a warning.  Leaving to the encapsulating profile what it fully
  // defines does not make the embedded profile dependent on it.
  if (!(m_pProfile->m_Header.flags & icEmbeddedProfileTrue)) {
    sReport += icMsgValidateNonCompliant;
    sReport += Info.GetSigPathName(sigPath);
    sReport += " - Embedded profile header flags do not mark it as embedded in another file.\n";

    rv = icMaxStatus(rv, icValidateNonCompliant);
  }
  if (m_pProfile->m_Header.flags & icUseWithEmbeddedDataOnly) {
    sReport += icMsgValidateWarning;
    sReport += Info.GetSigPathName(sigPath);
    sReport += " - Embedded profile header flags say it cannot be used independently of embedded colour data.\n";

    rv = icMaxStatus(rv, icValidateWarning);
  }

  if (pProfile) {
    // The annex's header requirements: the embedded profile "is a logical
    // extension for the profile that it is embedded into.  Therefore it shall
    // be of the same profile class, and the device space shall be either the
    // same as or a superset of the device space of the encapsulating ICC
    // profile."  Technical Note 04-2018 required the same device space
    // (#2693).
    //
    // A superset has more channels than the parent's space and begins with
    // the parent's channels in the same order.  Among the standard spaces,
    // CMYK is a superset of CMY.  A generic n-channel space (xCLR or ncXXXX)
    // with more channels is a named color superset: its colorants are named
    // by the profile itself, which is the only way to tell which they are.
    // A different standard space, such as CMYK under an RGB host, is not a
    // superset of it.
    icColorSpaceSignature childSpace = m_pProfile->m_Header.colorSpace;
    icColorSpaceSignature parentSpace = pProfile->m_Header.colorSpace;
    icUInt32Number nChildSamples = icGetSpaceSamples(childSpace);
    icUInt32Number nParentSamples = icGetSpaceSamples(parentSpace);
    bool bNamedColorSuperset = nChildSamples > nParentSamples && icIsSpaceCLR(childSpace);
    bool bStandardSuperset = parentSpace == icSigCmyData && childSpace == icSigCmykData;

    if (childSpace != parentSpace && !bStandardSuperset) {
      if (!bNamedColorSuperset) {
        sReport += icMsgValidateCriticalError;
        sReport += Info.GetSigPathName(sigPath);
        sReport += " - Embedded profile device space ";
        sReport += Info.GetColorSpaceSigName(childSpace);
        sReport += " is neither the same as nor a superset of the parent profile's ";
        sReport += Info.GetColorSpaceSigName(parentSpace);
        sReport += ".\n";

        rv = icMaxStatus(rv, icValidateCriticalError);
      }
      // A named color superset has channels the parent's colorant tags do
      // not define, so the embedded profile needs its own channel
      // definition.  A standard superset's channels are defined by its space.
      else if (!m_pProfile->GetTag(icSigColorantInfoTag) &&
               !m_pProfile->GetTag(icSigColorantTableTag)) {
        sReport += icMsgValidateNonCompliant;
        sReport += Info.GetSigPathName(sigPath);
        sReport += " - Embedded profile device space ";
        sReport += Info.GetColorSpaceSigName(childSpace);
        sReport += " is a named color superset of the parent profile's ";
        sReport += Info.GetColorSpaceSigName(parentSpace);
        sReport += " but has neither a colorantInfoTag nor a colorantTableTag to define its channels.\n";

        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
      // A superset's first channels shall be the parent's, in the same
      // order, so that the additional channels follow them.  Only colorant
      // names can show that, so it is checked when both profiles have a
      // colorantTableTag.
      else {
        rv = icMaxStatus(rv, icCheckSupersetColorantOrder(m_pProfile, pProfile, sigPath, sReport));
      }
    }
    if (nChildSamples < nParentSamples) {
      sReport += icMsgValidateCriticalError;
      sReport += Info.GetSigPathName(sigPath);
      sReport += " - Embedded profile has fewer device channels than parent profile.\n";

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

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
