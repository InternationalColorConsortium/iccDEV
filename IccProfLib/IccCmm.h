/** @file
    File:       IccCmm.h

    Contains:   Header file for implementation of the CIccCmm class.

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
// -Added support for Monochrome ICC profile apply by Rohit Patil 12-03-2008
// -Integrate changes for PCS adjustment by George Pawle 12-09-2008
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCCMM_H)
#define _ICCCMM_H

#include "IccProfile.h"
#include "IccTag.h"
#include "IccUtil.h"
#include "IccMatrixMath.h"
#include <list>
#include <cstring>
#include <cstdlib>

#if defined(__cpluplus) && defined(USEICCDEVNAMESPACE)
namespace iccDEV {
#endif
  
/// CMM return status values
//Note: Update CIccCMM::GetStatusText() as appropriate 
typedef enum {
  icCmmStatBad                = -1,
  icCmmStatOk                 = 0,
  icCmmStatCantOpenProfile    = 1,
  icCmmStatBadSpaceLink       = 2,
  icCmmStatInvalidProfile     = 3,
  icCmmStatBadXform           = 4,
  icCmmStatInvalidLut         = 5,
  icCmmStatProfileMissingTag  = 6,
  icCmmStatColorNotFound      = 7,
  icCmmStatIncorrectApply     = 8,
  icCmmStatBadColorEncoding   = 9,
  icCmmStatAllocErr           = 10,
  icCmmStatBadLutType         = 11,
  icCmmStatIdentityXform      = 12,
  icCmmStatUnsupportedPcsLink = 13,
  icCmmStatBadConnection      = 14,
  icCmmStatBadTintXform       = 15,
  icCmmStatTooManySamples     = 16,
  icCmmStatBadMCSLink         = 17,
  icCmmStatUnsupported        = 18,
} icStatusCMM;

/// CMM Interpolation types
typedef enum {
  icInterpLinear               = 0,
  icInterpTetrahedral          = 1,
} icXformInterp;

typedef enum {
  icNoMCS = 0,
  icToMCS = 1,
  icFromMCS = 2
}icMCSConnectionType;

/// CMM Xform LUT types
typedef enum {
  icXformLutColor              = 0x0,  //This is a combination of icXformLutColorimetric with icXformLutSpectral
  icXformLutNamedColor         = 0x1,
  icXformLutPreview            = 0x2,
  icXformLutGamut              = 0x3,
  icXformLutBPC                = 0x4,
  icXformLutBRDFParam          = 0x5,
  icXformLutBRDFDirect         = 0x6,
  icXformLutBRDFMcsParam       = 0x7,
  icXformLutMCS                = 0x8,
  icXformLutColorimetric       = 0x9,
  icXformLutSpectral           = 0xA,
 } icXformLutType;

#define icPerceptualRefBlackX 0.00336
#define icPerceptualRefBlackY 0.0034731
#define icPerceptualRefBlackZ 0.00287

#define icPerceptualRefWhiteX 0.9642
#define icPerceptualRefWhiteY 1.0000
#define icPerceptualRefWhiteZ 0.8249

// CMM Xform types
typedef enum {
  icXformTypeMatrixTRC  = 0,
  icXformType3DLut      = 1,
  icXformType4DLut      = 2,
  icXformTypeNDLut      = 3,
  icXformTypeNamedColor = 4,  //Creator uses icNamedColorXformHint
  icXformTypeMpe        = 5,
	icXformTypeMonochrome = 6,

  icXformTypePCS        = 0x7fffffe,
  icXformTypeUnknown    = 0x7ffffff,
} icXformType;


#ifdef __cplusplus

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Interface for creation of a named xform hint
**************************************************************************
*/
class ICCPROFLIB_API IIccCreateXformHint
{
public:
  virtual ~IIccCreateXformHint() {}
	virtual const char *GetHintType() const=0;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Manages the named xform hints
**************************************************************************
*/
class ICCPROFLIB_API CIccCreateXformHintManager
{
public:
	CIccCreateXformHintManager() { m_pList = NULL; }
	~CIccCreateXformHintManager();

	/// Adds and owns the passed named hint to it's list
	bool AddHint(IIccCreateXformHint* pHint);

	/// Deletes the object referenced by the passed named hint pointer and removes it from the list
	bool DeleteHint(IIccCreateXformHint* pHint);

	/// Finds and returns a pointer to the named hint
	IIccCreateXformHint* GetHint(const char* hintName);

private:
	// private hint ptr class
	class IIccCreateXformHintPtr {
	public:
		IIccCreateXformHint* ptr;
	};
	typedef std::list<IIccCreateXformHintPtr> IIccCreateXformHintList;

	// private members
	IIccCreateXformHintList* m_pList;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Hint for creation of a named color xform
**************************************************************************
*/
class ICCPROFLIB_API CIccCreateNamedColorXformHint : public IIccCreateXformHint
{
public:
  CIccCreateNamedColorXformHint() {
    csSpectralPcs = icSigNoSpectralData;
    memset(&spectralRange, 0, sizeof(spectralRange));
    memset(&biSpectralRange, 0, sizeof(biSpectralRange));
  }

	virtual const char *GetHintType() const {return "CIccCreateNamedColorXformHint";}

	icColorSpaceSignature csPcs;
	icColorSpaceSignature csDevice;
  icSpectralColorSignature csSpectralPcs;
  icSpectralRange spectralRange;
  icSpectralRange biSpectralRange;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Hint for getting CMM environment variable values
**************************************************************************
*/
class ICCPROFLIB_API CIccCreateCmmEnvVarXformHint : public IIccCreateXformHint
{
public:
  virtual ~CIccCreateCmmEnvVarXformHint() {}

  virtual const char *GetHintType() const {return "CIccCreateCmmEnvVarXformHint";}
  virtual IIccCmmEnvVarLookup *GetNewCmmEnvVarLookup() const=0;
};

/**
**************************************************************************
* Type: Class
*
* Purpose:
*  Hint for getting CMM environment variable values
**************************************************************************
*/
class ICCPROFLIB_API CIccCreateCmmPccEnvVarXformHint : public IIccCreateXformHint
{
public:
  virtual ~CIccCreateCmmPccEnvVarXformHint() {}

  virtual const char* GetHintType() const { return "CIccCreateCmmPccEnvVarXformHint"; }
  virtual IIccCmmEnvVarLookup* GetNewCmmEnvVarLookup() const = 0;
};


/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Interface for calculating adjust PCS factors
**************************************************************************
*/
class CIccXform;
class ICCPROFLIB_API IIccAdjustPCSXform
{
public:
	virtual ~IIccAdjustPCSXform() {}

	virtual bool CalcFactors(const CIccProfile* pProfile, const CIccXform* pXfm, icFloatNumber* Scale, icFloatNumber* Offset) const=0;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Hint for calculating adjust PCS factors
**************************************************************************
*/
class ICCPROFLIB_API CIccCreateAdjustPCSXformHint : public IIccCreateXformHint
{
public:
  virtual ~CIccCreateAdjustPCSXformHint() {}

	virtual const char *GetHintType() const {return "CIccCreateAdjustPCSXformHint";}
	virtual const char *GetAdjustPCSType() const=0;
	virtual IIccAdjustPCSXform *GetNewAdjustPCSXform() const=0;
};

/**
**************************************************************************
* Type: Class
*
* Purpose:
*		 Hint for using Luminance Matching colorimetric PCS processing
**************************************************************************
*/
class ICCPROFLIB_API CIccLuminanceMatchingHint : public IIccCreateXformHint
{
public:
  virtual const char *GetHintType() const { return "CIccLuminanceMatchingHint"; }
};



//forward reference to CIccXform used by CIccApplyXform
class CIccApplyXform;

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: 
 *  This is the base CMM xform object.  A general static creation function,
 *  base behavior, and data are defined.  The Create() function will assign
 *  a profile to the class.  The CIccProfile object will then be owned by the
 *  xform object and later deleted when the IccXform is deleted.
 **************************************************************************
 */
class ICCPROFLIB_API CIccXform
{
  friend class CIccPcsXform;
public:
  CIccXform();
  virtual ~CIccXform();

  virtual icXformType GetXformType() const = 0;

  ///Note: The returned CIccXform will own the profile.
  static CIccXform *Create(CIccProfile *pProfile, bool bInput=true, 
                           icRenderingIntent nIntent=icUnknownIntent, 
                           icXformInterp nInterp=icInterpLinear, 
                           IIccProfileConnectionConditions *pPcc=NULL,
                           icXformLutType nLutType=icXformLutColor,
                           bool bUseD2BTags=true, 
                           CIccCreateXformHintManager *pHintManager=NULL);

