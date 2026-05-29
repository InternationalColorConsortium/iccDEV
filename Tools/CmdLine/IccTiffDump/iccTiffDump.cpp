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
  {UNKNOWNID,        "Unknown"},
};

IdList compression_types[] = {
  {COMPRESSION_NONE,         "None"},
  {COMPRESSION_LZW,          "LZW"},
  {COMPRESSION_JPEG,         "JPEG"},
  {COMPRESSION_DEFLATE,      "Deflate"},
  {COMPRESSION_ADOBE_DEFLATE,"Deflate"},
  {UNKNOWNID,                "Unknown"},
};

const char* GetId(unsigned long nId, IdList* pIdList)
{
  for (;pIdList->nId != nId && pIdList->nId != UNKNOWNID; pIdList++);
  return pIdList->szName;
}

static FILE* icOpenWriteBinaryFile(const char* szFname);

static bool WriteEmbeddedIccProfile(const char* szFname, const unsigned char *pProfMem, unsigned int nLen)
{
  if (!pProfMem || !nLen)
    return false;

  FILE *fp = icOpenWriteBinaryFile(szFname);
  if (!fp)
    return false;

  bool failed = (fwrite(pProfMem, 1, nLen, fp) != nLen);
  if (!icFlushAndClose(fp))
    failed = true;

  return !failed;
}

void Usage() 
{
  printf("iccTiffDump built with IccProfLib version " ICCPROFLIBVER "\n\n");
  printf("Usage: iccTiffDump tiff_file {exported_icc_file}\n\n");
}

void DumpProfileInfo(CIccProfile* pProfile, std::string prefix)
{
  icHeader* pHdr = &pProfile->m_Header;
  CIccInfo Fmt;

  printf("%sVersion:          %s\n", prefix.c_str(), Fmt.GetVersionName(pHdr->version));

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
        DumpProfileInfo(pEmbeddedTag->GetProfile(), prefix + " ");
      }
    }
  }
}

//===================================================

static
FILE* icOpenWriteBinaryFile(const char* szFname)
{
  return icOpenRegularWriteBinaryFile(szFname);
}

//===================================================

int main(int argc, icChar* argv[])
{
  int minargs = 1;
  if (argc <= minargs) {
    Usage();
    return 0;
  }

  std::string srcName = icSanitizeConsoleText(argv[1]);

  CTiffImg SrcImg;
  if (!SrcImg.Open(argv[1])) {
    printf("\nFile [%s] cannot be opened.\n", srcName.c_str());
    return -1;
  }

  printf("-------------------->Tiff Image Dump<---------------------------\n");
  printf("Filename:          %s\n", srcName.c_str());
  printf("Size:              (%d x %d) pixels, (%.2lf\" x %.2lf\")\n",
    SrcImg.GetWidth(), SrcImg.GetHeight(),
    SrcImg.GetWidthIn(), SrcImg.GetHeightIn());
  printf("Planar:            %s\n", GetId(SrcImg.GetPlanar(), planar_types));
  printf("BitsPerSample:     %d\n", SrcImg.GetBitsPerSample());
  printf("SamplesPerPixel:   %d\n", SrcImg.GetSamples());
  int nExtra = SrcImg.GetExtraSamples();
  if (nExtra)
    printf("ExtraSamples:      %d\n", nExtra);
  printf("Photometric:       %s\n", GetId(SrcImg.GetPhoto(), photo_types));
  printf("BytesPerLine:      %d\n", SrcImg.GetBytesPerLine());
  printf("Resolution:        (%lf x %lf) pixels per/inch\n", SrcImg.GetXRes(), SrcImg.GetYRes());
  printf("Compression:       %s\n", GetId(SrcImg.GetCompress(), compression_types));

  unsigned char *pProfMem = nullptr;
  unsigned int nLen = 0;
  if (SrcImg.GetIccProfile(pProfMem, nLen)) {
    printf("Profile:           Embedded\n");

    // Profile description and metadata
    CIccProfile *pProfile = OpenIccProfile(pProfMem, nLen);
    if (pProfile) {
      DumpProfileInfo(pProfile, " ");
      if (argc > 2) {
        std::string dstName = icSanitizeConsoleText(argv[2]);
        if (pProfile->ReadTags(pProfile) && SaveIccProfile(argv[2], pProfile)) {
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
    }
    else if (argc > 2) {
      std::string dstName = icSanitizeConsoleText(argv[2]);
      if (WriteEmbeddedIccProfile(argv[2], pProfMem, nLen)) {
        printf("ICC profile bytes saved to: %s\n", dstName.c_str());
      }
      else {
        fprintf(stderr, "Failed to write ICC profile to %s\n", dstName.c_str());
        SrcImg.Close();
        return -1;
      }
    }
  } else {
    printf("Profile:           None\n");
  }

  SrcImg.Close();
  return 0;
}
