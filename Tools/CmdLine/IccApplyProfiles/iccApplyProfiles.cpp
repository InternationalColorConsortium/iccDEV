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
#include <cerrno>
#include <cstdlib>  // EXIT_FAILURE, used by the argument-contract guards in main()
#include <cstdarg>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include "IccCmm.h"
#include "IccCmmThread.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccSignatureUtils.h"
#include "IccConnect.h"
#include "TiffImg.h"
#include "IccProfLibVer.h"
#include "IccLibConnectVer.h"
#include "IccCmdLineUtil.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using IccApplyTraceClock = std::chrono::steady_clock;

static const IccApplyTraceClock::time_point g_iccApplyTraceStart =
  IccApplyTraceClock::now();

static int IccApplyTraceLevel()
{
  static const int level = []() {
    const char *value = std::getenv("ICC_APPLY_TRACE");
    if (!value || !value[0])
      return 0;

    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end || parsed < 0)
      return 1;
    if (parsed > 3)
      return 3;
    return static_cast<int>(parsed);
  }();
  return level;
}

static unsigned long long IccApplyElapsedMicroseconds()
{
  return static_cast<unsigned long long>(
    std::chrono::duration_cast<std::chrono::microseconds>(
      IccApplyTraceClock::now() - g_iccApplyTraceStart).count());
}

static unsigned long long IccApplyElapsedNanoseconds()
{
  return static_cast<unsigned long long>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      IccApplyTraceClock::now() - g_iccApplyTraceStart).count());
}

static bool IccApplyTimingEnabled()
{
  const char *value = std::getenv("ICC_APPLY_PROFILES_TIMING");
  return IccApplyTraceLevel() > 0 ||
         (value && value[0] && std::strcmp(value, "0"));
}

static void IccApplyTrace(int level, const char *format, ...)
{
  if (IccApplyTraceLevel() < level)
    return;

  char message[4096];
  va_list args;
  va_start(args, format);
  int messageLength = std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  std::fprintf(stderr, "[APPLY_TRACE] elapsed_us=%llu ",
               IccApplyElapsedMicroseconds());
  if (messageLength < 0) {
    std::fputs("trace_format_error", stderr);
  }
  else {
    size_t length = static_cast<size_t>(messageLength);
    if (length >= sizeof(message))
      length = sizeof(message) - 1;
    for (size_t i = 0; i < length; i++) {
      unsigned char ch = static_cast<unsigned char>(message[i]);
      if (ch >= 0x20 && ch < 0x7f)
        std::fputc(ch, stderr);
      else
        std::fprintf(stderr, "\\x%02X", static_cast<unsigned int>(ch));
    }
  }
  std::fputc('\n', stderr);
  std::fflush(stderr);
}

#define ICC_APPLY_TRACE(level, ...) \
  do { \
    if (IccApplyTraceLevel() >= (level)) \
      IccApplyTrace((level), __VA_ARGS__); \
  } while (0)

static FILE* OpenWriteTextFile(const std::string& path)
{
  // Export config paths are intentional caller-selected output files.

  // codeql[cpp/path-injection]
  return icOpenRegularWriteTextFile(path.c_str());
}