  ///Note: Provide an interface to work profile references.  The IccProfile is copied, and the copy's ownership
  ///is turned over to the Returned CIccXform object.
  static CIccXform *Create(CIccProfile &pProfile,
                           bool bInput=true, 
                           icRenderingIntent nIntent=icUnknownIntent, 
                           icXformInterp nInterp=icInterpLinear,
                           IIccProfileConnectionConditions *pPcc=NULL,
                           icXformLutType nLutType=icXformLutColor,
                           bool bUseD2BTags=true, 
                           CIccCreateXformHintManager *pHintManager=NULL);

  ///Note: The returned CIccXform will own the profile.
  static CIccXform *Create(CIccProfile *pProfile, CIccTag *pXformTag, bool bInput = true,
                           icRenderingIntent nIntent = icUnknownIntent,
                           icXformInterp nInterp = icInterpLinear,
                           IIccProfileConnectionConditions *pPcc = NULL,
                           bool bUseSpectralPCS = false,
                           CIccCreateXformHintManager *pHintManager = NULL);

  //ShareProfile should only be called when the profile is shared between transforms 
  void ShareProfile() { m_bOwnsProfile = false; } 
  void SetPcsAdjustXform() { m_bPcsAdjustXform = true; }

  virtual icStatusCMM Begin();

  virtual CIccApplyXform *GetNewApply(icStatusCMM &status);

  virtual void Apply(CIccApplyXform *pXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const = 0;

  //Detach and remove CIccIO object associated with xform's profile.  Must call after Begin()
  virtual bool RemoveIO() { return m_pProfile ? m_pProfile->Detach() : false; }

  ///Returns the source color space of the transform
  virtual icColorSpaceSignature GetSrcSpace() const;
  virtual icUInt16Number GetNumSrcSamples() const;

  ///Returns the destination color space of the transform
  virtual icColorSpaceSignature GetDstSpace() const;
  virtual icUInt16Number GetNumDstSamples() const;

  ///Checks if version 2 PCS is to be used
  virtual bool UseLegacyPCS() const { return false; }
  ///Checks if the profile is version 2
  virtual bool IsVersion2() const { return !m_pProfile || m_pProfile->m_Header.version < icVersionNumberV4; }
  
  ///Checks if the profile is to be used as input profile
  bool IsInput() const { return m_bInput; }

  virtual bool IsLateBinding() const { return false; }
  virtual IIccProfileConnectionConditions *GetProfileCC() const { return m_pProfile; }
  virtual void SetAppliedCC(IIccProfileConnectionConditions *pPCC) {}

  bool IsMCS() const { return m_nMCS!=icNoMCS; }

  bool IsExtendedPCS() const { return m_pProfile && (m_pProfile->m_Header.flags & icExtendedRangePCS) != 0; }

  bool IsAbstract() const { return m_bPcsAdjustXform || (m_pProfile && m_pProfile->m_Header.deviceClass==icSigAbstractClass); }
    
  /// The following function is for Overridden create function
  void SetParams(CIccProfile *pProfile, bool bInput, icRenderingIntent nIntent, icRenderingIntent nTagIntet,
                 bool bUseSpectralPCS, icXformInterp nInterp, CIccCreateXformHintManager *pHintManager=NULL, 
                 bool bAbsToRel=false, icMCSConnectionType nMCS=icNoMCS);

  /// Use these functions to extract the input/output curves from the xform
  virtual LPIccCurve* ExtractInputCurves()=0;
  virtual LPIccCurve* ExtractOutputCurves()=0;

  virtual bool NoClipPCS() const { return true; }

	/// Returns the profile pointer. Profile is still owned by the Xform.
	const CIccProfile* GetProfile() const { return m_pProfile; }
  
  /// Returns modifiable profile pointer. Profile is still owned by the Xform.
  CIccProfile* GetProfilePtr() const { return m_pProfile; }

	/// Returns the rendering intent being used by the Xform
	icRenderingIntent GetIntent() const { return m_nIntent; }
  icXformInterp GetInterp() const { return m_nInterp; }

  void SetSrcPCSConversion(bool bPcsConvert) { m_bSrcPcsConversion = bPcsConvert; }
  void SetDstPCSConversion(bool bPcsConvert) { m_bDstPcsConversion = bPcsConvert; }
  bool NeedAdjustPCS() { return m_bAdjustPCS; }
  bool NeedAdjustSrcPCS() { return m_bAdjustPCS && !m_bSrcPcsConversion; }
  bool NeedAdjustDstPCS() { return m_bAdjustPCS && !m_bDstPcsConversion; }
  bool LuminanceMatching() { return m_bLuminanceMatching; }

  virtual IIccProfileConnectionConditions *GetConnectionConditions() const { return m_pConnectionConditions; }

  virtual IIccCmmEnvVarLookup *GetCmmEnvVarLookup() { return m_pCmmEnvVarLookup; }

  void AttachCmmEnvVarLookup(IIccCmmEnvVarLookup* pCmmEnvVarLookup) { m_pCmmEnvVarLookup = pCmmEnvVarLookup; m_bDeleteEnvLooup = false; }

  void DetachAll();

protected:
  //Called by derived classes to initialize Base

  const icFloatNumber *CheckSrcAbs(CIccApplyXform *pApply, const icFloatNumber *Pixel) const;
  void CheckDstAbs(icFloatNumber *Pixel) const;
	void AdjustPCS(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

  virtual bool HasPerceptualHandling() { return true; }

  CIccProfile *m_pProfile;
  bool m_bOwnsProfile = true;
  bool m_bPcsAdjustXform = false;
  bool m_bInput;
  icRenderingIntent m_nIntent;
  icRenderingIntent m_nTagIntent;
  icXYZNumber m_MediaXYZ;
  icXformInterp m_nInterp;
  bool m_bUseD2BTags;
  bool m_bUseSpectralPCS;
  bool m_bAbsToRel;
  icMCSConnectionType m_nMCS;
  bool m_bLuminanceMatching;
  
  //Temporary field
  bool m_bSrcPcsConversion;
  bool m_bDstPcsConversion;

	// track PCS adjustments
	IIccAdjustPCSXform* m_pAdjustPCS;
	bool m_bAdjustPCS;
	icFloatNumber m_PCSScale[3]; // scale and offset for PCS adjustment in XYZ
	icFloatNumber m_PCSOffset[3];

  IIccProfileConnectionConditions *m_pConnectionConditions;

  bool m_bDeleteEnvLooup = true;
  IIccCmmEnvVarLookup *m_pCmmEnvVarLookup;
};


/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: Pointer to the Cmm Xform object
 **************************************************************************
 */
class ICCPROFLIB_API CIccXformPtr {
public:
  CIccXform *ptr;
};


/**
 **************************************************************************
 * Type: List Class
 * 
 * Purpose: List of CIccXformPtr which is updated on addition of Xforms
 ************************************************************************** 
 */
typedef std::list<CIccXformPtr> CIccXformList;


/**
**************************************************************************
* Type: Class
* 
* Purpose: The Apply Cmm Xform object (Allows xforms to have apply time data)
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyXform
{
  friend class CIccXform;
public:
  virtual ~CIccApplyXform();
  virtual icXformType GetXformType() const { return icXformTypeUnknown; }

  void __inline Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) { m_pXform->Apply(this, DstPixel, SrcPixel); }

  const CIccXform *GetXform() { return m_pXform; }

protected:
  icFloatNumber m_AbsLab[3];

  CIccApplyXform(CIccXform *pXform);

  const CIccXform *m_pXform;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: Pointer to the Apply Cmm Xform object
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyXformPtr {
public:
  CIccApplyXform *ptr;
};


/**
**************************************************************************
* Type: List Class
* 
* Purpose: List of CIccApplyXformPtr which is updated on addition of Apply Xforms
************************************************************************** 
*/
typedef std::list<CIccApplyXformPtr> CIccApplyXformList;


class CIccApplyPcsStep;

typedef enum {
  icPcsStepUnknown = 0,
  icPcsStepIdentity,
  icPcsStepRouteMcs,
  icPcsStepLabToXYZ,
  icPcsStepXYZToLab,
  icPcsStepLab2ToXYZ,
  icPcsStepXYZToLab2,
  icPcsStepLabToLab2,
  icPcsStepLab2ToLab,
  icPcsStepOffset,
  icPcsStepScale,
  icPcsStepMatrix,
  icPcsStepMpe,
  icPcsStepSrcMatrix,
  icPcsStepSparseMatrix,
  icPcsStepSrcSparseMatrix,
} icPcsStepType;

//forward reference to CIccXform used by CIccApplyXform
class CIccApplyPcsStep;

class ICCPROFLIB_API CIccPcsStep
{
public:
  virtual icPcsStepType GetType() { return icPcsStepUnknown; }

  virtual CIccApplyPcsStep *GetNewApply();

  virtual ~CIccPcsStep() {}
  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const=0;
  virtual icUInt16Number GetSrcChannels() const =0;
  virtual icUInt16Number GetDstChannels() const =0;

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const {return NULL;}
  virtual bool isIdentity() const { return false; }
  virtual CIccPcsStep *reduce() const { return (CIccPcsStep*)this; }
  
  virtual void dump(std::string &str) const =0;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: The Apply pcs step object (Allows pcs processing steps to have apply time data)
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyPcsStep
{
  friend class CIccPcsStep;
public:
  virtual ~CIccApplyPcsStep() {}
  virtual icPcsStepType GetXformType() const { return m_stepType; }

  void __inline Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) { m_pStep->Apply(this, DstPixel, SrcPixel); }

  const CIccPcsStep *GetStep() const { return m_pStep; }

protected:
  icPcsStepType m_stepType;

  CIccApplyPcsStep(CIccPcsStep *pStep) { m_pStep = pStep; m_stepType = icPcsStepUnknown; }

  const CIccPcsStep *m_pStep;
};


class ICCPROFLIB_API CIccPcsStepIdentity : public CIccPcsStep
{
public:
  CIccPcsStepIdentity(icUInt16Number nChannels) {m_nChannels=nChannels;}
  virtual icPcsStepType GetType() { return icPcsStepIdentity; }

  virtual ~CIccPcsStepIdentity() {}
  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const {return m_nChannels;}
  virtual icUInt16Number GetDstChannels() const {return m_nChannels;}

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const {return NULL;}
  virtual bool isIdentity() const { return true; }

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nChannels;
};

class CIccTagArray;
class CIccTagNumArray;

class ICCPROFLIB_API CIccPcsStepRouteMcs : public CIccPcsStep
{
public:
  CIccPcsStepRouteMcs(CIccTagArray *pSrcChannels, CIccTagArray *pDstChannels, CIccTagNumArray *pDefaults);
  virtual icPcsStepType GetType() { return icPcsStepRouteMcs; }

  virtual ~CIccPcsStepRouteMcs();
  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const {return m_nSrcChannels;}
  virtual icUInt16Number GetDstChannels() const {return m_nDstChannels;}

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const {return NULL;}
  virtual bool isIdentity() const ;

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nSrcChannels;
  icUInt16Number m_nDstChannels;
  int *m_Index;
  icFloatNumber *m_Defaults;
};

class ICCPROFLIB_API CIccPcsStepLabToLab2 : public CIccPcsStep
{
public:
  CIccPcsStepLabToLab2() {}
  virtual icPcsStepType GetType() { return icPcsStepLabToLab2; }

  virtual ~CIccPcsStepLabToLab2() {}
  virtual void Apply(CIccApplyPcsStep* pApply, icFloatNumber* pDst, const icFloatNumber* pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return 3; }
  virtual icUInt16Number GetDstChannels() const { return 3; }

  virtual CIccPcsStep* concat(CIccPcsStep* pNext) const;

  virtual void dump(std::string& str) const;
protected:
};


class ICCPROFLIB_API CIccPcsStepLab2ToLab : public CIccPcsStep
{
public:
  CIccPcsStepLab2ToLab() {}
  virtual icPcsStepType GetType() { return icPcsStepLab2ToLab; }

  virtual ~CIccPcsStepLab2ToLab() {}
  virtual void Apply(CIccApplyPcsStep* pApply, icFloatNumber* pDst, const icFloatNumber* pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return 3; }
  virtual icUInt16Number GetDstChannels() const { return 3; }

  virtual CIccPcsStep* concat(CIccPcsStep* pNext) const;

  virtual void dump(std::string& str) const;
protected:
};


class ICCPROFLIB_API CIccPcsLabStep : public CIccPcsStep
{
public:
  bool isSameWhite(const icFloatNumber *xyzWhite);
protected:
  icFloatNumber m_xyzWhite[3];
};

class ICCPROFLIB_API CIccPcsStepLabToXYZ : public CIccPcsLabStep
{
public:
  CIccPcsStepLabToXYZ(const icFloatNumber *xyzWhite=NULL);
  virtual icPcsStepType GetType() { return icPcsStepLabToXYZ; }

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return 3; }
  virtual icUInt16Number GetDstChannels() const { return 3; }

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;

  virtual void dump(std::string &str) const;
};

class ICCPROFLIB_API CIccPcsStepXYZToLab : public CIccPcsLabStep
{
public:
  CIccPcsStepXYZToLab(const icFloatNumber *xyzWhite=NULL);
  virtual icPcsStepType GetType() { return icPcsStepXYZToLab; }

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return 3; }
  virtual icUInt16Number GetDstChannels() const { return 3; }

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;

