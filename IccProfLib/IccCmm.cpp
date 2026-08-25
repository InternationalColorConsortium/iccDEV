/** @file
    File:       IccCmm.cpp

    Contains:   Implementation of the CIccCmm class.

    Version:    V1

    Copyright:  See ICC Software License
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
// -Added support for Monochrome ICC profile apply by Rohit Patil 12-03-2008
// -Integrated changes for PCS adjustment by George Pawle 12-09-2008
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#endif

#include <cmath>
#include <new>
#include "IccXformFactory.h"
#include "IccTag.h"
#include "IccMpeBasic.h"
#include "IccArrayBasic.h"
#include "IccStructBasic.h"
#include "IccIO.h"
#include "IccApplyBPC.h"
#include "IccSparseMatrix.h"
#include "IccEncoding.h"
#include "IccMatrixMath.h"
#include <cassert>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

////
// Useful Macros
////

static __inline bool IsSpaceSpectralPCS(icUInt32Number sig)
{
  sig = icGetColorSpaceType(sig);

  return sig==icSigReflectanceSpectralPcsData ||
         sig==icSigTransmissionSpectralPcsData ||
         sig==icSigRadiantSpectralPcsData ||
         sig==icSigBiDirReflectanceSpectralPcsData ||
         sig==icSigSparseMatrixSpectralPcsData;
}

static __inline bool IsValidXformIntent(icRenderingIntent intent)
{
  return intent >= icPerceptual && intent <= icAbsoluteColorimetric;
}

#define IsSpaceDevicePCS(x) ((x)==icSigDevXYZData || (x)==icSigDevLabData)
#define IsSpaceColorimetricPCS(x) ((x)==icSigXYZPcsData || (x)==icSigLabPcsData)
#define IsSpaceNChannel(x) (icGetColorSpaceType(x)==icSigNChannelData)
#define IsSpacePCS(x) (IsSpaceColorimetricPCS(x) || IsSpaceSpectralPCS(x))
#define IsSpaceMCS(x) (icGetColorSpaceType(x)==icSigSrcMCSChannelData)
#define IsSpaceCMYK(x) ((x)==icSigCmykData || (x)==icSig4colorData)

#define IsNChannelCompat(x, y) ((IsSpaceNChannel(x) && icNumColorSpaceChannels(x)==icGetSpaceSamples(y)) || (IsSpaceNChannel(y) && icNumColorSpaceChannels(y)==icGetSpaceSamples(x)))

#define IsCompatSpace(x, y) ((x)==(y) || (IsSpacePCS(x) && IsSpacePCS(y)) || (IsSpaceMCS(x) && IsSpaceMCS(y))/* || (IsSpaceCMYK(x) && IsSpaceCMYK(y))*/)

#define IsCompatSpacePCS(x, y) (((x)==icSigDevXYZData && (y)==icSigXYZData) || ((x)==icSigXYZData && (y)==icSigDevXYZData) || \
                                ((x)==icSigDevLabData && (y)==icSigLabData) || ((x)==icSigLabData && (y)==icSigDevLabData))

#define ICCPCSSTEPDUMPFMT ICCMTXSTEPDUMPFMT



/**
 **************************************************************************
 * Name: CIccPCS::UnitClip
 * 
 * Purpose: 
 *  Convert a double to an icUInt16Number with clipping
 **************************************************************************
 */
icFloatNumber CIccPCSUtil::UnitClip(icFloatNumber v)
{
  if (v<0)
    v = 0;
  if (v>1.0)
    v = 1.0;

  return v;
}

/**
 **************************************************************************
 * Name: CIccPCS::NegClip
 * 
 * Purpose: 
 *  Convert a double to an icUInt16Number with clipping of negative numbers
 **************************************************************************
 */
icFloatNumber CIccPCSUtil::NegClip(icFloatNumber v)
{
  if (v<0)
    v=0;
  
  return v;
}

/**
 **************************************************************************
 * Name: CIccPCS::LabToXyz
 * 
 * Purpose: 
 *  Convert Lab to XYZ
 **************************************************************************
 */
void CIccPCSUtil::LabToXyz(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip)
{
  icFloatNumber Lab[3];

  memcpy(&Lab,Src,sizeof(Lab));

  icLabFromPcs(Lab);

  icLabtoXYZ(Lab);

  icXyzToPcs(Lab);

  if (!bNoClip) {
    Dst[0] = UnitClip(Lab[0]);
    Dst[1] = UnitClip(Lab[1]);
    Dst[2] = UnitClip(Lab[2]);
  }
  else {
    Dst[0] = Lab[0];
    Dst[1] = Lab[1];
    Dst[2] = Lab[2];
  }
}


/**
 **************************************************************************
 * Name: CIccPCS::XyzToLab
 * 
 * Purpose: 
 *  Convert XYZ to Lab
 **************************************************************************
 */
void CIccPCSUtil::XyzToLab(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip)
{
  icFloatNumber XYZ[3];


  if (!bNoClip) {
    XYZ[0] = UnitClip(Src[0]);
    XYZ[1] = UnitClip(Src[1]);
    XYZ[2] = UnitClip(Src[2]);
  }
  else {
    XYZ[0] = Src[0];
    XYZ[1] = Src[1];
    XYZ[2] = Src[2];
  }
 
  icXyzFromPcs(XYZ);

  icXYZtoLab(XYZ);

  icLabToPcs(XYZ);

  if (!bNoClip) {
    Dst[0] = UnitClip(XYZ[0]);
    Dst[1] = UnitClip(XYZ[1]);
    Dst[2] = UnitClip(XYZ[2]);
  }
  else {
    Dst[0] = XYZ[0];
    Dst[1] = XYZ[1];
    Dst[2] = XYZ[2];
  }
}


/**
 **************************************************************************
 * Name: CIccPCS::Lab2ToXyz
 * 
 * Purpose:
 *  Convert version 2 Lab to XYZ
 **************************************************************************
 */
void CIccPCSUtil::Lab2ToXyz(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip)
{
  Lab2ToLab4(Dst, Src, bNoClip);
  LabToXyz(Dst, Dst, bNoClip);
}


/**
 **************************************************************************
 * Name: CIccPCS::XyzToLab2
 * 
 * Purpose: 
 *  Convert XYZ to version 2 Lab
 **************************************************************************
 */
void CIccPCSUtil::XyzToLab2(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip)
{
  XyzToLab(Dst, Src, bNoClip);
  Lab4ToLab2(Dst, Dst);
}


/**
 **************************************************************************
 * Name: CIccPCS::Lab2ToLab4
 * 
 * Purpose: 
 *  Convert version 2 Lab to version 4 Lab
 **************************************************************************
 */
void CIccPCSUtil::Lab2ToLab4(icFloatNumber *Dst, const icFloatNumber *Src, bool bNoClip)
{
  if (bNoClip) {
    Dst[0] = (icFloatNumber)(Src[0] * 65535.0f / 65280.0f);
    Dst[1] = (icFloatNumber)(Src[1] * 65535.0f / 65280.0f);
    Dst[2] = (icFloatNumber)(Src[2] * 65535.0f / 65280.0f);
  }
  else {
    Dst[0] = UnitClip((icFloatNumber)(Src[0] * 65535.0f / 65280.0f));
    Dst[1] = UnitClip((icFloatNumber)(Src[1] * 65535.0f / 65280.0f));
    Dst[2] = UnitClip((icFloatNumber)(Src[2] * 65535.0f / 65280.0f));
  }
}

/**
 **************************************************************************
 * Name: CIccPCS::Lab4ToLab2
 * 
 * Purpose: 
 *  Convert version 4 Lab to version 2 Lab
 **************************************************************************
 */
void CIccPCSUtil::Lab4ToLab2(icFloatNumber *Dst, const icFloatNumber *Src)
{
  Dst[0] = (icFloatNumber)(Src[0] * 65280.0f / 65535.0f);
  Dst[1] = (icFloatNumber)(Src[1] * 65280.0f / 65535.0f);
  Dst[2] = (icFloatNumber)(Src[2] * 65280.0f / 65535.0f);
}


/**
**************************************************************************
* Name: CIccCreateXformHintManager::CIccCreateXformHintManager
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccCreateXformHintManager::~CIccCreateXformHintManager()
{
  if (m_pList) {
    IIccCreateXformHintList::iterator i;

    for (i=m_pList->begin(); i!=m_pList->end(); i++) {
      delete i->ptr;
    }

    delete m_pList;
  }
}

/**
**************************************************************************
* Name: CIccCreateXformHintManager::AddHint
* 
* Purpose:
*  Adds and owns the passed named hint to it's list.
* 
* Args: 
*  pHint = pointer to the hint object to be added
* 
* Return: 
*  true = hint added to the list
*  false = hint not added
**************************************************************************
*/
bool CIccCreateXformHintManager::AddHint(IIccCreateXformHint* pHint)
{
  if (!m_pList) {
    m_pList = new (std::nothrow) IIccCreateXformHintList;
    if (!m_pList) {
      delete pHint; // don't leave the pointer hanging
      return false;
    }
  }

  if (pHint) {
    if (GetHint(pHint->GetHintType())) {
      delete pHint;
      return false;
    }
    IIccCreateXformHintPtr Hint;
    Hint.ptr = pHint;
    m_pList->push_back(Hint);
    return true;
  }

  return false;
}

/**
**************************************************************************
* Name: CIccCreateXformHintManager::DeleteHint
* 
* Purpose:
*  Deletes the object referenced by the passed named hint pointer 
*		and removes it from the list.
* 
* Args: 
*  pHint = pointer to the hint object to be deleted
* 
* Return: 
*  true = hint found and deleted
*  false = hint not found
**************************************************************************
*/
bool CIccCreateXformHintManager::DeleteHint(IIccCreateXformHint* pHint)
{
	if (m_pList && pHint) {
		IIccCreateXformHintList::iterator i;
		for (i=m_pList->begin(); i!=m_pList->end(); i++) {
			if (i->ptr) {
				if (i->ptr == pHint) {
					delete pHint;
					pHint = NULL;
					m_pList->erase(i);
					return true;
				}
			}
		}
	}

	return false;
}

/**
**************************************************************************
* Name: CIccCreateXformHintManager::GetHint
* 
* Purpose:
*  Finds and returns a pointer to the named hint.
* 
* Args: 
*  hintName = name of the desired hint
* 
* Return: 
*  Appropriate IIccCreateXformHint pointer
**************************************************************************
*/
IIccCreateXformHint* CIccCreateXformHintManager::GetHint(const char* hintName)
{
	IIccCreateXformHint* pHint=NULL;
	
	if (m_pList) {
		IIccCreateXformHintList::iterator i;
		for (i=m_pList->begin(); i!=m_pList->end(); i++) {
			if (i->ptr) {
				if (!strcmp(i->ptr->GetHintType(), hintName)) {
					pHint = i->ptr;
					break;
				}
			}
		}
	}

	return pHint;
}

/**
 **************************************************************************
 * Name: CIccXform::CIccXform
 * 
 * Purpose: 
 *  Constructor
 **************************************************************************
 */
CIccXform::CIccXform() 
{
  m_pProfile = NULL;
  m_bOwnsProfile = true;
  m_bInput = true;
  m_nIntent = icUnknownIntent;
  m_pAdjustPCS = NULL;
  m_bAdjustPCS = false;
  m_bAbsToRel = false;
  m_nMCS = icNoMCS;
  m_bUseSpectralPCS = false;
  m_bSrcPcsConversion = true;
  m_bDstPcsConversion = true;
  m_pConnectionConditions = NULL;
  m_bDeleteEnvLooup = true;
  m_pCmmEnvVarLookup = NULL;
  m_nTagIntent = icPerceptual;
  m_MediaXYZ = {};
  m_nInterp = icInterpLinear;
  m_bUseD2BTags = false;
  m_bLuminanceMatching = false;
  m_PCSOffset[0] = m_PCSOffset[1] = m_PCSOffset[2] = 0;
}


/**
 **************************************************************************
 * Name: CIccXform::~CIccXform
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccXform::~CIccXform()
{
  if (m_bOwnsProfile)
    delete m_pProfile;

  delete m_pAdjustPCS;

  if (m_bDeleteEnvLooup)
    delete m_pCmmEnvVarLookup;

}


void CIccXform::DetachAll()
{
  m_pProfile = NULL;
  m_bOwnsProfile = true;
  m_pConnectionConditions = NULL;
}

/**
 **************************************************************************
 * Name: CIccXform::Create
 * 
 * Purpose:
 *  This is a static Creation function that creates derived CIccXform objects and
 *  initializes them.
 * 
 * Args: 
 *  pProfile = pointer to a CIccProfile object that will be owned by the transform.  This object will
 *   be destroyed when the returned CIccXform object is destroyed.  The means that the CIccProfile
 *   object needs to be allocated on the heap.
 *  bInput = flag to indicate whether to use the input or output side of the profile,
 *  nIntent = the rendering intent to apply to the profile,   
 *  nInterp = the interpolation algorithm to use for N-D luts.
 *  nLutType = selection of which transform lut to use
 *  bUseD2BxB2DxTags = flag to indicate the use MPE flags if available
 *  pHintManager = pointer to object that contains xform creation hints
 * 
 * Return: 
 *  A suitable pXform object
 **************************************************************************
 */
CIccXform *CIccXform::Create(CIccProfile *pProfile,
                             bool bInput/* =true */,
                             icRenderingIntent nIntent/* =icUnknownIntent */, 
														 icXformInterp nInterp/* =icInterpLinear */, 
                             IIccProfileConnectionConditions *pPcc/*=NULL*/,
                             icXformLutType nLutType/* =icXformLutColor */, 
														 bool bUseD2BTags/* =true */, CIccCreateXformHintManager *pHintManager/* =NULL */,
                             bool bOwnsProfile/* =true */)
{
  // Ownership contract: on success pProfile is handed to the new xform via
  // SetParams() and freed when that xform is destroyed.  On every FAILURE path
  // below we dispose of pProfile ourselves -- but only when we own it.
  // bOwnsProfile is true for the usual callers that give their profile to
  // Create, and false for callers that merely lend a profile they still own (a
  // profile borrowed from a live xform).  Guarding each delete with it keeps the
  // owned case leak-free (#1304/#1305) without double-freeing a borrowed profile.
  CIccXform *rv = NULL;
  icRenderingIntent nTagIntent = nIntent;
  bool bUseSpectralPCS = false;
  bool bAbsToRel = false;
  //bool bRelToAbs = false;     // set, but never used
  icMCSConnectionType nMCS = icNoMCS;
  icXformLutType nUseLutType = nLutType;
  bool bUseColorimeticTags = true;

  if (nLutType == icXformLutSpectral) {
    nUseLutType = icXformLutColor;
    bUseD2BTags = true;
    bUseColorimeticTags = false;
  }
  else if (nLutType == icXformLutColorimetric) {
    nUseLutType = icXformLutColor;
  }

  if (pProfile->m_Header.deviceClass==icSigColorEncodingClass) {
    CIccProfile *pEncProfile;
    if (icConvertEncodingProfile(pEncProfile, pProfile)!=icEncConvertOk) {
      if (bOwnsProfile)
        delete pProfile;
      return NULL;
    }
    // The encoding profile is replaced by its converted form; free the original
    // only when we own it, then continue with (and later hand off) the copy.
    if (bOwnsProfile)
      delete pProfile;
    pProfile = pEncProfile;
  }
  if (pProfile->m_Header.deviceClass==icSigLinkClass/* && nIntent==icAbsoluteColorimetric*/) {
    nIntent = icPerceptual;
  }

  if (nTagIntent == icUnknownIntent)
    nTagIntent = icPerceptual;

  if (!IsValidXformIntent(nTagIntent)) {
    if (bOwnsProfile)
      delete pProfile;
    return NULL;
  }

// TODO -  there are too many layers in the switch, making it very difficult to understand the code
// it should be broken into functions for improved readability
  switch (nUseLutType) {
    case icXformLutColor:
		  //The search CMM uses shared profiles that only have access to the IO object during transform creation. The following
			//addresses a bug with the Begin() being called after the IO object is released resulting in the absolute intent
			//not having access to the media white point tag.  The following pre-loads it so that it is available.
      if (nIntent == icAbsoluteColorimetric && (pProfile->m_Header.pcs == icSigLabData || pProfile->m_Header.pcs == icSigXYZData)) {
        //load media white point tag into profile object so we have it if we need it
        pProfile->FindTag(icSigMediaWhitePointTag); //we don't really need the return value. just need it associated with the profile
      }
      if (bInput) {
        CIccTag *pTag = NULL;
        // Spectral-only profiles have no AToBx tag to fall back to; their MPE pipeline
        // lives in DToBx tags regardless of how the caller set bUseD2BTags. Opening
        // that path here lets icXformLutColor + a spectral source profile resolve
        // without the caller having to set useD2BxB2Dx explicitly.
        //
        // "Spectral-only" is the !pcs test, not the bare spectralPCS test that
        // stood here: ICC.2:2023 lets a profile carry an independent colorimetric
        // PCS as well, and for those the caller's opt-out has to be honoured.
        // CIccCmm::AddXform already draws the line that way -- its nDstSpace
        // selection reads (bUseD2BxB2DxTags || !m_Header.pcs) -- so without this
        // term the two disagree: AddXform records the 3-sample XYZ connection
        // while Create hands back a DToBx transform emitting spectralPCS samples,
        // and CIccCmm::Begin() then rejects the chain with icCmmStatBadSpaceLink.
        // Measured on SixChanInputRef (AToB3 6->3, DToB3 6->36) with the D2B
        // opt-out intents 11 and 13 (#1982).
        if (bUseD2BTags || (pProfile->m_Header.spectralPCS && !pProfile->m_Header.pcs)) {
          if (nLutType != icXformLutColorimetric &&
              (pProfile->m_Header.spectralPCS || pProfile->m_Header.version >= icVersionNumberV5)) {
            pTag = pProfile->FindTag(icSigDToB0Tag + nTagIntent);

            if (!pTag && nTagIntent == icAbsoluteColorimetric) {
              pTag = pProfile->FindTag(icSigDToB1Tag);
              if (pTag)
                nTagIntent = icRelativeColorimetric;
            }
            else if (!pTag && nTagIntent == icRelativeColorimetric) {
              pTag = pProfile->FindTag(icSigDToB3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }

            if (pTag && pProfile->m_Header.spectralPCS)
              bUseSpectralPCS = true;

            if (!pTag) {
              pTag = pProfile->FindTag(icSigDToB0Tag);
            }
            if (!pTag) {
              pTag = pProfile->FindTag(icSigDToB1Tag);
            }
            if (!pTag) {
              pTag = pProfile->FindTag(icSigDToB3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }
          }
          else if (pProfile->m_Header.version < icVersionNumberV5) {
            pTag = pProfile->FindTag(icSigDToB0Tag + nTagIntent);

            if (!pTag && nTagIntent ==icAbsoluteColorimetric) {
              pTag = pProfile->FindTag(icSigDToB1Tag);
              if (pTag)
                nTagIntent = icRelativeColorimetric;
            }

            //Apparently Using DtoB0 is not prescribed here by the v4 ICC Specification
            if (!pTag && pProfile->m_Header.version >= icVersionNumberV5) {
              pTag = pProfile->FindTag(icSigDToB0Tag);
            }
          }
        }

        if (bUseColorimeticTags) {
          if (!pTag) {
            pTag = pProfile->FindTag(icSigAToB0Tag + nTagIntent);
          }

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigAToB1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
          else if (!pTag && nTagIntent == icRelativeColorimetric) {
            pTag = pProfile->FindTag(icSigAToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigAToB0Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigAToB1Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigAToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }
        }

        //Unsupported elements cause fall back behavior
        if (pTag && !pTag->IsSupported())
          pTag = NULL;

        if (!pTag) {
          if (pProfile->m_Header.colorSpace == icSigRgbData) {
            rv = CIccXformCreator::CreateXform(icXformTypeMatrixTRC, NULL, pHintManager);
          }
          else if (pProfile->m_Header.colorSpace == icSigGrayData) {
            rv = CIccXformCreator::CreateXform(icXformTypeMonochrome, NULL, pHintManager);
          }
          else {
            if (bOwnsProfile)
              delete pProfile;
            return NULL;
          }
        }
        else if (pTag->GetType()==icSigMultiProcessElementType) {
          rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
        }
        else {
          switch(pProfile->m_Header.colorSpace) {
            case icSigXYZData:
            case icSigLabData:
            case icSigLuvData:
            case icSigYCbCrData:
            case icSigYxyData:
            case icSigRgbData:
            case icSigHsvData:
            case icSigHlsData:
            case icSigCmyData:
            case icSig3colorData:
              rv = CIccXformCreator::CreateXform(icXformType3DLut, pTag, pHintManager);
              break;

            case icSigCmykData:
            case icSig4colorData:
              rv = CIccXformCreator::CreateXform(icXformType4DLut, pTag, pHintManager);
              break;

            default:
              rv = CIccXformCreator::CreateXform(icXformTypeNDLut, pTag, pHintManager);
              break;
          }
        }
      }
      else {
        CIccTag *pTag = NULL;

        if (nLutType == icXformLutColorimetric && pProfile->m_Header.version >= icVersionNumberV5) {
          bUseD2BTags = false;
        }

        // Spectral-only destination profiles only carry BToDx tags; let icXformLutColor
        // resolve them without requiring the caller to set useD2BxB2Dx.
        //
        // Same !pcs correction as the bInput branch above, and it has to land in
        // the same commit: the two mis-selections currently cancel out.  A chain
        // whose source and destination are both dual-PCS resolves today because
        // both ends silently take the spectral route, so fixing only the input
        // side leaves the destination on BToDx and turns a working link into
        // icCmmStatUnsupportedPcsLink.  Measured with SixChanInputRef into
        // SixChanCameraRef under the opt-out intents (#1982).
        if (bUseD2BTags || (nLutType != icXformLutColorimetric &&
                            pProfile->m_Header.spectralPCS && !pProfile->m_Header.pcs)) {
          pTag = pProfile->FindTag(icSigBToD0Tag + nTagIntent);

          //Additional precedence not prescribed by the v4 ICC Specification
          if (!pTag && pProfile->m_Header.version >= icVersionNumberV5) {
            pTag = pProfile->FindTag(icSigBToD0Tag);

            if (!pTag) {
              pTag = pProfile->FindTag(icSigBToD1Tag);
              if (pTag) {
                nTagIntent = icRelativeColorimetric;
                //if (nTagIntent==icAbsoluteColorimetric)
                //  bRelToAbs = true; // value never used!
              }
            }

            if (!pTag) {
              pTag = pProfile->FindTag(icSigBToD3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }
          }

          //Unsupported elements cause fall back behavior
          if (pTag && !pTag->IsSupported())
            pTag = NULL;

          if (pTag)
            bUseSpectralPCS = true;

          if (!pTag && nTagIntent == icAbsoluteColorimetric && pProfile->m_Header.version < icVersionNumberV5) {
            pTag = pProfile->FindTag(icSigBToD1Tag);

            //Unsupported elements cause fall back behavior
            if (pTag && !pTag->IsSupported())
              pTag = NULL;

            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
        }

        if (bUseColorimeticTags) {

          if (!pTag) {
            pTag = pProfile->FindTag(icSigBToA0Tag + nTagIntent);
          }

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBToA1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigBToA0Tag);
          }

          //Additional precedence not prescribed by the v4 ICC Specification
          if (!pTag && pProfile->m_Header.version >= icVersionNumberV5) {

            pTag = pProfile->FindTag(icSigBToA1Tag);
            if (pTag) {
              nTagIntent = icRelativeColorimetric;
              //if (nTagIntent == icAbsoluteColorimetric)
              //  bRelToAbs = true; // value never used!
            }

            if (!pTag) {
              pTag = pProfile->FindTag(icSigBToA3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }
          }
        }

        if (!pTag) {
          if (pProfile->m_Header.colorSpace == icSigRgbData) {
            rv = CIccXformCreator::CreateXform(icXformTypeMatrixTRC, pTag, pHintManager);
          }
          else if (pProfile->m_Header.colorSpace == icSigGrayData) {
            rv = CIccXformCreator::CreateXform(icXformTypeMonochrome, NULL, pHintManager);
          }
          else {
            if (bOwnsProfile)
              delete pProfile;
            return NULL;
          }
        }
        else if (pTag->GetType()==icSigMultiProcessElementType) {
          rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
        }
        else {
          switch(pProfile->m_Header.pcs) {
            case icSigXYZData:
            case icSigLabData:
              rv = CIccXformCreator::CreateXform(icXformType3DLut, pTag, pHintManager);
              break;

          default:
            break;
          }
        }
      }
      break;

    case icXformLutNamedColor:
      {
        CIccTag *pTag = pProfile->FindTag(icSigNamedColor2Tag);
        if (!pTag) {
          if (bOwnsProfile)
            delete pProfile;
          return NULL;
        }

        // If the caller pre-attached a NamedColor hint (e.g. to set
        // nOverprintType), honor it and only fill in the header-derived
        // color-space fields it would not know.  Otherwise allocate a
        // fresh hint with header-derived defaults and the standard
        // icNamedColorOverWhite overprint.
        CIccCreateNamedColorXformHint* pCallerHint =
            pHintManager ? (CIccCreateNamedColorXformHint*)
              pHintManager->GetHint("CIccCreateNamedColorXformHint")
                         : nullptr;

        CIccCreateNamedColorXformHint* pNamedColorHint = pCallerHint;
        if (!pNamedColorHint) {
          pNamedColorHint = new (std::nothrow) CIccCreateNamedColorXformHint();
          if (!pNamedColorHint) {
            if (bOwnsProfile)
              delete pProfile;
            return NULL;
          }
        }

        pNamedColorHint->csPcs = pProfile->m_Header.pcs;
        pNamedColorHint->csDevice = pProfile->m_Header.colorSpace;
        pNamedColorHint->csSpectralPcs = pProfile->m_Header.spectralPCS;
        pNamedColorHint->spectralRange = pProfile->m_Header.spectralRange;
        pNamedColorHint->biSpectralRange = pProfile->m_Header.biSpectralRange;

        if (pHintManager) {
          if (!pCallerHint)
            pHintManager->AddHint(pNamedColorHint);
          rv = CIccXformCreator::CreateXform(icXformTypeNamedColor, pTag, pHintManager);
        }
        else {
          CIccCreateXformHintManager HintManager;
          HintManager.AddHint(pNamedColorHint);
          rv = CIccXformCreator::CreateXform(icXformTypeNamedColor, pTag, &HintManager);
        }

        if (pProfile->m_Header.spectralPCS)
          bUseSpectralPCS = true;
      }
      break;

    case icXformLutPreview:
      {
        bInput = false;
        CIccTag *pTag = pProfile->FindTag(icSigPreview0Tag + nTagIntent);
        if (!pTag) {
          pTag = pProfile->FindTag(icSigPreview0Tag);
        }
        if (!pTag) {
          if (bOwnsProfile)
            delete pProfile;
          return NULL;
        }
        else {
          switch(pProfile->m_Header.pcs) {
            case icSigXYZData:
            case icSigLabData:
              rv = CIccXformCreator::CreateXform(icXformType3DLut, pTag, pHintManager);
              break;

            default:
              break;
          }
        }
      }
      break;

    case icXformLutGamut:
      {
        bInput = false;
        CIccTag *pTag = pProfile->FindTag(icSigGamutTag);
        if (!pTag) {
          if (bOwnsProfile)
            delete pProfile;
          return NULL;
        }
        else {
          switch(pProfile->m_Header.pcs) {
            case icSigXYZData:
            case icSigLabData:
              rv = CIccXformCreator::CreateXform(icXformType3DLut, pTag, pHintManager);
              break;

            default:
              break;
          }
        }
      }
      break;

    case icXformLutBRDFParam:
    {
      // get the correct tag first

      CIccTag *pTag = NULL;
      if (bUseD2BTags) {
        if (pProfile->m_Header.spectralPCS) {

          pTag = pProfile->FindTag(icSigBrdfSpectralParameter0Tag + nTagIntent);

          if (pTag)
            bUseSpectralPCS = true;
        }
      }
      else
      {
        pTag = pProfile->FindTag(icSigBrdfColorimetricParameter0Tag + nTagIntent);

        //Unsupported elements cause fall back behavior
        if (pTag && !pTag->IsSupported())
          pTag = NULL;
      }

      // now extract the correct part from the structure
      CIccStructBRDF* pStructTag = dynamic_cast<CIccStructBRDF*>(pTag);

      if (pStructTag != NULL)
      {
        CIccTag *pTag2 = NULL;

          switch (nLutType) {
            case icXformLutBRDFParam:
              pTag2 = pStructTag->GetElem(icSigBrdfTransformMbr);
              break;
            default:
              // can't get here, get rid of warning
              break;
          }
          if (pTag2)
            rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag2, pHintManager);
      }
    }
    break;

    case icXformLutBRDFDirect:
    {
      // get the correct tag first

      CIccTag *pTag = NULL;
      if (bUseD2BTags) {
        if (pProfile->m_Header.spectralPCS || pProfile->m_Header.version >= icVersionNumberV5) {

          pTag = pProfile->FindTag(icSigBRDFDToB0Tag + nTagIntent);

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBRDFDToB1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
          else if (!pTag && nTagIntent == icRelativeColorimetric) {
            pTag = pProfile->FindTag(icSigBRDFDToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }

          if (pTag && pProfile->m_Header.spectralPCS)
            bUseSpectralPCS = true;

          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFDToB0Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFDToB1Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFDToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }
        }
      }
      else
      {
        pTag = pProfile->FindTag(icSigBRDFAToB0Tag + nTagIntent);
      }

      //Unsupported elements cause fall back behavior
      if (pTag && !pTag->IsSupported())
        pTag = NULL;

      if (pTag != NULL)
      {
        rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
      }
    }
    break;

    case icXformLutBRDFMcsParam:
    {
      CIccTag *pTag = NULL;
      if (pProfile->m_Header.deviceClass == icSigMultiplexVisualizationClass) {
        bInput = true;
        nMCS = icFromMCS;

        if (pProfile->m_Header.spectralPCS) {
          pTag = pProfile->FindTag(icSigBRDFMToS0Tag + nTagIntent);

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBRDFMToS1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
          else if (!pTag && nTagIntent != icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBRDFMToS3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFMToS0Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFMToS1Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFMToS3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }

          if (pTag)
            bUseSpectralPCS = true;
        }
        if (!pTag && pProfile->m_Header.pcs != 0) {
          pTag = pProfile->FindTag(icSigBRDFMToB0Tag + nTagIntent);

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBRDFMToB1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
          else if (!pTag && nTagIntent != icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBRDFMToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }
         if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFMToB0Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFMToB1Tag);
          }
          if (!pTag) {
            pTag = pProfile->FindTag(icSigBRDFMToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }
        }

        //Unsupported elements cause fall back behavior
        if (pTag && !pTag->IsSupported())
          pTag = NULL;
  
        //Unsupported elements cause fall back behavior
        if (pTag && !pTag->IsSupported())
          pTag = NULL;
      }
      if (pTag && pProfile->m_Header.mcs) {
        rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
      }
      else
        rv = NULL;
    }
    break;

    case icXformLutMCS:
      {
        CIccTag *pTag = NULL;
        if (pProfile->m_Header.deviceClass==icSigMultiplexIdentificationClass ||
            pProfile->m_Header.deviceClass==icSigInputClass) {
          bInput = true;
          nMCS = icToMCS;
          pTag = pProfile->FindTag(icSigAToM0Tag);
        }
        else if (pProfile->m_Header.deviceClass==icSigMultiplexVisualizationClass || 
                 pProfile->m_Header.deviceClass==icSigOutputClass) {
          bInput = true;
          nMCS = icFromMCS;

          if (pProfile->m_Header.spectralPCS) {
            pTag = pProfile->FindTag(icSigMToS0Tag + nTagIntent);

            if (!pTag && nTagIntent == icAbsoluteColorimetric) {
              pTag = pProfile->FindTag(icSigMToS1Tag);
              if (pTag)
                nTagIntent = icRelativeColorimetric;
            }
            else if (!pTag && nTagIntent != icAbsoluteColorimetric) {
              pTag = pProfile->FindTag(icSigMToS3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }

            if (!pTag) {
              pTag = pProfile->FindTag(icSigMToS0Tag);
            }
            if (!pTag) {
              pTag = pProfile->FindTag(icSigMToS1Tag);
            }
            if (!pTag) {
              pTag = pProfile->FindTag(icSigMToS3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }

            if (pTag)
              bUseSpectralPCS = true;
          }
          if (!pTag && pProfile->m_Header.pcs!=0) {
            pTag = pProfile->FindTag(icSigMToB0Tag + nTagIntent);

            if (!pTag && nTagIntent == icAbsoluteColorimetric) {
              pTag = pProfile->FindTag(icSigMToB1Tag);
              if (pTag)
                nTagIntent = icRelativeColorimetric;
            }
            else if (!pTag && nTagIntent != icAbsoluteColorimetric) {
              pTag = pProfile->FindTag(icSigMToB3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }

            if (!pTag) {
              pTag = pProfile->FindTag(icSigMToB0Tag);
            }
            if (!pTag) {
              pTag = pProfile->FindTag(icSigMToB1Tag);
            }
            if (!pTag) {
              pTag = pProfile->FindTag(icSigMToB3Tag);
              if (pTag) {
                nTagIntent = icAbsoluteColorimetric;
                bAbsToRel = true;
              }
            }
          }

          //Unsupported elements cause fall back behavior
          if (pTag && !pTag->IsSupported())
            pTag = NULL;
        }
        else if (pProfile->m_Header.deviceClass==icSigMultiplexLinkClass) {
          bInput = false;
          nMCS = icFromMCS;
          pTag = pProfile->FindTag(icSigMToA0Tag);
        }
        if (pTag && pProfile->m_Header.mcs) {
          rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
        }
        else
          rv = NULL;
      }
      break;
      
    default:
        // error unexpected transform type
      break;

  }

  if (rv) {
    if (pPcc)
      rv->m_pConnectionConditions = pPcc;
    else
      rv->m_pConnectionConditions = pProfile;

    rv->SetParams(pProfile, bInput, nIntent, nTagIntent, bUseSpectralPCS, nInterp, pHintManager, bAbsToRel, nMCS);

    // The icXformLutGamut case above forced bInput false to walk the gamt tag
    // in its stored B-to-A direction.  Record that this is a gamut xform so the
    // reported destination stays icSigGamutData rather than the device space.
    if (nLutType == icXformLutGamut)
      rv->SetGamutXform();
  }
  else if (bOwnsProfile) {
    // No xform was produced. pProfile never reached SetParams(), so free it here
    // when we own it (the caller's profile, or the encoding-converted replacement
    // built above) so it doesn't leak. A borrowed profile (bOwnsProfile==false)
    // is left for its real owner to free.
    delete pProfile;
  }

  return rv;
}

/**
**************************************************************************
* Name: CIccXform::Create
*
* Purpose:
*  This is a static Creation function that creates derived CIccXform objects and
*  initializes them.
*
* Args:
*  pProfile = pointer to a CIccProfile object that will be owned by the transform.  This object will
*   be destroyed when the returned CIccXform object is destroyed.  The means that the CIccProfile
*   object needs to be allocated on the heap.
*  pTag = tag that specifies transform to use.  It is assumed to use colorimetric PCS and have a lut type of icXformLutColor.
*  bInput = flag to indicate whether to use the input or output side of the profile,
*  nIntent = the rendering intent to apply to the profile,
*  nInterp = the interpolation algorithm to use for N-D luts.
*  pPcc = pointer to profile connection conditions to associate with transform
*  pHintManager = pointer to object that contains xform creation hints
*
* Return:
*  A suitable pXform object
**************************************************************************
*/
CIccXform *CIccXform::Create(CIccProfile *pProfile,
                             CIccTag *pTag,
                             bool bInput/* =true */,
                             icRenderingIntent nIntent/* =icUnknownIntent */,
                             icXformInterp nInterp/* =icInterpLinear */,
                             IIccProfileConnectionConditions *pPcc/*=NULL*/,
                             bool bUseSpectralPCS/* =false*/,
                             CIccCreateXformHintManager *pHintManager/* =NULL */,
                             bool bOwnsProfile/* =true */)
{
  // Ownership contract (same as the no-tag Create overload): on success pProfile
  // is handed to the new xform via SetParams(); on every failure path we free it
  // ourselves, but only when bOwnsProfile is true.  Before #1308 this overload
  // returned NULL on failure without freeing pProfile, leaking the caller's
  // owned profile; the guarded deletes below close that leak while leaving a
  // borrowed profile (bOwnsProfile==false) for its real owner.
  CIccXform *rv = NULL;
  icRenderingIntent nTagIntent = nIntent;
  bool bAbsToRel = false;
  icMCSConnectionType nMCS = icNoMCS;

  if (pProfile->m_Header.deviceClass == icSigColorEncodingClass) {
    if (bOwnsProfile)
      delete pProfile;
    return NULL;
  }

  if (pProfile->m_Header.deviceClass == icSigLinkClass/* && nIntent==icAbsoluteColorimetric*/) {
    nIntent = icPerceptual;
  }

  if (nTagIntent == icUnknownIntent)
    nTagIntent = icPerceptual;

  //Unsupported elements cause fall back behavior
  if (pTag == NULL) {
    if (bOwnsProfile)
      delete pProfile;
    return NULL;
  }
  if (pTag && !pTag->IsSupported()) {
    if (bOwnsProfile)
      delete pProfile;
    return NULL;
  }

  if (bInput) {
    if (pTag->GetType() == icSigMultiProcessElementType) {
      rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
    }
    else {
      switch (pProfile->m_Header.colorSpace) {
      case icSigXYZData:
      case icSigLabData:
      case icSigLuvData:
      case icSigYCbCrData:
      case icSigYxyData:
      case icSigRgbData:
      case icSigHsvData:
      case icSigHlsData:
      case icSigCmyData:
      case icSig3colorData:
        rv = CIccXformCreator::CreateXform(icXformType3DLut, pTag, pHintManager);
        break;

      case icSigCmykData:
      case icSig4colorData:
        rv = CIccXformCreator::CreateXform(icXformType4DLut, pTag, pHintManager);
        break;

      default:
        rv = CIccXformCreator::CreateXform(icXformTypeNDLut, pTag, pHintManager);
        break;
      }
    }
  }
  else {
    if (pTag->GetType() == icSigMultiProcessElementType) {
      rv = CIccXformCreator::CreateXform(icXformTypeMpe, pTag, pHintManager);
    }
    else {
      switch (pProfile->m_Header.pcs) {
      case icSigXYZData:
      case icSigLabData:
        rv = CIccXformCreator::CreateXform(icXformType3DLut, pTag, pHintManager);
        break;

      default:
        break;
      }
    }
  }

  if (rv) {
    if (pPcc)
      rv->m_pConnectionConditions = pPcc;
    else
      rv->m_pConnectionConditions = pProfile;

    rv->SetParams(pProfile, bInput, nIntent, nTagIntent, bUseSpectralPCS, nInterp, pHintManager, bAbsToRel, nMCS);
  }
  else if (bOwnsProfile) {
    // No xform was built for this profile/tag combination; pProfile never
    // reached SetParams(), so free it here when we own it (#1308).  A borrowed
    // profile (bOwnsProfile==false) is left for its real owner to free.
    delete pProfile;
  }

  return rv;
}

/**
 ******************************************************************************
 * Name: CIccXform::SetParams
 * 
 * Purpose: 
 *   This is an accessor function to set private values.  
 * 
 * Args: 
 *  pProfile = pointer to profile associated with transform
 *  bInput = indicates whether profile is input profile
 *  nIntent = rendering intent to apply to the profile
 *  nInterp = the interpolation algorithm to use for N-D luts
 ******************************************************************************/
void CIccXform::SetParams(CIccProfile *pProfile, bool bInput, icRenderingIntent nIntent, icRenderingIntent nTagIntent,
                          bool bUseSpectralPCS, icXformInterp nInterp, CIccCreateXformHintManager *pHintManager/* =NULL */,
                          bool bAbsToRel/*=false*/, icMCSConnectionType nMCS/*=icNoMCS*/)
{
  m_pProfile = pProfile;
  m_bInput = bInput;
  m_nIntent = nIntent;
  m_nTagIntent = nTagIntent;
  m_nInterp = nInterp;
	m_pAdjustPCS = NULL;
  m_bUseSpectralPCS = bUseSpectralPCS;
  m_bAbsToRel = bAbsToRel;
  m_nMCS = nMCS;
  m_bLuminanceMatching = false;

  if (pHintManager) {
    IIccCreateXformHint *pHint=NULL;

    pHint = pHintManager->GetHint("CIccCreateAdjustPCSXformHint");
    if (pHint) {
		  CIccCreateAdjustPCSXformHint *pAdjustPCSHint = (CIccCreateAdjustPCSXformHint*)pHint;
		  m_pAdjustPCS = pAdjustPCSHint->GetNewAdjustPCSXform();
	  }

    pHint = pHintManager->GetHint("CIccCreateCmmEnvVarXformHint");
    if (pHint) {
      CIccCreateCmmEnvVarXformHint *pCmmEnvVarHint = (CIccCreateCmmEnvVarXformHint*)pHint;
      m_pCmmEnvVarLookup = pCmmEnvVarHint->GetNewCmmEnvVarLookup();
    }

    pHint = pHintManager->GetHint("CIccLuminanceMatchingHint");
    if (pHint) {
      m_bLuminanceMatching = true;
    }
  }
}

/**
 **************************************************************************
 * Name: CIccXform::Create
 * 
 * Purpose:
 *  This is a static Creation function that creates derived CIccXform objects and
 *  initializes them.
 * 
 * Args: 
 *  Profile = reference to a CIccProfile object that will be used to create the transform.
 *   A copy of the CIccProfile object will be created and passed to the pointer based Create().
 *   The copied object will be destroyed when the returned CIccXform object is destroyed.
 *  bInput = flag to indicate whether to use the input or output side of the profile,
 *  nIntent = the rendering intent to apply to the profile,   
 *  nInterp = the interpolation algorithm to use for N-D luts.
 *  nLutType = selection of which transform lut to use
 *  bUseD2BxB2DxTags = flag to indicate the use MPE flags if available
 *  pHint = pointer to object passed to CIccXform creation functionality
 * 
 * Return: 
 *  A suitable pXform object
 **************************************************************************
 */
CIccXform *CIccXform::Create(CIccProfile &Profile,
                             bool bInput/* =true */,
                             icRenderingIntent nIntent/* =icUnknownIntent */, 
														 icXformInterp nInterp/* =icInterpLinear */, 
                             IIccProfileConnectionConditions *pPcc/*=NULL*/,
                             icXformLutType nLutType/* =icXformLutColor */, 
														 bool bUseD2BxB2DxTags/* =true */,
                             CIccCreateXformHintManager *pHintManager/* =NULL */)
{
  CIccProfile *pProfile = new (std::nothrow) CIccProfile(Profile);
  if (!pProfile)
    return NULL;

  // The pointer-based Create owns this copy: it stores it in the returned xform
  // on success and frees it on failure (bOwnsProfile defaults to true).  Do not
  // delete pProfile here on failure -- that would double-free the copy Create
  // already released.
  CIccXform *pXform = Create(pProfile, bInput, nIntent, nInterp, pPcc, nLutType, bUseD2BxB2DxTags, pHintManager);

  return pXform;
}


/**
 **************************************************************************
 * Name: CIccXform::CheckForInvalidPCSScale
 * 
 * Purpose: Check scale values before using them to divide.
 *
 **************************************************************************
 */
bool CIccXform::CheckForInvalidPCSScale() const
{
  if (   !std::isfinite(m_PCSScale[0])
      || !std::isfinite(m_PCSScale[1])
      || !std::isfinite(m_PCSScale[2])
      || m_PCSScale[0] == 0.0f
      || m_PCSScale[1] == 0.0f
      || m_PCSScale[2] == 0.0f )
    return true;

  return false;
}


/**
 **************************************************************************
 * Name: CIccXform::Begin
 * 
 * Purpose: 
 *  This function will be called before the xform is applied.  Derived objects
 *  should also call this base class function to initialize for Absolute Colorimetric
 *  Intent handling which is performed through the use of the CheckSrcAbs and
 *  CheckDstAbs functions.
 **************************************************************************
 */
icStatusCMM CIccXform::Begin()
{
  IIccProfileConnectionConditions *pCond = GetConnectionConditions();

  icFloatNumber mediaXYZ[3];
  icFloatNumber illumXYZ[3];

  const bool bUseAbsTagAsRel = m_bAbsToRel &&
    m_nTagIntent == icAbsoluteColorimetric &&
    m_nIntent != icAbsoluteColorimetric;
  const bool bNeedAbsAdjust = m_nIntent == icAbsoluteColorimetric || bUseAbsTagAsRel;

  if (bNeedAbsAdjust) {
    if (pCond) {
      pCond->getMediaWhiteXYZ(mediaXYZ);

      m_MediaXYZ.X = icDtoF(mediaXYZ[0]);
      m_MediaXYZ.Y = icDtoF(mediaXYZ[1]);
      m_MediaXYZ.Z = icDtoF(mediaXYZ[2]);
    }
    else {
      CIccTag *pTag = m_pProfile->FindTag(icSigMediaWhitePointTag);

      if (!pTag || pTag->GetType()!=icSigXYZType)
        return icCmmStatInvalidProfile;

      CIccTagXYZ *pXyzTag = (CIccTagXYZ*)pTag;

      m_MediaXYZ = (*pXyzTag)[0];
      mediaXYZ[0] = icFtoD(m_MediaXYZ.X);
      mediaXYZ[1] = icFtoD(m_MediaXYZ.Y);
      mediaXYZ[2] = icFtoD(m_MediaXYZ.Z);
    }
  }

  icXYZNumber illXYZ;
  if (pCond) {
    pCond->getNormIlluminantXYZ(illumXYZ);
    illXYZ.X = icDtoF(illumXYZ[0]);
    illXYZ.Y = icDtoF(illumXYZ[1]);
    illXYZ.Z = icDtoF(illumXYZ[2]);
  }
  else {
    illXYZ = m_pProfile->m_Header.illuminant;
    illumXYZ[0] = icFtoD(illXYZ.X);
    illumXYZ[1] = icFtoD(illXYZ.Y);
    illumXYZ[2] = icFtoD(illXYZ.Z);
  }

	// set up for any needed PCS adjustment
  if (bNeedAbsAdjust &&
        (m_MediaXYZ.X != illXYZ.X ||
        m_MediaXYZ.Y != illXYZ.Y ||
        m_MediaXYZ.Z != illXYZ.Z)) {

    icColorSpaceSignature Space = m_pProfile->m_Header.pcs;

    if (IsSpacePCS(Space)) {
      m_bAdjustPCS = true;				// turn ON PCS adjustment

      const bool bConvertAbsToRel = (m_nIntent == icAbsoluteColorimetric) ? !m_bInput : m_bInput;
      if (bConvertAbsToRel) {
        if (mediaXYZ[0] == 0.0f || mediaXYZ[1] == 0.0f || mediaXYZ[2] == 0.0f)
          return icCmmStatInvalidProfile;
        
        m_PCSScale[0] = illumXYZ[0] / mediaXYZ[0];
        m_PCSScale[1] = illumXYZ[1] / mediaXYZ[1];
        m_PCSScale[2] = illumXYZ[2] / mediaXYZ[2];
      }
      else {
        if (illumXYZ[0] == 0.0f || illumXYZ[1] == 0.0f || illumXYZ[2] == 0.0f)
          return icCmmStatInvalidProfile;
        
        m_PCSScale[0] = mediaXYZ[0] / illumXYZ[0];
        m_PCSScale[1] = mediaXYZ[1] / illumXYZ[1];
        m_PCSScale[2] = mediaXYZ[2] / illumXYZ[2];
      }

      if (m_PCSScale[0] == 0.0f || m_PCSScale[1] == 0.0f || m_PCSScale[2] == 0.0f)
        return icCmmStatInvalidProfile;

      m_PCSOffset[0] = 0.0;
      m_PCSOffset[1] = 0.0;
      m_PCSOffset[2] = 0.0;
    }
  }
  else if (m_nIntent == icPerceptual && (IsVersion2() || !HasPerceptualHandling())) {
      icColorSpaceSignature Space = m_pProfile->m_Header.pcs;

    if (IsSpacePCS(Space) && m_pProfile->m_Header.deviceClass!=icSigAbstractClass) {
      m_bAdjustPCS = true;				// turn ON PCS adjustment

      // set up for input transform, which needs version 2 black point to version 4
      m_PCSScale[0] = (icFloatNumber) (1.0 - icPerceptualRefBlackX / icPerceptualRefWhiteX);	// scale factors
      m_PCSScale[1] = (icFloatNumber) (1.0 - icPerceptualRefBlackY / icPerceptualRefWhiteY);
      m_PCSScale[2] = (icFloatNumber) (1.0 - icPerceptualRefBlackZ / icPerceptualRefWhiteZ);
      
      if (m_PCSScale[0] == 0.0f || m_PCSScale[1] == 0.0f || m_PCSScale[2] == 0.0f)
        return icCmmStatInvalidProfile;

      m_PCSOffset[0] = (icFloatNumber) (icPerceptualRefBlackX * 32768.0 / 65535.0);	// offset factors
      m_PCSOffset[1] = (icFloatNumber) (icPerceptualRefBlackY * 32768.0 / 65535.0);
      m_PCSOffset[2] = (icFloatNumber) (icPerceptualRefBlackZ * 32768.0 / 65535.0);

      if (!m_bInput) {				// output transform must convert version 4 black point to version 2
        m_PCSScale[0] = (icFloatNumber) 1.0 / m_PCSScale[0];	// invert scale factors
        m_PCSScale[1] = (icFloatNumber) 1.0 / m_PCSScale[1];
        m_PCSScale[2] = (icFloatNumber) 1.0 / m_PCSScale[2];

        m_PCSOffset[0] = - m_PCSOffset[0] * m_PCSScale[0];	// negate offset factors
        m_PCSOffset[1] = - m_PCSOffset[1] * m_PCSScale[1];
        m_PCSOffset[2] = - m_PCSOffset[2] * m_PCSScale[2];
      }
    }

  }

  if (m_pAdjustPCS) {
    CIccProfile ProfileCopy(*m_pProfile);

    // need to read in all the tags, so that a copy of the profile can be made
    if (!ProfileCopy.ReadTags(m_pProfile)) {
      return icCmmStatInvalidProfile;
    }
		
    if (!m_pAdjustPCS->CalcFactors(&ProfileCopy, this, m_PCSScale, m_PCSOffset)) {
      return icCmmStatIncorrectApply;
    }

    m_bAdjustPCS = true;
    delete m_pAdjustPCS;
    m_pAdjustPCS = NULL;
  }

  if (m_bAdjustPCS) {

    // make sure these are not zero, because we will divide by them later in the process
    if (!std::isfinite(m_PCSScale[0]) || !std::isfinite(m_PCSScale[1]) || !std::isfinite(m_PCSScale[2])
        || m_PCSScale[0] == 0.0f || m_PCSScale[1] == 0.0f || m_PCSScale[2] == 0.0f)
      return icCmmStatInvalidProfile;
    
    if ((m_bInput && GetNumDstSamples() < 3) ||
        (!m_bInput && GetNumSrcSamples() < 3)) {
      return icCmmStatInvalidProfile;
    }
  }

  return icCmmStatOk;
}

/**
**************************************************************************
* Name: CIccXform::GetNewApply
* 
* Purpose: 
*  This Factory function allocates data specific for the application of the xform.
*  This allows multiple threads to simultaneously use the same xform.
**************************************************************************
*/
CIccApplyXform *CIccXform::GetNewApply(icStatusCMM &status)
{
  CIccApplyXform *rv = new (std::nothrow) CIccApplyXform(this);

  if (!rv) {
    status = icCmmStatAllocErr;
    return NULL;
  }

  status = icCmmStatOk;
  return rv;
}

/**
 **************************************************************************
 * Name: CIccXform::ApplyN
 *
 * Purpose:
 *  Default implementation: applies single-pixel Apply for each pixel.
 *  Subclasses may override for better vectorized throughput.
 **************************************************************************
 */
void CIccXform::ApplyN(CIccApplyXform *pXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels) const
{
  icUInt16Number nSrc = GetNumSrcSamples();
  icUInt16Number nDst = GetNumDstSamples();
  for (icUInt32Number k = 0; k < nPixels; k++, SrcPixel += nSrc, DstPixel += nDst)
    Apply(pXform, DstPixel, SrcPixel);
}

/**
 **************************************************************************
* Name: CIccXform::AdjustPCS
 * 
 * Purpose: 
*  This function will take care of any PCS adjustments 
*  needed by the xform (the PCS is always version 4 relative).
 * 
 * Args: 
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
 * 
 **************************************************************************
 */
void CIccXform::AdjustPCS(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  icColorSpaceSignature Space = m_pProfile->m_Header.pcs;

  if (Space==icSigLabData) {
    if (UseLegacyPCS()) {
    	CIccPCSUtil::Lab2ToXyz(DstPixel, SrcPixel, true);
    }
    else {
      CIccPCSUtil::LabToXyz(DstPixel, SrcPixel, true);
    }
  }
  else {
    DstPixel[0] = SrcPixel[0];
    DstPixel[1] = SrcPixel[1];
    DstPixel[2] = SrcPixel[2];
  }

  DstPixel[0] = DstPixel[0] * m_PCSScale[0] + m_PCSOffset[0];
  DstPixel[1] = DstPixel[1] * m_PCSScale[1] + m_PCSOffset[1];
  DstPixel[2] = DstPixel[2] * m_PCSScale[2] + m_PCSOffset[2];

  if (Space==icSigLabData) {
    if (UseLegacyPCS()) {

    	CIccPCSUtil::XyzToLab2(DstPixel, DstPixel, true);
    }
    else {
      CIccPCSUtil::XyzToLab(DstPixel, DstPixel, true);
    }
  }
#ifndef SAMPLEICC_NOCLIPLABTOXYZ
  else {
    DstPixel[0] = CIccPCSUtil::NegClip(DstPixel[0]);
    DstPixel[1] = CIccPCSUtil::NegClip(DstPixel[1]);
    DstPixel[2] = CIccPCSUtil::NegClip(DstPixel[2]);
  }
#endif
}

/**
 **************************************************************************
 * Name: CIccXform::CheckSrcAbs
 * 
 * Purpose: 
 *  This function will be called by a derived CIccXform object's Apply() function
 *  BEFORE the actual xform is performed to take care of Absolute to Relative
 *  adjustments needed by the xform (IE the PCS is always version 4 relative).
 * 
 * Args: 
 *  Pixel = src pixel data (will not be modified)
 * 
 * Return: 
 *  returns Pixel or adjusted pixel data.
 **************************************************************************
 */
const icFloatNumber *CIccXform::CheckSrcAbs(CIccApplyXform *pApply, const icFloatNumber *Pixel) const
{
	if (m_bAdjustPCS && !m_bInput) {
    icFloatNumber *pAbsLab = pApply->m_AbsLab;
		AdjustPCS(pAbsLab, Pixel);
    return pAbsLab;
  }

  return Pixel;
}

/**
 **************************************************************************
 * Name: CIccXform::CheckDstAbs
 * 
 * Purpose: 
 *  This function will be called by a derived CIccXform object's Apply() function
 *  AFTER the actual xform is performed to take care of Absolute to Relative
 *  adjustments needed by the xform (IE the PCS is always version 4 relative).
 * 
 * Args: 
 *  Pixel = source pixel data which will be modified
 *
 **************************************************************************
 */
void CIccXform::CheckDstAbs(icFloatNumber *Pixel) const
{
	if (m_bAdjustPCS && m_bInput) {
		AdjustPCS(Pixel, Pixel);
  }
}
        
/**
**************************************************************************
* Name: CIccXform::GetSrcSpace
* 
* Purpose: 
*  Return the color space that is input to the transform.  
*  If a device space is either XYZ/Lab it is changed to dXYZ/dLab to avoid
*  confusion with PCS encoding of these spaces.  Device encoding of XYZ
*  and Lab spaces left to the device.
**************************************************************************
*/
icColorSpaceSignature CIccXform::GetSrcSpace() const
{
  icColorSpaceSignature rv;
  icProfileClassSignature deviceClass = m_pProfile->m_Header.deviceClass;

  if (m_bInput) {
    if (m_bPcsAdjustXform)
      rv = m_pProfile->m_Header.pcs;
    else {
      rv = m_pProfile->m_Header.colorSpace;

      if (deviceClass != icSigAbstractClass) {
        //convert signature to device colorspace signature (avoid confusion about encoding).
        if (rv == icSigXYZData) {
          rv = icSigDevXYZData;
        }
        else if (rv == icSigLabData) {
          rv = icSigDevLabData;
        }
      }
    }
  }
  else if (!m_bUseSpectralPCS || !m_pProfile->m_Header.spectralPCS) {
    rv = m_pProfile->m_Header.pcs;
  }
  else {
    rv = icGetColorSpaceType(m_pProfile->m_Header.spectralPCS);
  }

  return rv;
}

/**
**************************************************************************
* Name: CIccXform::GetNumSrcSamples
* 
* Purpose: 
*  Return the color space that is input to the transform.  
*  If a device space is either XYZ/Lab it is changed to dXYZ/dLab to avoid
*  confusion with PCS encoding of these spaces.  Device encoding of XYZ
*  and Lab spaces left to the device.
**************************************************************************
*/
icUInt16Number CIccXform::GetNumSrcSamples() const
{
  icUInt16Number rv;

  if (m_nMCS==icFromMCS) {
    rv = (icUInt16Number)icGetSpaceSamples((icColorSpaceSignature)m_pProfile->m_Header.mcs);
  }
  else if (m_bInput) {
    rv = (icUInt16Number)icGetSpaceSamples(m_pProfile->m_Header.colorSpace);
  }
  else if (!m_bUseSpectralPCS || !m_pProfile->m_Header.spectralPCS) {
    rv = (icUInt16Number)icGetSpaceSamples(m_pProfile->m_Header.pcs);
  }
  else {
    rv = (icUInt16Number)icGetSpectralSpaceSamples(&m_pProfile->m_Header);
  }

  return rv;
}

/**
**************************************************************************
* Name: CIccXform::GetDstSpace
* 
* Purpose: 
*  Return the color space that is output by the transform.  
*  If a device space is either XYZ/Lab it is changed to dXYZ/dLab to avoid
*  confusion with PCS encoding of these spaces.  Device encoding of XYZ
*  and Lab spaces left to the device.
**************************************************************************
*/
icColorSpaceSignature CIccXform::GetDstSpace() const
{
  // A gamut xform consumes the PCS and produces the single gamut channel, so
  // its destination is icSigGamutData whatever the profile's device space is.
  // m_bInput is false here purely to traverse the B-to-A shaped gamt tag; the
  // !m_bInput branch below would otherwise report m_Header.colorSpace and
  // contradict the PCS -> gamt link CIccCmm::AddXform() already recorded.
  if (m_bGamutXform)
    return icSigGamutData;

  icColorSpaceSignature rv;
  icProfileClassSignature deviceClass = m_pProfile->m_Header.deviceClass;

  if (m_nMCS==icToMCS) {
    rv = (icColorSpaceSignature)m_pProfile->m_Header.mcs;
  }
  else if (m_bInput) {
    if (m_bUseSpectralPCS && m_pProfile->m_Header.spectralPCS)
      rv = icGetColorSpaceType(m_pProfile->m_Header.spectralPCS);
    else
      rv = m_pProfile->m_Header.pcs;
  }
  else {
    rv = m_pProfile->m_Header.colorSpace;

    //convert signature to device colorspace signature (avoid confusion about encoding).
    if (deviceClass != icSigAbstractClass) {
      if (rv==icSigXYZData) {
        rv = icSigDevXYZData;
      }
      else if (rv==icSigLabData) {
        rv = icSigDevLabData;
      }
    }
  }

  return rv;
}

/**
**************************************************************************
* Name: CIccXform::GetNumDstSamples
* 
* Purpose: 
*  Return the color space that is input to the transform.  
*  If a device space is either XYZ/Lab it is changed to dXYZ/dLab to avoid
*  confusion with PCS encoding of these spaces.  Device encoding of XYZ
*  and Lab spaces left to the device.
**************************************************************************
*/
icUInt16Number CIccXform::GetNumDstSamples() const
{
  // Must track GetDstSpace() above: CIccCmm::Begin() rejects the chain with
  // icCmmStatBadSpaceLink when this disagrees with the CMM destination sample
  // count, and CIccApplyCmm sizes its pixel buffers from it.
  if (m_bGamutXform)
    return (icUInt16Number)icGetSpaceSamples(icSigGamutData);

  icUInt16Number rv;

  if (m_nMCS==icToMCS) {
    rv = (icUInt16Number)icGetSpaceSamples((icColorSpaceSignature)m_pProfile->m_Header.mcs);
  }
  else if (!m_bInput) {
    rv = (icUInt16Number)icGetSpaceSamples(m_pProfile->m_Header.colorSpace);
  }
  else if (!m_bUseSpectralPCS || !m_pProfile->m_Header.spectralPCS) {
    rv = (icUInt16Number)icGetSpaceSamples(m_pProfile->m_Header.pcs);
  }
  else {
    rv = (icUInt16Number)icGetSpectralSpaceSamples(&m_pProfile->m_Header);
  }

  return rv;
}


/**
**************************************************************************
* Name: CIccApplyXform::CIccApplyXform
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccApplyXform::CIccApplyXform(CIccXform *pXform) : m_AbsLab{}
{
  m_pXform = pXform;
}

/**
**************************************************************************
* Name: CIccApplyXform::CIccApplyXform
*
* Purpose:
*  Destructor
**************************************************************************
*/
CIccApplyXform::~CIccApplyXform()
{
}


/**
**************************************************************************
* Name: CIccApplyNDLutXform::CIccApplyNDLutXform
*
* Purpose:
*  Constructor
**************************************************************************
*/
CIccApplyNDLutXform::CIccApplyNDLutXform(CIccXformNDLut* pXform, CIccApplyCLUT *pApply) : CIccApplyXform(pXform)
{
  m_pApply = pApply;
}


/**
**************************************************************************
* Name: CIccApplyNDLutXform::~CIccApplyNDLutXform
*
* Purpose:
*  Destructor
**************************************************************************
*/
CIccApplyNDLutXform::~CIccApplyNDLutXform()
{
  delete m_pApply;
}


/**
**************************************************************************
* Name: CIccApplyPcsXform::CIccApplyPcsXform
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccApplyPcsXform::CIccApplyPcsXform(CIccXform *pXform) : CIccApplyXform(pXform)
{
  m_list = new CIccApplyPcsStepList();
  m_temp1 = NULL;
  m_temp2 = NULL;
}

/**
**************************************************************************
* Name: CIccApplyPcsXform::~CIccApplyPcsXform
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccApplyPcsXform::~CIccApplyPcsXform()
{

  if (m_list) {
    CIccApplyPcsStepList::iterator i;
    for (i=m_list->begin(); i!=m_list->end(); i++) {
      delete i->ptr;
    }
    delete m_list;
  }

  delete [] m_temp1;
  delete [] m_temp2;
}

/**
**************************************************************************
* Name: CIccApplyPcsXform::Init
* 
* Purpose: 
*  Initialize temporary variables used during pcs processing
**************************************************************************
*/
bool CIccApplyPcsXform::Init()
{
  CIccPcsXform *pXform = (CIccPcsXform*)m_pXform;
  icUInt16Number nChan = pXform->MaxChannels();

  if (nChan) {
    m_temp1 = new (std::nothrow) icFloatNumber[nChan];
    m_temp2 = new (std::nothrow) icFloatNumber[nChan];
  }

  return m_temp1!=NULL && m_temp2!=NULL;
}


void CIccApplyPcsXform::AppendApplyStep(CIccApplyPcsStep *pStep)
{
  CIccApplyPcsStepPtr ptr;

  if (pStep && m_list) {
    ptr.ptr = pStep;
    m_list->push_back(ptr);
  }
}


/**
**************************************************************************
* Name: CIccPcsXform::CIccPcsXform
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsXform::CIccPcsXform()
{
  m_list = new CIccPcsStepList();

  m_srcSpace = icSigUnknownData;
  m_nSrcSamples = 0;

  m_dstSpace = icSigUnknownData;
  m_nDstSamples = 0;
}

/**
**************************************************************************
* Name: CIccPcsXform::~CIccPcsXform
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsXform::~CIccPcsXform()
{
  if (m_list) {
    CIccPcsStepList::iterator step;
    for (step=m_list->begin(); step!=m_list->end(); step++) {
      delete step->ptr;
      step->ptr = NULL;
    }
    delete m_list;
  }
}

/**
 **************************************************************************
 * Name: icSpectralPcsMatchesRange
 *
 * Purpose:
 *  Reports whether a header's spectral PCS signature and its spectral range
 *  definition describe the same number of samples.
 *
 *  A spectral PCS signature carries its channel count in its low 16 bits, and that
 *  count is the pixel width the CMM moves across the connection: it is what
 *  CIccXform::GetNumSrcSamples()/GetNumDstSamples() report, and in turn what
 *  CIccApplyCmm::InitPixel() sizes m_Pixel/m_Pixel2 from. The PCS steps that
 *  CIccPcsXform::Connect() pushes are sized from the header's spectralRange /
 *  biSpectralRange instead. When the two disagree, a step operates on a wider
 *  vector than the pixel buffer holding it.
 *
 *  CIccProfile::Validate() already reports both disagreements as critical errors
 *  (IccProfile.cpp, the bi-spectral product and the plain spectral count in the
 *  spectralPCS switch), but nothing on the apply path consults it - iccApplyProfiles
 *  and the CMM never call Validate() - so the check has to exist here too.
 *
 *  The scoping deliberately mirrors the validator's. icSigNoSpectralData has no
 *  spectral PCS to be inconsistent with, and the sparse-matrix PCS is excluded
 *  because there the channel count is the size of the encoded matrix blob carried in
 *  the pixel rather than a sample count, so it is independent of the ranges by design
 *  (see CIccPcsStepSrcSparseMatrix, which takes the two separately).
 **************************************************************************
 */
static bool icSpectralPcsMatchesRange(const icHeader &hdr)
{
  icColorSpaceSignature sig = (icColorSpaceSignature)hdr.spectralPCS;

  switch (icGetColorSpaceType(sig)) {
    case icSigReflectanceSpectralData:
    case icSigTransmisionSpectralData:
    case icSigRadiantSpectralData:
      return icNumColorSpaceChannels(sig) == (icUInt32Number)hdr.spectralRange.steps;

    case icSigBiSpectralReflectanceData:
      // One sample per (spectral, bi-spectral) wavelength pair. Widened before the
      // multiply so a large pair cannot wrap the 16-bit product into a value that
      // happens to match the channel count.
      return icNumColorSpaceChannels(sig) ==
             (icUInt32Number)hdr.biSpectralRange.steps * (icUInt32Number)hdr.spectralRange.steps;

    default:
      return true;
  }
}

/**
 **************************************************************************
 * Name: CIccPcsXform::Connect
 * 
 * Purpose: 
 *  Insert PCS transform operations to perform PCS processing between 
 *  pFromXform and pToXform
 **************************************************************************
 */
icStatusCMM CIccPcsXform::Connect(CIccXform *pFromXform, CIccXform *pToXform)
{
  icStatusCMM stat;

  if (!pFromXform || !pToXform)
    return icCmmStatBadXform;

  if (pFromXform->IsMCS() && pToXform->IsMCS()) {
    CIccProfile *pFromProfile = pFromXform->GetProfilePtr();
    CIccProfile *pToProfile = pToXform->GetProfilePtr();

    if (!pFromProfile || !pToProfile) {
      return icCmmStatBadSpaceLink;
    }
    CIccTagArray *pFromChannels = (CIccTagArray*)(pFromProfile->FindTagOfType(icSigMultiplexTypeArrayTag, icSigTagArrayType));
    CIccTagArray *pToChannels = (CIccTagArray*)(pToProfile->FindTagOfType(icSigMultiplexTypeArrayTag, icSigTagArrayType));

    if (!pFromChannels || !pToChannels) {
      return icCmmStatBadSpaceLink;
    }
    if (pFromChannels->GetTagArrayType()!=icSigUtf8TextTypeArray ||
        pToChannels->GetTagArrayType()!=icSigUtf8TextTypeArray ||
        !pFromChannels->AreAllOfType(icSigUtf8TextType) ||
        !pToChannels->AreAllOfType(icSigUtf8TextType)) {
      return icCmmStatBadSpaceLink;
    }

    m_nSrcSamples = pFromXform->GetNumDstSamples();
    m_nDstSamples = pToXform->GetNumSrcSamples();

    if (pFromChannels->GetSize() != m_nSrcSamples || pToChannels->GetSize() != m_nDstSamples) {
      return icCmmStatBadSpaceLink;
    }
    int i, j;

    if (pFromProfile->m_Header.flags&icMCSNeedsSubsetTrue) {
      for (i=0; i<m_nSrcSamples; i++) {
        const icUChar *szSrcChan = ((CIccTagUtf8Text*)pFromChannels->GetIndex(i))->GetText();
        for (j=0; j<m_nDstSamples; j++) {
          const icUChar *szDstChan = ((CIccTagUtf8Text*)pToChannels->GetIndex(i))->GetText();
          if (!icUtf8StrCmp(szSrcChan, szDstChan))
            break;
        }
        if (j==m_nDstSamples)
          return icCmmStatBadMCSLink;
      }
    }

    if (pToProfile->m_Header.flags&icMCSNeedsSubsetTrue) {
      for (i=0; i<m_nDstSamples; i++) {
        const icUChar *szDstChan = ((CIccTagUtf8Text*)pToChannels->GetIndex(i))->GetText();
        for (j=0; j<m_nSrcSamples; j++) {
          const icUChar *szSrcChan = ((CIccTagUtf8Text*)pFromChannels->GetIndex(i))->GetText();
          if (!icUtf8StrCmp(szSrcChan, szDstChan))
            break;
        }
        if (j==m_nSrcSamples)
          return icCmmStatBadMCSLink;
      }
    }
    CIccTag *pTag = pToProfile->FindTag(icSigMultiplexDefaultValuesTag);
    CIccTagNumArray *pDefaults = NULL;
    if (pTag && pTag->IsNumArrayType()) {
      pDefaults = (CIccTagNumArray *)pTag;
    }

    pushRouteMcs(pFromChannels, pToChannels, pDefaults);
  }
  else {
    if (!pFromXform->IsInput() || (pToXform->IsInput() && !pToXform->IsAbstract())) {
      return icCmmStatBadSpaceLink;
    }

    m_srcSpace = pFromXform->GetDstSpace();
    if (IsSpaceSpectralPCS(m_srcSpace))
      m_srcSpace = icGetColorSpaceType(m_srcSpace);

    m_nSrcSamples = pFromXform->GetNumDstSamples();

    m_dstSpace = pToXform->GetSrcSpace();
    if (IsSpaceSpectralPCS(m_dstSpace))
      m_dstSpace = icGetColorSpaceType(m_dstSpace);

    m_nDstSamples = pToXform->GetNumSrcSamples();

    // m_nSrcSamples and m_nDstSamples were just taken from the neighbouring xforms, which
    // derive them from their profile's PCS signature. Every spectral branch of the switch
    // below instead sizes the steps it pushes from that profile's spectralRange, so a
    // header whose two statements of the same quantity disagree produces a chain that
    // reads or writes past the pixel buffer the caller allocated from the signature.
    //
    // Issue #1932 is that read: a header pairing an "rs" spectral PCS of 36 channels with
    // spectralRange.steps == 184 reached pushRef2Xyz(), whose 3x184 observer matrix -
    // well formed in itself - indexed a 36-float pixel buffer. Rejecting the profile is
    // the only correct outcome; there is no way to tell which of the two numbers the
    // producer meant, and Validate() already treats the disagreement as a critical error.
    //
    // Named pSpectralSrc/pSpectralDst rather than the pFromProfile/pToProfile used
    // elsewhere in this function: the icSigRadiantSpectralPcsData case below declares its
    // own pFromProfile, and reusing the name here would shadow it under -Wshadow.
    CIccProfile *pSpectralSrc = pFromXform->GetProfilePtr();
    CIccProfile *pSpectralDst = pToXform->GetProfilePtr();

    if ((pSpectralSrc && !icSpectralPcsMatchesRange(pSpectralSrc->m_Header)) ||
        (pSpectralDst && !icSpectralPcsMatchesRange(pSpectralDst->m_Header))) {
      return icCmmStatInvalidProfile;
    }

    switch (m_srcSpace) {
      case icSigLabPcsData:
        switch (m_dstSpace) {
          case icSigLabPcsData:
            if (pFromXform->UseLegacyPCS())
              pushLab2ToXyz(pFromXform->m_pConnectionConditions);
            else
              pushLabToXyz(pFromXform->m_pConnectionConditions);
            if (pFromXform->NeedAdjustDstPCS()) {
              pushScale3(pFromXform->m_PCSScale[0], pFromXform->m_PCSScale[1], pFromXform->m_PCSScale[2]);
              pushOffset3(pFromXform->m_PCSOffset[0], pFromXform->m_PCSOffset[1], pFromXform->m_PCSOffset[2]);
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            if (pToXform->UseLegacyPCS())
              pushXyzToLab2(pToXform->m_pConnectionConditions);
            else
              pushXyzToLab(pToXform->m_pConnectionConditions);
            break;

          case icSigXYZPcsData:
            if (pFromXform->UseLegacyPCS())
              pushLab2ToXyz(pFromXform->m_pConnectionConditions);
            else
              pushLabToXyz(pFromXform->m_pConnectionConditions);
            if (pFromXform->NeedAdjustDstPCS()) {
              pushScale3(pFromXform->m_PCSScale[0], pFromXform->m_PCSScale[1], pFromXform->m_PCSScale[2]);
              pushOffset3(pFromXform->m_PCSOffset[0], pFromXform->m_PCSOffset[1], pFromXform->m_PCSOffset[2]);
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                          pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                          pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            pushXyzToXyzIn();
            break;

          default:
          case icSigReflectanceSpectralPcsData:
          case icSigTransmissionSpectralPcsData:
          case icSigRadiantSpectralPcsData:
          case icSigBiDirReflectanceSpectralPcsData:
          case icSigSparseMatrixSpectralPcsData:
            return icCmmStatUnsupportedPcsLink;
            
        }
        break;

      case icSigXYZPcsData:
        switch (m_dstSpace) {
          case icSigLabPcsData:
            pushXyzInToXyz();
            if (pFromXform->NeedAdjustDstPCS()) {
              pushScale3(pFromXform->m_PCSScale[0], pFromXform->m_PCSScale[1], pFromXform->m_PCSScale[2]);
              pushOffset3(pFromXform->m_PCSOffset[0], pFromXform->m_PCSOffset[1], pFromXform->m_PCSOffset[2]);
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            if (pToXform->UseLegacyPCS())
              pushXyzToLab2(pToXform->m_pConnectionConditions);
            else
              pushXyzToLab(pToXform->m_pConnectionConditions);
            break;

          case icSigXYZPcsData:
            pushXyzInToXyz();
            if (pFromXform->NeedAdjustDstPCS()) {
              pushScale3(pFromXform->m_PCSScale[0], pFromXform->m_PCSScale[1], pFromXform->m_PCSScale[2]);
              pushOffset3(pFromXform->m_PCSOffset[0], pFromXform->m_PCSOffset[1], pFromXform->m_PCSOffset[2]);
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            pushXyzToXyzIn();
            break;

          default:
          case icSigReflectanceSpectralPcsData:
          case icSigTransmissionSpectralPcsData:
          case icSigRadiantSpectralPcsData:
          case icSigBiDirReflectanceSpectralPcsData:
          case icSigSparseMatrixSpectralPcsData:
            return icCmmStatUnsupportedPcsLink;
        }
        break;

      case icSigReflectanceSpectralPcsData:
      case icSigTransmissionSpectralPcsData:
        switch (m_dstSpace) {
          case icSigLabPcsData:
            if ((stat=pushRef2Xyz(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                                  pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                                  pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            if (pToXform->UseLegacyPCS())
              pushXyzToLab2(pToXform->m_pConnectionConditions);
            else
              pushXyzToLab(pToXform->m_pConnectionConditions);
            break;

          case icSigXYZPcsData:
            if ((stat=pushRef2Xyz(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            pushXyzToXyzIn();
            break;

          case icSigReflectanceSpectralPcsData:
          case icSigTransmissionSpectralPcsData:
            if ((stat=pushSpecToRange(pFromXform->m_pProfile->m_Header.spectralRange,
                                    pToXform->m_pProfile->m_Header.spectralRange))!=icCmmStatOk) {
              return stat;
            }
            break;

          case icSigRadiantSpectralPcsData:
            if ((stat=pushApplyIllum(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushSpecToRange(pFromXform->m_pProfile->m_Header.spectralRange,
                                        pToXform->m_pProfile->m_Header.spectralRange))!=icCmmStatOk) {
              return stat;
            }
            break;

          default:
          case icSigBiDirReflectanceSpectralPcsData:
          case icSigSparseMatrixSpectralPcsData:
            return icCmmStatUnsupportedPcsLink;
        }
        break;

      case icSigRadiantSpectralPcsData: {
        CIccProfile *pFromProfile = pFromXform->GetProfilePtr();
//        CIccProfile *pToProfile = pToXform->GetProfilePtr();      // unused!

        switch (m_dstSpace) {
          case icSigLabPcsData:
            if ((stat=pushRad2Xyz(pFromProfile, pFromXform->m_pConnectionConditions, false))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            if (pToXform->UseLegacyPCS())
              pushXyzToLab2(pToXform->m_pConnectionConditions);
            else
              pushXyzToLab(pToXform->m_pConnectionConditions);
            break;

          case icSigXYZPcsData:
            if ((stat=pushRad2Xyz(pFromProfile, pFromXform->m_pConnectionConditions, false))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            pushXyzToXyzIn();
            break;

          case icSigReflectanceSpectralPcsData:
          case icSigTransmissionSpectralPcsData:
            return icCmmStatUnsupportedPcsLink;


          case icSigRadiantSpectralPcsData:
            if ((stat=pushSpecToRange(pFromXform->m_pProfile->m_Header.spectralRange, 
                                   pToXform->m_pProfile->m_Header.spectralRange))!=icCmmStatOk) {
              return stat;
            }
            break;

          default:
          case icSigBiDirReflectanceSpectralPcsData:
          case icSigSparseMatrixSpectralPcsData:
            return icCmmStatUnsupportedPcsLink;
        }
      }
      break;

      case icSigBiDirReflectanceSpectralPcsData:
      case icSigSparseMatrixSpectralPcsData:
        switch (m_dstSpace) {
          case icSigLabPcsData:
            // This was the one call among the nine status-returning push* helpers whose
            // result was discarded; the bi-spectral cases below it, and pushXYZConvert()
            // on the next line, all propagate. Swallowing it let a refused illuminant
            // step leave the chain a step short and Begin() still report success.
            if ((stat=pushBiRef2Xyz(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            if (pToXform->UseLegacyPCS())
              pushXyzToLab2(pToXform->m_pConnectionConditions);
            else
              pushXyzToLab(pToXform->m_pConnectionConditions);
            break;

          case icSigXYZPcsData:
            if ((stat=pushBiRef2Xyz(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushXYZConvert(pFromXform, pToXform))!=icCmmStatOk) {
              return stat;
            }
            if (pToXform->NeedAdjustSrcPCS()) {
              if (pToXform->CheckForInvalidPCSScale())
                return icCmmStatBadXform;
              pushOffset3(pToXform->m_PCSOffset[0]/pToXform->m_PCSScale[0],
                            pToXform->m_PCSOffset[1]/pToXform->m_PCSScale[1],
                            pToXform->m_PCSOffset[2]/pToXform->m_PCSScale[2]);
              pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
            }
            pushXyzToXyzIn();
            break;

          case icSigReflectanceSpectralPcsData:
          case icSigTransmissionSpectralPcsData:
            if ((stat=pushBiRef2Ref(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushSpecToRange(pFromXform->m_pProfile->m_Header.spectralRange,
                                    pToXform->m_pProfile->m_Header.spectralRange))!=icCmmStatOk) {
              return stat;
            }
            break;

          case icSigRadiantSpectralPcsData:
            if ((stat=pushBiRef2Rad(pFromXform->m_pProfile, pFromXform->m_pConnectionConditions))!=icCmmStatOk) {
              return stat;
            }
            if ((stat=pushSpecToRange(pFromXform->m_pProfile->m_Header.spectralRange, 
                            pToXform->m_pProfile->m_Header.spectralRange))!=icCmmStatOk) {
              return stat;
            }
            break;

          case icSigBiDirReflectanceSpectralPcsData:
            if (icSameSpectralRange(pFromXform->m_pProfile->m_Header.spectralRange, pToXform->m_pProfile->m_Header.spectralRange) &&
                icSameSpectralRange(pFromXform->m_pProfile->m_Header.biSpectralRange, pToXform->m_pProfile->m_Header.biSpectralRange))
                break;
            else
              return icCmmStatUnsupportedPcsLink;
          
          default:
          case icSigSparseMatrixSpectralPcsData:
            return icCmmStatUnsupportedPcsLink;
        
        }
        break;
            
      default:
        return icCmmStatUnsupportedPcsLink;
        break;
    }
  }

  icStatusCMM rv = Optimize();
  if (rv!=icCmmStatOk)
    return rv;

  if (m_list->begin()==m_list->end())
    return icCmmStatIdentityXform;

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::ConnectFirst
 *
 * Purpose:
 *  Insert PCS transform operations to perform PCS processing going into
 *  first transform
 **************************************************************************
 */
icStatusCMM CIccPcsXform::ConnectFirst(CIccXform* pToXform, icColorSpaceSignature srcSpace)
{
  if (!pToXform)
    return icCmmStatBadXform;

  // Describe the edge conversion this object performs, exactly as Connect()
  // does for an interior connection.  A CIccPcsXform built here is pushed onto
  // the xform list by CheckPCSConnections(), after which CIccCmm::Begin() and
  // the Apply chain read GetSrcSpace()/GetNumSrcSamples()/GetDstSpace()/
  // GetNumDstSamples() off it like any other xform; leaving all four at the
  // CIccXform defaults ('????' and 0 samples) leaves the chain undescribed at
  // its own boundary (#1748).  The trailing-edge counterpart in ConnectLast()
  // is where that is actually fatal today - see the note there.
  //
  // This transform runs from the CMM's source space into whatever the following
  // transform consumes, so the source side is derived from srcSpace and the
  // destination side is taken from pToXform.  Spectral PCS signatures are
  // reduced to their colorimetric type the same way Connect() reduces them,
  // because the steps pushed below operate on colorimetric PCS values.
  m_srcSpace = srcSpace;
  if (IsSpaceSpectralPCS(m_srcSpace))
    m_srcSpace = icGetColorSpaceType(m_srcSpace);
  m_nSrcSamples = (icUInt16Number)icGetSpaceSamples(m_srcSpace);

  m_dstSpace = pToXform->GetSrcSpace();
  if (IsSpaceSpectralPCS(m_dstSpace))
    m_dstSpace = icGetColorSpaceType(m_dstSpace);
  m_nDstSamples = pToXform->GetNumSrcSamples();

  if (srcSpace == icSigXYZData) {
    pushXyzInToXyz();
    if (pToXform->NeedAdjustSrcPCS()) {
      pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
      pushOffset3(pToXform->m_PCSOffset[0], pToXform->m_PCSOffset[1], pToXform->m_PCSOffset[2]);
    }

    if (pToXform->GetSrcSpace() == icSigLabData) {
      if (pToXform->UseLegacyPCS())
        pushXyzToLab2(pToXform->m_pConnectionConditions);
      else
        pushXyzToLab(pToXform->m_pConnectionConditions);
    }
    else {
      pushXyzToXyzIn();
    }
  }
  else if (srcSpace == icSigLabData) {

    if (pToXform->GetSrcSpace() == icSigXYZData) {
      // Incoming PCS is Lab feeding a transform whose source space is XYZ.
      // CIccPcsStepLabToXyz emits *actual* (unscaled) XYZ, but an XYZ-source
      // transform consumes *internal* XYZ (scaled by icXyzToXyzIn = 32768/65535).
      // Mirror ConnectLast() and the icSigXYZData source branch above by rescaling
      // actual->internal XYZ here; without it the XYZ handed to the transform is
      // ~2x too large (65535/32768) and the Lab->device direction (e.g. round-trip
      // evaluation through a matrix/TRC display profile) is grossly wrong.
      pushLabToXyz(pToXform->m_pConnectionConditions);
      if (pToXform->NeedAdjustSrcPCS()) {
        pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
        pushOffset3(pToXform->m_PCSOffset[0], pToXform->m_PCSOffset[1], pToXform->m_PCSOffset[2]);
      }
      pushXyzToXyzIn();
    }
    else if (pToXform->NeedAdjustSrcPCS()) {
      pushLabToXyz(pToXform->m_pConnectionConditions);
      pushScale3(pToXform->m_PCSScale[0], pToXform->m_PCSScale[1], pToXform->m_PCSScale[2]);
      pushOffset3(pToXform->m_PCSOffset[0], pToXform->m_PCSOffset[1], pToXform->m_PCSOffset[2]);
      if (pToXform->UseLegacyPCS())
        pushXyzToLab2(pToXform->m_pConnectionConditions);
      else
        pushXyzToLab(pToXform->m_pConnectionConditions);
    }
    else if (pToXform->UseLegacyPCS() && !pToXform->IsInput()) {
      pushLabToLab2();
    }

  }


  icStatusCMM rv = Optimize();
  if (rv != icCmmStatOk)
    return rv;

  if (m_list->begin() == m_list->end())
    return icCmmStatIdentityXform;

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::ConnectLast
 *
 * Purpose:
 *  Insert PCS transform operations to perform PCS processing coming out of 
 *  last transform
 **************************************************************************
 */
icStatusCMM CIccPcsXform::ConnectLast(CIccXform* pFromXform, icColorSpaceSignature dstSpace)
{
  if (!pFromXform)
    return icCmmStatBadXform;
  icColorSpaceSignature srcSpace = pFromXform->GetDstSpace();

  // Mirror of the block in ConnectFirst() above, for the trailing edge: this
  // transform runs from whatever the preceding transform produced into the
  // CMM's destination space, so the source side comes from pFromXform and the
  // destination side is derived from dstSpace.
  //
  // This is the copy that has to be right for the chain to run at all.
  // CheckPCSConnections() appends this object, and CIccCmm::Begin() then
  // compares the last transform's GetNumDstSamples() against GetDestSamples()
  // before allocating the apply chain - a guard added against heap overflow,
  // and one that explicitly expects a transform to have been appended here.
  // With the count left at 0 it rejected every chain whose PCS edge space
  // differs from the profile's own PCS, e.g. driving a Lab-PCS profile with an
  // XYZ destination space returned icCmmStatBadSpaceLink for a profile and a
  // chain that were both perfectly valid (#1748).
  //
  // Note the local srcSpace above is rewritten to icSigXYZData further down
  // when a destination-PCS adjustment is inserted; m_srcSpace deliberately
  // records the space actually entering this object, which is the space
  // pFromXform emits.
  m_srcSpace = srcSpace;
  if (IsSpaceSpectralPCS(m_srcSpace))
    m_srcSpace = icGetColorSpaceType(m_srcSpace);
  m_nSrcSamples = pFromXform->GetNumDstSamples();

  m_dstSpace = dstSpace;
  if (IsSpaceSpectralPCS(m_dstSpace))
    m_dstSpace = icGetColorSpaceType(m_dstSpace);
  m_nDstSamples = (icUInt16Number)icGetSpaceSamples(m_dstSpace);

  if (pFromXform->NeedAdjustDstPCS() && IsSpaceColorimetricPCS(dstSpace)) {
    if (srcSpace == icSigLabData) {
      if (pFromXform->UseLegacyPCS())
        pushLab2ToXyz(pFromXform->m_pConnectionConditions);
      else
        pushLabToXyz(pFromXform->m_pConnectionConditions);
    }

    pushScale3(pFromXform->m_PCSScale[0], pFromXform->m_PCSScale[1], pFromXform->m_PCSScale[2]);
    pushOffset3(pFromXform->m_PCSOffset[0], pFromXform->m_PCSOffset[1], pFromXform->m_PCSOffset[2]);

    pushXyzToXyzIn();

    srcSpace = icSigXYZData;
  }

  if (srcSpace == icSigXYZData && dstSpace == icSigLabData) {
    pushXyzInToXyz();
    pushXyzToLab(pFromXform->m_pConnectionConditions);
  }
  else if (srcSpace == icSigLabData && dstSpace == icSigXYZData) {
    if (pFromXform->UseLegacyPCS())
      pushLab2ToXyz(pFromXform->m_pConnectionConditions);
    else
      pushLabToXyz(pFromXform->m_pConnectionConditions);
    if (pFromXform->IsInput())
      pushXyzToXyzIn();
  }
  else if (pFromXform->UseLegacyPCS() && pFromXform->IsInput() && srcSpace == icSigLabData && dstSpace == icSigLabData) {
    pushLab2ToLab();
  }

  icStatusCMM rv = Optimize();
  if (rv != icCmmStatOk)
    return rv;

  if (m_list->begin() == m_list->end())
    return icCmmStatIdentityXform;

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::Optimize
 * 
 * Purpose:
 *  Gives each step in the chain its one chance to precompute invariant state.
 *
 *  Runs after Connect()/Optimize() have finished building the list and before
 *  GetNewApply(), so a step can build immutable data here and treat it as
 *  read-only in Apply(). Doing that work lazily on first Apply() instead would
 *  be a data race: Apply() is const and runs concurrently on every worker
 *  thread of a CIccThreadedCmm.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::Begin()
{
  if (m_list) {
    CIccPcsStepList::iterator i;

    for (i = m_list->begin(); i != m_list->end(); i++) {
      if (i->ptr && !i->ptr->BeginStep())
        return icCmmStatInvalidLut;
    }
  }

  return icCmmStatOk;
}

/**
**************************************************************************
 * Name: CIccPcsXform::Optimize
 *
 * Purpose:
 *  Analyzes and concatenates/removes transforms in pcs transformation chain
 **************************************************************************
 */
icStatusCMM CIccPcsXform::Optimize()
{
  if (!m_list)
    return icCmmStatBadXform;

  CIccPcsStepList steps = *m_list;
  CIccPcsStepList::iterator next, last;
  bool done=false;

#if 0
  std::string str;
  for (next = steps.begin(); next != steps.end(); next++) {
    next->ptr->dump(str);
  }
  printf("PCS_Steps:\n%s", str.c_str());
#endif

  while (!done) {
    CIccPcsStepList newSteps;
    CIccPcsStepPtr ptr;

    done = true;

    next = steps.begin();
    if (next==steps.end()) {
      *m_list = steps;
      return icCmmStatIdentityXform;
    }
    last = next;
    next++;

    ptr.ptr = last->ptr;

    for (;next!=steps.end(); next++) {
      CIccPcsStep *pStep = ptr.ptr->concat(next->ptr);

      if (pStep) {
        done = false;

        delete ptr.ptr;
        delete next->ptr;
        ptr.ptr = pStep;
      }
      else {
        if (!ptr.ptr->isIdentity()) {
          newSteps.push_back(ptr);
        }
        else {
          // Identity steps are dropped from the optimized chain; delete them
          // so the CIccPcsStep object (and its data buffer) is not leaked.
          delete ptr.ptr;
        }
        ptr.ptr = next->ptr;
      }
    }
    if (!ptr.ptr->isIdentity()) {
      newSteps.push_back(ptr);
    }
    else {
      delete ptr.ptr;
    }

    steps = newSteps;

//     for (next=steps.begin(); next!=steps.end(); next++) {
//       ptr.ptr = next->ptr;
//       done = true;
//     }

  }

  if (!steps.empty()) {
    CIccPcsStepList newSteps;
    CIccPcsStepPtr ptr;

    for (next=steps.begin(); next != steps.end(); next++) {
      ptr.ptr = next->ptr->reduce();
      if (ptr.ptr != next->ptr)
        delete next->ptr;
      newSteps.push_back(ptr);
    }
    steps = newSteps;
  }

  *m_list = steps;

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::GetNewApply
 * 
 * Purpose: 
 *  Allocates a CIccApplyXform based object that can store local data for
 *  processing (needed by CIccPcsStepMpe).
 **************************************************************************
 */
CIccApplyXform *CIccPcsXform::GetNewApply(icStatusCMM &status)
{
  CIccApplyPcsXform *pNew = new (std::nothrow) CIccApplyPcsXform(this);

  if (pNew) {
    if (!pNew->Init()) {
      delete pNew;
      status = icCmmStatAllocErr;
      return NULL;
    }
  }
  else {
    status = icCmmStatAllocErr;
    return NULL;
  }

  CIccPcsStepList::iterator i;
  CIccApplyPcsStep *pStep;

  for (i=m_list->begin(); i!=m_list->end(); i++) {
    pStep = i->ptr->GetNewApply();
    if (!pStep || status != icCmmStatOk) {
      delete pNew;
      return NULL;
    }
    pNew->AppendApplyStep(pStep);
  }

  return pNew;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::MaxChannels
 * 
 * Purpose: 
 *  Returns the maximum number of channels used by PCS xform steps
 *
 *  This is what CIccApplyPcsXform::Init() sizes m_temp1/m_temp2 from, and
 *  CIccApplyPcsXform::Apply() hands those two buffers to every step in the list
 *  except the last, so the count has to cover each step's output width as well as
 *  each step's input width.
 *
 *  The walk used to take GetDstChannels() from the first step only and
 *  GetSrcChannels() from all of them. On a chain whose steps agree -- step i's
 *  output width equalling step i+1's input width -- that reaches the same answer,
 *  which is why it has held up: every intermediate output is also the next step's
 *  input and gets counted in that role. It stops holding as soon as a chain does not
 *  agree with itself, and then the miss is an undersized destination for a step that
 *  is about to write through it. Counting both widths at every step removes the
 *  dependence on that assumption; the result is never smaller than before, so a chain
 *  that sized correctly still sizes the same.
 **************************************************************************
 */
icUInt16Number CIccPcsXform::MaxChannels()
{
  icUInt16Number nMax = 0;
  CIccPcsStepList::const_iterator s;

  for (s = m_list->begin(); s != m_list->end(); s++) {
    if (s->ptr->GetSrcChannels()>nMax)
      nMax = s->ptr->GetSrcChannels();
    if (s->ptr->GetDstChannels()>nMax)
      nMax = s->ptr->GetDstChannels();
  }

  return nMax;
}

/**
 **************************************************************************
 * Name: CIccPcsXform::pushRouteMcs
 * 
 * Purpose: 
 *  Insert PCS step that routes MCS channel data from one profile to another
 **************************************************************************
 */
void CIccPcsXform::pushRouteMcs(CIccTagArray *pSrcChannels, CIccTagArray *pDstChannels, CIccTagNumArray *pDefaults)
{
  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepRouteMcs(pSrcChannels, pDstChannels, pDefaults);
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushLab2ToLab
 *
 * Purpose:
 *  Insert PCS step that converts from V2 Lab internal to actual XYZ
 **************************************************************************
 */
void CIccPcsXform::pushLab2ToLab()
{
  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepLab2ToLab();
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushXyzToLab2
 *
 * Purpose:
 *  Insert PCS step that converts from actual XYZ to V2 Lab internal
 **************************************************************************
 */
void CIccPcsXform::pushLabToLab2()
{
  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepLabToLab2();
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushLab2ToXyz
 * 
 * Purpose: 
 *  Insert PCS step that converts from V2 Lab internal to actual XYZ
 **************************************************************************
 */
void CIccPcsXform::pushLab2ToXyz( IIccProfileConnectionConditions *pPCC)
{
  icFloatNumber xyzWhite[3];
  pPCC->getNormIlluminantXYZ(xyzWhite);

  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepLab2ToXYZ(xyzWhite);
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushXyzToLab2
 * 
 * Purpose: 
 *  Insert PCS step that converts from actual XYZ to V2 Lab internal
 **************************************************************************
 */
void CIccPcsXform::pushXyzToLab2(IIccProfileConnectionConditions *pPCC)
{
  icFloatNumber xyzWhite[3];
  pPCC->getNormIlluminantXYZ(xyzWhite);

  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepXYZToLab2(xyzWhite);
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushLabToXyz
 * 
 * Purpose: 
 *  Insert PCS step that converts from V4 Lab internal to actual XYZ
 **************************************************************************
 */
void CIccPcsXform::pushLabToXyz(IIccProfileConnectionConditions *pPCC)
{
  icFloatNumber xyzWhite[3];
  pPCC->getNormIlluminantXYZ(xyzWhite);

  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepLabToXYZ(xyzWhite);
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushXyzToLab
 * 
 * Purpose: 
 *  Insert PCS step that converts from actual XYZ to V4 Lab internal
 **************************************************************************
 */
void CIccPcsXform::pushXyzToLab( IIccProfileConnectionConditions *pPCC)
{
  icFloatNumber xyzWhite[3];
  pPCC->getNormIlluminantXYZ(xyzWhite);

  CIccPcsStepPtr ptr;

  ptr.ptr = new CIccPcsStepXYZToLab(xyzWhite);
  m_list->push_back(ptr);
}

/**
 **************************************************************************
 * Name: CIccPcsXform::pushScale3
 * 
 * Purpose: 
 *  Insert PCS step that individually scaled three channels (conceptually
 *  equivalent to inserting a 3x3 diagonal matrix).
 **************************************************************************
 */
void CIccPcsXform::pushScale3(icFloatNumber v1, icFloatNumber v2, icFloatNumber v3)
{
  CIccPcsStepScale *scale;
  CIccPcsStepPtr ptr;

  scale = new CIccPcsStepScale(3);
  icFloatNumber *data = scale->data();
  data[0] = v1;
  data[1] = v2;
  data[2] = v3;

  ptr.ptr = scale;
  m_list->push_back(ptr);
}

/**
 **************************************************************************
 * Name: CIccPcsXform::pushXyzToXyzIn
 * 
 * Purpose: 
 *  Insert PCS step that converts from actual XYZ to internal XYZ
 **************************************************************************
 */
void CIccPcsXform::pushXyzToXyzIn()
{
  icFloatNumber scale = (icFloatNumber) (32768.0 / 65535.0);
  pushScale3(scale, scale, scale);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushXyzInToXyz
 * 
 * Purpose: 
 *  Insert PCS step that converts from internal XYZ to actual XYZ
 **************************************************************************
 */
void CIccPcsXform::pushXyzInToXyz()
{
  icFloatNumber scale = (icFloatNumber) (65535.0 / 32768.0);
  return pushScale3(scale, scale, scale);
}


/**
**************************************************************************
* Name: CIccPcsXform::pushXyzToXyzLum
*
* Purpose:
*  Insert PCS step that converts from normalized XYZ to XYZ Luminance
**************************************************************************
*/
void CIccPcsXform::pushXyzToXyzLum(IIccProfileConnectionConditions *pPCC)
{
  icFloatNumber XYZLum[3];
  pPCC->getLumIlluminantXYZ(&XYZLum[0]);

  icFloatNumber scale = XYZLum[1];

  return pushScale3(scale, scale, scale);
}


/**
**************************************************************************
* Name: CIccPcsXform::pushXyzLumToXyz
*
* Purpose:
*  Insert PCS step that converts from XYZ Luminance to normalized XYZ
**************************************************************************
*/
void CIccPcsXform::pushXyzLumToXyz(IIccProfileConnectionConditions *pPCC)
{
  icFloatNumber XYZLum[3];
  pPCC->getLumIlluminantXYZ(&XYZLum[0]);

  // XYZLum[1] is the illuminant luminance (Y) and the divisor for the inverse
  // luminance scale.  A malformed profile can drive it to zero -- e.g. a
  // spectralViewingConditions (svcn) tag whose illuminant Y is zero, or a zero
  // luminanceTag -- which turns 1.0/Y into a divide by zero (UBSAN
  // float-divide-by-zero at this line, issue #1406) yielding an infinite scale
  // that then corrupts every PCS sample fed through pushScale3.  Fall back to an
  // identity scale when the luminance is not a positive, finite value: there is
  // no meaningful luminance normalization to undo, and a no-op leaves the PCS
  // data intact instead of poisoning it with inf/NaN.
  icFloatNumber scale = (std::isfinite(XYZLum[1]) && XYZLum[1] > 0.0f)
                          ? 1.0f / XYZLum[1]
                          : 1.0f;

  return pushScale3(scale, scale, scale);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushXyzToXyzIn
 * 
 * Purpose: 
 *  Insert PCS step that adds an offset to 3 channels.  If bConvertIntXyzOffset
 *  is true the the offset is assumed to be in Internal XYZ format and
 *  will be converted to be an actual XYZ offset.
 **************************************************************************
 */
void CIccPcsXform::pushOffset3(icFloatNumber v1, icFloatNumber v2, icFloatNumber v3, bool bConvertIntXyzOffset/*=true*/)
{
  CIccPcsStepOffset *offset;
  CIccPcsStepPtr ptr;

  offset = new CIccPcsStepOffset(3);
  icFloatNumber *data = offset->data();
  if (bConvertIntXyzOffset) {
    data[0] = v1*65535.0f/32768.0f;
    data[1] = v2*65535.0f/32768.0f;
    data[2] = v3*65535.0f/32768.0f;
  }
  else {
    data[0] = v1;
    data[1] = v2;
    data[2] = v3;
  }

  ptr.ptr = offset;
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushScale
 * 
 * Purpose: 
 *  Insert PCS step that individually n channels (conceptually
 *  equivalent to inserting a nxn diagonal matrix).
 **************************************************************************
 */
void CIccPcsXform::pushScale(icUInt16Number n, const icFloatNumber *vals)
{
  CIccPcsStepScale *scale = new CIccPcsStepScale(n);
  memcpy(scale->data(), vals, n*sizeof(icFloatNumber));
  
  CIccPcsStepPtr ptr;
  ptr.ptr = scale;
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushMatrix
 * 
 * Purpose: 
 *  Insert PCS step defined by a nRows x nCols matrix with specified vals
 **************************************************************************
 */
void CIccPcsXform::pushMatrix(icUInt16Number nRows, icUInt16Number nCols, const icFloatNumber *vals)
{
  CIccPcsStepMatrix *mtx = new CIccPcsStepMatrix(nRows, nCols);
  memcpy(mtx->entry(0), vals, (size_t)nRows*nCols*sizeof(icFloatNumber));

  CIccPcsStepPtr ptr;
  ptr.ptr = mtx;
  m_list->push_back(ptr);
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushXYZConvert
 * 
 * Purpose: 
 *  Insert PCS step that converts from source XYZ colorimetry to dest XYZ
 *  colorimetry accounting for possible changes in illuminant and/or observer.
 *  Luminance matching is also accounted for.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushXYZConvert(CIccXform *pFromXform, CIccXform *pToXform)
{
  IIccProfileConnectionConditions *pSrcPcc = pFromXform->GetConnectionConditions();
  IIccProfileConnectionConditions *pDstPcc = pToXform->GetConnectionConditions();

  if (!pSrcPcc || !pDstPcc)
    return icCmmStatBadConnection;

  //If source and dest observer and illuminant are same then no transform is needed
  if (pSrcPcc->isEquivalentPcc(*pDstPcc)) {
    return icCmmStatOk;
  }

  CIccPcsStepPtr ptr;

  if (!pSrcPcc->isStandardPcc()) {

    CIccTagMultiProcessElement *pMpe = pSrcPcc->getCustomToStandardPcc();

    if (!pMpe || pMpe->NumInputChannels()!=3 || pMpe->NumOutputChannels()!=3)
      return icCmmStatBadSpaceLink;

    ptr.ptr = NULL;

    //push single matrix element as a CIccPcsStepMatrix so it can be optimized
    if (pMpe->NumElements()==1) {
      CIccMultiProcessElement *pElem = pMpe->GetElement(0);
      if (pElem) {
        if (pElem->GetType()==icSigMatrixElemType) {
          CIccMpeMatrix *pMatElem = (CIccMpeMatrix*)pElem;

          const icFloatNumber *pMat = pMatElem->GetMatrix();
          const icFloatNumber *pOffset = pMatElem->GetConstants();
          icUInt16Number inChannels = pMatElem->NumInputChannels();
          icUInt16Number outChannels = pMatElem->NumOutputChannels();

          // The guard above checks the containing MPE's channel counts, not this
          // element's, and the two can disagree in a profile that Validate()
          // rejects but nothing here re-checks. CIccMpeMatrix::SetSize allocates
          // inChannels*outChannels matrix entries and outChannels constants, so a
          // 1x1 element leaves pOffset[1..2] and 8 of the 9 copied matrix entries
          // out of bounds (#2175). PR #632 added this same guard to the
          // standardToCustomPcc arm below; this arm was missed.
          bool offsetsZero = true;
          if (pOffset) {
            for (int i = 0; i < outChannels; ++i) {
              if (pOffset[i] != 0.0) {
                offsetsZero = false;
                break;
              }
            }
          }

          // make sure the matrix is the expected size and offsets are zero
          if (pMat && (inChannels == 3) && (outChannels == 3) && (!pOffset || offsetsZero) ) {
            CIccPcsStepMatrix *pStepMtx = new (std::nothrow) CIccPcsStepMatrix(3, 3);

            if (pStepMtx ) {
              memcpy(pStepMtx->entry(0,0), pMat, 9*sizeof(icFloatNumber));
            }
            ptr.ptr = pStepMtx;
          }
        }
      }
    }

    if (!ptr.ptr) {
      CIccPcsStepMpe *pStepMpe = new (std::nothrow) CIccPcsStepMpe((CIccTagMultiProcessElement*)pMpe->NewCopy());

      if (!pStepMpe)
        return icCmmStatAllocErr;

      if (!pStepMpe->Begin()) {
        delete pStepMpe;
        return icCmmStatBadConnection;
      }

      ptr.ptr = pStepMpe;
    }

    m_list->push_back(ptr);
  }

  if (!pDstPcc->isStandardPcc()) {
 
    CIccTagMultiProcessElement *pMpe = pDstPcc->getStandardToCustomPcc();

    if (!pMpe || pMpe->NumInputChannels()!=3 || pMpe->NumOutputChannels()!=3)
      return icCmmStatBadSpaceLink;

    ptr.ptr = NULL;

    //push single matrix element as a CIccPcsStepMatrix so it can be optimized
    if (pMpe->NumElements()==1) {
      CIccMultiProcessElement *pElem = pMpe->GetElement(0);
      if (pElem) {
        if (pElem->GetType()==icSigMatrixElemType) {
          CIccMpeMatrix *pMatElem = (CIccMpeMatrix*)pElem;

          const icFloatNumber *pMat = pMatElem->GetMatrix();
          const icFloatNumber *pOffset = pMatElem->GetConstants();
          icUInt16Number inChannels = pMatElem->NumInputChannels();
          icUInt16Number outChannels = pMatElem->NumOutputChannels();
          
          // make sure we don't overrun the offset array
          bool offsetsZero = true;
          if (pOffset) {
            for (int i = 0; i < outChannels; ++i) {
              if (pOffset[i] != 0.0) {
                offsetsZero = false;
                break;
              }
            }
          }

          // make sure the matrix is the expected size and offsets are zero
          if (pMat && (inChannels == 3) && (outChannels == 3) && (!pOffset || offsetsZero) ) {
            CIccPcsStepMatrix *pStepMtx = new (std::nothrow) CIccPcsStepMatrix(3, 3);

            if (pStepMtx ) {
              memcpy(pStepMtx->entry(0,0), pMat, 9*sizeof(icFloatNumber));
            }
            ptr.ptr = pStepMtx;
          }
        }
      }
    }

    if (!ptr.ptr) {
      CIccPcsStepMpe *pStepMpe = new (std::nothrow) CIccPcsStepMpe((CIccTagMultiProcessElement*)pMpe->NewCopy());

      if (!pStepMpe)
        return icCmmStatAllocErr;

      if (!pStepMpe->Begin()) {
        delete pStepMpe;
        return icCmmStatBadConnection;
      }

      ptr.ptr = pStepMpe;
    }
 
    m_list->push_back(ptr);
  }

  if (pFromXform->LuminanceMatching()) {
    pushXyzToXyzLum(pSrcPcc);
  }
  if (pToXform->LuminanceMatching()) {
    pushXyzLumToXyz(pDstPcc);
  }
  
  return icCmmStatOk;
}

icStatusCMM CIccPcsXform::pushXYZNormalize(IIccProfileConnectionConditions *pPcc, const icSpectralRange &srcRange, const icSpectralRange &dstRange)
{
  if (!pPcc)
    return icCmmStatInvalidProfile;

  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();
  if (!pView)
    return icCmmStatProfileMissingTag;

  CIccPcsXform tmp;

  icSpectralRange illuminantRange;
  const icFloatNumber *illuminant = pView->getIlluminant(illuminantRange);
  icSpectralRange observerRange;
  const icFloatNumber *observer = pView->getObserver(observerRange);
  if (!illuminant || !observer)
    return icCmmStatInvalidProfile;

  icStatusCMM stat=icCmmStatOk;
    
  //make sure illuminant goes through identical conversion steps
  if (!icSameSpectralRange(srcRange, illuminantRange) || !icSameSpectralRange(dstRange, illuminantRange)) {
    
    if ((stat=tmp.pushSpecToRange(illuminantRange, srcRange))!=icCmmStatOk) {
      return stat;
    }
    if ((stat=tmp.pushSpecToRange(srcRange, dstRange))!=icCmmStatOk) {
      return stat;
    }
    if ((stat=tmp.pushSpecToRange(dstRange, observerRange))!=icCmmStatOk) {
      return stat;
    }
  }
  else {
    if ((stat=tmp.pushSpecToRange(illuminantRange, observerRange))!=icCmmStatOk) {
      return stat;
    }
  }
  tmp.pushMatrix(3, observerRange.steps, observer);

  CIccApplyXform *pApply = tmp.GetNewApply(stat);
  if (pApply) {
    icFloatNumber xyz[3], normxyz[3], pccxyz[3];
    
    if (illuminantRange.steps < 3) {
      delete pApply;
      return icCmmStatInvalidProfile;
    }

    //Get absolute xyz for illuminant and observer
    tmp.Apply(pApply, xyz, illuminant);

    //calculate normalized XYZ
    if (xyz[1] == 0.0f) {   // But don't divide by zero
        delete pApply;      // This can occur when the illuminant and observer don't overlap
        return icCmmStatInvalidProfile; // really bad illuminant and observer
    }
    
    normxyz[0] = xyz[0] / xyz[1];
    normxyz[1] = xyz[1] / xyz[1];
    normxyz[2] = xyz[2] / xyz[1];

    //get desired XYZ from pcc (might be slightly different from calculated normxyz)
    pPcc->getNormIlluminantXYZ(pccxyz);

    if (normxyz[0] == 0.0f || normxyz[1] == 0.0f || normxyz[2] == 0.0f ) {
      delete pApply;
      return icCmmStatInvalidProfile; // really bad illuminant and observer
    }
    
    //push scale factor to normalize XYZ values and correct for difference between calculated and desired XYZ
    pushScale3(pccxyz[0] / (normxyz[0] * xyz[1]),
               pccxyz[1] / (normxyz[1] * xyz[1]),
               pccxyz[2] / (normxyz[2] * xyz[1]));

    delete pApply;
  }
  
  return stat;
}

/**
 **************************************************************************
 * Name: CIccPcsXform::pushRef2Xyz
 * 
 * Purpose: 
 *  Insert PCS step that convert reflectance to XYZ colorimetry defined by the
 *  observer and illuminant accessed through the Profile Connections Conditions
 *  handle pPcc.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushRef2Xyz(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc)
{
  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();

  if (pView) {
    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pView->getIlluminant(illuminantRange);
    icSpectralRange observerRange;
    const icFloatNumber *observer = pView->getObserver(observerRange);
    if (!illuminant || !observer)
      return icCmmStatInvalidProfile;
      
    icStatusCMM stat;
    if ((stat=pushSpecToRange(pProfile->m_Header.spectralRange, illuminantRange))!=icCmmStatOk) {
      return stat;
    }
    
    pushScale(illuminantRange.steps, illuminant);
    if ((stat=pushSpecToRange(illuminantRange, observerRange))!=icCmmStatOk) {
      return stat;
    }
    
    pushMatrix(3, observerRange.steps, observer);

    if ((stat=pushXYZNormalize(pPcc, illuminantRange, illuminantRange))!=icCmmStatOk) {
      return stat;
    }
  }
  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::rangeMap
 * 
 * Purpose: 
 *  This helper function generates a PCS step matrix that can be used to convert
 *  spectral vectors from one spectral range to another using linear interpolation.
 **************************************************************************
 */
CIccPcsStepMatrix *CIccPcsXform::rangeMap(const icSpectralRange &srcRange, const icSpectralRange &dstRange)
{
  if (srcRange.steps != dstRange.steps ||
      srcRange.start != dstRange.start ||
      srcRange.end != dstRange.end) {
    CIccPcsStepMatrix *mtx = new (std::nothrow) CIccPcsStepMatrix(dstRange.steps, srcRange.steps);
    if (!mtx || !mtx->SetRange(srcRange, dstRange))
    {
      delete mtx;
      return NULL;
    }
    return mtx;
  }

  return NULL;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushSpecToRange
 * 
 * Purpose: 
 *  Insert PCS step that res-samples spectral vector data from a source spectral
 *  range to a destination spectral range.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushSpecToRange(const icSpectralRange &srcRange, const icSpectralRange &dstRange)
{
  if (!icSameSpectralRange(srcRange, dstRange)) {
    CIccPcsStepPtr ptr;
    ptr.ptr = rangeMap(srcRange, dstRange);
    
    if (!ptr.ptr)
      return icCmmStatInvalidProfile;

    m_list->push_back(ptr);
  }
  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushApplyIllum
 * 
 * Purpose: 
 *  Insert PCS step that applies an illuminant to incoming spectral transmissive
 *  vectors to . Illuminant from Profile Connection
 *  Conditions will be resampled to match the sampling range of the incoming
 *  vectors.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushApplyIllum(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc)
{
  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();

  if (pView) {
    CIccPcsStepPtr ptr;

    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pView->getIlluminant(illuminantRange);
    if (!illuminant)
      return icCmmStatInvalidProfile;

    CIccPcsStepScale *pScale = new CIccPcsStepScale(illuminantRange.steps);
    memcpy(pScale->data(), illuminant, illuminantRange.steps*sizeof(icFloatNumber));

    if (icSameSpectralRange(pProfile->m_Header.spectralRange, illuminantRange)) {
      ptr.ptr = pScale;
      m_list->push_back(ptr);
    }
    else {
      // Control reaches here only because the ranges differ, so rangeMap() returning
      // NULL can carry just one of its two meanings: not "no conversion is needed" but
      // "a conversion was needed and SetRange() refused to build one" -- it rejects any
      // pair carrying <= 1 step, a non-finite endpoint or a zero-width source span.
      // Skipping the step on that answer is what the outer if() already handles for the
      // identical case; here it silently shortens the chain instead, leaving the scale
      // running at illuminantRange.steps over a vector that is spectralRange.steps wide.
      // Nothing downstream contains that: CIccApplyPcsXform::Init() sizes its
      // temporaries from the steps that survived, and Begin() still reports success.
      // Of the six rangeMap() call sites in this file the other four -- pushSpecToRange()
      // and the three in pushBiRef2Rad()/pushBiRef2Ref() -- already reject a refused map;
      // these two were the last that did not.
      //
      // The return leg is the more damaging of the two to drop, because it is the last
      // step pushed here and so its output is what the caller reads back: without it the
      // chain hands illuminantRange.steps values to a consumer expecting
      // spectralRange.steps, writing past the end of the destination pixel whenever the
      // illuminant is the wider of the two. The two legs also fail independently -- an
      // illuminant declaring a zero-width span is refused as a resampling source while
      // still being usable as a destination -- so both are built before either is pushed,
      // which leaves the step list untouched on a reject the way the sibling sites do.
      CIccPcsStepMatrix *pToIllum = rangeMap(pProfile->m_Header.spectralRange, illuminantRange);
      CIccPcsStepMatrix *pFromIllum = pToIllum ? rangeMap(illuminantRange, pProfile->m_Header.spectralRange) : NULL;

      if (!pToIllum || !pFromIllum) {
        // Nothing has been pushed, so these are still owned here; a step handed to
        // m_list is deleted by the destructor instead.
        delete pToIllum;
        delete pFromIllum;
        delete pScale;
        return icCmmStatInvalidProfile;
      }

      ptr.ptr = pToIllum;
      m_list->push_back(ptr);

      ptr.ptr = pScale;
      m_list->push_back(ptr);

      ptr.ptr = pFromIllum;
      m_list->push_back(ptr);
    }
  }
  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushRad2Xyz
 * 
 * Purpose: 
 *  Insert PCS step that converts from source spectral radiometric vectors to
 *  actual XYZ colorimetry based upon observer information in Profile
 *  Connection Conditions.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushRad2Xyz(CIccProfile* pProfile, IIccProfileConnectionConditions *pPcc, bool bAbsoluteCIEColorimetry)
{
  const CIccTagSpectralViewingConditions *pProfView = pProfile ? pProfile->getPccViewingConditions() : NULL;
  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();
  if (pProfView && pView) {
    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pView->getIlluminant(illuminantRange);
    icSpectralRange observerRange;
    const icFloatNumber *observer = pView->getObserver(observerRange);
    if (!illuminant || !observer)
      return icCmmStatInvalidProfile;

    //Preserve smallest step size
    icFloatNumber spectralSteps = (icFloatNumber)pProfile->m_Header.spectralRange.steps;
    if (spectralSteps < 1.0)
        spectralSteps = 1.0;
    icFloatNumber observerSteps = (icFloatNumber)observerRange.steps;
    if (observerSteps < 1.0)
        observerSteps = 1.0;
    icFloatNumber dPCSStepSize = (icF16toF(pProfile->m_Header.spectralRange.end) -
                                icF16toF(pProfile->m_Header.spectralRange.start))/spectralSteps;
    icFloatNumber dObsStepSize = (icF16toF(observerRange.end) - icF16toF(observerRange.start)) / observerSteps;

    if (dPCSStepSize<dObsStepSize) {
      icFloatNumber *obs = pView->applyRangeToObserver(pProfile->m_Header.spectralRange);
      if (!obs)
        return icCmmStatInvalidProfile;

      pushMatrix(3, pProfile->m_Header.spectralRange.steps, obs);
      free(obs);
    }
    else {
      icStatusCMM stat;
      if ((stat=pushSpecToRange(pProfile->m_Header.spectralRange, observerRange))!=icCmmStatOk) {
        return stat;
      }
    
      pushMatrix(3, observerRange.steps, observer);
    }
    icFloatNumber k;
    if (bAbsoluteCIEColorimetry) {
      k = 683;
    }
    else {
      auto temp = pPcc->getObserverWhiteScaleFactor(illuminant, illuminantRange);
      if (fabs(temp) > 1e-8)
        k = 1.0f / temp;
      else
        k = 1.0;
    }
    pushScale3(k, k, k);
  }
  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushBiRef2Rad
 * 
 * Purpose: 
 *  Insert PCS steps that apply an illuminant to incoming bi-spectral reflectance
 *  matrices to get estimate of light "reflected" by surface.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushBiRef2Rad(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc)
{
  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();

  if (pView) {
    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pView->getIlluminant(illuminantRange);
    if (!illuminant)
      return icCmmStatProfileMissingTag;

    if (icGetColorSpaceType(pProfile->m_Header.spectralPCS)==icSigSparseMatrixSpectralPcsData) {
      CIccPcsStepSrcSparseMatrix *pMtx = new (std::nothrow) CIccPcsStepSrcSparseMatrix(pProfile->m_Header.spectralRange.steps,
                  pProfile->m_Header.biSpectralRange.steps,
                  (icUInt16Number)icGetSpaceSamples((icColorSpaceSignature)pProfile->m_Header.spectralPCS));
      if (!pMtx)
        return icCmmStatAllocErr;

      // rangeMap() folds two different answers into NULL: "no conversion is needed
      // because the ranges are identical", and "a conversion is needed but cannot be
      // built". SetRange() refuses any pair carrying <= 1 step, a non-finite endpoint or
      // a zero-width span, all of which a malformed header can ask for. Only the first
      // answer makes the wholesale copy below correct -- it writes illuminantRange.steps
      // floats into m_vals, which the constructor sized to biSpectralRange.steps. Testing
      // the identical case up front, the way pushSpecToRange() already does, leaves a
      // failed map as an unambiguous reject rather than a copy past the end of m_vals.
      if (!icSameSpectralRange(illuminantRange, pProfile->m_Header.biSpectralRange)) {
        CIccPcsStepMatrix *illumMtx = rangeMap(illuminantRange, pProfile->m_Header.biSpectralRange);
        if (!illumMtx) {
          delete pMtx;
          return icCmmStatInvalidProfile;
        }
        illumMtx->Apply(NULL, pMtx->data(), illuminant);
        delete illumMtx;
      }
      else {
        // The ranges match, so illuminantRange.steps == biSpectralRange.steps and the
        // copy is an exact fit for the buffer.
        memcpy(pMtx->data(), illuminant, illuminantRange.steps*sizeof(icFloatNumber));
      }

      CIccPcsStepPtr ptr;
      ptr.ptr = pMtx;
      m_list->push_back(ptr);

    }
    else {
      CIccPcsStepSrcMatrix *pMtx = new (std::nothrow) CIccPcsStepSrcMatrix(pProfile->m_Header.spectralRange.steps, pProfile->m_Header.biSpectralRange.steps);
      if (!pMtx)
        return icCmmStatAllocErr;

      // Same NULL ambiguity as the sparse branch above, and the same buffer contract:
      // m_vals holds biSpectralRange.steps floats. This is the site reached by the #1677
      // reproducer, where a header declaring biSpectralRange.steps == 0 made SetRange()
      // refuse the pair, so the copy pushed a whole illuminant into a zero-length array.
      if (!icSameSpectralRange(illuminantRange, pProfile->m_Header.biSpectralRange)) {
        CIccPcsStepMatrix *illumMtx = rangeMap(illuminantRange, pProfile->m_Header.biSpectralRange);
        if (!illumMtx) {
          delete pMtx;
          return icCmmStatInvalidProfile;
        }
        illumMtx->Apply(NULL, pMtx->data(), illuminant);
        delete illumMtx;
      }
      else {
        memcpy(pMtx->data(), illuminant, illuminantRange.steps*sizeof(icFloatNumber));
      }

      CIccPcsStepPtr ptr;
      ptr.ptr = pMtx;
      m_list->push_back(ptr);

    }
  }

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushBiRef2Xyz
 * 
 * Purpose: 
 *  Insert PCS step that applies an illuminant to incoming bi-spectral reflectance
 *  matrices to get actual XYZ values.  The illuminant from the Profile
 *  Connection Conditions is re-sampled to match the number of columns in
 *  the incoming matrices.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushBiRef2Xyz(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc)
{
  icStatusCMM stat = pushBiRef2Rad(pProfile, pPcc);
  if (stat!=icCmmStatOk)
    return stat;

  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();

  if (pView) {
    icSpectralRange observerRange;
    const icFloatNumber *observer = pView->getObserver(observerRange);
    if (!observer)
      return icCmmStatInvalidProfile;
    
    if ((stat=pushSpecToRange(pProfile->m_Header.spectralRange, observerRange))!=icCmmStatOk) {
      return stat;
    }
    
    pushMatrix(3, observerRange.steps, observer);
    if ((stat=pushXYZNormalize(pPcc, pProfile->m_Header.biSpectralRange, pProfile->m_Header.spectralRange))!=icCmmStatOk) {
      return stat;
    }
  }
  else {
    return icCmmStatBadConnection;
  }

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccPcsXform::pushBiRef2Ref
 * 
 * Purpose: 
 *  Insert PCS steps that apply an illuminant to incoming bi-spectral reflectance
 *  matrices and then normalizes by the illuminant to get an estimate of
 *  reflectance factor under that illuminant.
 **************************************************************************
 */
icStatusCMM CIccPcsXform::pushBiRef2Ref(CIccProfile *pProfile, IIccProfileConnectionConditions *pPcc)
{
  icStatusCMM stat = pushBiRef2Rad(pProfile, pPcc);
  if (stat!=icCmmStatOk)
    return stat;

  const CIccTagSpectralViewingConditions *pView = pPcc->getPccViewingConditions();

  if (pView) {
    icSpectralRange illuminantRange;
    const icFloatNumber *illuminant = pView->getIlluminant(illuminantRange);
    if (!illuminant)
      return icCmmStatProfileMissingTag;

    CIccPcsStepScale *pScale = new (std::nothrow) CIccPcsStepScale(pProfile->m_Header.spectralRange.steps);

    if (pScale) {
      icFloatNumber *pData = pScale->data();
      int i;

      // Third instance of the NULL ambiguity described in pushBiRef2Rad(). Here the
      // fallback walks illuminant[] to spectralRange.steps, but that array only holds
      // illuminantRange.steps entries, so a refused map reads past its end rather than
      // writing past one. pData is sized spectralRange.steps either way.
      if (!icSameSpectralRange(illuminantRange, pProfile->m_Header.spectralRange)) {
        CIccPcsStepMatrix *illumMtx = rangeMap(illuminantRange, pProfile->m_Header.spectralRange);
        if (!illumMtx) {
          delete pScale;
          return icCmmStatInvalidProfile;
        }
        illumMtx->Apply(NULL, pData, illuminant);
        for (i=0; i<pProfile->m_Header.spectralRange.steps; i++)
          pData[i] = 1.0f / pData[i];

        delete illumMtx;
      }
      else {
        // Ranges match, so this reads exactly illuminantRange.steps entries.
        for (i=0; i<pProfile->m_Header.spectralRange.steps; i++) {
          pData[i] = 1.0f / illuminant[i];
        }
      }

      CIccPcsStepPtr ptr;
      ptr.ptr = pScale;
      m_list->push_back(ptr);
    }
    else
      return icCmmStatAllocErr;
  }
  else
    return icCmmStatBadConnection;

  return icCmmStatOk;
}


#ifdef _DEBUG
//#define DUMPCSSTEPRESULTS
#ifdef DUMPCSSTEPRESULTS
  #define ICCDUMPPIXEL(n, pix) \
  if ((n)<96) { \
    printf("["); \
    int i; \
    for (i=0; i<(n); i++) { \
      if (i && !(i%12)) \
        printf("...\n"); \
      printf(" %.5f", pix[i]); \
    } \
    printf("]\n"); \
  } \
  else { \
    printf("[ BigAray with %d elements]\n", (n)); \
  }
#else
  #define ICCDUMPPIXEL(n, pix)
#endif
#else
  #define ICCDUMPPIXEL(n, pix)
#endif


/**
**************************************************************************
* Name: CIccPcsXform::Apply
* 
* Purpose: 
*  Applies the PcsXfrom steps using the apply pXform data to SrcPixel to get DstPixel
**************************************************************************
*/
void CIccPcsXform::Apply(CIccApplyXform *pXform, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  CIccApplyPcsXform *pApplyXform = (CIccApplyPcsXform*)pXform;
  CIccApplyPcsStepList *pList = pApplyXform->m_list;

  ICCDUMPPIXEL(GetNumSrcSamples(), SrcPixel);

  if (!pList) {
    memcpy(DstPixel, SrcPixel, GetNumSrcSamples()*sizeof(icFloatNumber));
    ICCDUMPPIXEL(GetNumSrcSamples(), DstPixel);
    return;
  }
  
  CIccApplyPcsStepList::iterator s, n;
  s = n =pList->begin();

  if (s==pList->end()) {
    memcpy(DstPixel, SrcPixel, GetNumSrcSamples()*sizeof(icFloatNumber));
    ICCDUMPPIXEL(GetNumSrcSamples(), DstPixel);
    return;
  }
 
  n++;

  if (n==pList->end()) {
    s->ptr->Apply(DstPixel, SrcPixel);
    ICCDUMPPIXEL(s->ptr->GetStep()->GetDstChannels(), DstPixel);
  }
  else {
    const icFloatNumber *src = SrcPixel;
    icFloatNumber *p1 = pApplyXform->m_temp1;
    icFloatNumber *p2 = pApplyXform->m_temp2;
    icFloatNumber *t;

    for (;n!=pList->end(); s=n, n++) {
      s->ptr->Apply(p1, src);
      ICCDUMPPIXEL(s->ptr->GetStep()->GetDstChannels(), p1);
      src=p1;
      t=p1; p1=p2; p2=t;
    }
    s->ptr->Apply(DstPixel, src);
    ICCDUMPPIXEL(s->ptr->GetStep()->GetDstChannels(), DstPixel);
  }
}

/**
**************************************************************************
* Name: CIccPcsStep::GetNewApply
* 
* Purpose: 
*  Allocates a new CIccApplyPcsStep to be used with processing.
**************************************************************************
*/
CIccApplyPcsStep* CIccPcsStep::GetNewApply()
{
  return new (std::nothrow) CIccApplyPcsStep(this);
}


/**
**************************************************************************
* Name: CIccPcsStepIdentity::Apply
* 
* Purpose: 
*  Copies pSrc to pDst
**************************************************************************
*/
void CIccPcsStepIdentity::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  if (pDst != pSrc)
    memcpy(pDst, pSrc, m_nChannels*sizeof(icFloatNumber));
}


/**
**************************************************************************
* Name: CIccPcsStepIdentity::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepIdentity::dump(std::string &str) const
{
  str += "\nCIccPcsStepIdentity\n\n";
}


/**
**************************************************************************
* Name: CIccPcsStepIdentity::CIccPcsStepIdentity
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepRouteMcs::CIccPcsStepRouteMcs(CIccTagArray *pSrcChannels, CIccTagArray *pDstChannels, CIccTagNumArray *pDefaults)
{
  m_nSrcChannels = (icUInt16Number)pSrcChannels->GetSize();
  m_nDstChannels = (icUInt16Number)pDstChannels->GetSize();
  m_Index = new int[m_nDstChannels];
  m_Defaults = new icFloatNumber[m_nDstChannels];

  memset(m_Defaults, 0, m_nDstChannels*sizeof(icFloatNumber));

  if (pDefaults) {
    pDefaults->GetValues(m_Defaults, 0, m_nDstChannels);
  }
  
  
  int i, j;

  for (i=0; i<m_nDstChannels; i++) {
    const icUChar *szDstChan = ((CIccTagUtf8Text*)(pDstChannels->GetIndex(i)))->GetText();
    for (j=0; j<m_nSrcChannels; j++) {
      const icUChar *szSrcChan = ((CIccTagUtf8Text*)(pSrcChannels->GetIndex(j)))->GetText();
      // char *szSrc = (char*)szSrcChan;  // value unused!
      if (!icUtf8StrCmp(szDstChan, szSrcChan))
        break;
    }
    if (j==m_nSrcChannels) {
      m_Index[i] = -1;
    }
    else {
      m_Index[i] = j;
    }
    //printf("%d - %d %s\n", m_Index[i], i, szDstChan);
  }
}


/**
**************************************************************************
* Name: CIccPcsStepIdentity::~CIccPcsStepIdentity
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsStepRouteMcs::~CIccPcsStepRouteMcs()
{
  delete [] m_Index;
  delete [] m_Defaults;
}


/**
**************************************************************************
* Name: CIccPcsStepRouteMcs::isIdentity
* 
* Purpose: 
*  Determines if applying this step will result in negligible change in data
**************************************************************************
*/
bool CIccPcsStepRouteMcs::isIdentity() const
{
  if (m_nSrcChannels!=m_nDstChannels)
    return false;

  int i;
  for (i=0; i<m_nDstChannels; i++) {
    if (m_Index[i]!=i)
      return false;
  }

  return true;
}



/**
**************************************************************************
* Name: CIccPcsStepRouteMcs::Apply
* 
* Purpose: 
*  Copies pSrc to pDst
**************************************************************************
*/
void CIccPcsStepRouteMcs::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  if (pDst != pSrc) {
    int i;
    for (i=0; i<m_nDstChannels; i++) {
      if (m_Index[i]>=0)
        pDst[i] = pSrc[m_Index[i]];
      else
        pDst[i] = m_Defaults[i];
    }
  }
}


/**
**************************************************************************
* Name: CIccPcsStepRouteMcs::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepRouteMcs::dump(std::string &str) const
{
  str += "\nCIccPcsStepRouteMcs\n\n";
}

extern icFloatNumber icD50XYZ[3];

/**
**************************************************************************
* Name: CIccPcsLabStep::isSameWhite
* 
* Purpose: 
*  Determines if this step has same white point as that passed in
**************************************************************************
*/
bool CIccPcsLabStep::isSameWhite(const icFloatNumber *xyzWhite)
{
  return (m_xyzWhite[0]==xyzWhite[0] &&
          m_xyzWhite[1]==xyzWhite[1] &&
          m_xyzWhite[2]==xyzWhite[2]);
}



/**
**************************************************************************
* Name: CIccPcsStepLabToXYZ::CIccPcsStepLabToXYZ
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepLabToXYZ::CIccPcsStepLabToXYZ(const icFloatNumber *xyzWhite/*=NULL*/)
{
  if (xyzWhite) {
    memcpy(m_xyzWhite, xyzWhite, sizeof(m_xyzWhite));
  }
  else {
    memcpy(m_xyzWhite, icD50XYZ, sizeof(m_xyzWhite));
  }
}


/**
**************************************************************************
* Name: CIccPcsStepLabToXYZ::Apply
* 
* Purpose: 
*  Converts from V4 Internal Lab to actual XYZ
**************************************************************************
*/
void CIccPcsStepLabToXYZ::Apply(CIccApplyPcsStep */* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  icFloatNumber Lab[3];

  //lab4 to XYZ
  Lab[0] = pSrc[0] * 100.0f;
  Lab[1] = (icFloatNumber)(pSrc[1]*255.0f - 128.0f);
  Lab[2] = (icFloatNumber)(pSrc[2]*255.0f - 128.0f);

  icLabtoXYZ(pDst, Lab, m_xyzWhite);
}


/**
**************************************************************************
* Name: CIccPcsStepLabToXYZ::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepLabToXYZ::dump(std::string &str) const
{
  str += "\nCIccPcsStepLabToXYZ\n\n";
}


/**
**************************************************************************
* Name: CIccPcsStepLabToXYZ::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is an icPcsStepXyzToLab step resulting in a combined
*  identity transform.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepLabToXYZ::concat(CIccPcsStep *pNext) const
{
  if (pNext) {
    if (pNext->GetType() == icPcsStepXYZToLab) {
      CIccPcsLabStep* pStep = (CIccPcsLabStep*)pNext;
      if (pStep->isSameWhite(m_xyzWhite))
        return new (std::nothrow) CIccPcsStepIdentity(3);
    }
    else if (pNext->GetType() == icPcsStepXYZToLab2) {
      CIccPcsLabStep* pStep = (CIccPcsLabStep*)pNext;
      if (pStep->isSameWhite(m_xyzWhite))
        return new (std::nothrow) CIccPcsStepLabToLab2();
    }
  }
  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab::CIccPcsStepXYZToLab
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepXYZToLab::CIccPcsStepXYZToLab(const icFloatNumber *xyzWhite/*=NULL*/)
{
  if (xyzWhite) {
    memcpy(m_xyzWhite, xyzWhite, sizeof(m_xyzWhite));
  }
  else {
    memcpy(m_xyzWhite, icD50XYZ, sizeof(m_xyzWhite));
  }
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab::Apply
* 
* Purpose: 
*  Converts from actual XYZ to V4 Internal Lab
**************************************************************************
*/
void CIccPcsStepXYZToLab::Apply(CIccApplyPcsStep */* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  icFloatNumber Lab[3];
  icXYZtoLab(Lab, (icFloatNumber*)pSrc, m_xyzWhite);
  //lab4 from XYZ
  pDst[0] = Lab[0] / 100.0f;
  pDst[1] = (icFloatNumber)((Lab[1] + 128.0f) / 255.0f);
  pDst[2] = (icFloatNumber)((Lab[2] + 128.0f) / 255.0f);
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepXYZToLab::dump(std::string &str) const
{
  str += "\nCIccPcsStepXYZToLab\n\n";
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is an icPcsStepLabToXYZ step resulting in a combined
*  identity transform.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepXYZToLab::concat(CIccPcsStep *pNext) const
{
  if (pNext && pNext->GetType()==icPcsStepLabToXYZ) {
    CIccPcsLabStep *pStep = (CIccPcsLabStep *)pNext;
    if (pStep->isSameWhite(m_xyzWhite))
      return new (std::nothrow) CIccPcsStepIdentity(3);
  }
  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepLab2ToXYZ::CIccPcsStepLab2ToXYZ
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepLab2ToXYZ::CIccPcsStepLab2ToXYZ(const icFloatNumber *xyzWhite/*=NULL*/)
{
  if (xyzWhite) {
    memcpy(m_xyzWhite, xyzWhite, sizeof(m_xyzWhite));
  }
  else {
    memcpy(m_xyzWhite, icD50XYZ, sizeof(m_xyzWhite));
  }
}

/**
**************************************************************************
* Name: CIccPcsStepLab2ToXYZ::Apply
* 
* Purpose: 
*  Converts from actual XYZ to V2 Internal Lab
**************************************************************************
*/
void CIccPcsStepLab2ToXYZ::Apply(CIccApplyPcsStep */* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  icFloatNumber Lab[3];

  //lab2 to XYZ
  Lab[0] = pSrc[0] * (65535.0f / 65280.0f) * 100.0f;
  Lab[1] = (icFloatNumber)(pSrc[1] * 65535.0f / 65280.0f * 255.0f - 128.0f);
  Lab[2] = (icFloatNumber)(pSrc[2] * 65535.0f / 65280.0f * 255.0f - 128.0f);

  icLabtoXYZ(pDst, Lab, m_xyzWhite);
}


/**
**************************************************************************
* Name: CIccPcsStepLab2ToXYZ::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepLab2ToXYZ::dump(std::string &str) const
{
  str += "\nCIccPcsStepLab2ToXYZ\n\n";
}


/**
**************************************************************************
* Name: CIccPcsStepLab2ToXYZ::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is an icPcsStepXYZToLab2 step resulting in a combined
*  identity transform.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepLab2ToXYZ::concat(CIccPcsStep *pNext) const
{
  if (pNext) {
    if (pNext->GetType() == icPcsStepXYZToLab2) {
      CIccPcsLabStep* pStep = (CIccPcsLabStep*)pNext;
      if (pStep->isSameWhite(m_xyzWhite))
        return new (std::nothrow) CIccPcsStepIdentity(3);
    }
    else if (pNext->GetType() == icPcsStepXYZToLab) {
      CIccPcsLabStep* pStep = (CIccPcsLabStep*)pNext;
      if (pStep->isSameWhite(m_xyzWhite))
        return new (std::nothrow) CIccPcsStepLab2ToLab();
    }
  }

  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab2::CIccPcsStepXYZToLab2
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepXYZToLab2::CIccPcsStepXYZToLab2(const icFloatNumber *xyzWhite/*=NULL*/)
{
  if (xyzWhite) {
    memcpy(m_xyzWhite, xyzWhite, sizeof(m_xyzWhite));
  }
  else {
    memcpy(m_xyzWhite, icD50XYZ, sizeof(m_xyzWhite));
  }
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab2::Apply
* 
* Purpose: 
*  Converts from V2 Internal Lab to actual XYZ
**************************************************************************
*/
void CIccPcsStepXYZToLab2::Apply(CIccApplyPcsStep */* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  icFloatNumber Lab[3];
  icXYZtoLab(Lab, (icFloatNumber*)pSrc, m_xyzWhite);
  //lab2 from XYZ
  pDst[0] = (Lab[0] / 100.0f) * (65280.0f / 65535.0f);
  pDst[1] = (icFloatNumber)((Lab[1] + 128.0f) / 255.0f) * (65280.0f / 65535.0f);
  pDst[2] = (icFloatNumber)((Lab[2] + 128.0f) / 255.0f) * (65280.0f / 65535.0f);
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab2::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepXYZToLab2::dump(std::string &str) const
{
  str += "\nCIccPcsStepXYZToLab2\n\n";
}


/**
**************************************************************************
* Name: CIccPcsStepXYZToLab2::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is an icPcsStepLab2ToXYZ step resulting in a combined
*  identity transform.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepXYZToLab2::concat(CIccPcsStep *pNext) const
{
  if (pNext && pNext->GetType()==icPcsStepLab2ToXYZ) {
    CIccPcsLabStep *pStep = (CIccPcsLabStep *)pNext;
    if (pStep->isSameWhite(m_xyzWhite))
      return new (std::nothrow) CIccPcsStepIdentity(3);
  }
  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepLabToLab2::Apply
*
* Purpose:
*  Converts from V2 Internal Lab to actual XYZ
**************************************************************************
*/
void CIccPcsStepLabToLab2::Apply(CIccApplyPcsStep* /* pApply */, icFloatNumber* pDst, const icFloatNumber* pSrc) const
{
  pDst[0] = (icFloatNumber)(pSrc[0] * 65280.0f / 65535.0f);
  pDst[1] = (icFloatNumber)(pSrc[1] * 65280.0f / 65535.0f);
  pDst[2] = (icFloatNumber)(pSrc[2] * 65280.0f / 65535.0f);
}


/**
**************************************************************************
* Name: CIccPcsStepLabToLab2::dump
*
* Purpose:
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepLabToLab2::dump(std::string& str) const
{
  str += "\nCIccPcsStepLabToLab2\n\n";
}

/**
**************************************************************************
* Name: CIccPcsStepLabToLab2::concat
*
* Purpose:
*  Determines if this step can be combined with the next step.
*  Checks if next step is an icPcsStepLab2ToLab step resulting in a combined
*  identity transform.
**************************************************************************
*/
CIccPcsStep* CIccPcsStepLabToLab2::concat(CIccPcsStep* pNext) const
{
  if (pNext && pNext->GetType() == icPcsStepLab2ToLab) {
    return new (std::nothrow) CIccPcsStepIdentity(3);
  }
  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepLab2ToLab::Apply
*
* Purpose:
*  Converts from V2 Internal Lab to actual XYZ
**************************************************************************
*/
void CIccPcsStepLab2ToLab::Apply(CIccApplyPcsStep* /* pApply */, icFloatNumber* pDst, const icFloatNumber* pSrc) const
{
  pDst[0] = (icFloatNumber)(pSrc[0] * 65535.0f / 65280.0f);
  pDst[1] = (icFloatNumber)(pSrc[1] * 65535.0f / 65280.0f);
  pDst[2] = (icFloatNumber)(pSrc[2] * 65535.0f / 65280.0f);
}


/**
**************************************************************************
* Name: CIccPcsStepLab2ToLab::dump
*
* Purpose:
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepLab2ToLab::dump(std::string& str) const
{
  str += "\nCIccPcsStepLab2ToLab\n\n";
}


/**
**************************************************************************
* Name: CIccPcsStepLab2ToLab::concat
*
* Purpose:
*  Determines if this step can be combined with the next step.
*  Checks if next step is an icPcsStepLab2ToLab step resulting in a combined
*  identity transform.
**************************************************************************
*/
CIccPcsStep* CIccPcsStepLab2ToLab::concat(CIccPcsStep* pNext) const
{
  if (pNext && pNext->GetType() == icPcsStepLabToLab2) {
    return new (std::nothrow) CIccPcsStepIdentity(3);
  }
  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepOffset::CIccPcsStepOffset
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepOffset::CIccPcsStepOffset(icUInt16Number nChannels)
{
  m_nChannels = nChannels;
  m_vals = new icFloatNumber[nChannels];
}


/**
**************************************************************************
* Name: CIccPcsStepOffset::CIccPcsStepOffset
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsStepOffset::~CIccPcsStepOffset()
{
  delete [] m_vals;
}


/**
**************************************************************************
* Name: CIccPcsStepOffset::Apply
* 
* Purpose: 
*  Added a fixed offset to the pSrc vector passed in
**************************************************************************
*/
void CIccPcsStepOffset::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  if (m_nChannels==3) {
    pDst[0] = m_vals[0] + pSrc[0];
    pDst[1] = m_vals[1] + pSrc[1];
    pDst[2] = m_vals[2] + pSrc[2];
  }
  else {
    int i;
    for (i=0; i<m_nChannels; i++) {
      pDst[i] = m_vals[i] + pSrc[i];
    }
  }
}


/**
**************************************************************************
* Name: CIccPcsStepOffset::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepOffset::dump(std::string &str) const
{
  str += "\nCIccPcsStepOffset\n\n";
  const size_t bufSize = 80;
  char buf[bufSize];
  for (int i=0; i<m_nChannels; i++) {
    snprintf(buf, bufSize, ICCPCSSTEPDUMPFMT, m_vals[i]);
    str += buf;
  }
  str +="\n";
}


/**
**************************************************************************
* Name: CIccPcsStepOffset::Apply
* 
* Purpose: 
*  Creates a new CIccPcsStepOffet step that is the result of adding the
*  offset of this object to the offset of another object.
**************************************************************************
*/
CIccPcsStepOffset *CIccPcsStepOffset::Add(const CIccPcsStepOffset *offset) const
{
  if (offset->m_nChannels != m_nChannels)
    return NULL;

  CIccPcsStepOffset *pNew = new (std::nothrow) CIccPcsStepOffset(m_nChannels);

  if (pNew) {
    int i;
    for (i=0; i<m_nChannels; i++) {
      pNew->m_vals[i] = m_vals[i] + offset->m_vals[i]; 
    }
  }

  return pNew;
}

/**
**************************************************************************
* Name: CIccPcsStepOffset::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is a compatible icPcsStepOffset step resulting in a 
*  single combined offset.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepOffset::concat(CIccPcsStep *pNext) const
{
  if (pNext && pNext->GetType()==icPcsStepOffset && m_nChannels==pNext->GetSrcChannels())
    return Add((const CIccPcsStepOffset*)pNext);

  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepOffset::isIdentity
* 
* Purpose: 
*  Determines if applying this step will result in negligible change in data
**************************************************************************
*/
bool CIccPcsStepOffset::isIdentity() const
{
  int i;
  for (i=0; i<m_nChannels; i++) {
    if (m_vals[i]<-icNearRange || m_vals[i]>icNearRange)
      return false;
  }

  return true;
}


/**
**************************************************************************
* Name: CIccPcsStepScale::CIccPcsStepScale
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepScale::CIccPcsStepScale(icUInt16Number nChannels)
{
  m_nChannels = nChannels;
  m_vals = new icFloatNumber[nChannels];
}


/**
**************************************************************************
* Name: CIccPcsStepScale::~CIccPcsStepScale
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsStepScale::~CIccPcsStepScale()
{
  delete [] m_vals;
}

/**
**************************************************************************
* Name: CIccPcsStepScale::Apply
* 
* Purpose: 
*  Multiplies fixed scale values to the pSrc vector passed in
**************************************************************************
*/
void CIccPcsStepScale::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  if (m_nChannels==3) {
    pDst[0] = m_vals[0] * pSrc[0];
    pDst[1] = m_vals[1] * pSrc[1];
    pDst[2] = m_vals[2] * pSrc[2];
  }
  else {
    int i;
    for (i=0; i<m_nChannels; i++) {
      pDst[i] = m_vals[i] * pSrc[i];
    }
  }
}


/**
**************************************************************************
* Name: CIccPcsStepScale::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepScale::dump(std::string &str) const
{
  str += "\nCIccPcsStepScale\n\n";
  const size_t bufSize = 80;
  char buf[bufSize];
  for (int i=0; i<m_nChannels; i++) {
    snprintf(buf, bufSize,  ICCPCSSTEPDUMPFMT, m_vals[i]);
    str += buf;
  }
  str +="\n";
}


/**
**************************************************************************
* Name: CIccPcsStepScale::Mult
* 
* Purpose: 
*  Creates a new CIccPcsStepScale step that is the result of multiplying the
*  scale of this object to the scale of another object.
**************************************************************************
*/
CIccPcsStepScale *CIccPcsStepScale::Mult(const CIccPcsStepScale *scale) const
{
  if (scale->m_nChannels != m_nChannels)
    return NULL;

  CIccPcsStepScale *pNew = new (std::nothrow) CIccPcsStepScale(m_nChannels);

  if (pNew) {
    int i;
    for (i=0; i<m_nChannels; i++) {
      pNew->m_vals[i] = m_vals[i] * scale->m_vals[i]; 
    }
  }
  return pNew;
}

/**
**************************************************************************
* Name: CIccPcsStepScale::Mult
* 
* Purpose: 
*  Creates a new CIccPcsStepMatrix step that is the result of multiplying the
*  scale of this object to the scale of another matrix.
**************************************************************************
*/
CIccPcsStepMatrix *CIccPcsStepScale::Mult(const CIccPcsStepMatrix *matrix) const
{
  if (matrix->GetSrcChannels() != m_nChannels)
    return NULL;

  CIccPcsStepMatrix *pNew = new (std::nothrow) CIccPcsStepMatrix(matrix->GetDstChannels(), matrix->GetSrcChannels());

  if (pNew) {
    int i, j;
    for (j=0; j<matrix->GetDstChannels(); j++) {
      const icFloatNumber *row = matrix->entry(j);
      icFloatNumber *to=pNew->entry(j);

      for (i=0; i<m_nChannels; i++) {
        to[i] = m_vals[i] * row[i]; 
      }
    }
  }

  return pNew;
}

/**
**************************************************************************
* Name: CIccPcsStepScale::Mult
*
* Purpose:
*  Creates a new CIccPcsStepMatrix step that is the result of multiplying the
*  scale of this object to the scale of another matrix.
**************************************************************************
*/
CIccPcsStepMatrix *CIccPcsStepScale::Mult(const CIccMpeMatrix *matrix) const
{
  if (matrix->NumInputChannels() != m_nChannels)
    return NULL;

  CIccPcsStepMatrix *pNew = new (std::nothrow) CIccPcsStepMatrix(matrix->NumOutputChannels(), matrix->NumInputChannels());

  if (pNew) {
    int i, j;
    icFloatNumber *mtx = matrix->GetMatrix();
    for (j = 0; j < matrix->NumOutputChannels(); j++) {
      const icFloatNumber *row = &mtx[j*matrix->NumInputChannels()];
      icFloatNumber *to = pNew->entry(j);

      for (i = 0; i < m_nChannels; i++) {
        to[i] = m_vals[i] * row[i];
      }
    }
  }

  return pNew;
}


/**
**************************************************************************
* Name: CIccPcsStepScale::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is a compatible icPcsStepScale or icPcsStepMatrix step
*  resulting in a single combined object.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepScale::concat(CIccPcsStep *pNext) const
{
  if (pNext) {
    if (pNext->GetType()==icPcsStepScale && m_nChannels==pNext->GetSrcChannels())
      return Mult((const CIccPcsStepScale*)pNext);
    if (pNext->GetType()==icPcsStepMatrix && m_nChannels==pNext->GetSrcChannels())
      return Mult((const CIccPcsStepMatrix*)pNext);
    if (pNext->GetType() == icPcsStepMpe && m_nChannels == pNext->GetSrcChannels()) {
      CIccPcsStepMpe *pMpe = (CIccPcsStepMpe*)pNext;
      CIccMpeMatrix *pMatrix = pMpe->GetMatrix();
      if (pMatrix)
        return Mult(pMatrix);
    }
  }

  return NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepScale::isIdentity
* 
* Purpose: 
*  Determines if applying this step will result in negligible change in data
**************************************************************************
*/
bool CIccPcsStepScale::isIdentity() const
{
  int i;
  for (i=0; i<m_nChannels; i++) {
    if (m_vals[i]<1.0f-icNearRange || m_vals[i]>1.0f+icNearRange)
      return false;
  }

  return true;
}



/**
**************************************************************************
* Name: CIccPcsStepMatrix::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepMatrix::dump(std::string &str) const
{
  str += "\nCIccPcsStepMatrix\n\n";
  dumpMtx(str);
}


/**
**************************************************************************
* Name: CIccPcsStepMatrix::Mult
* 
* Purpose: 
*  Creates a new CIccPcsStepMatrix step that is the result of multiplying the
*  matrix of this object to the scale of another object.
**************************************************************************
*/
CIccPcsStepMatrix *CIccPcsStepMatrix::Mult(const CIccPcsStepScale *scale) const
{
  icUInt16Number mCols = scale->GetSrcChannels();
  // icUInt16Number mRows = mCols;  // value unused - is the loop correct?

  if (m_nRows != mCols)
    return NULL;

  CIccPcsStepMatrix *pNew = new (std::nothrow) CIccPcsStepMatrix(m_nRows, m_nCols);
  const icFloatNumber *data = scale->data();

  if (pNew) {
    int i, j;
    for (j=0; j<m_nRows; j++) {
      const icFloatNumber *row = entry(j);
      icFloatNumber *to = pNew->entry(j);
      for (i=0; i<m_nCols; i++) {
        to[i] = data[j] * row[i];
      }
    }
  }

  return pNew;
}

/**
**************************************************************************
* Name: CIccPcsStepMatrix::Mult
* 
* Purpose: 
*  Creates a new CIccPcsStepMatrix step that is the result of concatentating
*  another matrix with this matrix. (IE result = matrix * this).
**************************************************************************
*/
CIccPcsStepMatrix *CIccPcsStepMatrix::Mult(const CIccPcsStepMatrix *matrix) const
{
  icUInt16Number mCols = matrix->m_nCols;
  icUInt16Number mRows = matrix->m_nRows;

  if (m_nRows != mCols)
    return NULL;

  CIccPcsStepMatrix *pNew = new (std::nothrow) CIccPcsStepMatrix(mRows, m_nCols);

  if (pNew) {
    int i, j, k;
    for (j=0; j<mRows; j++) {
      const icFloatNumber *row = matrix->entry(j);
      for (i=0; i<m_nCols; i++) {
        icFloatNumber *to = pNew->entry(j, i);
        const icFloatNumber *from = entry(0, i);

        *to = 0.0f;
        for (k=0; k<m_nRows; k++) {
          *to += row[k] * (*from);
          from += m_nCols;
        }
      }
    }
  }

  return pNew;
}


/**
**************************************************************************
* Name: CIccPcsStepMatrix::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is a compatible icPcsStepScale or icPcsStepMatrix step
*  resulting in a single combined object.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepMatrix::concat(CIccPcsStep *pNext) const
{
  if (pNext) {
    if (pNext->GetType()==icPcsStepScale && GetDstChannels()==pNext->GetSrcChannels())
      return Mult((const CIccPcsStepScale*)pNext);
    if (pNext->GetType()==icPcsStepMatrix && GetDstChannels()==pNext->GetSrcChannels())
      return Mult((const CIccPcsStepMatrix*)pNext);
  }

  return NULL;
}

/**
**************************************************************************
* Name: CIccPcsStepMatrix::concat
* 
* Purpose: 
*  Determines if this step can be combined with the next step.
*  Checks if next step is a compatible icPcsStepScale or icPcsStepMatrix step
*  resulting in a single combined object.
**************************************************************************
*/
CIccPcsStep *CIccPcsStepMatrix::reduce() const
{
  int nVals = m_nRows*m_nCols;
  int nNonZeros = 0;
  int i;

  for (i=0; i<nVals; i++) {
    icFloatNumber v = m_vals[i];
    if (icNotZero(v))
      nNonZeros++;
  }
  if (nNonZeros<nVals*3/4) {
    icUInt32Number nMatrixBytes = CIccSparseMatrix::MemSize(nNonZeros, m_nRows, sizeof(icFloatNumber))+4*sizeof(icFloatNumber);
    CIccPcsStepSparseMatrix *pMtx = new CIccPcsStepSparseMatrix(m_nRows, m_nCols, nMatrixBytes);
    CIccSparseMatrix mtx(pMtx->data(), nMatrixBytes);
    mtx.Init(m_nRows, m_nCols, true);
    mtx.FillFromFullMatrix(m_vals);
    return pMtx;
  }

  return (CIccPcsStep*)this;
}



/**
**************************************************************************
* Name: CIccPcsStepMpe::CIccPcsStepMpe
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepMpe::CIccPcsStepMpe(CIccTagMultiProcessElement *pMpe)
{
  m_pMpe = pMpe;
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::~CIccPcsStepMpe
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsStepMpe::~CIccPcsStepMpe()
{
  delete m_pMpe;
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::GetNewApply
* 
* Purpose: 
*  Allocates a new CIccApplyPcsStep to be used with processing.
**************************************************************************
*/
CIccApplyPcsStep* CIccPcsStepMpe::GetNewApply()
{
  CIccApplyPcsStepMpe *rv = new (std::nothrow) CIccApplyPcsStepMpe(this, m_pMpe->GetNewApply());

  return rv;
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::Apply
* 
* Purpose: 
*  Applies a MultiProcessingElement to a Source vector to get a Dest vector
**************************************************************************
*/
void CIccPcsStepMpe::Apply(CIccApplyPcsStep *pApply, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  CIccApplyPcsStepMpe *pMpeApply = (CIccApplyPcsStepMpe*)pApply;

  m_pMpe->Apply(pMpeApply->m_pApply, pDst, pSrc);
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepMpe::dump(std::string &str) const
{
  str += "\nCIccPcsStepMpe\n\n";
  m_pMpe->Describe(str, 100); // TODO propogate nVerboseness 
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::isIdentity
* 
* Purpose: 
*  Determines if applying this step will obviously result in no change in data
**************************************************************************
*/
bool CIccPcsStepMpe::isIdentity() const
{
  if (!m_pMpe || !m_pMpe->NumElements())
    return true;
  return false;
}

/**
**************************************************************************
* Name: CIccPcsStepMpe::GetSrcChannels
* 
* Purpose: 
*  Returns the number of channels of data required going into the multi-
*  processing element
**************************************************************************
*/
icUInt16Number CIccPcsStepMpe::GetSrcChannels() const
{
  return m_pMpe->NumInputChannels();
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::GetDstChannels
* 
* Purpose: 
*  Returns the number of channels of data coming out of the multi-
*  processing element
**************************************************************************
*/
icUInt16Number CIccPcsStepMpe::GetDstChannels() const
{
  return m_pMpe->NumOutputChannels();
}


/**
**************************************************************************
* Name: CIccPcsStepMpe::GetMatrix()
*
* Purpose:
*  Returns single CIccMpeMatrix element associated with PCS step or
*  NULL if the MPE is more complex
**************************************************************************
*/
CIccMpeMatrix *CIccPcsStepMpe::GetMatrix() const
{
  //Must be single element
  if (m_pMpe->NumElements() == 1) {
    CIccMultiProcessElement *pElem = m_pMpe->GetElement(0);
    //Must be a matrix
    if (pElem && pElem->GetType() == icSigMatrixElemType) {
      CIccMpeMatrix *pMtx = (CIccMpeMatrix*)pElem;

      //Should not apply any constants
      if (!pMtx->GetConstants() || !pMtx->GetApplyConstants())
        return pMtx;
    }
  }

  return NULL;
}




/**
**************************************************************************
* Name: CIccPcsStepMpe::Begin
* 
* Purpose: 
*  Initializes multi-processing element for processing.  Must be called before
*  Apply is called
**************************************************************************
*/
bool CIccPcsStepMpe::Begin()
{
  return m_pMpe->Begin();
}


/**
**************************************************************************
* Name: CIccPcsStepSrcMatrix::CIccPcsStepSrcMatrix
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepSrcMatrix::CIccPcsStepSrcMatrix(icUInt16Number nRows, icUInt16Number nCols)
{
  m_nRows = nRows;
  m_nCols = nCols;
  m_vals = new icFloatNumber[nCols];
}


/**
**************************************************************************
* Name: CIccPcsStepSrcMatrix::~CIccPcsStepSrcMatrix
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsStepSrcMatrix::~CIccPcsStepSrcMatrix()
{
  delete[] m_vals;
}


/**
**************************************************************************
* Name: CIccPcsStepSrcMatrix::Apply
* 
* Purpose: 
*  Multiplies illuminant stored in m_vals by pSrc matrix passed in resulting
*  in a pDst vector
**************************************************************************
*/
void CIccPcsStepSrcMatrix::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  int i, j;
  const icFloatNumber *row = pSrc;
  for (j=0; j<m_nRows; j++) {
    pDst[j] = 0.0f;
    for (i=0; i<m_nCols; i++) {
      pDst[j] += row[i] * m_vals[i];
    }
    row += m_nCols;
  }
}


/**
**************************************************************************
* Name: CIccPcsStepSrcMatrix::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepSrcMatrix::dump(std::string &str) const
{
  str += "\nCIccPcsStepSrcMatrix\n\n";
  const size_t bufSize = 80;
  char buf[bufSize];
  for (int i=0; i<m_nCols; i++) {
    snprintf(buf, bufSize, ICCPCSSTEPDUMPFMT, m_vals[i]);
    str += buf;
  }
  str += "\n";
}


/**
**************************************************************************
* Name: CIccPcsStepSparseMatrix::CIccPcsStepSparseMatrix
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepSparseMatrix::CIccPcsStepSparseMatrix(icUInt16Number nRows, icUInt16Number nCols, icUInt32Number nBytesPerMatrix)
{
  m_nRows = nRows;
  m_nCols = nCols;
  m_nBytesPerMatrix = nBytesPerMatrix;
  m_nChannels = 0;
  m_vals = new icFloatNumber[m_nBytesPerMatrix/sizeof(icFloatNumber)];
  m_pMtx = NULL;   // built by BeginStep(), once m_vals has been populated
}


/**
**************************************************************************
* Name: CIccPcsStepSparseMatrix::~CIccPcsStepSparseMatrix
*
* Purpose:
*  Destructor
**************************************************************************
*/
CIccPcsStepSparseMatrix::~CIccPcsStepSparseMatrix()
{
  delete m_pMtx;
  delete [] m_vals;
}


/**
**************************************************************************
* Name: CIccPcsStepSparseMatrix::BeginStep
*
* Purpose:
*  Builds the sparse matrix wrapper once, before any Apply().
*
*  The matrix is parsed from m_vals, which is fixed by the time the step list is
*  built, so the CIccSparseMatrix it produces is identical for every pixel.
*  Apply() used to construct one per pixel with bInitFromData=true, and
*  CIccSparseMatrix::Init() (IccSparseMatrix.cpp:145) unconditionally deletes and
*  re-news its m_Data accessor -- so that was a heap allocation and free on every
*  pixel, plus a dimension re-parse and the 4096-dimension bounds test.
*
*  It is held on the step rather than in a CIccApplyPcsStep because it is
*  immutable from here on and MultiplyVector() is const, so every thread can
*  share the one instance. Contrast CIccPcsStepSrcSparseMatrix, whose matrix
*  wraps per-pixel source data and therefore cannot be shared.
**************************************************************************
*/
bool CIccPcsStepSparseMatrix::BeginStep()
{
  // Idempotent: BeginStep() can be reached more than once, and rebuilding from
  // the same m_vals yields the same matrix.
  delete m_pMtx;

  m_pMtx = new (std::nothrow) CIccSparseMatrix((icUInt8Number*)m_vals,
                                               m_nBytesPerMatrix,
                                               icSparseMatrixFloatNum, true);
  return m_pMtx != NULL;
}


/**
**************************************************************************
* Name: CIccPcsStepSparseMatrix::Apply
* 
* Purpose: 
*  Multiplies illuminant stored in m_vals by pSrc matrix passed in resulting
*  in a pDst vector
**************************************************************************
*/
void CIccPcsStepSparseMatrix::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  // Matrix built once in BeginStep(). This used to construct a CIccSparseMatrix
  // here, which allocated and freed on every pixel -- see BeginStep().
  if (m_pMtx)
    m_pMtx->MultiplyVector(pDst, pSrc);
}


/**
**************************************************************************
* Name: CIccPcsStepSparseMatrix::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepSparseMatrix::dump(std::string &str) const
{
  str += "\nCIccPcsStepSparseMatrix\n\n";
//   char buf[80];
//   for (int i=0; i<m_nCols; i++) {
//     sprintf(buf, ICCPCSSTEPDUMPFMT, m_vals[i]);
//     str += buf;
//   }
//   str += "\n";
}


/**
**************************************************************************
* Name: CIccPcsStepSrcSparseMatrix::CIccPcsStepSrcSparseMatrix
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccPcsStepSrcSparseMatrix::CIccPcsStepSrcSparseMatrix(icUInt16Number nRows, icUInt16Number nCols, icUInt16Number nChannels)
{
  m_nRows = nRows;
  m_nCols = nCols;
  m_nChannels = nChannels;
  m_nBytesPerMatrix = nChannels * sizeof(icFloatNumber);

  m_vals = new icFloatNumber[nCols];
}


/**
**************************************************************************
* Name: CIccPcsStepSrcSparseMatrix::~CIccPcsSrcStepSparseMatrix
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccPcsStepSrcSparseMatrix::~CIccPcsStepSrcSparseMatrix()
{
  delete [] m_vals;
}


/**
**************************************************************************
* Name: CIccPcsStepSrcSparseMatrix::Apply
* 
* Purpose: 
*  Multiplies illuminant stored in m_vals by pSrc matrix passed in resulting
*  in a pDst vector
**************************************************************************
*/
void CIccPcsStepSrcSparseMatrix::Apply(CIccApplyPcsStep * /* pApply */, icFloatNumber *pDst, const icFloatNumber *pSrc) const
{
  CIccSparseMatrix mtx((icUInt8Number*)pSrc, m_nBytesPerMatrix, icSparseMatrixFloatNum, true);

  mtx.MultiplyVector(pDst, m_vals);
}


/**
**************************************************************************
* Name: CIccPcsStepSrcSparseMatrix::dump
* 
* Purpose: 
*  dumps the context of the step
**************************************************************************
*/
void CIccPcsStepSrcSparseMatrix::dump(std::string &str) const
{
  str += "\nCIccPcsStepSrcSparseMatrix\n\n";
  const size_t bufSize = 80;
  char buf[bufSize];
  for (int i=0; i<m_nCols; i++) {
    snprintf(buf, bufSize, ICCPCSSTEPDUMPFMT, m_vals[i]);
    str += buf;
  }
  str += "\n";
}


/**
**************************************************************************
* Name: CIccXformMonochrome::CIccXformMonochrome
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccXformMonochrome::CIccXformMonochrome()
{
	m_Curve = NULL;
	m_ApplyCurvePtr = NULL;
	m_bFreeCurve = false;
}

/**
**************************************************************************
* Name: CIccXformMonochrome::~CIccXformMonochrome
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccXformMonochrome::~CIccXformMonochrome()
{
	if (m_bFreeCurve) {
		delete m_Curve;
	}
}

/**
**************************************************************************
* Name: CIccXformMonochrome::Begin
* 
* Purpose: 
*  Does the initialization of the Xform before Apply() is called.
*  Must be called before Apply().
*
**************************************************************************
*/
icStatusCMM CIccXformMonochrome::Begin()
{
	icStatusCMM status;

	status = CIccXform::Begin();
	if (status != icCmmStatOk)
		return status;

	m_ApplyCurvePtr = NULL;

	if (m_bInput) {
		m_Curve = GetCurve(icSigGrayTRCTag);

		if (!m_Curve) {
			return icCmmStatProfileMissingTag;
		}
	}
	else {
		m_Curve = GetInvCurve(icSigGrayTRCTag);
		m_bFreeCurve = true;

		if (!m_Curve) {
			return icCmmStatProfileMissingTag;
		}
	}

	m_Curve->Begin();
	if (!m_Curve->IsIdentity()) {
		m_ApplyCurvePtr = m_Curve;
	}

	// Apply() used to rebuild this on every pixel, in both directions, from
	// compile-time constants: icXyzToPcs plus, for a Lab PCS, XyzToLab and its
	// three cube roots, behind a virtual UseLegacyPCS() call. Nothing in it
	// depends on the source colour, and both m_pProfile->m_Header.pcs and
	// UseLegacyPCS() are fixed by the time Begin() runs.
	//
	// Idempotent: recomputing from the same constants yields the same values, so
	// reaching Begin() a second time is harmless.
	m_PcsWhite[0] = icFloatNumber(icPerceptualRefWhiteX);
	m_PcsWhite[1] = icFloatNumber(icPerceptualRefWhiteY);
	m_PcsWhite[2] = icFloatNumber(icPerceptualRefWhiteZ);

	icXyzToPcs(m_PcsWhite);

	if (m_pProfile->m_Header.pcs==icSigLabData) {
		if (UseLegacyPCS()) {
			CIccPCSUtil::XyzToLab2(m_PcsWhite, m_PcsWhite, true);
		}
		else {
			CIccPCSUtil::XyzToLab(m_PcsWhite, m_PcsWhite, true);
		}
	}

	return icCmmStatOk;
}

/**
**************************************************************************
* Name: CIccXformMonochrome::Apply
* 
* Purpose: 
*  Does the actual application of the Xform.
*  
* Args:
*  pApply = ApplyXform object containing temporary storage used during Apply
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
void CIccXformMonochrome::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
	icFloatNumber Pixel[3];
  
  if (m_bSrcPcsConversion)
	  SrcPixel = CheckSrcAbs(pApply, SrcPixel);

	// m_PcsWhite is computed once in Begin(). Both branches below used to rebuild
	// it here on every pixel -- icXyzToPcs, and for a Lab PCS XyzToLab's three
	// cube roots behind a virtual UseLegacyPCS() call -- entirely from constants.
	if (m_bInput) {
		Pixel[0] = SrcPixel[0];

		if (m_ApplyCurvePtr) {
			Pixel[0] = m_ApplyCurvePtr->Apply(Pixel[0]);
		}

		DstPixel[0] = m_PcsWhite[0] * Pixel[0];
		DstPixel[1] = m_PcsWhite[1] * Pixel[0];
		DstPixel[2] = m_PcsWhite[2] * Pixel[0];
	}
	else {
		// The divide is kept rather than turned into a precomputed reciprocal:
		// x/w and x*(1/w) are not bit-identical, and the cube roots hoisted above
		// dominate a single division. Preserving exact output keeps the harness
		// checksum usable as a strict equality oracle for the rest of this branch.
		//
		// The header test selects which source component to read rather than
		// computing anything, so it stays; folding it into another member would
		// buy nothing measurable.
		if (m_pProfile->m_Header.pcs==icSigLabData) {
			DstPixel[0] = SrcPixel[0]/m_PcsWhite[0];
		}
		else {
			DstPixel[0] = SrcPixel[1]/m_PcsWhite[1];
		}

		if (m_ApplyCurvePtr) {
			DstPixel[0] = m_ApplyCurvePtr->Apply(DstPixel[0]);
		}
	}

  if (m_bDstPcsConversion)
	  CheckDstAbs(DstPixel);
}

/**
**************************************************************************
* Name: CIccXformMonochrome::GetCurve
* 
* Purpose: 
*  Gets the curve having the passed signature, from the profile.
*  
* Args:
*  sig = signature of the curve to be found
*
* Return:
*  Pointer to the curve.
**************************************************************************
*/
CIccCurve *CIccXformMonochrome::GetCurve(icSignature sig) const
{
	CIccTag *pTag = m_pProfile->FindTag(sig);

	if (pTag && (pTag->GetType()==icSigCurveType || pTag->GetType()==icSigParametricCurveType)) {
		return (CIccCurve*)pTag;
	}

	return NULL;
}

/**
**************************************************************************
* Name: CIccXformMonochrome::GetInvCurve
* 
* Purpose: 
*  Gets the inverted curve having the passed signature, from the profile.
*  
* Args:
*  sig = signature of the curve to be inverted
*
* Return:
*  Pointer to the inverted curve.
**************************************************************************
*/
CIccCurve *CIccXformMonochrome::GetInvCurve(icSignature sig) const
{
	CIccCurve *pCurve;
	CIccTagCurve *pInvCurve;

	if (!(pCurve = GetCurve(sig)))
		return NULL;

	pCurve->Begin();

	pInvCurve = new (std::nothrow) CIccTagCurve(2048);
    if (!pInvCurve)
      return NULL;

	int i;
	icFloatNumber x;
	icFloatNumber *Lut = &(*pInvCurve)[0];

	for (i=0; i<2048; i++) {
		x=(icFloatNumber)i / 2047;

		Lut[i] = pCurve->Find(x);
	}

	return pInvCurve;
}

/**
**************************************************************************
* Name: CIccXformMonochrome::ExtractInputCurves
* 
* Purpose: 
*  Gets the input curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the input curves.
**************************************************************************
*/
LPIccCurve* CIccXformMonochrome::ExtractInputCurves()
{
	if (m_bInput) {
		if (m_Curve) {
			LPIccCurve* Curve = new (std::nothrow) LPIccCurve[1];
            if (Curve) {
			  Curve[0] = (LPIccCurve)(m_Curve->NewCopy());
			  m_ApplyCurvePtr = NULL;
            }
			return Curve;
		}
	}

	return NULL;
}

/**
**************************************************************************
* Name: CIccXformMonochrome::ExtractOutputCurves
* 
* Purpose: 
*  Gets the output curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the output curves.
**************************************************************************
*/
LPIccCurve* CIccXformMonochrome::ExtractOutputCurves()
{
	if (!m_bInput) {
		if (m_Curve) {
			LPIccCurve* Curve = new (std::nothrow) LPIccCurve[1];
            if (Curve) {
			  Curve[0] = (LPIccCurve)(m_Curve->NewCopy());
			  m_ApplyCurvePtr = NULL;
            }
			return Curve;
		}
	}

	return NULL;
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::CIccXformMatrixTRC
 * 
 * Purpose: 
 *  Constructor
 **************************************************************************
 */
CIccXformMatrixTRC::CIccXformMatrixTRC() : m_e{}
{
  m_Curve[0] = m_Curve[1] = m_Curve[2] = NULL;
  m_ApplyCurvePtr = NULL;
  m_bFreeCurve = false;
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::~CIccXformMatrixTRC
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccXformMatrixTRC::~CIccXformMatrixTRC()
{
  if (m_bFreeCurve) {
    delete m_Curve[0];
    delete m_Curve[1];
    delete m_Curve[2];
  }
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::Begin
 * 
 * Purpose: 
 *  Does the initialization of the Xform before Apply() is called.
 *  Must be called before Apply().
 *
 **************************************************************************
 */
icStatusCMM CIccXformMatrixTRC::Begin()
{
  icStatusCMM status;
  const CIccTagXYZ *pXYZ;

  status = CIccXform::Begin();
  if (status != icCmmStatOk)
    return status;

  pXYZ = GetColumn(icSigRedMatrixColumnTag);
  if (!pXYZ) {
    return icCmmStatProfileMissingTag;
  }

  m_e[0] = icFtoD((*pXYZ)[0].X);
  m_e[3] = icFtoD((*pXYZ)[0].Y);
  m_e[6] = icFtoD((*pXYZ)[0].Z);

  pXYZ = GetColumn(icSigGreenMatrixColumnTag);
  if (!pXYZ) {
    return icCmmStatProfileMissingTag;
  }

  m_e[1] = icFtoD((*pXYZ)[0].X);
  m_e[4] = icFtoD((*pXYZ)[0].Y);
  m_e[7] = icFtoD((*pXYZ)[0].Z);

  pXYZ = GetColumn(icSigBlueMatrixColumnTag);
  if (!pXYZ) {
    return icCmmStatProfileMissingTag;
  }

  m_e[2] = icFtoD((*pXYZ)[0].X);
  m_e[5] = icFtoD((*pXYZ)[0].Y);
  m_e[8] = icFtoD((*pXYZ)[0].Z);

  m_ApplyCurvePtr = NULL;

  if (m_bInput) {
    m_Curve[0] = GetCurve(icSigRedTRCTag);
    m_Curve[1] = GetCurve(icSigGreenTRCTag);
    m_Curve[2] = GetCurve(icSigBlueTRCTag);

    if (!m_Curve[0] || !m_Curve[1] || !m_Curve[2]) {
      return icCmmStatProfileMissingTag;
    }

  }
  else {
    if (m_pProfile->m_Header.pcs!=icSigXYZData) {
      return icCmmStatBadSpaceLink;
    }

    m_Curve[0] = GetInvCurve(icSigRedTRCTag);
    m_Curve[1] = GetInvCurve(icSigGreenTRCTag);
    m_Curve[2] = GetInvCurve(icSigBlueTRCTag);

    m_bFreeCurve = true;

    if (!m_Curve[0] || !m_Curve[1] || !m_Curve[2]) {
      return icCmmStatProfileMissingTag;
    }

    if (!icMatrixInvert3x3(m_e)) {
      return icCmmStatInvalidProfile;
    }
  }

  m_Curve[0]->Begin();
  m_Curve[1]->Begin();
  m_Curve[2]->Begin();

  if (!m_Curve[0]->IsIdentity() || !m_Curve[1]->IsIdentity() || !m_Curve[2]->IsIdentity()) {
    m_ApplyCurvePtr = m_Curve;
  }
  
  return icCmmStatOk;
}


static icFloatNumber XYZScale(icFloatNumber v)
{
  v = (icFloatNumber)(v * 32768.0 / 65535.0);
  return v;
}

static icFloatNumber XYZDescale(icFloatNumber v)
{
  return (icFloatNumber)(v * 65535.0 / 32768.0);
}

static icFloatNumber RGBClip(icFloatNumber v, CIccCurve *pCurve)
{
  if (v<=0)
    return(pCurve->Apply(0));
  else if (v>=1.0)
    return (pCurve->Apply(1.0));

  return pCurve->Apply(v);
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::Apply
 * 
 * Purpose: 
 *  Does the actual application of the Xform.
 *  
 * Args:
 *  pApply = ApplyXform object containging temporary storage used during Apply
 *  DstPixel = Destination pixel where the result is stored,
 *  SrcPixel = Source pixel which is to be applied.
 **************************************************************************
 */
void CIccXformMatrixTRC::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  icFloatNumber Pixel[3];

  if (m_bSrcPcsConversion)
    SrcPixel = CheckSrcAbs(pApply, SrcPixel);

  Pixel[0] = SrcPixel[0];
  Pixel[1] = SrcPixel[1];
  Pixel[2] = SrcPixel[2];

  if (m_bInput) {

    double LinR, LinG, LinB;
    if (m_ApplyCurvePtr) {
      LinR = m_ApplyCurvePtr[0]->Apply(Pixel[0]);
      LinG = m_ApplyCurvePtr[1]->Apply(Pixel[1]);
      LinB = m_ApplyCurvePtr[2]->Apply(Pixel[2]);
    }
    else {
      LinR = Pixel[0];
      LinG = Pixel[1];
      LinB = Pixel[2];
    }

    DstPixel[0] = XYZScale((icFloatNumber)(m_e[0] * LinR + m_e[1] * LinG + m_e[2] * LinB));
    DstPixel[1] = XYZScale((icFloatNumber)(m_e[3] * LinR + m_e[4] * LinG + m_e[5] * LinB));
    DstPixel[2] = XYZScale((icFloatNumber)(m_e[6] * LinR + m_e[7] * LinG + m_e[8] * LinB));
  }
  else {
    double X = XYZDescale(Pixel[0]);
    double Y = XYZDescale(Pixel[1]);
    double Z = XYZDescale(Pixel[2]);

    if (m_ApplyCurvePtr) {
      DstPixel[0] = RGBClip((icFloatNumber)(m_e[0] * X + m_e[1] * Y + m_e[2] * Z), m_ApplyCurvePtr[0]);
      DstPixel[1] = RGBClip((icFloatNumber)(m_e[3] * X + m_e[4] * Y + m_e[5] * Z), m_ApplyCurvePtr[1]);
      DstPixel[2] = RGBClip((icFloatNumber)(m_e[6] * X + m_e[7] * Y + m_e[8] * Z), m_ApplyCurvePtr[2]);
    }
    else {
      DstPixel[0] = (icFloatNumber)(m_e[0] * X + m_e[1] * Y + m_e[2] * Z);
      DstPixel[1] = (icFloatNumber)(m_e[3] * X + m_e[4] * Y + m_e[5] * Z);
      DstPixel[2] = (icFloatNumber)(m_e[6] * X + m_e[7] * Y + m_e[8] * Z);
    }
  }

  if (m_bDstPcsConversion)
    CheckDstAbs(DstPixel);
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::GetCurve
 * 
 * Purpose: 
 *  Gets the curve having the passed signature, from the profile.
 *  
 * Args:
 *  sig = signature of the curve to be found
 *
 * Return:
 *  Pointer to the curve.
 **************************************************************************
 */
CIccCurve *CIccXformMatrixTRC::GetCurve(icSignature sig) const
{
  CIccTag *pTag = m_pProfile->FindTag(sig);

  if (pTag && (pTag->GetType()==icSigCurveType || pTag->GetType()==icSigParametricCurveType)) {
    return (CIccCurve*)pTag;
  }

  return NULL;
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::GetColumn
 * 
 * Purpose: 
 *  Gets the XYZ tag from the profile.
 *  
 * Args:
 *  sig = signature of the XYZ tag to be found.
 * 
 * Return:
 *  Pointer to the XYZ tag.
 **************************************************************************
 */
CIccTagXYZ *CIccXformMatrixTRC::GetColumn(icSignature sig) const
{
  CIccTag *pTag = m_pProfile->FindTag(sig);

  if (!pTag || pTag->GetType()!=icSigXYZType) {
    return NULL;
  }

  return (CIccTagXYZ*)pTag;
}

/**
 **************************************************************************
 * Name: CIccXformMatrixTRC::GetInvCurve
 * 
 * Purpose: 
 *  Gets the inverted curve having the passed signature, from the profile.
 *  
 * Args:
 *  sig = signature of the curve to be inverted
 *
 * Return:
 *  Pointer to the inverted curve.
 **************************************************************************
 */
CIccCurve *CIccXformMatrixTRC::GetInvCurve(icSignature sig) const
{
  CIccCurve *pCurve;
  CIccTagCurve *pInvCurve;

  if (!(pCurve = GetCurve(sig)))
    return NULL;

  pCurve->Begin();

  pInvCurve = new (std::nothrow) CIccTagCurve(2048);
  if (!pInvCurve)
    return NULL;

  int i;
  icFloatNumber x;
  icFloatNumber *Lut = &(*pInvCurve)[0];

  for (i=0; i<2048; i++) {
    x=(icFloatNumber)i / 2047;

    Lut[i] = pCurve->Find(x);
  }

  return pInvCurve;
}

/**
**************************************************************************
* Name: CIccXformMatrixTRC::ExtractInputCurves
* 
* Purpose: 
*  Gets the input curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the input curves.
**************************************************************************
*/
LPIccCurve* CIccXformMatrixTRC::ExtractInputCurves()
{
  if (m_bInput) {
    if (m_Curve[0]) {
      LPIccCurve* Curve = new (std::nothrow) LPIccCurve[3];
      if (Curve) {
        Curve[0] = (LPIccCurve)(m_Curve[0]->NewCopy());
        Curve[1] = (LPIccCurve)(m_Curve[1]->NewCopy());
        Curve[2] = (LPIccCurve)(m_Curve[2]->NewCopy());
        m_ApplyCurvePtr = NULL;
      }
      return Curve;
    }
  }

  return NULL;
}

/**
**************************************************************************
* Name: CIccXformMatrixTRC::ExtractOutputCurves
* 
* Purpose: 
*  Gets the output curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the output curves.
**************************************************************************
*/
LPIccCurve* CIccXformMatrixTRC::ExtractOutputCurves()
{
  if (!m_bInput) {
    if (m_Curve[0]) {
      LPIccCurve* Curve = new (std::nothrow) LPIccCurve[3];
      if (Curve) {
        Curve[0] = (LPIccCurve)(m_Curve[0]->NewCopy());
        Curve[1] = (LPIccCurve)(m_Curve[1]->NewCopy());
        Curve[2] = (LPIccCurve)(m_Curve[2]->NewCopy());
        m_ApplyCurvePtr = NULL;
      }
      return Curve;
    }
  }

  return NULL;
}

/**
 **************************************************************************
 * Name: CIccXform3DLut::CIccXform3DLut
 * 
 * Purpose: 
 *  Constructor
 *
 * Args:
 *   pTag = Pointer to the tag of type CIccMBB 
 **************************************************************************
 */
CIccXform3DLut::CIccXform3DLut(CIccTag *pTag)
{
  if (pTag && pTag->IsMBBType()) {
    m_pTag = (CIccMBB*)pTag;
  }
  else
    m_pTag = NULL;

  m_ApplyCurvePtrA = m_ApplyCurvePtrB = m_ApplyCurvePtrM = NULL;
  m_ApplyMatrixPtr = NULL;
}

/**
 **************************************************************************
 * Name: CIccXform3DLut::~CIccXform3DLut
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccXform3DLut::~CIccXform3DLut()
{
}

/**
 **************************************************************************
 * Name: CIccXform3DLut::Begin
 * 
 * Purpose: 
 *  Does the initialization of the Xform before Apply() is called.
 *  Must be called before Apply().
 *
 **************************************************************************
 */
 icStatusCMM CIccXform3DLut::Begin()
{
  icStatusCMM status;
  CIccCurve **Curve;
  int i;

  status = CIccXform::Begin();
  if (status != icCmmStatOk)
    return status;

  if (!m_pTag ||
      m_pTag->InputChannels()!=3) {
    return icCmmStatInvalidLut;
  }
  
  // catch cases where the LUT does not match the colorspace given
  // this avoids segfaults and stack overflows when applying the LUT
  icUInt16Number csOutChannels = icGetSpaceSamples( m_pTag->GetCsOutput() );
  if (csOutChannels != m_pTag->OutputChannels())
    return icCmmStatInvalidLut;
  

  m_ApplyCurvePtrA = NULL;
  m_ApplyCurvePtrB = NULL;
  m_ApplyCurvePtrM = NULL;

  if (m_pTag->m_bInputMatrix) {
    if (m_pTag->m_CurvesB) {
      Curve = m_pTag->m_CurvesB;

      Curve[0]->Begin();
      Curve[1]->Begin();
      Curve[2]->Begin();

      if (!Curve[0]->IsIdentity() || !Curve[1]->IsIdentity() || !Curve[2]->IsIdentity()) {
        m_ApplyCurvePtrB = Curve;
      }
    }

    if (m_pTag->m_CurvesM) {
      Curve = m_pTag->m_CurvesM;

      Curve[0]->Begin();
      Curve[1]->Begin();
      Curve[2]->Begin();
      
      if (!Curve[0]->IsIdentity() || !Curve[1]->IsIdentity() || !Curve[2]->IsIdentity()) {
        m_ApplyCurvePtrM = Curve;
      }
    }

    if (m_pTag->m_CLUT) {
      // Begin() refuses a CLUT that Init() never made usable -- reachable when
      // the tag was built by a parser rather than read from a profile.
      if (!m_pTag->m_CLUT->Begin())
        return icCmmStatInvalidLut;
    }

    if (m_pTag->m_CurvesA) {
      Curve = m_pTag->m_CurvesA;

      for (i=0; i<m_pTag->m_nOutput; i++) {
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrA = Curve;
          break;
        }
      }
    }

  }
  else {
    if (m_pTag->m_CurvesA) {
      Curve = m_pTag->m_CurvesA;

      Curve[0]->Begin();
      Curve[1]->Begin();
      Curve[2]->Begin();

      if (!Curve[0]->IsIdentity() || !Curve[1]->IsIdentity() || !Curve[2]->IsIdentity()) {
        m_ApplyCurvePtrA = Curve;
      }
    }

    if (m_pTag->m_CLUT) {
      // Begin() refuses a CLUT that Init() never made usable -- reachable when
      // the tag was built by a parser rather than read from a profile.
      if (!m_pTag->m_CLUT->Begin())
        return icCmmStatInvalidLut;
    }

    if (m_pTag->m_CurvesM) {
      Curve = m_pTag->m_CurvesM;

      for (i=0; i<m_pTag->m_nOutput; i++) {
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrM = Curve;
          break;
        }
      }
    }

    if (m_pTag->m_CurvesB) {
      Curve = m_pTag->m_CurvesB;

      for (i=0; i<m_pTag->m_nOutput; i++) {
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrB = Curve;
          break;
        }
      }
    }
  }

  m_ApplyMatrixPtr = NULL;
  if (m_pTag->m_Matrix) {
    if (m_pTag->m_bInputMatrix) {
      if (m_pTag->m_nInput!=3) {
        return icCmmStatInvalidProfile;
      }
    }
    else {
      if (m_pTag->m_nOutput!=3) {
        return icCmmStatInvalidProfile;
      }
    }

    if (!m_pTag->m_Matrix->IsIdentity()) {
      m_ApplyMatrixPtr = m_pTag->m_Matrix;
    }
  }

  // Apply() zero-fills its scratch pixel above channel 3 only when nothing
  // downstream will write those channels. Interp3d and Interp3dTetra write all
  // m_nOutput channels on every path, so with a CLUT present the fill is dead.
  m_bNeedScratchInit = (m_pTag->m_CLUT == NULL);

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccXform3DLut::Apply
 * 
 * Purpose: 
 *  Does the actual application of the Xform.
 *  
 * Args:
 *  pApply = ApplyXform object containing temporary storage used during Apply
 *  DstPixel = Destination pixel where the result is stored,
 *  SrcPixel = Source pixel which is to be applied.
 **************************************************************************
 */
void CIccXform3DLut::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  icFloatNumber Pixel[16];
  int i;

  if (m_bSrcPcsConversion)
    SrcPixel = CheckSrcAbs(pApply, SrcPixel);

  Pixel[0] = SrcPixel[0];
  Pixel[1] = SrcPixel[1];
  Pixel[2] = SrcPixel[2];

  // Only when nothing downstream will write these channels. With a CLUT present
  // Interp3d/Interp3dTetra write all m_nOutput channels on every path, so this
  // fill is dead -- and it ran unconditionally, on every pixel. Decided in
  // Begin(); the original comment was "just in case", which is the case.
  if (m_bNeedScratchInit) {
    for (i = 3; i < m_pTag->m_nOutput; ++i) {
      Pixel[i] = 0.0;
    }
  }

  if (m_pTag->m_bInputMatrix) {
    if (m_ApplyCurvePtrB) {
      Pixel[0] = m_ApplyCurvePtrB[0]->Apply(Pixel[0]);
      Pixel[1] = m_ApplyCurvePtrB[1]->Apply(Pixel[1]);
      Pixel[2] = m_ApplyCurvePtrB[2]->Apply(Pixel[2]);
    }

    if (m_ApplyMatrixPtr) {
      m_ApplyMatrixPtr->Apply(Pixel);
    }

    if (m_ApplyCurvePtrM) {
      Pixel[0] = m_ApplyCurvePtrM[0]->Apply(Pixel[0]);
      Pixel[1] = m_ApplyCurvePtrM[1]->Apply(Pixel[1]);
      Pixel[2] = m_ApplyCurvePtrM[2]->Apply(Pixel[2]);
    }

    if (m_pTag->m_CLUT) {
      if (m_nInterp==icInterpLinear)
        m_pTag->m_CLUT->Interp3d(Pixel, Pixel);
      else
        m_pTag->m_CLUT->Interp3dTetra(Pixel, Pixel);
    }

    if (m_ApplyCurvePtrA) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrA[i]->Apply(Pixel[i]);
      }
    }

  }
  else {
    if (m_ApplyCurvePtrA) {
      Pixel[0] = m_ApplyCurvePtrA[0]->Apply(Pixel[0]);
      Pixel[1] = m_ApplyCurvePtrA[1]->Apply(Pixel[1]);
      Pixel[2] = m_ApplyCurvePtrA[2]->Apply(Pixel[2]);
    }

    if (m_pTag->m_CLUT) {
      if (m_nInterp==icInterpLinear)
        m_pTag->m_CLUT->Interp3d(Pixel, Pixel);
      else
        m_pTag->m_CLUT->Interp3dTetra(Pixel, Pixel);
    }

    if (m_ApplyCurvePtrM) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrM[i]->Apply(Pixel[i]);
      }
    }

    if (m_ApplyMatrixPtr) {
      m_ApplyMatrixPtr->Apply(Pixel);
    }

    if (m_ApplyCurvePtrB) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrB[i]->Apply(Pixel[i]);
      }
    }
  }

  for (i=0; i<m_pTag->m_nOutput; i++) {
    DstPixel[i] = Pixel[i];
  }

  if (m_bDstPcsConversion)
    CheckDstAbs(DstPixel);
}

/**
**************************************************************************
* Name: CIccXform3DLut::ExtractInputCurves
* 
* Purpose: 
*  Gets the input curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the input curves.
**************************************************************************
*/
LPIccCurve* CIccXform3DLut::ExtractInputCurves()
{
  if (m_bInput) {
    if (m_pTag->m_bInputMatrix) {
      if (m_pTag->m_CurvesB) {
        LPIccCurve* Curve = new (std::nothrow) LPIccCurve[3];
        if (Curve) {
          Curve[0] = (LPIccCurve)(m_pTag->m_CurvesB[0]->NewCopy());
          Curve[1] = (LPIccCurve)(m_pTag->m_CurvesB[1]->NewCopy());
          Curve[2] = (LPIccCurve)(m_pTag->m_CurvesB[2]->NewCopy());
          m_ApplyCurvePtrB = NULL;
        }
        return Curve;
      }
    }
    else {
      if (m_pTag->m_CurvesA) {
        LPIccCurve* Curve = new (std::nothrow) LPIccCurve[3];
        if (Curve) {
          Curve[0] = (LPIccCurve)(m_pTag->m_CurvesA[0]->NewCopy());
          Curve[1] = (LPIccCurve)(m_pTag->m_CurvesA[1]->NewCopy());
          Curve[2] = (LPIccCurve)(m_pTag->m_CurvesA[2]->NewCopy());
          m_ApplyCurvePtrA = NULL;
        }
        return Curve;
      }
    }
  }

  return NULL;
}

/**
**************************************************************************
* Name: CIccXform3DLut::ExtractOutputCurves
* 
* Purpose: 
*  Gets the output curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the output curves.
**************************************************************************
*/
LPIccCurve* CIccXform3DLut::ExtractOutputCurves()
{
  if (!m_bInput) {
    if (m_pTag->m_bInputMatrix) {
      if (m_pTag->m_CurvesA) {
        LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nOutput];
        if (Curve) {
          for (int i=0; i<m_pTag->m_nOutput; i++) {
            Curve[i] = (LPIccCurve)(m_pTag->m_CurvesA[i]->NewCopy());
          }
          m_ApplyCurvePtrA = NULL;
        }
        return Curve;
      }
    }
    else {
      if (m_pTag->m_CurvesB) {
        LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nOutput];
        if (Curve) {
          for (int i=0; i<m_pTag->m_nOutput; i++) {
            Curve[i] = (LPIccCurve)(m_pTag->m_CurvesB[i]->NewCopy());
          }
          m_ApplyCurvePtrB = NULL;
        }
        return Curve;
      }
    }
  }

  return NULL;
}

/**
 **************************************************************************
 * Name: CIccXform4DLut::CIccXform4DLut
 * 
 * Purpose: 
 *  Constructor
 *
 * Args:
 *   pTag = Pointer to the tag of type CIccMBB 
 **************************************************************************
 */
CIccXform4DLut::CIccXform4DLut(CIccTag *pTag)
{
  if (pTag && pTag->IsMBBType()) {
    m_pTag = (CIccMBB*)pTag;
  }
  else
    m_pTag = NULL;

  m_ApplyCurvePtrA = m_ApplyCurvePtrB = m_ApplyCurvePtrM = NULL;
  m_ApplyMatrixPtr = NULL;
}


/**
 **************************************************************************
 * Name: CIccXform4DLut::~CIccXform4DLut
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccXform4DLut::~CIccXform4DLut()
{
}


/**
 **************************************************************************
 * Name: CIccXform4DLut::Begin
 * 
 * Purpose: 
 *  Does the initialization of the Xform before Apply() is called.
 *  Must be called before Apply().
 *
 **************************************************************************
 */
icStatusCMM CIccXform4DLut::Begin()
{
  icStatusCMM status;
  CIccCurve **Curve;
  int i;

  status = CIccXform::Begin();
  if (status != icCmmStatOk) {
    return status;
  }

  if (!m_pTag ||
      m_pTag->InputChannels()!=4) {
    return icCmmStatInvalidLut;
  }
  
  // catch cases where the LUT does not match the colorspace given
  // this avoids segfaults and stack overflows when applying the LUT
  icUInt16Number csOutChannels = icGetSpaceSamples( m_pTag->GetCsOutput() );
  if (csOutChannels != m_pTag->OutputChannels())
    return icCmmStatInvalidLut;

  m_ApplyCurvePtrA = m_ApplyCurvePtrB = m_ApplyCurvePtrM = NULL;

  if (m_pTag->m_bInputMatrix) {
    if (m_pTag->m_CurvesB) {
      Curve = m_pTag->m_CurvesB;

      Curve[0]->Begin();
      Curve[1]->Begin();
      Curve[2]->Begin();
      Curve[3]->Begin();

      if (!Curve[0]->IsIdentity() || !Curve[1]->IsIdentity() ||
          !Curve[2]->IsIdentity() || !Curve[3]->IsIdentity()) 
      {
        m_ApplyCurvePtrB = Curve;
      }
    }

    if (m_pTag->m_CLUT) {
      // Begin() refuses a CLUT that Init() never made usable -- reachable when
      // the tag was built by a parser rather than read from a profile.
      if (!m_pTag->m_CLUT->Begin())
        return icCmmStatInvalidLut;
    }

    if (m_pTag->m_CurvesA) {
      Curve = m_pTag->m_CurvesA;

      for (i=0; i<m_pTag->m_nOutput; i++) {
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrA = Curve;
          break;
        }
      }
    }

  }
  else {
    if (m_pTag->m_CurvesA) {
      Curve = m_pTag->m_CurvesA;

      Curve[0]->Begin();
      Curve[1]->Begin();
      Curve[2]->Begin();
      Curve[3]->Begin();

      if (!Curve[0]->IsIdentity() || !Curve[1]->IsIdentity() ||
          !Curve[2]->IsIdentity() || !Curve[3]->IsIdentity()) 
      {
        m_ApplyCurvePtrA = Curve;
      }
    }

    if (m_pTag->m_CLUT) {
      // Begin() refuses a CLUT that Init() never made usable -- reachable when
      // the tag was built by a parser rather than read from a profile.
      if (!m_pTag->m_CLUT->Begin())
        return icCmmStatInvalidLut;
    }

    if (m_pTag->m_CurvesM) {
      Curve = m_pTag->m_CurvesM;

      for (i=0; i<m_pTag->m_nOutput; i++) {
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrM = Curve;
          break;
        }
      }
    }

    if (m_pTag->m_CurvesB) {
      Curve = m_pTag->m_CurvesB;

      for (i=0; i<m_pTag->m_nOutput; i++) {
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrB = Curve;
          break;
        }
      }
    }
  }

  m_ApplyMatrixPtr = NULL;
  if (m_pTag->m_Matrix) {
    if (m_pTag->m_bInputMatrix) {
      return icCmmStatInvalidProfile;
    }
    else {
      if (m_pTag->m_nOutput!=3) {
        return icCmmStatInvalidProfile;
      }
    }

    if (!m_pTag->m_Matrix->IsIdentity()) {
      m_ApplyMatrixPtr = m_pTag->m_Matrix;
    }
  }

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccXform4DLut::Apply
 * 
 * Purpose: 
 *  Does the actual application of the Xform.
 *  
 * Args:
 *  pApply = ApplyXform object containging temporary storage used during Apply
 *  DstPixel = Destination pixel where the result is stored,
 *  SrcPixel = Source pixel which is to be applied.
 **************************************************************************
 */
void CIccXform4DLut::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  icFloatNumber Pixel[16];
  int i;

  if (m_bSrcPcsConversion)
    SrcPixel = CheckSrcAbs(pApply, SrcPixel);

  Pixel[0] = SrcPixel[0];
  Pixel[1] = SrcPixel[1];
  Pixel[2] = SrcPixel[2];
  Pixel[3] = SrcPixel[3];

  if (m_pTag->m_bInputMatrix) {
    if (m_ApplyCurvePtrB) {
      Pixel[0] = m_ApplyCurvePtrB[0]->Apply(Pixel[0]);
      Pixel[1] = m_ApplyCurvePtrB[1]->Apply(Pixel[1]);
      Pixel[2] = m_ApplyCurvePtrB[2]->Apply(Pixel[2]);
      Pixel[3] = m_ApplyCurvePtrB[3]->Apply(Pixel[3]);
    }

    if (m_pTag->m_CLUT) {
      m_pTag->m_CLUT->Interp4d(Pixel, Pixel);
    }

    if (m_ApplyCurvePtrA) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrA[i]->Apply(Pixel[i]);
      }
    }

  }
  else {
    if (m_ApplyCurvePtrA) {
      Pixel[0] = m_ApplyCurvePtrA[0]->Apply(Pixel[0]);
      Pixel[1] = m_ApplyCurvePtrA[1]->Apply(Pixel[1]);
      Pixel[2] = m_ApplyCurvePtrA[2]->Apply(Pixel[2]);
      Pixel[3] = m_ApplyCurvePtrA[3]->Apply(Pixel[3]);
    }

    if (m_pTag->m_CLUT) {
      m_pTag->m_CLUT->Interp4d(Pixel, Pixel);
    }

    if (m_ApplyCurvePtrM) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrM[i]->Apply(Pixel[i]);
      }
    }

    if (m_ApplyMatrixPtr) {
      m_ApplyMatrixPtr->Apply(Pixel);
    }

    if (m_ApplyCurvePtrB) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrB[i]->Apply(Pixel[i]);
      }
    }
  }

  for (i=0; i<m_pTag->m_nOutput; i++) {
    DstPixel[i] = Pixel[i];
  }

  if (m_bDstPcsConversion)
    CheckDstAbs(DstPixel);
}

/**
**************************************************************************
* Name: CIccXform4DLut::ExtractInputCurves
* 
* Purpose: 
*  Gets the input curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the input curves.
**************************************************************************
*/
LPIccCurve* CIccXform4DLut::ExtractInputCurves()
{
  if (m_bInput) {
    if (m_pTag->m_bInputMatrix) {
      if (m_pTag->m_CurvesB) {
        LPIccCurve* Curve = new (std::nothrow) LPIccCurve[4];
        if (Curve) {
          Curve[0] = (LPIccCurve)(m_pTag->m_CurvesB[0]->NewCopy());
          Curve[1] = (LPIccCurve)(m_pTag->m_CurvesB[1]->NewCopy());
          Curve[2] = (LPIccCurve)(m_pTag->m_CurvesB[2]->NewCopy());
          Curve[3] = (LPIccCurve)(m_pTag->m_CurvesB[3]->NewCopy());
          m_ApplyCurvePtrB = NULL;
        }
      return Curve;
      }
    }
    else {
      if (m_pTag->m_CurvesA) {
        LPIccCurve* Curve = new (std::nothrow) LPIccCurve[4];
        if (Curve) {
          Curve[0] = (LPIccCurve)(m_pTag->m_CurvesA[0]->NewCopy());
          Curve[1] = (LPIccCurve)(m_pTag->m_CurvesA[1]->NewCopy());
          Curve[2] = (LPIccCurve)(m_pTag->m_CurvesA[2]->NewCopy());
          Curve[3] = (LPIccCurve)(m_pTag->m_CurvesA[3]->NewCopy());
          m_ApplyCurvePtrA = NULL;
        }
        return Curve;
      }
    }
  }

  return NULL;
}

/**
**************************************************************************
* Name: CIccXform4DLut::ExtractOutputCurves
* 
* Purpose: 
*  Gets the output curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the output curves.
**************************************************************************
*/
LPIccCurve* CIccXform4DLut::ExtractOutputCurves()
{
	if (!m_bInput) {
		if (m_pTag->m_bInputMatrix) {
			if (m_pTag->m_CurvesA) {
				LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nOutput];
                if (Curve) {
                    for (int i=0; i<m_pTag->m_nOutput; i++) {
                        Curve[i] = (LPIccCurve)(m_pTag->m_CurvesA[i]->NewCopy());
                    }
                    m_ApplyCurvePtrA = NULL;
                }
				return Curve;
			}
		}
		else {
			if (m_pTag->m_CurvesB) {
				LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nOutput];
                if (Curve) {
                    for (int i=0; i<m_pTag->m_nOutput; i++) {
                        Curve[i] = (LPIccCurve)(m_pTag->m_CurvesB[i]->NewCopy());
                    }
                    m_ApplyCurvePtrB = NULL;
                }
				return Curve;
			}
		}
	}

  return NULL;
}


/**
 **************************************************************************
 * Name: CIccXformNDLut::CIccXformNDLut
 * 
 * Purpose: 
 *  Constructor
 *
 * Args:
 *   pTag = Pointer to the tag of type CIccMBB 
 **************************************************************************
 */
CIccXformNDLut::CIccXformNDLut(CIccTag *pTag)
{
  if (pTag && pTag->IsMBBType()) {
    m_pTag = (CIccMBB*)pTag;
  }
  else
    m_pTag = NULL;

  m_ApplyCurvePtrA = m_ApplyCurvePtrB = m_ApplyCurvePtrM = NULL;
  m_ApplyMatrixPtr = NULL;
  m_nNumInput = 0;
}


/**
 **************************************************************************
 * Name: CIccXformNDLut::~CIccXformNDLut
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccXformNDLut::~CIccXformNDLut()
{
}


/**
 **************************************************************************
 * Name: CIccXformNDLut::Begin
 * 
 * Purpose: 
 *  Does the initialization of the Xform before Apply() is called.
 *  Must be called before Apply().
 *
 **************************************************************************
 */
icStatusCMM CIccXformNDLut::Begin()
{
  icStatusCMM status;
  CIccCurve **Curve;
  int i;

  status = CIccXform::Begin();
  if (status != icCmmStatOk) {
    return status;
  }

  if (!m_pTag || (m_pTag->InputChannels()>2 && m_pTag->InputChannels()<5)) {
    return icCmmStatInvalidLut;
  }
  
  // catch cases where the LUT does not match the colorspace given
  // this avoids segfaults and stack overflows when applying the LUT
  icUInt16Number csOutChannels = icGetSpaceSamples( m_pTag->GetCsOutput() );
  if (csOutChannels != m_pTag->OutputChannels())
    return icCmmStatInvalidLut;

  // CWE-125: the input half of the same check. Apply() copies InputChannels()
  // floats out of SrcPixel, but that buffer is sized from the profile, not from
  // the tag: CIccApplyCmm::InitPixel() takes the max of GetNumSrcSamples() and
  // GetNumDstSamples() across the chain (floored at 16), and the first xform in a
  // chain is handed the application's own buffer. A tag declaring more input
  // channels than the profile's source space therefore reads past the end of it.
  //
  // Compare against GetNumSrcSamples() rather than against the tag's own
  // GetCsInput(): that is precisely the quantity InitPixel() sizes the buffer
  // from, so it stays correct where the two disagree - and they do disagree,
  // because the tag is not always the profile's own. NDLut is reached from three
  // sites: the no-tag CIccXform::Create() overload (the default: arm of its switch
  // on m_Header.colorSpace), the tag-explicit CIccXform::Create() overload behind
  // the public CIccCmm::AddXform(CIccProfile*, CIccTag*, ...), and
  // CIccXformMpe::Create(). At the tag-explicit one the caller chooses the tag,
  // and its m_csInput records whatever stamped it, so a BToAn tag handed in as an
  // input xform matches its own InputChannels() while SrcPixel is still sized from
  // m_Header.colorSpace. Checking the tag against itself would pass that case and
  // leave the over-read intact.
  //
  // Only the ND path needs this stated at all: CIccXform3DLut::Begin() and
  // CIccXform4DLut::Begin() pin InputChannels() to the 3 and 4 their Apply()
  // reads, while this one takes a variable count from the tag. (The guard above
  // already refuses 3 and 4 here, so a 3-channel space that routes to ND for want
  // of being enumerated in those switches, icSigDevLabData say, never reaches
  // this check.)
  //
  // That asymmetry has a history worth recording, because it looks accidental and
  // is not: 48a53197 ("Fix: SBO in CIccXform3DLut::Apply()", #655) added the
  // output check above to all three LUT xforms in a single patch. In 3DLut and
  // 4DLut the guard immediately above already pinned the input side, so the
  // output half was all those two needed; NDLut's guard only rejects 3 and 4, so
  // the same patch left it with a checked output and an unchecked input.
  //
  // This is not a new rule: CIccMBB::Validate() already reports the same
  // disagreement as "Incorrect number of input channels", a critical error, by
  // comparing m_nInput against icGetSpaceSamples(pProfile->m_Header.colorSpace)
  // for exactly these tags. The apply path simply never consulted it, so a
  // profile the validator rejects could still be driven through Apply().
  //
  // Nor does CIccCmm::Begin()'s own guard cover it, though its comment ("Make
  // sure the input channel and first transform input counts match. Otherwise
  // we'll have a heap overflow during Apply.") describes this hazard exactly.
  // That guard compares GetSourceSamples() against the first xform's
  // GetNumSrcSamples(), and both of those derive from m_Header.colorSpace, so it
  // is header-to-header and structurally cannot see a tag that disagrees with
  // the header. The tag's own count is only visible here. (#2119)
  icUInt16Number nSrcSamples = GetNumSrcSamples();
  if (nSrcSamples != m_pTag->InputChannels())
    return icCmmStatInvalidLut;

  m_nNumInput = m_pTag->m_nInput;

  // CWE-400/CWE-834: m_nNumInput is a copy of the tag's icUInt8Number input channel
  // count (m_pTag->m_nInput), so it is bounded to 0..255, and the m_Curves* arrays
  // walked below are allocated to match it (CIccMBB::NewCurvesA/B/M size by
  // m_nInput/m_nOutput). Assert that bound explicitly and reject an out-of-range
  // count as an invalid LUT so the per-channel curve walks can never be driven past
  // those arrays even if the count reached this object corrupted.
  // Bound is 16, not 256. Apply() copies the source into a fixed icFloatNumber
  // Pixel[16], so a tag declaring more input channels than that cannot be applied
  // correctly no matter what: the previous 256 bound let such a tag through and
  // Apply() then clamped the count to 16 per pixel, silently truncating the
  // transform rather than reporting it. Refusing it here removes both per-pixel
  // clamps and turns quiet wrong colour into icCmmStatInvalidLut.
  //
  // 16 is also the ceiling the format allows: icMaxChannels, which CIccCLUT::Init
  // enforces on load (see the CWE-674 note at IccTagLut.cpp:2182).
  const int kMaxLutInputChannels = 16;
  if (m_nNumInput < 0 || m_nNumInput > kMaxLutInputChannels)
    return icCmmStatInvalidLut;

  if (m_pTag->m_nOutput > kMaxLutInputChannels)
    return icCmmStatInvalidLut;

  m_ApplyCurvePtrA = m_ApplyCurvePtrB = m_ApplyCurvePtrM = NULL;

  if (m_pTag->m_bInputMatrix) {
    if (m_pTag->m_CurvesB) {
      Curve = m_pTag->m_CurvesB;

      // CWE-476: a Read()-accepted tag can still carry NULL curve slots when the
      // profile declared more channels than it supplied curves - CIccMBB::Validate
      // reports exactly this as "Incorrect number of B-curves". The apply path skips
      // Validate, so reject a missing curve here as an invalid LUT instead of
      // dereferencing NULL. After this loop every Curve[i] (i<count) is non-NULL, so
      // the IsIdentity walk below needs no further guard.
      for (i=0; i<m_nNumInput; i++) {
        if (!Curve[i])
          return icCmmStatInvalidLut;
        Curve[i]->Begin();
      }

      for (i=0; i<m_nNumInput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrB = Curve;
          break;
        }
      }
    }

    if (m_pTag->m_CLUT) {
      // Begin() refuses a CLUT that Init() never made usable -- reachable when
      // the tag was built by a parser rather than read from a profile.
      if (!m_pTag->m_CLUT->Begin())
        return icCmmStatInvalidLut;
    }

    if (m_pTag->m_CurvesA) {
      Curve = m_pTag->m_CurvesA;

      // CWE-476: reject NULL curve slots (see the m_CurvesB note above) so this
      // output-side walk can't dereference a missing A-curve on a malformed tag.
      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i])
          return icCmmStatInvalidLut;
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrA = Curve;
          break;
        }
      }
    }

  }
  else {
    if (m_pTag->m_CurvesA) {
      Curve = m_pTag->m_CurvesA;

      // CWE-476: reject NULL curve slots (see the m_CurvesB note above) so this
      // input-side walk can't dereference a missing A-curve on a malformed tag.
      for (i=0; i<m_nNumInput; i++) {
        if (!Curve[i])
          return icCmmStatInvalidLut;
        Curve[i]->Begin();
      }

      for (i=0; i<m_nNumInput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrA = Curve;
          break;
        }
      }
    }

    if (m_pTag->m_CLUT) {
      // Begin() refuses a CLUT that Init() never made usable -- reachable when
      // the tag was built by a parser rather than read from a profile.
      if (!m_pTag->m_CLUT->Begin())
        return icCmmStatInvalidLut;
    }

    if (m_pTag->m_CurvesM) {
      Curve = m_pTag->m_CurvesM;

      // CWE-476: reject NULL curve slots (see the m_CurvesB note above) so this
      // output-side walk can't dereference a missing M-curve on a malformed tag.
      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i])
          return icCmmStatInvalidLut;
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrM = Curve;
          break;
        }
      }
    }

    if (m_pTag->m_CurvesB) {
      Curve = m_pTag->m_CurvesB;

      // CWE-476: reject NULL curve slots (see the m_CurvesB note above) so this
      // output-side walk can't dereference a missing B-curve on a malformed tag.
      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i])
          return icCmmStatInvalidLut;
        Curve[i]->Begin();
      }

      for (i=0; i<m_pTag->m_nOutput; i++) {
        if (!Curve[i]->IsIdentity()) {
          m_ApplyCurvePtrB = Curve;
          break;
        }
      }
    }
  }

  m_ApplyMatrixPtr = NULL;
  if (m_pTag->m_Matrix) {
    if (m_pTag->m_bInputMatrix) {
      return icCmmStatInvalidProfile;
    }
    else {
      if (m_pTag->m_nOutput!=3) {
        return icCmmStatInvalidProfile;
      }
    }

    if (!m_pTag->m_Matrix->IsIdentity()) {
      m_ApplyMatrixPtr = m_pTag->m_Matrix;
    }
  }

  return icCmmStatOk;
}


/**
 **************************************************************************
 * Name: CIccXformNDLut::GetNewApply
 *
 * Purpose:
 *  Allocates a new apply object
 *
 * Args:
 *  status = reference to status of creation of the apply object
 **************************************************************************
 */
CIccApplyXform* CIccXformNDLut::GetNewApply(icStatusCMM& status)
{
  if (!m_pTag)
    return NULL;

  CIccCLUT* pCLUT = m_pTag->GetCLUT();
  CIccApplyCLUT* pApply = NULL;

  if (pCLUT) {      // was && m_nNumInput > 6, but this gets called for 1,2,5,6 as well, and withput pApply, it crashes
    pApply = pCLUT->GetNewApply();
    if (!pApply) {
      status = icCmmStatAllocErr;
      return NULL;
    }
  }

  CIccApplyNDLutXform* rv = new (std::nothrow) CIccApplyNDLutXform(this, pApply);
  
  if (!rv) {
    if (pApply)
      delete pApply;
    status = icCmmStatAllocErr;
    return NULL;
  }

  status = icCmmStatOk;
  return rv;
}

/**
 **************************************************************************
 * Name: CIccXformNDLut::Apply
 * 
 * Purpose: 
 *  Does the actual application of the Xform.
 *  
 * Args:
 *  pApply = ApplyXform object containging temporary storage used during Apply
 *  DstPixel = Destination pixel where the result is stored,
 *  SrcPixel = Source pixel which is to be applied.
 **************************************************************************
 */
void CIccXformNDLut::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  icFloatNumber Pixel[16] = {0};
  int i;

  if (m_bSrcPcsConversion)
    SrcPixel = CheckSrcAbs(pApply, SrcPixel);

  // No clamp: Begin() refuses m_nNumInput above 16, which is what Pixel[] holds.
  const int nInput = m_nNumInput;

  for (i=0; i<nInput; i++)
    Pixel[i] = SrcPixel[i];

  if (m_pTag->m_bInputMatrix) {
    if (m_ApplyCurvePtrB) {
      for (i=0; i<nInput; i++)
        Pixel[i] = m_ApplyCurvePtrB[i]->Apply(Pixel[i]);
    }

    if (m_pTag->m_CLUT) {
      switch(nInput) {
      case 5:
        m_pTag->m_CLUT->Interp5d(Pixel, Pixel);
        break;
      case 6:
        m_pTag->m_CLUT->Interp6d(Pixel, Pixel);
        break;
      default:
        {
          CIccApplyNDLutXform* pNDApply = (CIccApplyNDLutXform*)pApply;
          m_pTag->m_CLUT->InterpND(Pixel, Pixel, pNDApply->m_pApply);
          break;
        }
      }
    }

    if (m_ApplyCurvePtrA) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrA[i]->Apply(Pixel[i]);
      }
    }

  }
  else {
    if (m_ApplyCurvePtrA) {
      for (i=0; i<nInput; i++)
        Pixel[i] = m_ApplyCurvePtrA[i]->Apply(Pixel[i]);
    }

    if (m_pTag->m_CLUT) {
      switch(m_nNumInput) {
      case 5:
        m_pTag->m_CLUT->Interp5d(Pixel, Pixel);
        break;
      case 6:
        m_pTag->m_CLUT->Interp6d(Pixel, Pixel);
        break;
      default:
      {
        CIccApplyNDLutXform* pNDApply = (CIccApplyNDLutXform*)pApply;
        m_pTag->m_CLUT->InterpND(Pixel, Pixel, pNDApply->m_pApply);
        break;
      }
      break;
      }
    }

    if (m_ApplyCurvePtrM) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrM[i]->Apply(Pixel[i]);
      }
    }

    if (m_ApplyMatrixPtr) {
      m_ApplyMatrixPtr->Apply(Pixel);
    }

    if (m_ApplyCurvePtrB) {
      for (i=0; i<m_pTag->m_nOutput; i++) {
        Pixel[i] = m_ApplyCurvePtrB[i]->Apply(Pixel[i]);
      }
    }
  }

  // No clamp: Begin() refuses m_pTag->m_nOutput above 16 for the same reason.
  const int nOutput = m_pTag->m_nOutput;
  for (i=0; i<nOutput; i++) {
    DstPixel[i] = Pixel[i];
  }

  if (m_bDstPcsConversion)
    CheckDstAbs(DstPixel);
}

/**
**************************************************************************
* Name: CIccXformNDLut::ExtractInputCurves
* 
* Purpose: 
*  Gets the input curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the input curves.
**************************************************************************
*/
LPIccCurve* CIccXformNDLut::ExtractInputCurves()
{
	if (m_bInput) {
		if (m_pTag->m_bInputMatrix) {
			if (m_pTag->m_CurvesB) {
				LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nInput];
                if (Curve) {
                    for (int i=0; i<m_pTag->m_nInput; i++) {
                        Curve[i] = (LPIccCurve)(m_pTag->m_CurvesB[i]->NewCopy());
                    }
                    m_ApplyCurvePtrB = NULL;
                }
				return Curve;
			}
		}
		else {
			if (m_pTag->m_CurvesA) {
				LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nInput];
                if (Curve) {
                    for (int i=0; i<m_pTag->m_nInput; i++) {
                        Curve[i] = (LPIccCurve)(m_pTag->m_CurvesA[i]->NewCopy());
                    }
                    m_ApplyCurvePtrA = NULL;
                }
				return Curve;
			}
		}
	}

  return NULL;
}

/**
**************************************************************************
* Name: CIccXformNDLut::ExtractOutputCurves
* 
* Purpose: 
*  Gets the output curves. Should be called only after Begin() 
*  has been called. Once the curves are extracted, they will 
*  not be used by the Apply() function.
*  WARNING:  caller owns the curves and must be deleted by the caller.
*  
* Return:
*  Pointer to the output curves.
**************************************************************************
*/
LPIccCurve* CIccXformNDLut::ExtractOutputCurves()
{
	if (!m_bInput) {
		if (m_pTag->m_bInputMatrix) {
			if (m_pTag->m_CurvesA) {
				LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nOutput];
                if (Curve) {
                    for (int i=0; i<m_pTag->m_nOutput; i++) {
                        Curve[i] = (LPIccCurve)(m_pTag->m_CurvesA[i]->NewCopy());
                    }
                    m_ApplyCurvePtrA = NULL;
                }
				return Curve;
			}
		}
		else {
			if (m_pTag->m_CurvesB) {
				LPIccCurve* Curve = new (std::nothrow) LPIccCurve[m_pTag->m_nOutput];
                if (Curve) {
                    for (int i=0; i<m_pTag->m_nOutput; i++) {
                        Curve[i] = (LPIccCurve)(m_pTag->m_CurvesB[i]->NewCopy());
                    }
                    m_ApplyCurvePtrB = NULL;
                }
				return Curve;
			}
		}
	}

  return NULL;
}

/**
 **************************************************************************
 * Name: CIccXformNamedColor::CIccXformNamedColor
 * 
 * Purpose: 
 *  Constructor
 *
 * Args:
 *  pTag = Pointer to the tag of type CIccTagNamedColor2,
 *  csPCS = PCS color space,
 *  csDevice = Device color space 
 **************************************************************************
 */
CIccXformNamedColor::CIccXformNamedColor(CIccTag *pTag, icColorSpaceSignature csPcs, icColorSpaceSignature csDevice,
                                         icColorSpaceSignature csSpectralPcs/* =icSigNoSpectralData */,
                                         const icSpectralRange *pSpectralRange /* = NULL */,
                                         const icSpectralRange *pBiSpectralRange /* = NULL */,
                                         icNamedColorOverprintType nOverprintType /* = icNamedColorOverWhite */)
{
  m_nApplyInterface = icApplyPixel2Pixel; // was uninitialized
  m_pTag = NULL;
  m_pArray = NULL;
  m_nOverprintType = nOverprintType;
  if (pTag) {
    if (pTag->GetType()==icSigNamedColor2Type) {
      m_pTag = (CIccTagNamedColor2*)pTag;

      m_pTag->SetColorSpaces(csPcs, csDevice);
    }
    else if (pTag->GetTagArrayType()==icSigNamedColorArray) {
      CIccTagArray *pArray = (CIccTagArray*)pTag;
      CIccArrayNamedColor *pNamed = (CIccArrayNamedColor*)pArray->GetArrayHandler();

      if (pNamed) {
        m_pArray = pNamed;
        pNamed->SetColorSpaces(csPcs, csDevice, csSpectralPcs, pSpectralRange, pBiSpectralRange);
      }
    }
  }

  m_nSrcSpace = icSigUnknownData;
  m_nDestSpace = icSigUnknownData;
}


/**
 **************************************************************************
 * Name: CIccXformNamedColor::CIccXformNamedColor
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccXformNamedColor::~CIccXformNamedColor()
{
}

/**
 **************************************************************************
 * Name: CIccXformNamedColor::Begin
 * 
 * Purpose: 
 *  Does the initialization of the Xform before Apply() is called.
 *  Must be called before Apply().
 *
 **************************************************************************
 */
icStatusCMM CIccXformNamedColor::Begin()
{
  icStatusCMM status;

  status = CIccXform::Begin();
  if (status != icCmmStatOk)
    return status;

  if (m_pTag==NULL && m_pArray==NULL) {
    return icCmmStatProfileMissingTag;
  }

  if (m_nSrcSpace==icSigUnknownData ||
      m_nDestSpace==icSigUnknownData) {
    return icCmmStatIncorrectApply;
  }

  if (m_nSrcSpace != icSigNamedData) {
    if (m_nDestSpace != icSigNamedData) {
      m_nApplyInterface = icApplyPixel2Pixel;
    }
    else {
      m_nApplyInterface = icApplyPixel2Named;
    }
  }
  else {
    if (m_nDestSpace != icSigNamedData) {
      m_nApplyInterface = icApplyNamed2Pixel;
    }
    else {
      return icCmmStatIncorrectApply;
    }
  }

  if (m_pTag && !m_pTag->InitFindCachedPCSColor())
    return icCmmStatAllocErr;
  else if (m_pArray && !m_pArray->Begin())
    return icCmmStatAllocErr;

  return icCmmStatOk;
}



/**
 **************************************************************************
 * Name: CIccXformNamedColor::Apply
 * 
 * Purpose: 
 *  Does the actual application of the Xform.
 *  
 * Args:
 *  pApply = ApplyXform object containging temporary storage used during Apply
 *  DstColorName = Destination string where the color name result is stored,
 *  SrcPixel = Source pixel which is to be applied.
 **************************************************************************
 */
icStatusCMM CIccXformNamedColor::Apply(CIccApplyXform* pApply, icChar *DstColorName, const icFloatNumber *SrcPixel) const
{

  if (m_pArray) {
    const CIccArrayNamedColor *pArray = m_pArray;
    CIccStructNamedColor *pColor;

    std::string NamedColor;

    if (IsSrcPCS()) {
      if (IsSpaceSpectralPCS(m_nSrcSpace)) {
        pColor = pArray->FindSpectralColor(SrcPixel);
        if (pColor)
          NamedColor = pColor->getName();
        else 
          return icCmmStatColorNotFound;
      }
      else {
        if (m_bSrcPcsConversion)
          SrcPixel = CheckSrcAbs(pApply, SrcPixel);

        icFloatNumber pix[3];
        memcpy(pix, SrcPixel, 3*sizeof(icFloatNumber));

        if (m_nSrcSpace == icSigLabPcsData)
          icLabFromPcs(pix);
        else {
          icXyzFromPcs(pix);
          icXYZtoLab(pix, pix);
        }

        pColor = pArray->FindPcsColor(pix);
        if (pColor) 
          NamedColor = pColor->getName();
        else 
          return icCmmStatColorNotFound;
      }
    }
    else {
      pColor = pArray->FindDeviceColor(SrcPixel);
      if (pColor)
        NamedColor = pColor->getName();
      else 
        return icCmmStatColorNotFound;
    }

    snprintf(DstColorName, 256, "%s", NamedColor.c_str());
  }
  else if (m_pTag) {
    const CIccTagNamedColor2 *pTag = m_pTag;

    icFloatNumber DevicePix[16], PCSPix[3];
    std::string NamedColor;
    icUInt32Number i;
    icInt32Number j;

    if (IsSrcPCS()) {
      if (m_bSrcPcsConversion)
        SrcPixel = CheckSrcAbs(pApply, SrcPixel);

      for(i=0; i<3; i++)
        PCSPix[i] = SrcPixel[i];

      j = pTag->FindCachedPCSColor(PCSPix);
      if (j<0 || !pTag->GetColorName(NamedColor, j))
        return icCmmStatColorNotFound;
    }
    else {
      const icUInt32Number nDeviceCoords = pTag->GetDeviceCoords();
      if (nDeviceCoords > 16)
        return icCmmStatTooManySamples;

      for(i=0; i<nDeviceCoords; i++)
        DevicePix[i] = SrcPixel[i];

      j = pTag->FindDeviceColor(DevicePix);
      if (j<0 || !pTag->GetColorName(NamedColor, j))
        return icCmmStatColorNotFound;
    }

    snprintf(DstColorName, 256, "%s", NamedColor.c_str());
  }
  else
    return icCmmStatBadXform;

  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccXformNamedColor::Apply
* 
* Purpose: 
*  Does the actual application of the Xform.
*  
* Args:
*  pApply = ApplyXform object containing temporary storage used during Apply
*  DstPixel = Destination pixel where the result is stored,
*  SrcColorName = Source color name which is to be applied.
**************************************************************************
*/
icStatusCMM CIccXformNamedColor::Apply(CIccApplyXform*  /* pApply */, icFloatNumber *DstPixel, const icChar *SrcColorName, icFloatNumber tint) const
{

  if (m_pArray) {
    const CIccArrayNamedColor *pArray = m_pArray;

    CIccStructNamedColor *pColor;

    if (m_nSrcSpace != icSigNamedData)
      return icCmmStatBadSpaceLink;

    pColor = pArray->FindColor(SrcColorName);
    if (!pColor)
      return icCmmStatColorNotFound;

    if (IsDestPCS()) {
      if (IsSpaceSpectralPCS(m_nDestSpace)) {
        // Map the configured overprint to the corresponding NamedColor
        // array member.  Order matches icNamedColorOverprintType (0,1,2).
        static const icNamedColorlMemberSignature kOverprintSig[] = {
          icSigNmclSpectralDataMbr,        // OverWhite -> 'spec'
          icSigNmclSpectralOverBlackMbr,   // OverBlack -> 'spcb'
          icSigNmclSpectralOverGrayMbr,    // OverGray  -> 'spcg'
        };
        const int idx = (int)m_nOverprintType;
        const icNamedColorlMemberSignature sig =
            (idx >= 0 && idx < (int)(sizeof(kOverprintSig)/sizeof(kOverprintSig[0])))
              ? kOverprintSig[idx] : icSigNmclSpectralDataMbr;
        if (!pArray->GetSpectralTint(DstPixel, pColor, tint, sig))
          return icCmmStatBadTintXform;
      }
      else {
        if (!pArray->GetPcsTint(DstPixel, pColor, tint))
          return icCmmStatBadTintXform;

        if (m_nDestSpace == icSigLabData) {
          icLabToPcs(DstPixel);
        }
        else {
          icXyzToPcs(DstPixel);
        }
        if (m_bDstPcsConversion)
          CheckDstAbs(DstPixel);
      }
    }
    else {
      if (!pArray->GetDeviceTint(DstPixel, pColor, tint))
        return icCmmStatBadTintXform;
    }
  }
  else if (m_pTag) {
    const CIccTagNamedColor2 *pTag = m_pTag;

    icInt32Number j;

    if (m_nSrcSpace != icSigNamedData)
      return icCmmStatBadSpaceLink;

    if (IsDestPCS()) {

      j = pTag->FindColor(SrcColorName);
      if (j<0)
        return icCmmStatColorNotFound;

      if (m_nDestSpace == icSigLabData) {
        memcpy(DstPixel, pTag->GetEntry(j)->pcsCoords, 3*sizeof(icFloatNumber));
      }
      else {
        memcpy(DstPixel, pTag->GetEntry(j)->pcsCoords, 3*sizeof(icFloatNumber));
      }
      if (m_bDstPcsConversion)
        CheckDstAbs(DstPixel);
    }
    else {
      j = pTag->FindColor(SrcColorName);
      if (j<0)
        return icCmmStatColorNotFound;
      memcpy(DstPixel, pTag->GetEntry(j)->deviceCoords, pTag->GetDeviceCoords()*sizeof(icFloatNumber));
    }
  }

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccXformNamedColor::SetSrcSpace
 * 
 * Purpose: 
 *  Sets the source space of the Xform
 *  
 * Args:
 *  nSrcSpace = signature of the color space to be set
 **************************************************************************
 */
icStatusCMM CIccXformNamedColor::SetSrcSpace(icColorSpaceSignature nSrcSpace)
{
  if (m_pArray) {
    // Array-backed named-color xform: the source space is accepted as-is. The
    // PCS/device/named-data validation below applies only to the single-tag
    // (m_pTag) case.
  }
  else if (m_pTag) {
    CIccTagNamedColor2 *pTag = m_pTag;

    if (nSrcSpace!=pTag->GetPCS())
      if (nSrcSpace!=pTag->GetDeviceSpace())
        if (nSrcSpace!=icSigNamedData)
          return icCmmStatBadSpaceLink;
  }

  m_nSrcSpace = nSrcSpace;

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccXformNamedColor::SetDestSpace
 * 
 * Purpose: 
 *  Sets the destination space of the Xform
 *  
 * Args:
 *  nDestSpace = signature of the color space to be set
 **************************************************************************
 */
icStatusCMM CIccXformNamedColor::SetDestSpace(icColorSpaceSignature nDestSpace)
{
  if (m_nSrcSpace == nDestSpace)
    return icCmmStatBadSpaceLink;

  if (m_pArray) {
    // Array-backed named-color xform: the destination space is accepted as-is.
    // The PCS/device/named-data validation below applies only to the single-tag
    // (m_pTag) case.
  }
  else if (m_pTag) {
    CIccTagNamedColor2 *pTag = (CIccTagNamedColor2*)m_pTag;

    if (nDestSpace!=pTag->GetPCS())
      if (nDestSpace!=pTag->GetDeviceSpace())
        if (nDestSpace!=icSigNamedData)
          return icCmmStatBadSpaceLink;
  }

  m_nDestSpace = nDestSpace;

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccXformNamedColor::IsSrcPCS
 * 
 * Purpose: 
 *  Sets the source space is a PCS space
 **************************************************************************
 */
bool CIccXformNamedColor::IsSrcPCS() const
{
  if (m_pTag) {
    return m_nSrcSpace == m_pTag->GetPCS();
  }
  else if (m_pArray) {
    return IsSpacePCS(m_nSrcSpace);
  }
  else 
    return false;
}


/**
 **************************************************************************
 * Name: CIccXformNamedColor::IsDestPCS
 * 
 * Purpose: 
 *  Sets the destination space is a PCS space
 **************************************************************************
 */
bool CIccXformNamedColor::IsDestPCS() const
{
  if (m_pTag) {
    return m_nDestSpace == m_pTag->GetPCS();
  }
  else if (m_pArray) {
    return IsSpacePCS(m_nDestSpace);
  }
  else 
    return false;
}


/**
**************************************************************************
* Name: CIccXformMPE::CIccXformMPE
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccXformMpe::CIccXformMpe(CIccTag *pTag)
{
  if (pTag && pTag->GetType()==icSigMultiProcessElementType)
    m_pTag = (CIccTagMultiProcessElement*)pTag;
  else
    m_pTag = NULL;

  m_bUsingAcs = false;
  m_pAppliedPCC = NULL;
  m_bDeleteAppliedPCC = false;
}

/**
**************************************************************************
* Name: CIccXformMPE::~CIccXformMPE
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccXformMpe::~CIccXformMpe()
{
  if (m_bDeleteAppliedPCC)
    delete m_pAppliedPCC;
}

/**
**************************************************************************
* Name: CIccXformMPE::Create
* 
* Purpose:
*  This is a static Creation function that creates derived CIccXform objects and
*  initializes them.
* 
* Args: 
*  pProfile = pointer to a CIccProfile object that will be owned by the transform.  This object will
*   be destroyed when the returned CIccXform object is destroyed.  The means that the CIccProfile
*   object needs to be allocated on the heap.
*  bInput = flag to indicate whether to use the input or output side of the profile,
*  nIntent = the rendering intent to apply to the profile,   
*  nInterp = the interpolation algorithm to use for N-D luts.
*  nLutType = selection of which transform lut to use
*  pHintManager = hints for creating the xform
* 
* Return: 
*  A suitable pXform object
**************************************************************************
*/
CIccXform *CIccXformMpe::Create(CIccProfile *pProfile, bool bInput/* =true */, icRenderingIntent nIntent/* =icUnknownIntent */,
																icXformInterp nInterp/* =icInterpLinear */, icXformLutType nLutType/* =icXformLutColor */,
																CIccCreateXformHintManager *pHintManager/* =NULL */,
																bool bOwnsProfile/* =true */)
{
  // Same ownership contract as CIccXform::Create: on success pProfile is handed
  // to the new xform via SetParams(); on failure we free it ourselves when we
  // own it (bOwnsProfile==true).  This overload currently has no callers, but
  // the guards keep its behavior consistent with the rest of the Create family
  // so any future caller is leak-safe without risking a borrowed-profile
  // double-free.
  CIccXform *rv = NULL;
  icRenderingIntent nTagIntent = nIntent;
  bool bUseSpectralPCS = false;
  bool bAbsToRel = false;
  icXformLutType nUseLutType = nLutType;
  bool bUseColorimeticTags = true;
  bool bUseDToB = true;

  if (nLutType == icXformLutSpectral) {
    nUseLutType = icXformLutColor;
    bUseColorimeticTags = false;
  }
  else if (nLutType == icXformLutColorimetric) {
    nUseLutType = icXformLutColor;
    bUseDToB = false;
  }

  if (nTagIntent == icUnknownIntent)
    nTagIntent = icPerceptual;

  if (!IsValidXformIntent(nTagIntent)) {
    if (bOwnsProfile)
      delete pProfile;
    return NULL;
  }

  switch (nUseLutType) {
    case icXformLutColor:
      if (bInput) {
        CIccTag *pTag = NULL;
        if (bUseDToB) {
          pTag = pProfile->FindTag(icSigDToB0Tag + nTagIntent);

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigDToB1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
          else if (!pTag && nTagIntent != icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigDToB3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigDToB0Tag);
          }
        }

        //Unsupported elements cause fall back behavior
        if (pTag && !pTag->IsSupported())
          pTag = NULL;

        if (pTag && pProfile->m_Header.spectralPCS) {
          bUseSpectralPCS = true;
        }

        if (bUseColorimeticTags) {
          if (!pTag) {
            if (nTagIntent == icAbsoluteColorimetric)
              nTagIntent = icRelativeColorimetric;
            pTag = pProfile->FindTag(icSigAToB0Tag + nTagIntent);
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigAToB0Tag);
          }
        }

        if (!pTag) {
          if (bUseColorimeticTags && pProfile->m_Header.colorSpace == icSigRgbData && pProfile->m_Header.version < icVersionNumberV5) {
            rv = new (std::nothrow) CIccXformMatrixTRC();
          }
          else {
            if (bOwnsProfile)
              delete pProfile;
            return NULL;
          }
        }
        else if (pTag->GetType()==icSigMultiProcessElementType) {
          rv = new (std::nothrow) CIccXformMpe(pTag);
        }
        else {
          switch(pProfile->m_Header.colorSpace) {
            case icSigXYZData:
            case icSigLabData:
            case icSigLuvData:
            case icSigYCbCrData:
            case icSigYxyData:
            case icSigRgbData:
            case icSigHsvData:
            case icSigHlsData:
            case icSigCmyData:
            case icSig3colorData:
              rv = new (std::nothrow) CIccXform3DLut(pTag);
              break;

            case icSigCmykData:
            case icSig4colorData:
              rv = new (std::nothrow) CIccXform4DLut(pTag);
              break;

            default:
              rv = new (std::nothrow) CIccXformNDLut(pTag);
              break;
          }
        }
      }
      else {
        CIccTag *pTag = NULL; 
        
        if (bUseDToB) {
          pTag = pProfile->FindTag(icSigBToD0Tag + nTagIntent);

          if (!pTag && nTagIntent == icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBToD1Tag);
            if (pTag)
              nTagIntent = icRelativeColorimetric;
          }
          else if (!pTag && nTagIntent != icAbsoluteColorimetric) {
            pTag = pProfile->FindTag(icSigBToD3Tag);
            if (pTag) {
              nTagIntent = icAbsoluteColorimetric;
              bAbsToRel = true;
            }
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigBToD0Tag);
          }
        }

        //Unsupported elements cause fall back behavior
        if (pTag && !pTag->IsSupported())
          pTag = NULL;

        if (bUseColorimeticTags) {
          if (!pTag) {
            if (nTagIntent == icAbsoluteColorimetric)
              nTagIntent = icRelativeColorimetric;
            pTag = pProfile->FindTag(icSigBToA0Tag + nTagIntent);
          }

          if (!pTag) {
            pTag = pProfile->FindTag(icSigBToA0Tag);
          }
        }

        if (!pTag) {
          if (bUseColorimeticTags && pProfile->m_Header.colorSpace == icSigRgbData && pProfile->m_Header.version<icVersionNumberV5) {
            rv = new (std::nothrow) CIccXformMatrixTRC();
          }
          else {
            if (bOwnsProfile)
              delete pProfile;
            return NULL;
          }
        }

        if (pTag && pTag->GetType()==icSigMultiProcessElementType) {
          rv = new (std::nothrow) CIccXformMpe(pTag);
        }
        else {
          switch(pProfile->m_Header.pcs) {
            case icSigXYZData:
            case icSigLabData:
              rv = new (std::nothrow) CIccXform3DLut(pTag);
              break;

            default:
              break;
          }
        }
      }
      break;

    case icXformLutNamedColor:
      {
        CIccTag *pTag = pProfile->FindTag(icSigNamedColor2Tag);
        if (!pTag) {
          if (bOwnsProfile)
            delete pProfile;
          return NULL;
        }

        rv = new (std::nothrow) CIccXformNamedColor(pTag, pProfile->m_Header.pcs, pProfile->m_Header.colorSpace,
                                     pProfile->m_Header.spectralPCS,
                                     &pProfile->m_Header.spectralRange,
                                     &pProfile->m_Header.biSpectralRange);
      }
      break;

    case icXformLutPreview:
      {
        bInput = false;
        CIccTag *pTag = pProfile->FindTag(icSigPreview0Tag + nTagIntent);
        if (!pTag) {
          pTag = pProfile->FindTag(icSigPreview0Tag);
        }
        if (!pTag) {
          if (bOwnsProfile)
            delete pProfile;
          return NULL;
        }
        else {
          switch(pProfile->m_Header.pcs) {
            case icSigXYZData:
            case icSigLabData:
              rv = new (std::nothrow) CIccXform3DLut(pTag);
              break;

            default:
              break;
          }
        }
      }
      break;

    case icXformLutGamut:
      {
        bInput = false;
        CIccTag *pTag = pProfile->FindTag(icSigGamutTag);
        if (!pTag) {
          if (bOwnsProfile)
            delete pProfile;
          return NULL;
        }
        else {
          switch(pProfile->m_Header.pcs) {
            case icSigXYZData:
            case icSigLabData:
              rv = new (std::nothrow) CIccXform3DLut(pTag);
              break;

            default:
              break;
          }
        }
      }
      break;
      
  default:
    break;
  }

  if (rv) {
    rv->SetParams(pProfile, bInput, nIntent, nTagIntent, bUseSpectralPCS, nInterp, pHintManager, bAbsToRel);

    // Same reasoning as the icXformLutGamut case in CIccXform::Create(): this
    // overload has no in-tree caller, but it is public API and its gamut case
    // forces bInput false the same way, so it needs the same marking.
    if (nLutType == icXformLutGamut)
      rv->SetGamutXform();
  }
  else if (bOwnsProfile) {
    // No xform was produced; pProfile never reached SetParams(), so free it here
    // when we own it.  A borrowed profile is left for its real owner to free.
    delete pProfile;
  }

  return rv;
}

/**
**************************************************************************
* Name: CIccXformMPE::IsLateBinding
* 
* Purpose: 
*  Determines if any processing elements are late binding with connection
*  conditions
**************************************************************************
*/
bool CIccXformMpe::IsLateBinding() const
{
  if (m_pTag)
    return m_pTag->IsLateBinding();

  return false;
}

/**
**************************************************************************
* Name: CIccXformMPE::IsLateBindingReflectance
* 
* Purpose: 
*  Determines if any processing elements are late binding with connection
*  conditions
**************************************************************************
*/
bool CIccXformMpe::IsLateBindingReflectance() const
{
  if (m_pTag)
    return m_pTag->IsLateBindingReflectance();

  return false;
}

/**
**************************************************************************
* Name: CIccXformMPE::GetConnectionConditions
* 
* Purpose: 
*  Gets appropriate connection conditions
**************************************************************************
*/
IIccProfileConnectionConditions *CIccXformMpe::GetConnectionConditions() const
{
  if (m_pAppliedPCC)
    return m_pAppliedPCC;

  return m_pConnectionConditions;
}

/**
**************************************************************************
* Name: CIccXformMPE::SetAppliedCC
* 
* Purpose: 
*  This creates combined connection conditions based on profile, 
*  alternate connection conditions and whether reflectance is used
*  by any late binding processing elements.
**************************************************************************
*/
void CIccXformMpe::SetAppliedCC(IIccProfileConnectionConditions *pPCC)
{
  // CIccCmm::SetLateBindingCC may run more than once (e.g. once per
  // CIccCmmSearch::Begin candidate) and feeds an xform's own
  // GetConnectionConditions() back into SetAppliedCC. If we are handed the
  // connection conditions we already applied, there is nothing to do:
  // re-wrapping would leak the current combined PCC, and because the new
  // CIccCombinedConnectionConditions stores a pointer to it, freeing the old
  // one would leave that wrapper dangling (use-after-free).
  if (pPCC && pPCC == m_pAppliedPCC)
    return;

  // Otherwise we are replacing the applied PCC. Release any previously-owned
  // one first so it does not leak across repeated calls. This is safe because
  // the incoming pPCC does not alias it (that case returned above), so no
  // freshly-built combined PCC can reference the object being freed.
  if (m_bDeleteAppliedPCC) {
    delete m_pAppliedPCC;
  }
  m_pAppliedPCC = NULL;
  m_bDeleteAppliedPCC = false;

  if (!pPCC) {
    return;
  }

  if (m_pTag) {
    bool bReflectance = m_pTag->IsLateBindingReflectance();

    if (pPCC != (IIccProfileConnectionConditions *)m_pProfile) {
      if (!bReflectance) {
        const CIccTagSpectralViewingConditions *pViewPCC = pPCC ? pPCC->getPccViewingConditions() : NULL;
        const CIccTagSpectralViewingConditions *pViewProfile = m_pProfile ? m_pProfile->getPccViewingConditions() : NULL;

        if (pViewPCC && pViewProfile &&
          pViewPCC->getStdIllumiant() == pViewProfile->getStdIllumiant() &&
          pViewPCC->getIlluminantCCT() == pViewProfile->getIlluminantCCT() &&
          pViewPCC->getStdIllumiant() != icIlluminantUnknown) {
          m_pAppliedPCC = pPCC;
          m_bDeleteAppliedPCC = false;
        }
        else {
          // check first, because we really shouldn't fail in a constructor
          auto *pView = pPCC->getPccViewingConditions();
          if (pView) {
            m_pAppliedPCC = new CIccCombinedConnectionConditions(m_pProfile, pPCC, bReflectance);
            m_bDeleteAppliedPCC = true;
          } else {
            m_pAppliedPCC = pPCC;
            m_bDeleteAppliedPCC = false;
          }
        }
      }
      else {
        m_pAppliedPCC = new CIccCombinedConnectionConditions(m_pProfile, pPCC, bReflectance);
        m_bDeleteAppliedPCC = true;
      }
    }
    else {
      m_pAppliedPCC = NULL;
    }
  }
  else {
    m_pAppliedPCC = pPCC;
    m_bDeleteAppliedPCC = false;
  }
}


/**
**************************************************************************
* Name: CIccXformMPE::Begin
* 
* Purpose: 
*  This function will be called before the xform is applied.  Derived objects
*  should also call the base class function to initialize for Absolute Colorimetric
*  Intent handling which is performed through the use of the CheckSrcAbs and
*  CheckDstAbs functions.
**************************************************************************
*/
icStatusCMM CIccXformMpe::Begin()
{
  icStatusCMM status;
  status = CIccXform::Begin();

  if (status != icCmmStatOk)
    return status;

  if (!m_pTag) {
    return icCmmStatInvalidLut;
  }
  
  // make sure the input and output samples match, or we could cause an access violation
  icUInt16Number inputSamples = GetNumSrcSamples();
  icUInt16Number outputSamples = GetNumDstSamples();
  icUInt16Number xformInputSamples = m_pTag->NumInputChannels();
  icUInt16Number xformOutputSamples = m_pTag->NumOutputChannels();
  
  if (inputSamples != xformInputSamples || outputSamples != xformOutputSamples)
    return icCmmStatBadXform;

  if (!m_pTag->Begin(icElemInterpLinear, GetProfileCC(), GetConnectionConditions(), GetCmmEnvVarLookup())) {
    return icCmmStatInvalidProfile;
  }

  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccXformMpe::GetNewApply
* 
* Purpose: 
*  This Factory function allocates data specific for the application of the xform.
*  This allows multiple threads to simultaneously use the same xform.
**************************************************************************
*/
CIccApplyXform *CIccXformMpe::GetNewApply(icStatusCMM &status)
{
  if (!m_pTag)
    return NULL;

  CIccApplyXformMpe *rv= new (std::nothrow) CIccApplyXformMpe(this); 

  if (!rv) {
    status = icCmmStatAllocErr;
    return NULL;
  }

  rv->m_pApply = m_pTag->GetNewApply();
  if (!rv->m_pApply) {
    status = icCmmStatAllocErr;
    delete rv;
    return NULL;
  }

  status = icCmmStatOk;
  return rv;
}


/**
**************************************************************************
* Name: CIccXformMPE::Apply
* 
* Purpose: 
*  Does the actual application of the Xform.
*  
* Args:
*  pApply = ApplyXform object containging temporary storage used during Apply
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
void CIccXformMpe::Apply(CIccApplyXform* pApply, icFloatNumber *DstPixel, const icFloatNumber *SrcPixel) const
{
  const CIccTagMultiProcessElement *pTag = m_pTag;

  icFloatNumber temp[3];
  if (!m_bInput || m_bPcsAdjustXform) { //PCS comming in?
    if (m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent) {  //B2D3 tags don't need abs conversion
      if (m_bSrcPcsConversion)
        SrcPixel = CheckSrcAbs(pApply, SrcPixel);
    }

    //Since MPE tags use "real" values for PCS we need to convert from 
    //internal encoding used by IccProfLib
    switch (GetSrcSpace()) {
      case icSigXYZData:
        memcpy(&temp[0], SrcPixel, 3*sizeof(icFloatNumber));
        icXyzFromPcs(temp);
        SrcPixel = &temp[0];
        break;

      case icSigLabData:
        memcpy(&temp[0], SrcPixel, 3*sizeof(icFloatNumber));
        icLabFromPcs(temp);
        SrcPixel = &temp[0];
        break;

      default:
        break;
    }
  }

  //Note: pApply should be a CIccApplyXformMpe type here
  CIccApplyXformMpe *pApplyMpe = (CIccApplyXformMpe *)pApply;

  pTag->Apply(pApplyMpe->m_pApply, DstPixel, SrcPixel);

  if (m_bInput) { //PCS going out?
    //Since MPE tags use "real" values for PCS we need to convert to
    //internal encoding used by IccProfLib
    switch(GetDstSpace()) {
      case icSigXYZData:
        icXyzToPcs(DstPixel);
        break;

      case icSigLabData:
        icLabToPcs(DstPixel);
        break;

      default:
        break;
    }

    if (m_nIntent != icAbsoluteColorimetric || m_nIntent != m_nTagIntent) { //D2B3 tags don't need abs conversion
      if (m_bDstPcsConversion)
        CheckDstAbs(DstPixel);
    }
  }
}

/**
**************************************************************************
* Name: CIccApplyXformMpe::CIccApplyXformMpe
* 
* Purpose: 
*  Constructor
**************************************************************************
*/
CIccApplyXformMpe::CIccApplyXformMpe(CIccXformMpe *pXform) : CIccApplyXform(pXform)
{
    m_pApply = NULL;
}

/**
**************************************************************************
* Name: CIccApplyXformMpe::~CIccApplyXformMpe
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccApplyXformMpe::~CIccApplyXformMpe()
{
  delete m_pApply;
}


/**
**************************************************************************
* Name: CIccApplyCmm::CIccApplyCmm
* 
* Purpose: 
*  Constructor
*
* Args:
*  pCmm = ptr to CMM to apply against
**************************************************************************
*/
CIccApplyCmm::CIccApplyCmm(CIccCmm *pCmm)
{
  m_pCmm = pCmm;
  //m_pPCS = m_pCmm->GetPCS();

  m_Xforms = new CIccApplyXformList;
  m_Xforms->clear();

  m_Pixel = NULL;
  m_Pixel2 = NULL;
  m_ChunkBuf[0] = NULL;
  m_ChunkBuf[1] = NULL;
}

/**
**************************************************************************
* Name: CIccApplyCmm::~CIccApplyCmm
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccApplyCmm::~CIccApplyCmm()
{
  if (m_Xforms) {
    CIccApplyXformList::iterator i;

    for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
      delete i->ptr;
    }

    delete m_Xforms;
  }

//  delete m_pPCS;

  free(m_Pixel);
  free(m_Pixel2);
  free(m_ChunkBuf[0]);
  free(m_ChunkBuf[1]);
}

// Chunk size for transform-sequential multi-pixel apply (pixels per batch).
// Tuned so two chunk buffers (each kCmmChunkPixels * max_samples * 4 bytes)
// fit comfortably in L2 cache on typical hardware.
static const icUInt32Number kCmmChunkPixels = 256;

bool CIccApplyCmm::InitPixel()
{
  if (m_Pixel && m_Pixel2 && m_ChunkBuf[0] && m_ChunkBuf[1])
    return true;

  icUInt16Number nSamples = 16;
  CIccApplyXformList::iterator i;

  for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
    const CIccXform *xform = i->ptr->GetXform();
    if (xform) {
      icUInt16Number nXformSamples = xform->GetNumDstSamples();
      if (nXformSamples>nSamples)
        nSamples=nXformSamples;
      nXformSamples = xform->GetNumSrcSamples();
      if (nXformSamples>nSamples)
        nSamples=nXformSamples;
    }
  }
  m_Pixel = (icFloatNumber*)malloc(nSamples*sizeof(icFloatNumber));
  m_Pixel2 = (icFloatNumber*)malloc(nSamples*sizeof(icFloatNumber));

  if (!m_Pixel || !m_Pixel2)
    return false;

  m_ChunkBuf[0] = (icFloatNumber*)malloc(kCmmChunkPixels * nSamples * sizeof(icFloatNumber));
  m_ChunkBuf[1] = (icFloatNumber*)malloc(kCmmChunkPixels * nSamples * sizeof(icFloatNumber));

  if (!m_ChunkBuf[0] || !m_ChunkBuf[1])
    return false;

  return true;
}

//#define DEBUG_CMM_APPLY

#ifdef DEBUG_CMM_APPLY
static void DumpCmmApplyPixel(int nCount, const icFloatNumber* pPixel, icUInt16Number nSamples)
{
  printf("Xfm%d:", nCount);
  for (icUInt16Number i = 0; i < nSamples; i++) {
    if (i)
      printf(",");
    printf(" %.3f", pPixel[i]);
  }
  printf("\n");

}
#endif

/**
**************************************************************************
* Name: CIccApplyCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
icStatusCMM CIccApplyCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel)
{
  icFloatNumber *pDst, *pTmp;
  const icFloatNumber *pSrc;
  CIccApplyXformList::iterator i;
  //const CIccXform *pLastXform;
  int j, n = (int)m_Xforms->size();
  // bool bNoClip;  // set but not used, except in commented out code

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel()) {
    return icCmmStatAllocErr;
  }

  pSrc = SrcPixel;
  pDst = m_Pixel;

#ifdef DEBUG_CMM_APPLY
  int nCount = 0;
  printf("Start ApplyCmm\n");
  DumpCmmApplyPixel(nCount++, pSrc, icGetSpaceSamples(m_pCmm->m_nSrcSpace));
#endif

  if (n>1) {
    for (j=0, i=m_Xforms->begin(); j<n-1 && i!=m_Xforms->end(); i++, j++) {

      i->ptr->Apply(pDst, pSrc);

#ifdef DEBUG_CMM_APPLY
      DumpCmmApplyPixel(nCount++, pDst, i->ptr->GetXform()->GetNumDstSamples());
#endif

      pTmp = (icFloatNumber*)pSrc;
      pSrc = pDst;
      if (pTmp == SrcPixel)
        pDst = m_Pixel2;
      else
        pDst = pTmp;
    }

    // pLastXform = i->ptr->GetXform();     // set, but only used by unused value below
    i->ptr->Apply(DstPixel, pSrc);
    // bNoClip = pLastXform->NoClipPCS();  // set but not used
  }
  else if (n==1) {
    i = m_Xforms->begin();

    // pLastXform = i->ptr->GetXform();  // set, but only used by unused value below
    i->ptr->Apply(DstPixel, SrcPixel);

#ifdef DEBUG_CMM_APPLY
    DumpCmmApplyPixel(nCount++, pDst, i->ptr->GetXform()->GetNumDstSamples());
#endif

    // bNoClip = pLastXform->NoClipPCS();  // set but not used
  }
  else {
    // bNoClip = true;
  }

  //m_pPCS->CheckLast(DstPixel, m_pCmm->m_nDestSpace, bNoClip);

#ifdef DEBUG_CMM_APPLY
  DumpCmmApplyPixel(nCount, DstPixel, icGetSpaceSamples(m_pCmm->m_nDestSpace));
  printf("End ApplyCmm\n\n");
#endif

  return icCmmStatOk;
}

/**
**************************************************************************
* Name: CIccApplyCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
icStatusCMM CIccApplyCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels)
{
  int n = (int)m_Xforms->size();

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel())
    return icCmmStatAllocErr;

  if (n == 1) {
    m_Xforms->begin()->ptr->ApplyN(DstPixel, SrcPixel, nPixels);
    return icCmmStatOk;
  }

  // Transform-sequential chunked apply: process kCmmChunkPixels through each
  // xform in turn before advancing to the next xform.  This keeps each xform's
  // CLUT data warm in L2/L3 cache across all pixels in the chunk.
  int nSrcSamples = m_pCmm->GetSourceSamples();
  int nDstSamples = m_pCmm->GetDestSamples();
  icUInt32Number offset = 0;

  while (offset < nPixels) {
    icUInt32Number chunk = nPixels - offset;
    if (chunk > kCmmChunkPixels)
      chunk = kCmmChunkPixels;

    icFloatNumber *pBuf0 = m_ChunkBuf[0];
    icFloatNumber *pBuf1 = m_ChunkBuf[1];
    const icFloatNumber *pSrc = SrcPixel + offset * nSrcSamples;
    icFloatNumber *pDstChunk  = DstPixel  + offset * nDstSamples;

    CIccApplyXformList::iterator i = m_Xforms->begin();

    // First xform: src -> buf0
    i->ptr->ApplyN(pBuf0, pSrc, chunk);
    ++i;

    // Middle xforms: ping-pong buf0 <-> buf1
    for (int j = 1; j < n - 1; ++j, ++i) {
      i->ptr->ApplyN(pBuf1, pBuf0, chunk);
      icFloatNumber *pTmp = pBuf0; pBuf0 = pBuf1; pBuf1 = pTmp;
    }

    // Last xform: buf0 -> final dst
    i->ptr->ApplyN(pDstChunk, pBuf0, chunk);

    offset += chunk;
  }

  return icCmmStatOk;
}

void CIccApplyCmm::AppendApplyXform(CIccApplyXform *pApplyXform)
{
  CIccApplyXformPtr ptr;
  ptr.ptr = pApplyXform;

  m_Xforms->push_back(ptr);
}

/**
 **************************************************************************
 * Name: CIccCmm::CIccCmm
 * 
 * Purpose: 
 *  Constructor
 *
 * Args:
 *  nSrcSpace = signature of the source color space,
 *  nDestSpace = signature of the destination color space,
 *  bFirstInput = true if the first profile added is an input profile
 **************************************************************************
 */
CIccCmm::CIccCmm(icColorSpaceSignature nSrcSpace /*=icSigUnknownData*/,
                 icColorSpaceSignature nDestSpace /*=icSigUnknownData*/,
                 bool bFirstInput /*=true*/)
{
  m_bValid = false;

  m_bLastInput = !bFirstInput;
  m_nSrcSpace = nSrcSpace;
  m_nDestSpace = nDestSpace;

  m_nLastSpace = nSrcSpace;
  m_nLastParentSpace = icSigNoColorData;
  m_nLastIntent = icUnknownIntent;

  m_Xforms = new CIccXformList;
  m_Xforms->clear();

  m_pApply = NULL;
}

/**
 **************************************************************************
 * Name: CIccCmm::~CIccCmm
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccCmm::~CIccCmm()
{
  if (m_Xforms) {
    CIccXformList::iterator i;

    for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
      delete i->ptr;
    }

    delete m_Xforms;
  }

  delete m_pApply;
}

const icChar* CIccCmm::GetStatusText(icStatusCMM stat)
{
  switch (stat) {
  case icCmmStatBad:
    return "Bad CMM";
  case icCmmStatOk:
    return "OK";
  case icCmmStatCantOpenProfile:
    return "Cannot open profile";
  case icCmmStatBadSpaceLink:
    return "Invalid space link";
  case icCmmStatInvalidProfile:
    return "Invalid profile";
  case icCmmStatBadXform:
    return "Invalid profile transform";
  case icCmmStatInvalidLut:
    return "Invalid Look-Up Table";
  case icCmmStatProfileMissingTag:
    return "Missing tag in profile";
  case icCmmStatColorNotFound:
    return "Colour not found";
  case icCmmStatIncorrectApply:
    return "Incorrect Apply object";
  case icCmmStatBadColorEncoding:
    return "Invalid colour encoding used";
  case icCmmStatAllocErr:
    return "Memory allocation error";
  case icCmmStatBadLutType:
    return "Invalid Look-Up Table type";
  case icCmmStatIdentityXform:
    return "Identity transform used";
  case icCmmStatUnsupportedPcsLink:
    return "Unsupported PCS Link used";
  case icCmmStatBadConnection:
    return "Invalid profile connection";
  case icCmmStatBadTintXform:
    return "Invalid tint transform";
  case icCmmStatTooManySamples:
    return "Too many samples used";
  case icCmmStatBadMCSLink:
    return "Invalid MCS link connection";
  // icCmmStatUnsupported was added to icStatusCMM without a matching case
  // here (the enum declaration in IccCmm.h asks for GetStatusText to be kept
  // in sync).  Added while wiring status names into the CLI tool error
  // messages for issues #1322/#1323 so every enum value decodes to text.
  case icCmmStatUnsupported:
    return "Unsupported operation";
  // The evaluators refuse a profile whose class carries no device<->PCS
  // transform pair rather than reporting it as damaged; keep the text about the
  // *class* so the caller can tell "this profile is broken" from "this
  // operation does not apply to this kind of profile" (#1843).
  case icCmmStatUnsupportedProfileClass:
    return "Unsupported profile class";
  default:
    return "Unknown CMM Status value";

  }
}

/**
 **************************************************************************
 * Name: CIccCmm::AddXform
 * 
 * Purpose: 
 *  Adds a profile at the end of the Xform list 
 * 
 * Args: 
 *  szProfilePath = file name of the profile to be added,
 *  nIntent = rendering intent to be used with the profile,
 *  nInterp = type of interpolation to be used with the profile,
 *  nLutType = selection of which transform lut to use
 *  pHintManager = hints for creating the xform
 * 
 * Return: 
 *  icCmmStatOk, if the profile was added to the list succesfully
 **************************************************************************
 */
icStatusCMM CIccCmm::AddXform(const icChar *szProfilePath,
                              icRenderingIntent nIntent /*=icUnknownIntent*/,
                              icXformInterp nInterp /*icXformInterp*/,
                              IIccProfileConnectionConditions *pPcc/*=NULL*/,
                              icXformLutType nLutType /*=icXformLutColor*/,
                              bool bUseD2BxB2DxTags /*=true*/,
                              CIccCreateXformHintManager *pHintManager /*=NULL*/,
                              bool bUseSubProfile /*=false*/)
{
  CIccProfile *pProfile = OpenIccProfile(szProfilePath, bUseSubProfile);

  if (!pProfile) 
    return icCmmStatCantOpenProfile;

  icStatusCMM rv = AddXform(pProfile, nIntent, nInterp, pPcc, nLutType, bUseD2BxB2DxTags, pHintManager);
  // AddXform took ownership of the profile pointer, or deleted it if there was an error

  return rv;
}


/**
**************************************************************************
* Name: CIccCmm::AddXform
* 
* Purpose: 
*  Adds a profile at the end of the Xform list 
* 
* Args: 
*  pProfileMem = ptr to profile loaded into memory. Note: this memory
*   needs to be available until after the Begin() function is called.
*  nProfileLen = size in bytes of profile loaded into memory
*  nIntent = rendering intent to be used with the profile,
*  nInterp = type of interpolation to be used with the profile,
*  nLutType = selection of which transform lut to use
*  bUseD2BxB2DxTags = flag to indicate the use MPE flags if available
*  pHintManager = hints for creating the xform
* 
* Return: 
*  icCmmStatOk, if the profile was added to the list succesfully
**************************************************************************
*/
icStatusCMM CIccCmm::AddXform(icUInt8Number *pProfileMem,
                              icUInt32Number nProfileLen,
                              icRenderingIntent nIntent /*=icUnknownIntent*/,
                              icXformInterp nInterp /*icXformInterp*/,
                              IIccProfileConnectionConditions *pPcc/*=NULL*/,
                              icXformLutType nLutType /*=icXformLutColor*/,
                              bool bUseD2BxB2DxTags /*=true*/,
                              CIccCreateXformHintManager *pHintManager /*=NULL*/,
                              bool bUseSubProfile /*=false*/)
{
  CIccMemIO *pFile = new (std::nothrow) CIccMemIO;

  if (!pFile || !pFile->Attach(pProfileMem, nProfileLen))
    return icCmmStatCantOpenProfile;

  CIccProfile *pProfile = new (std::nothrow) CIccProfile;

  if (!pProfile)
    return icCmmStatCantOpenProfile;

  if (!pProfile->Attach(pFile, bUseSubProfile)) {
    delete pFile;
    delete pProfile;
    return icCmmStatCantOpenProfile;
  }

  icStatusCMM rv = AddXform(pProfile, nIntent, nInterp, pPcc, nLutType, bUseD2BxB2DxTags, pHintManager);
  // AddXform took ownership of the profile pointer, or deleted it if there was an error

  return rv;
}


/**
 **************************************************************************
 * Name: CIccCmm::AddXform
 * 
 * Purpose: 
 *  Adds a profile at the end of the Xform list 
 * 
 * Args: 
 *  pProfile = pointer to the CIccProfile object to be added (profile will be owned by the CMM's added xform),
 *      AddXform takes ownership of the profile, or deletes the profile on error.
 *  nIntent = rendering intent to be used with the profile,
 *  nInterp = type of interpolation to be used with the profile,
 *  nLutType = selection of which transform lut to use
 *  bUseD2BxB2DxTags = flag to indicate the use MPE flags if available
 *  pHintManager = hints for creating the xform
 * 
 * Return: 
 *  icCmmStatOk, if the profile was added to the list succesfully
 **************************************************************************
 */
icStatusCMM CIccCmm::AddXform(CIccProfile *pProfile,
                              icRenderingIntent nIntent /*=icUnknownIntent*/,
                              icXformInterp nInterp /*=icInterpLinear*/,
                              IIccProfileConnectionConditions *pPcc/*=NULL*/,
                              icXformLutType nLutType /*=icXformLutColor*/,
                              bool bUseD2BxB2DxTags /*=true*/,
                              CIccCreateXformHintManager *pHintManager /*=NULL*/)
{
  icColorSpaceSignature nSrcSpace, nDstSpace, nParentSpace=icSigNoColorData;
  bool bInput = !m_bLastInput;

  if (!pProfile)
    return icCmmStatInvalidProfile;

  switch(pProfile->m_Header.deviceClass) {
    case icSigMultiplexIdentificationClass:
    case icSigMultiplexVisualizationClass:
    case icSigMultiplexLinkClass:
      nIntent = icPerceptual;
      nLutType = icXformLutMCS;
      break;

    default:
      break;
  }

  switch (nLutType) {
    case icXformLutColor:
    case icXformLutColorimetric:
    case icXformLutSpectral:
    {
      //Check pProfile if nIntent and input can be found.
      if (bInput) {
        nSrcSpace = pProfile->m_Header.colorSpace;
        nParentSpace = pProfile->GetParentColorSpace();

        // Use spectralPCS as the destination when nLutType explicitly asks for
        // it, when the caller opted in via bUseD2BxB2DxTags, or when the
        // profile is spectral-only (no colorimetric pcs) so spectralPCS is the
        // only valid destination - matches the DToBx fallback in CIccXform::Create.
        if (nLutType == icXformLutSpectral ||
            (pProfile->m_Header.spectralPCS && nLutType != icXformLutColorimetric &&
             (bUseD2BxB2DxTags || !pProfile->m_Header.pcs)))
          nDstSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
        else
          nDstSpace = pProfile->m_Header.pcs;
      }
      else {
        if (pProfile->m_Header.deviceClass == icSigLinkClass) {
          delete pProfile;
          return icCmmStatBadSpaceLink;
        }
        if (pProfile->m_Header.deviceClass == icSigAbstractClass) {
          bInput = true;
          nIntent = icPerceptual; // Note: icPerceptualIntent = 0
        }

        // Symmetric to the bInput branch above: spectral-only destination profiles
        // accept their spectralPCS as the source space even without bUseD2BxB2DxTags.
        if (nLutType == icXformLutSpectral ||
            (pProfile->m_Header.spectralPCS && nLutType != icXformLutColorimetric &&
             (bUseD2BxB2DxTags || !pProfile->m_Header.pcs)))
          nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
        else
          nSrcSpace = pProfile->m_Header.pcs;

        nDstSpace = pProfile->m_Header.colorSpace;
        nParentSpace = pProfile->GetParentColorSpace();
      }
    }
    break;

    case icXformLutPreview:
      nSrcSpace = pProfile->m_Header.pcs;
      nDstSpace = pProfile->m_Header.pcs;
      bInput = false;
      break;

    case icXformLutGamut:
      nSrcSpace = pProfile->m_Header.pcs;
      nDstSpace = icSigGamutData;
      bInput = true;
      break;

    case icXformLutBRDFParam:
      if (!bInput) {
        delete pProfile;
        return icCmmStatBadSpaceLink;
      }
      nSrcSpace = pProfile->m_Header.colorSpace;
      nDstSpace = icSigBRDFParameters;
      bInput = true;
      break;

    case icXformLutBRDFDirect:
      if (!bInput) {
        delete pProfile;
        return icCmmStatBadSpaceLink;
      }
      nSrcSpace = icSigBRDFDirect;
      nDstSpace = pProfile->m_Header.pcs;
      bInput = true;
      break;

    case icXformLutBRDFMcsParam:
      if (!bInput) {
        delete pProfile;
        return icCmmStatBadSpaceLink;
      }
      nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.mcs;
      nDstSpace = icSigBRDFParameters;
      break;

    case icXformLutMCS:
      if (bInput) {
        nSrcSpace = pProfile->m_Header.colorSpace;
        nParentSpace = pProfile->GetParentColorSpace();
        nDstSpace = (icColorSpaceSignature)pProfile->m_Header.mcs;
      }
      else {
        if (m_Xforms->size()) {
          CIccXformList::iterator prev = --(m_Xforms->end());
          
          //Make sure previous profile connects with an icXformLutMCS
          if ((icXformLutType)(prev->ptr->GetXformType()) != icXformLutMCS) {
            //check to see if we can convert previous xform to connect via an MCS
            if (!prev->ptr->GetProfile()->m_Header.mcs) {
              delete pProfile;
              return icCmmStatBadMCSLink;
            }

            CIccXform *pPrev = prev->ptr;
            // pPrev still owns this profile: pass bOwnsProfile=false so a failed
            // Create leaves it intact (pPrev is left in place below on failure).
            // On success pPrev->DetachAll() transfers ownership to pNew, so no
            // ShareProfile() is needed here.
            CIccXform *pNew = CIccXform::Create(pPrev->GetProfilePtr(), pPrev->IsInput(), pPrev->GetIntent(), pPrev->GetInterp(),
                                                pPrev->GetConnectionConditions(), icXformLutMCS, bUseD2BxB2DxTags, pHintManager,
                                                /*bOwnsProfile=*/false);

            if (!pNew) {
              // Create failed (e.g., previous profile has no MCS-
              // suitable A2B / D2B tag, or factory was stripped).
              // Do NOT overwrite prev->ptr with NULL - that would
              // leave the list with a NULL xform that the next Apply
              // dereferences -> SIGSEGV. Leave pPrev in place and
              // surface a clean error status.
              delete pProfile;
              return icCmmStatBadMCSLink;
            }

            pPrev->DetachAll();
            delete pPrev;
            prev->ptr = pNew;

          }
        }
        else {
          delete pProfile;
          return icCmmStatBadMCSLink;
        }

        nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.mcs;
        if (pProfile->m_Header.deviceClass==icSigMultiplexVisualizationClass ||
            pProfile->m_Header.deviceClass==icSigOutputClass) {
          if (bUseD2BxB2DxTags && pProfile->m_Header.spectralPCS) {
            nDstSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
          }
          else {
            nDstSpace = pProfile->m_Header.pcs;
          }
        }
        else if (pProfile->m_Header.deviceClass==icSigMultiplexLinkClass) {
          nDstSpace = pProfile->m_Header.colorSpace;
          nParentSpace = pProfile->GetParentColorSpace();
        }
        else {
          delete pProfile;
          return icCmmStatBadSpaceLink;
        }
      }
      break;

    default:
      delete pProfile;
      return icCmmStatBadLutType;
  }

  //Make sure colorspaces match with previous xforms
  if (!m_Xforms->size()) {
    if (m_nSrcSpace == icSigUnknownData) {
      m_nLastSpace = nSrcSpace;
      m_nLastParentSpace = nParentSpace;
      m_nSrcSpace = nSrcSpace;
    }
    else if (!IsCompatSpace(m_nSrcSpace, nSrcSpace)) {
      delete pProfile;
      return icCmmStatBadSpaceLink;
    }
  }
  else if (!IsCompatSpace(m_nLastSpace, nSrcSpace)) {
    delete pProfile;
    return icCmmStatBadSpaceLink;
  }

  if (nSrcSpace==icSigNamedData) {
    delete pProfile;
    return icCmmStatBadSpaceLink;
  }
  
  //Automatic creation of intent from header/last profile
  if (nIntent==icUnknownIntent) {
    if (bInput) {
      nIntent = (icRenderingIntent)pProfile->m_Header.renderingIntent;
    }
    else {
      nIntent = m_nLastIntent;
    }
    if (nIntent == icUnknownIntent)
      nIntent = icPerceptual;
  }

  // this must check before creating the Xform, because that can delete the profile
  if (pProfile->m_Header.deviceClass == icSigMultiplexVisualizationClass) {
    bInput = true;
  }

  CIccXformPtr Xform;
  
  Xform.ptr = CIccXform::Create(pProfile, bInput, nIntent, nInterp, pPcc, nLutType, bUseD2BxB2DxTags, pHintManager);

  if (!Xform.ptr) {
    // profile was deleted inside CIccXform::Create
    return icCmmStatBadXform;
  }

  if (Xform.ptr->IsMCS() && Xform.ptr->IsInput()) {
    bInput = true;
  }

  m_nLastSpace = nDstSpace;
  m_nLastParentSpace = nParentSpace;
  m_nLastIntent = nIntent;
  m_bLastInput = bInput;

  m_Xforms->push_back(Xform);

  return icCmmStatOk;
}

/**
**************************************************************************
* Name: CIccCmm::AddXform
*
* Purpose:
*  Adds a profile at the end of the Xform list
*
* Args:
*  pProfile = pointer to the CIccProfile object to be added,
*      AddXform takes ownership of the profile, or deletes the profile on error.
*  nIntent = rendering intent to be used with the profile,
*  nInterp = type of interpolation to be used with the profile,
*  nLutType = selection of which transform lut to use
*  bUseD2BxB2DxTags = flag to indicate the use MPE flags if available
*  pHintManager = hints for creating the xform
*
* Return:
*  icCmmStatOk, if the profile was added to the list succesfully
**************************************************************************
*/
icStatusCMM CIccCmm::AddXform(CIccProfile *pProfile,
                              CIccTag *pXformTag,
                              icRenderingIntent nIntent/*= icUnknownIntent*/,
                              icXformInterp nInterp /*=icInterpLinear*/,
                              IIccProfileConnectionConditions *pPcc/*=NULL*/,
                              bool bUseSpectralPCS /*=false*/,
                              CIccCreateXformHintManager *pHintManager /*=NULL*/)
{
  icColorSpaceSignature nSrcSpace, nDstSpace, nParentSpace = icSigNoColorData;
  bool bInput = !m_bLastInput;

  if (!pProfile)
    return icCmmStatInvalidProfile;

  switch (pProfile->m_Header.deviceClass) {
  case icSigMultiplexIdentificationClass:
  case icSigMultiplexVisualizationClass:
  case icSigMultiplexLinkClass:
    delete pProfile;
    return icCmmStatBadLutType;

  default:
    break;
  }

  //Check pProfile if nIntent and input can be found.
  if (bInput) {
    nSrcSpace = pProfile->m_Header.colorSpace;
    nParentSpace = pProfile->GetParentColorSpace();

    if (bUseSpectralPCS && pProfile->m_Header.spectralPCS)
      nDstSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
    else {
      nDstSpace = pProfile->m_Header.pcs;
      bUseSpectralPCS = false;
    }
  }
  else {
    if (pProfile->m_Header.deviceClass == icSigLinkClass) {
      delete pProfile;
      return icCmmStatBadSpaceLink;
    }
    if (pProfile->m_Header.deviceClass == icSigAbstractClass) {
      bInput = true;
      nIntent = icPerceptual; // Note: icPerceptualIntent = 0
    }

    if (bUseSpectralPCS && pProfile->m_Header.spectralPCS)
      nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
    else {
      nSrcSpace = pProfile->m_Header.pcs;
      bUseSpectralPCS = false;
    }

    nDstSpace = pProfile->m_Header.colorSpace;
    nParentSpace = pProfile->GetParentColorSpace();
  }

  //Make sure colorspaces match with previous xforms
  if (!m_Xforms->size()) {
    if (m_nSrcSpace == icSigUnknownData) {
      m_nLastSpace = nSrcSpace;
      m_nSrcSpace = nSrcSpace;
    }
    else if (!IsCompatSpace(m_nSrcSpace, nSrcSpace)) {
      delete pProfile;
      return icCmmStatBadSpaceLink;
    }
  }
  else if (!IsCompatSpace(m_nLastSpace, nSrcSpace)) {
    delete pProfile;
    return icCmmStatBadSpaceLink;
  }

  if (nSrcSpace == icSigNamedData) {
    delete pProfile;
    return icCmmStatBadSpaceLink;
  }

  //Automatic creation of intent from header/last profile
  if (nIntent == icUnknownIntent) {
    if (bInput) {
      nIntent = (icRenderingIntent)pProfile->m_Header.renderingIntent;
    }
    else {
      nIntent = m_nLastIntent;
    }
    if (nIntent == icUnknownIntent)
      nIntent = icPerceptual;
  }

  CIccXformPtr Xform;

  Xform.ptr = CIccXform::Create(pProfile, pXformTag, bInput, nIntent, nInterp, pPcc, bUseSpectralPCS, pHintManager);

  if (!Xform.ptr) {
    // CIccXform::Create has already deleted the profile
    return icCmmStatBadXform;
  }

  m_nLastSpace = nDstSpace;
  m_nLastParentSpace = nParentSpace;
  m_nLastIntent = nIntent;
  m_bLastInput = bInput;

  m_Xforms->push_back(Xform);

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccCmm::AddXform
 * 
 * Purpose: 
 *  Adds a profile at the end of the Xform list 
 * 
 * Args: 
 *  Profile = reference a CIccProfile object that will be copies and added,
 *      AddXform takes ownership of the profile, or deletes the profile on error.
 *  nIntent = rendering intent to be used with the profile,
 *  nInterp = type of interpolation to be used with the profile,
 *  nLutType = selection of which transform lut to use
 *  bUseD2BxB2DxTags = flag to indicate the use MPE flags if available
 *  pHintManager = hints for creating the xform
 * 
 * Return: 
 *  icCmmStatOk, if the profile was added to the list succesfully
 **************************************************************************
 */
icStatusCMM CIccCmm::AddXform(CIccProfile &Profile,
                              icRenderingIntent nIntent /*=icUnknownIntent*/,
                              icXformInterp nInterp /*=icInterpLinear*/,
                              IIccProfileConnectionConditions *pPcc/*=NULL*/,
                              icXformLutType nLutType /*=icXformLutColor*/,
                              bool bUseD2BxB2DxTags /*=true*/,
                              CIccCreateXformHintManager *pHintManager /*=NULL*/)
{
  CIccProfile *pProfile = new (std::nothrow) CIccProfile(Profile);

  // CFL-045: Guard against null PCS causing vptr corruption in copy (CWE-843)
  if (pProfile && (icUInt32Number)pProfile->m_Header.pcs == 0 && (icUInt32Number)pProfile->m_Header.spectralPCS == 0) {
    delete pProfile;
    return icCmmStatInvalidProfile;
  }

  if (!pProfile) 
    return icCmmStatAllocErr;

  //borrow the caller's AttachIO to perform the AddXform
  pProfile->CopyAttach(&Profile, true);
  
  // CFL-078: Save deviceClass before AddXform - cenc ownership transfer
  icProfileClassSignature savedClass = pProfile->m_Header.deviceClass;

  icStatusCMM stat = AddXform(pProfile, nIntent, nInterp, pPcc, nLutType, bUseD2BxB2DxTags, pHintManager);
  // AddXform took ownership of the profile pointer, or deleted it if there was an error
  
  if (stat == icCmmStatOk && savedClass != icSigColorEncodingClass)
    pProfile->CopyAttach(nullptr);
    
  return stat;
}

icStatusCMM CIccCmm::AddXform(CIccXform* pXform)
{
  if (!pXform)
    return icCmmStatBadXform;

  m_nLastSpace = pXform->GetDstSpace();
  m_nLastParentSpace = icSigNoColorData;
  m_nLastIntent = icUnknownIntent;
  m_bLastInput = false;

  CIccXformPtr ptr;
  ptr.ptr = pXform;
  m_Xforms->push_back(ptr);

  return icCmmStatOk;
}

icStatusCMM CIccCmm::CheckPCSConnections(bool bUsePCSConversions/*=false*/)
{
  icStatusCMM rv = icCmmStatOk;

  CIccXformList::iterator last, next;
  CIccXformList xforms;
  CIccXformPtr ptr;
  bool bUsesPcsXforms = false;

  next=m_Xforms->begin();

  if (next!=m_Xforms->end()) {
    last = next;
    next++;

    icColorSpaceSignature lastSpace = last->ptr->GetSrcSpace();
    if (!last->ptr->IsInput() && IsSpaceColorimetricPCS(lastSpace) && (GetSourceSpace() !=lastSpace || last->ptr->UseLegacyPCS())) {
      CIccPcsXform* pPcs = new (std::nothrow) CIccPcsXform();

      if (!pPcs) {
        return icCmmStatAllocErr;
      }

      rv = pPcs->ConnectFirst(last->ptr, GetSourceSpace());

      if (rv != icCmmStatOk && rv != icCmmStatIdentityXform) {
        delete pPcs;
        return rv;
      }

      if (rv != icCmmStatIdentityXform) {
        ptr.ptr = pPcs;
        xforms.push_back(ptr);

        bUsesPcsXforms = true;
      }
      else {
        delete pPcs;
      }
    }

    xforms.push_back(*last);

    for (;next!=m_Xforms->end(); last=next, next++) {
      if ((last->ptr->IsInput() && last->ptr->IsMCS() && next->ptr->IsMCS()) ||
          (IsSpaceSpectralPCS(last->ptr->GetDstSpace()) || IsSpaceSpectralPCS(next->ptr->GetSrcSpace())) ||
          (!bUsePCSConversions && 
           (IsSpaceColorimetricPCS(last->ptr->GetDstSpace()) || IsSpaceColorimetricPCS(next->ptr->GetSrcSpace())))) {
        last->ptr->SetDstPCSConversion(false);
        next->ptr->SetSrcPCSConversion(false);
        CIccPcsXform *pPcs = new (std::nothrow) CIccPcsXform();

        if (!pPcs) {
          return icCmmStatAllocErr;
        }

        rv = pPcs->Connect(last->ptr, next->ptr);

        // A hard Connect() failure aborts CheckPCSConnections.  pPcs is still a
        // locally-owned allocation that has not been handed to the xform list,
        // so free it before returning - the sibling CIccPcsXform blocks (the
        // ConnectFirst case above and the trailing case below) both delete on
        // their failure paths; this one previously leaked it (#1337).
        if (rv!=icCmmStatOk && rv!=icCmmStatIdentityXform) {
          delete pPcs;
          return rv;
        }

        if (rv!=icCmmStatIdentityXform) {
          // A real conversion is needed: ownership of pPcs passes to the xform
          // list, which frees it when the CMM is destroyed.
          ptr.ptr = pPcs;
          xforms.push_back(ptr);

          bUsesPcsXforms = true;
        }
        else {
          // Identity conversion: the spaces already match, so the extra
          // PcsXform is unnecessary - discard it.
          delete pPcs;
        }
      }
      xforms.push_back(*next);
    }

    lastSpace = last->ptr->GetDstSpace();
    if (last->ptr->IsInput() && IsSpaceColorimetricPCS(lastSpace) && 
        (last->ptr->NeedAdjustPCS() || GetDestSpace() != lastSpace || last->ptr->UseLegacyPCS())) {
      CIccPcsXform* pPcs = new (std::nothrow) CIccPcsXform();

      if (!pPcs) {
        return icCmmStatAllocErr;
      }
      rv = pPcs->ConnectLast(last->ptr, GetDestSpace());

      if (rv != icCmmStatOk && rv != icCmmStatIdentityXform) {
        delete pPcs;
        return rv;
      }

      if (rv != icCmmStatIdentityXform) {
        ptr.ptr = pPcs;
        xforms.push_back(ptr);

        bUsesPcsXforms = true;
      }
      else {
        delete pPcs;
      }
    }
  }


  if (bUsesPcsXforms) {
    *m_Xforms = xforms;
  }

  return rv;
}

icStatusCMM CIccCmm::CheckPCSRangeConversions()
{
  icStatusCMM rv = icCmmStatOk;

  CIccXformList::iterator last, next;
  CIccXformList xforms;
  CIccXformPtr ptr;
  bool bUsesRangeConversion = false;

  next = m_Xforms->begin();

  if (next != m_Xforms->end()) {
    last = next;
    next++;

    xforms.push_back(*last);

    for (; next != m_Xforms->end(); last = next, next++) {
      //Add extended PCS to standard PCS conversion as needed
      if (last->ptr->IsInput() && last->ptr->IsExtendedPCS() && IsSpaceColorimetricPCS(last->ptr->GetDstSpace()) &&
        !next->ptr->IsExtendedPCS()) {
        CIccProfile* pProfile = last->ptr->GetProfilePtr();
        CIccTag* pTag = pProfile->FindTag(icSigHToS0Tag + last->ptr->GetIntent());
        if (!pTag) {
          pTag = pProfile->FindTag(icSigHToS0Tag);
          if (!pTag) {
            pTag = pProfile->FindTag(icSigHToS1Tag);
          }
        }
        //If we find the HToSxTag then create a transform for it and inject it into the transform list
        if (pTag) {
          // pProfile is borrowed here (it is shared with the surrounding link and
          // ShareProfile() is called on success below), so pass bOwnsProfile=false
          // to keep a failed Create from deleting a profile this code still owns.
          ptr.ptr = CIccXform::Create(pProfile, pTag, true, last->ptr->GetIntent(), last->ptr->GetInterp(),
                                      last->ptr->GetConnectionConditions(), false /*bUseSpectralPCS*/,
                                      NULL /*pHintManager*/, false /*bOwnsProfile*/);
          if (ptr.ptr) {
            ptr.ptr->ShareProfile(); //Indicate that profile is shared (so it won't be deleted)
            ptr.ptr->SetPcsAdjustXform(); // Indicates that transform should be treated as abstract
            ptr.ptr->AttachCmmEnvVarLookup(last->ptr->GetCmmEnvVarLookup());
            xforms.push_back(ptr);
            bUsesRangeConversion = true;
          }
        }
      }

      xforms.push_back(*next);
    }
  }
  
  if (bUsesRangeConversion) {
    *m_Xforms = xforms;
  }

  return rv;
}


/**
**************************************************************************
* Name: CIccCmm::SetLateBindingCC
* 
* Purpose: 
*  Initializes the LateBinding Connection Conditions used by 
*  colorimetric based transforms
*
**************************************************************************
*/
void CIccCmm::SetLateBindingCC()
{
  CIccXformList::iterator i;
  CIccXform *pLastXform = NULL;

  for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
    if (i->ptr->IsInput()) {
      if (i->ptr->IsLateBinding()) {
        CIccXformList::iterator j=i;
        j++;
        if (j!=m_Xforms->end()) {
          if (j->ptr->IsLateBinding()) {
            i->ptr->SetAppliedCC(i->ptr->GetConnectionConditions());
            j->ptr->SetAppliedCC(i->ptr->GetConnectionConditions());
            pLastXform = i->ptr;
          }
          else {
            i->ptr->SetAppliedCC(i->ptr->GetConnectionConditions());
            j->ptr->SetAppliedCC(j->ptr->GetConnectionConditions());
            pLastXform = NULL;
          }
        }
        else {
          i->ptr->SetAppliedCC(i->ptr->GetConnectionConditions());
          pLastXform = NULL;
        }
      }
      else if (IsSpaceSpectralPCS(i->ptr->GetDstSpace())) {
        CIccXformList::iterator j=i;
        j++;
        if (j!=m_Xforms->end()) {
          if (j->ptr->IsLateBinding()) {
            j->ptr->SetAppliedCC(i->ptr->GetConnectionConditions());
            pLastXform = i->ptr;
          }
          else if (!j->ptr->IsAbstract()){
            j->ptr->SetAppliedCC(j->ptr->GetConnectionConditions());
            pLastXform = NULL;
          }
        }
      }
      else {
        pLastXform = NULL;
      }
    }
    else {
      if (!pLastXform) 
        i->ptr->SetAppliedCC(i->ptr->GetConnectionConditions());
      else
        pLastXform = NULL;
    }
  }
}


/**
**************************************************************************
* Name: CIccCmm::Begin
* 
* Purpose: 
*  Does the initialization of the Xforms before Apply() is called.
*  Must be called before Apply().
*
**************************************************************************
*/
icStatusCMM CIccCmm::Begin(bool bAllocApplyCmm/*=true*/, bool bUsePCSConversions/*=false*/)
{
  if (m_pApply)
    return icCmmStatOk;

  if (m_nDestSpace==icSigUnknownData) {
    m_nDestSpace = m_nLastSpace;
  }
  else if (!IsCompatSpace(m_nDestSpace, m_nLastSpace) ||
           GetDestSamples() != (icUInt16Number)icGetSpaceSamples(m_nLastSpace)) {
    return icCmmStatBadSpaceLink;
  }

  if (m_nSrcSpace==icSigNamedData || m_nDestSpace==icSigNamedData) {
    return icCmmStatBadSpaceLink;
  }

  CheckPCSRangeConversions();
  SetLateBindingCC();

  icStatusCMM rv;
  CIccXformList::iterator i = m_Xforms->begin();
  
  // Make sure the input channel and first transform input counts match.
  // Otherwise we'll have a heap overflow during Apply.
  if (i != m_Xforms->end()) {
    icUInt16Number cmmInputCount = GetSourceSamples();
    icUInt16Number xformInputCount = i->ptr->GetNumSrcSamples();
    if (xformInputCount != cmmInputCount)
      return icCmmStatBadSpaceLink;
  }

  for (; i!=m_Xforms->end(); i++) {

    rv = i->ptr->Begin();

    if (rv!= icCmmStatOk)
      return rv;
  }

  rv = CheckPCSConnections(bUsePCSConversions);
  if (rv != icCmmStatOk && rv!=icCmmStatIdentityXform)
    return rv;

  // Make sure the output channel and last transform output counts match.
  // Otherwise we'll have a heap overflow during Apply.
  // Check here because CheckPCSConnections can add a transform to the end!
  auto lastXform = GetLastXform();
  if (lastXform) {
    icUInt16Number cmmOutputCount = GetDestSamples();
    icUInt16Number xformOutputCount = lastXform->GetNumDstSamples();
    if (xformOutputCount != cmmOutputCount)
      return icCmmStatBadSpaceLink;
  }

  if (bAllocApplyCmm) {
    m_pApply = GetNewApplyCmm(rv);
  }
  else
    rv = icCmmStatOk;

  return rv;
}


/**
 **************************************************************************
 * Name: CIccCmm::GetNewApplyCmm
 * 
 * Purpose: 
 *  Allocates an CIccApplyCmm object that does the initialization of the Xforms
 *  that provides an Apply() function.
 *  Multiple CIccApplyCmm objects can be allocated and used in separate threads.
 *
 **************************************************************************
 */
CIccApplyCmm *CIccCmm::GetNewApplyCmm(icStatusCMM &status)
{
  CIccApplyCmm *pApply = new (std::nothrow) CIccApplyCmm(this);

  if (!pApply) {
    status = icCmmStatAllocErr;
    return NULL;
  }

  CIccXformList::iterator i;
  CIccApplyXform *pXform;

  for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
    pXform = i->ptr->GetNewApply(status);
    if (!pXform || status != icCmmStatOk) {
      delete pApply;
      return NULL;
    }
    pApply->AppendApplyXform(pXform);
  }

  m_bValid = true;

  status = icCmmStatOk;

  return pApply;
}


/**
**************************************************************************
* Name: CIccCmm::Apply
* 
* Purpose: 
*  Uses the m_pApply object allocated during Begin to Apply the transformations
*  associated with the CMM.
*
**************************************************************************
*/
icStatusCMM CIccCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel)
{
  return m_pApply->Apply(DstPixel, SrcPixel);
}


/**
**************************************************************************
* Name: CIccCmm::Apply
* 
* Purpose: 
*  Uses the m_pApply object allocated during Begin to Apply the transformations
*  associated with the CMM.
*
**************************************************************************
*/
icStatusCMM CIccCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels)
{
  return m_pApply->Apply(DstPixel, SrcPixel, nPixels);
}


/**
**************************************************************************
* Name: CIccCmm::RemoveAllIO()
* 
* Purpose: 
*  Remove any attachments to CIccIO objects associated with the profiles
*  related to the transforms attached to the CMM.
*  Must be called after Begin().
*
*  Return:
*   icCmmStatOK - All IO objects removed
*   icCmmStatBadXform - Begin() has not been performed.
**************************************************************************
*/
icStatusCMM CIccCmm::RemoveAllIO()
{
  if (!Valid())
    return icCmmStatBadXform;

  CIccXformList::iterator i;

  for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
    i->ptr->RemoveIO();
  }

  return icCmmStatOk;
}

/**
 *************************************************************************
 ** Name: CIccCmm::IsInGamut
 **
 ** Purpose:
 **  Function to check if internal representation of gamut is in gamut.  A
 **  colour is in gamut only when the gamut value is zero; every non-zero
 **  value means out of gamut.
 **
 **  Args:
 **   pInternal = internal pixel representation of gamut value
 **
 **  Return:
 **    true if in gamut, false if out of gamut
 **************************************************************************/
bool CIccCmm::IsInGamut(icFloatNumber *pInternal)
{
  // The gamt tag may be lut8Type, lut16Type or lutBToAType, so what reaches us
  // is a normalized float, not an 8-bit code.  Treating anything below 1/255 as
  // in gamut (and the equivalent (unsigned)(v*255.0) truncation it replaced)
  // accepted every 16-bit code below 258; 1/255 is exactly 257/65535.
  //
  // Stored nodes rarely land there - all 7 gamt tables in Testing/ hold 8-bit
  // values replicated x257, so wherever a non-zero node exists the smallest is
  // exactly 257, i.e. 1/255, which the old test judged correctly.  Interpolation
  // is what exposes the threshold: between a zero node and a 257 node every
  // intermediate value falls under the cutoff.  Sweeping 3444 Lab coordinates
  // through CMYK-3DLUTs.icc yields 92 out-of-gamut results the old test called
  // in gamut, the smallest ~7.1e-15.
  //
  // Compare against zero, which is what the tag definition specifies.  NaN and
  // infinity both fail this test and so report out of gamut, the safe verdict.
  return *pInternal == 0.0f;
}


// Decode one already-encoded fixed-integer sample carried in a float into its
// icUIntN value.  Unlike icFtoU8/icFtoU16 this does NOT pre-clamp to [0,1]: the
// argument is a sample in 0..255 / 0..65535, not a normalized float being
// quantized.  Clamps to the encoding range and rounds; NaN maps to 0.
static icUInt8Number icToU8Sample(icFloatNumber v)
{
  if (std::isnan(v) || v <= 0.0f)
    return 0;
  if (v >= 255.0f)
    return 255;
  return (icUInt8Number)icRoundOffset(v);
}

static icUInt16Number icToU16Sample(icFloatNumber v)
{
  if (std::isnan(v) || v <= 0.0f)
    return 0;
  if (v >= 65535.0f)
    return 65535;
  return (icUInt16Number)icRoundOffset(v);
}

/**
 **************************************************************************
 * Name: CIccCmm::ToInternalEncoding
 * 
 * Purpose: 
 *  Functions for converting to Internal representation of pixel colors.
 *  
 * Args:
 *  nSpace = color space signature of the data,
 *  nEncode = icFloatColorEncoding type of the data,
 *  pInternal = converted data is stored here,
 *  pData = the data to be converted
 *  bClip = flag to clip to internal range
 **************************************************************************
 */
icStatusCMM CIccCmm::ToInternalEncoding(icColorSpaceSignature nSpace, icFloatColorEncoding nEncode,
                                        icFloatNumber *pInternal, const icFloatNumber *pData,
                                        bool bClip)
{
  int nSamples = icGetSpaceSamples(nSpace);
  if (!nSamples)
    return icCmmStatBadColorEncoding;


  int i;
  CIccPixelBuf pInput(nSamples);

  if (!pInput.get())
    return icCmmStatAllocErr;

  memcpy(pInput, pData, nSamples*sizeof(icFloatNumber));
  bool bCLRspace = icIsSpaceCLR(nSpace);

// ERROR - case values are not part of the enumerated type!
  switch(icGetColorSpaceType(nSpace))
  {
    case icSigReflectanceSpectralPcsData:
    case icSigTransmissionSpectralPcsData:
    case icSigBiDirReflectanceSpectralPcsData:
    case icSigSparseMatrixSpectralPcsData:
      bCLRspace = true;
      break;
    
    default:
      break;
  }

  switch(nSpace) {

    case icSigLabData:
      {
        switch(nEncode) {
        case icEncodeValue:
          {
            icLabToPcs(pInput);
            break;
          }
        // #2146: icEncodeUnitFloat was absent here while FromInternalEncoding's
        // icSigLabData branch pairs it with icEncodeFloat, so the library wrote
        // Lab data it then refused to read back. Paired identically rather than
        // given a clipping body of its own: the destination side applies no
        // clip on this path, and matching it exactly is what restores the round
        // trip. Lab float already IS the internal PCS encoding, so like
        // icEncodeFloat this converts nothing.
        case icEncodeUnitFloat:
        case icEncodeFloat:
          {
            break;
          }
        case icEncode8Bit:
          {
            pInput[0] = icU8toF(icToU8Sample(pInput[0]))*100.0f;
            pInput[1] = icU8toAB(icToU8Sample(pInput[1]));
            pInput[2] = icU8toAB(icToU8Sample(pInput[2]));

            icLabToPcs(pInput);
            break;
          }
        case icEncode16Bit:
          {
            pInput[0] = icU16toF(icToU16Sample(pInput[0]));
            pInput[1] = icU16toF(icToU16Sample(pInput[1]));
            pInput[2] = icU16toF(icToU16Sample(pInput[2]));
            break;
          }
        case icEncode16BitV2:
          {
            pInput[0] = icU16toF(icToU16Sample(pInput[0]));
            pInput[1] = icU16toF(icToU16Sample(pInput[1]));
            pInput[2] = icU16toF(icToU16Sample(pInput[2]));

            CIccPCSUtil::Lab2ToLab4(pInput, pInput);
            break;
          }
        default:
            return icCmmStatBadColorEncoding;
            break;
        }
        break;
      }

    case icSigXYZData:
      {
        switch(nEncode) {
        case icEncodeValue:
          {
            pInput[0] = (icFloatNumber)pInput[0];
            pInput[1] = (icFloatNumber)pInput[1];
            pInput[2] = (icFloatNumber)pInput[2];
            icXyzToPcs(pInput);
            break;
          }
        case icEncodePercent:
          {
            pInput[0] = (icFloatNumber)(pInput[0] / 100.0);
            pInput[1] = (icFloatNumber)(pInput[1] / 100.0);
            pInput[2] = (icFloatNumber)(pInput[2] / 100.0);
            icXyzToPcs(pInput);
            break;
          }
        // #2146: the icSigLabData counterpart above, for the other PCS. Also
        // deliberately unclipped: icXyzFromPcs scales by 65535/32768, so the
        // external XYZ float range runs to ~2.0 and clipping a source to
        // 0.0-1.0 here would discard legitimate values rather than harden
        // anything.
        case icEncodeUnitFloat:
        case icEncodeFloat:
          {
            icXyzToPcs(pInput);
            break;
          }
          
        case icEncode16Bit:
        case icEncode16BitV2:
          {
            pInput[0] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[0]));
            pInput[1] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[1]));
            pInput[2] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[2]));
            icXyzToPcs(pInput);
            break;
          }
          
        default:
            return icCmmStatBadColorEncoding;
            break;
        }
        break;
      }

    case icSigNamedData:
      return icCmmStatBadColorEncoding;

    default:
      {
        switch(nEncode) {
        case icEncodeValue:
          {
            if (!bCLRspace || nSamples<3) {
              return icCmmStatBadColorEncoding;
            }
            if (nSamples==3)
              icLabToPcs(pInput);
            break;
          }

        case icEncodePercent:
          {
            if (bClip) {
              for(i=0; i<nSamples; i++) {
                pInput[i] = (icFloatNumber)(pInput[i]/100.0);
                if (pInput[i] < 0.0) pInput[i] = 0.0;
                if (pInput[i] > 1.0) pInput[i] = 1.0;
              }
            }
            else {
              for(i=0; i<nSamples; i++) {
                pInput[i] = (icFloatNumber)(pInput[i]/100.0);
              }
            }
            break;
          }
        
        case icEncodeFloat:
        case icEncodeUnitFloat:
          {
            if (bClip) {
              for(i=0; i<nSamples; i++) {
                if (pInput[i] < 0.0) pInput[i] = 0.0;
                if (pInput[i] > 1.0) pInput[i] = 1.0;
              }
            }
            break;
          }
          
        case icEncode8Bit:
          {
            for(i=0; i<nSamples; i++) {
              pInput[i] = icU8toF(icToU8Sample(pInput[i]));
            }
            break;
          }

        case icEncode16Bit:
        case icEncode16BitV2:
          {
            for(i=0; i<nSamples; i++) {
              pInput[i] = icU16toF(icToU16Sample(pInput[i]));
            }
            break;
          }

        default:
            return icCmmStatBadColorEncoding;
            break;
        }
        break;
      }
  }

  memcpy(pInternal, pInput, nSamples*sizeof(icFloatNumber));
  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccCmm::ToInternalEncoding
* 
* Purpose: 
*  Functions for converting to Internal representation of 8 bit pixel colors.
*  
* Args:
*  nSpace = color space signature of the data,
*  nEncode = icFloatColorEncoding type of the data,
*  pInternal = converted data is stored here,
*  pData = the data to be converted
*  bClip = flag to clip to internal range
**************************************************************************
*/
icStatusCMM CIccCmm::ToInternalEncoding(icColorSpaceSignature nSpace, icFloatNumber *pInternal,
                                        const icUInt8Number *pData)
{
  switch(nSpace) {
    case icSigRgbData:
    {
      pInternal[0] = (icFloatNumber)((icFloatNumber)pData[0] / 255.0);
      pInternal[1] = (icFloatNumber)((icFloatNumber)pData[1] / 255.0);
      pInternal[2] = (icFloatNumber)((icFloatNumber)pData[2] / 255.0);

      return icCmmStatOk;
    }
    case icSigCmykData:
    {
      pInternal[0] = (icFloatNumber)((icFloatNumber)pData[0] / 255.0);
      pInternal[1] = (icFloatNumber)((icFloatNumber)pData[1] / 255.0);
      pInternal[2] = (icFloatNumber)((icFloatNumber)pData[2] / 255.0);
      pInternal[3] = (icFloatNumber)((icFloatNumber)pData[3] / 255.0);
      return icCmmStatOk;
    }
    default:
    {
      icUInt32Number i;
      icUInt32Number nSamples = icGetSpaceSamples(nSpace);
      CIccPixelBuf FloatPixel(nSamples);
      if (!FloatPixel.get())
        return icCmmStatAllocErr;

      for(i=0; i<nSamples; i++) {
        FloatPixel[i] = (icFloatNumber)pData[i];    
      }
      return ToInternalEncoding(nSpace, icEncode8Bit, pInternal, FloatPixel);
    }
  }

}


/**
**************************************************************************
* Name: CIccCmm::ToInternalEncoding
* 
* Purpose: 
*  Functions for converting to Internal representation of 16 bit pixel colors.
*  
* Args:
*  nSpace = color space signature of the data,
*  nEncode = icFloatColorEncoding type of the data,
*  pInternal = converted data is stored here,
*  pData = the data to be converted
*  bClip = flag to clip to internal range
**************************************************************************
*/
icStatusCMM CIccCmm::ToInternalEncoding(icColorSpaceSignature nSpace, icFloatNumber *pInternal,
                                        const icUInt16Number *pData)
{
  switch(nSpace) {
    case icSigRgbData:
    {
      pInternal[0] = (icFloatNumber)((icFloatNumber)pData[0] / 65535.0);
      pInternal[1] = (icFloatNumber)((icFloatNumber)pData[1] / 65535.0);
      pInternal[2] = (icFloatNumber)((icFloatNumber)pData[2] / 65535.0);

      return icCmmStatOk;
    }
    case icSigCmykData:
    {
      pInternal[0] = (icFloatNumber)((icFloatNumber)pData[0] / 65535.0);
      pInternal[1] = (icFloatNumber)((icFloatNumber)pData[1] / 65535.0);
      pInternal[2] = (icFloatNumber)((icFloatNumber)pData[2] / 65535.0);
      pInternal[3] = (icFloatNumber)((icFloatNumber)pData[3] / 65535.0);
      return icCmmStatOk;
    }
    default:
    {
      icUInt32Number i;
      icUInt32Number nSamples = icGetSpaceSamples(nSpace);
      CIccPixelBuf pFloatPixel(nSamples);
      if (!pFloatPixel.get())
        return icCmmStatAllocErr;

      for(i=0; i<nSamples; i++) {
        pFloatPixel[i] = (icFloatNumber)pData[i];    
      }
      return ToInternalEncoding(nSpace, icEncode16Bit, pInternal, pFloatPixel);
    }
  }
}


/**
 **************************************************************************
 * Name: CIccCmm::FromInternalEncoding
 * 
 * Purpose: 
 *  Functions for converting from Internal representation of pixel colors.
 *  
 * Args:
 *  nSpace = color space signature of the data,
 *  nEncode = icFloatColorEncoding type of the data,
 *  pData = converted data is stored here,
 *  pInternal = the data to be converted
 *  bClip = flag to clip data to internal range
 **************************************************************************
 */
icStatusCMM CIccCmm::FromInternalEncoding(icColorSpaceSignature nSpace, icFloatColorEncoding nEncode,
                                          icFloatNumber *pData, const icFloatNumber *pInternal, bool bClip)
{
  int nSamples = icGetSpaceSamples(nSpace);
  if (!nSamples)
    return icCmmStatBadColorEncoding;

  int i;
  CIccPixelBuf pInput(nSamples);

  if (!pInput.get())
    return icCmmStatAllocErr;

  memcpy(pInput, pInternal, nSamples*sizeof(icFloatNumber));
  bool bCLRspace = (icIsSpaceCLR(nSpace) || (nSpace == icSigDevLabData) || (nSpace==icSigDevXYZData));

  switch(nSpace) {

    case icSigLabData:
      {
        switch(nEncode) {
        case icEncodeValue:
          {
            icLabFromPcs(pInput);
            break;
          }
        case icEncodeUnitFloat:
        case icEncodeFloat:
          {
            break;
          }
        case icEncode8Bit:
          {
            icLabFromPcs(pInput);

            // icABtoU8() clamps the a*/b* channels (NaN/<0/>255) before their
            // cast; the L* channel is scaled by hand here, so apply the same
            // guard. Without it an out-of-range or NaN L* (e.g. produced by a
            // malformed transform) yields an out-of-range float->uint8
            // conversion, which is undefined behavior.
            icFloatNumber l8 = pInput[0] / 100.0f * 255.0f + 0.5f;
            if (std::isnan(pInput[0]))
              l8 = 0.0f;
            else if (l8 < 0.0f)
              l8 = 0.0f;
            else if (l8 > 255.0f)
              l8 = 255.0f;
            pInput[0] = (icUInt8Number)l8;
            pInput[1] = icABtoU8(pInput[1]);
            pInput[2] = icABtoU8(pInput[2]);
            break;
          }
        case icEncode16Bit:
          {
            pInput[0] = icFtoU16(pInput[0]);
            pInput[1] = icFtoU16(pInput[1]);
            pInput[2] = icFtoU16(pInput[2]);
            break;
          }
        case icEncode16BitV2:
          {
            CIccPCSUtil::Lab4ToLab2(pInput, pInput);

            pInput[0] = icFtoU16(pInput[0]);
            pInput[1] = icFtoU16(pInput[1]);
            pInput[2] = icFtoU16(pInput[2]);
            break;
          }
        default:
            return icCmmStatBadColorEncoding;
            break;
        }
        break;
      }

    case icSigXYZData:
    {
        switch(nEncode) {
        case icEncodeValue:
          {
            icXyzFromPcs(pInput);
            break;
          }
        case icEncodePercent:
          {
            icXyzFromPcs(pInput);
            pInput[0] = (icFloatNumber)(pInput[0] * 100.0);
            pInput[1] = (icFloatNumber)(pInput[1] * 100.0);
            pInput[2] = (icFloatNumber)(pInput[2] * 100.0);            
            break;
          }
        case icEncodeFloat:
        case icEncodeUnitFloat:
          {
            icXyzFromPcs(pInput);
            break;
          }
          
        case icEncode16Bit:
        case icEncode16BitV2:
          {
            icXyzFromPcs(pInput);
            pInput[0] = icDtoUSF(pInput[0]);
            pInput[1] = icDtoUSF(pInput[1]);
            pInput[2] = icDtoUSF(pInput[2]);
            break;
          }
          
        default:
            return icCmmStatBadColorEncoding;
            break;
        }
        break;
      }

    case icSigNamedData:
      return icCmmStatBadColorEncoding;

    default:
      {
        switch(nEncode) {
        case icEncodeValue:
          {
            if (nSpace == icSigDevXYZData) {
              icXyzFromPcs(pInput);
            }
            else if (bCLRspace && nSamples >=3) {
              icLabFromPcs(pInput);
            }
            break;
          }
        case icEncodePercent:
          {
            if (bClip) {
              for(i=0; i<nSamples; i++) {
                if (pInput[i] < 0.0) pInput[i] = 0.0;
                if (pInput[i] > 1.0) pInput[i] = 1.0;
                pInput[i] = (icFloatNumber)(pInput[i]*100.0);
              }
            }
            else {
              for(i=0; i<nSamples; i++) {
                pInput[i] = (icFloatNumber)(pInput[i]*100.0);
              }
            }
            break;
          }
        
        case icEncodeFloat:
          break;

        case icEncodeUnitFloat:
          {
            if (bClip) {
              for(i=0; i<nSamples; i++) {
                if (pInput[i] < 0.0) pInput[i] = 0.0;
                if (pInput[i] > 1.0) pInput[i] = 1.0;
              }
            }
            break;
          }
          
        case icEncode8Bit:
          {
            for(i=0; i<nSamples; i++) {
              pInput[i] = icFtoU8(pInput[i]);
            }
            break;
          }
          
        case icEncode16Bit:
        case icEncode16BitV2:
          {
            for(i=0; i<nSamples; i++) {
              pInput[i] = icFtoU16(pInput[i]);
            }
            break;
          }
        
        default:
            return icCmmStatBadColorEncoding;
            break;
        }
        break;
      }
  }

  memcpy(pData, pInput, nSamples*sizeof(icFloatNumber));
  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccCmm::FromInternalEncoding
* 
* Purpose: 
*  Functions for converting from Internal representation of 8 bit pixel colors.
*  
* Args:
*  nSpace = color space signature of the data,
*  nEncode = icFloatColorEncoding type of the data,
*  pData = converted data is stored here,
*  pInternal = the data to be converted
*  bClip = flag to clip data to internal range
**************************************************************************
*/
icStatusCMM CIccCmm::FromInternalEncoding(icColorSpaceSignature nSpace, icUInt8Number *pData,
                                          const icFloatNumber *pInternal)
{
  switch(nSpace) {
    case icSigRgbData:
    {
      pData[0] = icFtoU8(pInternal[0]);
      pData[1] = icFtoU8(pInternal[1]);
      pData[2] = icFtoU8(pInternal[2]);

      return icCmmStatOk;
    }
    case icSigCmykData:
    {
      pData[0] = icFtoU8(pInternal[0]);
      pData[1] = icFtoU8(pInternal[1]);
      pData[2] = icFtoU8(pInternal[2]);
      pData[3] = icFtoU8(pInternal[3]);

      return icCmmStatOk;
    }
    default:
    {
      icUInt32Number i;
      icUInt32Number nSamples = icGetSpaceSamples(nSpace);

      CIccPixelBuf pFloatPixel(nSamples);
      icStatusCMM convertStat;

      if (!pFloatPixel.get())
        return icCmmStatAllocErr;

      convertStat = FromInternalEncoding(nSpace, icEncode8Bit, pFloatPixel, pInternal);
      if (convertStat)
        return convertStat;
      for(i=0; i<nSamples; i++) {
        pData[i] = (icUInt8Number)(pFloatPixel[i] + 0.5);    
      }

      return icCmmStatOk;
    }
  }
}


/**
**************************************************************************
* Name: CIccCmm::FromInternalEncoding
* 
* Purpose: 
*  Functions for converting from Internal representation of 16 bit pixel colors.
*  
* Args:
*  nSpace = color space signature of the data,
*  nEncode = icFloatColorEncoding type of the data,
*  pData = converted data is stored here,
*  pInternal = the data to be converted
*  bClip = flag to clip data to internal range
**************************************************************************
*/
icStatusCMM CIccCmm::FromInternalEncoding(icColorSpaceSignature nSpace, icUInt16Number *pData,
                                          const icFloatNumber *pInternal)
{
  switch(nSpace) {
    case icSigRgbData:
    {
      pData[0] = icFtoU16(pInternal[0]);
      pData[1] = icFtoU16(pInternal[1]);
      pData[2] = icFtoU16(pInternal[2]);

      return icCmmStatOk;
    }
    case icSigCmykData:
    {
      pData[0] = icFtoU16(pInternal[0]);
      pData[1] = icFtoU16(pInternal[1]);
      pData[2] = icFtoU16(pInternal[2]);
      pData[3] = icFtoU16(pInternal[3]);

      return icCmmStatOk;
    }
    default:
    {
      icUInt32Number i;
      icUInt32Number nSamples = icGetSpaceSamples(nSpace);
      CIccPixelBuf pFloatPixel(nSamples);
      icStatusCMM convertStat;

      if (!pFloatPixel.get())
        return icCmmStatAllocErr;

      convertStat = FromInternalEncoding(nSpace, icEncode16Bit, pFloatPixel, pInternal);
      if (convertStat)
        return convertStat;
      for(i=0; i<nSamples; i++) {
        pData[i] = (icUInt16Number)(pFloatPixel[i] + 0.5);    
      }

      return icCmmStatOk;
    }
  }
}


/**
 **************************************************************************
 * Name: CIccCmm::GetFloatColorEncoding
 * 
 * Purpose: 
 *  Converts the encoding type to characters for printing
 *  
 * Args:
 *  val = encoding type
 * 
 * Return:
 *  characters for printing
 **************************************************************************
 */
const icChar* CIccCmm::GetFloatColorEncoding(icFloatColorEncoding val)
{
  switch(val) {

    case icEncodeValue:
      return "icEncodeValue";

    case icEncodeFloat:
      return "icEncodeFloat";

    case icEncodeUnitFloat:
      return "icEncodeUnitFloat";

    case icEncodePercent:
      return "icEncodePercent";

    case icEncode8Bit:
      return "icEncode8Bit";

    case icEncode16Bit:
      return "icEncode16Bit";

    case icEncode16BitV2:
      return "icEncode16BitV2";

    default:
      return "icEncodeUnknown";
  }
}

/**
 **************************************************************************
 * Name: CIccCmm::GetFloatColorEncoding
 * 
 * Purpose: 
 *  Converts the string containing encoding type to icFloatColorEncoding
 *  
 * Args:
 *  val = string containing encoding type
 * 
 * Return:
 *  encoding type
 **************************************************************************
 */
icFloatColorEncoding CIccCmm::GetFloatColorEncoding(const icChar* val)
{
  if (!stricmp(val, "icEncodePercent")) {
    return icEncodePercent;
  }
  else if (!stricmp(val, "icEncodeUnitFloat")) {
    return icEncodeUnitFloat;
  }
  else if (!stricmp(val, "icEncodeFloat")) {
    return icEncodeFloat;
  }
  else if (!stricmp(val, "icEncode8Bit")) {
    return icEncode8Bit;
  }
  else if (!stricmp(val, "icEncode16Bit")) {
    return icEncode16Bit;
  }
  else if (!stricmp(val, "icEncode16BitV2")) {
    return icEncode16BitV2;
  }
  else if (!stricmp(val, "icEncodeValue")) {
    return icEncodeValue;
  }
  else {
    return icEncodeUnknown;
  }

}

/**
 **************************************************************************
 * Name: CIccCmm::GetNumXforms
 * 
 * Purpose: 
 *  Get number of xforms in the xform list
 *  
 * Return:
 * number of m_Xforms
 **************************************************************************
 */
icUInt32Number CIccCmm::GetNumXforms() const
{
  return (icUInt32Number)m_Xforms->size();
}


/**
 **************************************************************************
 * Name: CIccCmm::HasXformsOfType
 *
 * Purpose:
 * Check to see if one of the xforms has a given type
 *
 * Return:
 * true if one of the xforme is of a given type
 **************************************************************************
 */
bool CIccCmm::HasXformsOfType(icXformType nXformType) const
{
  CIccXformList::iterator xform;
  for (xform = m_Xforms->begin(); xform != m_Xforms->end(); xform++) {
    if (xform->ptr->GetXformType() == nXformType) {
      return true;
    }
  }
  return false;
}


/**
 **************************************************************************
 * Name: CIccCmm::GetFirstXform
 *
 * Purpose:
 *  Get first xform in the xform list
 *
 * Return:
 * firs xform or null if no xforms
 **************************************************************************
 */
CIccXform* CIccCmm::GetFirstXform() const
{
  if (!m_Xforms->size())
    return nullptr;

  return m_Xforms->front().ptr;
}


/**
 **************************************************************************
 * Name: CIccCmm::GetLastXform
 *
 * Purpose:
 *  Get last xform in the xform list
 *
 * Return:
 * firs xform or null if no xforms
 **************************************************************************
 */
CIccXform* CIccCmm::GetLastXform() const
{
  if (!m_Xforms->size())
    return nullptr;

  return m_Xforms->back().ptr;
}



/**
 **************************************************************************
 * Name: CIccCmm::IterateXforms
 *
 * Purpose:
 *  Get information from xform in the list
 *
 **************************************************************************
 */
void CIccCmm::IterateXforms( IXformIterator* pIterater) const
{
  for (auto x = m_Xforms->begin(); x != m_Xforms->end(); x++) {
    pIterater->iterate(x->ptr);
  }
}


/**
**************************************************************************
* Name: CIccCmm::GetFirstXformSource
* 
* Purpose: 
*  Get source colorspace of first transform (similar to m_nSrcSpace with differences in dev colorimetric spaces)
*  
* Return:
* colorspace
**************************************************************************
*/
icColorSpaceSignature CIccCmm::GetFirstXformSource()
{
  if (!m_Xforms->size())
    return m_nSrcSpace;

  return m_Xforms->begin()->ptr->GetSrcSpace();
}

/**
**************************************************************************
* Name: CIccCmm::GetNumXforms
* 
* Purpose: 
*  Get source colorspace of last transform (similar to m_nSrcSpace with differences in dev colorimetric spaces)
*  
* Return:
* colorspace
**************************************************************************
*/
icColorSpaceSignature CIccCmm::GetLastXformDest()
{
  if (!m_Xforms->size())
    return m_nDestSpace;

  return m_Xforms->rbegin()->ptr->GetDstSpace();
}

/**
**************************************************************************
* Name: CIccApplyCmm::CIccApplyCmm
* 
* Purpose: 
*  Constructor
*
* Args:
*  pCmm = ptr to CMM to apply against
**************************************************************************
*/
CIccApplyNamedColorCmm::CIccApplyNamedColorCmm(CIccNamedColorCmm *pCmm) : CIccApplyCmm(pCmm)
{
}


/**
**************************************************************************
* Name: CIccApplyCmm::CIccApplyCmm
* 
* Purpose: 
*  Destructor
**************************************************************************
*/
CIccApplyNamedColorCmm::~CIccApplyNamedColorCmm()
{
}


/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
icStatusCMM CIccApplyNamedColorCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel)
{
  icFloatNumber *pDst, *pTmp;
  const icFloatNumber *pSrc;
  CIccApplyXformList::iterator i;
  int j, n = (int)m_Xforms->size();
  CIccApplyXform *pApply;
  const CIccXform *pApplyXform;
  CIccXformNamedColor *pXform;

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel()) {
    return icCmmStatAllocErr;
  }

  icChar NamedColor[256];
  icStatusCMM rv;

  pSrc = SrcPixel;
  pDst = m_Pixel;

#ifdef DEBUG_CMM_APPLY
  int nCount = 0;
  printf("Start ApplyNamedCmm\n");
  DumpCmmApplyPixel(nCount++, pSrc, icGetSpaceSamples(m_pCmm->GetSourceSpace()));
#endif


  if (n>1) {
    for (j=0, i=m_Xforms->begin(); j<n-1 && i!=m_Xforms->end(); i++, j++) {

      pApply = i->ptr;
      pApplyXform = pApply->GetXform();
      if (!pApplyXform) return icCmmStatIncorrectApply;
      if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
        pXform = (CIccXformNamedColor*)pApplyXform;

        switch(pXform->GetInterface()) {
        case icApplyPixel2Pixel:
          pXform->Apply(pApply, pDst, pSrc);
#ifdef DEBUG_CMM_APPLY
          DumpCmmApplyPixel(nCount++, pDst, pXform->GetNumDstSamples());
#endif
          break;

        case icApplyPixel2Named:
          rv = pXform->Apply(pApply, NamedColor, pSrc);
          if (rv) {
            return rv;
          }
#ifdef DEBUG_CMM_APPLY
          printf("Xfm%d: \"%s\"\n", nCount++, NamedColor);
#endif
          break;

        case icApplyNamed2Pixel:
          if (j==0) {
            return icCmmStatIncorrectApply;
          }

          rv = pXform->Apply(pApply, pDst, NamedColor);
#ifdef DEBUG_CMM_APPLY
          DumpCmmApplyPixel(nCount++, pDst, pXform->GetNumDstSamples());
#endif

          if (rv) {
            return rv;
          }
          break;

        default:
          break;
        }
      }
      else {
        pApplyXform->Apply(pApply, pDst, pSrc);
#ifdef DEBUG_CMM_APPLY
        DumpCmmApplyPixel(nCount++, pDst, pApplyXform->GetNumDstSamples());
#endif
      }
      pTmp = (icFloatNumber*)pSrc;
      pSrc = pDst;
      if (pTmp==SrcPixel)
        pDst = m_Pixel2;
      else
        pDst = pTmp;
    }

    pApply = i->ptr;
    pApplyXform = pApply->GetXform();
    if (!pApplyXform) return icCmmStatIncorrectApply;
    if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
      pXform = (CIccXformNamedColor*)pApplyXform;

      switch(pXform->GetInterface()) {
      case icApplyPixel2Pixel:
        pXform->Apply(pApply, DstPixel, pSrc);
#ifdef DEBUG_CMM_APPLY
        DumpCmmApplyPixel(nCount++, DstPixel, pXform->GetNumDstSamples());
#endif
        break;

      case icApplyPixel2Named:
      default:
        return icCmmStatIncorrectApply;
        break;

      case icApplyNamed2Pixel:
        rv = pXform->Apply(pApply, DstPixel, NamedColor);
#ifdef DEBUG_CMM_APPLY
        DumpCmmApplyPixel(nCount++, DstPixel, pXform->GetNumDstSamples());
#endif
        if (rv) {
          return rv;
        }
        break;

      }
    }
    else {
      pApplyXform->Apply(pApply, DstPixel, pSrc);
#ifdef DEBUG_CMM_APPLY
      DumpCmmApplyPixel(nCount++, DstPixel, pApplyXform->GetNumDstSamples());
#endif
    }

  }
  else if (n==1) {
    i = m_Xforms->begin();

    pApply = i->ptr;
    pApplyXform = pApply->GetXform();
    if (!pApplyXform) return icCmmStatIncorrectApply;
    if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
      return icCmmStatIncorrectApply;
    }

    pApplyXform->Apply(pApply, DstPixel, pSrc);
#ifdef DEBUG_CMM_APPLY
    DumpCmmApplyPixel(nCount++, DstPixel, pApplyXform->GetNumDstSamples());
#endif
  }

#ifdef DEBUG_CMM_APPLY
  DumpCmmApplyPixel(nCount++, DstPixel, icGetSpaceSamples(m_pCmm->GetDestSpace()));
  printf("End ApplyNamedCmm\n\n");
#endif

  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstPixel = Destination pixel where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
icStatusCMM CIccApplyNamedColorCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels)
{
  icFloatNumber *pDst;
  const icFloatNumber *pSrc;
  CIccApplyXformList::iterator i;
  int j, n = (int)m_Xforms->size();
  CIccApplyXform *pApply;
  const CIccXform *pApplyXform;
  CIccXformNamedColor *pXform;
  icUInt32Number k; 

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel()) {
    return icCmmStatAllocErr;
  }

  icChar NamedColor[255];
  icStatusCMM rv;

  for (k=0; k<nPixels; k++) {

    pSrc = SrcPixel;
    pDst = m_Pixel;

    if (n>1) {
      for (j=0, i=m_Xforms->begin(); j<n-1 && i!=m_Xforms->end(); i++, j++) {

        pApply = i->ptr;
        pApplyXform = pApply->GetXform();
        if (!pApplyXform) return icCmmStatIncorrectApply;
        if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
          pXform = (CIccXformNamedColor*)pApplyXform;

          switch(pXform->GetInterface()) {
          case icApplyPixel2Pixel:
            pXform->Apply(pApply, pDst, pSrc);
            break;

          case icApplyPixel2Named:
            rv = pXform->Apply(pApply, NamedColor, pSrc);
            if (rv) {
              return rv;
            }
            break;

          case icApplyNamed2Pixel:
            if (j==0) {
              return icCmmStatIncorrectApply;
            }

            rv = pXform->Apply(pApply, pDst, NamedColor);

            if (rv) {
              return rv;
            }
            break;

          default:
            break;
          }
        }
        else {
          pApplyXform->Apply(pApply, pDst, pSrc);
        }
        pSrc = pDst;
      }

      pApply = i->ptr;
      pApplyXform = pApply->GetXform();
      if (!pApplyXform) return icCmmStatIncorrectApply;
      if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
        pXform = (CIccXformNamedColor*)pApplyXform;

        switch(pXform->GetInterface()) {
        case icApplyPixel2Pixel:
          pXform->Apply(pApply, DstPixel, pSrc);
          break;

        case icApplyPixel2Named:
        default:
          return icCmmStatIncorrectApply;
          break;

        case icApplyNamed2Pixel:
          rv = pXform->Apply(pApply, DstPixel, NamedColor);
          if (rv) {
            return rv;
          }
          break;

        }
      }
      else {
        pApplyXform->Apply(pApply, DstPixel, pSrc);
      }

    }
    else if (n==1) {
      i = m_Xforms->begin();

      pApply = i->ptr;
      pApplyXform = pApply->GetXform();
      if (!pApplyXform) return icCmmStatIncorrectApply;
      if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
        return icCmmStatIncorrectApply;
      }

      pApplyXform->Apply(pApply, DstPixel, pSrc);
    }

    SrcPixel += m_pCmm->GetSourceSamples();
    DstPixel += m_pCmm->GetDestSamples();
  }

  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstColorName = Destination string where the result is stored,
*  SrcPixel = Source pixel which is to be applied.
**************************************************************************
*/
icStatusCMM CIccApplyNamedColorCmm::Apply(icChar* DstColorName, const icFloatNumber *SrcPixel)
{
  icFloatNumber *pDst, *pTmp;
  const icFloatNumber *pSrc;
  CIccApplyXformList::iterator i;
  int j, n = (int)m_Xforms->size();
  CIccApplyXform *pApply;
  const CIccXform *pApplyXform;
  CIccXformNamedColor *pXform;

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel()) {
    return icCmmStatAllocErr;
  }

  icChar NamedColor[256];
  icStatusCMM rv;

  pSrc = SrcPixel;
  pDst = m_Pixel;

  if (n>1) {
    for (j=0, i=m_Xforms->begin(); j<n-1 && i!=m_Xforms->end(); i++, j++) {

      pApply = i->ptr;
      pApplyXform = pApply->GetXform();
      if (!pApplyXform) return icCmmStatIncorrectApply;
      if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
        pXform = (CIccXformNamedColor*)pApplyXform;
        switch(pXform->GetInterface()) {
        case icApplyPixel2Pixel:
          pXform->Apply(pApply, pDst, pSrc);
          break;

        case icApplyPixel2Named:
          rv = pXform->Apply(pApply, NamedColor, pSrc);
          if (rv) {
            return rv;
          }
          break;

        case icApplyNamed2Pixel:
          if (j==0) {
            return icCmmStatIncorrectApply;
          }
          rv = pXform->Apply(pApply, pDst, NamedColor);
          if (rv) {
            return rv;
          }
          break;

        default:
          break;
        }
      }
      else {
        pApplyXform->Apply(pApply, pDst, pSrc);
      }
      pTmp = (icFloatNumber*)pSrc;
      pSrc = pDst;
      if (pTmp==SrcPixel)
        pDst = m_Pixel2;
      else
        pDst = pTmp;
    }

    pApply = i->ptr;
    pApplyXform = pApply->GetXform();
    if (!pApplyXform) return icCmmStatIncorrectApply;
    if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
      pXform = (CIccXformNamedColor*)pApplyXform;
      switch(pXform->GetInterface()) {

      case icApplyPixel2Named:
        rv = pXform->Apply(pApply, DstColorName, pSrc);
        if (rv) {
          return rv;
        }
        break;

      case icApplyPixel2Pixel:
      case icApplyNamed2Pixel:
      default:
        return icCmmStatIncorrectApply;
        break;
      }
    }
    else {
      return icCmmStatIncorrectApply;
    }

  }
  else if (n==1) {
    i = m_Xforms->begin();
    pApply = i->ptr;
    pApplyXform = pApply->GetXform();
    if (!pApplyXform) return icCmmStatIncorrectApply;
    if (pApplyXform->GetXformType()!=icXformTypeNamedColor) {
      return icCmmStatIncorrectApply;
    }

    pXform = (CIccXformNamedColor*)pApplyXform;
    rv = pXform->Apply(pApply, DstColorName, pSrc);
    if (rv) {
      return rv;
    }
  }

  return icCmmStatOk;
}


/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstPixel = Destination pixel where the result is stored,
*  SrcColorName = Source color name which is to be searched.
**************************************************************************
*/
icStatusCMM CIccApplyNamedColorCmm::Apply(icFloatNumber *DstPixel, const icChar *SrcColorName, icFloatNumber tint/*=1.0*/)
{
  icFloatNumber *pDst, *pTmp;
  const icFloatNumber *pSrc;
  CIccApplyXformList::iterator i;
  int j, n = (int)m_Xforms->size();
  CIccApplyXform *pApply;
  const CIccXform *pApplyXform;
  CIccXformNamedColor *pXform;

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel()) {
    return icCmmStatAllocErr;
  }

  icChar NamedColor[255];
  icStatusCMM rv;

  i=m_Xforms->begin();
  pApply = i->ptr;
  pApplyXform = pApply->GetXform();
  if (!pApplyXform) return icCmmStatIncorrectApply;
  if (pApplyXform->GetXformType()!=icXformTypeNamedColor)
    return icCmmStatIncorrectApply;

  pXform = (CIccXformNamedColor*)pApplyXform;  

  pDst = m_Pixel;

  if (n>1) {
    rv = pXform->Apply(pApply, pDst, SrcColorName, tint);
    if (rv) {
      return rv;
    }

    pSrc = pDst;
    pDst = m_Pixel2;

    for (j=0, i++; j<n-2 && i!=m_Xforms->end(); i++, j++) {

      pApply = i->ptr;
      pApplyXform = pApply->GetXform();
      if (!pApplyXform) return icCmmStatIncorrectApply;
      if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
        CIccXformNamedColor *pXformLocal = (CIccXformNamedColor*)pApplyXform;
        switch(pXformLocal->GetInterface()) {
        case icApplyPixel2Pixel:
          pXformLocal->Apply(pApply, pDst, pSrc);
          break;

        case icApplyPixel2Named:
          pXformLocal->Apply(pApply, NamedColor, pSrc);
          break;

        case icApplyNamed2Pixel:
          rv = pXformLocal->Apply(pApply, pDst, NamedColor);
          if (rv) {
            return rv;
          }
          break;

        default:
          break;
        }
      }
      else {
        pApplyXform->Apply(pApply, pDst, pSrc);
      }
      pTmp = (icFloatNumber*)pSrc;
      pSrc = pDst;
      pDst = pTmp;
    }

    pApply = i->ptr;
    pApplyXform = pApply->GetXform();
    if (!pApplyXform) return icCmmStatIncorrectApply;
    if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
      pXform = (CIccXformNamedColor*)pApplyXform;
      switch(pXform->GetInterface()) {
      case icApplyPixel2Pixel:
        pXform->Apply(pApply, DstPixel, pSrc);
        break;

      case icApplyPixel2Named:
      default:
        return icCmmStatIncorrectApply;
        break;

      case icApplyNamed2Pixel:
        rv = pXform->Apply(pApply, DstPixel, NamedColor);
        if (rv) {
          return rv;
        }
        break;

      }
    }
    else {
      pApplyXform->Apply(pApply, DstPixel, pSrc);
    }

  }
  else if (n==1) {
    rv = pXform->Apply(pApply, DstPixel, SrcColorName, tint);
    if (rv) {
      return rv;
    }
  }


  return icCmmStatOk;
}

/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstColorName = Destination string where the result is stored, 
*  SrcColorName = Source color name which is to be searched.
**************************************************************************
*/
icStatusCMM CIccApplyNamedColorCmm::Apply(icChar *DstColorName, const icChar *SrcColorName, icFloatNumber tint/*=1.0*/)
{
  icFloatNumber *pDst, *pTmp;
  const icFloatNumber *pSrc;
  CIccApplyXformList::iterator i;
  int j, n = (int)m_Xforms->size();
  icChar NamedColor[256];
  icStatusCMM rv;
  CIccApplyXform *pApply;
  const CIccXform *pApplyXform;
  CIccXformNamedColor *pXform;

  if (!n)
    return icCmmStatBadXform;

  if (!m_Pixel && !InitPixel()) {
    return icCmmStatAllocErr;
  }

  i=m_Xforms->begin();

  pApply = i->ptr;
  pApplyXform = pApply->GetXform();
  if (!pApplyXform) return icCmmStatIncorrectApply;
  if (pApplyXform->GetXformType()!=icXformTypeNamedColor)
    return icCmmStatIncorrectApply;

  pXform = (CIccXformNamedColor*)pApplyXform;

  pDst = m_Pixel;

  if (n>1) {
    rv = pXform->Apply(pApply, pDst, SrcColorName, tint);

    if (rv) {
      return rv;
    }

    pSrc = pDst;
    pDst = m_Pixel2;

    for (j=0, i++; j<n-2 && i!=m_Xforms->end(); i++, j++) {

      pApply = i->ptr;
      pApplyXform = pApply->GetXform();
      if (!pApplyXform) return icCmmStatIncorrectApply;
      if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
        pXform = (CIccXformNamedColor*)pApplyXform;
        switch(pXform->GetInterface()) {
        case icApplyPixel2Pixel:
          pXform->Apply(pApply, pDst, pSrc);
          break;


        case icApplyPixel2Named:
          rv = pXform->Apply(pApply, NamedColor, pSrc);
          if (rv) {
            return rv;
          }
          break;

        case icApplyNamed2Pixel:
          rv = pXform->Apply(pApply, pDst, NamedColor);
          if (rv) {
            return rv;
          }
          break;

        default:
          break;
        }
      }
      else {
        pApplyXform->Apply(pApply, pDst, pSrc);
      }
      pTmp = (icFloatNumber*)pSrc;
      pSrc = pDst;
      pDst = pTmp;
    }

    pApply = i->ptr;
    pApplyXform = pApply->GetXform();
    if (!pApplyXform) return icCmmStatIncorrectApply;
    if (pApplyXform->GetXformType()==icXformTypeNamedColor) {
      pXform = (CIccXformNamedColor*)pApplyXform;
      switch(pXform->GetInterface()) {
      case icApplyPixel2Named:
        rv = pXform->Apply(pApply, DstColorName, pSrc);
        if (rv) {
          return rv;
        }
        break;

      case icApplyPixel2Pixel:
      case icApplyNamed2Pixel:
      default:
        return icCmmStatIncorrectApply;
        break;
      }
    }
    else {
      return icCmmStatIncorrectApply;
    }

  }
  else if (n==1) {
    return icCmmStatIncorrectApply;
  }

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccNamedColorCmm::CIccNamedColorCmm
 * 
 * Purpose: 
 *  Constructor
 *
 * Args:
 *  nSrcSpace = signature of the source color space,
 *  nDestSpace = signature of the destination color space,
 *  bFirstInput = true if the first profile added is an input profile
 **************************************************************************
 */
CIccNamedColorCmm::CIccNamedColorCmm(icColorSpaceSignature nSrcSpace, icColorSpaceSignature nDestSpace,
                                     bool bFirstInput) : CIccCmm(nSrcSpace, nDestSpace, bFirstInput)
{
  // No file access occurs here; CodeQL traces intentional CLI profile paths through this constructor.
  // codeql[cpp/path-injection]
  m_nApplyInterface = icApplyPixel2Pixel;
}

/**
 **************************************************************************
 * Name: CIccNamedColorCmm::~CIccNamedColorCmm
 * 
 * Purpose: 
 *  Destructor
 **************************************************************************
 */
CIccNamedColorCmm::~CIccNamedColorCmm()
{
}


/**
 **************************************************************************
 * Name: CIccNamedColorCmm::AddXform
 * 
 * Purpose: 
 *  Adds a profile at the end of the Xform list 
 * 
 * Args: 
 *  szProfilePath = file name of the profile to be added,
 *  nIntent = rendering intent to be used with the profile,
 *  nInterp = type of interpolation to be used with the profile
 *  pHintManager = hints for creating the xform
 * 
 * Return: 
 *  icCmmStatOk, if the profile was added to the list succesfully
 **************************************************************************
 */
icStatusCMM CIccNamedColorCmm::AddXform(const icChar *szProfilePath,
                                        icRenderingIntent nIntent /*=icUnknownIntent*/,
                                        icXformInterp nInterp /*icXformInterp*/,
                                        IIccProfileConnectionConditions *pPcc/*=NULL*/,
                                        icXformLutType nLutType /*=icXformLutColor*/,
                                        bool bUseD2BxB2DxTags /*=true*/,
                                        CIccCreateXformHintManager *pHintManager /*=NULL*/,
                                        bool bUseSubProfile /*=false*/)
{
  CIccProfile *pProfile = OpenIccProfile(szProfilePath, bUseSubProfile);

  if (!pProfile) 
    return icCmmStatCantOpenProfile;

  icStatusCMM rv = AddXform(pProfile, nIntent, nInterp, pPcc, nLutType, bUseD2BxB2DxTags, pHintManager);
  // AddXform took ownership of the profile pointer, or deleted it if there was an error

  return rv;
}

/**
 **************************************************************************
 * Name: CIccNamedColorCmm::AddXform
 * 
 * Purpose: 
 *  Adds a profile at the end of the Xform list 
 * 
 * Args: 
 *  pProfile = pointer to the CIccProfile object to be added,
 *      AddXform takes ownership of the profile, or deletes the profile on error.
 *  nIntent = rendering intent to be used with the profile,
 *  nInterp = type of interpolation to be used with the profile
 *  nLutType = type of lut to use from the profile
 *  pHintManager = hints for creating the xform
 * 
 * Return: 
 *  icCmmStatOk, if the profile was added to the list succesfully
 **************************************************************************
 */
icStatusCMM CIccNamedColorCmm::AddXform(CIccProfile *pProfile,
                                        icRenderingIntent nIntent /*=icUnknownIntent*/,
                                        icXformInterp nInterp /*=icInterpLinear*/,
                                        IIccProfileConnectionConditions *pPcc/*=NULL*/,
                                        icXformLutType nLutType /*=icXformLutColor*/,
                                        bool bUseD2BxB2DxTags /*=true*/,
                                        CIccCreateXformHintManager *pHintManager /*=NULL*/)
{
  icColorSpaceSignature nSrcSpace, nDstSpace;
  CIccXformPtr Xform;
  bool bInput = !m_bLastInput;
  icStatusCMM rv;
  icXformLutType nUseLutType = nLutType;

  switch(pProfile->m_Header.deviceClass) {
    case icSigMultiplexIdentificationClass:
    case icSigMultiplexLinkClass:
      nIntent = icPerceptual;
      nLutType = icXformLutMCS;
      break;

    case icSigMultiplexVisualizationClass:
      nLutType = icXformLutMCS;
      break;

    default:
      break;
  }

  // namedColorimetric / namedSpectral / namedDevice pin which v5 named
  // array member the name->space apply path reads (nmclPcsDataMbr /
  // spectral / nmclDeviceDataMbr respectively); plain icXformLutNamedColor
  // leaves the colorimetric-vs-spectral choice to the bUseD2BxB2Dx /
  // spectralPCS-presence heuristic below.  Outside the named-color
  // branch the colorimetric/spectral variants collapse to their non-
  // named counterparts; namedDevice collapses to icXformLutColor (auto).
  const bool bExplicitNamed   = (nLutType == icXformLutNamedColor ||
                                 nLutType == icXformLutNamedColorimetric ||
                                 nLutType == icXformLutNamedSpectral ||
                                 nLutType == icXformLutNamedDevice);
  const bool bWantSpectral    = (nLutType == icXformLutSpectral ||
                                 nLutType == icXformLutNamedSpectral);
  const bool bWantColorimetric = (nLutType == icXformLutColorimetric ||
                                 nLutType == icXformLutNamedColorimetric);
  const bool bWantDevice      = (nLutType == icXformLutNamedDevice);

  Xform.ptr = NULL;
  switch (nUseLutType) {
    //Automatically choose which one
    case icXformLutColor:
    case icXformLutColorimetric:
    case icXformLutSpectral:
    case icXformLutNamedColor:
    case icXformLutNamedColorimetric:
    case icXformLutNamedSpectral:
    case icXformLutNamedDevice:
    {
      CIccTag *pTag = pProfile->FindTag(icSigNamedColor2Tag);

      if (pTag && (pProfile->m_Header.deviceClass==icSigNamedColorClass || bExplicitNamed)) {
        if (bInput) {
          nSrcSpace = icSigNamedData;
        }
        else if (bWantDevice) {
          // name <- device direction: source is the profile's colorSpace.
          nSrcSpace = pProfile->m_Header.colorSpace;
        }
        else if (bWantSpectral || (!bWantColorimetric && (bUseD2BxB2DxTags || !pProfile->m_Header.pcs) && pProfile->m_Header.spectralPCS)) {
          nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
          bUseD2BxB2DxTags = true;
        }
        else {
          nSrcSpace = pProfile->m_Header.pcs;
        }

        if (!m_Xforms->size()) {
          if (m_nSrcSpace==icSigUnknownData) {
            m_nSrcSpace = nSrcSpace;
          }
          else {
            nSrcSpace = m_nSrcSpace;
          }
        }
        else {
          if (m_nLastSpace==icSigUnknownData) {
            m_nLastSpace = nSrcSpace;
          }
          else {
            nSrcSpace = m_nLastSpace;
          }
        }

        if (nSrcSpace==icSigNamedData) {
          if (bWantDevice) {
            // name -> device direction: dst is the profile's colorSpace.
            // Apply routes through CIccArrayNamedColor::GetDeviceTint
            // (v5 array) or memcpy of pTag->GetEntry(j)->deviceCoords
            // (v4 NamedColor2Tag), both of which surface
            // icCmmStatBadTintXform if no device member is available.
            nDstSpace = pProfile->m_Header.colorSpace;
          }
          else if (bWantSpectral || (!bWantColorimetric && bUseD2BxB2DxTags && pProfile->m_Header.spectralPCS)) {
            nDstSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
            bUseD2BxB2DxTags = true;
          }
          else {
            nDstSpace = pProfile->m_Header.pcs;
          }
          bInput = true;
        }
        else {
          nDstSpace = icSigNamedData;
          bInput = false;
        }

        Xform.ptr = CIccXform::Create(pProfile, bInput, nIntent, nInterp, pPcc, icXformLutNamedColor, bUseD2BxB2DxTags, pHintManager);
        if (!Xform.ptr) {
          // CIccXform::Create has already deleted the profile
          return icCmmStatBadXform;
        }
        CIccXformNamedColor *pXform = (CIccXformNamedColor *)Xform.ptr;
        rv = pXform->SetSrcSpace(nSrcSpace);
        if (rv) {
          delete Xform.ptr;
          return rv;
        }

        rv = pXform->SetDestSpace(nDstSpace);
        if (rv) {
          delete Xform.ptr;
          return rv;
        }
      }
      else {
        //It isn't named color so make we will use color lut.
        if (nUseLutType==icXformLutNamedColor ||
            nUseLutType==icXformLutNamedColorimetric ||
            nUseLutType==icXformLutNamedSpectral ||
            nUseLutType==icXformLutNamedDevice)
          nUseLutType = icXformLutColor;

        //Check pProfile if nIntent and input can be found.
        if (bInput) {
          nSrcSpace = pProfile->m_Header.colorSpace;

          if (bWantSpectral || (!bWantColorimetric && (bUseD2BxB2DxTags || !pProfile->m_Header.pcs) && pProfile->m_Header.spectralPCS)) {
            nDstSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
            bUseD2BxB2DxTags = true;
          }
          else
            nDstSpace = pProfile->m_Header.pcs;
        }
        else {
          if (pProfile->m_Header.deviceClass == icSigLinkClass) {
            delete pProfile;
            return icCmmStatBadSpaceLink;
          }
          if (pProfile->m_Header.deviceClass == icSigAbstractClass) {
            bInput = true;
            nIntent = icPerceptual; // Note: icPerceptualIntent = 0
          }

          if (bWantSpectral || (!bWantColorimetric && (bUseD2BxB2DxTags || !pProfile->m_Header.pcs) && pProfile->m_Header.spectralPCS)) {
            nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
            bUseD2BxB2DxTags = true;
          }
          else
            nSrcSpace = pProfile->m_Header.pcs;

          nDstSpace = pProfile->m_Header.colorSpace;
        }
      }
    }
    break;

    case icXformLutPreview:
      nSrcSpace = pProfile->m_Header.pcs;
      nDstSpace = pProfile->m_Header.pcs;
      bInput = false;
      break;

    case icXformLutGamut:
      nSrcSpace = pProfile->m_Header.pcs;
      nDstSpace = icSigGamutData;
      bInput = true;
      break;

    case icXformLutBRDFParam:
      nSrcSpace = pProfile->m_Header.colorSpace;
      nDstSpace = icSigUnknownData;
      break;

    case icXformLutBRDFDirect:
      nSrcSpace = pProfile->m_Header.colorSpace;
      nDstSpace = icSigUnknownData;
      break;

    case icXformLutMCS:
      switch(pProfile->m_Header.deviceClass)
      {
        case icSigInputClass:
        case icSigMultiplexIdentificationClass:
          nSrcSpace = pProfile->m_Header.colorSpace;
          nDstSpace = (icColorSpaceSignature)pProfile->m_Header.mcs;
          break;
        case icSigMultiplexVisualizationClass:
          nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.mcs;
          if (bUseD2BxB2DxTags && pProfile->m_Header.spectralPCS) {
            nDstSpace = (icColorSpaceSignature)pProfile->m_Header.spectralPCS;
          }
          else {
            nDstSpace = pProfile->m_Header.pcs;
          }
          bInput = true;
          break;

        case icSigMultiplexLinkClass:
          nSrcSpace = (icColorSpaceSignature)pProfile->m_Header.mcs;
          nDstSpace = pProfile->m_Header.colorSpace;
          break;

        default:
          delete pProfile;
          return icCmmStatBadLutType;
      }
      break;

    default:
      delete pProfile;
      return icCmmStatBadLutType;
  }

  //Make sure color spaces match with previous xforms
  if (!m_Xforms->size()) {
    if (m_nSrcSpace == icSigUnknownData) {
      m_nLastSpace = nSrcSpace;
      m_nSrcSpace = nSrcSpace;
    }
    else if (!IsCompatSpace(m_nSrcSpace, nSrcSpace) && !IsNChannelCompat(m_nSrcSpace, nSrcSpace)) {
      if (!Xform.ptr)
        delete pProfile;
      return icCmmStatBadSpaceLink;
    }
  }
  else if (!IsCompatSpace(m_nLastSpace, nSrcSpace) && !IsNChannelCompat(m_nSrcSpace, nSrcSpace))  {
      if (!Xform.ptr)
        delete pProfile;
      return icCmmStatBadSpaceLink;
  }

  if (!m_Xforms->size())
    m_nSrcSpace = nSrcSpace;

  m_nDestSpace = nDstSpace;

  //Automatic creation of intent from header/last profile
  if (nIntent==icUnknownIntent) {
    if (bInput) {
      nIntent = (icRenderingIntent)pProfile->m_Header.renderingIntent;
    }
    else {
      nIntent = m_nLastIntent;
    }
    if (nIntent == icUnknownIntent)
      nIntent = icPerceptual;
  }

  // this must be done before CIccXform::Create frees the profile pointer
  bool bLinked = (pProfile->m_Header.deviceClass == icSigLinkClass);

  if (!Xform.ptr)
    Xform.ptr = CIccXform::Create(pProfile, bInput, nIntent, nInterp, pPcc, nUseLutType, bUseD2BxB2DxTags, pHintManager);

  if (!Xform.ptr) {
    // CIccXform::Create has already deleted the profile
    return icCmmStatBadXform;
  }

  m_nLastSpace = Xform.ptr->GetDstSpace();
  m_nLastIntent = nIntent;

  if (bLinked)
    bInput = false;
  m_bLastInput = bInput;
  
  m_Xforms->push_back(Xform);

  return icCmmStatOk;
}

/**
 **************************************************************************
 * Name: CIccNamedColorCmm::Begin
 * 
 * Purpose: 
 *  Does the initialization of the Xforms in the list before Apply() is called.
 *  Must be called before Apply().
 *
 **************************************************************************
 */
 icStatusCMM CIccNamedColorCmm::Begin(bool bAllocNewApply/* =true */, bool bUsePcsConversion/*=false*/)
{
  if (m_nDestSpace==icSigUnknownData) {
    m_nDestSpace = m_nLastSpace;
  }
  else if (!IsCompatSpace(m_nDestSpace, m_nLastSpace) && !IsCompatSpacePCS(m_nDestSpace, m_nLastSpace)) {
    return icCmmStatBadSpaceLink;
  }

  if (m_nSrcSpace != icSigNamedData) {
    if (m_nDestSpace != icSigNamedData) {
      m_nApplyInterface = icApplyPixel2Pixel;
    }
    else {
      m_nApplyInterface = icApplyPixel2Named;
    }
  }
  else {
    if (m_nDestSpace != icSigNamedData) {
      m_nApplyInterface = icApplyNamed2Pixel;
    }
    else {
      m_nApplyInterface = icApplyNamed2Named;
    }
  }

  CheckPCSRangeConversions();
  SetLateBindingCC();

  icStatusCMM rv;
  CIccXformList::iterator i;

  for (i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
    rv = i->ptr->Begin();

    if (rv!= icCmmStatOk) {
      return rv;
    }
  }

  rv = CheckPCSConnections(bUsePcsConversion);
  if (rv != icCmmStatOk && rv!=icCmmStatIdentityXform)
    return rv;

  if (bAllocNewApply) {
    rv = icCmmStatOk;

    m_pApply = GetNewApplyCmm(rv);
  }
  else
    rv = icCmmStatOk;

  return rv;
}

 /**
 **************************************************************************
 * Name: CIccNamedColorCmm::GetNewApplyCmm
 * 
 * Purpose: 
 *  Allocates a CIccApplyCmm object that allows one to call apply from
 *  multiple threads.
 *
 **************************************************************************
 */
 CIccApplyCmm *CIccNamedColorCmm::GetNewApplyCmm(icStatusCMM &status)
 {
  CIccApplyCmm *pApply = new (std::nothrow) CIccApplyNamedColorCmm(this);

  if (pApply) {
    for (CIccXformList::iterator i=m_Xforms->begin(); i!=m_Xforms->end(); i++) {
      CIccApplyXform *pXform = i->ptr->GetNewApply(status);
      if (status != icCmmStatOk || !pXform) {
        delete pApply;
        return NULL;
      }
      pApply->AppendApplyXform(pXform);
    }

    m_bValid = true;
    status = icCmmStatOk;
  }
  return pApply;
}


 /**
 **************************************************************************
 * Name: CIccApplyNamedColorCmm::Apply
 * 
 * Purpose: 
 *  Does the actual application of the Xforms in the list.
 *  
 * Args:
 *  DstColorName = Destination string where the result is stored, 
 *  SrcPoxel = Source pixel
 **************************************************************************
 */
icStatusCMM CIccNamedColorCmm::Apply(icChar* DstColorName, const icFloatNumber *SrcPixel)
{
  return ((CIccApplyNamedColorCmm*)m_pApply)->Apply(DstColorName, SrcPixel);
}


/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DestPixel = Destination pixel where the result is stored, 
*  SrcColorName = Source color name which is to be searched.
**************************************************************************
*/
icStatusCMM CIccNamedColorCmm::Apply(icFloatNumber *DstPixel, const icChar *SrcColorName, icFloatNumber tint/*=1.0*/)
{
  return ((CIccApplyNamedColorCmm*)m_pApply)->Apply(DstPixel, SrcColorName, tint);
}


/**
**************************************************************************
* Name: CIccApplyNamedColorCmm::Apply
* 
* Purpose: 
*  Does the actual application of the Xforms in the list.
*  
* Args:
*  DstColorName = Destination string where the result is stored, 
*  SrcColorName = Source color name which is to be searched.
**************************************************************************
*/
icStatusCMM CIccNamedColorCmm::Apply(icChar* DstColorName, const icChar *SrcColorName, icFloatNumber tint/*=1.0*/)
{
  return ((CIccApplyNamedColorCmm*)m_pApply)->Apply(DstColorName, SrcColorName, tint);
}


/**
 **************************************************************************
 * Name: CIccNamedColorCmm::SetLastXformDest
 * 
 * Purpose: 
 *  Sets the destination Color space of the last Xform in the list
 * 
 * Args: 
 *  nDestSpace = signature of the color space to be set
 **************************************************************************
 */
icStatusCMM CIccNamedColorCmm::SetLastXformDest(icColorSpaceSignature nDestSpace)
{
  int n = (int)m_Xforms->size();
  CIccXformPtr *pLastXform;

  if (!n)
    return icCmmStatBadXform;

  pLastXform = &m_Xforms->back();
  
  if (pLastXform->ptr->GetXformType()==icXformTypeNamedColor) {
    CIccXformNamedColor *pXform = (CIccXformNamedColor *)pLastXform->ptr;
    if (pXform->GetSrcSpace() == icSigNamedData &&
        nDestSpace == icSigNamedData) {
      return icCmmStatBadSpaceLink;
    }

    if (nDestSpace != icSigNamedData &&
        pXform->GetDstSpace() == icSigNamedData) {
      return icCmmStatBadSpaceLink;
    }
    
    return pXform->SetDestSpace(nDestSpace);
  }

  return icCmmStatBadXform;
}


/**
****************************************************************************
* Name: CIccMruCmm::CIccMruCmm
* 
* Purpose: private constructor - Use Attach to create CIccMruCmm objects
*****************************************************************************
*/
CIccMruCmm::CIccMruCmm()
{
  m_pCmm = NULL;
  m_bDeleteCmm = false;
  m_nCacheSize = 0;
}


/**
****************************************************************************
* Name: CIccMruCmm::~CIccMruCmm
* 
* Purpose: destructor
*****************************************************************************
*/
CIccMruCmm::~CIccMruCmm()
{
   if (m_bDeleteCmm)
     delete m_pCmm;
}


/**
****************************************************************************
* Name: CIccMruCmm::Attach
* 
* Purpose: Create a Cmm decorator object that implements a cache of most
*  recently used pixel transformations.
* 
* Args:
*  pCmm - pointer to cmm object that we are attaching to.
*  nCacheSize - number of most recently used transformations to cache
*  bDeleteCmm - flag to indicate whether cmm should be deleted when
*    this is destroyed.
*
* Return:
*  A CIccMruCmm object that represents a cached form of the pCmm passed in.
*  The pCmm will be owned by the returned object unless.
*
*  If this function fails the pCmm object will be deleted.
*****************************************************************************
*/
CIccMruCmm* CIccMruCmm::Attach(CIccCmm *pCmm, icUInt8Number nCacheSize/* =4 */, bool bDeleteCmm/*=true*/)
{
  if (!pCmm || !nCacheSize)
    return NULL;

  if (!pCmm->Valid()) {
    if (bDeleteCmm)
      delete pCmm;
    return NULL;
  }

  CIccMruCmm *rv = new (std::nothrow) CIccMruCmm();
  if (!rv)
    return NULL;

  rv->m_pCmm = pCmm;
  rv->m_nCacheSize = nCacheSize;
  rv->m_bDeleteCmm = bDeleteCmm;

  rv->m_nSrcSpace = pCmm->GetSourceSpace();
  rv->m_nDestSpace = pCmm->GetDestSpace();
  rv->m_nLastSpace = pCmm->GetLastSpace();
  rv->m_nLastIntent = pCmm->GetLastIntent();

  if (rv->Begin()!=icCmmStatOk) {
    delete rv;
    return NULL;
  }

  return rv;
}

CIccApplyCmm *CIccMruCmm::GetNewApplyCmm(icStatusCMM &status)
{
  CIccApplyMruCmm *rv = new (std::nothrow) CIccApplyMruCmm(this);

  if (!rv) {
    status = icCmmStatAllocErr;
    return NULL;
  }

  if (!rv->Init(m_pCmm, m_nCacheSize)) {
    delete rv;
    status = icCmmStatBad;
    return NULL;
  }

  return rv;
}

/**
****************************************************************************
* Name: CIccMruCache::CIccMruCache
*
* Purpose: constructor
*****************************************************************************
*/
template<class T>
CIccMruCache<T>::CIccMruCache()
{
  m_cache = NULL;
  m_nNumPixel = 0;
  m_pixelData = NULL;
  m_nSrcSamples = 0;
  m_pFirst = NULL;
}

/**
****************************************************************************
* Name: CIccMruCache::~CIccMruCache
*
* Purpose: destructor
*****************************************************************************
*/
template<class T>
CIccMruCache<T>::~CIccMruCache()
{
  delete[] m_cache;

  free(m_pixelData);
}

/**
****************************************************************************
* Name: CIccMruCache::Init
*
* Purpose: Initialize the object and set up the cache
*
* Args:
*  pCmm - pointer to cmm object that we are attaching to.
*  nCacheSize - number of most recently used transformations to cache
*
* Return:
*  true if successful
*****************************************************************************
*/
template<class T>
bool CIccMruCache<T>::Init(icUInt16Number nSrcSamples, icUInt16Number nDstSamples, icUInt16Number nCacheSize)
{
  m_nSrcSamples = nSrcSamples;
  m_nSrcSize = nSrcSamples * sizeof(T);
  m_nDstSize = nDstSamples * sizeof(T);

  m_nTotalSamples = m_nSrcSamples + nDstSamples;

  m_nNumPixel = 0;
  m_nCacheSize = nCacheSize;

  m_pFirst = NULL;
  m_cache = new (std::nothrow) CIccMruPixel<T>[nCacheSize];

  if (!m_cache)
    return false;

  m_pixelData = (T*)malloc((size_t)nCacheSize * m_nTotalSamples * sizeof(T));

  if (!m_pixelData)
    return false;

  return true;
}

template<class T>
CIccMruCache<T> *CIccMruCache<T>::NewMruCache(icUInt16Number nSrcSamples, icUInt16Number nDstSamples, icUInt16Number nCacheSize /* = 4 */)
{
  CIccMruCache<T> *rv = new (std::nothrow) CIccMruCache<T>;

  if (rv && !rv->Init(nSrcSamples, nDstSamples, nCacheSize)) {
    delete rv;
    return NULL;
  }

  return rv;
}

/**
****************************************************************************
* Name: CIccMruCache::Apply
*
* Purpose: Apply a transformation to a pixel.
*
* Args:
*  DstPixel - Location to store pixel results
*  SrcPixel - Location to get pixel values from
*
* Return:
*  true if SrcPixel found in cache and DstPixel initialized with value
*  fails if SrcPixel not found (DstPixel not touched)
*****************************************************************************
*/
template<class T>
bool CIccMruCache<T>::Apply(T *DstPixel, const T *SrcPixel)
{
  CIccMruPixel<T> *ptr, *prev = NULL, *last = NULL;
  int i;
  T *pixel;

  for (ptr = m_pFirst, i = 0; ptr; ptr = ptr->pNext, i++) {
    if (!memcmp(SrcPixel, ptr->pPixelData, m_nSrcSize)) {
      memcpy(DstPixel, &ptr->pPixelData[m_nSrcSamples], m_nDstSize);
      if (ptr != m_pFirst) {
        last->pNext = ptr->pNext;

        ptr->pNext = m_pFirst;
        m_pFirst = ptr;
      }
      return true;
    }
    prev = last;
    last = ptr;
  }

  //If we get here SrcPixel is not in the cache
  if (i < m_nCacheSize) {
    pixel = &m_pixelData[i*m_nTotalSamples];

    ptr = &m_cache[i];
    ptr->pPixelData = pixel;

    if (m_pFirst) {
      ptr->pNext = m_pFirst;
    }
    m_pFirst = ptr;
  }
  else {  //Reuse oldest value and put it at the front of the list
    if (prev)
      prev->pNext = NULL;

    // Static analysis false positive: last is never NULL here because
    // m_nCacheSize >= 1 guarantees at least one loop iteration sets last = ptr
    assert( last != NULL );
    if (!last)
      return false;
    assert( m_pFirst != NULL );
    last->pNext = m_pFirst;

    m_pFirst = last;
    pixel = last->pPixelData;
  }
  
  //T *dest = &pixel[m_nSrcSamples];        // ERROR - the memcpy is probably wrong, as this value was unused

  memcpy(pixel, SrcPixel, m_nSrcSize);

  return false;
}

template<class T>
void CIccMruCache<T>::Update(T* DstPixel)
{
  memcpy(&m_pFirst->pPixelData[m_nSrcSamples], DstPixel, m_nDstSize);
}

//Make sure typedef classes get built
template class CIccMruCache<icFloatNumber>;
template class CIccMruCache<icUInt8Number>;
template class CIccMruCache<icUInt16Number>;


CIccApplyMruCmm::CIccApplyMruCmm(CIccMruCmm *pCmm) : CIccApplyCmm(pCmm)
{
  m_pCachedCmm = NULL;
  m_pCache = NULL;
}

/**
****************************************************************************
* Name: CIccApplyMruCmm::~CIccApplyMruCmm
* 
* Purpose: destructor
*****************************************************************************
*/
CIccApplyMruCmm::~CIccApplyMruCmm()
{
  delete m_pCache;
}

/**
****************************************************************************
* Name: CIccApplyMruCmm::Init
* 
* Purpose: Initialize the object and set up the cache
* 
* Args:
*  pCmm - pointer to cmm object that we are attaching to.
*  nCacheSize - number of most recently used transformations to cache
*
* Return:
*  true if successful
*****************************************************************************
*/
bool CIccApplyMruCmm::Init(CIccCmm *pCachedCmm, icUInt16Number nCacheSize)
{
  m_pCachedCmm = pCachedCmm;

  m_pCache = CIccMruCacheFloat::NewMruCache(m_pCmm->GetSourceSamples(), m_pCmm->GetDestSamples(), nCacheSize);

  if (!m_pCache)
    return false;

  return true;
}

/**
****************************************************************************
* Name: CIccMruCmm::Apply
* 
* Purpose: Apply a transformation to a pixel.
* 
* Args:
*  DstPixel - Location to store pixel results
*  SrcPixel - Location to get pixel values from
*
* Return:
*  icCmmStatOk if successful
*****************************************************************************
*/
icStatusCMM CIccApplyMruCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel)
{
#if defined(_DEBUG)
  if (!m_pCache)
    return icCmmStatInvalidLut;
#endif

  if (!m_pCache->Apply(DstPixel, SrcPixel)) {

    m_pCachedCmm->Apply(DstPixel, SrcPixel);

    m_pCache->Update(DstPixel);
  }

  return icCmmStatOk;
}

/**
****************************************************************************
* Name: CIccMruCmm::Apply
* 
* Purpose: Apply a transformation to a pixel.
* 
* Args:
*  DstPixel - Location to store pixel results
*  SrcPixel - Location to get pixel values from
*  nPixels - number of pixels to convert
*
* Return:
*  icCmmStatOk if successful
*****************************************************************************
*/
icStatusCMM CIccApplyMruCmm::Apply(icFloatNumber *DstPixel, const icFloatNumber *SrcPixel, icUInt32Number nPixels)
{
  icUInt32Number k;

#if defined(_DEBUG)
  if (!m_pCache)
    return icCmmStatInvalidLut;
#endif

  for (k=0; k<nPixels;k++) {
    if (!m_pCache->Apply(DstPixel, SrcPixel)) {
      m_pCachedCmm->Apply(DstPixel, SrcPixel);
      m_pCache->Update(DstPixel);
    }
    SrcPixel += m_pCmm->GetSourceSamples();
    DstPixel += m_pCmm->GetDestSamples();
  }

  return icCmmStatOk;
}


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
