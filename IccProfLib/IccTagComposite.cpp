/** @file
    File:       IccTagComposite.cpp

    Contains:   Implementation of Struct and Array Tags

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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak Oct-21-2006
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#endif

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "IccTagComposite.h"
#include "IccStructBasic.h"
#include "IccUtil.h"
#include "IccIO.h"
#include "IccStructFactory.h"
#include "IccArrayFactory.h"

void IIccStruct::Describe(std::string &sDescription, int nVerboseness) const
{
  if (m_pTagStruct) {
    const size_t bufSize = 256;
    char buf[bufSize];
    CIccInfo info;

    snprintf(buf, bufSize, "BEGIN UNKNOWN_TAG_STRUCT ");
    sDescription += buf;
    sDescription += info.GetStructSigName(m_pTagStruct->GetTagStructType());
    sDescription += "\n\n";

    TagEntryList::iterator i;
    TagEntryList *pList = m_pTagStruct->GetElemList();
    for (i=pList->begin(); i!=pList->end(); i++) {
      i->pTag->Describe(sDescription, nVerboseness);
    }

    sDescription += "\n";
    snprintf(buf, bufSize, "END TAG_STRUCT\n");
    sDescription += buf;
    sDescription += "\n";
  }
}

TagEntryList* IIccStruct::getTagEntries() const
{
  if (m_pTagStruct)
    return m_pTagStruct->m_ElemEntries;

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::CIccTagStruct
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagStruct::CIccTagStruct()
{
  m_ElemEntries = new(TagEntryList);
  m_ElemVals = new(TagPtrList);
  m_pStruct = NULL;
}

/**
 ******************************************************************************
 * Name: CIccTagStruct::CIccTagStruct
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagStruct::CIccTagStruct(const CIccTagStruct &subTags)
{
  m_sigStructType = subTags.m_sigStructType;
  m_ElemEntries = new(TagEntryList);
  m_ElemVals = new(TagPtrList);

  if (!subTags.m_ElemEntries->empty()) {
    TagEntryList::const_iterator i;
    IccTagEntry entry;
    for (i=subTags.m_ElemEntries->begin(); i!=subTags.m_ElemEntries->end(); i++) {
      entry.pTag = i->pTag->NewCopy();
      memcpy(&entry.TagInfo, &i->TagInfo, sizeof(icTag));
      m_ElemEntries->push_back(entry);
    }
  }

  if (!subTags.m_ElemVals->empty()) {
    TagPtrList::const_iterator i;
    IccTagPtr tagptr;
    for (i=subTags.m_ElemVals->begin(); i!=subTags.m_ElemVals->end(); i++) {
      tagptr.ptr = i->ptr->NewCopy();
      m_ElemVals->push_back(tagptr);
    }
  }

  if (subTags.m_pStruct) {
    m_pStruct = subTags.m_pStruct->NewCopy(this);
  }
  else {
    m_pStruct = NULL;
  }
}

/**
 ******************************************************************************
 * Name: &operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagStruct &CIccTagStruct::operator=(const CIccTagStruct &subTags)
{
  if (&subTags == this)
    return *this;

  Cleanup();

  m_sigStructType = subTags.m_sigStructType;

  if (!subTags.m_ElemEntries->empty()) {
    m_ElemEntries->clear();
    TagEntryList::const_iterator i;
    IccTagEntry entry;
    for (i=subTags.m_ElemEntries->begin(); i!=subTags.m_ElemEntries->end(); i++) {
      entry.pTag = i->pTag->NewCopy();
      memcpy(&entry.TagInfo, &i->TagInfo, sizeof(icTag));
      m_ElemEntries->push_back(entry);
    }
  }

  if (!subTags.m_ElemVals->empty()) {
    m_ElemVals->clear();
    TagPtrList::const_iterator i;
    IccTagPtr tagptr;
    for (i=subTags.m_ElemVals->begin(); i!=subTags.m_ElemVals->end(); i++) {
      tagptr.ptr = i->ptr->NewCopy();
      m_ElemVals->push_back(tagptr);
    }
  }

  if (subTags.m_pStruct)
    m_pStruct = subTags.m_pStruct->NewCopy(this);
  else
    m_pStruct = NULL;

  return *this;
}

/**
 ******************************************************************************
 * Name: CIccTagStruct::~CIccTagStruct
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagStruct::~CIccTagStruct()
{
  Cleanup();

  delete m_ElemEntries;
  delete m_ElemVals;

  if (m_pStruct)
    delete m_pStruct;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::SetTagStructType
 *
 * Purpose:
 *
 * Args:
 *
 * Return:
 ******************************************************************************/