  virtual void dump(std::string &str) const;
};

class ICCPROFLIB_API CIccPcsStepLab2ToXYZ : public CIccPcsLabStep
{
public:
  CIccPcsStepLab2ToXYZ(const icFloatNumber *xyzWhite=NULL);
  virtual icPcsStepType GetType() { return icPcsStepLab2ToXYZ; }

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return 3; }
  virtual icUInt16Number GetDstChannels() const { return 3; }

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;

  virtual void dump(std::string &str) const;
};

class ICCPROFLIB_API CIccPcsStepXYZToLab2 : public CIccPcsLabStep
{
public:
  CIccPcsStepXYZToLab2(const icFloatNumber *xyzWhite=NULL);
  virtual icPcsStepType GetType() { return icPcsStepXYZToLab2; }

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return 3; }
  virtual icUInt16Number GetDstChannels() const { return 3; }

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;

  virtual void dump(std::string &str) const;
};

class ICCPROFLIB_API CIccPcsStepOffset : public CIccPcsStep
{
public:
  CIccPcsStepOffset(icUInt16Number nChannels);
  virtual icPcsStepType GetType() { return icPcsStepOffset; }

  virtual ~CIccPcsStepOffset();

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return m_nChannels; }
  virtual icUInt16Number GetDstChannels() const { return m_nChannels; }

  CIccPcsStepOffset *Add(const CIccPcsStepOffset *offset) const;

  icFloatNumber *data() { return m_vals; }
  const icFloatNumber *data() const { return m_vals; }

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;
  virtual bool isIdentity() const;

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nChannels;
  icFloatNumber *m_vals;

};

class CIccPcsStepMatrix;
class CIccMpeMatrix;

class ICCPROFLIB_API CIccPcsStepScale : public CIccPcsStep
{
public:
  CIccPcsStepScale(icUInt16Number nChannels);
  virtual icPcsStepType GetType() { return icPcsStepScale; }

  virtual ~CIccPcsStepScale();

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return m_nChannels; }
  virtual icUInt16Number GetDstChannels() const { return m_nChannels; }

  icFloatNumber *data() { return m_vals; }
  const icFloatNumber *data() const { return m_vals; }

  CIccPcsStepScale *Mult(const CIccPcsStepScale *scale) const;
  CIccPcsStepMatrix *Mult(const CIccPcsStepMatrix *matrix) const;
  CIccPcsStepMatrix *Mult(const CIccMpeMatrix *matrix) const;

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;
  virtual bool isIdentity() const;

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nChannels;
  icFloatNumber *m_vals;

};

class ICCPROFLIB_API CIccPcsStepMatrix : public CIccPcsStep, public CIccMatrixMath //Apply a matrix to a vector provided by the source
{
public:
  CIccPcsStepMatrix(icUInt16Number nRows, icUInt16Number nCols, bool bInitIdentity=false) : CIccMatrixMath(nRows, nCols, bInitIdentity) {}
  CIccPcsStepMatrix(const CIccPcsStepMatrix &mat) : CIccMatrixMath(mat) {}
  CIccPcsStepMatrix(const CIccMatrixMath &mat) : CIccMatrixMath(mat) {}
  virtual icPcsStepType GetType() { return icPcsStepMatrix; }