static bool GetFloatRowByteCount(unsigned int nWidth, int nSamples, size_t& nBytes)
{
  if (nSamples <= 0)
    return false;

#if defined(__SIZEOF_INT128__)
  __extension__ typedef unsigned __int128 CIccUInt128;
  const CIccUInt128 nByteCount = (CIccUInt128)nWidth *
                                 (CIccUInt128)(unsigned int)nSamples *
                                 (CIccUInt128)sizeof(icFloatNumber);

  if (nByteCount > (CIccUInt128)((size_t)-1))
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

static bool AddPixelBufSlack(size_t& nBytes)
{
  const size_t nMaxSize = (size_t)-1;
  const size_t nSlackBytes = 16 * sizeof(icFloatNumber);

  if (nBytes > nMaxSize - nSlackBytes)
    return false;

  nBytes += nSlackBytes;
  return true;
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
  printf("iccApplyProfiles built with IccProfLib version " ICCPROFLIBVER ", IccLibConnect Version " ICCLIBCONNECTVER "\n\n");

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
  printf("    3 - icEncodeFloat\n\n");

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
  // #2262: the acronym was transposed, B-D-R-F -- ICC.2-2023 9.2.14-17 and
  // 9.2.26-29 spell the tags brdfAToB0Tag..brdfDToB3Tag, and the library's own
  // identifiers already agree (icXformLutBRDFParam, icSigBRDFDToB0Tag,
  // CIccStructBRDF).  The same three lines carry the typo in iccApplyNamedCmm
  // and iccApplyToLink and are corrected there too.
  //
  // The four codes also take a rendering intent in their units digit, which
  // printing them as flat values said they did not: CIccXform::Create() indexes
  // a four-entry tag array with nTagIntent in each of the
  // icXformLutBRDFParam..icXformLutMCS cases, and the decode that feeds it
  // falls through its type switch for 5..8, so "nIntent % 10" reaches the
  // intent unchanged.  80 is qualified because it is the one code where that is
  // only half true: icXformLutMCS offsets by nTagIntent on its MVIS/Output
  // branch (MToS0..3, MToB0..3) but reads a single AToM0Tag for
  // MultiplexIdentification/Input and a single MToA0Tag for MultiplexLink, so
  // 80..83 are indistinguishable in the to-MCS direction.
  //
  // Written in the "NN + Intent" form iccApplyNamedCmm uses for its 10/20/40
  // rows rather than enumerated in this screen's own style:
  // spelling out 50..53, 60..63, 70..73 and 80..83 would add twelve lines to
  // say the same thing, and this way the three tools' BRDF and MCS rows agree
  // line for line -- nothing had ever compared them, which is how the same
  // three transforms came to be named two different ways.
  printf("    50 + Intent - BRDF Parameters\n");
  printf("    60 + Intent - BRDF Direct\n");
  printf("    70 + Intent - BRDF MCS Parameters\n");
  printf("    80 + Intent - MCS connection (Intent applies to MToS/MToB only)\n");
  printf("  +1000 - Use Luminance based PCS adjustment\n");
  printf(" +10000 - Use V5 sub-profile if present\n");
}

//===================================================

int main(int argc, const char** argv)
{
  ICC_APPLY_TRACE(1, "event=process_start argc=%d trace_level=%d",
                  argc, IccApplyTraceLevel());
  for (int argIndex = 0; argIndex < argc; argIndex++)
    ICC_APPLY_TRACE(2, "event=argument index=%d value=%s", argIndex, argv[argIndex]);

  int minargs = 2;
  if (argc < minargs) {
    ICC_APPLY_TRACE(1, "%s", "event=usage reason=insufficient_arguments");
    Usage();
    return 0;
  }

  CIccCfgImageApply cfgApply;
  CIccCfgConnectOptions cfgConnect;
  CIccCfgProfileSequence cfgProfiles;
  bool bThreadArg = false;
  int nThreadArg = cfgConnect.m_nThreads;

  if (argc > 3 && !stricmp(argv[1], "-threads")) {
    char* end = nullptr;
    errno = 0;
    long parsed = strtol(argv[2], &end, 10);
    if (errno || !end || end == argv[2] || *end || parsed < 0 ||
        parsed > CIccThreadedCmm::GetMaxThreads()) {
      printf("Invalid thread count '%s'\n", argv[2]);
      Usage();
      return EXIT_FAILURE;
    }
    nThreadArg = static_cast<int>(parsed);
    bThreadArg = true;
    argv += 2;
    argc -= 2;
  }

  if (argc > 2 && !stricmp(argv[1], "-cfg")) {
    // Usage 1 is exactly "-cfg <path>"; every setting comes from the JSON file, so
    // there is nothing a further argument could mean. Anything beyond argv[2] was
    // silently discarded and the run still reported success, so a caller could not
    // tell an honoured argument list from an ignored one. Same defect and same
    // guard as the sibling tools: iccApplyNamedCmm (#1906) and iccApplySearch
    // (#2075). The argc test above has already established argc >= 3, so argv[2] is
    // readable here and only a longer list can reach this branch. Note argc is the
    // count left after the optional "-threads N" pair above, which is why the
    // comparison is against 3 rather than the process-entry argc.
    if (argc != 3) {
      printf("Unexpected extra arguments for -cfg\n");
      return EXIT_FAILURE;
    }

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
    // CIccCfgProfileSequence::fromArgs() consumes the profile group in pairs and
    // stops once fewer than two arguments remain, reporting how many it used. An
    // odd token left at the end was therefore neither consumed nor rejected: the
    // transform ran as though it had not been given and the tool exited 0, so a
    // mistyped trailing path or intent looked like a honoured command line. Two or
    // more leftover tokens already fail, because fromArgs() reads them as a further
    // profile/intent pair and returns 0, which is why only the odd remainder needs
    // this guard (#1674). Compare the consumed count against what was offered
    // rather than advancing argv/argc once more: this is the last use of the
    // argument vector, so the pointer bump that pairs with the count above would be
    // a store no later read could observe (#1958).
    // Not covered here: a trailing "-ENV:sig value" pair is *counted* as consumed
    // even when the profile pair it precedes never arrives, so fromArgs() reports
    // it as used and this comparison cannot see it. That shape pushes a profile
    // entry with an empty file path, which the embedded-profile branch below then
    // treats as "-embedded" -- a defect in fromArgs() itself rather than in the
    // argument count, and so out of scope for this guard.
    if (nArg != argc) {
      printf("Unexpected extra arguments\n");
      return EXIT_FAILURE;
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
          fclose(f);
          return -1;
        }
        if (!icFlushAndClose(f)) {
          printf("Unable to close config file '%s'\n", exportFile.c_str());
          return -1;
        }
      }
      else {
        printf("Unable to export config file '%s'\n", exportFile.c_str());
        return -1;
      }
    }
  }

  if (bThreadArg)
    cfgConnect.m_nThreads = nThreadArg;

  ICC_APPLY_TRACE(
    1,
    "event=config_ready src=%s dst=%s profiles=%zu threads=%d",
    cfgApply.m_srcImgFile.c_str(), cfgApply.m_dstImgFile.c_str(),
    cfgProfiles.m_profiles.size(), cfgConnect.m_nThreads);
  for (size_t profileIndex = 0; profileIndex < cfgProfiles.m_profiles.size();
       profileIndex++) {
    const CIccCfgProfilePtr &profile = cfgProfiles.m_profiles[profileIndex];
    ICC_APPLY_TRACE(
      2,
      "event=profile index=%zu path=%s embedded=%d intent=%d transform=%d "
      "icc_env=%zu pcc=%s pcc_env=%zu use_bpc=%d use_v5=%d",
      profileIndex, profile->m_iccFile.c_str(), profile->m_useEmbedded ? 1 : 0,
      profile->m_intent, static_cast<int>(profile->m_transform),
      profile->m_iccEnvVars.size(), profile->m_pccFile.c_str(),
      profile->m_pccEnvVars.size(), profile->m_useBPC ? 1 : 0,
      profile->m_useV5SubProfile ? 1 : 0);
  }

  int i, j, k;
  unsigned int sn, sen, photo, bps, dbps;
  CTiffImg SrcImg, DstImg;
  unsigned char *sptr, *dptr;
  const char *last_path = NULL;

  //Open source image file and get information from it
  unsigned long long phaseStartUs = IccApplyElapsedMicroseconds();
  if (!SrcImg.Open(cfgApply.m_srcImgFile.c_str())) {
    ICC_APPLY_TRACE(1, "event=source_open_failed path=%s duration_us=%llu",
                    cfgApply.m_srcImgFile.c_str(),
                    IccApplyElapsedMicroseconds() - phaseStartUs);
    printf("\nFile [%s] cannot be opened.\n", cfgApply.m_srcImgFile.c_str());
    return -1;
  }
  sn = SrcImg.GetSamples();
  sen = SrcImg.GetExtraSamples();
  bps = SrcImg.GetBitsPerSample();
  ICC_APPLY_TRACE(
    1,
    "event=source_opened path=%s duration_us=%llu width=%u height=%u samples=%u "
    "extra_samples=%u bits_per_sample=%u bytes_per_line=%u x_res=%.6f "
    "y_res=%.6f resolution_unit=%u",
    cfgApply.m_srcImgFile.c_str(),
    IccApplyElapsedMicroseconds() - phaseStartUs, SrcImg.GetWidth(),
    SrcImg.GetHeight(), sn, sen, bps, SrcImg.GetBytesPerLine(),
    SrcImg.GetXRes(), SrcImg.GetYRes(), SrcImg.GetResolutionUnit());

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

  ICC_APPLY_TRACE(
    1,
    "event=output_options dest_bits_per_sample=%u compress=%d planar=%d embed=%d "
    "source_has_profile=%d source_profile_bytes=%u",
    dbps, bCompress ? 1 : 0, bSeparation ? 1 : 0, bEmbed ? 1 : 0,
    bHasSrcProfile ? 1 : 0, nSrcProfileLen);

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

  std::string sConnectError;
  phaseStartUs = IccApplyElapsedMicroseconds();
  ICC_APPLY_TRACE(
    1,
    "event=cmm_create_begin profiles=%zu embedded_bytes=%u requested_threads=%d",
    cfgProfiles.m_profiles.size(), nEmbeddedLen, cfgConnect.m_nThreads);
  std::unique_ptr<CIccConnectCmm> pConnect(
    CIccConnectCmm::CreateStandard(cfgProfiles, pEmbedded, nEmbeddedLen,
                                   cfgConnect.m_nThreads, &sConnectError));

  if (!pConnect) {
    ICC_APPLY_TRACE(1, "event=cmm_create_failed duration_us=%llu error=%s",
                    IccApplyElapsedMicroseconds() - phaseStartUs,
                    sConnectError.c_str());
    if (!sConnectError.empty())
      printf("Error - %s\n", sConnectError.c_str());
    else
      printf("Error - Unable to begin profile application - Possibly invalid or incompatible profiles\n");
    return -1;
  }

  CIccCmm* pTheCmm = pConnect->GetCmm();
  const bool bUseRowApply = cfgConnect.m_nThreads != 1;
  ICC_APPLY_TRACE(1, "event=cmm_create_end duration_us=%llu mode=%s",
                  IccApplyElapsedMicroseconds() - phaseStartUs,
                  bUseRowApply ? "row" : "pixel");

  // Set last_path to the last profile's file for downstream embed logic.
  if (!cfgProfiles.m_profiles.empty())
    last_path = cfgProfiles.m_profiles.back()->m_iccFile.c_str();

  //Get and validate the source color space from the Cmm.
  icColorSpaceSignature SrcspaceSig = pTheCmm->GetSourceSpace();
  int nSrcColorSamples = icGetSpaceSamples(SrcspaceSig);
  int nSrcSamples = nSrcColorSamples;
  ICC_APPLY_TRACE(1, "event=source_space signature=0x%08x name=%s samples=%d",
                  static_cast<unsigned int>(SrcspaceSig),
                  ColorSpaceSignatureToStr(SrcspaceSig), nSrcColorSamples);

  if (nSrcSamples != (int)sn ) {
    //Allow color management to ignore extra samples when non extra samples match samples in profile
    if (sen != 0) {
      if (nSrcSamples != (int)(sn - sen)) {
        printf("Number of non-extra samples %u in image[%s] doesn't match device samples %d in first profile\n", sn - sen, cfgApply.m_srcImgFile.c_str(), nSrcSamples);
        return -1;
      }
      else
        nSrcSamples = sn;
    }
    else {
      printf("Number of samples %u in image[%s] doesn't match device samples %d in first profile\n", sn, cfgApply.m_srcImgFile.c_str(), nSrcSamples);
      return -1;
    }
  }

  //Get and validate the destination color space from the CMM.
  icColorSpaceSignature DestSpaceSig = pTheCmm->GetDestSpace();
  icColorSpaceSignature DestColorSpaceSig = DestSpaceSig;
  int nDestSamples = icGetSpaceSamples(DestSpaceSig);

  icColorSpaceSignature DestParentSpaceSig = pTheCmm->GetLastParentSpace();
  int nDestParentSamples = icGetSpaceSamples(DestParentSpaceSig);
  ICC_APPLY_TRACE(
    1,
    "event=destination_space signature=0x%08x name=%s samples=%d "
    "parent_signature=0x%08x parent_name=%s parent_samples=%d",
    static_cast<unsigned int>(DestSpaceSig),
    ColorSpaceSignatureToStr(DestSpaceSig), nDestSamples,
    static_cast<unsigned int>(DestParentSpaceSig),
    ColorSpaceSignatureToStr(DestParentSpaceSig), nDestParentSamples);
  
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
    photo = PHOTO_ICCLAB;
    break;

  default:
    photo = PHOTO_MINISBLACK;
    break;
  }

  unsigned long sbpp = (nSrcSamples * bps + 7) / 8;
  unsigned long dbpp = (nDestSamples * dbps  + 7)/ 8;

  //Open up output image using information from SrcImg and theCmm
  // GetXRes()/GetYRes() are taken from the source, so its RESOLUTIONUNIT has to come
  // with them; without it a centimetre-based source produced an inch-defaulted output
  // and the physical size shifted by 2.54x.  Same defect as iccSpecSepToTiff, because
  // both tools reach it through the same shared CTiffImg::Create() (#2220).
  phaseStartUs = IccApplyElapsedMicroseconds();
  if (!DstImg.Create(cfgApply.m_dstImgFile.c_str(), SrcImg.GetWidth(), SrcImg.GetHeight(), dbps, photo, nDestSamples, nExtraSamples, SrcImg.GetXRes(), SrcImg.GetYRes(), bCompress, bSeparation, SrcImg.GetResolutionUnit())) {
    ICC_APPLY_TRACE(1, "event=destination_create_failed path=%s duration_us=%llu",
                    cfgApply.m_dstImgFile.c_str(),
                    IccApplyElapsedMicroseconds() - phaseStartUs);
    printf("Unable to create Tiff file - '%s'\n", cfgApply.m_dstImgFile.c_str());
    return -1;
  }
  ICC_APPLY_TRACE(
    1,
    "event=destination_created path=%s duration_us=%llu bytes_per_line=%u "
    "photo=%u samples=%d extra_samples=%d",
    cfgApply.m_dstImgFile.c_str(),
    IccApplyElapsedMicroseconds() - phaseStartUs, DstImg.GetBytesPerLine(),
    photo, nDestSamples, nExtraSamples);

  //Embed the last profile into output image as needed
  if (bEmbed && last_path) {
    phaseStartUs = IccApplyElapsedMicroseconds();
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
    ICC_APPLY_TRACE(1, "event=profile_embed path=%s bytes=%zu duration_us=%llu",
                    last_path, length,
                    IccApplyElapsedMicroseconds() - phaseStartUs);
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
  size_t nSrcRowBytes = 0;
  size_t nDstRowBytes = 0;
  if (bUseRowApply) {
    if (!GetFloatRowByteCount(SrcImg.GetWidth(), nSrcColorSamples, nSrcRowBytes) ||
        !GetFloatRowByteCount(SrcImg.GetWidth(), nDestSamples, nDstRowBytes) ||
        !AddPixelBufSlack(nSrcRowBytes) ||
        !AddPixelBufSlack(nDstRowBytes)) {
      printf("Invalid row buffer size!\n");
      free(pSBuf);
      free(pDBuf);
      return -1;
    }

    pSrcRowBuf = (icFloatNumber*)calloc(1, nSrcRowBytes);
    pDstRowBuf = (icFloatNumber*)calloc(1, nDstRowBytes);
    if (!pSrcRowBuf || !pDstRowBuf) {
      printf("Out of Memory!\n");
      free(pSBuf);
      free(pDBuf);
      free(pSrcRowBuf);
      free(pDstRowBuf);
      return -1;
    }
  }
  ICC_APPLY_TRACE(
    1,
    "event=buffers_ready source_line_bytes=%u destination_line_bytes=%u "
    "source_float_row_bytes=%zu destination_float_row_bytes=%zu",
    SrcImg.GetBytesPerLine(), DstImg.GetBytesPerLine(), nSrcRowBytes,
    nDstRowBytes);

  //Allocate pixel buffers for performing encoding transformations
  CIccPixelBuf SrcPixel(nSrcSamples+16), DestPixel(nDestSamples+16), Pixel(icIntMax(nSrcSamples, nDestSamples)+16);
  int lastPer = -1;
  int curper;

  // Boundary rule (per Max Derhak): TIFF pixel values use a *device encoding*
  // regardless of color space. Integer formats map linearly to [0, 1] via
  // division/multiplication by the format's full-scale; floating point passes
  // through unchanged. Any PCS-encoding bridging is handled inside the CMM's
  // first/last xforms, so the boundary code here is uniform and does not
  // depend on TIFF photometric or PCS color-space signatures.
  auto decodePixel = [&](icFloatNumber *pPixel, unsigned char *pSrcBytes) -> bool {
    switch(bps) {
      case 8: {
        const icUInt8Number *pSPixel = pSrcBytes;
        for (k=0; k<nSrcColorSamples; k++)
          pPixel[k] = (icFloatNumber)pSPixel[k] / 255.0f;
        break;
      }
      case 16: {
        const icUInt16Number *pSPixel = (const icUInt16Number*)pSrcBytes;
        for (k=0; k<nSrcColorSamples; k++)
          pPixel[k] = (icFloatNumber)pSPixel[k] / 65535.0f;
        break;
      }
      case 32:
        if (sizeof(icFloatNumber)==sizeof(icFloat32Number)) {
          memcpy(pPixel, pSrcBytes, nSrcColorSamples * sizeof(icFloat32Number));
        }
        else {
          const icFloat32Number *pSPixel = (const icFloat32Number*)pSrcBytes;
          for (k=0; k<nSrcColorSamples; k++)
            pPixel[k] = (icFloatNumber)pSPixel[k];
        }
        break;

      default:
        printf("Invalid source bit depth\n");
        return false;
    }

    return true;
  };

  auto encodePixel = [&](unsigned char *pDstBytes, icFloatNumber *pPixel) -> bool {
    switch(dbps) {
      case 8: {
        icUInt8Number *pDPixel = pDstBytes;
        for (k=0; k<nDestSamples; k++)
          pDPixel[k] = UnitClipToUInt8(pPixel[k]);
        break;
      }
      case 16: {
        icUInt16Number *pDPixel = (icUInt16Number*)pDstBytes;
        for (k=0; k<nDestSamples; k++)
          pDPixel[k] = UnitClipToUInt16(pPixel[k]);
        break;
      }
      case 32:
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
  bool bApplySuccess = true;
  unsigned long long readTotalUs = 0;
  unsigned long long decodeTotalUs = 0;
  unsigned long long applyTotalNs = 0;
  unsigned long long encodeTotalUs = 0;
  unsigned long long writeTotalUs = 0;
  unsigned long long applyCalls = 0;
  unsigned int completedRows = 0;
  const int traceLevel = IccApplyTraceLevel();
  const bool timingEnabled = IccApplyTimingEnabled();
  const bool showProgress = !timingEnabled;
  const unsigned long long processingStartUs = IccApplyElapsedMicroseconds();
  ICC_APPLY_TRACE(
    1,
    "event=processing_begin rows=%u columns=%u pixels=%llu mode=%s "
    "source_samples=%d destination_samples=%d source_bpp=%lu destination_bpp=%lu",
    SrcImg.GetHeight(), SrcImg.GetWidth(),
    static_cast<unsigned long long>(SrcImg.GetWidth()) * SrcImg.GetHeight(),
    bUseRowApply ? "row" : "pixel", nSrcColorSamples, nDestSamples, sbpp, dbpp);
  for (i=0; i<(int)SrcImg.GetHeight(); i++) {
    const unsigned long long rowStartUs =
      timingEnabled ? IccApplyElapsedMicroseconds() : 0;
    unsigned long long stepStartUs = rowStartUs;
    ICC_APPLY_TRACE(2, "event=row_begin row=%d", i);
    if (!SrcImg.ReadLine(pSBuf)) {
      printf("Error reading line %d from Tiff file - '%s'\n", i, cfgApply.m_srcImgFile.c_str());
      bApplySuccess = false;
      break;
    }
    const unsigned long long rowReadUs = timingEnabled ?
      IccApplyElapsedMicroseconds() - stepStartUs : 0;
    readTotalUs += rowReadUs;
    if (bUseRowApply) {
      if (timingEnabled)
        stepStartUs = IccApplyElapsedMicroseconds();
      for (sptr=pSBuf, j=0; j<(int)SrcImg.GetWidth(); j++, sptr+=sbpp) {
        if (!decodePixel(pSrcRowBuf + j * nSrcColorSamples, sptr)) {
          free(pSBuf);
          free(pDBuf);
          free(pSrcRowBuf);
          free(pDstRowBuf);
          return -1;
        }
      }
      const unsigned long long rowDecodeUs = timingEnabled ?
        IccApplyElapsedMicroseconds() - stepStartUs : 0;
      decodeTotalUs += rowDecodeUs;

      const unsigned long long applyStartNs = timingEnabled ?
        IccApplyElapsedNanoseconds() : 0;
      const icStatusCMM applyStatus =
        pTheCmm->Apply(pDstRowBuf, pSrcRowBuf, SrcImg.GetWidth());
      const unsigned long long rowApplyNs = timingEnabled ?
        IccApplyElapsedNanoseconds() - applyStartNs : 0;
      applyTotalNs += rowApplyNs;
      applyCalls++;
      if (applyStatus != icCmmStatOk) {
        printf("Error applying profiles to line %d (status %d: %s)\n",
               i, (int)applyStatus, CIccCmm::GetStatusText(applyStatus));
        bApplySuccess = false;
        break;
      }

      if (timingEnabled)
        stepStartUs = IccApplyElapsedMicroseconds();
      for (dptr=pDBuf, j=0; j<(int)SrcImg.GetWidth(); j++, dptr+=dbpp) {
        if (traceLevel >= 3) {
          ICC_APPLY_TRACE(
            3,
            "event=pixel row=%d column=%d src_first=%.9g src_last=%.9g "
            "dst_first=%.9g dst_last=%.9g",
            i, j,
            nSrcColorSamples > 0 ? pSrcRowBuf[j * nSrcColorSamples] : 0.0f,
            nSrcColorSamples > 0 ?
              pSrcRowBuf[j * nSrcColorSamples + nSrcColorSamples - 1] : 0.0f,
            nDestSamples > 0 ? pDstRowBuf[j * nDestSamples] : 0.0f,
            nDestSamples > 0 ?
              pDstRowBuf[j * nDestSamples + nDestSamples - 1] : 0.0f);
        }
        if (!encodePixel(dptr, pDstRowBuf + j * nDestSamples)) {
          free(pSBuf);
          free(pDBuf);
          free(pSrcRowBuf);
          free(pDstRowBuf);
          return -1;
        }
      }
      const unsigned long long rowEncodeUs = timingEnabled ?
        IccApplyElapsedMicroseconds() - stepStartUs : 0;
      encodeTotalUs += rowEncodeUs;
      ICC_APPLY_TRACE(
        2,
        "event=row_transform row=%d read_us=%llu decode_us=%llu apply_us=%llu "
        "encode_us=%llu pixels=%u",
        i, rowReadUs, rowDecodeUs, rowApplyNs / 1000ULL, rowEncodeUs, SrcImg.GetWidth());
    }
    else {
      unsigned long long rowDecodeUs = 0;
      unsigned long long rowApplyNs = 0;
      unsigned long long rowEncodeUs = 0;
      for (sptr=pSBuf, dptr=pDBuf, j=0; j<(int)SrcImg.GetWidth(); j++, sptr+=sbpp, dptr+=dbpp) {
        if (timingEnabled)
          stepStartUs = IccApplyElapsedMicroseconds();
        if (!decodePixel(SrcPixel, sptr)) {
          free(pSBuf);
          free(pDBuf);
          free(pSrcRowBuf);
          free(pDstRowBuf);
          return -1;
        }
        if (timingEnabled)
          rowDecodeUs += IccApplyElapsedMicroseconds() - stepStartUs;

        //Use CMM to convert SrcPixel to DestPixel
        const unsigned long long applyStartNs = timingEnabled ?
          IccApplyElapsedNanoseconds() : 0;
        pTheCmm->Apply(DestPixel, SrcPixel);
        const unsigned long long pixelApplyNs = timingEnabled ?
          IccApplyElapsedNanoseconds() - applyStartNs : 0;
        rowApplyNs += pixelApplyNs;
        applyCalls++;
        if (traceLevel >= 3) {
          ICC_APPLY_TRACE(
            3,
            "event=pixel row=%d column=%d apply_us=%llu src_first=%.9g "
            "src_last=%.9g dst_first=%.9g dst_last=%.9g",
            i, j, pixelApplyNs / 1000ULL,
            nSrcColorSamples > 0 ? SrcPixel[0] : 0.0f,
            nSrcColorSamples > 0 ? SrcPixel[nSrcColorSamples - 1] : 0.0f,
            nDestSamples > 0 ? DestPixel[0] : 0.0f,
            nDestSamples > 0 ? DestPixel[nDestSamples - 1] : 0.0f);
        }

        if (timingEnabled)
          stepStartUs = IccApplyElapsedMicroseconds();
        if (!encodePixel(dptr, DestPixel)) {
          free(pSBuf);
          free(pDBuf);
          free(pSrcRowBuf);
          free(pDstRowBuf);
          return -1;
        }
        if (timingEnabled)
          rowEncodeUs += IccApplyElapsedMicroseconds() - stepStartUs;
      }
      decodeTotalUs += rowDecodeUs;
      applyTotalNs += rowApplyNs;
      encodeTotalUs += rowEncodeUs;
      ICC_APPLY_TRACE(
        2,
        "event=row_transform row=%d read_us=%llu decode_us=%llu apply_us=%llu "
        "encode_us=%llu pixels=%u",
        i, rowReadUs, rowDecodeUs, rowApplyNs / 1000ULL, rowEncodeUs, SrcImg.GetWidth());
    }

    //Output the converted pixels to the destination image
    if (timingEnabled)
      stepStartUs = IccApplyElapsedMicroseconds();
    if (!DstImg.WriteLine(pDBuf)) {
      printf("Error writing line %d to Tiff file - '%s'\n", i, cfgApply.m_dstImgFile.c_str());
      bApplySuccess = false;
      break;
    }
    const unsigned long long rowWriteUs = timingEnabled ?
      IccApplyElapsedMicroseconds() - stepStartUs : 0;
    writeTotalUs += rowWriteUs;
    completedRows++;
    ICC_APPLY_TRACE(2, "event=row_end row=%d write_us=%llu total_us=%llu",
                    i, rowWriteUs,
                    IccApplyElapsedMicroseconds() - rowStartUs);

    //Display status of how much we have accomplished
    if (showProgress) {
      curper = static_cast<int>((static_cast<float>(i + 1) * 100.0f) /
                                static_cast<float>(SrcImg.GetHeight()));
      if (curper !=lastPer) {
        printf("\r%d%%", curper);
        lastPer = curper;
      }
    }
  }
  if (showProgress)
    printf("\n");
  const unsigned long long processingElapsedUs =
    IccApplyElapsedMicroseconds() - processingStartUs;
  const unsigned long long totalPixels =
    static_cast<unsigned long long>(SrcImg.GetWidth()) * completedRows;
  const double megaPixelsPerSecond = processingElapsedUs ?
    static_cast<double>(totalPixels) / static_cast<double>(processingElapsedUs) :
    0.0;
  if (timingEnabled) {
    const double applyPercent = processingElapsedUs ?
      static_cast<double>(applyTotalNs) * 100.0 /
        (static_cast<double>(processingElapsedUs) * 1000.0) : 0.0;
    const CIccApplyThreadedCmm *pThreadedApply =
      dynamic_cast<const CIccApplyThreadedCmm*>(pTheCmm->GetApply());
    const unsigned long long asyncWorkerStrips = pThreadedApply ?
      pThreadedApply->GetAsyncWorkerStripCount() : 0;
    printf("[TIMING] Loop ms: %.3f\n", processingElapsedUs / 1000.0);
    printf("[TIMING] Apply ms: %.3f\n", applyTotalNs / 1000000.0);
    printf("[TIMING] Apply pct: %.3f\n", applyPercent);
    printf("[TIMING] Apply calls: %llu\n", applyCalls);
    printf("[TIMING] Async worker strips: %llu\n", asyncWorkerStrips);
  }
  ICC_APPLY_TRACE(
    1,
    "event=processing_end success=%d elapsed_us=%llu completed_rows=%u pixels=%llu "
    "megapixels_per_second=%.6f read_us=%llu decode_us=%llu apply_us=%llu "
    "encode_us=%llu write_us=%llu",
    bApplySuccess ? 1 : 0, processingElapsedUs, completedRows, totalPixels,
    megaPixelsPerSecond, readTotalUs, decodeTotalUs, applyTotalNs / 1000ULL,
    encodeTotalUs, writeTotalUs);

  //Clean everything up by closeing files and freeing buffers
  SrcImg.Close();

  free(pSBuf);
  free(pDBuf);
  free(pSrcRowBuf);
  free(pDstRowBuf);

  DstImg.Close();

  ICC_APPLY_TRACE(1, "event=process_end exit_code=%d total_elapsed_us=%llu",
                  bApplySuccess ? 0 : -1, IccApplyElapsedMicroseconds());
  return bApplySuccess ? 0 : -1;
}
