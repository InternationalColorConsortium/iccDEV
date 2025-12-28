/** @file
    File:       IccProfile.cpp

    Contains:   Implementation of the CIccProfile class.

    Version:    V1

    Copyright:  � see ICC Software License
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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
#endif
#include <ctime>
#include <cstring>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "IccProfile.h"
#include "IccTag.h"
#include "IccArrayBasic.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccMatrixMath.h"
#include "IccMD5.h"


#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/**
 **************************************************************************
 * Name: CIccProfile::CIccProfile
 * 
 * Purpose: 
 *  Constructor
 **************************************************************************
 */
CIccProfile::CIccProfile()
{
  m_pAttachIO = NULL;
  m_bSharedIO = false;
  memset(&m_Header, 0, sizeof(m_Header));

  m_parentColorSpace = icSigNoColorData;
}

/**
 **************************************************************************
 * Name: CIccProfile::CIccProfile
 * 
 * Purpose: 
 *  Copy Constructor. The copy constructor makes the copy of the 
 *  CIccProfile object in it's present state. It DOES NOT make a 
 *  copy of the m_pAttachIO member variable. Any operation with the
 *  IO object should be done before making a copy.
 * 
 * Args:
 *  Profile = CIccProfile object which is to be copied.
 **************************************************************************
 */
CIccProfile::CIccProfile(const CIccProfile &Profile)
{
  m_pAttachIO = NULL;
  memset(&m_Header, 0, sizeof(m_Header));
  memcpy(&m_Header, &Profile.m_Header, sizeof(m_Header));

  if (!Profile.m_TagVals.empty()) {
    TagPtrList::const_iterator i;
    IccTagPtr tagptr = {0};
    for (i=Profile.m_TagVals.begin(); i!=Profile.m_TagVals.end(); i++) {
      tagptr.ptr = i->ptr->NewCopy();
      m_TagVals.push_back(tagptr);
    }
  }

  if (!Profile.m_Tags.empty()) {
    TagEntryList::const_iterator i;
    IccTagEntry entry = {};
    for (i=Profile.m_Tags.begin(); i!=Profile.m_Tags.end(); i++) {
      TagPtrList::const_iterator j, k;

      //Make sure that tag entry values point to shared tags in m_TagVals
      for (j=Profile.m_TagVals.begin(), k=m_TagVals.begin(); j!=Profile.m_TagVals.end() && k!=m_TagVals.end(); j++, k++) {
        if (i->pTag == j->ptr) {
          //k should point to the the corresponding copied tag
          entry.pTag = k->ptr;
          break;
        }
      }

      if (j==Profile.m_TagVals.end()) {  //Did we not find the tag?
        entry.pTag = NULL;
      }

      memcpy(&entry.TagInfo, &i->TagInfo, sizeof(icTag));
      m_Tags.push_back(entry);
    }
  }

  m_pAttachIO = NULL;  
}

/**
 **************************************************************************
 * Name: CIccProfile::operator=
 * 
 * Purpose: 
 *  Copy Operator. The copy operator makes the copy of the 
 *  CIccProfile object in it's present state. It DOES NOT make a 
 *  copy of the m_pAttachIO member variable. Any operation with the
 *  IO object should be done before making a copy.
 * 
 * Args:
 *  Profile = CIccProfile object which is to be copied.
 **************************************************************************
 */
CIccProfile &CIccProfile::operator=(const CIccProfile &Profile)
{
  if (&Profile == this)
    return *this;

  Cleanup();

  memcpy(&m_Header, &Profile.m_Header, sizeof(m_Header));
  m_parentColorSpace = Profile.m_parentColorSpace;

  if (!Profile.m_TagVals.empty()) {
    TagPtrList::const_iterator i;
    IccTagPtr tagptr = {0};
    for (i=Profile.m_TagVals.begin(); i!=Profile.m_TagVals.end(); i++) {
      tagptr.ptr = i->ptr->NewCopy();
      m_TagVals.push_back(tagptr);
    }
  }

  if (!Profile.m_Tags.empty()) {
    TagEntryList::const_iterator i;
    IccTagEntry entry = {};
    for (i=Profile.m_Tags.begin(); i!=Profile.m_Tags.end(); i++) {
      TagPtrList::const_iterator j, k;

      //Make sure that tag entry values point to shared tags in m_TagVals
      for (j=Profile.m_TagVals.begin(), k=m_TagVals.begin(); j!=Profile.m_TagVals.end() && k!=m_TagVals.end(); j++, k++) {
        if (i->pTag == j->ptr) {
          //k should point to the the corresponding copied tag
          entry.pTag = k->ptr;
          break;
        }
      }

      if (j==Profile.m_TagVals.end()) {  //Did we not find the tag?
        entry.pTag = NULL;
      }

      memcpy(&entry.TagInfo, &i->TagInfo, sizeof(icTag));
      m_Tags.push_back(entry);
    }
  }

  m_pAttachIO = NULL;

  return *this;  
}

/**
 **************************************************************************
 * Name: CIccProfile::CIccProfile
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccProfile::~CIccProfile()
{
  Cleanup();
}

/**
 ***************************************************************************
 * Name: CIccProfile::Cleanup
 * 
 * Purpose: Detach from a pending IO object
 ***************************************************************************
 */
void CIccProfile::Cleanup()
{
  if (m_pAttachIO && !m_bSharedIO) {
    delete m_pAttachIO;
  }
  m_pAttachIO = nullptr;

  TagPtrList::iterator i;

  for (i=m_TagVals.begin(); i!=m_TagVals.end(); i++) {
    if (i->ptr != nullptr) {
      delete i->ptr;
    }
  }
  m_Tags.clear();
  m_TagVals.clear();
  memset(&m_Header, 0, sizeof(m_Header));
  m_parentColorSpace = icSigNoColorData;
}

/**
 ****************************************************************************
 * Name: CIccProfile::GetTag
 * 
 * Purpose: Get a tag entry with a given signature
 * 
 * Args: 
 *  sig - signature id to find in tag directory
 * 
 * Return: 
 *  Pointer to desired tag directory entry, or NULL if not found.
 *****************************************************************************
 */
IccTagEntry* CIccProfile::GetTag(icSignature sig) const
{
  TagEntryList::const_iterator i;

  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if (i->TagInfo.sig==(icTagSignature)sig)
      return (IccTagEntry*)&(i->TagInfo);
  }

  return NULL;
}


/**
 ****************************************************************************
 * Name: CIccProfile::GetTag
 *
 * Purpose: Get a tag entry with a given signature
 *
 * Args:
 *  sig - signature id to find in tag directory
 *  pParentProfile is pointer to parent object of embedded profile
 *
 * Return:
 *  Pointer to desired tag directory entry, or NULL if not found.
 *****************************************************************************
 */