  virtual ~CIccPcsStepMatrix() {}

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const {VectorMult(pDst, pSrc); }//Must support pApply=NULL
  virtual icUInt16Number GetSrcChannels() const { return m_nCols; }
  virtual icUInt16Number GetDstChannels() const { return m_nRows; }

  CIccPcsStepMatrix *Mult(const CIccPcsStepScale *scale) const;
  CIccPcsStepMatrix *Mult(const CIccPcsStepMatrix *matrix) const;
  
  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const;
  virtual bool isIdentity() const {return isIdentityMtx();}
  virtual CIccPcsStep *reduce() const;

  virtual void dump(std::string &str) const;
};

class CIccTagMultiProcessElement;

class ICCPROFLIB_API CIccPcsStepMpe : public CIccPcsStep //Apply a matrix to a vector provided by the source
{
public:
  CIccPcsStepMpe(CIccTagMultiProcessElement *pMpe);
  virtual icPcsStepType GetType() { return icPcsStepMpe; }

  virtual ~CIccPcsStepMpe();

  virtual CIccApplyPcsStep *GetNewApply();

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const;
  virtual icUInt16Number GetDstChannels() const;

  virtual bool isIdentity() const;

  CIccMpeMatrix *GetMatrix() const;

  //Begin must be called before Apply
  bool Begin();

  virtual void dump(std::string &str) const;

protected:
  CIccTagMultiProcessElement *m_pMpe;
};

class CIccApplyTagMpe;

class ICCPROFLIB_API CIccApplyPcsStepMpe : public CIccApplyPcsStep
{
  friend class CIccPcsStepMpe;
  virtual ~CIccApplyPcsStepMpe() { delete m_pApply; }
protected:
  CIccApplyPcsStepMpe(CIccPcsStep *pStep, CIccApplyTagMpe *pApply) : CIccApplyPcsStep(pStep) { m_pApply = pApply; }

  CIccApplyTagMpe *m_pApply;
};


class ICCPROFLIB_API CIccPcsStepSrcMatrix : public CIccPcsStep //Apply a vector to the matrix provided by the source
{
public:
  CIccPcsStepSrcMatrix(icUInt16Number nRows, icUInt16Number nCols);
  virtual icPcsStepType GetType() { return icPcsStepSrcMatrix; }

  virtual ~CIccPcsStepSrcMatrix();

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return m_nCols; }
  virtual icUInt16Number GetDstChannels() const { return m_nRows; }

  icFloatNumber *data() { return m_vals;}
  const icFloatNumber *data() const { return m_vals;}

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const { return NULL; }

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nRows, m_nCols;
  icFloatNumber *m_vals;

};


class ICCPROFLIB_API CIccPcsStepSparseMatrix : public CIccPcsStep //Apply a sparse matrix to the vector provided by the source
{
public:
  CIccPcsStepSparseMatrix(icUInt16Number nRows, icUInt16Number nCols, icUInt32Number nBytesPerMatrix);
  virtual icPcsStepType GetType() { return icPcsStepSparseMatrix; }

  virtual ~CIccPcsStepSparseMatrix();

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return m_nCols; }
  virtual icUInt16Number GetDstChannels() const { return m_nRows; }

  icFloatNumber *data() { return m_vals;}
  const icFloatNumber *data() const { return m_vals;}

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const { return NULL; }

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nRows, m_nCols, m_nChannels;
  icUInt32Number m_nBytesPerMatrix;
  icFloatNumber *m_vals;

};

class ICCPROFLIB_API CIccPcsStepSrcSparseMatrix : public CIccPcsStep //Apply a vector to the matrix provided by the source
{
public:
  CIccPcsStepSrcSparseMatrix(icUInt16Number nRows, icUInt16Number nCols, icUInt16Number nChannels);
  virtual icPcsStepType GetType() { return icPcsStepSrcSparseMatrix; }

  virtual ~CIccPcsStepSrcSparseMatrix();

  virtual void Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const;
  virtual icUInt16Number GetSrcChannels() const { return m_nCols; }
  virtual icUInt16Number GetDstChannels() const { return m_nRows; }

  icFloatNumber *data() { return m_vals;}
  const icFloatNumber *data() const { return m_vals;}

  virtual CIccPcsStep *concat(CIccPcsStep *pNext) const { return NULL; }

  virtual void dump(std::string &str) const;
protected:
  icUInt16Number m_nRows, m_nCols, m_nChannels;
  icUInt32Number m_nBytesPerMatrix;
  icFloatNumber *m_vals;

};


/**
**************************************************************************
* Type: Class
* 
* Purpose: Pointer to the PcsStep object
**************************************************************************
*/
class ICCPROFLIB_API CIccPcsStepPtr {
public:
  CIccPcsStep *ptr;
};

/**
**************************************************************************
* Type: List Class
* 
* Purpose: List of CIccApplyXformPtr which is updated on addition of Apply Xforms
************************************************************************** 
*/
typedef std::list<CIccPcsStepPtr> CIccPcsStepList;


/**
**************************************************************************
* Type: Class
* 
* Purpose: The PCS Cmm Xform object
**************************************************************************
*/
class  ICCPROFLIB_API CIccPcsXform : public CIccXform
{
public:
  CIccPcsXform();
  virtual ~CIccPcsXform(); 

  virtual icXformType GetXformType() const { return icXformTypePCS; }

  icStatusCMM Connect(CIccXform *pFromXform, CIccXform *pToXform);
  icStatusCMM ConnectFirst(CIccXform* pToXform, icColorSpaceSignature srcSpace);
  icStatusCMM ConnectLast(CIccXform* pFromXform, icColorSpaceSignature dstSpace);

  virtual icStatusCMM Begin() { return icCmmStatOk; }

  virtual CIccApplyXform *GetNewApply(icStatusCMM &status);  //Must be called after Begin

  virtual void Apply(CIccApplyXform *pXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

  ///Returns the source color space of the transform
  virtual icColorSpaceSignature GetSrcSpace() const { return m_srcSpace; }
  virtual icUInt16Number GetNumSrcSamples() const { return m_nSrcSamples; }

  ///Returns the destination color space of the transform
  virtual icColorSpaceSignature GetDstSpace() const { return m_dstSpace; }
  virtual icUInt16Number GetNumDstSamples() const { return m_nDstSamples; }

  ///Checks if version 2 PCS is to be used
  virtual bool UseLegacyPCS() const { return false; }
  ///Checks if the profile is version 2
  virtual bool IsVersion2() const { return false; }

  /// Use these functions to extract the input/output curves from the xform
  virtual LPIccCurve* ExtractInputCurves() { return NULL; }
  virtual LPIccCurve* ExtractOutputCurves() { return NULL; }

  icUInt16Number MaxChannels();

  static CIccPcsStepMatrix *rangeMap(const icSpectralRange &srcRange, const icSpectralRange &dstRange);

protected:

  icStatusCMM Optimize();

  void pushRouteMcs(CIccTagArray *pSrcChannels, CIccTagArray *pDstChannels, CIccTagNumArray *pDefaults);
  void pushLab2ToLab();
  void pushLabToLab2();
  void pushLab2ToXyz(IIccProfileConnectionConditions *pPCC);
  void pushXyzToLab2(IIccProfileConnectionConditions *pPCC);
  void pushLabToXyz(IIccProfileConnectionConditions *pPCC);
  void pushXyzToLab(IIccProfileConnectionConditions *pPCC);
  void pushXyzToXyzIn();
  void pushXyzInToXyz();
  void pushXyzToXyzLum(IIccProfileConnectionConditions *pPCC);
  void pushXyzLumToXyz(IIccProfileConnectionConditions *pPCC);
  void pushScale3(icFloatNumber v1, icFloatNumber v2, icFloatNumber v3);
  void pushOffset3(icFloatNumber v1, icFloatNumber v2, icFloatNumber v3, bool bConvertIntXyzOffset=true);
  void pushRef2Xyz(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc);
  void pushSpecToRange(const icSpectralRange &srcRange, const icSpectralRange &dstRange);
  void pushApplyIllum(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc);
  void pushRad2Xyz(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc, bool bAbsoluteCIEColorimetry=false);
  icStatusCMM pushBiRef2Xyz(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc);
  icStatusCMM pushBiRef2Ref(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc);
  icStatusCMM pushBiRef2Rad(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc);
  void pushScale(icUInt16Number n, const icFloatNumber *vals);
  void pushMatrix(icUInt16Number nRows, icUInt16Number nCols, const icFloatNumber *vals);
  void pushMatrixTransverse(icUInt16Number nRows, icUInt16Number nCols, icFloatNumber *vals);
  icStatusCMM pushXYZConvert(CIccXform *pSrcXform, CIccXform *pDstXform);
  void pushXYZNormalize(IIccProfileConnectionConditions *pPcc, const icSpectralRange &srcRange, const icSpectralRange &dstRange);


  icColorSpaceSignature m_srcSpace;
  icUInt16Number m_nSrcSamples;

  icColorSpaceSignature m_dstSpace;
  icUInt16Number m_nDstSamples;

  CIccPcsStepList *m_list;
};


/**
**************************************************************************
* Type: Class
* 
* Purpose: Pointer to the Apply PCS Step object
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyPcsStepPtr {
public:
  CIccApplyPcsStep *ptr;
};


/**
**************************************************************************
* Type: List Class
* 
* Purpose: List of CIccApplyXformPtr which is updated on addition of Apply Xforms
************************************************************************** 
*/
typedef std::list<CIccApplyPcsStepPtr> CIccApplyPcsStepList;


/**
**************************************************************************
* Type: Class
* 
* Purpose: The PCS Apply Cmm Xform object (Allows xforms to have apply time data)
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyPcsXform : public CIccApplyXform
{
  friend class CIccPcsXform;
public:
  virtual ~CIccApplyPcsXform();

  virtual icXformType GetXformType() const { return icXformTypePCS; }
  
  void AppendApplyStep(CIccApplyPcsStep *pStep);

protected:
  CIccApplyPcsXform(CIccXform *pXform);
  bool Init();

  CIccApplyPcsStepList *m_list;

  icFloatNumber *m_temp1;
  icFloatNumber *m_temp2;
};


/**
**************************************************************************
* Type: Class
* 
* Purpose: This is the general Monochrome Xform (uses a grayTRCTag)
* 
**************************************************************************
*/
class ICCPROFLIB_API CIccXformMonochrome : public CIccXform
{
public:
	CIccXformMonochrome();
	virtual ~CIccXformMonochrome();

