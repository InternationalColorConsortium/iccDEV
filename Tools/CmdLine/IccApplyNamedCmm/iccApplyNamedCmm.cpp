/*
    File:       IccApplyNamedCmm.cpp

    Contains:   Console app that applies profiles to text data geting test results

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
 // -Initial implementation by Max Derhak 5-15-2003
 // -Modification to support iccMAX by Max Derhak in 2014
 // -Addition of JSON configuraiton by Max Derhak in 2024
 //
 //////////////////////////////////////////////////////////////////////


#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccMpeCalc.h"
#include "IccProfLibVer.h"
#include "IccConnect.h"
#include "../IccCmdLineUtil.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <cstdlib>


// ============================================================================

static
FILE* icOpenWriteTextFile(const char* szFname)
{
  return icOpenRegularWriteTextFile(szFname);
}

// ============================================================================

//----------------------------------------------------
// Function Declarations
//----------------------------------------------------
#define IsSpacePCS(x) ((x)==icSigXYZData || (x)==icSigLabData)



class CIccLogDebugger : public IIccCalcDebugger
{
public:

  virtual ~CIccLogDebugger() {}

  void reset()
  {
    m_log.clear();
  }

  std::list<std::string> m_log;

  virtual void BeginApply()
  {
    m_log.push_back("Begin Calc Apply");
  }

  virtual void EndApply()
  {
    m_log.push_back("End Calculator Apply");
    m_log.push_back("");
  }

  virtual bool BeforeOp(SIccCalcOp* op, SIccOpState& os, SIccCalcOp* /*ops*/)
  {
    if (op->sig == icSigIfOp || op->sig == icSigSelectOp) {
      std::string str = "Start:";
      std::string opDesc;
      op->Describe(opDesc, 100);
      const size_t bufSize = 200;
      char buf[bufSize];
      snprintf(buf, bufSize, "%7s    ", opDesc.c_str());
      str += buf;
      for (int j = 0; j < (int)os.pStack->size(); j++) {
        snprintf(buf, bufSize, " %.4f", (*os.pStack)[j]);
        str += buf;
      }
      m_log.push_back(str);
    }
    return false;
  }

  virtual bool AfterOp(SIccCalcOp* op, SIccOpState& os, SIccCalcOp* /*ops*/)
  {
    std::string str;
    const size_t bufSize = 200;
    char buf[bufSize];
    if (op->sig == icSigDataOp) {
      snprintf(buf, bufSize, "%13s    ", "data");
      str = buf;
    }
    else {
      bool bEnd = false;
      if (op->sig == icSigIfOp || op->sig == icSigSelectOp) {
        str += "End:  ";
        bEnd = true;
      }
      std::string opDesc;
      op->Describe(opDesc, 100);
      
      if (bEnd)
        snprintf(buf, bufSize, "%7s    ", opDesc.c_str());
      else
        snprintf(buf, bufSize, "%13s    ", opDesc.c_str());
      str += buf;
    }

    for (int j = 0; j < (int)os.pStack->size(); j++) {
      snprintf(buf, bufSize, " %.4f", (*os.pStack)[j]);
      str += buf;
    }
    m_log.push_back(str);

    if (op->sig == icSigIfOp || op->sig == icSigSelectOp)
      m_log.push_back("");

    return false;
  }
  virtual void Error(const char* szMsg)
  {
    m_log.push_back(szMsg);
  }
};

typedef std::shared_ptr<CIccLogDebugger> LogDebuggerPtr;

//----------------------------------------------------
// Function Definitions
//----------------------------------------------------