IccTagEntry* CIccProfile::GetTag(icSignature sig, const CIccProfile *pParentProfile) const
{
  TagEntryList::const_iterator i;

  for (i = m_Tags.begin(); i != m_Tags.end(); i++) {
    if (i->TagInfo.sig == (icTagSignature)sig)
      return (IccTagEntry*)&(i->TagInfo);
  }

  if (pParentProfile)
    return pParentProfile->GetTag(sig);

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccProfile::AreTagsUnique
 * 
 * Purpose: For each tag it checks to see if any other tags have the same
 *  signature.
 * 
 * 
 * Return: 
 *  true if all tags have unique signatures, or false if there are duplicate
 *  tag signatures.
 *******************************************************************************
 */
bool CIccProfile::AreTagsUnique() const
{
  typedef std::unordered_map<icTagSignature,int> tag_lookup_map;
  tag_lookup_map tag_lookup;
  
  int n;
  TagEntryList::const_iterator i;
  
  for (n=0, i = m_Tags.begin(); i != m_Tags.end(); ++i, n++) {
    // check for a previous tag with the same sig/type
    tag_lookup_map::const_iterator found = tag_lookup.find(i->TagInfo.sig);
    if ( found != tag_lookup.end() ) {
      return false;
    } else {
      // else this is the first of this type seen, so insert it into our list
      tag_lookup[i->TagInfo.sig] = n;
    }
  }

  return true;
}


/**
******************************************************************************
* Name: CIccProfile::GetTag
* 
* Purpose: Finds the first tag entry that points to the indicated tag object
* 
* Args: 
*  pTag - pointer to tag object desired to be found
* 
* Return: 
*  pointer to first tag directory entry that points to the desired tag object,
*  or NULL if tag object is not pointed to by any tag directory entries.
*******************************************************************************
*/
IccTagEntry* CIccProfile::GetTag(CIccTag *pTag) const
{
  TagEntryList::const_iterator i;

  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if (i->pTag==pTag)
      return (IccTagEntry*)&(i->TagInfo);
  }

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccProfile::FindTag
 * 
 * Purpose: Finds the tag object associated with the directory entry with the
 *  given signature.  If the profile object is attached to an IO object then
 *  the tag may need to be loaded first.
 *  This has to do a search of the tag list, and may be slow.
 *
 * Args: 
 *  sig - tag signature to find in profile
 * 
 * Return: 
 *  The desired tag object, or NULL if unable to find in the directory or load
 *  tag object.
 *******************************************************************************
 */
CIccTag* CIccProfile::FindTag(icSignature sig)
{
  IccTagEntry *pEntry = GetTag(sig);
  if (pEntry)
    return FindTag( *pEntry );
  else
    return NULL;
}


/**
 ******************************************************************************
 * Name: CIccProfile::FindTag
 * 
 * Purpose: Return the tag object associated with the directory entry given,
 *  and load if the profile object is attached to an IO object.
 *  This is faster when iterating tags, to avoid O(N^2) behavior.
 *
 * Args: 
 *  entry - IccTagEntry for this tag
 *
 * Return: 
 *  The desired tag object, or NULL if unable to load the tag object.
 *
 *******************************************************************************
 */
CIccTag* CIccProfile::FindTag(IccTagEntry &entry)
{
  if (!entry.pTag && m_pAttachIO)
    LoadTag(&entry, m_pAttachIO);
  return entry.pTag;
}

/**
 ******************************************************************************
 * Name: CIccProfile::FindTagConst
 *
 * Purpose: Finds the loaded tag object associated with the directory entry
 *  with the given signature.  
 *
 * Args:
 *  sig - tag signature to find in profile
 *
 * Return:
 *  The desired tag object, or NULL if unable to find the loaded tag.
 *******************************************************************************
 */
const CIccTag* CIccProfile::FindTagConst(icSignature sig) const
{
  IccTagEntry* pEntry = GetTag(sig);

  if (pEntry) {
    return pEntry->pTag;
  }

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccProfile::FindTagOfType
 * 
 * Purpose: Finds the tag object associated with the directory entry with the
 *  given signature that has a given tag type signature..  
 *  If the profile object is attached to an IO object then the tag may need to
 *  be loaded first.
 * 
 * Args: 
 *  tagSig - tag signature to find in profile
 *  typeSig - tag signature to find in profile
 * 
 * Return: 
 *  The desired tag object that has the desired tag type signature, or NULL if 
 *  unable to find in the directory, load tag object, or the tag has the
 *  wrong tag type signature.
 *******************************************************************************
 */
CIccTag* CIccProfile::FindTagOfType(icSignature tagSig, icTagTypeSignature typeSig)
{
  CIccTag *pTag = FindTag(tagSig);

  if (pTag && pTag->GetType()==typeSig)
    return pTag;

  return NULL;
}



/**
******************************************************************************
* Name: CIccProfile::GetTagIO
* 
* Purpose: Finds the tag directory entry with the given signature and returns
*  a CIccIO object that can be used to read the tag data stored in the profile.
*  This only works if the profile is still connected to the file IO object.
* 
* Args: 
*  sig - tag signature to find in profile
* 
* Return: 
*  A CIccIO object that can be used to read the tag data from the file.
*  Note: the caller is responsible for deleting the returned CIccIO object.
*******************************************************************************
*/
CIccMemIO* CIccProfile::GetTagIO(icSignature sig)
{
  IccTagEntry *pEntry = GetTag(sig);

  if (pEntry && m_pAttachIO) {
    CIccMemIO *pIO = new CIccMemIO;

    if (!pIO)
      return NULL;
    
    if (!pIO->Alloc(pEntry->TagInfo.size)) {
      delete pIO;
      return NULL;
    }

    m_pAttachIO->Seek(pEntry->TagInfo.offset, icSeekSet);
    
    const size_t expected_length = pIO->GetLength();
    size_t read_length = m_pAttachIO->Read8(pIO->GetData(), expected_length);
    if (read_length == expected_length)
      return pIO;
    else {
      delete pIO;
      return NULL;
    }
  }

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccProfile::AttachTag
 * 
 * Purpose: Assign a tag object to a directory entry in the profile.  This
 *  will assume ownership of the tag object.
 * 
 * Args: 
 *  sig - signature of tag 'name' to use to assign tag object with,
 *  pTag - pointer to tag object to attach to profile.
 * 
 * Return: 
 *  true = tag assigned to profile,
 *  false - tag not assigned to profile (tag already exists).  
 *******************************************************************************
 */
bool CIccProfile::AttachTag(icSignature sig, CIccTag *pTag)
{
  IccTagEntry *pEntry = GetTag(sig);

  if (pEntry) {
    if (pEntry->pTag == pTag)
      return true;

    return false;
  }

  IccTagEntry Entry = {};
  Entry.TagInfo.sig = (icTagSignature)sig;
  Entry.TagInfo.offset = 0;
  Entry.TagInfo.size = 0;
  Entry.pTag = pTag;

  m_Tags.push_back(Entry);

  TagPtrList::iterator i;

  for (i=m_TagVals.begin(); i!=m_TagVals.end(); i++)
    if (i->ptr == pTag)
      break;

  if (i==m_TagVals.end()) {
    IccTagPtr TagPtr = {};
    TagPtr.ptr = pTag;
    m_TagVals.push_back(TagPtr);
  }

  return true;
}


/**
 ******************************************************************************
 * Name: CIccProfile::DeleteTag
 * 
 * Purpose: Delete tag directory entry with given signature.  If no other tag
 *  directory entries use the tag object, the tag object will also be deleted.
 * 
 * Args: 
 *  sig - signature of tag directory entry to remove
 * 
 * Return: 
 *  true - desired tag directory entry was found and deleted,
 *  false - desired tag directory entry was not found
 *******************************************************************************
 */
bool CIccProfile::DeleteTag(icSignature sig)
{
  TagEntryList::iterator i;

  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if (i->TagInfo.sig==(icTagSignature)sig)
      break;
  }
  if (i!=m_Tags.end()) {
    CIccTag *pTag = i->pTag;
    m_Tags.erase(i);

    if (pTag && !GetTag(pTag)) {
      DetachTag(pTag);
      delete pTag;
    }
    return true;
  }

  return false;
}


/**
******************************************************************************
* Name: CIccProfile::ConnectSubProfile
*
* Purpose: This checks for a embedded profile tag and creates a CIccIO object
* for purposes of reading embedded profile in tag
*
* Args:
*  pIO - pointer to IO object to use to 
*  bOwnIO - flag to indicate whether returned IO object should own pIO (pIO closed when returned object closed).
*
* Return:
*  the IO object (file) if there is an embedded profile.
*  NULL if there is no embedded profile.
*******************************************************************************
*/
CIccIO* CIccProfile::ConnectSubProfile(CIccIO *pIO, bool bOwnIO) const
{
  TagEntryList::const_iterator i;

  for (i = m_Tags.begin(); i != m_Tags.end(); i++) {
    if (i->TagInfo.sig == icSigEmbeddedV5ProfileTag && i->TagInfo.size>2*sizeof(icUInt32Number)) {
      pIO->Seek(i->TagInfo.offset, icSeekSet);
      icTagTypeSignature sig=(icTagTypeSignature)0, extra;

      if (pIO->Read32(&sig) && pIO->Read32(&extra) && sig == icSigEmbeddedProfileType) {
        CIccEmbedIO *pEmbedIO = new CIccEmbedIO();
        
        if (pEmbedIO->Attach(pIO, i->TagInfo.size - 2 * sizeof(icUInt32Number), bOwnIO))
          return pEmbedIO;
      }
    }
  }

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccProfile::Attach
 * 
 * Purpose: This allows for deferred IO with a profile.  The profile header and
 *  tag directory will be read, but tag data will not be read.  The IO object
 *  will remain attached to the profile for the purpose of reading data in as
 *  needed.
 * 
 * Args: 
 *  pIO - pointer to IO object to begin reading profile file with.
 * 
 * Return: 
 *  true - the IO object (file) is an ICC profile, and the CIccProfile object
 *    is now attached to the object,
 *  false - the IO object (file) is not an ICC profile.
 *******************************************************************************
 */
bool CIccProfile::Attach(CIccIO *pIO, bool bUseSubProfile/*=false*/)
{
  if (m_Tags.size())
    Cleanup();

  if (!ReadBasic(pIO)) {
    Cleanup();
    return false;
  }

  if (bUseSubProfile) {
    CIccIO *pSubIO = ConnectSubProfile(pIO, true);

    if (pSubIO) {
      icColorSpaceSignature parentColorSpace = m_Header.colorSpace;

      Cleanup();
      m_parentColorSpace = parentColorSpace;
      if (!ReadBasic(pSubIO)) {
        Cleanup();
        return false;
      }
      pIO = pSubIO;
    }
  }

  m_pAttachIO = pIO;

  return true;
}

/**
******************************************************************************
* Name: CIccProfile::Detach
* 
* Purpose: Discontinues the use of defferred IO with a profile.  This can be done
*  once all the information needed for performing a transform has been extracted
*  from the profile.
* 
* Args: 
*  true - If an IO object was attached to the profile
*  false - if no IO object was attached to the profile
*******************************************************************************
*/
bool CIccProfile::Detach()
{
  if (m_pAttachIO && !m_bSharedIO) {
    TagEntryList::iterator i;

    for (i = m_Tags.begin(); i != m_Tags.end(); i++) {
      if (i->pTag)
        i->pTag->DetachIO();
    }

    delete m_pAttachIO;

    m_pAttachIO = NULL;
    return true;
  }
  else if (m_bSharedIO) {
    m_pAttachIO = NULL;
    m_bSharedIO = false;

    return true;
  }

  return false;
}


/**
******************************************************************************
* Name: CIccProfile::CopyAttach
*
* Purpose: Allows a copy of a profile object to share the IO of another object
*  The shared object does not own the shared IO so it will not be deleted when
*  detached.
*
* Args:
*  pProfile - pointer to a profile to get a copy of the shared IO (if null then
*             shared IO is disconnected)
*  bShareIO - flag indicating whether pAttachedIO should be considered shared
*******************************************************************************
*/

void CIccProfile::CopyAttach(CIccProfile* pProfile, bool bSharedIO)
{
  if (!pProfile) {
    m_pAttachIO = nullptr;
    m_bSharedIO = false;
  }
  else {
    m_pAttachIO = pProfile->m_pAttachIO;
    m_bSharedIO = bSharedIO;
  }
}

/**
******************************************************************************
* Name: CIccProfile::ReadTags
* 
* Purpose: This will read the all the tags from the IO object into the
*  CIccProfile object. The IO object must have been attached before
*		calling this function.
* 
* Return: 
*  true - CIccProfile object now contains all tag data,
*  false - No IO object attached or tags cannot be read.
*******************************************************************************
*/
bool CIccProfile::ReadTags(CIccProfile* pProfile)
{
	CIccIO *pIO = m_pAttachIO;
	
	if (pProfile && pProfile->m_pAttachIO) {
		pIO = pProfile->m_pAttachIO;
	}

  TagEntryList::iterator i;
  //If there is no IO handle then ReadTags is successful if they have all been
  //loaded in
  if (!pIO) {
    for (i = m_Tags.begin(); i != m_Tags.end(); i++) {
      if (!i->pTag) {
        return false;
      }
    }
		return true;
	}

	size_t pos = pIO->Tell();

	for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
		if (!LoadTag((IccTagEntry*)&(i->TagInfo), pIO, true)) {
			pIO->Seek(pos, icSeekSet);
			return false;
		}
	}

	pIO->Seek(pos, icSeekSet);

	return true;
}

/**
 ******************************************************************************
 * Name: CIccProfile::Read
 * 
 * Purpose: This will read the entire ICC profile from the IO object into the
 *  CIccProfile object
 * 
 * Args: 
 *  pIO - pointer to IO object to read ICC profile from
 *  bUseSubProfile - will attempt to open a sub-profile if present
 *
 * Return: 
 *  true - the IO object (file) is an ICC profile, and the CIccProfile object
 *   now contains all its data,
 *  false - the IO object (file) is not an ICC profile.
 *******************************************************************************
 */
bool CIccProfile::Read(CIccIO *pIO, bool bUseSubProfile/*=false*/)
{
  if (m_Tags.size())
    Cleanup();

  if (!ReadBasic(pIO)) {
    Cleanup();
    return false;
  }

  if (bUseSubProfile) {
    CIccIO *pSubIO = ConnectSubProfile(pIO, false);

    if (pSubIO) {
      icColorSpaceSignature parentColorSpace = m_Header.colorSpace;
      Cleanup();
      m_parentColorSpace = parentColorSpace;
      if (!ReadBasic(pSubIO)) {
        Cleanup();
        return false;
      }
      pIO = pSubIO;
    }
  }

  TagEntryList::iterator i;

  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if (!LoadTag((IccTagEntry*)&(i->TagInfo), pIO)) {
      Cleanup();
      return false;
    }
  }

  return true;
}

/**
******************************************************************************
* Name: CIccProfile::ReadValidate
* 
* Purpose: This will read the entire ICC profile from the IO object into the
*  CIccProfile object
* 
* Args: 
*  pIO - pointer to IO object to read ICC profile from
*  sReport - string to put validation report info into. String should be initialized
*  before calling
* 
* Return: 
*  icValidateOK if file can be read, bad status otherwise.
*******************************************************************************
*/
icValidateStatus CIccProfile::ReadValidate(CIccIO *pIO, std::string &sReport)
{
  icValidateStatus rv = icValidateOK;

  if (m_Tags.size())
    Cleanup();

  if (!ReadBasic(pIO)) {
    sReport += icMsgValidateCriticalError;
    sReport += " - Unable to read profile!**\n\tProfile has invalid structure!\n";
    Cleanup();

    return icValidateCriticalError;
  }

  // Check profile header
  if (!CheckFileSize(pIO)) {
    sReport += icMsgValidateNonCompliant;
    sReport += "Bad Header File Size\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  CIccInfo Info;
  icProfileID profileID;
  // check if bytes 84..89 are zero, if not we assume a profile id
  if (Info.IsProfileIDCalculated(&m_Header.profileID)) {
      CalcProfileID(pIO, &profileID);
      // if the provided and the calculated profileid missmatch
      if (memcmp((char *) profileID.ID8, (char *) m_Header.profileID.ID8, 16) != 0) {
          if (m_Header.version >= icVersionNumberV4) { // error with bad profile ID on v4.x.y profiles (or higher)
              sReport += icMsgValidateNonCompliant;
              sReport += "Bad Profile ID\n";
              rv = icMaxStatus(rv, icValidateNonCompliant);
          } else { // on older profiles the reserved bytes are (mis-)interpreted as profile id
              sReport += icMsgValidateWarning;
              sReport += "Version 2 profile has non-zero reserved data that doesn't match calculated Profile ID\n";
              rv = icMaxStatus(rv, icValidateWarning);
          }
      } else { // the provided and the calculated profileid match
          if (m_Header.version < icVersionNumberV4) { // the profileid should only be used in v4.x.y profiles (or higher)
              sReport += icMsgValidateWarning;
              sReport += "Version 2 profile has non-zero reserved data that matches calculated Profile ID\n";
              rv = icMaxStatus(rv, icValidateWarning);
          }
      }
  }

  TagEntryList::iterator i;

  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if ((i->TagInfo.offset % 4) != 0) {
        sReport += icMsgValidateNonCompliant;
        sReport += Info.GetTagSigName(i->TagInfo.sig);
        sReport += " - Offset is not aligned on 4-byte boundary!\n";

        rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    if (!LoadTag((IccTagEntry*)&(i->TagInfo), pIO)) {
      sReport += icMsgValidateCriticalError;
      sReport += " - ";
      sReport += Info.GetTagSigName(i->TagInfo.sig);
      sReport += " - Tag has invalid structure!\n";

      rv = icMaxStatus(rv, icValidateCriticalError);
    }
  }

  if (rv==icValidateCriticalError)
    Cleanup();

  return rv;
}


/**
 ******************************************************************************
 * Name: CIccProfile::Write
 * 
 * Purpose: Write the data associated with the CIccProfile object to an IO
 *  IO object.
 * 
 * Args: 
 *  pIO - pointer to IO object to write data to
 * 
 * Return: 
 *  true - success, false - failure
 *******************************************************************************
 */
bool CIccProfile::Write(CIccIO *pIO, icProfileIDSaveMethod nWriteId)
{
  //Write Header
  pIO->Seek(0, icSeekSet);

  pIO->Write32(&m_Header.size);
  pIO->Write32(&m_Header.cmmId);
  pIO->Write32(&m_Header.version);
  pIO->Write32(&m_Header.deviceClass);
  pIO->Write32(&m_Header.colorSpace);
  pIO->Write32(&m_Header.pcs);
  pIO->Write16(&m_Header.date.year);
  pIO->Write16(&m_Header.date.month);
  pIO->Write16(&m_Header.date.day);
  pIO->Write16(&m_Header.date.hours);
  pIO->Write16(&m_Header.date.minutes);
  pIO->Write16(&m_Header.date.seconds);
  pIO->Write32(&m_Header.magic);
  pIO->Write32(&m_Header.platform);
  pIO->Write32(&m_Header.flags);
  pIO->Write32(&m_Header.manufacturer);
  pIO->Write32(&m_Header.model);
  pIO->Write64(&m_Header.attributes);
  pIO->Write32(&m_Header.renderingIntent);
  pIO->Write32(&m_Header.illuminant.X);
  pIO->Write32(&m_Header.illuminant.Y);
  pIO->Write32(&m_Header.illuminant.Z);
  pIO->Write32(&m_Header.creator);
  pIO->Write8(&m_Header.profileID, sizeof(m_Header.profileID));
  pIO->Write32(&m_Header.spectralPCS);
  pIO->Write16(&m_Header.spectralRange.start);
  pIO->Write16(&m_Header.spectralRange.end);
  pIO->Write16(&m_Header.spectralRange.steps);
  pIO->Write16(&m_Header.biSpectralRange.start);
  pIO->Write16(&m_Header.biSpectralRange.end);
  pIO->Write16(&m_Header.biSpectralRange.steps);
  pIO->Write32(&m_Header.mcs);
  pIO->Write32(&m_Header.deviceSubClass);
  pIO->Write8(&m_Header.reserved[0], sizeof(m_Header.reserved));

  TagEntryList::iterator i, j;
  icUInt32Number count;

  for (count=0, i=m_Tags.begin(); i!= m_Tags.end(); i++) {
    if (i->pTag)
      count++;
  }

  pIO->Write32(&count);

  size_t dirpos = pIO->GetLength();

  //Write Unintialized TagDir
  for (i=m_Tags.begin(); i!= m_Tags.end(); i++) {
    if (i->pTag) {
      i->TagInfo.offset = 0;
      i->TagInfo.size = 0;

      pIO->Write32(&i->TagInfo.sig);
      pIO->Write32(&i->TagInfo.offset);
      pIO->Write32(&i->TagInfo.size);
    }
  }

  //Write Tags
  for (i=m_Tags.begin(); i!= m_Tags.end(); i++) {
    if (i->pTag) {
      for (j=m_Tags.begin(); j!=i; j++) {
        if (i->pTag == j->pTag)
          break;
      }

      if (i==j) {
        i->TagInfo.offset = (icUInt32Number) pIO->GetLength();
        i->pTag->Write(pIO);
        i->TagInfo.size = (icUInt32Number)( pIO->GetLength() - i->TagInfo.offset );

        pIO->Align32();
      }
      else {
        i->TagInfo.offset = j->TagInfo.offset;
        i->TagInfo.size = j->TagInfo.size;
      }
    }
  }

  pIO->Seek(dirpos, icSeekSet);

  //Write TagDir with offsets and sizes
  for (i=m_Tags.begin(); i!= m_Tags.end(); i++) {
    if (i->pTag) {
      pIO->Write32(&i->TagInfo.sig);
      pIO->Write32(&i->TagInfo.offset);
      pIO->Write32(&i->TagInfo.size);
    }
  }

  //Update header with size
  m_Header.size = (icUInt32Number) pIO->GetLength();
  pIO->Seek(0, icSeekSet);
  pIO->Write32(&m_Header.size);

  bool bWriteId;

  switch (nWriteId) {
    case icVersionBasedID:
    default:
      bWriteId = (m_Header.version>=icVersionNumberV4);
      break;
    case icAlwaysWriteID:
      bWriteId = true;
      break;
    case icNeverWriteID:
      bWriteId = false;
  }
  if (m_Header.deviceClass==icSigColorEncodingClass) {
    bWriteId = false;
  }

  //Write the profile ID if version 4 profile
  if(bWriteId) {
    CalcProfileID(pIO, &m_Header.profileID);
    pIO->Seek(84, icSeekSet);
    pIO->Write8(&m_Header.profileID, sizeof(m_Header.profileID));
  }

  return true;
}

bool CIccProfile::ReadProfileID(icProfileID &profileID)
{
  if (!m_pAttachIO) {
    memset(&profileID, 0, sizeof(profileID));
    return false;
  }

  CalcProfileID(m_pAttachIO, &profileID);

  return true;
}


bool CIccProfile::ReadPccTags()
{
  FindTag(icSigStandardToCustomPccTag);
  FindTag(icSigCustomToStandardPccTag);
  FindTag(icSigSpectralViewingConditionsTag);

  return true;
}


/**
 ******************************************************************************
 * Name: CIccProfile::InitHeader
 * 
 * Purpose: Initializes the data to be written in the profile header.
 * 
 *******************************************************************************
 */
void CIccProfile::InitHeader()
{
  m_Header.size = 0;
  m_Header.cmmId = icSigIccDEV;
  m_Header.version=icVersionNumberV4;
  m_Header.deviceClass = (icProfileClassSignature)0;
  m_Header.colorSpace = (icColorSpaceSignature)0;
  m_Header.pcs = icSigLabData;
  
  struct tm *newtime;
  time_t long_time;

  time( &long_time );                /* Get time as long integer. */
  newtime = gmtime( &long_time ); 

  m_Header.date.year = newtime->tm_year+1900;
  m_Header.date.month = newtime->tm_mon+1;
  m_Header.date.day = newtime->tm_mday;
  m_Header.date.hours = newtime->tm_hour;
  m_Header.date.minutes = newtime->tm_min;
  m_Header.date.seconds = newtime->tm_sec;

  m_Header.magic = icMagicNumber;
  m_Header.platform = (icPlatformSignature)0;
  m_Header.flags = 0;
  m_Header.manufacturer=0;
  m_Header.model=0;
  m_Header.attributes=0;
  m_Header.renderingIntent=icPerceptual;
  m_Header.illuminant.X = icDtoF((icFloatNumber)0.9642);
  m_Header.illuminant.Y = icDtoF((icFloatNumber)1.0000);
  m_Header.illuminant.Z = icDtoF((icFloatNumber)0.8249);
  m_Header.creator = icSigIccDEV;
  m_Header.spectralPCS = icSigNoSpectralData;
  m_Header.spectralRange.start = 0;
  m_Header.spectralRange.end = 0;
  m_Header.spectralRange.steps = 0;
  m_Header.biSpectralRange.start = 0;
  m_Header.biSpectralRange.end = 0;
  m_Header.biSpectralRange.steps = 0;

  memset(&m_Header.profileID, 0, sizeof(m_Header.profileID));
  memset(&m_Header.reserved[0], 0, sizeof(m_Header.reserved));
}


/**
 *****************************************************************************
 * Name: CIccProfile::ReadBasic
 * 
 * Purpose: Read in ICC header and tag directory entries.
 * 
 * Args: 
 *  pIO - pointer to IO object to read data with
 * 
 * Return: 
 *  true - valid ICC header and tag directory, false - failure
 ******************************************************************************
 */
bool CIccProfile::ReadBasic(CIccIO *pIO)
{
  //Read Header
  if (pIO->Seek(0, icSeekSet)<0 ||
      !pIO->Read32(&m_Header.size) ||
      !pIO->Read32(&m_Header.cmmId) ||
      !pIO->Read32(&m_Header.version) ||
      !pIO->Read32(&m_Header.deviceClass) ||
      !pIO->Read32(&m_Header.colorSpace) ||
      !pIO->Read32(&m_Header.pcs) ||
      !pIO->Read16(&m_Header.date.year) ||
      !pIO->Read16(&m_Header.date.month) ||
      !pIO->Read16(&m_Header.date.day) ||
      !pIO->Read16(&m_Header.date.hours) ||
      !pIO->Read16(&m_Header.date.minutes) ||
      !pIO->Read16(&m_Header.date.seconds) ||
      !pIO->Read32(&m_Header.magic) ||
      !pIO->Read32(&m_Header.platform) ||
      !pIO->Read32(&m_Header.flags) ||
      !pIO->Read32(&m_Header.manufacturer) ||
      !pIO->Read32(&m_Header.model) ||
      !pIO->Read64(&m_Header.attributes) ||
      !pIO->Read32(&m_Header.renderingIntent) ||
      !pIO->Read32(&m_Header.illuminant.X) ||
      !pIO->Read32(&m_Header.illuminant.Y) ||
      !pIO->Read32(&m_Header.illuminant.Z) ||
      !pIO->Read32(&m_Header.creator) ||
      pIO->Read8(&m_Header.profileID, sizeof(m_Header.profileID))!=sizeof(m_Header.profileID) ||
      !pIO->Read32(&m_Header.spectralPCS) ||
      !pIO->Read16(&m_Header.spectralRange.start) ||
      !pIO->Read16(&m_Header.spectralRange.end) ||
      !pIO->Read16(&m_Header.spectralRange.steps) ||
      !pIO->Read16(&m_Header.biSpectralRange.start) ||
      !pIO->Read16(&m_Header.biSpectralRange.end) ||
      !pIO->Read16(&m_Header.biSpectralRange.steps) ||
      !pIO->Read32(&m_Header.mcs) ||
      !pIO->Read32(&m_Header.deviceSubClass) || 
      pIO->Read8(&m_Header.reserved[0], sizeof(m_Header.reserved))!=sizeof(m_Header.reserved)) {
    return false;
  }

  if (m_Header.magic != icMagicNumber)
    return false;

  icUInt32Number count, i;
  IccTagEntry TagEntry = {};

  TagEntry.pTag = NULL;

  if (!pIO->Read32(&count))
    return false;

  //Read TagDir
  for (i=0; i<count; i++) {
    if (!pIO->Read32(&TagEntry.TagInfo.sig) ||
        !pIO->Read32(&TagEntry.TagInfo.offset) ||
        !pIO->Read32(&TagEntry.TagInfo.size)) {
      return false;
    }
    m_Tags.push_back(TagEntry);
  }


  return true;
}


/**
 ******************************************************************************
 * Name: CIccProfile::LoadTag
 * 
 * Purpose: This will load from the indicated IO object and associate a tag
 *  object to a tag directory entry.  Nothing happens if tag directory entry
 *  is associated with a tag object.
 * 
 * Args: 
 *  pTagEntry - pointer to tag directory entry,
 *  pIO - pointer to IO object to read tag object data from
 * 
 * Return: 
 *  true - tag directory object associated with tag directory entry,
 *  false - failure
 *******************************************************************************
 */
bool CIccProfile::LoadTag(IccTagEntry *pTagEntry, CIccIO *pIO, bool bReadAll/*=false*/)
{
  if (!pTagEntry)
    return false;

  if (pTagEntry->pTag)
    return pTagEntry->pTag->ReadAll();

  // if the tag claims to be inside the header, or zero length, return an error
  if (pTagEntry->TagInfo.offset<sizeof(m_Header) ||
    !pTagEntry->TagInfo.size) {
    return false;
  }
  
  // if the tag claims to be longer than the actual file, return an error
  // NOTE - ccox - it would be nice to cache the file length instead of calculating it per tag
  if ( (pTagEntry->TagInfo.offset + pTagEntry->TagInfo.size) > pIO->GetLength())
    return false;

  icTagTypeSignature sigType;

  //First we need to get the tag type to create the right kind of tag
  if (pIO->Seek(pTagEntry->TagInfo.offset, icSeekSet)!= pTagEntry->TagInfo.offset)
    return false;

  if (!pIO->Read32(&sigType))
    return false;

  CIccTag *pTag = CIccTag::Create(sigType);

  if (!pTag)
    return false;

  //Now seek back to where the tag starts so the created tag object can read
  //in its data.
  if (pIO->Seek(pTagEntry->TagInfo.offset, icSeekSet)!= pTagEntry->TagInfo.offset) {
    delete pTag;
    return false;
  }

  if (!pTag->Read(pTagEntry->TagInfo.size, pIO, this)) {
    delete pTag;
    return false;
  }

  if (bReadAll) {
    if (!pTag->ReadAll()) {
      delete pTag;
      return false;
    }
  }

  switch(pTagEntry->TagInfo.sig) {
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
    if (pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.colorSpace, m_Header.pcs);
    break;

  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
    if (pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.pcs, m_Header.colorSpace);
    break;
  
  case icSigGamutTag:
    if (pTag->IsMBBType())
      ((CIccMBB*)pTag)->SetColorSpaces(m_Header.pcs, icSigGamutData);
    break;

  case icSigNamedColor2Tag:
    if (pTag->GetTagArrayType()==icSigNamedColorArray) {
      CIccArrayNamedColor *pNamed = (CIccArrayNamedColor*)icGetTagArrayHandler(pTag);
      if (pNamed)
        pNamed->SetColorSpaces(m_Header.pcs, m_Header.colorSpace,
                               m_Header.spectralPCS,
                               &m_Header.spectralRange,
                               &m_Header.biSpectralRange);
    }
    else if (pTag->GetType()==icSigNamedColor2Type) {
      ((CIccTagNamedColor2*)pTag)->SetColorSpaces(m_Header.pcs, m_Header.colorSpace);
    }
    break;

  default:
    break;
  }

  pTagEntry->pTag = pTag;

  IccTagPtr TagPtr = {};

  TagPtr.ptr = pTag;

  m_TagVals.push_back(TagPtr);

  TagEntryList::iterator i;

  for (i=m_Tags.begin(); i!= m_Tags.end(); i++) {
    if (i->TagInfo.offset == pTagEntry->TagInfo.offset &&
        i->pTag != pTag)
      i->pTag = pTag; 
  }
  
  return true;
}


/**
 ******************************************************************************
 * Name: CIccProfile::DetachTag
 * 
 * Purpose: Remove association of a tag object from all tag directory entries.
 *  Associated tag directory entries will be removed from the tag directory.
 *  The tag object is NOT deleted from memory, but is considered to be
 *  no longer associated with the CIccProfile object.  The caller assumes
 *  ownership of the tag object.
 * 
 * Args: 
 *  pTag - pointer to tag object unassociate with the profile object
 * 
 * Return: 
 *  true - tag object found and unassociated with profile object,
 *  false - tag object not found
 *******************************************************************************
 */
bool CIccProfile::DetachTag(CIccTag *pTag)
{
  if (!pTag)
    return false;
  
  TagPtrList::iterator i;

  for (i=m_TagVals.begin(); i!=m_TagVals.end(); i++) {
    if (i->ptr == pTag)
      break;
  }

  if (i==m_TagVals.end())
    return false;

  m_TagVals.erase(i);

  TagEntryList::iterator j;
  for (j=m_Tags.begin(); j!=m_Tags.end();) {
    if (j->pTag == pTag) {
      j=m_Tags.erase(j);
    }
    else
      j++;
  }
  return true;
}

static inline bool compare_float(float x, float y, float eps=0.01f) {
    return (fabsf(x-y)<eps);
}

#if 0
// currently unused
static inline bool compare_float(double x, double y, double eps=0.0000001f) {
    return (fabs(x-y)<eps);
}
#endif


/**
****************************************************************************
* Name: CIccProfile::CheckHeader
* 
* Purpose: Validates profile header.
* 
* Return: 
*  icValidateOK if valid, or other error status.
*****************************************************************************
*/
icValidateStatus CIccProfile::CheckHeader(std::string &sReport) const
{
  icValidateStatus rv = icValidateOK;

  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo Info;

  switch(m_Header.deviceClass) {
  case icSigInputClass:
  case icSigDisplayClass:
  case icSigOutputClass:
  case icSigLinkClass:
  case icSigColorSpaceClass:
  case icSigAbstractClass:
  case icSigNamedColorClass:
    break;

  case icSigColorEncodingClass:
  case icSigMaterialIdentificationClass:
  case icSigMaterialVisualizationClass:
  case icSigMaterialLinkClass:
    if (m_Header.version<icVersionNumberV5) {
      CIccInfo classInfo;
      sReport += icMsgValidateCriticalError;
      snprintf(buf, bufSize, " - %s not supported in Version %s profiles!\n", classInfo.GetProfileClassSigName(m_Header.deviceClass),
                                                                      Info.GetVersionName(m_Header.version));
      sReport += buf;
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
    break;

  default:
    sReport += icMsgValidateCriticalError;
    snprintf(buf, bufSize, " - %s: Unknown profile class!\n", Info.GetProfileClassSigName(m_Header.deviceClass));
    sReport += buf;
    rv = icMaxStatus(rv, icValidateCriticalError);
  }


  if (m_Header.deviceClass==icSigMaterialIdentificationClass ||
      m_Header.deviceClass==icSigMaterialLinkClass ||
      m_Header.deviceClass==icSigMaterialVisualizationClass) {
    if (icGetColorSpaceType(m_Header.mcs)!=icSigSrcMCSChannelData) {
      sReport += icMsgValidateCriticalError;
      sReport += " - Invalid MCS designator\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
  }
  else if (m_Header.mcs != icSigNoMCSData && m_Header.deviceClass != icSigInputClass) {
    sReport += icMsgValidateNonCompliant;
    sReport += " - Invalid MCS designator for device class\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }
 
  if (m_Header.colorSpace!=icSigNoColorData ||
        m_Header.version<icVersionNumberV5 || 
        (m_Header.deviceClass!=icSigNamedColorClass &&
         m_Header.deviceClass!=icSigMaterialIdentificationClass &&
         m_Header.deviceClass!=icSigMaterialVisualizationClass)) {
    if (!Info.IsValidSpace(m_Header.colorSpace)) {
      if (!(m_Header.version>=icVersionNumberV5 && m_Header.deviceClass==icSigAbstractClass && Info.IsValidSpectralSpace(m_Header.colorSpace) && IsTagPresent(icSigDToB0Tag))) {
        sReport += icMsgValidateCriticalError;
        snprintf(buf, bufSize, " - %s: Unknown color space!\n", Info.GetColorSpaceSigName(m_Header.colorSpace));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateCriticalError);
      }
    }
  }

  if (m_Header.deviceClass==icSigMaterialIdentificationClass ||
      m_Header.deviceClass==icSigMaterialLinkClass) {
    if (m_Header.pcs!=icSigNoColorData) {
      sReport += icMsgValidateNonCompliant;
      snprintf(buf, bufSize, "Invalid PCS designator for %s\n", Info.GetProfileClassSigName(m_Header.deviceClass));
      sReport += buf;
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else if (m_Header.deviceClass==icSigColorEncodingClass) {
    if (m_Header.cmmId ||
      m_Header.pcs ||
      m_Header.date.day || m_Header.date.month || m_Header.date.year ||
      m_Header.date.hours || m_Header.date.minutes || m_Header.date.seconds ||
      m_Header.platform ||
      m_Header.flags ||
      m_Header.manufacturer ||
      m_Header.model ||
      m_Header.attributes ||
      m_Header.renderingIntent ||
      m_Header.illuminant.X || m_Header.illuminant.Y || m_Header.illuminant.Z ||
      m_Header.creator ||
      m_Header.profileID.ID32[0] || m_Header.profileID.ID32[1] || 
      m_Header.profileID.ID32[2] || m_Header.profileID.ID32[3] ||
      m_Header.spectralPCS ||
      m_Header.spectralRange.start || m_Header.spectralRange.end || m_Header.spectralRange.steps ||
      m_Header.biSpectralRange.start || m_Header.biSpectralRange.end || m_Header.biSpectralRange.steps
      ) {
      sReport += icMsgValidateNonCompliant;
      snprintf(buf, bufSize, " - Encoding Class has non-zero Header data were zeros are required!\n");
      sReport += buf;
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else {
    if (m_Header.deviceClass==icSigLinkClass) {
      if (!Info.IsValidSpace(m_Header.pcs)) {
        sReport += icMsgValidateCriticalError;
        snprintf(buf, bufSize, " - %s: Unknown pcs color space!\n", Info.GetColorSpaceSigName(m_Header.pcs));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateCriticalError);
      }
    }
    else {
      switch(m_Header.pcs) {
        case icSigNoColorData:
        case icSigXYZData:
        case icSigLabData:
          break;

        default:
          sReport += icMsgValidateCriticalError;
          snprintf(buf, bufSize, " - %s: Invalid pcs color space!\n", Info.GetColorSpaceSigName(m_Header.pcs));
          sReport += buf;
          rv = icMaxStatus(rv, icValidateCriticalError);
          break;
      }

      if (m_Header.spectralPCS && m_Header.version<icVersionNumberV5) {
        sReport += icMsgValidateNonCompliant;
        snprintf(buf, bufSize, " - Spectral PCS usage in version %s ICC profile!\n", Info.GetVersionName(m_Header.version));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }

      switch(icGetColorSpaceType(m_Header.spectralPCS)) {
// ERROR -- comparison of different enum types!
        case icSigNoSpectralData:
          if (m_Header.spectralRange.start ||
              m_Header.spectralRange.end ||
              m_Header.spectralRange.steps ||
              m_Header.biSpectralRange.start ||
              m_Header.biSpectralRange.end ||
              m_Header.biSpectralRange.steps) {
                sReport += icMsgValidateWarning;
                snprintf(buf, bufSize, "%s - Spectral PCS wavelengths defined with no spectral PCS!\n", Info.GetColorSpaceSigName(m_Header.pcs));
                sReport += buf;
                rv = icMaxStatus(rv, icValidateWarning);
          }
          break;

// ERROR - case type is not part of enum!
        case icSigBiSpectralReflectanceData:
        case icSigSparseMatrixReflectanceData:
          if (icGetColorSpaceType(m_Header.spectralPCS)==icSigBiSpectralReflectanceData) {
            if (icNumColorSpaceChannels(m_Header.spectralPCS)!=m_Header.biSpectralRange.steps * m_Header.spectralRange.steps) {
              sReport += icMsgValidateCriticalError;
              sReport += "Number of channels defined for spectral PCS do not match spectral range definitions.\n";
              rv = icMaxStatus(rv, icValidateCriticalError);
            }
          }

          if (icF16toF(m_Header.biSpectralRange.end)<=icF16toF(m_Header.biSpectralRange.start)) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "end BiDir Spectral PCS wavelength must be larger than start BiDir Spectral PCS wavelength!\n");
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }

          if (m_Header.biSpectralRange.steps<2) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "%d: Must have more 2 or more BiDir spectral wavelength steps!\n", m_Header.biSpectralRange.steps);
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
          if (icF16toF(m_Header.spectralRange.end)<=icF16toF(m_Header.spectralRange.start)) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "end Spectral PCS wavelength must be larger than start Spectral PCS wavelength!\n");
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
          if (m_Header.spectralRange.steps<2) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "%d: Must have more 2 or more spectral wavelength steps!\n", m_Header.spectralRange.steps);
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
          break;

        case icSigReflectanceSpectralData:
        case icSigTransmisionSpectralData:
        case icSigRadiantSpectralData:
          if (icNumColorSpaceChannels(m_Header.spectralPCS)!=m_Header.spectralRange.steps) {
            sReport += icMsgValidateCriticalError;
            sReport += "Number of channels defined for spectral PCS do not match spectral range definition.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
          if (icF16toF(m_Header.spectralRange.end)<=icF16toF(m_Header.spectralRange.start)) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "end Spectral PCS wavelength must be larger than start Spectral PCS wavelength!\n");
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }

          if (m_Header.spectralRange.steps<2) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "%d: Must have more 2 or more spectral wavelength steps!\n", m_Header.spectralRange.steps);
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
          if (m_Header.biSpectralRange.start ||
              m_Header.biSpectralRange.end ||
              m_Header.biSpectralRange.steps) {
            sReport += icMsgValidateCriticalError;
            snprintf(buf, bufSize, "%s - Spectral PCS wavelengths defined with no spectral PCS!\n", Info.GetColorSpaceSigName(m_Header.pcs));
            sReport += buf;
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
          break;

        default:
          sReport += icMsgValidateCriticalError;
          snprintf(buf, bufSize, "%s: Invalid spectral PCS color space!\n", Info.GetColorSpaceSigName((icColorSpaceSignature)m_Header.spectralPCS));
          sReport += buf;
          rv = icMaxStatus(rv, icValidateCriticalError);
          break;
      }

      if (m_Header.pcs==icSigNoColorData && m_Header.spectralPCS==icSigNoSpectralData) {
        sReport += icMsgValidateCriticalError;
        snprintf(buf, bufSize, "Both Colorimetric PCS or Spectral PCS are not defined!\n");
        sReport += buf;
        rv = icMaxStatus(rv, icValidateCriticalError);
      }

    }

    rv = icMaxStatus(rv, Info.CheckData(sReport, m_Header.date, "Header date"));

    switch(m_Header.platform) {
    case icSigMacintosh:
    case icSigMicrosoft:
    case icSigSolaris:
    case icSigSGI:
    case icSigTaligent:
    case icSigUnknownPlatform:
      break;
    
    default:
      sReport += icMsgValidateWarning;
      snprintf(buf, bufSize, "%s: Unknown platform signature.\n", Info.GetPlatformSigName(m_Header.platform));
      sReport += buf;
      rv = icMaxStatus(rv, icValidateWarning);
    }

    // Report on various bits of profile flags as per Table 21 in v4.3.0
    if(m_Header.version<icVersionNumberV5 && m_Header.flags & 0x0000FFFC) {
        sReport += icMsgValidateNonCompliant;
        sReport += "Reserved profile flags (bits 2-15) are non-zero.\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    else if (m_Header.version == icVersionNumberV5 && m_Header.flags & 0x0000FFF8) {
      sReport += icMsgValidateNonCompliant;
      sReport += "Reserved profile flags (bits 3-15) are non-zero.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    else if (m_Header.flags & 0x0000FFF0) {
      sReport += icMsgValidateNonCompliant;
      sReport += "Reserved profile flags (bits 4-15) are non-zero.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }

    if(m_Header.flags & 0xFFFF0000) {
        sReport += icMsgValidateWarning;
        sReport += "Vendor-specific profile flags (bits 16-32) are non-zero.\n";
        rv = icMaxStatus(rv, icValidateWarning);
    }

    // Report on various bits of device attributes as per Table 22 in v4.3.0 
    if(m_Header.attributes & 0x0000FFF0) {
        sReport += icMsgValidateNonCompliant;
        sReport += "Reserved device attributes (bits 4-31) are non-zero.\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    if(m_Header.attributes & 0xFFFF0000) {
        sReport += icMsgValidateWarning;
        sReport += "Vendor-specific device attributes (bits 32-63) are non-zero.\n";
        rv = icMaxStatus(rv, icValidateWarning);
    }

    icUInt8Number  bcdpair = (icUInt8Number)(m_Header.version >> 24);
    // Report on unusual version (stored as BCD)
    if (bcdpair<0x05 && (m_Header.version & 0x0000FFFF)) {
        sReport += icMsgValidateWarning;
        sReport += "Version number bytes 10 and 11 are reserved but non-zero.\n";
        rv = icMaxStatus(rv, icValidateWarning);
    }
    switch (bcdpair) {
    case 0x02:
        bcdpair = (icUInt8Number)((m_Header.version & 0x00FF0000) >> 16);
        if ((bcdpair > 0x40) || (bcdpair & 0x0F)) {
            sReport += icMsgValidateWarning;
            sReport += "Version 2 minor number is unexpected.\n";
            rv = icMaxStatus(rv, icValidateWarning);
        }
        break;
    case 0x04:
        bcdpair = (icUInt8Number)((m_Header.version & 0x00FF0000) >> 16);
        if ((bcdpair > 0x40) || (bcdpair & 0x0F)) {
            sReport += icMsgValidateWarning;
            sReport += "Version 4 minor number is unexpected.\n";
            rv = icMaxStatus(rv, icValidateWarning);
        }
        break;
    case 0x05:
        bcdpair = (icUInt8Number)((m_Header.version & 0x00FF0000) >> 16);
        if ((bcdpair > 0x10) || (bcdpair & 0x0F)) {
            sReport += icMsgValidateWarning;
            sReport += "Version 5 minor number is unexpected.\n";
            rv = icMaxStatus(rv, icValidateWarning);
        }
        break;
    default:
        sReport += icMsgValidateWarning;
        snprintf(buf, bufSize, "Major version number (%d) is unexpected.\n", ((bcdpair >> 4) * 10 + (bcdpair & 0x0F)));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateWarning);
    }

    switch((icCmmSignature)m_Header.cmmId) {
    //Account for registered CMM's as well:
    case icSigUnknownCmm:
    case icSigAdobe:
    case icSigAgfa:
    case icSigApple:
    case icSigColorGear:
    case icSigColorGearLite:
    case icSigColorGearC:
    case icSigEFI:
    case icSigExactScan:
    case icSigFujiFilm:
    case icSigHarlequinRIP:
    case icSigArgyllCMS:
    case icSigLogoSync:
    case icSigHeidelberg:
    case icSigLittleCMS:
    case icSigKodak:
    case icSigKonicaMinolta:
    case icSigWindowsCMS:
    case icSigMutoh:
    case icSigRefIccMAX:
    case icSigDemoIccMAX:
	case icSigIccDEV:
    case icSigRolfGierling:
    case icSigSampleICC:
    case icSigToshiba:
    case icSigTheImagingFactory:
    case icSigVivo:
    case icSigWareToGo:
    case icSigZoran:
    case icSigOnyxGraphics:
      break;

    default:
      sReport += icMsgValidateWarning;
      snprintf(buf, bufSize, "%s: Unregistered CMM signature.\n", Info.GetCmmSigName((icCmmSignature)m_Header.cmmId));
      sReport += buf;
      rv = icMaxStatus(rv, icValidateWarning);
    }

    switch(m_Header.renderingIntent) {
    case icPerceptual:
    case icRelativeColorimetric:
    case icSaturation:
    case icAbsoluteColorimetric:
      break;

    default:
      sReport += icMsgValidateCriticalError;
      snprintf(buf, bufSize, "%s: Unknown rendering intent!\n", Info.GetRenderingIntentName((icRenderingIntent)m_Header.renderingIntent));
      sReport += buf;
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    rv = icMaxStatus(rv, Info.CheckData(sReport, m_Header.illuminant, "Header Illuminant"));
    icFloatNumber X = icFtoD(m_Header.illuminant.X);
    icFloatNumber Y = icFtoD(m_Header.illuminant.Y);
    icFloatNumber Z = icFtoD(m_Header.illuminant.Z);
    if (m_Header.version<icVersionNumberV5
        && (
               (!compare_float(X, 0.9642f, 0.0004f))
            || (!compare_float(Y, 1.0f, 0.0004f))
            || (!compare_float(Z, 0.8249f, 0.0004f))
            )
        ){
      sReport += icMsgValidateNonCompliant;
      sReport += "Non D50 Illuminant XYZ values";
      sReport +="\r\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }

  int sum=0, num = sizeof(m_Header.reserved) / sizeof(m_Header.reserved[0]);
  for (int i=0; i<num; i++) {
    sum += m_Header.reserved[i];
  }
  if (sum) {
    sReport += icMsgValidateNonCompliant;
    sReport += "Reserved value must be zero.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}


/**
****************************************************************************
* Name: CIccProfile::CheckTagExclusion
* 
* Purpose: Some tags does not have a defined interpretation for a profile 
*           of a specific class. This function does these tests.
* 
* Return: 
*  true if test successful, else false.
*****************************************************************************
*/
bool CIccProfile::CheckTagExclusion(std::string &sReport) const
{
  bool rv = true;

  CIccInfo Info;
  const size_t bufSize = 128;
  icChar buf[bufSize];
  snprintf(buf, bufSize, "%s", Info.GetSigName(m_Header.deviceClass));
  if (m_Header.deviceClass!=icSigInputClass && m_Header.deviceClass!=icSigDisplayClass &&
      m_Header.deviceClass != icSigColorEncodingClass) {
    if (GetTag(icSigGrayTRCTag) || GetTag(icSigRedTRCTag) || GetTag(icSigGreenTRCTag) ||
       GetTag(icSigBlueTRCTag) || GetTag(icSigRedColorantTag) || GetTag(icSigGreenColorantTag) ||
       GetTag(icSigBlueColorantTag) || GetTag(icSigCicpTag))
    {
      sReport += icMsgValidateWarning;
      sReport += buf;
      sReport += "Tag exclusion test failed.\n";
      rv = false;
    }
  }

  switch(m_Header.deviceClass) {
  case icSigNamedColorClass:
    {
      if (GetTag(icSigAToB0Tag) || GetTag(icSigAToB1Tag) || GetTag(icSigAToB2Tag) ||
        GetTag(icSigBToA0Tag) || GetTag(icSigBToA1Tag) || GetTag(icSigBToA2Tag) ||
        GetTag(icSigProfileSequenceDescTag) || GetTag(icSigGamutTag))
      {
        sReport += icMsgValidateWarning;
        sReport += buf;
        sReport += "Tag exclusion test failed.\n";
        rv = false;
      }
      break;
    }

  case icSigMaterialIdentificationClass:
  case icSigMaterialLinkClass:
  case icSigMaterialVisualizationClass:
    {
      if (GetTag(icSigAToB0Tag) || GetTag(icSigAToB1Tag) || GetTag(icSigAToB2Tag) ||
        GetTag(icSigBToA0Tag) || GetTag(icSigBToA1Tag) || GetTag(icSigBToA2Tag) ||
        GetTag(icSigProfileSequenceDescTag) || GetTag(icSigGamutTag) || GetTag(icSigNamedColor2Tag))
      {
        sReport += icMsgValidateWarning;
        sReport += buf;
        sReport += "Tag exclusion test failed.\n";
        rv = false;
      }
      break;
    }
  case icSigAbstractClass:
    {
      if (GetTag(icSigNamedColor2Tag) ||
        GetTag(icSigAToB1Tag) || GetTag(icSigAToB2Tag) ||
        GetTag(icSigBToA1Tag) || GetTag(icSigBToA2Tag) || GetTag(icSigGamutTag))
      {
        sReport += icMsgValidateWarning;
        sReport += buf;
        sReport += "Tag exclusion test failed.\n";
        rv = false;
      }
      break;
    }

  case icSigLinkClass:
    {
      if (GetTag(icSigMediaWhitePointTag) || GetTag(icSigNamedColor2Tag) ||
        GetTag(icSigAToB1Tag) || GetTag(icSigAToB2Tag) ||
        GetTag(icSigBToA1Tag) || GetTag(icSigBToA2Tag) || GetTag(icSigGamutTag))
      {
        sReport += icMsgValidateWarning;
        sReport += buf;
        sReport += "Tag exclusion test failed.\n";
        rv = false;
      }
      break;
    }

  default:
    {
    }
  }

  return rv;
}


/**
****************************************************************************
* Name: CIccProfile::CheckTagTypes
* 
* Purpose: Check if tags have allowed tag types.
* 
* Return: 
*  icValidateOK if valid, or other error status.
*****************************************************************************
*/
icValidateStatus CIccProfile::CheckTagTypes(std::string &sReport) const
{
  icValidateStatus rv = icValidateOK;

  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo Info;
  
  TagEntryList::const_iterator i;
  for (i = m_Tags.begin(); i != m_Tags.end(); ++i) {
    icTagSignature tagsig = i->TagInfo.sig;
    
    icTagTypeSignature typesig = icSigUnknownType;
    icStructSignature structSig = icSigUnknownStruct;
    icArraySignature arraySig = icSigUnknownArray;
    
    // missing the internal tag data would cause a problem, issue #322
    if (i->pTag) {
      typesig = i->pTag->GetType();
      structSig = i->pTag->GetTagStructType();
      arraySig = i->pTag->GetTagArrayType();
    }
    
    snprintf(buf, bufSize, "%s", Info.GetSigName(tagsig));
    if (!IsTypeValid(tagsig, typesig, structSig, arraySig)) {
      sReport += icMsgValidateNonCompliant;
      sReport += buf;
      snprintf(buf,bufSize, " %s: Invalid tag type (Might be critical!).\n", Info.GetTagTypeSigName(typesig));
      sReport += buf;
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }

  return rv;  
}


/**
****************************************************************************
* Name: CIccProfile::IsTypeValid
* 
* Purpose: Check if tags have allowed tag types.
* 
* Return: 
*  true if valid, else false.
*****************************************************************************
*/
bool CIccProfile::IsTypeValid(icTagSignature tagSig, icTagTypeSignature typeSig, 
                              icStructSignature structSig/*=icSigUnknownStruct*/,
                              icArraySignature arraySig/*=icSigUnknownStruct*/) const
{
  switch(tagSig) {
    // A to B tags
  case icSigAToB0Tag:
  case icSigAToB1Tag:
  case icSigAToB2Tag:
    {
      switch(typeSig) {
      case icSigLut8Type:
      case icSigLut16Type:
        return true;

      case icSigLutAtoBType:
        if (m_Header.version >= icVersionNumberV4)
          return true;
        else
          return false;

      case icSigMultiProcessElementType:
        if (m_Header.version >= icVersionNumberV5)
          return true;
        else
          return false;

      case icSigTagStructType:
        if (m_Header.version >= icVersionNumberV5 &&
            structSig==icSigBRDFStruct)
          return true;
        else
          return false;

      default:
        return false;
      }
    }

  case icSigAToB3Tag:
    {
      if (m_Header.version < icVersionNumberV5)
        return false;

      switch(typeSig) {
        case icSigLut8Type:
        case icSigLut16Type:
          return true;

        case icSigLutAtoBType:
          if (m_Header.version >= icVersionNumberV4)
            return true;
          else
            return false;

        case icSigMultiProcessElementType:
          if (m_Header.version >= icVersionNumberV5)
            return true;
          else
            return false;

        case icSigTagStructType:
          if (m_Header.version >= icVersionNumberV5 &&
            structSig==icSigBRDFStruct)
            return true;
          else
            return false;

        default:
          return false;
      }
    }

    // B to A tags
  case icSigBToA0Tag:
  case icSigBToA1Tag:
  case icSigBToA2Tag:
  case icSigGamutTag:
  case icSigPreview0Tag:
  case icSigPreview1Tag:
  case icSigPreview2Tag:  
    {
      switch(typeSig) {
      case icSigLut8Type:
      case icSigLut16Type:
        return true;

      case icSigLutBtoAType:
        if (m_Header.version >= icVersionNumberV4)
          return true;
        else
          return false;

      case icSigMultiProcessElementType:
        if (m_Header.version >= icVersionNumberV5)
          return true;
        else
          return false;

//       case icSigTagStructType:
//         if (m_Header.version >= icVersionNumberV5 &&
//           structSig==icSigBRDFStruct)
//           return true;
//         else
//           return false;

      default:
        return false;
      }
    }

  case icSigBToA3Tag:
    {
      if (m_Header.version < icVersionNumberV5)
        return false;

      switch(typeSig) {
        case icSigLut8Type:
        case icSigLut16Type:
          return true;

        case icSigLutBtoAType:
          if (m_Header.version >= icVersionNumberV4)
            return true;
          else
            return false;

        case icSigMultiProcessElementType:
          if (m_Header.version >= icVersionNumberV5)
            return true;
          else
            return false;

          //       case icSigTagStructType:
          //         if (m_Header.version >= icVersionNumberV5 &&
          //           structSig==icSigBRDFStruct)
          //           return true;
          //         else
          //           return false;

        default:
          return false;
      }
    }

  case icSigDToB0Tag:
  case icSigDToB1Tag:
  case icSigDToB2Tag:
  case icSigBToD0Tag:
  case icSigBToD1Tag:
  case icSigBToD2Tag:
    {
      if (m_Header.version < icVersionNumberV4_2)
        return false; 

      if (typeSig!=icSigMultiProcessElementType)
        return false;
      return true;
    }

  case icSigDToB3Tag:
  case icSigBToD3Tag:
  case icSigAToM0Tag:
  case icSigMToA0Tag:
  case icSigMToB0Tag:
  case icSigMToB1Tag:
  case icSigMToB2Tag:
  case icSigMToB3Tag:
  case icSigMToS0Tag:
  case icSigMToS1Tag:
  case icSigMToS2Tag:
  case icSigMToS3Tag:
  case icSigBRDFAToB0Tag:
  case icSigBRDFAToB1Tag:
  case icSigBRDFAToB2Tag:
  case icSigBRDFAToB3Tag:
  case icSigBRDFDToB0Tag:
  case icSigBRDFDToB1Tag:
  case icSigBRDFDToB2Tag:
  case icSigBRDFDToB3Tag:
    {
      if (m_Header.version < icVersionNumberV5)
        return false; 

      if (typeSig!=icSigMultiProcessElementType)
        return false;
      return true;
    }


  case icSigGamutBoundaryDescription0Tag:
  case icSigGamutBoundaryDescription1Tag:
  case icSigGamutBoundaryDescription2Tag:
  case icSigGamutBoundaryDescription3Tag:
    {
      if (typeSig!=icSigGamutBoundaryDescType)
        return false;
      return true;
    }

  case icSigBrdfColorimetricParameter0Tag:
  case icSigBrdfColorimetricParameter1Tag:
  case icSigBrdfColorimetricParameter2Tag:
  case icSigBrdfColorimetricParameter3Tag:
  case icSigBrdfSpectralParameter0Tag:
  case icSigBrdfSpectralParameter1Tag:
  case icSigBrdfSpectralParameter2Tag:
  case icSigBrdfSpectralParameter3Tag:
  {
      if (structSig==icSigBRDFStruct)
        return true;
      else
        return false;
    }

    // Matrix column tags - XYZ types
  case icSigBlueMatrixColumnTag:
  case icSigGreenMatrixColumnTag:
  case icSigRedMatrixColumnTag:
  case icSigLuminanceTag:
  case icSigMediaWhitePointTag:
  case icSigMediaBlackPointTag:
    {
      if (typeSig!=icSigXYZType) {
        return false;
      }
      else return true;
    }

    // TRC tags
  case icSigBlueTRCTag:
  case icSigGreenTRCTag:
  case icSigRedTRCTag:
  case icSigGrayTRCTag:
    {
      switch(typeSig) {
      case icSigCurveType:
      case icSigParametricCurveType:
        return true;

      default:
        return false;
      }
    }

  case icSigCalibrationDateTimeTag:
    {
      if (typeSig!=icSigDateTimeType)
        return false;
      else return true;
    }

  case icSigCharTargetTag:
    {
      if (typeSig!=icSigTextType)
        return false;
      else
        return true;
    }

  case icSigChromaticAdaptationTag:
    {
      if (typeSig!=icSigS15Fixed16ArrayType)
        return false;
      else return true;
    }

  case icSigChromaticityTag:
    {
      if (typeSig!=icSigChromaticityType)
        return false;
      else return true;
    }

  case icSigCicpTag:
    {
      if (typeSig != icSigCicpType)
        return false;
      else if (m_Header.version < icVersionNumberV4_4 || m_Header.version==icVersionNumberV5)
        return false;

      return true;
    }

  case icSigColorantInfoTag:
  case icSigColorantInfoOutTag:
  {
    //if (arraySig == icSigColorantInfoArray)
    //  return true;
    //else return true;
    return true;
  }

  case icSigColorantOrderTag:
  case icSigColorantOrderOutTag:
    {
      if (typeSig!=icSigColorantOrderType)
        return false;
      else return true;
    }

  case icSigColorantTableTag:
  case icSigColorantTableOutTag:
    {
      if (typeSig!=icSigColorantTableType)
        return false;
      else return true;
    }

    // Multi-localized Unicode type tags
  case icSigCopyrightTag:
    {
      if (m_Header.version>=icVersionNumberV4) {
        if (typeSig!=icSigMultiLocalizedUnicodeType)
          return false;
        else return true;
      }
      else {
        if (typeSig!=icSigTextType)
          return false;
        else return true;
      }
    }

  case icSigCxFTag:
    {
      if (m_Header.version >= icVersionNumberV5) {
        if (typeSig != icSigUtf8TextType &&
            typeSig != icSigZipUtf8TextType)
          return false;
        return true;
      }
      else {
        if (typeSig != icSigUtf8TextType &&
            typeSig != icSigZipUtf8TextType &&
#if defined(XRITE_ADDITIONS)
            typeSig != icSigZipXmlType_XRITE &&
#endif
            typeSig != icSigZipXmlType)
          return false;
        return true;
      }
    }

  case icSigColorSpaceNameTag:
    {
      if (typeSig!=icSigUtf8TextType)
        return false;
      return true;	  
    }

  case icSigReferenceNameTag:
    {
      if (typeSig!=icSigUtf8TextType)
        return false;
      return true;	  	  
    }

  case icSigColorEncodingParamsTag:
    {
      if (m_Header.version >= icVersionNumberV5 &&
          structSig==icSigColorEncodingParamsSruct)
        return true;

      return false;
    }

  case icSigViewingCondDescTag:
  case icSigDeviceMfgDescTag:
  case icSigDeviceModelDescTag:
  case icSigProfileDescriptionTag:
    {
      if (m_Header.version>=icVersionNumberV4) {
        if (typeSig!=icSigMultiLocalizedUnicodeType)
          return false;
        else return true;
      }
      else {
        if (typeSig!=icSigTextDescriptionType)
          return false;
        else return true;
      }
    }

  case icSigMeasurementTag:
    {
      if (typeSig!=icSigMeasurementType)
        return false;
      else return true;
    }

  case icSigNamedColor2Tag:
    {
      if (typeSig==icSigNamedColor2Type)
        return true;
      if (m_Header.version >= icVersionNumberV5 &&
          arraySig==icSigNamedColorArray)
        return true;

      return false;
    }

  case icSigOutputResponseTag:
    {
      if (typeSig!=icSigResponseCurveSet16Type)
        return false;
      else return true;
    }

  case icSigProfileSequenceDescTag:
    {
      if (typeSig!=icSigProfileSequenceDescType)
        return false;
      else return true;
    }

  case icSigTechnologyTag:
  case icSigPerceptualRenderingIntentGamutTag:
  case icSigSaturationRenderingIntentGamutTag:
    {
      if (typeSig!=icSigSignatureType)
        return false;
      else return true;
    }

  case icSigViewingConditionsTag:
    {
      if (typeSig!=icSigViewingConditionsType)
        return false;
      else return true;
    }

  case icSigMaterialTypeArrayTag:
    {
      if (typeSig!=icSigTagArrayType || 
          arraySig!=icSigUtf8TextTypeArray)
        return false;
      else
        return true;
    }

  case icSigMaterialDefaultValuesTag:
    {
      if (typeSig!=icSigUInt8ArrayType &&
          typeSig!=icSigUInt16ArrayType &&
          typeSig!=icSigFloat16ArrayType &&
          typeSig!=icSigFloat32ArrayType)
        return false;
      else
        return true;
    }

  case icSigSurfaceMapTag:
  {
    if (typeSig != icSigEmbeddedHeightImageType && typeSig != icSigEmbeddedNormalImageType)
      return false;
    else return true;
  }

  //The Private Tag case
  default:
    {
      return true;
    }
  }
}


/**
 ****************************************************************************
 * Name: CIccProfile::CheckRequiredTags
 * 
 * Purpose: Check if the Profile has the required tags 
 *  for the specified Profile/Device class.
 * 
 * Return: 
 *  icValidateOK if valid, or other error status.
 *****************************************************************************
 */
icValidateStatus CIccProfile::CheckRequiredTags(std::string &sReport, const CIccProfile *pParentProfile) const
{
  if (m_Tags.size() <= 0) {
    sReport += icMsgValidateCriticalError;
    sReport += "No tags present.\n";
    return icValidateCriticalError;
  }

  icValidateStatus rv = icValidateOK;

  icProfileClassSignature sig = m_Header.deviceClass;

  if (m_Header.deviceClass==icSigColorEncodingClass) {
    if (!GetTag(icSigReferenceNameTag)) {
      sReport += icMsgValidateNonCompliant;
      sReport += "Required tags missing.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    return rv;
  }
  else {
    if (!pParentProfile && (!GetTag(icSigProfileDescriptionTag, pParentProfile) ||
        !GetTag(icSigCopyrightTag, pParentProfile))) {
         sReport += icMsgValidateNonCompliant;
         sReport += "Required tags missing.\n";
         rv = icMaxStatus(rv, icValidateNonCompliant);
    }

    if (sig != icSigLinkClass && sig != icSigMaterialIdentificationClass && sig != icSigMaterialLinkClass) {
      if ((m_Header.version<icVersionNumberV5 || m_Header.pcs != 0) && !GetTag(icSigMediaWhitePointTag, pParentProfile)) {
        sReport += icMsgValidateCriticalError;
        sReport += "Media white point tag missing.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
      }
    }
  }

  if (m_Header.version<icVersionNumberV5) {
    switch(sig) {
      case icSigInputClass:
        if (m_Header.colorSpace == icSigGrayData) {
          if (!GetTag(icSigGrayTRCTag)) {
            sReport += icMsgValidateCriticalError;
            sReport += "Gray TRC tag missing.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
        else {
          if (!GetTag(icSigAToB0Tag)) {
            if (!GetTag(icSigRedMatrixColumnTag) || !GetTag(icSigGreenMatrixColumnTag) ||
               !GetTag(icSigBlueMatrixColumnTag) || !GetTag(icSigRedTRCTag) ||
               !GetTag(icSigGreenTRCTag) || !GetTag(icSigBlueTRCTag)) {
                 sReport += icMsgValidateCriticalError;
                 sReport += "Critical tag(s) missing.\n";
                 rv = icMaxStatus(rv, icValidateCriticalError);
               }
          }
        }
        break;

      case icSigDisplayClass:
        if (m_Header.colorSpace == icSigGrayData) {
          if (!GetTag(icSigGrayTRCTag)) {
            sReport += icMsgValidateCriticalError;
            sReport += "Gray TRC tag missing.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
        else {
          if (!GetTag(icSigAToB0Tag) || !GetTag(icSigBToA0Tag)) {
            if (!GetTag(icSigRedMatrixColumnTag) || !GetTag(icSigGreenMatrixColumnTag) ||
               !GetTag(icSigBlueMatrixColumnTag) || !GetTag(icSigRedTRCTag) ||
               !GetTag(icSigGreenTRCTag) || !GetTag(icSigBlueTRCTag)) {
                 sReport += icMsgValidateCriticalError;
                 sReport += "Critical tag(s) missing.\n";
                 rv = icMaxStatus(rv, icValidateCriticalError);
               }
          }
        }
        break;

      case icSigOutputClass:
        if (m_Header.colorSpace == icSigGrayData) {
          if (!GetTag(icSigGrayTRCTag)) {
            sReport += icMsgValidateCriticalError;
            sReport += "Gray TRC tag missing.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
        else {
          if (!GetTag(icSigAToB0Tag) || !GetTag(icSigBToA0Tag) ||
             !GetTag(icSigAToB1Tag) || !GetTag(icSigBToA1Tag) ||
             !GetTag(icSigAToB2Tag) || !GetTag(icSigBToA2Tag)) {
               sReport += icMsgValidateCriticalError;
               sReport += "Critical tag(s) missing.\n";
               rv = icMaxStatus(rv, icValidateCriticalError);
             }

          if (!GetTag(icSigGamutTag) && m_Header.version < icVersionNumberV5) {
            sReport += icMsgValidateNonCompliant;
            sReport += "Gamut tag missing.\n";
            rv = icMaxStatus(rv, icValidateNonCompliant);
          }

          if (m_Header.version >= icVersionNumberV4) {
            switch (m_Header.colorSpace) {
              case icSig2colorData:
              case icSig3colorData:
              case icSig4colorData:
              case icSig5colorData:
              case icSig6colorData:
              case icSig7colorData:
              case icSig8colorData:
              case icSig9colorData:
              case icSig10colorData:
              case icSig11colorData:
              case icSig12colorData:
              case icSig13colorData:
              case icSig14colorData:
              case icSig15colorData:
                if (!GetTag(icSigColorantTableTag)) {
                  sReport += icMsgValidateNonCompliant;
                  sReport += "xCLR output profile is missing colorantTableTag\n";
                  rv = icMaxStatus(rv, icValidateNonCompliant);
                }
                break;

              default:
                break;
            }
          }
        }
        break;

      case icSigLinkClass:
        if (!GetTag(icSigAToB0Tag) || !GetTag(icSigProfileSequenceDescTag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }

        if (icIsSpaceCLR(m_Header.colorSpace)) {
          if (!GetTag(icSigColorantTableTag)) {
            sReport += icMsgValidateNonCompliant;
            sReport += "Required tag(s) missing.\n";
            rv = icMaxStatus(rv, icValidateNonCompliant);
          }
        }

        if (icIsSpaceCLR(m_Header.pcs)) {
          if (!GetTag(icSigColorantTableOutTag)) {
            sReport += icMsgValidateNonCompliant;
            sReport += "Required tag(s) missing.\n";
            rv = icMaxStatus(rv, icValidateNonCompliant);
          }
        }
        break;

      case icSigColorSpaceClass:
        if (!GetTag(icSigAToB0Tag) || !GetTag(icSigBToA0Tag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }
        break;

      case icSigAbstractClass:
        if (!GetTag(icSigAToB0Tag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }
        break;

      case icSigNamedColorClass:
        if (!GetTag(icSigNamedColor2Tag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }

        break;

      default:
        sReport += icMsgValidateCriticalError;
        sReport += "Unknown Profile Class.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
        break;
    }
  }
  else {
    switch(sig) {
      case icSigInputClass:
        if (m_Header.spectralPCS) {
          //????
        }
        if (m_Header.pcs) {
          if (m_Header.colorSpace == icSigGrayData) {
            if (!GetTag(icSigAToB0Tag) && !GetTag(icSigAToB1Tag) && !GetTag(icSigAToB3Tag) && !GetTag(icSigGrayTRCTag)) {
              sReport += icMsgValidateCriticalError;
              sReport += "Critical tag(s) missing.\n";
              rv = icMaxStatus(rv, icValidateCriticalError);
            }
          }
          else {
            if (!GetTag(icSigAToB0Tag) && !GetTag(icSigAToB1Tag) && !GetTag(icSigAToB3Tag)) {
              if (!GetTag(icSigRedMatrixColumnTag) || !GetTag(icSigGreenMatrixColumnTag) ||
                !GetTag(icSigBlueMatrixColumnTag) || !GetTag(icSigRedTRCTag) ||
                !GetTag(icSigGreenTRCTag) || !GetTag(icSigBlueTRCTag)) {
                  sReport += icMsgValidateCriticalError;
                  sReport += "Critical tag(s) missing.\n";
                  rv = icMaxStatus(rv, icValidateCriticalError);
              }
            }
          }
        }
        break;

      case icSigDisplayClass:
        if (m_Header.spectralPCS) {
          //????
        }
        if (m_Header.pcs) {
          if (m_Header.colorSpace == icSigGrayData) {
            if (!GetTag(icSigAToB0Tag) && !GetTag(icSigAToB1Tag) && !GetTag(icSigAToB3Tag) && !GetTag(icSigGrayTRCTag)) {
              sReport += icMsgValidateCriticalError;
              sReport += "Critical tag(s) missing.\n";
              rv = icMaxStatus(rv, icValidateCriticalError);
            }
          }
          else {
            if ((!GetTag(icSigAToB0Tag) && !GetTag(icSigAToB1Tag) && !GetTag(icSigAToB3Tag)) /*|| 
                (!GetTag(icSigBToA0Tag) && !GetTag(icSigBToA1Tag) && !GetTag(icSigBToA3Tag))*/) {
              if (!GetTag(icSigRedMatrixColumnTag) || !GetTag(icSigGreenMatrixColumnTag) ||
                !GetTag(icSigBlueMatrixColumnTag) || !GetTag(icSigRedTRCTag) ||
                !GetTag(icSigGreenTRCTag) || !GetTag(icSigBlueTRCTag)) {
                  sReport += icMsgValidateCriticalError;
                  sReport += "Critical tag(s) missing.\n";
                  rv = icMaxStatus(rv, icValidateCriticalError);
              }
            }
          }
        }
        break;

      case icSigOutputClass:
        if (m_Header.spectralPCS) {
          //????
        }
        if (m_Header.pcs) {
          if (m_Header.colorSpace == icSigGrayData) {
            if (!GetTag(icSigAToB0Tag) && !GetTag(icSigBToA0Tag) &&
                !GetTag(icSigAToB1Tag) && !GetTag(icSigBToA1Tag) &&
                !GetTag(icSigAToB3Tag) && !GetTag(icSigBToA3Tag) && !GetTag(icSigGrayTRCTag)) {
              sReport += icMsgValidateCriticalError;
              sReport += "Critical tag(s) missing.\n";
              rv = icMaxStatus(rv, icValidateCriticalError);
            }
          }
          else {
            if (!GetTag(icSigAToB0Tag) && !GetTag(icSigBToA0Tag) &&
                !GetTag(icSigAToB1Tag) && !GetTag(icSigBToA1Tag) &&
                !GetTag(icSigAToB3Tag) && !GetTag(icSigBToA3Tag)) {
                sReport += icMsgValidateCriticalError;
                sReport += "Critical tag(s) missing.\n";
                rv = icMaxStatus(rv, icValidateCriticalError);
            }

            if (!GetTag(icSigGamutTag) && m_Header.version < icVersionNumberV5) {
              sReport += icMsgValidateNonCompliant;
              sReport += "Gamut tag missing.\n";
              rv = icMaxStatus(rv, icValidateNonCompliant);
            }

            if (m_Header.version >= 0x04000000L) {
              switch (m_Header.colorSpace) {
              case icSig2colorData:
              case icSig3colorData:
              case icSig4colorData:
              case icSig5colorData:
              case icSig6colorData:
              case icSig7colorData:
              case icSig8colorData:
              case icSig9colorData:
              case icSig10colorData:
              case icSig11colorData:
              case icSig12colorData:
              case icSig13colorData:
              case icSig14colorData:
              case icSig15colorData:
                if (!GetTag(icSigColorantTableTag)) {
                  sReport += icMsgValidateNonCompliant;
                  sReport += "xCLR output profile is missing colorantTableTag\n";
                  rv = icMaxStatus(rv, icValidateNonCompliant);
                }
                break;

              default:
                break;
              }
            }
          }
        }
        break;

      case icSigLinkClass:
        if (!GetTag(icSigAToB0Tag)){
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }

        if (icIsSpaceCLR(m_Header.colorSpace)) {
          if (!GetTag(icSigColorantTableTag)) {
            sReport += icMsgValidateNonCompliant;
            sReport += "Required tag(s) missing.\n";
            rv = icMaxStatus(rv, icValidateNonCompliant);
          }
        }

        if (icIsSpaceCLR(m_Header.pcs)) {
          if (!GetTag(icSigColorantTableOutTag)) {
            sReport += icMsgValidateNonCompliant;
            sReport += "Required tag(s) missing.\n";
            rv = icMaxStatus(rv, icValidateNonCompliant);
          }
        }
        break;

      case icSigColorSpaceClass:
        if (m_Header.spectralPCS) {
          //????
        }
        if (m_Header.pcs) {
          if ((!GetTag(icSigAToB0Tag) && !GetTag(icSigAToB1Tag) && !GetTag(icSigAToB3Tag)) || 
              (!GetTag(icSigBToA0Tag) && !GetTag(icSigBToA1Tag) && !GetTag(icSigBToA3Tag))) {
            sReport += icMsgValidateCriticalError;
            sReport += "Critical tag(s) missing.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
        break;

      case icSigAbstractClass:
        if (m_Header.spectralPCS) {
          //????
        }
        if (m_Header.pcs) {
          if (!GetTag(icSigAToB0Tag) && !GetTag(icSigAToB3Tag)) {
            sReport += icMsgValidateCriticalError;
            sReport += "Critical tag(s) missing.\n";
            rv = icMaxStatus(rv, icValidateCriticalError);
          }
        }
        break;

      case icSigNamedColorClass:
        if (!GetTag(icSigNamedColor2Tag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }
        break;

      case icSigMaterialIdentificationClass:
        if (!GetTag(icSigAToM0Tag) && !GetTag(icSigMaterialTypeArrayTag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }
        break;

      case icSigMaterialVisualizationClass:
        if (!GetTag(icSigMToB0Tag) && !GetTag(icSigMToS0Tag)&& !GetTag(icSigMaterialTypeArrayTag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }
        break;

      case icSigMaterialLinkClass:
        if (!GetTag(icSigMToA0Tag)&& !GetTag(icSigMaterialTypeArrayTag)) {
          sReport += icMsgValidateCriticalError;
          sReport += "Critical tag(s) missing.\n";
          rv = icMaxStatus(rv, icValidateCriticalError);
        }
        break;

      default:
        sReport += icMsgValidateCriticalError;
        sReport += "Unknown Profile Class.\n";
        rv = icMaxStatus(rv, icValidateCriticalError);
        break;
    }

  }

  if (!CheckTagExclusion(sReport)) {
    rv = icMaxStatus(rv, icValidateWarning);
  }

  return rv;
}

/**
 *****************************************************************************
 * Name: CIccProfile::CheckFileSize()
 * 
 * Purpose: Check if the Profile file size matches with the size mentioned
 *  in the header and is evenly divisible by four.
 * 
 * Args: 
 * 
 * Return: 
 *  true - size matches,
 *  false - size mismatches
 ******************************************************************************
 */
bool CIccProfile::CheckFileSize(CIccIO *pIO) const
{
  size_t FileSize;
  size_t curPos = pIO->Tell();

  if (!pIO->Seek(0, icSeekEnd))
    return false;

  FileSize = pIO->Tell();

  if (!FileSize)
    return false;

  if (!pIO->Seek(curPos, icSeekSet))
    return false;

  if (FileSize != m_Header.size)
    return false;

  if ((m_Header.version>=icVersionNumberV4_2) && ((FileSize%4 != 0) || (m_Header.size%4 != 0)))
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccProfile::Validate
 * 
 * Purpose: Check the data integrity of the profile, and conformance to
 *  the ICC specification
 * 
 * Args:
 *  sReport = String to put report into
 * 
 * Return: 
 *  icValidateOK if profile is valid, warning/error level otherwise
 *****************************************************************************
 */
icValidateStatus CIccProfile::Validate(std::string &sReport, std::string sSigPath/*=""*/, const CIccProfile *pParentProfile) const
{
  icValidateStatus rv = icValidateOK;

  //Check header
  rv = icMaxStatus(rv, CheckHeader(sReport));

  // Check for duplicate tags
  if (!AreTagsUnique()) {
    sReport += icMsgValidateWarning;
    sReport += " - There are duplicate tags.\n";
    rv =icMaxStatus(rv, icValidateWarning);
  }

  // Check Required Tags which includes exclusion tests
  rv = icMaxStatus(rv, CheckRequiredTags(sReport, pParentProfile));

  // Per Tag tests
  rv = icMaxStatus(rv, CheckTagTypes(sReport));
  TagEntryList::const_iterator i;
  for (i=m_Tags.begin(); i!=m_Tags.end(); i++) {
    if (i->pTag)        // should we give an error if pTag is NULL/Not loaded?
      rv = icMaxStatus(rv, i->pTag->Validate(sSigPath + icGetSigPath(i->TagInfo.sig), sReport, this));
  }

  return rv;
}

/**
 ****************************************************************************
 * Name: CIccProfile::GetSpaceSamples
 * 
 * Purpose: Get the number of device channels from the data color space
 * 
 * Return: Number of device channels.
 *  
 *****************************************************************************
 */
icUInt16Number CIccProfile::GetSpaceSamples() const
{
  return (icUInt16Number)icGetSpaceSamples(m_Header.colorSpace);
}


/**
 ****************************************************************************
 * Name: CIccProfile::GetParentSpaceSamples
 *
 * Purpose: Get the number of device channels from the parent's data color space
 *
 * Return: Number of parent's device channels.
 *
 *****************************************************************************
 */
icUInt16Number CIccProfile::GetParentSpaceSamples() const
{
  return (icUInt16Number)icGetSpaceSamples(m_parentColorSpace);
}


/**
 ****************************************************************************
 * Name: CIccProfile::getPccIlluminant
 * 
 * Purpose: Get the Illuminant associated with profile connection.
 * 
 * Return: The illuminant type associated with profile connection.
 *  
 *****************************************************************************
 */
icIlluminant CIccProfile::getPccIlluminant()
{
  if (m_Header.version<icVersionNumberV5)
    return icIlluminantD50;

  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();

  if (!pCond) {
    if (icIsIllumD50(m_Header.illuminant))
      return icIlluminantD50;
    else
      return icIlluminantUnknown;
  }

  return pCond->getStdIllumiant();
}



/**
 ****************************************************************************
 * Name: CIccProfile::getPccCCT
 * 
 * Purpose: Get the Correlated Color Temperature associated with profile connection.
 * 
 * Return: The Correlated Color Temperature associated with profile connection.
 *  
 *****************************************************************************
 */
icFloatNumber CIccProfile::getPccCCT()
{
  if (m_Header.version<icVersionNumberV5)
    return 5000.f;

  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();

  if (!pCond) {
    if (icIsIllumD50(m_Header.illuminant))
      return 5000.0f;
    else
      return 0.0;
  }

  return pCond->getIlluminantCCT();
}


/**
 ****************************************************************************
 * Name: CIccProfile::getPccObserver
 * 
 * Purpose: Get the observer type associated with profile connection.
 * 
 * Return: The observer type associated with profile connection.
 *  
 *****************************************************************************
 */
icStandardObserver CIccProfile::getPccObserver()
{
  if (m_Header.version<icVersionNumberV5)
    return icStdObs1931TwoDegrees;

  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond) {
    if (icIsIllumD50(m_Header.illuminant))
      return icStdObs1931TwoDegrees;
    else
      return icStdObsUnknown;
  }

  return pCond->getStdObserver();
}

/**
 ****************************************************************************
 * Name: CIccProfile::getNormIlluminantXYZ
 * 
 * Purpose: Get the Normalized XYZ coordinates for the illuminant and observer
 *  associated with profile connection.
 * 
 *****************************************************************************
 */
void CIccProfile::getNormIlluminantXYZ(icFloatNumber *pXYZ)
{
  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond) {
    if (m_Header.version < icVersionNumberV5) {
      memcpy(pXYZ, icD50XYZ, 3*sizeof(icFloatNumber));
    }
    else {
      pXYZ[0] = icFtoD(m_Header.illuminant.X);
      pXYZ[1] = icFtoD(m_Header.illuminant.Y);
      pXYZ[2] = icFtoD(m_Header.illuminant.Z);
    }
  }
  else {
    if (pCond->isStandardPcc()) {
      memcpy(pXYZ, icD50XYZ, 3 * sizeof(icFloatNumber));
    }
    else {
      pXYZ[0] = pCond->m_illuminantXYZ.X / pCond->m_illuminantXYZ.Y;
      pXYZ[1] = 1.0f;
      pXYZ[2] = pCond->m_illuminantXYZ.Z / pCond->m_illuminantXYZ.Y;
    }
  }
}

/**
****************************************************************************
* Name: CIccProfile::getLumIlluminantXYZ
*
* Purpose: Get the XYZ Luminance coordinates (in cd/m^2 units) for the
* illuminant and observer associated with profile connection.
*
*****************************************************************************
*/
void CIccProfile::getLumIlluminantXYZ(icFloatNumber *pXYZ)
{
  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond) { 
    const CIccTagXYZ *pXYZTag = (CIccTagXYZ*) FindTagOfType(icSigLuminanceTag, icSigXYZType);
    if (pXYZTag) {
      icFloatNumber Y = icFtoD((*pXYZTag)[0].Y);

      pXYZ[0] = icFtoD(m_Header.illuminant.X) * Y;
      pXYZ[1] = icFtoD(m_Header.illuminant.Y) * Y;
      pXYZ[2] = icFtoD(m_Header.illuminant.Z) * Y;
    }
    else { // No Luminance Tag so just use default luminance
      pXYZ[0] = icFtoD(m_Header.illuminant.X) * icDefaultLuminance;
      pXYZ[1] = icFtoD(m_Header.illuminant.Y) * icDefaultLuminance;
      pXYZ[2] = icFtoD(m_Header.illuminant.Z) * icDefaultLuminance;
    }
  }
  else {
    pXYZ[0] = pCond->m_illuminantXYZ.X;
    pXYZ[1] = pCond->m_illuminantXYZ.Y;
    pXYZ[2] = pCond->m_illuminantXYZ.Z;
  }
}

/**
 ****************************************************************************
 * Name: CIccProfile::getMediaWhiteXYZ
 * 
 * Purpose: Get the Media White point XYZ coordinates for the illuminant and observer
 *  associated with observing profile connection conditions.
 * 
 *****************************************************************************
 */
bool CIccProfile::getMediaWhiteXYZ(icFloatNumber *pXYZ)
{
  CIccTag *pTag = FindTag(icSigMediaWhitePointTag);
  if (pTag && pTag->GetType()==icSigXYZType) {
    CIccTagXYZ *pXYZTag = (CIccTagXYZ*)pTag;
    icXYZNumber *pMediaXYZ = pXYZTag->GetXYZ(0);
    pXYZ[0] = icFtoD(pMediaXYZ->X);
    pXYZ[1] = icFtoD(pMediaXYZ->Y);
    pXYZ[2] = icFtoD(pMediaXYZ->Z);

    return true;
  }
  else { //No media white point so use illuminant
    pXYZ[0] = icFtoD(m_Header.illuminant.X);
    pXYZ[1] = icFtoD(m_Header.illuminant.Y);
    pXYZ[2] = icFtoD(m_Header.illuminant.Z);

    return false;
  }
}

/**
 ****************************************************************************
 * Name: CIccProfile::calcNormIlluminantXYZ
 * 
 * Purpose: Get the  XYZ Luminance coordinates (in cd/m^2 units) for the
 * illuminant and observer associated with profile connection.
 *
 * This assumes that the illuminant is not normalized and values for each
 * wavelength are expressed in terms of (cd/m^2)
 * 
 *****************************************************************************
 */
bool CIccProfile::calcLumIlluminantXYZ(icFloatNumber *pXYZ, IIccProfileConnectionConditions * /* pObservingPCC */)
{
  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();

  if (pCond) {
    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pCond->getIlluminant(illuminantRange);

    CIccMatrixMath *obs = pCond->getObserverMatrix(illuminantRange);

    if (!obs) {
      pXYZ[0] = icFtoD(m_Header.illuminant.X) * icDefaultLuminance;
      pXYZ[1] = icFtoD(m_Header.illuminant.Y) * icDefaultLuminance;
      pXYZ[2] = icFtoD(m_Header.illuminant.Z) * icDefaultLuminance;

      return false;
    }

    obs->VectorScale(illuminant);

    pXYZ[0] = 683.0f * obs->RowSum(0);
    pXYZ[1] = 683.0f * obs->RowSum(1);
    pXYZ[2] = 683.0f * obs->RowSum(2);

    delete obs;

    return true;
  }
  else {
    pXYZ[0] = icFtoD(m_Header.illuminant.X) * icDefaultLuminance;
    pXYZ[1] = icFtoD(m_Header.illuminant.Y) * icDefaultLuminance;
    pXYZ[2] = icFtoD(m_Header.illuminant.Z) * icDefaultLuminance;

    return false;
  }
}

/**
****************************************************************************
* Name: CIccProfile::calcLumIlluminantXYZ
*
* Purpose: Get the Normalized XYZ coordinates for the illuminant and observer
*  associated with profile connection.
*
*****************************************************************************
*/
bool CIccProfile::calcNormIlluminantXYZ(icFloatNumber *pXYZ, IIccProfileConnectionConditions * /* pObservingPCC */)
{
  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();

  if (pCond) {
    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pCond->getIlluminant(illuminantRange);

    CIccMatrixMath *obs = pCond->getObserverMatrix(illuminantRange);

    if (!obs) {
      pXYZ[0] = icFtoD(m_Header.illuminant.X);
      pXYZ[1] = icFtoD(m_Header.illuminant.Y);
      pXYZ[2] = icFtoD(m_Header.illuminant.Z);

      return false;
    }

    obs->VectorScale(illuminant);
    obs->Scale(obs->RowSum(1));

    pXYZ[0] = obs->RowSum(0);
    pXYZ[1] = obs->RowSum(1);
    pXYZ[2] = obs->RowSum(2);

    delete obs;

    return true;
  }
  else {
    pXYZ[0] = icFtoD(m_Header.illuminant.X);
    pXYZ[1] = icFtoD(m_Header.illuminant.Y);
    pXYZ[2] = icFtoD(m_Header.illuminant.Z);

    return false;
  }
}

/**
 ****************************************************************************
 * Name: CIccProfile::calcMediaWhiteXYZ
 * 
 * Purpose: Get the Media White point XYZ coordinates for the illuminant and observer
 *  associated with observing profile connection conditions.
 * 
 *****************************************************************************
 */
bool CIccProfile::calcMediaWhiteXYZ(icFloatNumber *pXYZ, IIccProfileConnectionConditions *pObservingPCC)
{
  const CIccTagSpectralViewingConditions *pView = pObservingPCC->getPccViewingConditions();
  CIccTag *pTag;
  bool rv = true;

  pTag = FindTag(icSigSpectralWhitePointTag);

  if (!pView || !pTag) { //Use profile's definition of Media White Point
getmediaXYZ:
    pTag = FindTag(icSigMediaWhitePointTag);
    if (pTag && pTag->GetType()==icSigXYZType) {
      CIccTagXYZ *pXYZTag = (CIccTagXYZ*)pTag;
      icXYZNumber *pMediaXYZ = pXYZTag->GetXYZ(0);
      pXYZ[0] = icFtoD(pMediaXYZ->X);
      pXYZ[1] = icFtoD(pMediaXYZ->Y);
      pXYZ[2] = icFtoD(pMediaXYZ->Z);
      return rv;
    }
    else { //No media white point so use illuminant
      pXYZ[0] = icFtoD(m_Header.illuminant.X);
      pXYZ[1] = icFtoD(m_Header.illuminant.Y);
      pXYZ[2] = icFtoD(m_Header.illuminant.Z);
      return false;
    }
  }
  else if (pTag->IsNumArrayType()) { //pView contains observer and illuminant && spectral white point is available
    CIccTagNumArray *pNumTag = (CIccTagNumArray*)pTag;
    icSignature sig =m_Header.spectralPCS;
    icSpectralRange range = m_Header.spectralRange;
    icSpectralRange birange = m_Header.biSpectralRange;

    rv = false;

    if (!sig) {
      pTag = FindTag(icSigSpectralDataInfoTag);
      if (pTag && pTag->GetType()==icSigSpectralDataInfoType) {
        CIccTagSpectralDataInfo *pInfo = (CIccTagSpectralDataInfo *)pTag;
        sig = pInfo->m_nSig;
        range = pInfo->m_spectralRange;
        birange = pInfo->m_biSpectralRange;
      }
    }

    icUInt32Number samples = icGetSpaceSamples((icColorSpaceSignature)sig);
    icFloatNumber *pWhite;

    if (samples && pNumTag->GetNumValues()>=samples) {
      pWhite=new icFloatNumber[samples];
      if (pWhite) {
        pNumTag->GetValues(pWhite);
      }
      else {
        goto getmediaXYZ;
      }
    }
    else {
      goto getmediaXYZ;
    }

    if (icIsSameColorSpaceType(sig, icSigReflectanceSpectralData)) {
      CIccMatrixMath *pMtx = pObservingPCC->getReflectanceObserver(range);

      pMtx->VectorMult(pXYZ, pWhite);
      delete pMtx;
      delete [] pWhite;

      return true;
    }
    else if (icIsSameColorSpaceType(sig, icSigRadiantSpectralData)) {
      CIccMatrixMath obs(3,range.steps);
      pObservingPCC->getEmissiveObserver(range, pWhite, obs.entry(0));

      obs.VectorMult(pXYZ, pWhite);
      delete [] pWhite;

      return true;
    }
    else {
      delete [] pWhite;
      goto getmediaXYZ;

    }
  }
  else {
    goto getmediaXYZ;
  }
}


//////////////////////////////////////////////////////////////////////
//  Function Definitions
//////////////////////////////////////////////////////////////////////

/**
 *****************************************************************************
 * Name: ReadIccProfile
 * 
 * Purpose: Read an ICC profile file.
 * 
 * Args: 
 *  szFilename - zero terminated string with filename of ICC profile to read 
 *  bUseSubProfile - will attempt to open a subprofile if present
 *
 * Return: 
 *  Pointer to ICC profile object, or NULL on failure
 ******************************************************************************
 */
CIccProfile* ReadIccProfile(const icChar *szFilename, bool bUseSubProfile/*=false*/)
{
  CIccFileIO *pFileIO = new CIccFileIO;

  if (!pFileIO->Open(szFilename, "rb")) {
    delete pFileIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc->Read(pFileIO, bUseSubProfile)) {
    delete pIcc;
    delete pFileIO;
    return NULL;
  }
  delete pFileIO;

  return pIcc;
}


#ifdef WIN32
/**
*****************************************************************************
* Name: ReadIccProfile
* 
* Purpose: Read an ICC profile file.
* 
* Args: 
*  szFilename - zero terminated string with filename of ICC profile to read 
*  bUseSubProfile - will attempt to open a subprofile if present
*
* Return: 
*  Pointer to icc profile object, or NULL on failure
******************************************************************************
*/
CIccProfile* ReadIccProfile(const icWChar *szFilename, bool bUseSubProfile/*=false*/)
{
  CIccFileIO *pFileIO = new CIccFileIO;

  if (!pFileIO->Open(szFilename, L"rb")) {
    delete pFileIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc->Read(pFileIO)) {
    delete pIcc;
    delete pFileIO;
    return NULL;
  }
  delete pFileIO;

  return pIcc;
}

#endif

/**
*****************************************************************************
* Name: ReadIccProfile
* 
* Purpose: Read an ICC profile file.
* 
* Args: 
*  pMem = pointer to memory containing profile data
*  nSize = size of memory related to profile
*  bUseSubProfile - will attempt to open a subprofile if present
*
* Return: 
*  Pointer to icc profile object, or NULL on failure
******************************************************************************
*/
CIccProfile* ReadIccProfile(const icUInt8Number *pMem, icUInt32Number nSize, bool /* bUseSubProfile =false*/)
{
  CIccMemIO *pMemIO = new CIccMemIO();

  if (!pMemIO->Attach((icUInt8Number*)pMem, nSize)) {
    delete pMemIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc->Read(pMemIO)) {
    delete pIcc;
    delete pMemIO;
    return NULL;
  }
  delete pMemIO;

  return pIcc;
}


/**
 ******************************************************************************
 * Name: OpenIccProfile
 * 
 * Purpose: Open an ICC profile file.  This will only read the profile header
 *  and tag directory.  Loading of actual tags will be deferred until the
 *  tags are actually referenced by FindTag().
 * 
 * Args: 
 *  szFilename - zero terminated string with filename of ICC profile to read
 *  bUseSubProfile - will attempt to open a subprofile if present
 * 
 * Return: 
 *  Pointer to icc profile object, or NULL on failure
 *******************************************************************************
 */
CIccProfile* OpenIccProfile(const icChar *szFilename, bool bUseSubProfile/*=false*/)
{
  CIccFileIO *pFileIO = new CIccFileIO;

  if (!pFileIO->Open(szFilename, "rb")) {
    delete pFileIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc->Attach(pFileIO, bUseSubProfile)) {
    delete pIcc;
    delete pFileIO;
    return NULL;
  }

  return pIcc;
}

#ifdef WIN32
/**
******************************************************************************
* Name: OpenIccProfile
* 
* Purpose: Open an ICC profile file.  This will only read the profile header
*  and tag directory.  Loading of actual tags will be deferred until the
*  tags are actually referenced by FindTag().
* 
* Args: 
*  szFilename - zero terminated string with filename of ICC profile to read 
*  bUseSubProfile - will attempt to open a subprofile if present
*
* Return: 
*  Pointer to icc profile object, or NULL on failure
*******************************************************************************
*/
CIccProfile* OpenIccProfile(const icWChar *szFilename, bool bUseSubProfile/*=false*/)
{
  CIccFileIO *pFileIO = new CIccFileIO;

  if (!pFileIO->Open(szFilename, L"rb")) {
    delete pFileIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc->Attach(pFileIO, bUseSubProfile)) {
    delete pIcc;
    delete pFileIO;
    return NULL;
  }

  return pIcc;
}
#endif

/**
******************************************************************************
* Name: OpenIccProfile
* 
* Purpose: Open an ICC profile file.  This will only read the profile header
*  and tag directory.  Loading of actual tags will be deferred until the
*  tags are actually referenced by FindTag().
* 
* Args: 
*  pMem = pointer to memory containing profile data
*  nSize = size of memory related to profile
*  bUseSubProfile = flag to indicate that to use embedded subprofile
* 
* Return: 
*  Pointer to icc profile object, or NULL on failure
*******************************************************************************
*/
CIccProfile* OpenIccProfile(const icUInt8Number *pMem, icUInt32Number nSize, bool bUseSubProfile/*=false*/)
{
  CIccMemIO *pMemIO = new CIccMemIO;

  if (!pMemIO->Attach((icUInt8Number*)pMem, nSize)) {
    delete pMemIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc->Attach(pMemIO, bUseSubProfile)) {
    delete pIcc;
    delete pMemIO;
    return NULL;
  }

  return pIcc;
}

/**
******************************************************************************
* Name: ValidateIccProfile
* 
* Purpose: Open an ICC profile file.  This will only read the profile header
*  and tag directory.  Loading of actual tags will be deferred until the
*  tags are actually referenced by FindTag().
* 
* Args: 
*  pIO - Handle to IO access object (Not ValidateIccProfile assumes ownership of this object)
*  sReport - std::string to put report into
*  nStatus - return status value
* 
* Return: 
*  Pointer to icc profile object, or NULL on failure
*******************************************************************************
*/
CIccProfile* ValidateIccProfile(CIccIO *pIO, std::string &sReport, icValidateStatus &nStatus)
{
  if (!pIO) {
    sReport = icMsgValidateCriticalError;
    sReport += " - ";
    sReport += "- Invalid I/O Handle\n";
    delete pIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc) {
    delete pIO;
    return NULL;
  }

  nStatus = pIcc->ReadValidate(pIO, sReport);

  if (nStatus>=icValidateCriticalError) {
    delete pIcc;
    delete pIO;
    return NULL;
  }

  delete pIO;

  nStatus = pIcc->Validate(sReport);

  return pIcc;
}

#ifdef WIN32
/**
******************************************************************************
* Name: ValidateIccProfile
* 
* Purpose: Open an ICC profile file.  This will only read the profile header
*  and tag directory.  Loading of actual tags will be deferred until the
*  tags are actually referenced by FindTag().
* 
* Args: 
*  szFilename - zero terminated string with filename of ICC profile to read 
*  sReport - std::string to put report into
*  nStatus - return status value
* 
* Return: 
*  Pointer to icc profile object, or NULL on failure
*******************************************************************************
*/
CIccProfile* ValidateIccProfile(const icWChar *szFilename, std::string &sReport, icValidateStatus &nStatus)
{
  CIccFileIO *pFileIO = new CIccFileIO;

  if (!pFileIO->Open(szFilename, L"rb")) {
    delete pFileIO;
    return NULL;
  }

  return ValidateIccProfile(pFileIO, sReport, nStatus);
}
#endif


/**
******************************************************************************
* Name: ValidateIccProfile
* 
* Purpose: Open an ICC profile file.  This will only read the profile header
*  and tag directory.  Loading of actual tags will be deferred until the
*  tags are actually referenced by FindTag().
* 
* Args: 
*  szFilename - zero terminated string with filename of ICC profile to read 
*  sReport - std::string to put report into
*  nStatus - return status value
* 
* Return: 
*  Pointer to icc profile object, or NULL on failure
*******************************************************************************
*/
CIccProfile* ValidateIccProfile(const icChar *szFilename, std::string &sReport, icValidateStatus &nStatus)
{
  CIccFileIO *pFileIO = new CIccFileIO;

  if (!pFileIO->Open(szFilename, "rb")) {
    sReport = icMsgValidateCriticalError;
    sReport += " - ";
    sReport += szFilename;
    sReport += "- Invalid Filename\n";
    delete pFileIO;
    return NULL;
  }

  CIccProfile *pIcc = new CIccProfile;

  if (!pIcc) {
    delete pFileIO;
    return NULL;
  }

  nStatus = pIcc->ReadValidate(pFileIO, sReport);

  if (nStatus>=icValidateCriticalError) {
    delete pIcc;
    delete pFileIO;
    return NULL;
  }

  delete pFileIO;

  nStatus = icMaxStatus(nStatus, pIcc->Validate(sReport));

  return pIcc;
}


/**
******************************************************************************
* Name: ValidateIccProfile
*
* Purpose: Open an ICC profile file.  This will only read the profile header
*  and tag directory.  Loading of actual tags will be deferred until the
*  tags are actually referenced by FindTag().
*
* Args:
*  pMem = pointer to memory containing profile data
*  nSize = size of memory related to profile
*  sReport - std::string to put report into
*  nStatus - return status value
*
* Return:
*  Pointer to icc profile object, or NULL on failure
*******************************************************************************
*/
CIccProfile* ValidateIccProfile(const icUInt8Number* pMem, icUInt32Number nSize, std::string& sReport, icValidateStatus& nStatus)
{
  CIccMemIO* pMemIO = new CIccMemIO;

  if (!pMemIO->Attach((icUInt8Number*)pMem, nSize)) {
    sReport = icMsgValidateCriticalError;
    sReport += " - Unable to attach IO access to memory block.";
    delete pMemIO;
    return NULL;
  }

  CIccProfile* pIcc = new CIccProfile;

  if (!pIcc) {
    delete pMemIO;
    return NULL;
  }

  nStatus = pIcc->ReadValidate(pMemIO, sReport);

  if (nStatus >= icValidateCriticalError) {
    delete pIcc;
    delete pMemIO;
    return NULL;
  }

  delete pMemIO;

  nStatus = icMaxStatus(nStatus, pIcc->Validate(sReport));

  return pIcc;
}


/**
 ******************************************************************************
 * Name: SaveIccProfile
 * 
 * Purpose: Save an ICC profile file.  
 * 
 * Args: 
 *  szFilename - zero terminated string with filename of ICC profile to create
 * 
 * Return: 
 *  true = success, false = failure
 *******************************************************************************
 */
bool SaveIccProfile(const icChar *szFilename, CIccProfile *pIcc, icProfileIDSaveMethod nWriteId)
{
  CIccFileIO FileIO;

  if (!pIcc)
    return false;

  if (!FileIO.Open(szFilename, "w+b")) {
    return false;
  }

  if (!pIcc->Write(&FileIO, nWriteId)) {
    return false;
  }

  return true;
}

/**
******************************************************************************
* Name: SaveIccProfile
*
* Purpose: Save an ICC profile file.
*
* Args:
*  f - handle to file io stream (closed outside of this function)
*
* Return:
*  true = success, false = failure
*******************************************************************************
*/
bool SaveIccProfile(FILE *f, CIccProfile *pIcc, icProfileIDSaveMethod nWriteId)
{
  CIccFileIO FileIO;

  if (!pIcc)
    return false;

  if (!FileIO.Attach(f)) {
    return false;
  }

  if (!pIcc->Write(&FileIO, nWriteId)) {
    return false;
  }

  FileIO.Detach();

  return true;
}

#ifdef WIN32
/**
******************************************************************************
* Name: SaveIccProfile
* 
* Purpose: Save an ICC profile file.  
* 
* Args: 
*  szFilename - zero terminated string with filename of ICC profile to create
* 
* Return: 
*  true = success, false = failure
*******************************************************************************
*/
bool SaveIccProfile(const icWChar *szFilename, CIccProfile *pIcc, icProfileIDSaveMethod nWriteId)
{
  CIccFileIO FileIO;

  if (!pIcc)
    return false;

  if (!FileIO.Open(szFilename, L"w+b")) {
    return false;
  }

  if (!pIcc->Write(&FileIO, nWriteId)) {
    return false;
  }

  return true;
}
#endif

/**
 ****************************************************************************
 * Name: CalcProfileID
 * 
 * Purpose: Calculate the Profile ID using MD5 Fingerprinting method. 
 * 
 * Args: 
 *  pIO = The CIccIO object,
 *  pProfileID = array where the profileID will be stored
 *
 ****************************************************************************
 */
void CalcProfileID(CIccIO *pIO, icProfileID *pProfileID)
{
  icUInt32Number nBlock;
  MD5_CTX context;
  icUInt8Number buffer[1024] = {0};

  //remember where we are
  size_t pos = pIO->Tell();

  //Get length and set up to read entire file
  size_t len = pIO->GetLength();
  pIO->Seek(0, icSeekSet);

  //read file updating checksum as we go
  icMD5Init(&context);
  nBlock = 0;
  while(len) {
    size_t num = pIO->Read8(&buffer[0],1024);
    if (num == 0)
        break;              // can't give a useful error here, but we need to break the infinite loop
    if (!nBlock) {  // Zero out 3 header contents in Profile ID calculation
      memset(buffer+44, 0, 4); //Profile flags
      memset(buffer+64, 0, 4);  //Rendering Intent
      memset(buffer+84, 0, 16); //Profile Id
    }
    icMD5Update(&context,buffer, (unsigned int) num);
    nBlock++;
    len -= num;
  }
  icMD5Final(&pProfileID->ID8[0],&context);

  //go back where we were
  pIO->Seek(pos, icSeekSet);
}

/**
 ****************************************************************************
 * Name: CalcProfileID
 * 
 * Purpose: Calculate the Profile ID using MD5 Fingerprinting method. 
 * 
 * Args: 
 *  szFileName = name of the file whose profile ID has to be calculated,
 *  pProfileID = array where the profileID will be stored
 *****************************************************************************
 */
bool CalcProfileID(const icChar *szFilename, icProfileID *pProfileID)
{
  CIccFileIO FileIO;

  if (!FileIO.Open(szFilename, "rb")) {
    memset(pProfileID, 0, sizeof(icProfileID));
    return false;
  }

  CalcProfileID(&FileIO, pProfileID);
  return true;
}

#ifdef WIN32
/**
****************************************************************************
* Name: CalcProfileID
* 
* Purpose: Calculate the Profile ID using MD5 Fingerprinting method. 
* 
* Args: 
*  szFileName = name of the file whose profile ID has to be calculated,
*  pProfileID = array where the profileID will be stored
*****************************************************************************
*/
bool CalcProfileID(const icWChar *szFilename, icProfileID *pProfileID)
{
  CIccFileIO FileIO;

  if (!FileIO.Open(szFilename, L"rb")) {
    memset(pProfileID, 0, sizeof(icProfileID));
    return false;
  }

  CalcProfileID(&FileIO, pProfileID);
  return true;
}
#endif


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
