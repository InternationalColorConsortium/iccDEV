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
#include <cerrno>
#include <string>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccApplyBPC.h"
#include "TiffImg.h"
#include "IccCmdLineUtil.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
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

// TIFFTAG_RESOLUTIONUNIT was never reported, so every image was printed as though its
// resolution were counted in inches.  A centimetre-based file therefore read as 2.54x
// its real physical size and the label actively contradicted the file (#2220).  The
// inch wording is kept verbatim so existing output and any log grep for it still match.
IdList resolution_units[] = {
  {RESUNIT_NONE,       "(relative, no absolute unit)"},
  {RESUNIT_INCH,       "pixels per/inch"},
  {RESUNIT_CENTIMETER, "pixels per/centimeter"},
  {UNKNOWNID,          "pixels per/unrecognized unit"},
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

static bool IsRegularOutputDestination(const char* szFname)
{
  if (!szFname || !szFname[0])
    return false;

#if defined(_WIN32)
  DWORD attributes = GetFileAttributesA(szFname);
  if (attributes != INVALID_FILE_ATTRIBUTES) {
    const DWORD rejectedAttributes = FILE_ATTRIBUTE_DEVICE |
                                     FILE_ATTRIBUTE_DIRECTORY |
                                     FILE_ATTRIBUTE_REPARSE_POINT;
    return !(attributes & rejectedAttributes);
  }
  DWORD error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
#else
  struct stat st;
  if (lstat(szFname, &st) == 0)
    return S_ISREG(st.st_mode);
  return errno == ENOENT;
#endif
}

static bool WriteEmbeddedIccProfile(const char* szFname,
                                    const unsigned char *pProfMem,
                                    unsigned int nLen)
{
  if (!pProfMem || !nLen || !IsRegularOutputDestination(szFname))
    return false;

  std::string tempName;
  FILE *fp = NULL;
  int fd = -1;
  unsigned int attempt;

  for (attempt = 0; attempt < 100; attempt++) {
    char suffix[64];
#if defined(_WIN32)
    snprintf(suffix, sizeof(suffix), ".tmp-%ld-%u", (long)_getpid(), attempt);
    tempName = std::string(szFname) + suffix;
    fd = _open(tempName.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
               _S_IREAD | _S_IWRITE);
    if (fd >= 0)
      fp = _fdopen(fd, "wb");
#else
    snprintf(suffix, sizeof(suffix), ".tmp-%ld-%u", (long)getpid(), attempt);
    tempName = std::string(szFname) + suffix;
    fd = open(tempName.c_str(), O_WRONLY | O_CREAT | O_EXCL,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd >= 0)
      fp = fdopen(fd, "wb");
#endif
    if (fp)
      break;
    if (fd >= 0) {
#if defined(_WIN32)
      _close(fd);
#else
      close(fd);
#endif
      remove(tempName.c_str());
      return false;
    }
    if (errno != EEXIST)
      return false;
  }

  if (!fp)
    return false;

  bool failed = fwrite(pProfMem, 1, nLen, fp) != nLen;
  if (!icFlushAndClose(fp))
    failed = true;

  if (failed) {
    remove(tempName.c_str());
    return false;
  }

  if (!IsRegularOutputDestination(szFname)) {
    remove(tempName.c_str());
    return false;
  }

#if defined(_WIN32)
  if (!MoveFileExA(tempName.c_str(), szFname,
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    remove(tempName.c_str());
    return false;
  }
#else
  if (rename(tempName.c_str(), szFname) != 0) {
    remove(tempName.c_str());
    return false;
  }
#endif

  return true;
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
  // ICCDEV_FLAG_WRITE asserts that a profile was written out.  Emitting it
  // unconditionally also claimed it for `iccTiffDump --evidence-json in.tif`,
  // which names no output and writes nothing, so anything counting the flag
  // across an evidence corpus over-reported.
  printf("\"qaFlags\":[%s],", hasOutputDigest ? "\"ICCDEV_FLAG_WRITE\"" : "");
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

  // ICCDEV_FLAG_WRITE carries its evidence in a nested object named for the
  // flag, the same shape iccApplyNamedCmm uses for ICCDEV_FLAG_TRANSFORM.  The
  // flat copies above stay for readers that index the document by bare key.
  // "write" closes the object, so its last member takes no separator.
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
    return 1;
  }

  std::string srcName = icSanitizeConsoleText(argv[1]);

  CTiffImg SrcImg;
  if (!SrcImg.Open(argv[1])) {
    fprintf(stderr, "\nFile [%s] cannot be opened.\n", srcName.c_str());
    return 1;
  }

  if (!bEvidenceJson) {
  printf("-------------------->Tiff Image Dump<---------------------------\n");
  printf("Filename:          %s\n", srcName.c_str());
  // A physical size only exists when RESOLUTIONUNIT names an absolute unit; under
  // RESUNIT_NONE the resolution values fix an aspect ratio and nothing else, so
  // GetWidthIn()/GetHeightIn() report 0 and printing inches would invent a measurement
  // the file never made.  They also convert from centimetres now, which the bare
  // division they used to do did not (#2220).
  const double dWidthIn = SrcImg.GetWidthIn();
  const double dHeightIn = SrcImg.GetHeightIn();
  if (dWidthIn > 0.0 && dHeightIn > 0.0)
    printf("Size:              (%d x %d) pixels, (%.2lf\" x %.2lf\")\n",
      SrcImg.GetWidth(), SrcImg.GetHeight(), dWidthIn, dHeightIn);
  else
    printf("Size:              (%d x %d) pixels\n",
      SrcImg.GetWidth(), SrcImg.GetHeight());
  printf("Planar:            %s\n", GetId(SrcImg.GetPlanar(), planar_types));
  printf("BitsPerSample:     %d (%s)\n", SrcImg.GetBitsPerSample(),
            GetSampleFormatDescription( SrcImg.GetSampleFormat()) );

  printf("SamplesPerPixel:   %d\n", SrcImg.GetSamples());
  // Report on tag PRESENCE, not on a nonzero count.  The old test printed nothing
  // when tag 338 was absent and nothing when it was stored as zero, and said nothing
  // at all about a directory libtiff had repaired: the tracked 81-band
  // Testing/hybrid/Data/smCows380_5_780.tif dumped "SamplesPerPixel: 81" and
  // "Photometric: Min Is Black" -- one colour channel -- with the other 80 channels
  // unmentioned, while libtiff's own warning went to stderr and named no count.
  // GetEffectiveExtraSamples() is what libtiff repaired the layout to, so state it
  // and say it was not stored, rather than leave the reader to reconcile the two
  // numbers themselves (#2386).
  //
  // The present-and-repaired half is now covered too.  Where tag 338 IS present and
  // libtiff repairs anyway, it overwrites the stored count in place and reports the
  // repaired figure through every getter it has, so this line used to be libtiff's
  // number presented as the file's: a 6-sample MinIsBlack image storing ExtraSamples
  // [0] printed "ExtraSamples: 5", byte-identical to an honest five-extra file.  Two
  // materially different directories were indistinguishable in the dump, which is the
  // half of #2386 left open by #2402.
  //
  // CTiffImg::Open() now reads tag 338's count field straight out of the IFD, so the
  // stored figure is available to compare -- see HasStoredExtraSamplesCount().  When
  // the two disagree, print both and name the repair; the reader needs the stored one
  // to know the file is malformed and the effective one to understand the layout the
  // rest of libtiff is using.  Reporting only: iccApplyProfiles still consumes the
  // repaired count, and which of the two SHOULD drive colour management is the
  // carrier-contract question open on #2379/#2385, deliberately not decided here.
  unsigned int nExtra = SrcImg.GetExtraSamples();
  unsigned int nEffectiveExtra = SrcImg.GetEffectiveExtraSamples();
  if (SrcImg.HasStoredExtraSamples()) {
    // Gated on the flag, not on a nonzero count: a stored 0 is a legitimate value, and
    // a false flag means the count could not be determined (BigTIFF, or an unreadable
    // file) rather than that the tag was absent.
    if (SrcImg.HasStoredExtraSamplesCount() &&
        SrcImg.GetStoredExtraSamplesCount() != nExtra)
      printf("ExtraSamples:      %u (file stores %u; libtiff repaired the layout)\n",
        nExtra, SrcImg.GetStoredExtraSamplesCount());
    else
      printf("ExtraSamples:      %u\n", nExtra);
  }
  else if (nEffectiveExtra)
    printf("ExtraSamples:      not stored; libtiff treats %u of %u samples as non-color\n",
      nEffectiveExtra, SrcImg.GetSamples());
  printf("Photometric:       %s\n", GetId(SrcImg.GetPhoto(), photo_types));
  printf("BytesPerLine:      %d\n", SrcImg.GetBytesPerLine());
  printf("Resolution:        (%lf x %lf) %s\n", SrcImg.GetXRes(), SrcImg.GetYRes(),
    GetId(SrcImg.GetResolutionUnit(), resolution_units));
  printf("Compression:       %s\n", GetId(SrcImg.GetCompress(), compression_types));
  }

  unsigned char *pProfMem = nullptr;
  unsigned int nLen = 0;
  std::string embeddedProfileId;
  std::string extractedProfileId;
  if (SrcImg.GetIccProfile(pProfMem, nLen)) {
    if (!bEvidenceJson) {
      printf("Profile:           Embedded\n");
      fflush(stdout);
    }

    if (argc > 2) {
      std::string dstName = icSanitizeConsoleText(argv[2]);
      if (!WriteEmbeddedIccProfile(argv[2], pProfMem, nLen)) {
        fprintf(stderr, "\nUnable to extract profile to: %s\n", dstName.c_str());
        SrcImg.Close();
        return 1;
      }
      // Only the evidence path consumes this, and it costs a second complete
      // CIccProfile parse of the file just written, so it stays inside the
      // guard rather than running on every ordinary extraction.
      if (bEvidenceJson)
        extractedProfileId = GetProfileId(argv[2]);
      if (!bEvidenceJson) {
        printf("\nProfile extracted byte-for-byte to: %s\n", dstName.c_str());
        fflush(stdout);
      }
    }

    // Profile description and metadata
    CIccProfile *pProfile = OpenIccProfile(pProfMem, nLen);
    if (!pProfile) {
      fprintf(stderr, "\nUnable to open embedded ICC profile\n");
      SrcImg.Close();
      return 1;
    }

    if (bEvidenceJson)
      embeddedProfileId = GetProfileId(pProfile);
    else
      DumpProfileInfo(pProfile, " ");

    std::string validateReport;
    if (!pProfile->ReadTags(pProfile)) {
      fprintf(stderr, "\nUnable to read embedded ICC profile\n");
      delete pProfile;
      SrcImg.Close();
      return 1;
    }
    else if (pProfile->Validate(validateReport) > icValidateWarning) {
      fprintf(stderr, "\nEmbedded ICC profile violates the ICC specification:\n%s",
              validateReport.c_str());
      delete pProfile;
      SrcImg.Close();
      return 1;
    }
    delete pProfile;
  } else {
    if (!bEvidenceJson)
      printf("Profile:           None\n");
    if (argc > 2) {
      fprintf(stderr, bEvidenceJson ? "No embedded ICC profile to extract\n" :
              "\nNo embedded ICC profile to extract\n");
      SrcImg.Close();
      return 1;
    }
  }

  if (bEvidenceJson)
    EmitWriteEvidenceJson(argv[1], argc > 2 ? argv[2] : "",
                          embeddedProfileId, extractedProfileId);

  SrcImg.Close();
  return 0;
}