	virtual icXformType GetXformType() const { return icXformTypeMonochrome; }

	virtual icStatusCMM Begin();
	virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

	virtual LPIccCurve* ExtractInputCurves();
	virtual LPIccCurve* ExtractOutputCurves();

protected:

  virtual bool HasPerceptualHandling() { return false; }

	CIccCurve *m_Curve;
	CIccCurve *GetCurve(icSignature sig) const;
	CIccCurve *GetInvCurve(icSignature sig) const;

	bool m_bFreeCurve;
	/// used only when applying the xform
	LPIccCurve m_ApplyCurvePtr;
};

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: This is the general Matrix-TRC Xform
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccXformMatrixTRC : public CIccXform
{
public:
  CIccXformMatrixTRC();
  virtual ~CIccXformMatrixTRC();

  virtual icXformType GetXformType() const { return icXformTypeMatrixTRC; }

  virtual icStatusCMM Begin();
  virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;
  
  virtual LPIccCurve* ExtractInputCurves();
  virtual LPIccCurve* ExtractOutputCurves();

  icFloatNumber* GetMatrix() { return &m_e[0]; }

protected:

  virtual bool HasPerceptualHandling() { return false; }

  icFloatNumber m_e[9];
  CIccCurve *m_Curve[3];
  CIccCurve *GetCurve(icSignature sig) const;
  CIccCurve *GetInvCurve(icSignature sig) const;

  CIccTagXYZ *GetColumn(icSignature sig) const;
  bool m_bFreeCurve;
  /// used only when applying the xform
  const LPIccCurve* m_ApplyCurvePtr;
};


/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: This is the general 3D-LUT Xform
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccXform3DLut : public CIccXform
{
public:
  CIccXform3DLut(CIccTag *pTag);
  virtual ~CIccXform3DLut();

  virtual icXformType GetXformType() const { return icXformType3DLut; }

  virtual icStatusCMM Begin();
  virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

  virtual bool UseLegacyPCS() const { return m_pTag->UseLegacyPCS(); }

  virtual LPIccCurve* ExtractInputCurves();
  virtual LPIccCurve* ExtractOutputCurves();
protected:

  const CIccMBB *m_pTag;

  /// Pointers to data in m_pTag, used only for applying the xform
  const LPIccCurve* m_ApplyCurvePtrA;
  const LPIccCurve* m_ApplyCurvePtrB;
  const LPIccCurve* m_ApplyCurvePtrM;
  const CIccMatrix* m_ApplyMatrixPtr;
};


/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: This is the general 4D-LUT Xform
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccXform4DLut : public CIccXform
{
public:
  CIccXform4DLut(CIccTag *pTag);
  virtual ~CIccXform4DLut();

  virtual icXformType GetXformType() const { return icXformType4DLut; }

  virtual icStatusCMM Begin();
  virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

  virtual bool UseLegacyPCS() const { return m_pTag->UseLegacyPCS(); }

  virtual LPIccCurve* ExtractInputCurves();
  virtual LPIccCurve* ExtractOutputCurves();
protected:
  const CIccMBB *m_pTag;

  /// Pointers to data in m_pTag, used only for applying the xform
  const LPIccCurve* m_ApplyCurvePtrA;
  const LPIccCurve* m_ApplyCurvePtrB;
  const LPIccCurve* m_ApplyCurvePtrM;
  const CIccMatrix* m_ApplyMatrixPtr;
};

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: This is the general ND-LUT Xform
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccXformNDLut : public CIccXform
{
public:
  CIccXformNDLut(CIccTag *pTag);
  virtual ~CIccXformNDLut();

  virtual icXformType GetXformType() const { return icXformTypeNDLut; }

  virtual icStatusCMM Begin();

  virtual CIccApplyXform* GetNewApply(icStatusCMM& status);  //Must be called after Begin

  virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

  virtual bool UseLegacyPCS() const { return m_pTag->UseLegacyPCS(); }

  virtual LPIccCurve* ExtractInputCurves();
  virtual LPIccCurve* ExtractOutputCurves();
protected:
  const CIccMBB *m_pTag;
  int m_nNumInput;

  /// Pointers to data in m_pTag, used only for applying the xform
  const LPIccCurve* m_ApplyCurvePtrA;
  const LPIccCurve* m_ApplyCurvePtrB;
  const LPIccCurve* m_ApplyCurvePtrM;
  const CIccMatrix* m_ApplyMatrixPtr;
};


class ICCPROFLIB_API CIccApplyNDLutXform : public CIccApplyXform
{
  friend class CIccXformNDLut;
public:
  CIccApplyNDLutXform(CIccXformNDLut* pXform, CIccApplyCLUT* pApply);
  virtual ~CIccApplyNDLutXform();

protected:
  CIccApplyCLUT* m_pApply;
};


/**
 **************************************************************************
 * Type: Enum
 * 
 * Purpose: Defines the interface to be used when applying Named Color
 *  Profiles.
 * 
 **************************************************************************
 */
typedef enum {
  icApplyPixel2Pixel = 0,
  icApplyNamed2Pixel = 1,
  icApplyPixel2Named = 2,
  icApplyNamed2Named = 3,
} icApplyInterface;

//Forward reference for CIccXformNamedColor
class CIccArrayNamedColor;

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: This is the general Xform for Named Color Profiles.
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccXformNamedColor : public CIccXform
{
public:
  CIccXformNamedColor(CIccTag *pTag, icColorSpaceSignature csPcs, icColorSpaceSignature csDevice, 
                      icSpectralColorSignature csSpectralPcs=icSigNoSpectralData,
                      const icSpectralRange *pSpectralRange = NULL,
                      const icSpectralRange *pBiSPectralRange = NULL);
  virtual ~CIccXformNamedColor();

  virtual icXformType GetXformType() const { return icXformTypeNamedColor; }

  virtual icStatusCMM Begin();

  ///Returns the type of interface that will be applied
  icApplyInterface GetInterface() const {return m_nApplyInterface;}

  virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const {} 

  icStatusCMM Apply(CIccApplyXform *pApplyXform, icChar *DstColorName, const icFloatNumber *SrcPixel) const;
  icStatusCMM Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icChar *SrcColorName, icFloatNumber tint=1.0f) const;

  virtual bool UseLegacyPCS() const { return m_pTag ? m_pTag->UseLegacyPCS() : false; }

  icStatusCMM SetSrcSpace(icColorSpaceSignature nSrcSpace);
  icStatusCMM SetDestSpace(icColorSpaceSignature nDestSpace);

