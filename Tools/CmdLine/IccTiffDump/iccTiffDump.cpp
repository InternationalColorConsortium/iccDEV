/*
    File:       iccTiffDump.cpp

    Contains:   Console app display info about Tiff file and its ICC profile

    Version:    V1

    Copyright:  (c) see below
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
// -Fix Saving to Icc Profile by David Hoyt 16-APR-2025
//////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <vector>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccApplyBPC.h"
#include "TiffImg.h"
#include "IccCmdLineUtil.h"
#include "IccSameFile.h"
#include "../IccTiffTelemetry.h"
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
  printf("Usage: iccTiffDump {--evidence-json} {--ifd N} {--list-directories} tiff_file {exported_icc_file}\n\n");
  printf("IFD indexes are zero-based main-chain directories. Listing does not extract or validate ICC data.\n");
  printf("Inspection is metadata-only; it does not assert pixel decoding or SVS compatibility.\n\n");
#ifdef ICCDEV_TELEMETRY
  CIccTiffTelemetry::Help(stdout);
#endif
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
                                  const std::string& extractedProfileId,
                                  CTiffImg &image)
{
  std::string outputDigest;
  bool hasOutputDigest = outputPath && outputPath[0] &&
    icSha256File(outputPath, outputDigest);

  printf("{");
  printf("\"schema\":\"iccdev-qa-evidence/v1\",");
  printf("\"tool\":\"iccTiffDump\",");
  printf("\"input\":\"%s\",", icJsonEscape(tiffPath).c_str());
  printf("\"directory\":%u,\"bigTiff\":%s,\"tiled\":%s,\"tileWidth\":%u,\"tileHeight\":%u,",
         image.GetDirectory(), image.IsBigTiff() ? "true" : "false",
         image.IsTiled() ? "true" : "false", image.GetTileWidth(), image.GetTileHeight());
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

#ifdef ICCDEV_TELEMETRY
struct TiffDumpTelemetryStats {
  explicit TiffDumpTelemetryStats(CIccTiffTelemetry &receipt) : telemetry(receipt) {}

  ~TiffDumpTelemetryStats()
  {
    if (started && telemetry.Active() && !completed)
      Record("run_failed", "\"status\":\"tool_failure\",\"exit_code\":1,\"stage\":" +
             CIccTiffTelemetry::JsonString(stage));
  }

  void SetStage(const char *value)
  {
    stage = value;
    telemetry.SetStage(value);
  }

  CIccTiffTelemetry &telemetry;
  const char *stage = "initialization";
  bool started = false;
  bool completed = false;
  bool writesOk = true;
  unsigned int preflightIfds = 0;
  unsigned int directoriesVisited = 0;
  unsigned int profilesParsed = 0;
  unsigned int profilesRead = 0;
  unsigned int profilesValidated = 0;
  unsigned long long profileBytes = 0;
  unsigned long long extractedBytes = 0;
  double metadataOpenMs = 0;
  double preflightMs = 0;
  double inventoryMs = 0;
  double extractionMs = 0;
  double profileParseMs = 0;
  double profileReadMs = 0;
  double profileValidateMs = 0;
  std::string geometry;

  std::string Fields() const
  {
    return "\"preflight_ifds\":" + std::to_string(preflightIfds) +
      ",\"directories_visited\":" + std::to_string(directoriesVisited) +
      ",\"profiles_parsed\":" + std::to_string(profilesParsed) +
      ",\"profiles_read\":" + std::to_string(profilesRead) +
      ",\"profiles_validated\":" + std::to_string(profilesValidated) +
      ",\"profile_bytes\":" + std::to_string(profileBytes) +
      ",\"extracted_bytes\":" + std::to_string(extractedBytes) +
      ",\"decoded_pixels\":0,\"decoded_bytes\":0" +
      ",\"metadata_open_ms\":" + CIccTiffTelemetry::Number(metadataOpenMs) +
      ",\"preflight_ms\":" + CIccTiffTelemetry::Number(preflightMs) +
      ",\"inventory_ms\":" + CIccTiffTelemetry::Number(inventoryMs) +
      ",\"extraction_ms\":" + CIccTiffTelemetry::Number(extractionMs) +
      ",\"profile_parse_ms\":" + CIccTiffTelemetry::Number(profileParseMs) +
      ",\"profile_read_ms\":" + CIccTiffTelemetry::Number(profileReadMs) +
      ",\"profile_validate_ms\":" + CIccTiffTelemetry::Number(profileValidateMs) +
      (geometry.empty() ? "" : "," + geometry);
  }

  void Record(const char *event, const std::string &fields = "")
  {
    if (telemetry.Active() && !telemetry.Emit(event, Fields() +
        (fields.empty() ? "" : "," + fields)))
      writesOk = false;
  }
};

static std::string TiffDumpGeometry(TIFF *tif, unsigned int directory)
{
  uint32_t width = 0, height = 0, tileWidth = 0, tileHeight = 0;
  uint16_t bits = 0, samples = 0, planar = 0, orientation = 0;
  uint16_t resolutionUnit = RESUNIT_INCH;
  float xResolution = 0, yResolution = 0;
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits);
  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
  TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
  TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resolutionUnit);
  TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xResolution);
  TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yResolution);
  const bool tiled = TIFFIsTiled(tif) != 0;
  if (tiled) {
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileHeight);
  }
  return "\"directory\":" + std::to_string(directory) +
    ",\"width\":" + std::to_string(width) +
    ",\"height\":" + std::to_string(height) +
    ",\"bits_per_sample\":" + std::to_string(bits) +
    ",\"samples_per_pixel\":" + std::to_string(samples) +
    ",\"planar\":" + std::to_string(planar) +
    ",\"layout\":" + CIccTiffTelemetry::JsonString(tiled ? "tiled" : "stripped") +
    ",\"tiled\":" + (tiled ? "true" : "false") +
    ",\"big_tiff\":" + (TIFFIsBigTIFF(tif) ? "true" : "false") +
    ",\"tile_width\":" + std::to_string(tileWidth) +
    ",\"tile_height\":" + std::to_string(tileHeight) +
    ",\"orientation\":" + std::to_string(orientation) +
    ",\"resolution_unit\":" + std::to_string(resolutionUnit) +
    ",\"x_resolution\":" + CIccTiffTelemetry::Number(xResolution) +
    ",\"y_resolution\":" + CIccTiffTelemetry::Number(yResolution);
}
#endif

static bool ListDirectories(const char *path, bool evidence
#ifdef ICCDEV_TELEMETRY
                            , CIccTiffTelemetry *telemetry,
                            TiffDumpTelemetryStats *stats
#endif
                            )
{
  // Open metadata only: inventory must not decode pixels or parse ICC payloads.
  CTiffImg image;
#ifdef ICCDEV_TELEMETRY
  if (telemetry)
    stats->SetStage("metadata_open");
  double phaseStart = telemetry ? CIccTiffTelemetry::Now() : 0;
#endif
  const bool opened = image.Open(path, true);
#ifdef ICCDEV_TELEMETRY
  if (telemetry) {
    stats->metadataOpenMs = CIccTiffTelemetry::Since(phaseStart);
    stats->Record("metadata_open", std::string("\"success\":") +
                  (opened ? "true" : "false"));
  }
#endif
  if (!opened)
    return false;
#ifdef ICCDEV_TELEMETRY
  if (telemetry) {
    stats->SetStage("inventory_preflight");
    phaseStart = CIccTiffTelemetry::Now();
  }
#endif
  // Validate the capped main-chain walk before emitting any inventory JSON.
  const unsigned int count = image.GetDirectoryCount();
#ifdef ICCDEV_TELEMETRY
  if (telemetry) {
    stats->preflightMs = CIccTiffTelemetry::Since(phaseStart);
    stats->preflightIfds = count;
    stats->Record("inventory_preflight",
                  std::string("\"directory_cap\":4096,\"success\":") +
                  (count && count <= 4096 ? "true" : "false"));
  }
#endif
  if (!count || count > 4096) {
    fprintf(stderr, "Cannot inventory TIFF directories (malformed chain or more than 4096 directories)\n");
    return false;
  }
#ifdef ICCDEV_TELEMETRY
  if (telemetry) {
    stats->SetStage("inventory");
    phaseStart = CIccTiffTelemetry::Now();
  }
#endif
  if (evidence)
    printf("{\"schema\":\"iccdev-tiff-inventory/v1\",\"directories\":[");
  TIFF *tif = image.GetTiffHandle();
  for (unsigned int i = 0; i < count; i++) {
    // TIFFSetDirectory(i) traverses the main IFD chain from its start. Walk
    // forward on the already open handle so listing many IFDs stays linear.
    if (i && !TIFFReadDirectory(tif))
      return false;
    uint32_t width = 0, height = 0, tileWidth = 0, tileHeight = 0;
    uint16_t photo = 0, bitsPerSample = 0, samplesPerPixel = 0;
    uint16_t planar = 0, sampleFormat = 0, orientation = 0, compression = 0;
    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height) ||
        !TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photo) ||
        !TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample))
      return false;
    const bool tiled = TIFFIsTiled(tif) != 0;
    if (tiled && (!TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth) ||
                  !TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileHeight)))
      return false;
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
    TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &compression);
    unsigned char *profile = NULL;
    unsigned int length = 0;
    TIFFGetField(tif, TIFFTAG_ICCPROFILE, &length, &profile);
    if (!profile)
      length = 0;
#ifdef ICCDEV_TELEMETRY
    if (telemetry) {
      stats->directoriesVisited++;
      stats->profileBytes += length;
      stats->geometry = TiffDumpGeometry(tif, i);
      stats->Record("directory_inspected",
                    "\"directory_profile_bytes\":" + std::to_string(length));
    }
#endif
    // SubIFDs are reported as opaque offsets, never recursively traversed.
    uint16_t subCount = 0;
    uint64_t *subOffsets = NULL;
    TIFFGetField(tif, TIFFTAG_SUBIFD, &subCount, &subOffsets);
    if (evidence) {
      printf("%s{\"directory\":%u,\"width\":%u,\"height\":%u,\"bigTiff\":%s,"
             "\"tiled\":%s,\"tileWidth\":%u,\"tileHeight\":%u,\"compression\":%u,"
             "\"codecAvailable\":%s,\"iccLength\":%u,\"bitsPerSample\":%u,"
             "\"samplesPerPixel\":%u,\"planar\":%u,\"sampleFormat\":%u,"
             "\"orientation\":%u,\"blockCount\":%u,\"subIfdOffsets\":[",
             i ? "," : "", i, width, height,
             TIFFIsBigTIFF(tif) ? "true" : "false", tiled ? "true" : "false",
             tileWidth, tileHeight, compression,
             TIFFIsCODECConfigured(compression) ? "true" : "false",
             length, bitsPerSample, samplesPerPixel, planar,
             sampleFormat, orientation,
             tiled ? TIFFNumberOfTiles(tif) : TIFFNumberOfStrips(tif));
      for (unsigned int s = 0; s < subCount; s++)
        printf("%s%llu", s ? "," : "", static_cast<unsigned long long>(subOffsets[s]));
      printf("]}");
    }
    else {
      printf("IFD %u: %u x %u, %s, %s, tile %u x %u, compression %u, ICC %u bytes\n",
             i, width, height, TIFFIsBigTIFF(tif) ? "BigTIFF" : "TIFF",
             tiled ? "tiled" : "stripped", tileWidth,
             tileHeight, compression, length);
      for (unsigned int s = 0; s < subCount; s++)
        printf("  SubIFD offset: %llu\n", static_cast<unsigned long long>(subOffsets[s]));
    }
  }
  if (evidence)
    printf("]}\n");
#ifdef ICCDEV_TELEMETRY
  if (telemetry) {
    stats->inventoryMs = CIccTiffTelemetry::Since(phaseStart);
    stats->Record("inventory_completed");
  }
#endif
  return true;
}

int main(int argc, icChar* argv[])
{
  int minargs = 1;
  bool bEvidenceJson = false;
  bool listDirectories = false;
  unsigned int directory = 0;
  bool hasDirectory = false;
  std::vector<icChar*> positional;
  const bool bareInvocation = argc <= 1;
#ifdef ICCDEV_TELEMETRY
  CIccTiffTelemetry telemetry;
  TiffDumpTelemetryStats telemetryStats(telemetry);
#endif
  if (argc)
    positional.push_back(argv[0]);
  int arg = 1;
  while (arg < argc) {
#ifdef ICCDEV_TELEMETRY
    const int telemetryOption = telemetry.ParseOption(arg, argc, argv);
    if (telemetryOption < 0)
      return 1;
    if (telemetryOption) {
      arg++;
      continue;
    }
#else
    if (icRejectTiffTelemetryOption(argv[arg]))
      return 1;
#endif
    if (!stricmp(argv[arg], "--help") || !stricmp(argv[arg], "-h")) {
      Usage();
      return 0;
    }
    if (!stricmp(argv[arg], "--evidence-json"))
      bEvidenceJson = true;
    else if (!stricmp(argv[arg], "--list-directories"))
      listDirectories = true;
    else if (!stricmp(argv[arg], "--ifd")) {
      if (hasDirectory || arg + 1 >= argc) {
        fprintf(stderr, "Invalid --ifd index\n");
        return 1;
      }
      const char *text = argv[++arg];
      char *end = NULL;
      errno = 0;
      const unsigned long value = strtoul(text, &end, 10);
      if (text[0] < '0' || text[0] > '9' || !end || *end || errno || value > 4095) {
        fprintf(stderr, "Invalid --ifd index (expected 0..4095)\n");
        return 1;
      }
      directory = static_cast<unsigned int>(value);
      hasDirectory = true;
    }
    else if (argv[arg][0] == '-' && argv[arg][1] == '-') {
      fprintf(stderr, "Unknown option: %s\n", icSanitizeConsoleText(argv[arg]).c_str());
      return 1;
    }
    else
      positional.push_back(argv[arg]);
    arg++;
  }
  argc = static_cast<int>(positional.size());
  argv = positional.data();
  if (argc <= minargs) {
    Usage();
    return bareInvocation ? 0 : 1;
  }
  else if (argc > 3) {
    Usage();
    return 1;
  }
#ifdef ICCDEV_TELEMETRY
  std::vector<std::string> protectedPaths;
  protectedPaths.push_back(argv[1]);
  if (argc > 2)
    protectedPaths.push_back(argv[2]);
  if (!telemetry.Start("iccTiffDump", protectedPaths,
                       "\"metadata_only\":true,\"decoded_pixels\":0,\"decoded_bytes\":0"))
    return 1;
  telemetryStats.started = true;
#endif
  if (listDirectories) {
    if (argc != 2 || hasDirectory) {
      fprintf(stderr, "Directory listing cannot be combined with --ifd or extraction\n");
      return 1;
    }
    const bool listed = ListDirectories(argv[1], bEvidenceJson
#ifdef ICCDEV_TELEMETRY
                                       , telemetry.Active() ? &telemetry : NULL,
                                       &telemetryStats
#endif
                                       );
#ifdef ICCDEV_TELEMETRY
    if (!listed)
      return 1;
    if (telemetry.Active() &&
        (!telemetryStats.writesOk || !telemetry.Finish(telemetryStats.Fields())))
      return 1;
    telemetryStats.completed = true;
#endif
    return listed ? 0 : 1;
  }

  // #2692: the extracted profile is written to argv[2].  Naming the input TIFF
  // there replaced the image with its own profile and exited 0.
  if (argc > 2 && icOutputIsInput(argv[2], argv[1]))
    return 1;

  std::string srcName = icSanitizeConsoleText(argv[1]);

  CTiffImg SrcImg;
#ifdef ICCDEV_TELEMETRY
  telemetryStats.SetStage("metadata_open");
  double phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
  const bool opened = SrcImg.Open(argv[1], true, directory);
#ifdef ICCDEV_TELEMETRY
  if (telemetry.Active()) {
    telemetryStats.metadataOpenMs = CIccTiffTelemetry::Since(phaseStart);
    if (opened) {
      telemetryStats.directoriesVisited = 1;
      telemetryStats.geometry = TiffDumpGeometry(SrcImg.GetTiffHandle(), directory);
    }
    telemetryStats.Record("metadata_open",
                          std::string("\"success\":") + (opened ? "true" : "false"));
  }
#endif
  if (!opened) {
    fprintf(stderr, "\nFile [%s] cannot be opened.\n", srcName.c_str());
    return 1;
  }

  if (!bEvidenceJson) {
  printf("-------------------->Tiff Image Dump<---------------------------\n");
  printf("Filename:          %s\n", srcName.c_str());
  printf("Directory:         %u\n", SrcImg.GetDirectory());
  printf("Container:         %s\n", SrcImg.IsBigTiff() ? "BigTIFF" : "TIFF");
  printf("Layout:            %s\n", SrcImg.IsTiled() ? "Tiled" : "Stripped");
  if (SrcImg.IsTiled())
    printf("Tile size:         %u x %u\n", SrcImg.GetTileWidth(), SrcImg.GetTileHeight());
  printf("Block count:       %u\n", SrcImg.IsTiled() ? TIFFNumberOfTiles(SrcImg.GetTiffHandle()) :
                                                    TIFFNumberOfStrips(SrcImg.GetTiffHandle()));
  printf("Codec available:   %s (not a pixel decode check)\n",
         TIFFIsCODECConfigured(static_cast<uint16_t>(SrcImg.GetCompress())) ? "yes" : "no");
  // A physical size only exists when RESOLUTIONUNIT names an absolute unit; under
  // RESUNIT_NONE the resolution values fix an aspect ratio and nothing else, so
  // GetWidthIn()/GetHeightIn() report 0 and printing inches would invent a measurement
  // the file never made.  They also convert from centimetres now, which the bare
  // division they used to do did not (#2220).
  const double dWidthIn = SrcImg.GetWidthIn();
  const double dHeightIn = SrcImg.GetHeightIn();
  if (dWidthIn > 0.0 && dHeightIn > 0.0)
    printf("Size:              (%u x %u) pixels, (%.2lf\" x %.2lf\")\n",
      SrcImg.GetWidth(), SrcImg.GetHeight(), dWidthIn, dHeightIn);
  else
    printf("Size:              (%u x %u) pixels\n",
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
    // a false flag means the count could not be determined (an unreadable
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
  if (SrcImg.GetBytesPerLine())
    printf("BytesPerLine:      %u\n", SrcImg.GetBytesPerLine());
  else
    printf("BytesPerLine:      unavailable (unsupported pixel geometry)\n");
  printf("Resolution:        (%lf x %lf) %s\n", SrcImg.GetXRes(), SrcImg.GetYRes(),
    GetId(SrcImg.GetResolutionUnit(), resolution_units));
  printf("Compression:       %s\n", GetId(SrcImg.GetCompress(), compression_types));
  }

  unsigned char *pProfMem = nullptr;
  unsigned int nLen = 0;
  std::string embeddedProfileId;
  std::string extractedProfileId;
#ifdef ICCDEV_TELEMETRY
  telemetryStats.SetStage("profile_metadata");
  phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
  if (SrcImg.GetIccProfile(pProfMem, nLen)) {
#ifdef ICCDEV_TELEMETRY
    if (telemetry.Active()) {
      telemetryStats.profileBytes = nLen;
      telemetryStats.Record("profile_metadata",
                            "\"embedded_profile_present\":true,\"duration_ms\":" +
                            CIccTiffTelemetry::Number(CIccTiffTelemetry::Since(phaseStart)));
    }
#endif
    if (!bEvidenceJson) {
      printf("Profile:           Embedded\n");
      fflush(stdout);
    }

    if (argc > 2) {
      std::string dstName = icSanitizeConsoleText(argv[2]);
#ifdef ICCDEV_TELEMETRY
      telemetryStats.SetStage("profile_extraction");
      phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
      const bool extracted = WriteEmbeddedIccProfile(argv[2], pProfMem, nLen);
#ifdef ICCDEV_TELEMETRY
      if (telemetry.Active()) {
        telemetryStats.extractionMs = CIccTiffTelemetry::Since(phaseStart);
        if (extracted)
          telemetryStats.extractedBytes = nLen;
        telemetryStats.Record("profile_extraction",
                              std::string("\"success\":") + (extracted ? "true" : "false"));
      }
#endif
      if (!extracted) {
        fprintf(stderr, "\nUnable to extract profile to: %s\n", dstName.c_str());
        SrcImg.Close();
        return 1;
      }
      // Only the evidence path consumes this, and it costs a second complete
      // CIccProfile parse of the file just written, so it stays inside the
      // guard rather than running on every ordinary extraction.
      if (bEvidenceJson) {
#ifdef ICCDEV_TELEMETRY
        telemetryStats.SetStage("extracted_profile_parse");
        phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
        extractedProfileId = GetProfileId(argv[2]);
#ifdef ICCDEV_TELEMETRY
        if (telemetry.Active())
          telemetryStats.Record("extracted_profile_parse",
                                "\"duration_ms\":" +
                                CIccTiffTelemetry::Number(CIccTiffTelemetry::Since(phaseStart)));
#endif
      }
      if (!bEvidenceJson) {
        printf("\nProfile extracted byte-for-byte to: %s\n", dstName.c_str());
        fflush(stdout);
      }
    }

    // Profile description and metadata
#ifdef ICCDEV_TELEMETRY
    telemetryStats.SetStage("profile_parse");
    phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
    CIccProfile *pProfile = OpenIccProfile(pProfMem, nLen);
#ifdef ICCDEV_TELEMETRY
    if (telemetry.Active()) {
      telemetryStats.profileParseMs = CIccTiffTelemetry::Since(phaseStart);
      if (pProfile)
        telemetryStats.profilesParsed++;
      telemetryStats.Record("profile_parse",
                            std::string("\"success\":") + (pProfile ? "true" : "false"));
    }
#endif
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
#ifdef ICCDEV_TELEMETRY
    telemetryStats.SetStage("profile_read");
    phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
    const bool tagsRead = pProfile->ReadTags(pProfile);
#ifdef ICCDEV_TELEMETRY
    if (telemetry.Active()) {
      telemetryStats.profileReadMs = CIccTiffTelemetry::Since(phaseStart);
      if (tagsRead)
        telemetryStats.profilesRead++;
      telemetryStats.Record("profile_read",
                            std::string("\"success\":") + (tagsRead ? "true" : "false"));
    }
#endif
    if (!tagsRead) {
      fprintf(stderr, "\nUnable to read embedded ICC profile\n");
      delete pProfile;
      SrcImg.Close();
      return 1;
    }
#ifdef ICCDEV_TELEMETRY
    telemetryStats.SetStage("profile_validate");
    phaseStart = telemetry.Active() ? CIccTiffTelemetry::Now() : 0;
#endif
    const icValidateStatus validation = pProfile->Validate(validateReport);
#ifdef ICCDEV_TELEMETRY
    if (telemetry.Active()) {
      telemetryStats.profileValidateMs = CIccTiffTelemetry::Since(phaseStart);
      if (validation <= icValidateWarning)
        telemetryStats.profilesValidated++;
      telemetryStats.Record("profile_validate",
                            "\"validation_status\":" + std::to_string(static_cast<int>(validation)) +
                            ",\"success\":" + (validation <= icValidateWarning ? "true" : "false"));
    }
#endif
    if (validation > icValidateWarning) {
      fprintf(stderr, "\nEmbedded ICC profile violates the ICC specification:\n%s",
              validateReport.c_str());
      delete pProfile;
      SrcImg.Close();
      return 1;
    }
    delete pProfile;
  } else {
#ifdef ICCDEV_TELEMETRY
    if (telemetry.Active())
      telemetryStats.Record("profile_metadata",
                            "\"embedded_profile_present\":false,\"duration_ms\":" +
                            CIccTiffTelemetry::Number(CIccTiffTelemetry::Since(phaseStart)));
#endif
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
                          embeddedProfileId, extractedProfileId, SrcImg);

#ifdef ICCDEV_TELEMETRY
  telemetryStats.SetStage("close");
#endif
  SrcImg.Close();
#ifdef ICCDEV_TELEMETRY
  if (telemetry.Active() &&
      (!telemetryStats.writesOk || !telemetry.Finish(telemetryStats.Fields())))
    return 1;
  telemetryStats.completed = true;
#endif
  return 0;
}
