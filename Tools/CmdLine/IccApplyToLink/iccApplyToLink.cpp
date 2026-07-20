/*
    File:       IccApplyToLink.cpp

    Contains:   Console app that applies profiles to create link

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2023 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 3-15-2023
//
//////////////////////////////////////////////////////////////////////


#include <cerrno>
#include <climits>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <list>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccApplyBPC.h"
#include "IccEnvVar.h"
#include "IccProfLibVer.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "../IccCmdLineUtil.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ============================================================================

static bool IccApplyToLinkDiagnosticsEnabled()
{
  const char *env = getenv("ICC_APPLYTOLINK_DIAGNOSTICS");
  return env && env[0] && env[0] != '0';
}

static void PrintRangeDiagnostic(icFloatNumber loRange, icFloatNumber hiRange, icFloatNumber sizeRange)
{
  if (!IccApplyToLinkDiagnosticsEnabled()) {
    return;
  }

  fprintf(stderr,
          "ICC_DIAG: range min=%g max=%g size=%g finite(min,max,size)=%d,%d,%d\n",
          loRange,
          hiRange,
          sizeRange,
          std::isfinite(loRange) ? 1 : 0,
          std::isfinite(hiRange) ? 1 : 0,
          std::isfinite(sizeRange) ? 1 : 0);
}

// Compute an integer completion percentage for the LUT-generation progress
// display below. The device-link build walks lutCount = nLutSize^nSrcSamples
// grid points, which for a 5-channel source at nLutSize=33 is 33^5 = 39135393
// -- so the old `((c + 1) * 100) / lutCount` overflowed 'int' once c+1 reached
// 21474837 (21474837 * 100 > INT_MAX), the signed-integer-overflow reported in
// #1730. Widening the numerator to 64-bit (100ULL) keeps the product exact
// before the divide, and the result is a 0..100 percentage that fits back in
// 'int'. total==0 is guarded (caller also checks lutCount>0) to avoid /0.
static constexpr int GetProgressPercent(size_t completed, size_t total)
{
  return total ? (int)((completed * 100ULL) / total) : 0;
}

// Pin the helper's result for the exact input that overflowed in #1730
// (21474837 * 100 exceeds INT_MAX). This documents the case and guards the
// computation against being narrowed back to a 32-bit type, where the multiply
// would overflow. Note it does NOT catch reintroducing the original call-site
// expression: both parameters are size_t, so writing `completed * 100` here
// still evaluates in 64-bit unsigned and this assertion would still hold.
static_assert(GetProgressPercent(21474837ULL, 39135393ULL) == 54,
              "Large LUT progress must be computed without 32-bit overflow");

class ILinkWriter
{
public:
  ILinkWriter() {}
  virtual ~ILinkWriter() {};

  virtual bool setFile(const char* szOutputFile) = 0;

  virtual void setOption(int nOptionValue) = 0;

  virtual void setTitle(const char* szTitle) = 0;

  virtual void setCmm(CIccCmm *pCmm) = 0;

  virtual bool setLutSize(icUInt8Number nGridSize) = 0;

  virtual void setInputRange(icFloatNumber fMinInput, icFloatNumber fMaxInput) = 0;

  virtual bool begin(icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace) = 0;

  virtual void setNextNode(icFloatNumber* pPixel)=0;

  virtual bool finish() = 0;
};

typedef std::shared_ptr <ILinkWriter> LinkWriterPtr;

class CCubeWriter : public ILinkWriter, public IXformIterator
{
public:
  CCubeWriter() { }
  virtual ~CCubeWriter()
  {
    if (m_f)
      fclose(m_f);
  }

  virtual bool setFile(const char* szOutputFile)
  {
    m_filename = icSanitizeFileName( szOutputFile );

    return true;
  }

  virtual void setOption(int nOptionValue)
  {
    m_precision = nOptionValue;
  }

  virtual void setTitle(const char* szTitle)
  {
    m_title = icSanitizeTagText( szTitle );
  }

  virtual void setCmm(CIccCmm* pCmm)
  {
    m_pCmm = pCmm;
  }

  virtual bool setLutSize(icUInt8Number nGridSize)
  {
    m_grid = nGridSize;

    return true;
  }

  virtual void setInputRange(icFloatNumber fMinInput, icFloatNumber fMaxInput)
  {
    m_fMinInput = fMinInput;
    m_fMaxInput = fMaxInput;
  }

  virtual bool begin(icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace)
  {
    icUInt32Number nSrcSamples = icGetSpaceSamples(srcSpace);
    icUInt32Number nDstSamples = icGetSpaceSamples(dstSpace);

    if (nSrcSamples != 3) {
      printf("# of source Samples is not 3\n");
      return false;
    }

    if (nDstSamples != 3) {
      printf("# of destination samples is not 3\n");
      return false;
    }

    m_f = icOpenRegularWriteBinaryFile(m_filename.c_str());
    if (!m_f) {
      printf("Unable to open '%s'\n", m_filename.c_str());
      return false;
    }

    fprintf(m_f, "TITLE \"%s\"\n\n", m_title.c_str());

    fprintf(m_f, "# cube file created by iccApplyToLink version " ICCPROFLIBVER "\n#\n");
    if (m_pCmm) {
      fprintf(m_f, "# Profiles applied (by profile description):\n");
      m_pCmm->IterateXforms(this);
    }
    fprintf(m_f, "\n");

    fprintf(m_f, "LUT_3D_SIZE %d\n", m_grid);

    if (!icIsNear(m_fMinInput, 0.0f) || !icIsNear(m_fMaxInput, 1.0f)) {
      const size_t fmtSize = 100;   // allow for twice precision
      char fmt[fmtSize];
      snprintf(fmt, fmtSize, "LUT_3D_INPUT_RANGE %%.%df %%.%df\n", m_precision, m_precision);
      fprintf(m_f, fmt, m_fMinInput, m_fMaxInput);
    }

    fprintf(m_f, "\n");

    return true;
  }

  virtual void iterate(const CIccXform* pXform)
  {
    if (pXform) {
      const CIccProfile* pProfile = pXform->GetProfile();
      if (pProfile) {
        const CIccTag* pDesc = pProfile->FindTagConst(icSigProfileDescriptionTag);
        std::string text;
        if (icGetTagText(pDesc, text)) {
          std::string cleanText = icSanitizeTagText( text );
          fprintf(m_f, "# - %s\n", cleanText.c_str());
        }

      }
    }
  }

  virtual void setNextNode(icFloatNumber* pPixel)
  {
    const size_t fmtSize = 50;  // must be larger than allowed precision
    char fmt[fmtSize];
    snprintf(fmt, fmtSize, "%%.%df", m_precision);

    for (int i = 0; i < 3; i++) {
      if (i)
        fprintf(m_f, " ");
      fprintf(m_f, fmt, pPixel[i]);
    }
    fprintf(m_f, "\n");
  }

  virtual bool finish()
  {
    bool rv = true;

    if (m_f) {
      if (fflush(m_f))
        rv = false;
      if (ferror(m_f))
        rv = false;
      if (fclose(m_f))
        rv = false;
    }

    m_f = nullptr;

    return rv;
  }

protected:
  std::string m_filename;
  int m_precision = 4;
  std::string m_title;

  CIccCmm* m_pCmm = nullptr;

  int m_grid = 0;

  icFloatNumber m_fMinInput = 0.0;
  icFloatNumber m_fMaxInput = 1.0;

  FILE* m_f = nullptr;
};

class CDevLinkWriter : public ILinkWriter, public IXformIterator
{
public:
  CDevLinkWriter() { }
  virtual ~CDevLinkWriter()
  {
    delete m_pProfile;
  }

  virtual bool setFile(const char* szOutputFile)
  {
    m_filename = icSanitizeFileName( szOutputFile );

    return true;
  }

  virtual void setOption(int nOptionValue)
  {
    m_bWriteV5 = nOptionValue != 0;
  }

  virtual void setTitle(const char* szTitle)
  {
    m_title = icSanitizeTagText( szTitle );
  }

  virtual void setCmm(CIccCmm* pCmm)
  {
    m_pCmm = pCmm;
  }

  virtual bool setLutSize(icUInt8Number nGridSize)
  {
    m_grid = nGridSize;

    return true;
  }

  virtual void setInputRange(icFloatNumber fMinInput, icFloatNumber fMaxInput)
  {
    m_fMinInput = fMinInput;
    m_fMaxInput = fMaxInput;
  }

  virtual bool begin(icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace)
  {
    icUInt16Number nSrcSamples = (icUInt16Number)icGetSpaceSamples(srcSpace);
    icUInt16Number nDstSamples = (icUInt16Number)icGetSpaceSamples(dstSpace);
    m_nOutSamples = nDstSamples;

    if (!nSrcSamples || (!m_bWriteV5 && nSrcSamples > 15) || (m_bWriteV5 && nSrcSamples > 16)) {
      printf("Invalid number of source samples\n");
      return false;
    }
    if (!nDstSamples || (!m_bWriteV5 && nDstSamples > 15)) {
      printf("Invalid number of destination samples\n");
      return false;
    }

    CIccInfo Info;
    if (!Info.IsValidSpace(srcSpace)) {
      printf("Invalid source color space\n");
      return false;
    }
    if (!Info.IsValidSpace(dstSpace)) {
      printf("Invalid destination color space\n");
      return false;
    }

    m_pProfile = new CIccProfile();

    //Initialize profile header
    m_pProfile->InitHeader();
    m_pProfile->m_Header.colorSpace = srcSpace;
    m_pProfile->m_Header.pcs = dstSpace;
    m_pProfile->m_Header.deviceClass = icSigLinkClass;

    //Add description Tag
    CIccTagMultiLocalizedUnicode* pTextTag = new CIccTagMultiLocalizedUnicode();
    if (m_title.size()) {
      pTextTag->SetText(m_title.c_str());
    }
    else {
      pTextTag->SetText("Device link created by iccApplyToLink");
    }
    m_pProfile->AttachTag(icSigProfileDescriptionTag, pTextTag);

    pTextTag = new CIccTagMultiLocalizedUnicode();
    pTextTag->SetText("Copyright ICC");
    m_pProfile->AttachTag(icSigCopyrightTag, pTextTag);

    if (m_bWriteV5) {
      m_pProfile->m_Header.version = icVersionNumberV5;

      //Create A2B0 Tag with LUT
      CIccTagMultiProcessElement* pTag = new CIccTagMultiProcessElement(nSrcSamples, nDstSamples);

      if (!icIsNear(m_fMinInput, 0.0) || !icIsNear(m_fMaxInput, 1.0)) {
        CIccMpeCurveSet* pCurves = new CIccMpeCurveSet(nSrcSamples);
        CIccSingleSampledCurve* pCurve0 = new CIccSingleSampledCurve(m_fMinInput, m_fMaxInput);

        pCurve0->SetSize(2);
        pCurve0->GetSamples()[0] = 0;
        pCurve0->GetSamples()[1] = 1;

        pCurves->SetCurve(0, pCurve0);

        for (icUInt16Number i = 1; i < nSrcSamples; i++) {
          pCurves->SetCurve(1, pCurve0);
        }

        pTag->Attach(pCurves);
      }

      CIccMpeCLUT* pMpeCLUT = new CIccMpeCLUT();
      CIccCLUT* pCLUT = new CIccCLUT((icUInt8Number)nSrcSamples, (icUInt8Number)nDstSamples);
      pCLUT->Init(m_grid);
      m_pLutPtr = pCLUT->GetData(0);
      m_nCountdown = pCLUT->NumPoints();

      pMpeCLUT->SetCLUT(pCLUT);
      pTag->Attach(pMpeCLUT);

      m_pProfile->AttachTag(icSigAToB0Tag, pTag);
    }
    else {
      m_pProfile->m_Header.version = icVersionNumberV4_3;

      m_pFirstProfile = nullptr;
      m_pLastProfile = nullptr;

      m_pTagSeq = new CIccTagProfileSeqDesc();

      if (m_pCmm) {
        m_pCmm->IterateXforms(this);
      }

      if (m_pTagSeq && m_pTagSeq->m_Descriptions && m_pTagSeq->m_Descriptions->size()) {
        m_pProfile->AttachTag(icSigProfileSequenceDescTag, m_pTagSeq);
      }
      else {
        delete m_pTagSeq;
        m_pTagSeq = nullptr;
      }

      if (icIsSpaceCLR(srcSpace) && m_pFirstProfile) {
        if (m_pFirstProfile->m_Header.version < icVersionNumberV5) {
          const CIccTag* pTagColor = m_pFirstProfile->FindTagConst(icSigColorantTableTag);
          if (pTagColor) {
            m_pProfile->AttachTag(icSigColorantTableTag, pTagColor->NewCopy());
          }
        }
        else if (m_pFirstProfile->m_Header.version >= icVersionNumberV5) {
          // V5+ source profiles carry colorant naming through the V5 machinery
          // rather than the legacy colorantTable tag, so there is nothing to
          // copy across for the CLR input space here.
        }
      }

      if (icIsSpaceCLR(dstSpace) && m_pLastProfile) {
        if (m_pLastProfile->m_Header.deviceClass == icSigLinkClass) {
          if (m_pLastProfile->m_Header.version < icVersionNumberV5) {
            const CIccTag* pTagColor = m_pLastProfile->FindTagConst(icSigColorantTableOutTag);
            if (pTagColor) {
              m_pProfile->AttachTag(icSigColorantTableTag, pTagColor->NewCopy());
            }
          }
          else if (m_pLastProfile->m_Header.version >= icVersionNumberV5) {
            // V5+ destination DeviceLink: no legacy colorantTableOut tag to copy;
            // V5 colorant naming is handled through the V5 machinery elsewhere.
          }
        }
        else if (m_pLastProfile->m_Header.version < icVersionNumberV5) {
          const CIccTag* pTagColor = m_pLastProfile->FindTagConst(icSigColorantTableTag);
          if (pTagColor) {
            m_pProfile->AttachTag(icSigColorantTableOutTag, pTagColor->NewCopy());
          }
        }
        else if (m_pLastProfile->m_Header.version >= icVersionNumberV5) {
          // V5+ destination profile: no legacy colorantTable tag to copy; V5
          // colorant naming is handled through the V5 machinery elsewhere.
        }
      }

      CIccTagLutAtoB* pTagLut = new CIccTagLutAtoB();
      pTagLut->Init((icUInt8Number)nSrcSamples, (icUInt8Number)nDstSamples);
      LPIccCurve *pCurves = pTagLut->NewCurvesA();
      for (icUInt16Number i = 0; i < nSrcSamples; i++) {
        pCurves[i] = new CIccTagCurve();
      }
      pCurves = pTagLut->NewCurvesB();
      for (icUInt16Number i = 0; i < nDstSamples; i++) {
        pCurves[i] = new CIccTagCurve();
      }
      CIccCLUT* pCLUT = pTagLut->NewCLUT(m_grid);
      m_pLutPtr = pCLUT->GetData(0);
      m_nCountdown = pCLUT->NumPoints();

      m_pProfile->AttachTag(icSigAToB0Tag, pTagLut);
    }

    return true;
  }

  virtual void iterate(const CIccXform* pXform)
  {
    if (pXform) {
      const CIccProfile* pProfile = pXform->GetProfile();
      if (pProfile) {
        if (!m_pFirstProfile)
          m_pFirstProfile = pProfile;
        m_pLastProfile = pProfile;

        if (!m_bWriteV5 && m_pTagSeq) {
          CIccProfileDescStruct psd;

          psd.m_deviceMfg = pProfile->m_Header.manufacturer;
          psd.m_deviceModel = pProfile->m_Header.model;
          psd.m_attributes = pProfile->m_Header.attributes;
          psd.m_technology = icSigUndefined;

          // Populate the profileSequenceDesc device-description sub-tags with
          // valid default mluc text up front. When a source profile carried no
          // deviceMfgDesc/deviceModelDesc tag these were left unset, and writing
          // a V4 device link then serialized empty descriptions -- the pseq
          // write failed and iccApplyToLink exited 255 (#1730). The real dmnd/
          // dmdd values below overwrite these defaults when present.
          psd.m_deviceMfgDesc.SetType(icSigMultiLocalizedUnicodeType);
          CIccTagMultiLocalizedUnicode* pMfgDesc =
            (CIccTagMultiLocalizedUnicode*)psd.m_deviceMfgDesc.GetTag();
          if (pMfgDesc)
            pMfgDesc->SetText("Unknown device manufacturer");

          psd.m_deviceModelDesc.SetType(icSigMultiLocalizedUnicodeType);
          CIccTagMultiLocalizedUnicode* pModelDesc =
            (CIccTagMultiLocalizedUnicode*)psd.m_deviceModelDesc.GetTag();
          if (pModelDesc) {
            // The profile description is the best available model text when dmdd is absent.
            std::string profileDesc;
            const CIccTag* pDesc = pProfile->FindTagConst(icSigProfileDescriptionTag);
            if (pDesc && icGetTagText(pDesc, profileDesc))
              pModelDesc->SetText(profileDesc.c_str());
            else
              pModelDesc->SetText("Unknown device model");
          }

          const CIccTag* pTag = pProfile->FindTagConst(icSigTechnologyTag);
          if (pTag && pTag->GetType() == icSigSignatureType) {
            const CIccTagSignature* pSigTag = (const CIccTagSignature*)pTag;

            psd.m_technology = (icTechnologySignature)pSigTag->GetValue();
          }

          pTag = pProfile->FindTagConst(icSigDeviceMfgDescTag);
          if (pTag) {
            if (pTag->GetType() == icSigTextDescriptionType) {
              const CIccTagTextDescription* pTextTag = (const CIccTagTextDescription*)pTag;

              psd.m_deviceMfgDesc.SetType(icSigTextDescriptionType);
              CIccTagTextDescription* pText = (CIccTagTextDescription*)psd.m_deviceMfgDesc.GetTag();
              if (pText)
                *pText = *pTextTag;
            }
            else if (pTag->GetType() == icSigMultiLocalizedUnicodeType) {
              const CIccTagMultiLocalizedUnicode* pTextTag = (const CIccTagMultiLocalizedUnicode*)pTag;

              psd.m_deviceMfgDesc.SetType(icSigMultiLocalizedUnicodeType);
              CIccTagMultiLocalizedUnicode* pText = (CIccTagMultiLocalizedUnicode*)psd.m_deviceMfgDesc.GetTag();
              if (pText)
                *pText = *pTextTag;
            }
          }

          // Copy the source deviceModelDesc into m_deviceModelDesc. This block
          // previously wrote the model text into m_deviceMfgDesc (a copy/paste
          // of the manufacturer block above), clobbering the manufacturer
          // description and leaving the model description at its default.
          pTag = pProfile->FindTagConst(icSigDeviceModelDescTag);
          if (pTag) {
            if (pTag->GetType() == icSigTextDescriptionType) {
              const CIccTagTextDescription* pTextTag = (const CIccTagTextDescription*)pTag;

              psd.m_deviceModelDesc.SetType(icSigTextDescriptionType);
              CIccTagTextDescription* pText = (CIccTagTextDescription*)psd.m_deviceModelDesc.GetTag();
              if (pText)
                *pText = *pTextTag;
            }
            else if (pTag->GetType() == icSigMultiLocalizedUnicodeType) {
              const CIccTagMultiLocalizedUnicode* pTextTag = (const CIccTagMultiLocalizedUnicode*)pTag;

              psd.m_deviceModelDesc.SetType(icSigMultiLocalizedUnicodeType);
              CIccTagMultiLocalizedUnicode* pText = (CIccTagMultiLocalizedUnicode*)psd.m_deviceModelDesc.GetTag();
              if (pText)
                *pText = *pTextTag;
            }
          }

          m_pTagSeq->m_Descriptions->push_back(psd);
        }
      }
    }
  }

  virtual void setNextNode(icFloatNumber* pPixel)
  {
    if (m_nCountdown) {
      memcpy(m_pLutPtr, pPixel, m_nOutSamples * sizeof(icFloatNumber));
      m_pLutPtr += m_nOutSamples;
      m_nCountdown--;
    }
  }

  virtual bool finish()
  {
    bool rv = false;
    if (m_pProfile) {
      rv = SaveIccProfile(m_filename.c_str(), m_pProfile);
      delete m_pProfile;
    }

    m_pProfile = nullptr;

    return rv;
  }

protected:
  std::string m_filename;
  bool m_bWriteV5 = true;
  std::string m_title;

  std::string m_description;

  CIccCmm* m_pCmm = nullptr;

  int m_grid = 0;

  icFloatNumber m_fMinInput = 0.0;
  icFloatNumber m_fMaxInput = 1.0;

  icFloatNumber* m_pLutPtr = nullptr;
  icUInt32Number m_nCountdown = 0;

  icUInt16Number m_nOutSamples;

  CIccProfile* m_pProfile = nullptr;

  //For establishing the profile sequence description and color order tags for v4
  const CIccProfile* m_pFirstProfile = nullptr;
  const CIccProfile* m_pLastProfile = nullptr;

  CIccTagProfileSeqDesc* m_pTagSeq = nullptr;
};

void Usage() 
{
  printf("iccApplyToLink built with IccProfLib version " ICCPROFLIBVER "\n\n");

  printf("Usage: iccApplyToLink dst_link_file link_type lut_size option title range_min range_max first_transform interp {{-ENV:sig value} profile_file_path rendering_intent {-PCC connection_conditions_path}}\n\n");
  printf("  dst_link_file is path of file to create\n\n");
  
  printf("  For link_type:\n");
  printf("    0 - Device Link\n");
  printf("    1 - .cube text file\n\n");
  
  printf("  Where lut_size represents the number of grid entries for each lut dimension.\n\n");
  
  printf("  For option when link_type is 0:\n");
  printf("    0 - version 4 profile with 16-bit table\n");
  printf("    1 - version 5 profile\n\n");
  printf("  For option when link_type is 1:\n");
  printf("    option represents the digits of precision for lut for .cube files\n\n");

  printf("  title is the title/description for the dest_link_file\n\n");

  printf("  range_min specifies the minimum input value (usually 0.0)\n");
  printf("  range_max specifies the maximum input value (usually 1.0)\n\n");

  printf("  For first_transform:\n");
  printf("    0 - use destination transform from first profile\n");
  printf("    1 - use source transform from first profile\n\n");

  printf("  For interp:\n");
  printf("    0 - linear interpolation\n");
  printf("    1 - tetrahedral interpolation\n\n");

  printf("  For rendering_intent:\n");
  printf("    0 - Perceptual\n");
  printf("    1 - Relative\n");
  printf("    2 - Saturation\n");
  printf("    3 - Absolute\n");
  printf("    10 - Perceptual without D2Bx/B2Dx\n");
  printf("    11 - Relative without D2Bx/B2Dx\n");
  printf("    12 - Saturation without D2Bx/B2Dx\n");
  printf("    13 - Absolute without D2Bx/B2Dx\n");
  printf("    20 - Preview Perceptual\n");
  printf("    21 - Preview Relative\n");
  printf("    22 - Preview Saturation\n");
  printf("    23 - Preview Absolute\n");
  printf("    30 - Gamut\n");
  printf("    33 - Gamut Absolute\n");
  printf("    40 - Perceptual with BPC\n");
  printf("    41 - Relative Colorimetric with BPC\n");
  printf("    42 - Saturation with BPC\n");
  printf("    50 - BDRF Parameters\n");
  printf("    60 - BDRF Direct\n");
  printf("    70 - BDRF MCS Parameters\n");
  printf("    80 - MCS connection\n");
  printf("  +100 - Use Luminance based PCS adjustment\n");
  printf(" +1000 - Use V5 sub-profile if present\n");
}

//===================================================

static bool ParseIntArg(const char *arg, int minValue, int maxValue, int &value)
{
  char *end = NULL;
  long parsed;

  if (!arg || !*arg)
    return false;

  errno = 0;
  parsed = strtol(arg, &end, 10);

  if (errno == ERANGE || end == arg || *end != '\0' ||
      parsed < minValue || parsed > maxValue ||
      parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }

  value = (int)parsed;
  return true;
}

static bool ParseFloatArg(const char *arg, icFloatNumber &value)
{
  char *end = NULL;
  double parsed;

  if (!arg || !*arg)
    return false;

  errno = 0;
  parsed = strtod(arg, &end);

  if (errno == ERANGE || end == arg || *end != '\0' || !std::isfinite(parsed)) {
    return false;
  }

  value = (icFloatNumber)parsed;
  return std::isfinite(value);
}

//===================================================

typedef std::vector<CIccProfile*> IccProfilePtrList;

// The tool owns every -PCC profile it opens until cmm.Begin() has consumed the
// connection conditions; they are tracked in pccList and must be released on
// every exit path.  Centralizing the release here keeps the early-return error
// paths from stranding them (#1336).
static void releasePccList(IccProfilePtrList& pccList)
{
  for (IccProfilePtrList::iterator pcc = pccList.begin(); pcc != pccList.end(); ++pcc) {
    delete *pcc;
  }
  pccList.clear();
}

int main(int argc, icChar* argv[])
{
  int minargs = 10; // minimum number of arguments
  if(argc<minargs) {
    Usage();
    return 0;
  }

  int nNumProfiles, temp;
  temp = argc - minargs;

  //remaining arguments must be in pairs
  if(temp%2 != 0) {
    printf("\nMissing arguments!\n");
    Usage();
    return -1;
  }

  nNumProfiles = temp/2;

  int nLinkType = 0;
  if (!ParseIntArg(argv[2], 0, 1, nLinkType)) {
    printf("Invalid link_type '%s': expected 0 (Device Link) or 1 (.cube text file)\n", argv[2]);
    return 1;
  }

  ILinkWriter* pLinkWriter;
  if (nLinkType == 1) {
    pLinkWriter = new CCubeWriter();
  }
  else {
    pLinkWriter = new CDevLinkWriter();
  }

  if (!pLinkWriter) {
    printf("Unable to allocate writer\n");
    return -1;
  }

  LinkWriterPtr pWriter(pLinkWriter);

  pWriter->setFile(argv[1]);

  int nLutSize = 0;
  if (!ParseIntArg(argv[3], 2, 255, nLutSize)) {
    printf("Invalid LUT size '%s': expected an integer between 2 and 255\n", argv[3]);
    return EXIT_FAILURE;
  }
  
  pWriter->setLutSize(nLutSize);

  int nOption = 0;
  if (nLinkType == 0) {
    if (!ParseIntArg(argv[4], 0, 1, nOption)) {
      printf("Invalid option '%s': DeviceLink option must be 0 (v4) or 1 (v5)\n", argv[4]);
      return 1;
    }
  }
  else {
    if (!ParseIntArg(argv[4], 0, 20, nOption)) {
      printf("Invalid option '%s': .cube precision must be between 0 and 20\n", argv[4]);
      return 1;
    }
  }
  pWriter->setOption(nOption);

  pWriter->setTitle(argv[5]);

  icFloatNumber loRange;
  icFloatNumber hiRange;
  if (!ParseFloatArg(argv[6], loRange) || !ParseFloatArg(argv[7], hiRange)) {
    printf("Invalid input range: range_min and range_max must be finite numbers\n");
    return 1;
  }
  icFloatNumber sizeRange = hiRange - loRange;
  PrintRangeDiagnostic(loRange, hiRange, sizeRange);
  if (!std::isfinite(loRange) || !std::isfinite(hiRange) || !std::isfinite(sizeRange)) {
    printf("Invalid input range: range_min and range_max must be finite numbers\n");
    return 1;
  }
  pWriter->setInputRange(loRange, hiRange);

  //Retrieve command line arguments
  int nFirstTransform = 0;
  if (!ParseIntArg(argv[8], 0, 1, nFirstTransform)) {
    printf("Invalid first_transform '%s': expected 0 or 1\n", argv[8]);
    return 1;
  }
  bool bFirstTransform = nFirstTransform != 0;
  int nInterpVal = 0;
  if (!ParseIntArg(argv[9], 0, 1, nInterpVal)) {
    printf("Invalid interp '%s': expected 0 (linear) or 1 (tetrahedral)\n", argv[9]);
    return 1;
  }
  icXformInterp nInterp = (nInterpVal == 0) ? icInterpLinear : icInterpTetrahedral;

  int nIntent, nType, nLuminance;

  //Allocate a CIccCmm to use to apply profiles. 
  //Let profiles determine starting and ending color spaces.
  //Third argument indicates that Input transform from first profile should be used.
  CIccCmm theCmm(icSigUnknownData, icSigUnknownData, bFirstTransform);
  
  //PCC profiles need to stay around until the CMM has been completely initialized to apply transforms.  
  //TheCmm doesn't own them so keep a list so they can be released when they aren't needed any more.
  IccProfilePtrList pccList;

  int i, nCount;
  bool bUseD2BxB2DxTags;
  icStatusCMM stat;       //status variable for CMM operations
  icCmmEnvSigMap sigMap;  //Keep track of CMM Environment for each profile
  bool bUseSubProfile;

  //Remaining arguments define a sequence of profiles to be applied.  
  //Add them to theCmm one at a time providing CMM environment variables and PCC overrides as provided.
  for(i = 0, nCount=minargs; i<nNumProfiles; i++, nCount+=2) {
#if defined(_WIN32) || defined(_WIN64)
    if (!strnicmp(argv[nCount], "-ENV:", 5)) {  //check for -ENV: to allow for CMM Environment variables to be defined for next transform
#else
    if (!strncasecmp(argv[nCount], "-ENV:", 5)) {
#endif
      icSignature sig = icGetSigVal(argv[nCount]+5);
      icFloatNumber val;
      if (!ParseFloatArg(argv[nCount+1], val)) {
        printf("Invalid environment value '%s' for %s: expected a finite number\n", argv[nCount+1], argv[nCount]);
        releasePccList(pccList);
        return 1;
      }

      sigMap[sig]=val;
    }
    else if (stricmp(argv[nCount], "-PCC")) { //Attach profile while ignoring -PCC (this are handled below as profiles are attached)
      bUseD2BxB2DxTags = true;
      if (!ParseIntArg(argv[nCount+1], INT_MIN, INT_MAX, nIntent)) {
        printf("Invalid rendering intent '%s': expected an integer code\n", argv[nCount+1]);
        releasePccList(pccList);
        return 1;
      }
      bUseSubProfile = (nIntent / 1000) > 0;
      nIntent = nIntent % 1000;
      nLuminance = nIntent / 100;
      nIntent = nIntent % 100;
      nType = abs(nIntent) / 10;
      nIntent = nIntent % 10;
      CIccProfile *pPccProfile = NULL;

      //Adjust type and hint information based on rendering intent
      CIccCreateXformHintManager Hint;
      switch(nType) {
      case 1:
        nType = 0;
        bUseD2BxB2DxTags = false;
        break;
      case 4:
        nType = 0;
        Hint.AddHint(new CIccApplyBPCHint());
        break;
      default:
        break;
      }
      
      if (nIntent < (int)icPerceptual || nIntent > (int)icAbsoluteColorimetric) {
        printf("Invalid rendering intent '%s': decoded intent is out of range\n", argv[nCount+1]);
        releasePccList(pccList);
        return 1;
      }
      
      if (nType < (int)icXformLutMinimum || nType > (int)icXformLutMaximum) {
        printf("Invalid rendering intent '%s': decoded transform type is out of range\n", argv[nCount+1]);
        releasePccList(pccList);
        return 1;
      }

      if (nLuminance) {
        Hint.AddHint(new CIccLuminanceMatchingHint());
      }

      // Use of following -PCC arg allows for profile connection conditions to be defined
      if (i+1<nNumProfiles && !stricmp(argv[nCount+2], "-PCC")) {  
        pPccProfile = OpenIccProfile(argv[nCount+3]);
        if (!pPccProfile) {
          printf("Unable to open Profile Connections Conditions from '%s'\n", argv[nCount+3]);
          // Free any -PCC profiles opened on earlier loop iterations (#1336).
          releasePccList(pccList);
          return -1;
        }
        //Keep track of pPccProfile for until after cmm.Begin is called
        pccList.push_back(pPccProfile);
      }

     
      //CMM Environment variables are passed in as a Hint to the Xform associated with the profile
      if (sigMap.size()>0) {
        Hint.AddHint(new CIccCmmEnvVarHint(sigMap));
      }

      //Read profile from path and add it to theCmm
      CIccProfile* pXformProfile = ReadIccProfile(argv[nCount], bUseSubProfile); //We need all tags in profile for providing information to link
      stat = theCmm.AddXform(pXformProfile, nIntent<0 ? icUnknownIntent : (icRenderingIntent)nIntent, nInterp, pPccProfile,
                              (icXformLutType)nType, bUseD2BxB2DxTags, &Hint);
      if (stat) {
        // AddXform can fail for reasons that have nothing to do with the
        // profile itself.  In particular icCmmStatBadSpaceLink (2) means the
        // profile's source color space does not connect to the output space
        // of the previous transform in the chain (CIccCmm alternates the
        // input/output direction of successive profiles based on the
        // first_transform argument, so the connecting spaces depend on chain
        // position and direction, not just the profile).  The old message
        // here ("Invalid Profile(N)") reported any failure as an invalid
        // profile, which misled users when a structurally valid profile was
        // simply chained incompatibly - see issue #1322.  Decode the status
        // with CIccCmm::GetStatusText so the real cause is visible.
        printf("Error - Unable to add '%s' to transform chain (status %d: %s)\n", argv[nCount], stat, CIccCmm::GetStatusText(stat));
        if (stat == icCmmStatBadSpaceLink) {
          printf("The profile's color spaces do not connect with the previous transform in the chain.\n");
        }
        // AddXform already owns/frees pXformProfile on failure (#1327); the
        // tool still owns the accumulated -PCC profiles, so release them before
        // bailing out instead of leaking them (#1336).
        releasePccList(pccList);
        return -1;
      }
      sigMap.clear();

    }
  }

  //All profiles have been added to CMM.  Tell CMM that we are ready to begin applying colors/pixels
  if((stat=theCmm.Begin())) {
    // Begin() walks the assembled xform list and connects/optimizes the
    // transforms; it is where cross-profile incompatibilities that AddXform
    // could not see (PCS conversion setup, connection conditions) surface.
    // Decode the status code into text (issue #1322 follow-through) instead
    // of printing a bare number.
    printf("Error - Unable to begin profile application (status %d: %s) - Possibly invalid or incompatible profiles\n", stat, CIccCmm::GetStatusText(stat));
    // Begin() failed after the chain was assembled; the -PCC profiles are still
    // tool-owned at this point, so release them rather than leak them (#1336).
    releasePccList(pccList);
    return -1;
  }

  pWriter->setCmm(&theCmm);

  //Now that Begin() has consumed the connection conditions we can release the
  //-PCC profile nodes.
  releasePccList(pccList);

  if (!pWriter->begin(theCmm.GetSourceSpace(), theCmm.GetDestSpace())) {
    printf("Unable to begin writing LUT\n");
      return -1;
  }

  //Get and validate the source color space from the Cmm.
  icColorSpaceSignature SrcspaceSig = theCmm.GetSourceSpace();
  int nSrcSamples = icGetSpaceSamples(SrcspaceSig);

  //Get and validate the destination color space from theCmm.
  icColorSpaceSignature DestspaceSig = theCmm.GetDestSpace();
  int nDestSamples = icGetSpaceSamples(DestspaceSig);

  if (nSrcSamples <= 0 || nDestSamples <= 0) {
    printf("Invalid CMM channel count: source samples=%d destination samples=%d\n", nSrcSamples, nDestSamples);
    return 1;
  }
  
  size_t lutCount = 1;
  size_t lutLimit = (((1ULL<<32)-1) / nLutSize);     // limit to 4 Gig(32 bits) instead of size_t(64 bits)
  for (auto si = 0; si < nSrcSamples; si++) {
    if (lutCount > lutLimit ) {    // avoid overflow
      printf("LUT size too large\n");
      return -1;
    }
    lutCount *= nLutSize;
  }

  // Avoid vector(size, value) here: libstdc++ 15's fill_n countdown trips
  // -fsanitize=integer on its internal unsigned zero-crossing.
  std::vector<int> idx;
  std::vector<icFloatNumber> srcPixel;
  std::vector<icFloatNumber> dstPixel;
  idx.reserve(nSrcSamples);
  srcPixel.reserve(nSrcSamples);
  dstPixel.reserve(nDestSamples);
  for (int si = 0; si < nSrcSamples; si++) {
    idx.push_back(0);
    srcPixel.push_back(0.0f);
  }
  for (int si = 0; si < nDestSamples; si++) {
    dstPixel.push_back(0.0f);
  }

  // 2 <= nLutSize <= 255, so maxLUT cannot be zero
  icUInt32Number maxLut = nLutSize - 1;
  int lastPer = -1;
  
  int j = 0;
  // c indexes every grid point written to the device link and can reach
  // lutCount-1 (up to ~4 billion for large nSrcSamples/nLutSize), so it is
  // size_t rather than int -- an 'int' counter would itself overflow past
  // INT_MAX before the loop's idx[] rollover sets j<0 to terminate. j stays
  // int because it walks source channels back to -1 as the sentinel.
  for (size_t c = 0; j >= 0; c++) {

    for (auto si = 0; si < nSrcSamples; si++) {
      srcPixel[si] = sizeRange * (icFloatNumber)idx[si] / maxLut + loRange;
    }

    //Use CMM to convert SrcPixel to DestPixel
    theCmm.Apply(&dstPixel[0], &srcPixel[0]);

    pWriter->setNextNode(&dstPixel[0]);

    for (j = nSrcSamples - 1; j >= 0;) {
      idx[j]++;
      if (idx[j] >= nLutSize) {
        idx[j] = 0;
        j--;
      }
      else
        break;
    }
 
    //Display status of how much we have accomplished
    if (lutCount > 0) {     // explicit check to avoid divide by zero
        int curPer = GetProgressPercent(c + 1, lutCount);
        if (curPer != lastPer) {
          printf("\r%d%%", curPer);
          fflush(stdout);
          lastPer = curPer;
        }
    }
  }

  if (pWriter->finish()) {
    printf("\nLUT successfully written to '%s'\n", argv[1]);
  }
  else {
    printf("\nUnable to write LUT to '%s'\n", argv[1]);
    return -1;
  }

  return 0;
}
