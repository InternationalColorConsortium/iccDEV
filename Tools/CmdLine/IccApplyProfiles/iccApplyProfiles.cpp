/*
    File:       CmdApplyProfiles.cpp

    Contains:   Console app that applies profiles

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2016 The International Color Consortium. All rights 
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
// -Modification to support iccMAX by Max Derhak in 2014
//
//////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <memory>
#include <string>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccConnect.h"
#include "TiffImg.h"
#include "IccProfLibVer.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static FILE* OpenWriteTextFile(const std::string& path)
{
#if defined(_WIN32)
  // Export config paths are intentional caller-selected output files.
  // codeql[cpp/path-injection]
  return fopen(path.c_str(), "wt");
#else
  // Export config paths are intentional caller-selected output files.
  // codeql[cpp/path-injection]
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0)
    return nullptr;

  FILE* f = fdopen(fd, "w");
  if (!f)
    close(fd);

  return f;
#endif
}

static bool GetFloatRowByteCount(unsigned int nWidth, int nSamples, size_t& nBytes)
{
  if (nSamples <= 0)
    return false;

#if defined(__SIZEOF_INT128__)
  const unsigned __int128 nByteCount = (unsigned __int128)nWidth *
                                      (unsigned __int128)(unsigned int)nSamples *
                                      (unsigned __int128)sizeof(icFloatNumber);

  if (nByteCount > (unsigned __int128)((size_t)-1))
    return false;

  nBytes = (size_t)nByteCount;
  return true;
#else
  const size_t nMaxSize = (size_t)-1;
  const size_t nWidthSize = (size_t)nWidth;
  const size_t nSampleSize = (size_t)nSamples;

  if (nWidthSize > nMaxSize / nSampleSize)
    return false;

  const size_t nValues = nWidthSize * nSampleSize;
  if (nValues > nMaxSize / sizeof(icFloatNumber))
    return false;

  nBytes = nValues * sizeof(icFloatNumber);
  return true;
#endif
}

static icFloatNumber UnitClip(icFloatNumber v)
{
  if (std::isnan(v))
    return 0.0;
  if (std::isinf(v)) {
    if (v < 0.0)
      return 0.0;
    else
      return 1.0;
  }
  if (v < 0.0)
    return 0.0;
  if (v > 1.0)
    return 1.0;
  return v;
}

static icUInt8Number UnitClipToUInt8(icFloatNumber v)
{
  return static_cast<icUInt8Number>(UnitClip(v) * 255.0f + 0.5f);
}

static icUInt16Number UnitClipToUInt16(icFloatNumber v)
{
  return static_cast<icUInt16Number>(UnitClip(v) * 65535.0f + 0.5f);
}



void Usage() 
{
  printf("iccApplyProfiles built with IccProfLib version " ICCPROFLIBVER "\n\n");

  printf("Usage: iccApplyProfiles {-threads N} -cfg config_file\n\n");
  printf("  Optional: -threads [N] (use N worker threads; 0=hardware concurrency, 1=single-threaded)\n");
  printf("  Optional: -cfg config_file (use JSON formatted configuration file to define apply options)\n\n");

  printf("Alt-Usage: iccApplyProfiles {-threads N} {-exportcfg config_file} src_tiff_file dst_tiff_file dst_sample_encoding dst_compression dst_planar dst_embed_icc interpolation {{-ENV:sig value} profile_file_path rendering_intent {-PCC connection_conditions_path}}\n\n");
  printf("  Optional: -threads [N] (use N worker threads; 0=hardware concurrency, 1=single-threaded)\n");
  printf("  Optional: -exportcfg config_file (create config_file based on rest of arguments)\n\n");
  printf("  For dst_sample_encoding:\n");
  printf("    0 - Same as src\n");
  printf("    1 - icEncode8Bit\n");
  printf("    2 - icEncode16Bit\n");
  printf("    4 - icEncodeFloat\n\n");

  printf("  For dst_compression:\n");
  printf("    0 - No compression\n");
  printf("    1 - LZW compression\n\n");

  printf("  For dst_planar:\n");
  printf("    0 - Contig\n");
  printf("    1 - Separation\n\n");

  printf("  For dst_embed_icc:\n");
  printf("    0 - Do not Embed\n");
  printf("    1 - Embed Last ICC\n\n");

  printf("  For interpolation:\n");
  printf("    0 - Linear\n");
  printf("    1 - Tetrahedral\n\n");

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
  printf("  +1000 - Use Luminance based PCS adjustment\n");
  printf(" +10000 - Use V5 sub-profile if present\n");
}

//===================================================

int main(int argc, const char** argv)
{
  int minargs = 2;
  if (argc < minargs) {
    Usage();
    return 0;
  }

  CIccCfgImageApply cfgApply;
  CIccCfgConnectOptions cfgConnect;
  CIccCfgProfileSequence cfgProfiles;
  bool bThreadArg = false;
  int nThreadArg = cfgConnect.m_nThreads;

  if (argc > 3 && !stricmp(argv[1], "-threads")) {
    nThreadArg = atoi(argv[2]);
    if (nThreadArg < 0) {
      printf("Invalid thread count '%s'\n", argv[2]);
      Usage();
      return -1;
    }
    bThreadArg = true;
    argv += 2;
    argc -= 2;
  }

  if (argc > 2 && !stricmp(argv[1], "-cfg")) {
    json cfg;
    if (!loadJsonFrom(cfg, argv[2]) || !cfg.is_object()) {
      printf("Unable to read configuration from '%s'\n", argv[2]);
      return -1;
    }

    if (cfg.find("imageFiles") == cfg.end() || !cfgApply.fromJson(cfg["imageFiles"])) {
      printf("Unable to parse imageFiles configuration from '%s'\n", argv[2]);
      return -1;
    }

    if (cfg.find("profileSequence") == cfg.end() || !cfgProfiles.fromJson(cfg["profileSequence"])) {
      printf("Unable to parse profileSequence configuration from '%s'\n", argv[2]);
      return -1;
    }

    auto connectOptions = cfg.find("connect");
    if (connectOptions != cfg.end() && !cfgConnect.fromJson(*connectOptions)) {
      printf("Unable to parse connect configuration from '%s'\n", argv[2]);
      return -1;
    }
  }
  else {
    std::string exportFile;

    argv++;
    argc--;

    if (argc > 2 && !stricmp(argv[0], "-exportcfg")) {
      exportFile = argv[1];
      argv += 2;
      argc -= 2;
    }

    int nArg = cfgApply.fromArgs(&argv[0], argc);
    if (!nArg) {
      printf("Unable to parse configuration arguments\n");
      Usage();
      return -1;
    }
    argv += nArg;
    argc -= nArg;

    nArg = cfgProfiles.fromArgs(&argv[0], argc);
    if (!nArg) {
      printf("Unable to parse profile sequence arguments\n");
      Usage();
      return -1;
    }

    if (bThreadArg)
      cfgConnect.m_nThreads = nThreadArg;

    if (!exportFile.empty()) {
      FILE* f = OpenWriteTextFile(exportFile);
      if (f) {
        json cfgJson;
        json applyJson, connectJson, profilesJson;

        cfgApply.toJson(applyJson);
        cfgJson["imageFiles"] = applyJson;

        cfgConnect.toJson(connectJson);
        if (!connectJson.empty())
          cfgJson["connect"] = connectJson;
        
        cfgProfiles.toJson(profilesJson);
        cfgJson["profileSequence"] = profilesJson;

        std::string jsonText = cfgJson.dump(1);
        if (fwrite(jsonText.c_str(), 1, jsonText.size(), f) != jsonText.size()) {
          printf("Unable to write complete config file '%s'\n", exportFile.c_str());
        }
        fclose(f);
      }
      else {
        printf("Unable to export config file '%s'\n", exportFile.c_str());
      }
    }
  }

  if (bThreadArg)
    cfgConnect.m_nThreads = nThreadArg;

  int i, j, k;
  unsigned int sn, sen, sphoto, photo, bps, dbps;
  CTiffImg SrcImg, DstImg;
  unsigned char *sptr, *dptr;
  const char *last_path = NULL;

  //Open source image file and get information from it
  if (!SrcImg.Open(cfgApply.m_srcImgFile.c_str())) {
    printf("\nFile [%s] cannot be opened.\n", cfgApply.m_srcImgFile.c_str());
    return -1;
  }
  sn = SrcImg.GetSamples();
  sen = SrcImg.GetExtraSamples();
  sphoto = SrcImg.GetPhoto();
  bps = SrcImg.GetBitsPerSample();

  //Setup source encoding based on bits per sample (bps) in source image
  // really just error checking
  switch(bps) {
    case 8:     // 8 bit uint
      break;
    case 16:    // 16 bit uint
      break;
    case 32:    // 32 bit float
      break;
    default:
      printf("Source bit depth / color data encoding not supported.\n");
      return -1;
  }

  if (cfgApply.m_dstEncoding == icEncodeUnknown) {
    dbps = bps;
  }
  else {
    icFloatColorEncoding destEncoding = cfgApply.m_dstEncoding;
    switch (destEncoding) {
      case icEncode8Bit:
        dbps = 8;
        break;
      case icEncode16Bit:
        dbps = 16;
        break;
      case icEncodeFloat:
        dbps = 32;
        break;
      default:
        printf("Source color data encoding not recognized.\n");
        return -1;
    }
  }
  unsigned char* pSrcProfile;
  unsigned int nSrcProfileLen;
  bool bHasSrcProfile = SrcImg.GetIccProfile(pSrcProfile, nSrcProfileLen);

  //Retrieve command line arguments
  bool bCompress = cfgApply.m_dstCompression == icDstBoolFromSrc ? SrcImg.GetCompress() : (cfgApply.m_dstCompression != icDstBoolFalse);
  bool bSeparation = cfgApply.m_dstPlanar == icDstBoolFromSrc ? SrcImg.GetPlanar() : (cfgApply.m_dstPlanar != icDstBoolFalse);
  bool bEmbed = cfgApply.m_dstEmbedIcc == icDstBoolFromSrc ? bHasSrcProfile : (cfgApply.m_dstEmbedIcc != icDstBoolFalse);


  // Use embedded ICC from source image when first profile entry has no file path.
  unsigned char* pEmbedded = nullptr;
  unsigned int nEmbeddedLen = 0;
  if (!cfgProfiles.m_profiles.empty() && cfgProfiles.m_profiles[0]->m_iccFile.empty()) {
    if (!bHasSrcProfile) {
      printf("Source image doesn't have embedded profile!\n");
      return -1;
    }
    pEmbedded = pSrcProfile;
    nEmbeddedLen = nSrcProfileLen;
  }

  std::unique_ptr<CIccConnectCmm> pConnect(
    CIccConnectCmm::CreateStandard(cfgProfiles, pEmbedded, nEmbeddedLen, cfgConnect.m_nThreads));

  if (!pConnect) {
    printf("Error - Unable to begin profile application - Possibly invalid or incompatible profiles\n");
    return -1;
  }

  CIccCmm* pTheCmm = pConnect->GetCmm();
  const bool bUseRowApply = cfgConnect.m_nThreads != 1;

  // Set last_path to the last profile's file for downstream embed logic.
  if (!cfgProfiles.m_profiles.empty())
    last_path = cfgProfiles.m_profiles.back()->m_iccFile.c_str();

  //Get and validate the source color space from the Cmm.
  icColorSpaceSignature SrcspaceSig = pTheCmm->GetSourceSpace();
  int nSrcColorSamples = icGetSpaceSamples(SrcspaceSig);
  int nSrcSamples = nSrcColorSamples;

  if (nSrcSamples != (int)sn ) {
    //Allow color management to ignore extra samples when non extra samples match samples in profile
    if (sen != 0) {
      if (nSrcSamples != (int)(sn - sen)) {
        printf("Number of non-extra samples %d in image[%s] doesn't match device samples %d in first profile\n", sn - sen, cfgApply.m_srcImgFile.c_str(), nSrcSamples);
        return -1;
      }
      else
        nSrcSamples = sn;
    }
    else {
      printf("Number of samples %d in image[%s] doesn't match device samples %d in first profile\n", sn, cfgApply.m_srcImgFile.c_str(), nSrcSamples);
      return -1;
    }
  }

  //Get and validate the destination color space from the CMM.
  icColorSpaceSignature DestSpaceSig = pTheCmm->GetDestSpace();
  icColorSpaceSignature DestColorSpaceSig = DestSpaceSig;
  int nDestSamples = icGetSpaceSamples(DestSpaceSig);

  icColorSpaceSignature DestParentSpaceSig = pTheCmm->GetLastParentSpace();
  int nDestParentSamples = icGetSpaceSamples(DestParentSpaceSig);
  
  int nExtraSamples = 0;

  if (nDestParentSamples && nDestSamples != nDestParentSamples) {
    DestColorSpaceSig = DestParentSpaceSig;
    nExtraSamples = nDestSamples - nDestParentSamples;
  }

  switch (DestColorSpaceSig) {
  case icSigRgbData:
    photo = PHOTO_RGB;
    break;

  case icSigCmyData:
  case icSigCmykData:
  case icSig4colorData:
  case icSig5colorData:
  case icSig6colorData:
  case icSig7colorData:
  case icSig8colorData:
    photo = PHOTO_MINISWHITE;
    break;

  case icSigXYZData:
    //Fall through - No break here

  case icSigLabData:
    photo = PHOTO_CIELAB;
    break;

  default:
    photo = PHOTO_MINISBLACK;
    break;
  }

  unsigned long sbpp = (nSrcSamples * bps + 7) / 8;
  unsigned long dbpp = (nDestSamples * dbps  + 7)/ 8;

  //Open up output image using information from SrcImg and theCmm
  if (!DstImg.Create(cfgApply.m_dstImgFile.c_str(), SrcImg.GetWidth(), SrcImg.GetHeight(), dbps, photo, nDestSamples, nExtraSamples, SrcImg.GetXRes(), SrcImg.GetYRes(), bCompress, bSeparation)) {
    printf("Unable to create Tiff file - '%s'\n", cfgApply.m_dstImgFile.c_str());
    return -1;
  }

  //Embed the last profile into output image as needed
  if (bEmbed && last_path) {
    size_t length = 0;
    icUInt8Number *pDestProfile = NULL;

    CIccFileIO io;
    if (io.Open(last_path, "r")) {
      length = io.GetLength();
      pDestProfile = (icUInt8Number *)malloc(length);
      if (pDestProfile) {
        io.Read8(pDestProfile, length);
        DstImg.SetIccProfile(pDestProfile, ( unsigned int) length);
        free(pDestProfile);
      }
      io.Close();
    }
  }

  // Allocate single line buffer for reading source image pixels
  unsigned char *pSBuf = (unsigned char *)malloc(SrcImg.GetBytesPerLine());
  if (!pSBuf) {
    printf("Out of Memory!\n");
    return -1;
  }

  //Allocate buffer for putting color managed pixels into that will be sent to output tiff image
  unsigned char *pDBuf = (unsigned char *)malloc(DstImg.GetBytesPerLine());
  if (!pDBuf) {
    printf("Out of Memory!\n");
    free(pSBuf);
    return -1;
  }

  icFloatNumber *pSrcRowBuf = nullptr;
  icFloatNumber *pDstRowBuf = nullptr;
  if (bUseRowApply) {
    size_t nSrcRowBytes = 0;
    size_t nDstRowBytes = 0;

    if (!GetFloatRowByteCount(SrcImg.GetWidth(), nSrcColorSamples, nSrcRowBytes) ||
        !GetFloatRowByteCount(SrcImg.GetWidth(), nDestSamples, nDstRowBytes)) {
      printf("Invalid row buffer size!\n");
      free(pSBuf);
      free(pDBuf);
      return -1;
    }

    pSrcRowBuf = (icFloatNumber*)malloc(nSrcRowBytes);
    pDstRowBuf = (icFloatNumber*)malloc(nDstRowBytes);
    if (!pSrcRowBuf || !pDstRowBuf) {
      printf("Out of Memory!\n");
      free(pSBuf);
      free(pDBuf);
      free(pSrcRowBuf);
      free(pDstRowBuf);
      return -1;
    }
  }

  //Allocate pixel buffers for performing encoding transformations
  CIccPixelBuf SrcPixel(nSrcSamples+16), DestPixel(nDestSamples+16), Pixel(icIntMax(nSrcSamples, nDestSamples)+16);
  int lastPer = -1;
  int curper;

  auto decodePixel = [&](icFloatNumber *pPixel, unsigned char *pSrcBytes) -> bool {
    switch(bps) {
      case 8:
        if (sphoto==PHOTO_CIELAB) {
          unsigned char *pSPixel = pSrcBytes;
          pPixel[0]=(icFloatNumber)pSPixel[0] / 255.0f;
          pPixel[1]=(icFloatNumber)(pSPixel[1]-128) / 255.0f;
          pPixel[2]=(icFloatNumber)(pSPixel[2]-128) / 255.0f;
        }
        else {
          unsigned char *pSPixel = pSrcBytes;
          for (k=0; k<nSrcColorSamples; k++)
            pPixel[k] = (icFloatNumber)pSPixel[k] / 255.0f;
        }
        break;

      case 16:
        if (sphoto==PHOTO_CIELAB) {
          unsigned short *pSPixel = (unsigned short*)pSrcBytes;
          pPixel[0]=(icFloatNumber)pSPixel[0] / 65535.0f;
          pPixel[1]=(icFloatNumber)(pSPixel[1]-0x8000) / 65535.0f;
          pPixel[2]=(icFloatNumber)(pSPixel[2]-0x8000) / 65535.0f;
        }
        else {
          unsigned short *pSPixel = (unsigned short*)pSrcBytes;
          for (k=0; k<nSrcColorSamples; k++)
            pPixel[k] = (icFloatNumber)pSPixel[k] / 65535.0f;
        }
        break;

      case 32:
        if (sizeof(icFloatNumber)==sizeof(icFloat32Number)) {
          memcpy(pPixel, pSrcBytes, nSrcColorSamples * sizeof(icFloat32Number));
        }
        else {
          icFloat32Number *pSPixel = (icFloat32Number*)pSrcBytes;
          for (k=0; k<nSrcColorSamples; k++)
            pPixel[k] = (icFloatNumber)pSPixel[k];
        }

        if (sphoto==PHOTO_CIELAB || sphoto==PHOTO_ICCLAB)
          icLabToPcs(pPixel);
        break;

      default:
        printf("Invalid source bit depth\n");
        return false;
    }

    if (sphoto == PHOTO_CIELAB && SrcspaceSig==icSigXYZData) {
      icLabFromPcs(pPixel);
      icLabtoXYZ(pPixel);
      icXyzToPcs(pPixel);
    }

    return true;
  };

  auto encodePixel = [&](unsigned char *pDstBytes, icFloatNumber *pPixel) -> bool {
    if (photo==PHOTO_CIELAB && DestSpaceSig==icSigXYZData) {
      icXyzFromPcs(pPixel);
      icXYZtoLab(pPixel);
      icLabToPcs(pPixel);
    }

    switch(dbps) {
      case 8:
        if (photo==PHOTO_CIELAB) {
          unsigned char *pDPixel = pDstBytes;
          pDPixel[0] = UnitClipToUInt8(pPixel[0]);
          pDPixel[1] = static_cast<icUInt8Number>((UnitClipToUInt8(pPixel[1]) + 128) & 0xFF);
          pDPixel[2] = static_cast<icUInt8Number>((UnitClipToUInt8(pPixel[2]) + 128) & 0xFF);
        }
        else {
          icUInt8Number *pDPixel = pDstBytes;
          for (k=0; k<nDestSamples; k++)
            pDPixel[k] = UnitClipToUInt8(pPixel[k]);
        }
        break;

      case 16:
        if (photo==PHOTO_CIELAB) {
          unsigned short *pDPixel = (unsigned short*)pDstBytes;
          pDPixel[0] = UnitClipToUInt16(pPixel[0]);
          pDPixel[1] = static_cast<icUInt16Number>((UnitClipToUInt16(pPixel[1]) + 0x8000) & 0xFFFF);
          pDPixel[2] = static_cast<icUInt16Number>((UnitClipToUInt16(pPixel[2]) + 0x8000) & 0xFFFF);
        }
        else {
          icUInt16Number *pDPixel = (icUInt16Number*)pDstBytes;
          for (k=0; k<nDestSamples; k++)
            pDPixel[k] = UnitClipToUInt16(pPixel[k]);
        }
        break;

      case 32:
        if (photo==PHOTO_CIELAB || photo==PHOTO_ICCLAB)
          icLabFromPcs(pPixel);

        if (sizeof(icFloatNumber)==sizeof(icFloat32Number)) {
          memcpy(pDstBytes, pPixel, dbpp);
        }
        else {
          icFloat32Number *pDPixel = (icFloat32Number*)pDstBytes;
          for (k=0; k<nDestSamples; k++)
            pDPixel[k] = static_cast<icFloat32Number>(pPixel[k]);
        }
        break;

      default:
        printf("Invalid destination bit depth\n");
        return false;
    }

    return true;
  };

  //Read each line
  for (i=0; i<(int)SrcImg.GetHeight(); i++) {
    if (!SrcImg.ReadLine(pSBuf)) {
      break;
    }
    if (bUseRowApply) {
      for (sptr=pSBuf, j=0; j<(int)SrcImg.GetWidth(); j++, sptr+=sbpp) {
        if (!decodePixel(pSrcRowBuf + j * nSrcColorSamples, sptr)) {
          free(pSBuf);
          free(pDBuf);
          free(pSrcRowBuf);
          free(pDstRowBuf);
          return -1;
        }
      }

      pTheCmm->Apply(pDstRowBuf, pSrcRowBuf, SrcImg.GetWidth());

      for (dptr=pDBuf, j=0; j<(int)SrcImg.GetWidth(); j++, dptr+=dbpp) {
        if (!encodePixel(dptr, pDstRowBuf + j * nDestSamples)) {
          free(pSBuf);
          free(pDBuf);
          free(pSrcRowBuf);
          free(pDstRowBuf);
          return -1;
        }
      }
    }
    else {
      for (sptr=pSBuf, dptr=pDBuf, j=0; j<(int)SrcImg.GetWidth(); j++, sptr+=sbpp, dptr+=dbpp) {

      //Special conversions need to be made to convert CIELAB and CIEXYZ to internal PCS encoding
      switch(bps) {
        case 8:
          if (sphoto==PHOTO_CIELAB) {
            unsigned char *pSPixel = sptr;
            icFloatNumber *pPixel = SrcPixel;
            pPixel[0]=(icFloatNumber)pSPixel[0] / 255.0f;
            pPixel[1]=(icFloatNumber)(pSPixel[1]-128) / 255.0f;
            pPixel[2]=(icFloatNumber)(pSPixel[2]-128) / 255.0f;
          }
          else {
            unsigned char *pSPixel = sptr;
            icFloatNumber *pPixel = SrcPixel;
            for (k=0; k<nSrcColorSamples; k++) {
              pPixel[k] = (icFloatNumber)pSPixel[k] / 255.0f;
            }
          }
          break;

        case 16:
          if (sphoto==PHOTO_CIELAB) {
            unsigned short *pSPixel = (unsigned short*)sptr;
            icFloatNumber *pPixel = SrcPixel;
            pPixel[0]=(icFloatNumber)pSPixel[0] / 65535.0f;
            pPixel[1]=(icFloatNumber)(pSPixel[1]-0x8000) / 65535.0f;
            pPixel[2]=(icFloatNumber)(pSPixel[2]-0x8000) / 65535.0f;
          }
          else {
            unsigned short *pSPixel = (unsigned short*)sptr;
            icFloatNumber *pPixel = SrcPixel;
            for (k=0; k<nSrcColorSamples; k++) {
              pPixel[k] = (icFloatNumber)pSPixel[k] / 65535.0f;
            }
          }
          break;

        case 32:
          {
            if (sizeof(icFloatNumber)==sizeof(icFloat32Number)) {
              memcpy(SrcPixel.get(), sptr, sbpp);
            }
            else {
              icFloat32Number *pSPixel = (icFloat32Number*)sptr;
              icFloatNumber *pPixel = SrcPixel;
              for (k=0; k<nSrcColorSamples; k++) {
                pPixel[k] = (icFloatNumber)pSPixel[k];
              }
            }

            if (sphoto==PHOTO_CIELAB || sphoto==PHOTO_ICCLAB) {
              icLabToPcs(SrcPixel);
            }
          }
          break;

        default:
          printf("Invalid source bit depth\n");
          return -1;
      }
      if (sphoto == PHOTO_CIELAB && SrcspaceSig==icSigXYZData) {
        icLabFromPcs(SrcPixel);
        icLabtoXYZ(SrcPixel);
        icXyzToPcs(SrcPixel);
      }

      //Use CMM to convert SrcPixel to DestPixel
      pTheCmm->Apply(DestPixel, SrcPixel);

      //Special conversions need to be made to convert from internal PCS encoding CIELAB
      if (photo==PHOTO_CIELAB && DestSpaceSig==icSigXYZData) {
        icXyzFromPcs(DestPixel);
        icXYZtoLab(DestPixel);
        icLabToPcs(DestPixel);
      }
      switch(dbps) {
        case 8:
          if (photo==PHOTO_CIELAB) {
            unsigned char *pDPixel = dptr;
            icFloatNumber *pPixel = DestPixel;
            pDPixel[0] = UnitClipToUInt8(pPixel[0]);
            pDPixel[1] = static_cast<icUInt8Number>((UnitClipToUInt8(pPixel[1]) + 128) & 0xFF);
            pDPixel[2] = static_cast<icUInt8Number>((UnitClipToUInt8(pPixel[2]) + 128) & 0xFF);
          }
          else {
            icUInt8Number *pDPixel = dptr;
            icFloatNumber *pPixel = DestPixel;
            for (k=0; k<nDestSamples; k++) {
              pDPixel[k] = UnitClipToUInt8(pPixel[k]);
            }
          }
          break;

        case 16:
          if (photo==PHOTO_CIELAB) {
            unsigned short *pDPixel = (unsigned short*)dptr;
            icFloatNumber *pPixel = DestPixel;
            pDPixel[0] = UnitClipToUInt16(pPixel[0]);
            pDPixel[1] = static_cast<icUInt16Number>((UnitClipToUInt16(pPixel[1]) + 0x8000) & 0xFFFF);
            pDPixel[2] = static_cast<icUInt16Number>((UnitClipToUInt16(pPixel[2]) + 0x8000) & 0xFFFF);
          }
          else {
            icUInt16Number *pDPixel = (icUInt16Number*)dptr;
            icFloatNumber *pPixel = DestPixel;
            for (k=0; k<nDestSamples; k++) {
              pDPixel[k] = UnitClipToUInt16(pPixel[k]);
            }
          }
          break;

        case 32:
          {
            if (photo==PHOTO_CIELAB || photo==PHOTO_ICCLAB) {
              icLabFromPcs(SrcPixel);
            }

            if (sizeof(icFloatNumber)==sizeof(icFloat32Number)) {
              memcpy(dptr, DestPixel.get(), dbpp);
            }
            else {
              icFloat32Number *pDPixel = (icFloat32Number*)dptr;
              icFloatNumber *pPixel = DestPixel;
              for (k=0; k<nDestSamples; k++) {
                pDPixel[k] = static_cast<icFloat32Number>(pPixel[k]);
              }
            }
          }
          break;

        default:
          printf("Invalid source bit depth\n");
          return -1;
      }
      }
    }

    //Output the converted pixels to the destination image
    if (!DstImg.WriteLine(pDBuf)) {
      break;
    }

    //Display status of how much we have accomplished
    curper = static_cast<int>((static_cast<float>(i + 1) * 100.0f) /
                              static_cast<float>(SrcImg.GetHeight()));
    if (curper !=lastPer) {
      printf("\r%d%%", curper);
      lastPer = curper;
    }
  }
  printf("\n");

  //Clean everything up by closeing files and freeing buffers
  SrcImg.Close();

  free(pSBuf);
  free(pDBuf);
  free(pSrcRowBuf);
  free(pDstRowBuf);

  DstImg.Close();

  return 0;
}