  ///Returns the source color space of the transform
  icColorSpaceSignature GetSrcSpace() const { return m_nSrcSpace; }
  ///Returns the destination color space of the transform
  icColorSpaceSignature GetDstSpace() const { return m_nDestSpace; }

  ///Checks if the source space of the transform is PCS
  bool IsSrcPCS() const;
  ///Checks if the destination space of the transform is PCS
  bool IsDestPCS() const;


  virtual LPIccCurve* ExtractInputCurves() {return NULL;}
  virtual LPIccCurve* ExtractOutputCurves() {return NULL;}

protected:

  virtual bool HasPerceptualHandling() { return false; }

  CIccTagNamedColor2 *m_pTag;
  CIccArrayNamedColor *m_pArray;
  icApplyInterface m_nApplyInterface;
  icColorSpaceSignature m_nSrcSpace;
  icColorSpaceSignature m_nDestSpace;
};


/**
**************************************************************************
* Type: Class
* 
* Purpose: This is the general Xform for Multi Processing Elements.
* 
**************************************************************************
*/
class ICCPROFLIB_API CIccXformMpe : public CIccXform
{
public:
  CIccXformMpe(CIccTag *pTag);
  virtual ~CIccXformMpe();

  virtual icXformType GetXformType() const { return icXformTypeMpe; }

  ///Note: The returned CIccXform will own the profile.
  static CIccXform *Create(CIccProfile *pProfile, bool bInput=true, icRenderingIntent nIntent=icUnknownIntent, 
    icXformInterp nInterp=icInterpLinear, icXformLutType nLutType=icXformLutColor, CIccCreateXformHintManager *pHintManager=NULL);

  virtual icStatusCMM Begin();