void Usage()
{
  printf("iccApplyNamedCmm built with IccProfLib version " ICCPROFLIBVER "\n\n");

  printf("Usage 1: iccApplyNamedCmm -cfg config_file_path\n");
  printf("  Where config_file_path is a json formatted ICC profile application configuration file\n\n");
  printf("Usage 2: iccApplyNamedCmm (-exportcfg/-exportcfganddata config_file_path} {-debugcalc} data_file_path final_data_encoding{:FmtPrecision{:FmtDigits}} interpolation {{-ENV:Name value} profile_file_path Rendering_intent {-PCC connection_conditions_path}}\n\n");
  
  printf("  For final_data_encoding:\n");
  printf("    0 - icEncodeValue (converts to/from lab encoding when samples=3)\n");
  printf("    1 - icEncodePercent\n");
  printf("    2 - icEncodeUnitFloat (may clip to 0.0 to 1.0)\n");
  printf("    3 - icEncodeFloat\n");
  printf("    4 - icEncode8Bit\n");
  printf("    5 - icEncode16Bit\n");
  printf("    6 - icEncode16BitV2\n\n");

  printf("    FmtPrecision - formatting for # of digits after decimal (default=4)\n");
  printf("    FmtDigits - formatting for total # of digits (default=5+FmtPrecision)\n\n");

  printf("  For interpolation:\n");
  printf("    0 - Linear\n");
  printf("    1 - Tetrahedral\n\n");

  printf("  For Rendering_intent:\n");
  printf("     0 - Perceptual\n");
  printf("     1 - Relative\n");
  printf("     2 - Saturation\n");
  printf("     3 - Absolute\n");
  printf("     10 + Intent - without D2Bx/B2Dx\n");
  printf("     20 + Intent - Preview\n");
  printf("     30 - Gamut\n");
  printf("     33 - Gamut Absolute\n");
  printf("     40 + Intent - with BPC\n");
  printf("     50 - BDRF Model\n");
  printf("     60 - BDRF Light\n");
  printf("     70 - BDRF Output\n");
  printf("     80 - MCS connection\n");
  printf("     90 + Intent - Colorimetric Only\n");
  printf("    100 + Intent - Spectral Only\n");
  printf("    +1000 - Use Luminance based PCS adjustment\n");
  printf("   +10000 - Use V5 sub-profile if present\n");
  printf("  +100000 - Use HToS tag if present\n");
  printf(" +1000000 - NamedColor over black (icSigNmclSpectralOverBlackMbr 'spcb')\n");
  printf(" +2000000 - NamedColor over gray  (icSigNmclSpectralOverGrayMbr 'spcg')\n");
}


//===================================================