bool CIccTagStruct::SetTagStructType(icStructSignature sig)
{
  if (m_pStruct)
    delete m_pStruct;

  m_sigStructType = sig;
  m_pStruct = CIccStructCreator::CreateStruct(m_sigStructType, this);

  return m_pStruct != NULL;
}

/**
 ******************************************************************************
 * Name: CIccTagStruct::ParseMem
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagStruct* CIccTagStruct::ParseMem(icUInt8Number *pMem, icUInt32Number size)
{
  CIccMemIO IO;

  if (!IO.Attach(pMem, size))
    return NULL;

  CIccTagStruct *pTags = new CIccTagStruct;

  if (!pTags->Read(size, &IO)) {
    delete pTags;
    return NULL;
  }

  return pTags;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccTagStruct::Describe(std::string &sDescription, int nVerboseness)
{
  std::string name;
  CIccStructCreator::GetStructSigName(name, m_sigStructType);

  sDescription += "BEGIN STRUCT \"";
  sDescription += name + "\"\n";

  if (m_pStruct) {
    m_pStruct->Describe(sDescription, nVerboseness);
  }
  else {
    IIccStruct *pStruct = CIccStructCreator::CreateStruct(m_sigStructType, this);
    if (pStruct) {
      pStruct->Describe(sDescription, nVerboseness);
      delete pStruct;
    }
    else {
      CIccStructUnknown structHandler(this);

      structHandler.Describe(sDescription, nVerboseness);
    }
  }
  sDescription += "END STRUCT \"";
  sDescription += name + "\"\n";
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::Read
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccTagStruct::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  m_tagSize = size;
  
  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icStructSignature) +
    sizeof(icUInt32Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  Cleanup();

  m_tagStart = (icUInt32Number) pIO->Tell();

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icStructSignature sigStructType;

  if (!pIO->Read32(&sigStructType))
    return false;

  SetTagStructType(sigStructType);

  icUInt32Number count, i;
  IccTagEntry TagEntry;

  TagEntry.pTag = NULL;

  if (!pIO->Read32(&count))
    return false;

  if (headerSize + (icUInt64Number)count*sizeof(icUInt32Number)*3 > size)
    return false;

  //Read TagDir
  for (i=0; i<count; i++) {
    if (!pIO->Read32(&TagEntry.TagInfo.sig) ||
        !pIO->Read32(&TagEntry.TagInfo.offset) ||
        !pIO->Read32(&TagEntry.TagInfo.size)) {
      return false;
    }
    m_ElemEntries->push_back(TagEntry);
  }

  TagEntryList::iterator entry;

  for (entry=m_ElemEntries->begin(); entry!=m_ElemEntries->end(); entry++) {
    if (!LoadElem((IccTagEntry*)&(entry->TagInfo), pIO)) {
      Cleanup();
      return false;
    }
  }


  return true;
}

/**
 ******************************************************************************
 * Name: CIccTagStruct::Write
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccTagStruct::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  m_tagStart = (icUInt32Number) pIO->Tell();

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_sigStructType))
    return false;

  TagEntryList::iterator i, j;
  icUInt32Number count;

  for (count=0, i=m_ElemEntries->begin(); i!= m_ElemEntries->end(); i++) {
    if (i->pTag)
      count++;
  }

  pIO->Write32(&count);

  size_t dirpos = pIO->Tell();

  //Write Unintialized TagDir
  for (i=m_ElemEntries->begin(); i!= m_ElemEntries->end(); i++) {
    if (i->pTag) {
      i->TagInfo.offset = 0;
      i->TagInfo.size = 0;

      pIO->Write32(&i->TagInfo.sig);
      pIO->Write32(&i->TagInfo.offset);
      pIO->Write32(&i->TagInfo.size);
    }
  }

  //Write Tags
  for (i=m_ElemEntries->begin(); i!= m_ElemEntries->end(); i++) {
    if (i->pTag) {
      for (j=m_ElemEntries->begin(); j!=i; j++) {
        if (i->pTag == j->pTag)
          break;
      }

      if (i==j) {
        i->TagInfo.offset = (icUInt32Number) pIO->GetLength();
        i->pTag->Write(pIO);
        i->TagInfo.size = (icUInt32Number)( pIO->GetLength() - i->TagInfo.offset);
        i->TagInfo.offset -= m_tagStart;

        pIO->Align32();
      }
      else {
        i->TagInfo.offset = j->TagInfo.offset;
        i->TagInfo.size = j->TagInfo.size;
      }
    }
  }
  size_t epos = pIO->Tell();

  pIO->Seek(dirpos, icSeekSet);

  //Write TagDir with offsets and sizes
  for (i=m_ElemEntries->begin(); i!= m_ElemEntries->end(); i++) {
    if (i->pTag) {
      pIO->Write32(&i->TagInfo.sig);
      pIO->Write32(&i->TagInfo.offset);
      pIO->Write32(&i->TagInfo.size);
    }
  }

  pIO->Seek(epos, icSeekSet);


  return true;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccTagStruct::Validate(std::string sigPath, std::string &sReport,
                                           const CIccProfile* pProfile /*=NULL*/) const
{ 
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  // Check for duplicate tags
  if (!AreElemsUnique()) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - There are duplicate elements.\n";
    rv =icMaxStatus(rv, icValidateWarning);
  }

  if (m_pStruct) {  //Should call GetStructHandler before validate to get 
    rv = icMaxStatus(rv, m_pStruct->Validate(sigPath, sReport, pProfile));
  }
  else {
    sReport += "Unknown tag struct type - Validating struct sub-tags\n";
    rv = icMaxStatus(rv, icValidateWarning);

    // Per Tag tests
    TagEntryList::iterator i;
    for (i = m_ElemEntries->begin(); i != m_ElemEntries->end(); i++) {
      rv = icMaxStatus(rv, i->pTag->Validate(sigPath + icGetSigPath(i->TagInfo.sig), sReport, pProfile));
    }
  }

  return rv;
}

