/** @file
    File:       IccProfile.h

    Contains:   Header for implementation of the CIccProfile class.

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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCPROFILE_H)
#define _ICCPROFILE_H

#include "IccDefs.h"
#include "IccPcc.h"
#include <list>
#include <string>

#if defined(__cplusplus) && defined(USEICCDEVNAMESPACE)
namespace iccDEV {
#endif

#ifdef __cplusplus

class ICCPROFLIB_API CIccTag;
class ICCPROFLIB_API CIccIO;
class ICCPROFLIB_API CIccMemIO;

/**
 **************************************************************************
 * Type: Structure
 * 
 * Purpose: 
 *  This structure stores all the information of an individual tag 
 *  including the header information (tag signature, offset and size).
 **************************************************************************
 */
struct IccTagEntry
{

  icTag TagInfo;
  CIccTag* pTag;
};

/**
 **************************************************************************
 * Type: List
 * 
 * Purpose: List of all the tag entries.
 *
 **************************************************************************
 */
typedef std::list<IccTagEntry> TagEntryList;

/**
 **************************************************************************
 * Type: Structure
 * 
 * Purpose: Contains pointer to a tag.
 **************************************************************************
 */
typedef struct {CIccTag *ptr;} IccTagPtr;

/**
 **************************************************************************
 * Type: List
 * 
 * Purpose: List of pointers to the tags.
 *  
 **************************************************************************
 */
typedef std::list<IccTagPtr> TagPtrList;

typedef enum {
  icVersionBasedID,
  icAlwaysWriteID,
  icNeverWriteID,
}icProfileIDSaveMethod;

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: 
 *  This is the base class for an ICC profile. All the operations 
 *  on a profile is done using this object.
 **************************************************************************
 */
class ICCPROFLIB_API CIccProfile : public IIccProfileConnectionConditions 
{
public:
  CIccProfile();
  CIccProfile(const CIccProfile &Profile);
  CIccProfile &operator=(const CIccProfile &Profile);
  virtual CIccProfile* NewCopy() const { return new CIccProfile(*this); }
  virtual CIccProfile* NewProfile() const { return new CIccProfile(); }
  virtual ~CIccProfile();

  virtual const char *GetClassName() const { return "CIccProfile"; }

  icHeader m_Header;

  TagEntryList *m_Tags;

  CIccTag* FindTag(icSignature sig);
  const CIccTag* FindTagConst(icSignature sig) const;
  CIccTag* FindTagOfType(icSignature tagSig, icTagTypeSignature typeSig);

  bool AttachTag(icSignature sig, CIccTag *pTag);
  bool DeleteTag(icSignature sig);
  CIccMemIO* GetTagIO(icSignature sig); //caller should delete returned result
	bool ReadTags(CIccProfile* pProfile); // will read in all the tags using the IO of the passed profile

  bool Attach(CIccIO *pIO, bool bUseSubProfile=false);
  bool Detach();
  bool HasIO() { return m_pAttachIO != NULL;  }

  void CopyAttach(CIccProfile* pProfile, bool bSharedIO=false);

  bool Read(CIccIO *pIO, bool bUseSubProfile=false);
  icValidateStatus ReadValidate(CIccIO *pIO, std::string &sReport);
  bool Write(CIccIO *pIO, icProfileIDSaveMethod nWriteId=icVersionBasedID);

  bool ReadProfileID(icProfileID &profileID); //works if HasIO() is true 

  bool ReadPccTags();

  void InitHeader();
  icValidateStatus Validate(std::string &sReport, std::string sSigPath="", const CIccProfile *pParentProfile=NULL) const;

  icUInt16Number GetSpaceSamples() const;
  icUInt16Number GetParentSpaceSamples() const;
  icColorSpaceSignature GetParentColorSpace() const { return m_parentColorSpace; }

  bool AreTagsUnique() const;
	bool IsTagPresent(icSignature sig) const { return (GetTag(sig)!=NULL); }


  //Implement the IIccProfileConnectionConditions interface
  virtual const CIccTagSpectralViewingConditions *getPccViewingConditions() { return (const CIccTagSpectralViewingConditions*)
                                                                                FindTagOfType(icSigSpectralViewingConditionsTag, 
                                                                                              icSigSpectralViewingConditionsType); }
  virtual CIccTagMultiProcessElement *getCustomToStandardPcc() { return (CIccTagMultiProcessElement*)
                                                                   FindTagOfType(icSigCustomToStandardPccTag, 
                                                                                 icSigMultiProcessElementType); }