int main(int argc, const char* argv[])
{
  int minargs = 2;
  if (argc < minargs) {
    Usage();
    return 0;
  }

  CIccCfgDataApply cfgApply;
  CIccCfgProfileSequence cfgProfiles;
  CIccCfgColorData cfgData;

  if (argc > 2 && !stricmp(argv[1], "-cfg")) {
    json cfg;
    if (!loadJsonFrom(cfg, argv[2]) || !cfg.is_object()) {
      printf("Unable to read configuration from '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }

    if (cfg.find("dataFiles") == cfg.end() || !cfgApply.fromJson(cfg["dataFiles"])) {
      printf("Unable to parse dataFile configuration from '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }

    if (cfg.find("profileSequence") == cfg.end() || !cfgProfiles.fromJson(cfg["profileSequence"])) {
      printf("Unable to parse profileSequence configuration from '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }

    if (cfgApply.m_srcType == icCfgColorData) {
      if (cfgApply.m_srcFile.empty()) {
        if (!cfgData.fromJson(cfg["colorData"])) {
          printf("Unable to parse colorData configuration from '%s'\n", argv[2]);
          return EXIT_FAILURE;
        }
      }
      else {
        json data;
        if (!loadJsonFrom(data, cfgApply.m_srcFile.c_str()) || !cfgData.fromJson(data)) {
          printf("Unable to load color data from '%s'\n", cfgApply.m_srcFile.c_str());
          return EXIT_FAILURE;
        }
      }
    }
    else if (cfgApply.m_srcType == icCfgIt8) {
      cfgData.m_srcSpace = cfgApply.m_srcSpace;
      if (cfgApply.m_srcFile.empty() || !cfgData.fromIt8(cfgApply.m_srcFile.c_str())) {
        printf("Unable to parse IT8 data file '%s'\n", cfgApply.m_srcFile.c_str());
        return EXIT_FAILURE;
      }
    }
    else if (cfgApply.m_srcType == icCfgLegacy) {
      if (!cfgData.fromLegacy(cfgApply.m_srcFile.c_str())) {
        printf("Unable to parse legacy data file '%s'\n", cfgApply.m_srcFile.c_str());
        return EXIT_FAILURE;
      }
    }
  }
  else {
    std::string exportFile;
    bool bExportData = false;

    argv++;
    argc--;

    if (argc > 2 && 
        (!stricmp(argv[0], "-exportcfg") ||
         !stricmp(argv[0], "-exportcfganddata"))) {
      exportFile = argv[1];
      if (!stricmp(argv[0], "-exportcfganddata"))
        bExportData = true;
      argv += 2;
      argc -= 2;
    }

    if (argc > 1 && !stricmp(argv[0], "-debugcalc")) {
      cfgApply.m_debugCalc = true;

      argv++;
      argc--;
    }

    int nArg = cfgApply.fromArgs(&argv[0], argc);
    if (!nArg) {
      printf("Unable to parse configuration arguments\n");
      return EXIT_FAILURE;
    }
    argv += nArg;
    argc -= nArg;

    nArg = cfgProfiles.fromArgs(&argv[0], argc);
    if (!nArg) {
      printf("Unable to parse profile sequence arguments\n");
      return EXIT_FAILURE;
    }

    if (cfgApply.m_srcType != icCfgLegacy || !cfgData.fromLegacy(cfgApply.m_srcFile.c_str())) {
      printf("Unable to parse legacy data file '%s'\n", cfgApply.m_srcFile.c_str());
      return EXIT_FAILURE;
    }

    if (!exportFile.empty()) {
      FILE* f = icOpenWriteTextFile(exportFile.c_str());
      if (f) {
        json cfgJson;
        json applyJson, profilesJson;

        cfgApply.toJson(applyJson);

        if (bExportData) {
          json dataJson;
          cfgData.toJson(dataJson);
          cfgJson["colorData"] = dataJson;

          applyJson["srcFile"] = nullptr;
          applyJson["srcType"] = "colorData";
        }

        cfgJson["dataFiles"] = applyJson;

        cfgProfiles.toJson(profilesJson);
        cfgJson["profileSequence"] = profilesJson;

        std::string jsonText = cfgJson.dump(1);
        size_t n = fwrite(jsonText.c_str(), 1, jsonText.size(), f);
        if (n != jsonText.size()) {
          printf("Error writing json config file '%s'\n", exportFile.c_str());
          fclose(f);
          return EXIT_FAILURE;
        }
        if (!icFlushAndClose(f)) {
          printf("Error closing json config file '%s'\n", exportFile.c_str());
          return EXIT_FAILURE;
        }
      }
      else {
        printf("Unable to export config file '%s'\n", exportFile.c_str());
        return EXIT_FAILURE;
      }
    }
  }
  LogDebuggerPtr pDebugger;
  
  if (cfgApply.m_debugCalc) {
    pDebugger = LogDebuggerPtr(new CIccLogDebugger());
    IIccCalcDebugger::SetDebugger(pDebugger.get());
  }

  const size_t precSize = 30;
  char precisionStr[precSize];
  snprintf(precisionStr, precSize, "%%%d.%dlf ", cfgApply.m_dstDigits, cfgApply.m_dstPrecision);

  icFloatColorEncoding srcEncoding, destEncoding;

  //Setup source encoding
  srcEncoding = cfgData.m_encoding;
  
  icColorSpaceSignature SrcspaceSig = cfgData.m_srcSpace;

  //If first profile colorspace is PCS and it matches the source data space then treat as input profile
  bool bInputProfile = !IsSpacePCS(SrcspaceSig);
  if (!bInputProfile && !cfgProfiles.m_profiles.empty()) {
    // Named CMM profile paths are intentional caller-selected inputs.
    // codeql[cpp/path-injection]
    CIccProfile* pProf = OpenIccProfile(cfgProfiles.m_profiles[0]->m_iccFile.c_str());
    if (pProf) {
      if (pProf->m_Header.deviceClass != icSigAbstractClass && IsSpacePCS(pProf->m_Header.colorSpace))
        bInputProfile = true;
      delete pProf;
    }
  }

  std::string sConnectError;
  std::unique_ptr<CIccConnectCmm> pConnect(
    CIccConnectCmm::CreateNamed(cfgProfiles, SrcspaceSig, bInputProfile, &sConnectError));

  if (!pConnect) {
    if (!sConnectError.empty())
      printf("Error - %s\n", sConnectError.c_str());
    else
      printf("Error - Unable to begin profile application - Possibly invalid or incompatible profiles\n");
    return EXIT_FAILURE;
  }

  CIccNamedColorCmm* pNamedCmm = pConnect->GetNamedCmm();
  CIccCmm* pMruCmm = nullptr;

  //Get and validate the source color space from pNamedCmm->
  SrcspaceSig = pNamedCmm->GetSourceSpace();
  icUInt32Number nSrcSamples = icGetSpaceSamples(SrcspaceSig);

  bool bClip = true;
  //We don't want to interpret device data as pcs encoded data
  if (bInputProfile && IsSpacePCS(SrcspaceSig)) {
    if (SrcspaceSig == icSigXYZPcsData)
      SrcspaceSig = icSigDevXYZData;
    else if (SrcspaceSig == icSigLabPcsData)
      SrcspaceSig = icSigDevLabData;

    if (srcEncoding == icEncodeFloat)
      bClip = false;
  }

  //Get and validate the destination color space from the CMM.
  icColorSpaceSignature DestspaceSig = pNamedCmm->GetDestSpace();
  int nDestSamples = icGetSpaceSamples(DestspaceSig);
  
  //Allocate pixel buffers for performing encoding transformations
  char SrcNameBuf[256], DestNameBuf[256];
  CIccPixelBuf SrcPixel(nSrcSamples+16), DestPixel(nDestSamples+16), Pixel(icIntMax(nSrcSamples, nDestSamples)+16);

  CIccCfgColorData outData;

  outData.m_space = DestspaceSig;

  destEncoding = cfgApply.m_dstEncoding;
  if(DestspaceSig==icSigNamedData)
    destEncoding = icEncodeValue;
  outData.m_encoding = destEncoding;

  outData.m_srcSpace = SrcspaceSig;

  if(SrcspaceSig==icSigNamedData)
    srcEncoding = icEncodeValue;
  outData.m_srcEncoding = srcEncoding;

  //Apply profiles to each input color
  for (auto dataIter = cfgData.m_data.begin(); dataIter != cfgData.m_data.end(); dataIter++) {
    CIccCfgDataEntry* pData = dataIter->get();

    int i;

    if (!pData)
      continue;
  
    if (pDebugger)
      pDebugger->reset();

    CIccCfgDataEntryPtr out(new CIccCfgDataEntry());

    out->m_srcName = pData->m_name;
    out->m_srcValues = pData->m_srcValues;

    //Are names coming is as an input?
    if(SrcspaceSig ==icSigNamedData) {

      const char* szName = pData->m_name.c_str();
      icFloatNumber tint;
      
      if (pData->m_values.size())
        tint = pData->m_values[0];
      else
        tint = 1.0;

      switch(pNamedCmm->GetInterface()) {
        case icApplyNamed2Pixel:
          {

            if(pNamedCmm->Apply(DestPixel, szName, tint)) {
              printf("Profile application failed.\n");
              return EXIT_FAILURE;
            }

            if(CIccCmm::FromInternalEncoding(DestspaceSig, destEncoding, DestPixel, DestPixel, destEncoding!=icEncodeFloat)) {
              printf("Invalid final data encoding\n");
              return EXIT_FAILURE;
            }

            for(i = 0; i<nDestSamples; i++) {
              out->m_values.push_back(DestPixel[i]);
            }
            break;
          }
        case icApplyNamed2Named:
          {
            if(pNamedCmm->Apply(DestNameBuf, SrcNameBuf, tint)) {
              printf("Profile application failed.\n");
              return EXIT_FAILURE;
            }

            out->m_name = DestNameBuf;
            break;
          }
        case icApplyPixel2Pixel:
        case icApplyPixel2Named:
        default:
          printf("Incorrect interface.\n");
          return EXIT_FAILURE;
      }      
    }
    else {
      for (icUInt32Number si = 0; si < nSrcSamples && si < pData->m_values.size(); si++) {
        Pixel[si] = pData->m_values[si];
      }

      out->m_srcValues = pData->m_values;

      if(CIccCmm::ToInternalEncoding(SrcspaceSig, srcEncoding, SrcPixel, Pixel, bClip)) {
        printf("Invalid source data encoding\n");
        return EXIT_FAILURE;
      }

      switch(pNamedCmm->GetInterface()) {
        case icApplyPixel2Pixel:
          {
            if (pMruCmm) {
              if (pMruCmm->Apply(DestPixel, SrcPixel)) {
                printf("Profile application failed.\n");
                return EXIT_FAILURE;
              }
            }
            else if(pNamedCmm->Apply(DestPixel, SrcPixel)) {
              printf("Profile application failed.\n");
              return EXIT_FAILURE;
            }
            if(CIccCmm::FromInternalEncoding(DestspaceSig, destEncoding, DestPixel, DestPixel)) {
              printf("Invalid final data encoding\n");
              return EXIT_FAILURE;
            }

            for(i = 0; i<nDestSamples; i++) {
              out->m_values.push_back(DestPixel[i]);
            }
            break;
          }
        case icApplyPixel2Named:
          {
            if(pNamedCmm->Apply(DestNameBuf, SrcPixel)) {
              printf("Profile application failed.\n");
              return EXIT_FAILURE;
            }
            out->m_name = DestNameBuf;
            break;
          }
        case icApplyNamed2Pixel:
        case icApplyNamed2Named:
        default:
          printf("Incorrect interface.\n");
          return EXIT_FAILURE;
      }      
    }

    if (pDebugger)
      out->m_debugInfo = pDebugger->m_log;

    outData.m_data.push_back(out);
  }

  //Now output the data
//   cfgApply.m_dstType = icCfgIt8;
//   cfgApply.m_dstDigits = 0;
//   cfgApply.m_dstPrecision = 2;
//   cfgApply.m_debugCalc = false;

  if (cfgApply.m_dstType == icCfgLegacy) {
    outData.toLegacy(cfgApply.m_dstFile.c_str(), cfgProfiles.m_profiles, cfgApply.m_dstDigits, cfgApply.m_dstPrecision, cfgApply.m_debugCalc);
  }
  else if (cfgApply.m_dstType == icCfgColorData) {
    json out;
    json seq;
    cfgProfiles.toJson(seq);
    if (seq.is_object())
      out["appliedProfileSequence"] = seq;

    json data;
    outData.toJson(data);
    if (data.is_object())
      out["colorData"] = data;

    if (out.is_object())
      saveJsonAs(out, cfgApply.m_dstFile.c_str());
  }
  else if (cfgApply.m_dstType==icCfgIt8) {
    outData.toIt8(cfgApply.m_dstFile.c_str(), cfgApply.m_dstDigits, cfgApply.m_dstPrecision);
  }
  else {
    printf("Unsupported output format\n");
    delete pMruCmm;

    return EXIT_FAILURE;
  }

  delete pMruCmm;

  return 0;
}