  virtual CIccApplyXform *GetNewApply(icStatusCMM &status);
  virtual void Apply(CIccApplyXform *pApplyXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const;

  virtual bool UseLegacyPCS() const { return false; }
  virtual LPIccCurve* ExtractInputCurves() {return NULL;}
  virtual LPIccCurve* ExtractOutputCurves() {return NULL;}

  virtual bool NoClipPCS() const { return true; }

  virtual bool IsLateBinding() const;
  virtual bool IsLateBindingReflectance() const;
  virtual IIccProfileConnectionConditions *GetConnectionConditions() const;
  virtual void SetAppliedCC(IIccProfileConnectionConditions *pPCC);


protected:
  CIccTagMultiProcessElement *m_pTag;
  bool m_bUsingAcs;
  IIccProfileConnectionConditions *m_pAppliedPCC;
  bool m_bDeleteAppliedPCC;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: The Apply general MPE Xform object (Allows xforms to have apply time data)
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyXformMpe : public CIccApplyXform
{
  friend class CIccXformMpe;
public:
  virtual ~CIccApplyXformMpe();
  virtual icXformType GetXformType() const { return icXformTypeMpe; }

protected:
  CIccApplyXformMpe(CIccXformMpe *pXform);

  CIccApplyTagMpe *m_pApply;
};


/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: Independant PCS class to do PCS based calculations.
 *  This is a class for managing PCS colorspace transformations.  There
 *  are two important categories V2 <-> V4, and Lab <-> XYZ.
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccPCSUtil
{
public:
  CIccPCSUtil();
  virtual ~CIccPCSUtil() {}

  static void LabToXyz(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip=false);
  static void XyzToLab(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip=false);
  static void Lab2ToXyz(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip=false);
  static void XyzToLab2(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip=false);
  static icFloatNumber NegClip(icFloatNumber v);
  static icFloatNumber UnitClip(icFloatNumber v);

  static void Lab2ToLab4(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoclip=false);
  static void Lab4ToLab2(icFloatNumber *Dst, const icFloatNumber *Src);
};


/**
 **************************************************************************
  Color data passed to/from the CMM is encoded as floating point numbers ranging from 0.0 to 1.0
  Often data is encoded using other ranges.  The icFloatColorEncoding enum is used by the
  ToInternalEncoding() and FromInternalEncoding() functions to convert to/from the internal
  encoding.  The valid encoding transforms for the following color space signatures are given
  below.

  'CMYK', 'RGB ', 'GRAY', 'CMY ', 'Luv ', 'YCbr', 'Yxy ', 'HSV ', 'HLS ', 'gamt'
    icEncodePercent: 0.0 <= value <= 100.0
    icEncodeFloat: 0.0 <= value <= 1.0
    icEncode8Bit: 0.0 <= value <= 255
    icEncode16Bit: 0.0 <= value <= 65535
    icEncode16BitV2: 0.0 <= value <= 65535

  'XCLR'
    icEncodeValue: (if X>=3) 0.0 <= L <= 100.0; -128.0 <= a,b <= 127.0 others 0.0 <= value <= 1.0
    icEncodePercent: 0.0 <= value <= 100.0
    icEncodeFloat: 0.0 <= value <= 1.0
    icEncode8Bit: 0.0 <= value <= 255
    icEncode16Bit: 0.0 <= value <= 65535
    icEncode16BitV2: 0.0 <= value <= 65535

  'Lab '
    icEncodeValue: 0.0 <= L <= 100.0; -128.0 <= a,b <= 127.0
    icEncodeFloat: 0.0 <= L,a,b <= 1.0 - ICC PCS encoding (See ICC Specification)
    icEncode8BIt: ICC 8 bit Lab Encoding - See ICC Specification
    icEncode16Bit: ICC 16 bit V4 Lab Encoding - See ICC Specification
    icEncode16BitV2: ICC 16 bit V2 Lab Encoding - See ICC Specification

  'XYZ '
    icEncodeValue: 0.0 <= X,Y,Z < 1.999969482421875
    icEncodePercent: 0.0 <= X,Y,Z < 199.9969482421875
    icEncodeFloat: 0.0 <= L,a,b <= 1.0 - ICC PCS encoding (See ICC Specification
    icEncode16Bit: ICC 16 bit XYZ Encoding - (icU1Fixed15) See ICC Specification
    icEncode16BitV2: ICC 16 bit XYZ Encoding - (icU1Fixed15) See ICC Specification
 **************************************************************************
*/

typedef enum
{
  icEncodeValue=0,
  icEncodePercent,
  icEncodeUnitFloat,
  icEncodeFloat,
  icEncode8Bit,
  icEncode16Bit,
  icEncode16BitV2,
  icEncodeUnknown,
} icFloatColorEncoding;

//Forward Reference of CIccCmm for CIccCmmApply
class CIccCmm;

/**
**************************************************************************
* Type: Class 
* 
* Purpose: Defines a class that provides and interface for applying pixel
*  transformations through a CMM.  Multiply CIccCmmApply objects can use
*  a single CIccCmm Object.
* 
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyCmm
{
  friend class CIccCmm;
public:
  virtual ~CIccApplyCmm();

  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel);

  //Make sure that when DstPixel==SrcPixel the sizeof DstPixel is less than size of SrcPixel
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels);

  void AppendApplyXform(CIccApplyXform *pApplyXform);

  CIccCmm *GetCmm() { return m_pCmm; }

  bool InitPixel();

protected:
  CIccApplyCmm(CIccCmm *pCmm);

  CIccApplyXformList *m_Xforms;
  CIccCmm *m_pCmm;

  icFloatNumber *m_Pixel;
  icFloatNumber *m_Pixel2;
};

class IXformIterator
{
public:
  IXformIterator() {}
  virtual ~IXformIterator() {}

  virtual void iterate(const CIccXform* pxform) = 0;
};

/**
 **************************************************************************
 * Type: Class 
 * 
 * Purpose: Defines a class that allows one or more profiles to be applied
 *  in order that they are Added.
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccCmm 
{
  friend class CIccApplyCmm;
public:
  CIccCmm(icColorSpaceSignature nSrcSpace=icSigUnknownData,
          icColorSpaceSignature nDestSpace=icSigUnknownData,
          bool bFirstInput=true);
  virtual ~CIccCmm();

  //virtual CIccPCS *GetPCS() { return new CIccPCS(); }

  ///Must make at least one call to some form of AddXform() before calling Begin()
  virtual icStatusCMM AddXform(const icChar *szProfilePath, 
                               icRenderingIntent nIntent=icUnknownIntent,
                               icXformInterp nInterp=icInterpLinear, 
                               IIccProfileConnectionConditions *pPcc=NULL,
                               icXformLutType nLutType=icXformLutColor,
                               bool bUseD2BxB2DxTags=true, 
                               CIccCreateXformHintManager *pHintManager=NULL,
                               bool bUseSubProfile=false);
  virtual icStatusCMM AddXform(icUInt8Number *pProfileMem, icUInt32Number nProfileLen,
                               icRenderingIntent nIntent=icUnknownIntent, 
                               icXformInterp nInterp=icInterpLinear,
                               IIccProfileConnectionConditions *pPcc=NULL,
                               icXformLutType nLutType=icXformLutColor,
                               bool bUseD2BxB2DxTags =true,
                               CIccCreateXformHintManager *pHintManager=NULL,
                               bool bUseSubProfile=false);
  virtual icStatusCMM AddXform(CIccProfile *pProfile, 
                               icRenderingIntent nIntent=icUnknownIntent,
                               icXformInterp nInterp=icInterpLinear, 
                               IIccProfileConnectionConditions *pPcc=NULL,
                               icXformLutType nLutType=icXformLutColor,
                               bool bUseD2BxB2DxTags =true,
                               CIccCreateXformHintManager *pHintManager=NULL);  //Note: profile will be owned by the CMM
  virtual icStatusCMM AddXform(CIccProfile &Profile, 
                               icRenderingIntent nIntent=icUnknownIntent,
                               icXformInterp nInterp=icInterpLinear, 
                               IIccProfileConnectionConditions *pPcc=NULL,
                               icXformLutType nLutType=icXformLutColor,
                               bool bUseD2BxB2DxTags =true,
                               CIccCreateXformHintManager *pHintManager=NULL);  //Note the profile will be copied
  virtual icStatusCMM AddXform(CIccProfile *pProfile,
                               CIccTag *pXformTag,
                               icRenderingIntent nIntent = icUnknownIntent,
                               icXformInterp nInterp = icInterpLinear,
                               IIccProfileConnectionConditions *pPcc = NULL,
                               bool bUseD2BxB2DxTags =false,
                               CIccCreateXformHintManager *pHintManager = NULL);  //Note: profile will be owned by the CMM
  virtual icStatusCMM AddXform(CIccXform* pXform); //note pXform will be owned by the CMM

  //The Begin function should be called before Apply or GetNewApplyCmm()
  virtual icStatusCMM Begin(bool bAllocNewApply=true, bool bUsePcsConversion=false);

  //Get an additional Apply cmm object to apply pixels with.  The Apply object should be deleted by the caller.
  virtual CIccApplyCmm *GetNewApplyCmm(icStatusCMM &status); 

  virtual CIccApplyCmm *GetApply() { return m_pApply; }

  //The following apply functions should only be called if using Begin(true);
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel);
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels);

  //Call to Detach and remove all pending IO objects attached to the profiles used by the CMM. Should be called only after Begin()
  virtual icStatusCMM RemoveAllIO();

  ///Returns the number of profiles/transforms added 
  virtual icUInt32Number GetNumXforms() const;

  ///Returns the number of profiles/transforms added 
  bool HasXformsOfType(icXformType nXformType) const;

  CIccXform* GetFirstXform() const;
  CIccXform* GetLastXform() const;

  virtual void IterateXforms(IXformIterator* pIterater) const;

  ///Returns the source color space
  icColorSpaceSignature GetSourceSpace() const { return m_nSrcSpace; }
  ///Returns the destination color space
  icColorSpaceSignature GetDestSpace() const { return m_nDestSpace; }
    ///Returns the color space of the last profile added
  icColorSpaceSignature GetLastSpace() const { return m_nLastSpace; }
  ///Returns the parent's color space of the last profile added
  icColorSpaceSignature GetLastParentSpace() const { return m_nLastParentSpace; }

  ///Returns the rendering intent of the last profile added
  icRenderingIntent GetLastIntent() const { return m_nLastIntent; }

  bool IsLastOutPut() const { return !m_bLastInput; }

  ///Returns the number of samples in the source color space
  icUInt16Number GetSourceSamples() const {return (icUInt16Number)icGetSpaceSamples(m_nSrcSpace);}
  ///Returns the number of samples in the destination color space
  icUInt16Number GetDestSamples() const {return (icUInt16Number)icGetSpaceSamples(m_nDestSpace);}

  ///Checks if this is a valid CMM object
  bool Valid() const { return m_bValid; }

  //Function to convert check if Internal representation of 'gamt' color is in gamut.
  static bool IsInGamut(icFloatNumber *pData);

  ///Functions for converting to Internal representation of pixel colors
  static icStatusCMM ToInternalEncoding(icColorSpaceSignature nSpace, icFloatColorEncoding nEncode, 
                                        icFloatNumber *pInternal, const icFloatNumber *pData, bool bClip=true);
  static icStatusCMM ToInternalEncoding(icColorSpaceSignature nSpace, icFloatNumber *pInternal, 
                                        const icUInt8Number *pData);
  static icStatusCMM ToInternalEncoding(icColorSpaceSignature nSpace, icFloatNumber *pInternal, 
                                        const icUInt16Number *pData);
  icStatusCMM ToInternalEncoding(icFloatNumber *pInternal, const icUInt8Number *pData) {return ToInternalEncoding(m_nSrcSpace, pInternal, pData);}
  icStatusCMM ToInternalEncoding(icFloatNumber *pInternal, const icUInt16Number *pData) {return ToInternalEncoding(m_nSrcSpace, pInternal, pData);}

  
  ///Functions for converting from Internal representation of pixel colors
  static icStatusCMM FromInternalEncoding(icColorSpaceSignature nSpace, icFloatColorEncoding nEncode, 
                                          icFloatNumber *pData, const icFloatNumber *pInternal, bool bClip=true);
  static icStatusCMM FromInternalEncoding(icColorSpaceSignature nSpace, icUInt8Number *pData, 
                                          const icFloatNumber *pInternal);
  static icStatusCMM FromInternalEncoding(icColorSpaceSignature nSpace, icUInt16Number *pData, 
                                          const icFloatNumber *pInternal);
  icStatusCMM FromInternalEncoding(icUInt8Number *pData, icFloatNumber *pInternal) {return FromInternalEncoding(m_nDestSpace, pData, pInternal);}
  icStatusCMM FromInternalEncoding(icUInt16Number *pData, icFloatNumber *pInternal) {return FromInternalEncoding(m_nDestSpace, pData, pInternal);}

  static const icChar *GetFloatColorEncoding(icFloatColorEncoding val);
  static icFloatColorEncoding GetFloatColorEncoding(const icChar* val);

  virtual icColorSpaceSignature GetFirstXformSource();
  virtual icColorSpaceSignature GetLastXformDest();

  static const icChar *GetStatusText(icStatusCMM stat);

protected:
  void SetLateBindingCC();

  icStatusCMM CheckPCSRangeConversions();
  icStatusCMM CheckPCSConnections(bool bUsePCSConversions=false);

  CIccApplyCmm *m_pApply;

  bool m_bValid;

  bool m_bLastInput;
  icColorSpaceSignature m_nSrcSpace;
  icColorSpaceSignature m_nDestSpace;

  icColorSpaceSignature m_nLastSpace;
  icColorSpaceSignature m_nLastParentSpace;
  icRenderingIntent m_nLastIntent;

  CIccXformList *m_Xforms;
};

//Forward Class for CIccApplyNamedColorCmm
class CIccNamedColorCmm;
/**
**************************************************************************
* Type: Class 
* 
* Purpose: Defines a class that provides and interface for applying pixel
*  transformations through a Named Color CMM.  Multiply CIccApplyNamedColorCmm
*  objects can refer to a single CIccNamedColorCmm Object.
* 
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyNamedColorCmm : public CIccApplyCmm
{
  friend class CIccNamedColorCmm;
public:
  virtual ~CIccApplyNamedColorCmm();

  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel);

  //Make sure that when DstPixel==SrcPixel the sizeof DstPixel is less than size of SrcPixel
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels);

  ///Define 4 apply interfaces that are used depending upon the source and destination xforms
  virtual icStatusCMM Apply(icChar* DstColorName, const icFloatNumber *SrcPixel);
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icChar *SrcColorName, icFloatNumber tint=1.0);
  virtual icStatusCMM Apply(icChar* DstColorName, const icChar *SrcColorName, icFloatNumber tint=1.0);

protected:
  CIccApplyNamedColorCmm(CIccNamedColorCmm *pCmm);
};

/**
 **************************************************************************
 * Type: Class
 * 
 * Purpose: A Slower Named Color Profile compatible CMM
 * 
 **************************************************************************
 */
class ICCPROFLIB_API CIccNamedColorCmm : public CIccCmm
{
  friend class CIccApplyNamedColorCmm;
public:
  ///nSrcSpace cannot be icSigUnknownData if first profile is named color
  CIccNamedColorCmm(icColorSpaceSignature nSrcSpace=icSigUnknownData, 
                    icColorSpaceSignature nDestSpace=icSigUnknownData,
                    bool bFirstInput=true);
  virtual ~CIccNamedColorCmm();

  ///Must make at least one call to some form of AddXform() before calling Begin()
  virtual icStatusCMM AddXform(const icChar *szProfilePath, 
                               icRenderingIntent nIntent=icUnknownIntent,
                               icXformInterp nInterp=icInterpLinear, 
                               IIccProfileConnectionConditions *pPcc=NULL,
                               icXformLutType nLutType=icXformLutColor,
                               bool bUseD2BxB2DxTags=true,
                               CIccCreateXformHintManager *pHintManager=NULL,
                               bool bUseSubProfile=false);
  virtual icStatusCMM AddXform(CIccProfile *pProfile, 
                               icRenderingIntent nIntent=icUnknownIntent,
                               icXformInterp nInterp=icInterpLinear, 
                               IIccProfileConnectionConditions *pPcc=NULL,
                               icXformLutType nLutType=icXformLutColor,
                               bool bUseD2BxB2DxTags =true,
                               CIccCreateXformHintManager *pHintManager=NULL);  //Note: profile will be owned by the CMM

  ///Must be called before calling Apply() or GetNewApply()
  //The Begin function should be called before Apply or GetNewApplyCmm()
  virtual icStatusCMM Begin(bool bAllocNewApply=true, bool bUsePcsConversion=false);

  virtual CIccApplyCmm *GetNewApplyCmm(icStatusCMM &status); 


  //The following apply functions should only be called if using Begin(true);
  icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) { return CIccCmm::Apply(DstPixel, SrcPixel); }
  icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels) { return CIccCmm::Apply(DstPixel, SrcPixel, nPixels); }
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icChar *SrcColorName, icFloatNumber tint=1.0);
  virtual icStatusCMM Apply(icChar* DstColorName, const icFloatNumber *SrcPixel);
  virtual icStatusCMM Apply(icChar* DstColorName, const icChar *SrcColorName, icFloatNumber tint=1.0);

  ///Returns the type of interface that will be applied
  icApplyInterface GetInterface() const {return m_nApplyInterface;}

  icStatusCMM SetLastXformDest(icColorSpaceSignature nDestSpace); 