  virtual CIccTagMultiProcessElement *getStandardToCustomPcc() { return (CIccTagMultiProcessElement*)
                                                                   FindTagOfType(icSigStandardToCustomPccTag, 
                                                                                 icSigMultiProcessElementType); }
  virtual icIlluminant getPccIlluminant();
  virtual icFloatNumber getPccCCT();
  virtual icStandardObserver getPccObserver();
  virtual void getNormIlluminantXYZ(icFloatNumber *pXYZ);
  virtual void getLumIlluminantXYZ(icFloatNumber *pXYZ);
  virtual bool getMediaWhiteXYZ(icFloatNumber *pXYZ);
  
  bool calcNormIlluminantXYZ(icFloatNumber *pXYZ, IIccProfileConnectionConditions *pObservingPCC);
  bool calcLumIlluminantXYZ(icFloatNumber *pXYZ, IIccProfileConnectionConditions *pObservingPCC);
  bool calcMediaWhiteXYZ(icFloatNumber *pXYZ, IIccProfileConnectionConditions *pObservingPCC);

protected:

  void Cleanup();
  IccTagEntry* GetTag(icSignature sig) const;
  IccTagEntry* GetTag(icSignature sig, const CIccProfile* pParent) const;
  IccTagEntry* GetTag(CIccTag *pTag) const;
  bool ReadBasic(CIccIO *pIO);
  bool LoadTag(IccTagEntry *pTagEntry, CIccIO *pIO, bool bReadAll=false);
  bool DetachTag(CIccTag *pTag);

  CIccIO* ConnectSubProfile(CIccIO *pIO, bool bOwnIO) const;

  // Profile Validation functions
  icValidateStatus CheckRequiredTags(std::string &sReport, const CIccProfile *pParentProfile = NULL) const;
  bool CheckTagExclusion(std::string &sReport) const;
  icValidateStatus CheckHeader(std::string &sReport) const;
  icValidateStatus CheckTagTypes(std::string &sReport) const;
  bool IsTypeValid(icTagSignature tagSig, icTagTypeSignature typeSig,
                   icStructSignature structSig=icSigUndefinedStruct,
                   icArraySignature arraySig=icSigUndefinedArray) const;
  bool CheckFileSize(CIccIO *pIO) const;

  CIccIO *m_pAttachIO;
  bool m_bSharedIO = false;

  TagPtrList *m_TagVals;

  icColorSpaceSignature m_parentColorSpace = icSigNoColorData;
};

CIccProfile ICCPROFLIB_API *ReadIccProfile(const icChar *szFilename, bool bUseSubProfile=false);
CIccProfile ICCPROFLIB_API *ReadIccProfile(const icUInt8Number *pMem, icUInt32Number nSize, bool bUseSubProfile=false);
CIccProfile ICCPROFLIB_API *OpenIccProfile(const icChar *szFilename, bool bUseSubProfile=false);
CIccProfile ICCPROFLIB_API *OpenIccProfile(const icUInt8Number *pMem, icUInt32Number nSize, bool bUseSubProfile=false);  //pMem must be available for entire life of returned CIccProfile Object

CIccProfile ICCPROFLIB_API *ValidateIccProfile(CIccIO *pIO, std::string &sReport, icValidateStatus &nStatus);
CIccProfile ICCPROFLIB_API *ValidateIccProfile(const icChar *szFilename, std::string &sReport, icValidateStatus &nStatus);
CIccProfile ICCPROFLIB_API* ValidateIccProfile(const icUInt8Number* pMem, icUInt32Number nSize, std::string& sReport, icValidateStatus& nStatus);

bool ICCPROFLIB_API SaveIccProfile(const icChar *szFilename, CIccProfile *pIcc, icProfileIDSaveMethod nWriteId=icVersionBasedID);
bool ICCPROFLIB_API SaveIccProfile(FILE *f, CIccProfile *pIcc, icProfileIDSaveMethod nWriteId = icVersionBasedID);

void ICCPROFLIB_API CalcProfileID(CIccIO *pIO, icProfileID *profileID);
bool ICCPROFLIB_API CalcProfileID(const icChar *szFilename, icProfileID *profileID);

#ifdef WIN32
CIccProfile ICCPROFLIB_API *ReadIccProfile(const icWChar *szFilename, bool bUseSubProfile=false);
CIccProfile ICCPROFLIB_API *OpenIccProfile(const icWChar *szFilename, bool bUseSubProfile=false);
CIccProfile ICCPROFLIB_API *ValidateIccProfile(const icWChar *szFilename, std::string &sReport, icValidateStatus &nStatus);
bool ICCPROFLIB_API SaveIccProfile(const icWChar *szFilename, CIccProfile *pIcc, icProfileIDSaveMethod nWriteId=icVersionBasedID);
bool ICCPROFLIB_API CalcProfileID(const icWChar *szFilename, icProfileID *profileID);
#endif


typedef CIccProfile* CIccProfilePtr;

#endif //__cplusplus

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCPROFILE_H)
