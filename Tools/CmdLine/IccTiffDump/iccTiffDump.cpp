/*
    File:       iccTiffDump.cpp

    Contains:   Console app display info about Tiff file and its ICC profile

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2010 The International Color Consortium. All rights 
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
// -Fix Saving to Icc Profile by David Hoyt 16-APR-2025
//////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <string>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccApplyBPC.h"
#include "TiffImg.h"
#include "../IccCmdLineUtil.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct {
  unsigned long nId;
  char const * const szName;
} IdList;
#define UNKNOWNID 0xffffffff

IdList planar_types[] = {
  {0,  "Interleaved samples"},
  {PLANARCONFIG_CONTIG,  "Interleaved samples"},
  {PLANARCONFIG_SEPARATE, "Samples in separate planes"},
  {UNKNOWNID, "Unknown"},
};

IdList photo_types[] = {
  {PHOTO_MINISWHITE, "Min Is White"},
  {PHOTO_MINISBLACK, "Min Is Black"},
  {PHOTO_CIELAB,     "CIELab"},
  {PHOTO_ICCLAB,     "IccLab"},
  {PHOTO_RGB,        "RGB"},
  {PHOTO_PALETTE,    "Palette"},
  {UNKNOWNID,        "Unknown"},
};

IdList compression_types[] = {
  {COMPRESSION_NONE,         "None"},
  {COMPRESSION_LZW,          "LZW"},
  {COMPRESSION_JPEG,         "JPEG"},
  {COMPRESSION_PACKBITS,     "PackBits"},
  {COMPRESSION_DEFLATE,      "Deflate"},
  {COMPRESSION_ADOBE_DEFLATE,"Deflate"},
  {UNKNOWNID,                "Unknown"},
};

const char* GetId(unsigned long nId, IdList* pIdList)
{
  for (;pIdList->nId != nId && pIdList->nId != UNKNOWNID; pIdList++);
  return pIdList->szName;
}

void Usage()
{
  printf("iccTiffDump built with IccProfLib version " ICCPROFLIBVER "\n\n");
  printf("Usage: iccTiffDump {--evidence-json} tiff_file {exported_icc_file}\n\n");
}

static std::string GetProfileId(CIccProfile* pProfile)
{
  if (!pProfile)
    return std::string();

  CIccInfo Fmt;
  if (Fmt.IsProfileIDCalculated(&pProfile->m_Header.profileID))
    return Fmt.GetProfileID(&pProfile->m_Header.profileID);

  return std::string();
}

static std::string GetProfileId(const char* profilePath)
{
  if (!profilePath || !profilePath[0])
    return std::string();

  CIccProfile* pProfile = OpenIccProfile(profilePath);
  if (!pProfile)
    return std::string();

  std::string id = GetProfileId(pProfile);
  delete pProfile;
  return id;
}

static void EmitWriteEvidenceJson(const char* tiffPath, const char* outputPath,
                                  const std::string& embeddedProfileId,
                                  const std::string& extractedProfileId)
{
  std::string outputDigest;
  bool hasOutputDigest = outputPath && outputPath[0] &&
    icSha256File(outputPath, outputDigest);

  printf("{");
  printf("\"schema\":\"iccdev-qa-evidence/v1\",");
  printf("\"tool\":\"iccTiffDump\",");
  printf("\"input\":\"%s\",", icJsonEscape(tiffPath).c_str());
  printf("\"output\":\"%s\",", icJsonEscape(outputPath).c_str());
  printf("\"qaFlags\":[\"ICCDEV_FLAG_WRITE\"],");
  if (hasOutputDigest)
    printf("\"outputDigest\":\"%s\",", outputDigest.c_str());
  else
    printf("\"outputDigest\":null,");
  if (!embeddedProfileId.empty())
    printf("\"embeddedProfileId\":\"%s\",", icJsonEscape(embeddedProfileId).c_str());
  else
    printf("\"embeddedProfileId\":null,");
  if (!extractedProfileId.empty())
    printf("\"extractedProfileId\":\"%s\",", icJsonEscape(extractedProfileId).c_str());
  else
    printf("\"extractedProfileId\":null,");
  printf("\"write\":{");
  if (hasOutputDigest)
    printf("\"outputDigest\":\"%s\",", outputDigest.c_str());
  else
    printf("\"outputDigest\":null,");
  if (!embeddedProfileId.empty())
    printf("\"embeddedProfileId\":\"%s\",", icJsonEscape(embeddedProfileId).c_str());
  else
    printf("\"embeddedProfileId\":null,");
  if (!extractedProfileId.empty())
    printf("\"extractedProfileId\":\"%s\"", icJsonEscape(extractedProfileId).c_str());
  else
    printf("\"extractedProfileId\":null");
  printf("}}\n");
}

void DumpProfileInfo(CIccProfile* pProfile, std::string prefix, int level = 1)
{
  icHeader* pHdr = &pProfile->m_Header;
  CIccInfo Fmt;
  const int profileRecursionLimit = 4;
  const size_t bufSize = 64;
  char buf[bufSize];

  if (level > profileRecursionLimit) {
    printf("%sSubprofile recursion halted\n", prefix.c_str());
    return;
  }

  printf("%sVersion:          %s\n", prefix.c_str(), Fmt.GetVersionName(pHdr->version));

  printf("%sClass:            %s\n", prefix.c_str(), Fmt.GetProfileClassSigName(pHdr->deviceClass) );
  if (pHdr->deviceSubClass)
    printf("%sSubClass:         %s\n", prefix.c_str(), icGetSig(buf, bufSize, pHdr->deviceSubClass));
  if (pHdr->colorSpace)
    printf("%sColor Space:      %s\n", prefix.c_str(), Fmt.GetColorSpaceSigName(pHdr->colorSpace));
  if (pHdr->pcs)
    printf("%sColorimetric PCS: %s\n", prefix.c_str(), Fmt.GetColorSpaceSigName(pHdr->pcs));
  if (pHdr->spectralPCS) {
    printf("%sSpectral PCS:     %s\n", prefix.c_str(), Fmt.GetSpectralColorSigName(pHdr->spectralPCS));
    if (pHdr->spectralRange.start || pHdr->spectralRange.end || pHdr->spectralRange.steps) {
      printf("%sSpectral Range:   start=%.1fnm, end=%.1fnm, steps=%d\n", prefix.c_str(),
        icF16toF(pHdr->spectralRange.start),
        icF16toF(pHdr->spectralRange.end),
        pHdr->spectralRange.steps);
    }
    if (pHdr->biSpectralRange.start || pHdr->biSpectralRange.end || pHdr->biSpectralRange.steps) {
      printf("%sBiSpectral Range: start=%.1fnm, end=%.1fnm, steps=%d\n", prefix.c_str(),
        icF16toF(pHdr->biSpectralRange.start),
        icF16toF(pHdr->biSpectralRange.end),
        pHdr->biSpectralRange.steps);
    }
  }

  CIccTag* pDesc = pProfile->FindTag(icSigProfileDescriptionTag);
  if (pDesc) {
    if (pDesc->GetType() == icSigTextDescriptionType) {
      CIccTagTextDescription* pText = (CIccTagTextDescription*)pDesc;
      std::string desc = icSanitizeConsoleText(pText->GetText());
      printf("%sDescription:      %s\n", prefix.c_str(), desc.c_str());
    }
    else if (pDesc->GetType() == icSigMultiLocalizedUnicodeType) {
      CIccTagMultiLocalizedUnicode* pStrs = (CIccTagMultiLocalizedUnicode*)pDesc;
      if (pStrs->m_Strings) {
        CIccMultiLocalizedUnicode::iterator text = pStrs->m_Strings->begin();
        if (text != pStrs->m_Strings->end()) {
          std::string line;
          text->GetText(line);
          std::string desc = icSanitizeConsoleText(line);
          printf("%sDescription:      %s\n", prefix.c_str(), desc.c_str());
        }
      }
    }
  }
  
  CIccTag* pEmbedded = pProfile->FindTag(icSigEmbeddedV5ProfileTag);
  if (pEmbedded) {
    // if it has a different type, don't try to dereference it!
    if (pEmbedded->GetType() == icSigEmbeddedProfileType) {
      CIccTagEmbeddedProfile* pEmbeddedTag = (CIccTagEmbeddedProfile*)pEmbedded;
      if (pEmbeddedTag->GetProfile()) {
        printf("%sSub-Profile:      Embedded\n", prefix.c_str());
        DumpProfileInfo(pEmbeddedTag->GetProfile(), prefix + " ", level+1 );
      }
    }
  }
}

//===================================================

static
const char *GetSampleFormatDescription( unsigned int format )
{
  const int bufSize = 256;
  static char buf[ bufSize ];

  switch( format ) {
    case SAMPLEFORMAT_UINT:
      return "unsigned integer";
      break;
    case SAMPLEFORMAT_INT:
      return "signed integer";
      break;
    case SAMPLEFORMAT_IEEEFP:
      return "floating point";
      break;
    case SAMPLEFORMAT_VOID:
      return "undefined";
      break;
    case SAMPLEFORMAT_COMPLEXINT:
      return "complex integer";
      break;
    case SAMPLEFORMAT_COMPLEXIEEEFP:
      return "complex floating point";
      break;
    default:
      snprintf(buf,bufSize,"Unknown format: %u = 0x%8.8X", format, format );
      return buf;
      break;
  }

// unreachable
}

//===================================================

int main(int argc, icChar* argv[])
{
  int minargs = 1;
  bool bEvidenceJson = false;

  if (argc > 1 && !stricmp(argv[1], "--evidence-json")) {
    bEvidenceJson = true;
    argv++;
    argc--;
  }

  if (argc <= minargs) {
    Usage();
    return 0;
  }
  else if (argc > 3) {
    Usage();
    return -1;
  }

  std::string srcName = icSanitizeConsoleText(argv[1]);

  CTiffImg SrcImg;
  if (!SrcImg.Open(argv[1])) {
    printf("\nFile [%s] cannot be opened.\n", srcName.c_str());
    return -1;
  }

  if (!bEvidenceJson) {
    printf("-------------------->Tiff Image Dump<---------------------------\n");
    printf("Filename:          %s\n", srcName.c_str());
    printf("Size:              (%d x %d) pixels, (%.2lf\" x %.2lf\")\n",
      SrcImg.GetWidth(), SrcImg.GetHeight(),
      SrcImg.GetWidthIn(), SrcImg.GetHeightIn());
    printf("Planar:            %s\n", GetId(SrcImg.GetPlanar(), planar_types));
    printf("BitsPerSample:     %d (%s)\n", SrcImg.GetBitsPerSample(),
           GetSampleFormatDescription(SrcImg.GetSampleFormat()));
    printf("SamplesPerPixel:   %d\n", SrcImg.GetSamples());
  }
  int nExtra = SrcImg.GetExtraSamples();
  if (!bEvidenceJson && nExtra)
    printf("ExtraSamples:      %d\n", nExtra);
  if (!bEvidenceJson) {
    printf("Photometric:       %s\n", GetId(SrcImg.GetPhoto(), photo_types));
    printf("BytesPerLine:      %d\n", SrcImg.GetBytesPerLine());
    printf("Resolution:        (%lf x %lf) pixels per/inch\n", SrcImg.GetXRes(), SrcImg.GetYRes());
    printf("Compression:       %s\n", GetId(SrcImg.GetCompress(), compression_types));
  }

  unsigned char *pProfMem = nullptr;
  unsigned int nLen = 0;
  std::string embeddedProfileId;
  std::string extractedProfileId;
  if (SrcImg.GetIccProfile(pProfMem, nLen)) {
    if (!bEvidenceJson)
      printf("Profile:           Embedded\n");

    // Profile description and metadata
    CIccProfile *pProfile = OpenIccProfile(pProfMem, nLen);
    if (!pProfile) {
      printf("\nUnable to open embedded ICC profile\n");
      SrcImg.Close();
      return 1;
    }

    std::string validateReport;
    if (!pProfile->ReadTags(pProfile)) {
      printf("\nUnable to read embedded ICC profile\n");
      delete pProfile;
      SrcImg.Close();
      return 1;
    }
    else if (pProfile->Validate(validateReport) > icValidateWarning) {
      printf("\nEmbedded ICC profile violates the ICC specification:\n%s",
             validateReport.c_str());
      delete pProfile;
      SrcImg.Close();
      return 1;
    }

    embeddedProfileId = GetProfileId(pProfile);
    if (!bEvidenceJson)
      DumpProfileInfo(pProfile, " ");
    if (argc > 2) {
      std::string dstName = icSanitizeConsoleText(argv[2]);
      if (SaveIccProfile(argv[2], pProfile)) {
        extractedProfileId = GetProfileId(argv[2]);
        if (!bEvidenceJson)
          printf("\nProfile extracted to: %s\n", dstName.c_str());
      }
      else {
        printf("\nUnable to extract profile\n");
        delete pProfile;
        SrcImg.Close();
        return -1;
      }
    }
    delete pProfile;
  } else {
    if (!bEvidenceJson)
      printf("Profile:           None\n");
    if (argc > 2) {
      if (bEvidenceJson)
        fprintf(stderr, "No embedded ICC profile to extract\n");
      else
        printf("\nNo embedded ICC profile to extract\n");
      SrcImg.Close();
      return -1;
    }
  }

  if (bEvidenceJson)
    EmitWriteEvidenceJson(argv[1], argc > 2 ? argv[2] : "",
                          embeddedProfileId, extractedProfileId);

  SrcImg.Close();
  return 0;
}
