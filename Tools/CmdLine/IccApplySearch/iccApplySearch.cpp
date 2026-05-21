/*
    File:       iccApplySearch.cpp

    Contains:   Console app that applies profiles to text data geting test results

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2025 The International Color Consortium. All rights
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
 // -Initial implementation by Max Derhak 5-20-2025
 //
 //////////////////////////////////////////////////////////////////////


#include "IccCmmSearch.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccMpeCalc.h"
#include "IccProfLibVer.h"
#include "IccSearch.h"
#include "IccConnect.h"
#include <memory>
#include <vector>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif


// ============================================================================

static
FILE* icOpenWriteTextFile(const char* szFname)
{
  if (!szFname || !szFname[0])
    return stdout;

#if defined(_WIN32)
  return fopen(szFname, "wt");
#else
  int fd = open(szFname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0)
    return nullptr;

  FILE* f = fdopen(fd, "wt");
  if (!f)
    close(fd);

  return f;
#endif
}

// ============================================================================


//----------------------------------------------------
// Function Declarations
//----------------------------------------------------

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

static
bool IsSpacePCS( const icColorSpaceSignature &x )
{
    return ((x)==icSigXYZData || (x)==icSigLabData);
}


void Usage()
{
  printf("Usage 1: iccApplySearch -cfg config_file_path\n");
  printf("  Where config_file_path is a json formatted ICC profile application configuration file\n\n");
  printf("Usage 2: iccApplySearch {-debugcalc} data_file_path encoding[:precision[:digits]] interpolation {-ENV:tag value} profile1_path intent1 {{-ENV:tag value} middle_profile_path mid_intent} {-ENV:tag value} profile2_path intent2 -INIT init_intent2 {pcc_path1 weight1 ...}\n");
  printf("Built with IccProfLib version " ICCPROFLIBVER "\n\n");
  
  printf("  For final_data_encoding:\n");
  printf("    0 - icEncodeValue (converts to/from lab encoding when samples=3)\n");
  printf("    1 - icEncodePercent\n");
  printf("    2 - icEncodeUnitFloat (may clip to 0.0 to 1.0)\n");
  printf("    3 - icEncodeFloat\n");
  printf("    4 - icEncode8Bit\n");
  printf("    5 - icEncode16Bit\n");
  printf("    6 - icEncode16BitV2\n\n");

  printf("  FmtPrecision - formatting for # of digits after decimal (default=4)\n");
  printf("  FmtDigits - formatting for total # of digits (default=5+FmtPrecision)\n\n");

  printf("  For interpolation:\n");
  printf("    0 - Linear\n");
  printf("    1 - Tetrahedral\n\n");

  printf("  For init_intent/intent1/intent2/mid_intent:\n");
  printf("     0 - Perceptual\n");
  printf("     1 - Relative\n");
  printf("     2 - Saturation\n");
  printf("     3 - Absolute\n");
  printf("     10 + Intent - without D2Bx/B2Dx\n");
  printf("     40 + Intent - with BPC\n");
  printf("     90 + Intent - Colorimetric Only\n");
  printf("    100 + Intent - Spectral Only\n");
  printf(" +10000 - Use V5 sub-profile if present\n");
  
  printf("\n");
}


//===================================================

int main(int argc, const char* argv[])
{
  int minargs = 3;  // name -cfg file.json
  if (argc < minargs) {
    Usage();
    return 0;
  }

  CIccCfgDataApply cfgApply;
  CIccCfgSearchApply cfgSearchApply;
  CIccCfgColorData cfgData;

  if (!stricmp(argv[1], "-cfg")) {
    json cfg;
    if (!loadJsonFrom(cfg, argv[2]) || !cfg.is_object()) {
      printf("Unable to read configuration from '%s'\n", argv[2]);
      return -1;
    }

    if (cfg.find("dataFiles") == cfg.end() || !cfgApply.fromJson(cfg["dataFiles"])) {
      printf("Unable to parse dataFile configuration from '%s'\n", argv[2]);
      return -1;
    }

    if (cfg.find("searchApply") == cfg.end() || !cfgSearchApply.fromJson(cfg["searchApply"])) {
      printf("Unable to parse profileSequence configuration from '%s'\n", argv[2]);
      return -1;
    }

    if (cfgApply.m_srcType == icCfgColorData) {
      if (cfgApply.m_srcFile.empty()) {
        if (!cfgData.fromJson(cfg["colorData"])) {
          printf("Unable to parse colorData configuration from '%s'\n", argv[2]);
          return -1;
        }
      }
      else {
        json data;
        if (!loadJsonFrom(data, cfgApply.m_srcFile.c_str()) || !cfgData.fromJson(data)) {
          printf("Unable to load color data from '%s'\n", cfgApply.m_srcFile.c_str());
          return -1;
        }
      }
    }
    else if (cfgApply.m_srcType == icCfgIt8) {
      cfgData.m_srcSpace = cfgApply.m_srcSpace;
      if (cfgApply.m_srcFile.empty() || !cfgData.fromIt8(cfgApply.m_srcFile.c_str())) {
        printf("Unable to parse IT8 data file '%s'\n", cfgApply.m_srcFile.c_str());
        return -1;
      }
    }
    else if (cfgApply.m_srcType == icCfgLegacy) {
      if (!cfgData.fromLegacy(cfgApply.m_srcFile.c_str())) {
        printf("Unable to parse legacy data file '%s'\n", cfgApply.m_srcFile.c_str());
        return -1;
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
      return -1;
    }
    argv += nArg;
    argc -= nArg;

    nArg = cfgSearchApply.fromArgs(&argv[0], argc);
    if (!nArg) {
      printf("Unable to parse profile sequence arguments\n");
      return -1;
    }

    if (cfgApply.m_srcType != icCfgLegacy || !cfgData.fromLegacy(cfgApply.m_srcFile.c_str())) {
      printf("Unable to parse legacy data file '%s'\n", cfgApply.m_srcFile.c_str());
      return -1;
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

        cfgSearchApply.toJson(profilesJson);
        cfgJson["searchApply"] = profilesJson;


        std::string jsonText = cfgJson.dump(1);
        fwrite(jsonText.c_str(), 1, jsonText.size(), f);
        fclose(f);
      }
      else {
        printf("Unable to export config file '%s'\n", exportFile.c_str());
      }
    }

  }

  if (cfgSearchApply.m_profiles.size() != 2 && cfgSearchApply.m_profiles.size() != 3) {
    printf("Only sequences of 2 or 3 profiles are supported\n");
    return -1;
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

  std::string sConnectError;
  std::unique_ptr<CIccConnectCmm> pConnect(
    CIccConnectCmm::CreateSearch(cfgSearchApply, &sConnectError));

  if (!pConnect) {
    if (!sConnectError.empty())
      printf("Error - %s\n", sConnectError.c_str());
    printf("Unable to begin profile application - Possibly invalid or incompatible profiles\n");
    return -1;
  }

  CIccCmmSearch* pCmm = pConnect->GetSearchCmm();
  CIccCmm *pMruCmm = NULL;

  //Get and validate the source color space from the CMM.
  icColorSpaceSignature SrcspaceSig = pCmm->GetSourceSpace();
  icUInt32Number nSrcSamples = icGetSpaceSamples(SrcspaceSig);

  bool bClip = true;
  //We don't want to interpret device data as pcs encoded data
  if (IsSpacePCS(SrcspaceSig)) {
    if (SrcspaceSig == icSigXYZPcsData)
      SrcspaceSig = icSigDevXYZData;
    else if (SrcspaceSig == icSigLabPcsData)
      SrcspaceSig = icSigDevLabData;

    if (srcEncoding == icEncodeFloat)
      bClip = false;
  }

  //Get and validate the destination color space from the CMM.
  icColorSpaceSignature DestspaceSig = pCmm->GetDestSpace();
  icUInt32Number nDestSamples = icGetSpaceSamples(DestspaceSig);
  
  //Allocate pixel buffers for performing encoding transformations
  CIccPixelBuf SrcPixel(nSrcSamples+16), DestPixel(nDestSamples+16), Pixel(icIntMax(nSrcSamples, nDestSamples)+16);

  CIccCfgColorData outData;

  outData.m_space = DestspaceSig;

  destEncoding = cfgApply.m_dstEncoding;
  outData.m_encoding = destEncoding;

  outData.m_srcSpace = SrcspaceSig;

  outData.m_srcEncoding = srcEncoding;

  //Apply profiles to each input color
  for (auto dataIter = cfgData.m_data.begin(); dataIter != cfgData.m_data.end(); dataIter++) {
    CIccCfgDataEntry* pData = dataIter->get();

    size_t i;

    if (!pData)
      continue;
  
    if (pDebugger)
      pDebugger->reset();

    CIccCfgDataEntryPtr out(new CIccCfgDataEntry());

    out->m_srcName = pData->m_name;
    out->m_srcValues = pData->m_srcValues;

    
    for (i = 0; i < nSrcSamples && i < pData->m_values.size(); i++) {
      Pixel[i] = pData->m_values[i];
    }

    out->m_srcValues = pData->m_values;

    if(CIccCmm::ToInternalEncoding(SrcspaceSig, srcEncoding, SrcPixel, Pixel, bClip)) {
      printf("Invalid source data encoding\n");
      return -1;
    }

    if (pMruCmm) {
      if (pMruCmm->Apply(DestPixel, SrcPixel)) {
        printf("Profile application failed.\n");
        return -1;
      }
    }
    else if(pCmm->Apply(DestPixel, SrcPixel)) {
      printf("Profile application failed.\n");
      return -1;
    }
    if(CIccCmm::FromInternalEncoding(DestspaceSig, destEncoding, DestPixel, DestPixel)) {
      printf("Invalid final data encoding\n");
      return -1;
    }

    for(i = 0; i < nDestSamples; i++) {
      out->m_values.push_back(DestPixel[i]);
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
    outData.toLegacy(cfgApply.m_dstFile.c_str(), cfgSearchApply.m_profiles, cfgApply.m_dstDigits, cfgApply.m_dstPrecision, cfgApply.m_debugCalc);
  }
  else if (cfgApply.m_dstType == icCfgColorData) {
    json out;
    json seq;
    cfgSearchApply.toJson(seq);
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

    return -1;
  }

  delete pMruCmm;

  return 0;
}