protected:
  icApplyInterface m_nApplyInterface;
};

template<class T>
class ICCPROFLIB_API CIccMruPixel
{
public:
  CIccMruPixel() { pPixelData = NULL; pNext = NULL; }

  T *pPixelData;
  CIccMruPixel *pNext;
};

typedef CIccMruPixel<icFloatNumber> CIccMruPixelFloat;
typedef CIccMruPixel<icUInt8Number> CIccMruPixel8;
typedef CIccMruPixel<icUInt16Number> CIccMruPixel16;

/**
**************************************************************************
* Type: Class
*
* Purpose: Defines a class that provides and interface for caching
* application pf pixel transformations through a CMM.
*
**************************************************************************
*/
template <class T>
class ICCPROFLIB_API CIccMruCache
{
public:
  static CIccMruCache<T> *NewMruCache(icUInt16Number nSrcSamples, icUInt16Number nDstSamples, icUInt16Number nCacheSize = 4);

  virtual ~CIccMruCache();

  virtual bool Apply(T *DstPixel, const T *SrcPixel);
  virtual void Update(T *DstPixel);

protected:
  CIccMruCache();
  bool Init(icUInt16Number nSrcSamples, icUInt16Number nDstSamples, icUInt16Number nCacheSize = 4);

  icUInt16Number m_nCacheSize;

  T *m_pixelData;

  CIccMruPixel<T> *m_pFirst;
  CIccMruPixel<T> *m_cache;

  icUInt16Number m_nNumPixel;

  icUInt32Number m_nTotalSamples;
  icUInt32Number m_nSrcSamples;

  icUInt32Number m_nSrcSize;
  icUInt32Number m_nDstSize;
};

typedef CIccMruCache<icFloatNumber> CIccMruCacheFloat;
typedef CIccMruCache<icUInt8Number> CIccMruCache8;
typedef CIccMruCache<icUInt16Number> CIccMruCache16;

// forward class CIccMruCmm used by CIccApplyMruCmm
class CIccMruCmm;
/**
**************************************************************************
* Type: Class 
* 
* Purpose: Defines a class that provides and interface for applying pixel
*  transformations through a CMM.  Multiply CIccCmmApply objects can use
*  a single CIccCmm Object.
* 
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyMruCmm : public CIccApplyCmm
{
  friend class CIccMruCmm;
public:
  virtual ~CIccApplyMruCmm();

  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel);

  //Make sure that when DstPixel==SrcPixel the sizeof DstPixel is greater than size of SrcPixel
  virtual icStatusCMM Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels);

protected:
  CIccApplyMruCmm(CIccMruCmm *pCmm);

  bool Init(CIccCmm *pCachedCmm, icUInt16Number nCacheSize);

  CIccCmm *m_pCachedCmm;
  CIccMruCacheFloat *m_pCache;
};

/**
**************************************************************************
* Type: Class
* 
* Purpose: A CMM decorator class that provides limited caching of results
* 
**************************************************************************
*/
class ICCPROFLIB_API CIccMruCmm : public CIccCmm
{
  friend class CIccApplyMruCmm;
private:
  CIccMruCmm();
public:
  virtual ~CIccMruCmm();

  //This is the function used to create a new CIccMruCmm.  The pCmm must be valid and its Begin() already called.
  static CIccMruCmm* Attach(CIccCmm *pCmm, icUInt8Number nCacheSize=6, bool bDeleteCmm=true);  //The returned object will own pCmm, and pCmm is deleted on failure.

  //override AddXform/Begin functions to return bad status.
  virtual icStatusCMM AddXform(const icChar *szProfilePath, icRenderingIntent nIntent=icUnknownIntent,
    icXformInterp nInterp=icInterpLinear, icXformLutType nLutType=icXformLutColor,
    bool bUseMpeTags=true, CIccCreateXformHintManager *pHintManager=NULL) { return icCmmStatBad; }
  virtual icStatusCMM AddXform(icUInt8Number *pProfileMem, icUInt32Number nProfileLen,
    icRenderingIntent nIntent=icUnknownIntent, icXformInterp nInterp=icInterpLinear,
    icXformLutType nLutType=icXformLutColor, bool bUseMpeTags=true, CIccCreateXformHintManager *pHintManager=NULL)  { return icCmmStatBad; }
  virtual icStatusCMM AddXform(CIccProfile *pProfile, icRenderingIntent nIntent=icUnknownIntent,
    icXformInterp nInterp=icInterpLinear, icXformLutType nLutType=icXformLutColor,
    bool bUseMpeTags=true, CIccCreateXformHintManager *pHintManager=NULL)  { return icCmmStatBad; }
  virtual icStatusCMM AddXform(CIccProfile &Profile, icRenderingIntent nIntent=icUnknownIntent,
    icXformInterp nInterp=icInterpLinear, icXformLutType nLutType=icXformLutColor,
    bool bUseMpeTags=true, CIccCreateXformHintManager *pHintManager=NULL) { return icCmmStatBad; }

  virtual CIccApplyCmm *GetNewApplyCmm(icStatusCMM &status); 

  //Forward calls to attached CMM
  virtual icStatusCMM RemoveAllIO() { return m_pCmm->RemoveAllIO(); }
  //virtual CIccPCS *GetPCS() { return m_pCmm->GetPCS(); }
  virtual icUInt32Number GetNumXforms() const { return m_pCmm->GetNumXforms(); }

  virtual icColorSpaceSignature GetFirstXformSource() { return m_pCmm->GetFirstXformSource(); }
  virtual icColorSpaceSignature GetLastXformDest() { return m_pCmm->GetLastXformDest(); }

protected:
  CIccCmm *m_pCmm;
  icUInt16Number m_nCacheSize;
  bool m_bDeleteCmm;

};

#endif //__cplusplus

#if defined(__cplusplus) && defined(USEICCDEVNAMESPACE)
}; //namespace iccDEV
#endif

#endif // !defined(_ICCCMM_H)