/**
 ***************************************************************************
 * Name: CIccTagStruct::Cleanup
 * 
 * Purpose: Detach from a pending IO object
 ***************************************************************************
 */
void CIccTagStruct::Cleanup()
{
  TagPtrList::iterator i;

  for (i=m_ElemVals->begin(); i!=m_ElemVals->end(); i++) {
    if (i->ptr)
      delete i->ptr;
  }
  m_ElemEntries->clear();
  m_ElemVals->clear();

  if (m_pStruct)
    delete m_pStruct;
  m_pStruct = NULL;
  m_sigStructType = icSigUndefinedStruct;
}

/**
 ****************************************************************************
 * Name: CIccTagStruct::GetElem
 * 
 * Purpose: Get a tag entry with a given signature
 * 
 * Args: 
 *  sig - signature id to find in tag structure
 * 
 * Return: 
 *  Pointer to desired tag struct entry, or NULL if not found.
 *****************************************************************************
 */
IccTagEntry* CIccTagStruct::GetElem(icSignature sig) const
{
  TagEntryList::const_iterator i;

  for (i=m_ElemEntries->begin(); i!=m_ElemEntries->end(); i++) {
    if (i->TagInfo.sig==sig)
      return (IccTagEntry*)&(i->TagInfo);
  }

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::AreElemsUnique
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
bool CIccTagStruct::AreElemsUnique() const
{
  TagEntryList::const_iterator i, j;

  for (i=m_ElemEntries->begin(); i!=m_ElemEntries->end(); i++) {
    j=i;
    for (j++; j!= m_ElemEntries->end(); j++) {
      if (i->TagInfo.sig == j->TagInfo.sig)
        return false;
    }
  }

  return true;
}


/**
******************************************************************************
* Name: CIccTagStruct::GetElem
* 
* Purpose: Finds the first tag entry that points to the indicated tag object
* 
* Args: 
*  pTag - pointer to tag object desired to be found
* 
* Return: 
*  pointer to first tag struct entry that points to the desired tag object,
*  or NULL if tag object is not pointed to by any tag struct entries.
*******************************************************************************
*/
IccTagEntry* CIccTagStruct::GetElem(CIccTag *pTag) const
{
  TagEntryList::const_iterator i;

  for (i=m_ElemEntries->begin(); i!=m_ElemEntries->end(); i++) {
    if (i->pTag==pTag)
      return (IccTagEntry*)&(i->TagInfo);
  }

  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::FindElem
 * 
 * Purpose: Finds the tag object associated with the struct entry with the
 *  given signature. 
 * 
 * Args: 
 *  sig - element signature to find
 * 
 * Return: 
 *  The desired tag object, or NULL if unable to find in the struct
 *******************************************************************************
 */
CIccTag* CIccTagStruct::FindElem(icSignature sig)
{
  IccTagEntry *pEntry = GetElem(sig);

  if (pEntry) {
    return pEntry->pTag;
  }

  return NULL;
}

/**
******************************************************************************
* Name: CIccTagStruct::FindElemOfType
* 
* Purpose: Finds the tag object associated with the struct entry with the
*  given signature that has a specific tag type. 
* 
* Args: 
*  sig - element signature to find
*  sigType - signature of desired tag type
* 
* Return: 
*  The desired tag object, or NULL if unable to find in the struct
*******************************************************************************
*/
CIccTag* CIccTagStruct::FindElemOfType(icSignature sig, icTagTypeSignature sigType)
{
  IccTagEntry *pEntry = GetElem(sig);

  if (pEntry && pEntry->pTag && pEntry->pTag->GetType()==sigType) {
    return pEntry->pTag;
  }

  return NULL;
}


/**
******************************************************************************
* Name: CIccTagStruct::GetElemNumberValue
* 
* Purpose: Returns the number value associated with the first entry of a
*  CIccTagNumberArray based tag with the given signature. 
* 
* Args: 
*  sig - subtag signature to find
*  defaultValue - value to use if the tag cannot be found, or is not a number tag
* 
* Return: 
*  The tags value or defaultValue if unable to find subtag in the struct or if the
*  subtag is not a CIccTagNumberArray based tag.
*******************************************************************************
*/
icFloatNumber CIccTagStruct::GetElemNumberValue(icSignature sig, icFloatNumber defaultValue/* =0 */)
{
  CIccTag *pTag = FindElem(sig);

  if (!pTag || !pTag->IsNumArrayType())
    return defaultValue;

  CIccTagNumArray *pNumArray = (CIccTagNumArray*)pTag;
  icFloatNumber rv = defaultValue;

  pNumArray->GetValues(&rv, 0, 1);

  return rv;
}

/**
 ******************************************************************************
 * Name: CIccTagStruct::AttachTag
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
bool CIccTagStruct::AttachElem(icSignature sig, CIccTag *pTag)
{
  IccTagEntry *pEntry = GetElem(sig);

  if (pEntry) {
    if (pEntry->pTag == pTag)
      return true;

    return false;
  }

  IccTagEntry Entry;
  Entry.TagInfo.sig = (icTagSignature)sig;
  Entry.TagInfo.offset = 0;
  Entry.TagInfo.size = 0;
  Entry.pTag = pTag;

  m_ElemEntries->push_back(Entry);

  TagPtrList::iterator i;

  for (i=m_ElemVals->begin(); i!=m_ElemVals->end(); i++)
    if (i->ptr == pTag)
      break;

  if (i==m_ElemVals->end()) {
    IccTagPtr TagPtr;
    TagPtr.ptr = pTag;
    m_ElemVals->push_back(TagPtr);
  }

  return true;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::DeleteSubTag
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
bool CIccTagStruct::DeleteElem(icSignature sig)
{
  TagEntryList::iterator i;

  for (i=m_ElemEntries->begin(); i!=m_ElemEntries->end(); i++) {
    if (i->TagInfo.sig==sig)
      break;
  }
  if (i!=m_ElemEntries->end()) {
    CIccTag *pTag = i->pTag;
    m_ElemEntries->erase(i);

    if (!GetElem(pTag)) {
      DetachElem(pTag);
      delete pTag;
    }
    return true;
  }

  return false;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::LoadSubTag
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
bool CIccTagStruct::LoadElem(IccTagEntry *pTagEntry, CIccIO *pIO)
{
  if (!pTagEntry)
    return false;

  if (pTagEntry->pTag)
    return true;

  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt32Number);

  if (pTagEntry->TagInfo.offset<headerSize ||
      !pTagEntry->TagInfo.size ||
      pTagEntry->TagInfo.offset+pTagEntry->TagInfo.size > m_tagSize) {
    return false;
  }

  icTagTypeSignature sigType;

  icUInt32Number offset = pTagEntry->TagInfo.offset + m_tagStart;

  //First we need to get the tag type to create the right kind of tag
  if (pIO->Seek(offset, icSeekSet)!=offset)
    return false;

  if (!pIO->Read32(&sigType))
    return false;

  CIccTag *pTag = CIccTag::Create(sigType);

  if (!pTag)
    return false;

  //Now seek back to where the tag starts so the created tag object can read
  //in its data.
  //First we need to get the tag type to create the right kind of tag
  if (pIO->Seek(offset, icSeekSet)!=offset) {
    delete pTag;
    return false;
  }

  if (!pTag->Read(pTagEntry->TagInfo.size, pIO)) {
    delete pTag;
    return false;
  }

  pTagEntry->pTag = pTag;

  IccTagPtr TagPtr;

  TagPtr.ptr = pTag;

  m_ElemVals->push_back(TagPtr);

  TagEntryList::iterator i;

  for (i=m_ElemEntries->begin(); i!= m_ElemEntries->end(); i++) {
    if (i->TagInfo.offset == pTagEntry->TagInfo.offset &&
        i->pTag != pTag)
      i->pTag = pTag; 
  }
  
  return true;
}


/**
 ******************************************************************************
 * Name: CIccTagStruct::DetachSubTag
 * 
 * Purpose: Remove association of a tag object from all tag directory entries.
 *  Associated tag directory entries will be removed from the tag directory.
 *  The tag object is NOT deleted from memory, but is considered to be
 *  no longer associated with the CIccTagStruct object.  The caller assumes
 *  ownership of the tag object.
 * 
 * Args: 
 *  pTag - pointer to tag object unassociated with the profile object
 * 
 * Return: 
 *  true - tag object found and unassociated with profile object,
 *  false - tag object not found
 *******************************************************************************
 */
bool CIccTagStruct::DetachElem(CIccTag *pTag)
{
  if (!pTag)
    return false;
  
  TagPtrList::iterator i;

  for (i=m_ElemVals->begin(); i!=m_ElemVals->end(); i++) {
    if (i->ptr == pTag)
      break;
  }

  if (i==m_ElemVals->end())
    return false;

  m_ElemVals->erase(i);

  TagEntryList::iterator j;
  for (j=m_ElemEntries->begin(); j!=m_ElemEntries->end();) {
    if (j->pTag == pTag) {
      j=m_ElemEntries->erase(j);
    }
    else
      j++;
  }
  return true;
}


/**
****************************************************************************
* Name: CIccTagStruct::GetStructHandler
* 
* Purpose: Get struct object handler
* 
* Args: 
* 
* Return: 
*  Pointer to a IIccStruct object handler for the associated struct type
*****************************************************************************
*/
IIccStruct* CIccTagStruct::GetStructHandler()
{
  if (!m_pStruct) {

    m_pStruct = CIccStructCreator::CreateStruct(m_sigStructType, this);
  }
  return m_pStruct;
}


/**
******************************************************************************
* Name: CIccTagArray::CIccTagArray
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccTagArray::CIccTagArray()
{
  m_TagVals = NULL;
  m_nSize = 0;
  m_pArray = NULL;
  m_sigArrayType = icSigUnknownArray;
}

/**
******************************************************************************
* Name: CIccTagArray::CIccTagArray
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccTagArray::CIccTagArray(icArraySignature sigArrayType/* =icSigUndefinedArray */)
{
  m_TagVals = NULL;
  m_nSize = 0;

  m_pArray = NULL;
  SetTagArrayType(sigArrayType);
}

/**
******************************************************************************
* Name: CIccTagArray::CIccTagArray
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccTagArray::CIccTagArray(const CIccTagArray &tagAry)
{
  if (tagAry.m_nSize) {
    m_TagVals = new IccTagPtr[tagAry.m_nSize];

    icUInt32Number i;
    for (i=0; i<tagAry.m_nSize; i++) {
      if (tagAry.m_TagVals[i].ptr)
        m_TagVals[i].ptr = tagAry.m_TagVals[i].ptr->NewCopy();
      else
        m_TagVals[i].ptr = NULL;
    }
    m_nSize = tagAry.m_nSize;
  }
  m_sigArrayType = tagAry.m_sigArrayType;

  if (tagAry.m_pArray)
    m_pArray = tagAry.m_pArray->NewCopy(this);
  else
    m_pArray = NULL;
}

/**
******************************************************************************
* Name: &operator=
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccTagArray &CIccTagArray::operator=(const CIccTagArray &tagAry)
{
  if (&tagAry == this)
    return *this;

  Cleanup();

  if (tagAry.m_nSize) {
    m_TagVals = new IccTagPtr[tagAry.m_nSize];

    icUInt32Number i;
    for (i=0; i<m_nSize; i++) {
      if (tagAry.m_TagVals[i].ptr)
        m_TagVals[i].ptr = tagAry.m_TagVals[i].ptr->NewCopy();
      else
        m_TagVals[i].ptr = NULL;
    }
    m_nSize = tagAry.m_nSize;
  }

  m_sigArrayType = tagAry.m_sigArrayType;

  if (tagAry.m_pArray)
    m_pArray = tagAry.m_pArray->NewCopy(this);
  else
    m_pArray = NULL;

  return *this;
}

/**
******************************************************************************
* Name: CIccTagArray::~CIccTagArray
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccTagArray::~CIccTagArray()
{
  Cleanup();
}


/**
******************************************************************************
* Name: CIccTagArray::~SetTagArrayType
*
* Purpose:
*   Set the signature of the tag array type.  This also sets the m_pArray
*   to allow array type specific handling to be utilized.
*
* Args:
*   sig - signature of array type
*
* Return:
*   true if sig is a recognized array type, false otherwise
******************************************************************************/
bool CIccTagArray::SetTagArrayType(icArraySignature sig)
{
  if (m_pArray)
    delete m_pArray;

  m_sigArrayType = sig;
  m_pArray = CIccArrayCreator::CreateArray(m_sigArrayType, this);

  return (m_pArray != NULL) || (m_sigArrayType == icSigUtf8TextTypeArray);
}


/**
******************************************************************************
* Name: CIccTagArray::AreAllOftype
* 
* Purpose: 
*   checks if all array elements have type signature sigTagType
* Args: 
*   sigTagType
* Return: 
*   true if all elements are of type sigTagType, false otherwise
******************************************************************************/
bool CIccTagArray::AreAllOfType(icTagTypeSignature sigTagType)
{
  icUInt32Number i;
  for (i=0; i<m_nSize; i++) {
    CIccTag *pTag = GetIndex(i);
    if (!pTag || pTag->GetType()!=sigTagType)
      return false;
  }

  return true;
}


/**
******************************************************************************
* Name: CIccTagArray::Describe
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
void CIccTagArray::Describe(std::string &sDescription, int nVerboseness)
{
  std::string name;
  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo info;

 // IIccArray *pArrayObj = GetArrayHandler();       // unused, appears to be a memory leak
  CIccArrayCreator::GetArraySigName(name, m_sigArrayType);

  sDescription += "BEGIN TAG_ARRAY \"";
  sDescription += name;
  sDescription += "\"\n\n";

  icUInt32Number i;

  for (i=0; i<m_nSize; i++) {
    if (i)
      sDescription += "\n";
    snprintf(buf, bufSize, "BEGIN INDEX[%d]\n", i);
    sDescription +=  buf;
    
    if (m_TagVals[i].ptr) {
      m_TagVals[i].ptr->Describe(sDescription, nVerboseness);
    }
    snprintf(buf, bufSize, "END INDEX[%d]\n", i);
    sDescription += buf;
  }

  sDescription += "\n";
  sDescription += "END TAG_ARRAY \"";
  sDescription += name;
  sDescription += "\"\n";
}


/**
******************************************************************************
* Name: CIccTagArray::Read
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccTagArray::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) +
    sizeof(icTagTypeSignature) +
    sizeof(icStructSignature) +
    sizeof(icUInt32Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  Cleanup();

  size_t nTagStart = pIO->Tell();

  if (!pIO->Read32(&sig))
    return false;

  icArraySignature sigArrayType;

  if (!pIO->Read32(&m_nReserved) ||
      !pIO->Read32(&sigArrayType))
    return false;

  SetTagArrayType(sigArrayType);

  icUInt32Number count, i, j;

  if (!pIO->Read32(&count))
    return false;

  if (headerSize + count*sizeof(icPositionNumber) > size)
    return false;

  if (count) {
    icPositionNumber *tagPos = new icPositionNumber[count];

    if (!SetSize(count)) {
      delete[] tagPos;
      return false;
    }

    for (i=0; i<count; i++) {
      if (!pIO->Read32(&tagPos[i].offset) ||
          !pIO->Read32(&tagPos[i].size)) {
        delete [] tagPos;
        return false;
      }
    }

    for (i=0; i<count; i++) {
      if (!tagPos[i].offset || !tagPos[i].size) {
        m_TagVals[i].ptr = NULL;
        continue;
      }

      for (j=0; j<i; j++) {
        if (tagPos[i].offset == tagPos[j].offset)
          break;
      }

      if (j<i) {
        m_TagVals[i].ptr = m_TagVals[j].ptr;
      }
      else {
        if (tagPos[i].offset + tagPos[i].size > size) {
          delete [] tagPos;
          return false;
        }
        pIO->Seek(nTagStart + tagPos[i].offset, icSeekSet);

        icTagTypeSignature tagSig;
        pIO->Read32(&tagSig);
        pIO->Seek(nTagStart + tagPos[i].offset, icSeekSet);

        CIccTag *pTag = CIccTagCreator::CreateTag(tagSig);
        if (pTag) {
          if (!pTag->Read(tagPos[i].size, pIO)) {
            delete [] tagPos;
            return false;
          }
          m_TagVals[i].ptr = pTag;
        }
        else {
          delete [] tagPos;
          return false;
        }
      }
    }

    delete [] tagPos;
  }  

  return true;
}

/**
******************************************************************************
* Name: CIccTagArray::Write
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccTagArray::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  size_t nTagStart = pIO->Tell();

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_sigArrayType))
    return false;

  if (!pIO->Write32(&m_nSize))
    return false;

  if (m_nSize) {
    icUInt32Number i, j;

    size_t pos = pIO->Tell();

    //Write Unintialized TagPosition block
    icUInt32Number zero = 0;
    for (i=0; i<m_nSize; i++) {
      if (!pIO->Write32(&zero) || !pIO->Write32(&zero))
        return false;
    }
    icPositionNumber *tagPos = new icPositionNumber[m_nSize];
    if (!tagPos)
      return false;

    //Write Tags
    for (i=0; i<m_nSize; i++) {
      if (m_TagVals[i].ptr) {
        for (j=0; j<i; j++) {
          if (m_TagVals[i].ptr==m_TagVals[j].ptr)
            break;
        }

        if (j<i) {
          tagPos[i].offset = tagPos[j].offset;
          tagPos[i].size = tagPos[j].offset;
        }
        else {
          tagPos[i].offset = (icUInt32Number)(pIO->Tell() - nTagStart);
          if (!m_TagVals[i].ptr->Write(pIO)) {
            delete [] tagPos;
            return false;
          }
          tagPos[i].size = (icUInt32Number)( pIO->Tell() - nTagStart - tagPos[i].offset );
          pIO->Align32();
        }
      }
      else {
        tagPos[i].offset = 0;
        tagPos[i].size = 0;
      }
    }
    size_t endPos = pIO->Tell();

    //Update TagPosition block
    pIO->Seek(pos, icSeekSet);
    for (i=0; i<m_nSize; i++) {
      if (!pIO->Write32(&tagPos[i].offset) ||
          !pIO->Write32(&tagPos[i].size)) {
          delete [] tagPos;
          return false;
      }
    }
    delete[] tagPos;
    pIO->Seek(endPos, icSeekSet);
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagArray::SetSize
 * 
 * Purpose: Sets the size of the tag array.
 * 
 * Args: 
 *  nSize - number of tag entries,
 *****************************************************************************
 */
bool CIccTagArray::SetSize(icUInt32Number nSize)
{
  if (!m_nSize) {
    m_TagVals = (IccTagPtr*)calloc(nSize, sizeof(IccTagPtr));
    if (!m_TagVals) {
      m_nSize =0;
      return false;
    }
  }
  else {
    if (nSize<=m_nSize)
      return true;

    m_TagVals = (IccTagPtr*)icRealloc(m_TagVals, nSize*sizeof(IccTagPtr));
    if (!m_TagVals) {
      m_nSize = 0;
      return false;
    }
    memset(&m_TagVals[m_nSize], 0, (nSize-m_nSize)*sizeof(IccTagPtr));
  }
  m_nSize = nSize;
  return true;
}


/**
******************************************************************************
* Name: CIccTagArray::Validate
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icValidateStatus CIccTagArray::Validate(std::string sigPath, std::string &sReport,
                                         const CIccProfile* pProfile /*=NULL*/) const
{ 
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sigAryPath = sigPath + icGetSigPath(m_sigArrayType);

  if (m_pArray) {  //Should call GetArrayHandler before validate to get 
    rv = icMaxStatus(rv, m_pArray->Validate(sigPath, sReport, pProfile));
  }
  else if (m_sigArrayType==icSigUtf8TextTypeArray) { //UTF8 text arrays are known
    //Check # of channels 
    if (icGetFirstSigPathSig(sigPath) == icSigMaterialTypeArrayTag && 
        pProfile &&
        m_nSize != icGetMaterialColorSpaceSamples(pProfile->m_Header.mcs)) {
      std::string sSigPathName = Info.GetSigPathName(sigPath);

      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - Number of material channel names does not match MCS in header.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
    icUInt32Number i;
    for (i=0; i<m_nSize; i++) {
      rv = icMaxStatus(rv, m_TagVals[i].ptr->Validate(sigAryPath, sReport, pProfile));
    }
  }
  else { 
    icUInt32Number i;
    sReport += "Unknown tag array type - Validating array sub-tags\n";
    rv = icMaxStatus(rv, icValidateWarning);

    for (i=0; i<m_nSize; i++) {
      rv = icMaxStatus(rv, m_TagVals[i].ptr->Validate(sigAryPath, sReport, pProfile));
    }
  }

  return rv;
}

/**
***************************************************************************
* Name: CIccTagArray::Cleanup
* 
* Purpose: Detach from a pending IO object
***************************************************************************
*/
void CIccTagArray::Cleanup()
{
  icUInt32Number i, j;
  CIccTag* pTag;

  for (i=0; i<m_nSize; i++) {
    pTag = m_TagVals[i].ptr;
    if (pTag) {
      for (j=i+1; j<m_nSize; j++) {
        if (m_TagVals[i].ptr==pTag)
          m_TagVals[i].ptr = NULL;
      }
      delete pTag;
      m_TagVals[i].ptr = NULL;
    }
  }

  if (m_TagVals)
    free(m_TagVals);
  m_TagVals = nullptr;

  if (m_pArray)
    delete m_pArray;

  m_pArray = NULL;
  m_sigArrayType = icSigUndefinedArray;
}

/**
****************************************************************************
* Name: CIccTagArray::GetIndex
* 
* Purpose: Get a tag entry with a given index
* 
* Args: 
*  nIndex - index of tag in array
* 
* Return: 
*  Pointer to desired tag entry, or NULL if not found.
*****************************************************************************
*/
CIccTag* CIccTagArray::GetIndex(icUInt32Number nIndex) const
{
  if (nIndex>m_nSize)
    return NULL;

  return m_TagVals[nIndex].ptr;
}


/**
****************************************************************************
* Name: CIccTagArray::AttachTag
* 
* Purpose: Get a tag entry with a given signature
* 
* Args: 
*  nIndex - index of tag in array
* 
* Return: 
*  Pointer to desired tag struct entry, or NULL if not found.
*****************************************************************************
*/
bool CIccTagArray::AttachTag(icUInt32Number nIndex, CIccTag *pTag)
{
  if (nIndex>m_nSize || !pTag) {
    return false;
  }

  if (m_TagVals[nIndex].ptr && m_TagVals[nIndex].ptr != pTag) {
    return false;
  }

  m_TagVals[nIndex].ptr = pTag;
  return true;
}


/**
****************************************************************************
* Name: CIccTagArray::DetachTag
* 
* Purpose: Detach tag value at given index
* 
* Args: 
*  nIndex - index of tag in array
*  bDeleteFlag - flag indicating detached tag should be deleted
* 
* Return: 
*  Pointer to Detached tag struct entry, or NULL if not found or deleted.
*****************************************************************************
*/
CIccTag * CIccTagArray::DetachTag(icUInt32Number nIndex, bool bDeleteFlag)
{
   if (nIndex>m_nSize)
     return NULL;

   CIccTag *rv = m_TagVals[nIndex].ptr;
   m_TagVals[nIndex].ptr = NULL;

   if (bDeleteFlag) {
     delete rv;
     rv = NULL;
   }

   return rv;
}


/**
****************************************************************************
* Name: CIccTagArray::GetArrayHandler
* 
* Purpose: Get Array object handler
* 
* Args: 
* 
* Return: 
*  Pointer to an Array object handler for the associated array type
*****************************************************************************
*/
IIccArray *CIccTagArray::GetArrayHandler()
{
  if (!m_pArray) {
    m_pArray = CIccArrayCreator::CreateArray(m_sigArrayType, this);
  }

  return m_pArray;
}


IIccStruct *icGetTagStructHandler(CIccTag *pTag)
{
  if (!pTag)
    return NULL;
  if (pTag->GetType()!=icSigTagStructType)
    return NULL;

  CIccTagStruct *pStruct = (CIccTagStruct*)pTag;

  return pStruct->GetStructHandler();
}

IIccStruct *icGetTagStructHandlerOfType(CIccTag* pTag, icStructSignature sig)
{
  if (pTag->GetTagStructType()==sig)
    return icGetTagStructHandler(pTag);

  return NULL;
}

IIccArray *icGetTagArrayHandler(CIccTag *pTag)
{
  if (!pTag)
    return NULL;
  if (pTag->GetType()!=icSigTagArrayType)
    return NULL;

  CIccTagArray *pArray = (CIccTagArray*)pTag;

  return pArray->GetArrayHandler();
}

IIccArray *icGetTagArrayHandlerOfType(CIccTag* pTag, icArraySignature sig)
{
  if (pTag->GetTagArrayType()==sig)
    return icGetTagArrayHandler(pTag);

  return NULL;
}

