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
#include "IccLibConnectVer.h"
#include "IccConnect.h"
#include "IccCmdLineUtil.h"
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
  printf("iccApplyNamedCmm built with IccProfLib version " ICCPROFLIBVER ", IccLibConnect Version " ICCLIBCONNECTVER "\n\n");

  printf("Usage 1: iccApplyNamedCmm {--evidence-json} -cfg config_file_path\n");
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

  // #2124: this list read as though all seven selectors were always available,
  // so a Lab destination refusing icEncodePercent looked like a broken encoding
  // rather than a documented restriction. The valid set is per colour space --
  // the icFloatColorEncoding table in IccCmm.h, which the
  // ToInternalEncoding()/FromInternalEncoding() switches implement. Naming the
  // two exclusions the reference profiles actually hit keeps the note short.
  //
  // Deliberately does NOT quote the run-time diagnostic verbatim: the argument
  // regression greps for that exact line to prove a rejection happened, and
  // printing it here would let usage output satisfy those greps. For the same
  // reason the table is described as listing the per-space sets rather than
  // "the full set" -- it carries no source/destination axis, and the two
  // converters do not accept identical sets for every space, so it is a
  // pointer, not a promise. (It also omitted icEncodeUnitFloat for the two PCS
  // spaces when this note was written; #2146 fixed the source side that
  // omission described and brought the table into line.)
  printf("    Not every encoding is valid for every colour space: a 'Lab '\n");
  printf("    destination refuses icEncodePercent and an 'XYZ ' destination\n");
  printf("    refuses icEncode8Bit, each rejected when the data is converted\n");
  printf("    rather than here. IccCmm.h's icFloatColorEncoding table lists\n");
  printf("    the per-space encodings.\n\n");

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
  // Two corrections, both #2262.
  //
  // (1) The acronym was transposed, B-D-R-F.  ICC.2-2023 9.2.14-17 and 9.2.26-29
  // spell the tags brdfAToB0Tag..brdfDToB3Tag, and every identifier in the
  // library already agrees: icXformLutBRDFParam (IccCmm.h), icSigBRDFDToB0Tag,
  // CIccStructBRDF.  The same three lines carry the typo in iccApplyProfiles
  // and iccApplyToLink and are corrected there too.
  //
  // (2) These four codes take a rendering intent in their units digit exactly
  // as "20 + Intent" does, and printing them as flat values said they did not.
  // Each of the four transform types selects from a four-entry tag array
  // indexed by nTagIntent in CIccXform::Create() -- brdfSpectralParameter0 /
  // brdfColorimetricParameter0 for 50, BRDFDToB0 / BRDFAToB0 for 60,
  // BRDFMToS0 / BRDFMToB0 for 70, MToS0 / MToB0 for 80 (IccProfLib/IccCmm.cpp,
  // the icXformLutBRDFParam..icXformLutMCS cases).  80 is qualified because it
  // is the one code where that is only half true: icXformLutMCS offsets by
  // nTagIntent on its MVIS/Output branch (MToS0..3, MToB0..3) but reads a
  // single AToM0Tag for MultiplexIdentification/Input and a single MToA0Tag for
  // MultiplexLink, so 80..83 are indistinguishable in the to-MCS direction --
  // the same reason 30/31/32 are identical and gamut stays in the flat form.
  //
  // The decode that feeds them preserves the digit: fromArgs() falls through
  // its type switch for 5..8, so "nIntent % 10" reaches m_intent unchanged
  // (IccCmmConfig.cpp).  Reading these lines as "51 is not a valid code" is
  // what produced the refuted Finding 2 on #2261, so the omission had already
  // cost a review round.
  //
  // The transform names are the ones the exported configuration and
  // docs/icc-connect-config.schema.json publish for these same types --
  // brdfParam, brdfDirect, brdfMcsParam -- rather than the previous
  // "Model"/"Light"/"Output" wording, which matched neither the schema nor the
  // two sibling tools: nothing connected the old row for 60 to the
  // "transform": "brdfDirect" that -exportcfg writes when it is given.
  printf("     50 + Intent - BRDF Parameters\n");
  printf("     60 + Intent - BRDF Direct\n");
  printf("     70 + Intent - BRDF MCS Parameters\n");
  printf("     80 + Intent - MCS connection (Intent applies to MToS/MToB only)\n");
  printf("     90 + Intent - Colorimetric Only\n");
  printf("    100 + Intent - Spectral Only\n");
  printf("    +1000 - Use Luminance based PCS adjustment\n");
  printf("   +10000 - Use V5 sub-profile if present\n");
  printf("  +100000 - Use HToS tag if present\n");
  printf(" +1000000 - NamedColor over black (icSigNmclSpectralOverBlackMbr 'spcb')\n");
  printf(" +2000000 - NamedColor over gray  (icSigNmclSpectralOverGrayMbr 'spcg')\n");
  // The two overprint codes read as additive flags, and #2190 was filed by a
  // caller who combined them. They select mutually exclusive array members, so
  // say here that only one may be given rather than let "+3000000" look legal.
  printf("            (over black and over gray are alternatives, not flags:\n");
  printf("             only one of +1000000 / +2000000 may be given)\n");
}

static std::string GetProfileId(const char* profilePath)
{
  if (!profilePath || !profilePath[0])
    return std::string();

  CIccProfile* pProfile = OpenIccProfile(profilePath);
  if (!pProfile)
    return std::string();

  CIccInfo Fmt;
  std::string id;
  if (Fmt.IsProfileIDCalculated(&pProfile->m_Header.profileID))
    id = Fmt.GetProfileID(&pProfile->m_Header.profileID);
  delete pProfile;
  return id;
}

static void EmitTransformEvidenceJson(const char* inputPath, const char* profilePath,
                                      const char* outputPath)
{
  std::string inputDigest;
  std::string outputDigest;
  std::string profileId = GetProfileId(profilePath);
  bool hasInputDigest = icSha256File(inputPath, inputDigest);
  bool hasOutputDigest = icSha256File(outputPath, outputDigest);

  printf("{");
  printf("\"schema\":\"iccdev-qa-evidence/v1\",");
  printf("\"tool\":\"iccApplyNamedCmm\",");
  printf("\"input\":\"%s\",", icJsonEscape(inputPath).c_str());
  printf("\"profile\":\"%s\",", icJsonEscape(profilePath).c_str());
  printf("\"output\":\"%s\",", icJsonEscape(outputPath).c_str());
  printf("\"qaFlags\":[\"ICCDEV_FLAG_TRANSFORM\"],");
  if (hasInputDigest)
    printf("\"inputDigest\":\"%s\",", inputDigest.c_str());
  else
    printf("\"inputDigest\":null,");
  if (!profileId.empty())
    printf("\"profileId\":\"%s\",", icJsonEscape(profileId).c_str());
  else
    printf("\"profileId\":null,");
  if (hasOutputDigest)
    printf("\"outputDigest\":\"%s\",", outputDigest.c_str());
  else
    printf("\"outputDigest\":null,");
  printf("\"transform\":{");
  if (hasInputDigest)
    printf("\"inputDigest\":\"%s\",", inputDigest.c_str());
  else
    printf("\"inputDigest\":null,");
  if (!profileId.empty())
    printf("\"profileId\":\"%s\",", icJsonEscape(profileId).c_str());
  else
    printf("\"profileId\":null,");
  if (hasOutputDigest)
    printf("\"outputDigest\":\"%s\"", outputDigest.c_str());
  else
    printf("\"outputDigest\":null");
  printf("}}\n");
}

//===================================================

int main(int argc, const char* argv[])
{
  int minargs = 2;
  bool bEvidenceJson = false;

  if (argc > 1 && !stricmp(argv[1], "--evidence-json")) {
    bEvidenceJson = true;
    argv++;
    argc--;
  }

  if (argc < minargs) {
    Usage();
    return 0;
  }

  CIccCfgDataApply cfgApply;
  CIccCfgProfileSequence cfgProfiles;
  CIccCfgColorData cfgData;

  if (argc > 2 && !stricmp(argv[1], "-cfg")) {
    // Usage 1 is exactly "-cfg <path>"; every setting comes from the JSON file, so
    // there is nothing a further argument could mean. Anything beyond argv[2] was
    // read as configured-and-applied when it had in fact been ignored (#1674).
    if (argc != 3) {
      printf("Unexpected extra arguments for -cfg\n");
      return EXIT_FAILURE;
    }

    json cfg;
    if (!loadJsonFrom(cfg, argv[2]) || !cfg.is_object()) {
      printf("Unable to read configuration from '%s'\n", icSanitizeConsoleText(argv[2]).c_str());
      return EXIT_FAILURE;
    }

    if (cfg.find("dataFiles") == cfg.end() || !cfgApply.fromJson(cfg["dataFiles"])) {
      printf("Unable to parse dataFile configuration from '%s'\n", icSanitizeConsoleText(argv[2]).c_str());
      return EXIT_FAILURE;
    }

    if (cfg.find("profileSequence") == cfg.end() || !cfgProfiles.fromJson(cfg["profileSequence"])) {
      printf("Unable to parse profileSequence configuration from '%s'\n", icSanitizeConsoleText(argv[2]).c_str());
      return EXIT_FAILURE;
    }

    if (cfgApply.m_srcType == icCfgColorData) {
      if (cfgApply.m_srcFile.empty()) {
        if (!cfgData.fromJson(cfg["colorData"])) {
          printf("Unable to parse colorData configuration from '%s'\n", icSanitizeConsoleText(argv[2]).c_str());
          return EXIT_FAILURE;
        }
      }
      else {
        json data;
        if (!loadJsonFrom(data, cfgApply.m_srcFile.c_str()) || !cfgData.fromJson(data)) {
          printf("Unable to load color data from '%s'\n", icSanitizeConsoleText(cfgApply.m_srcFile.c_str()).c_str());
          return EXIT_FAILURE;
        }
      }
    }
    else if (cfgApply.m_srcType == icCfgIt8) {
      cfgData.m_srcSpace = cfgApply.m_srcSpace;
      if (cfgApply.m_srcFile.empty() || !cfgData.fromIt8(cfgApply.m_srcFile.c_str())) {
        printf("Unable to parse IT8 data file '%s'\n", icSanitizeConsoleText(cfgApply.m_srcFile.c_str()).c_str());
        return EXIT_FAILURE;
      }
    }
    else if (cfgApply.m_srcType == icCfgLegacy) {
      if (!cfgData.fromLegacy(cfgApply.m_srcFile.c_str())) {
        printf("Unable to parse legacy data file '%s'\n", icSanitizeConsoleText(cfgApply.m_srcFile.c_str()).c_str());
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
    // fromArgs() reports how many arguments it consumed and stops at the first it
    // does not recognise, so anything left over was silently discarded -- a
    // mistyped trailing profile path or intent ran the transform as though it had
    // not been given, and the tool exited 0. Refuse the remainder rather than
    // reporting success for a command line that was not fully honoured (#1674).
    // This is the last use of the argument vector, so compare the consumed count
    // against what was offered instead of advancing argv/argc once more: the
    // pointer bump that paired with the count above would be a store no later
    // read could observe (#1958).
    if (nArg != argc) {
      printf("Unexpected extra arguments\n");
      return EXIT_FAILURE;
    }

    if (cfgApply.m_srcType != icCfgLegacy || !cfgData.fromLegacy(cfgApply.m_srcFile.c_str())) {
      printf("Unable to parse legacy data file '%s'\n", icSanitizeConsoleText(cfgApply.m_srcFile.c_str()).c_str());
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
          printf("Error writing json config file '%s'\n", icSanitizeConsoleText(exportFile.c_str()).c_str());
          fclose(f);
          return EXIT_FAILURE;
        }
        if (!icFlushAndClose(f)) {
          printf("Error closing json config file '%s'\n", icSanitizeConsoleText(exportFile.c_str()).c_str());
          return EXIT_FAILURE;
        }
      }
      else {
        printf("Unable to export config file '%s'\n", icSanitizeConsoleText(exportFile.c_str()).c_str());
        return EXIT_FAILURE;
      }
    }
  }
  // Usage 2 has no destination file -- CIccCfgDataApply::fromArgs clears
  // m_dstFile unconditionally -- and icOpenRegularWriteFile() returns stdout
  // for an empty name.  The transformed dataset would therefore go to the same
  // stream as the evidence: --evidence-json there emitted colour data with a
  // JSON object stapled to the end, which no parser accepts, and an
  // outputDigest that was always null.  --evidence-json is a -cfg-with-dstFile
  // mode, the only form docs/tools-cli-reference.md shows.  Checked here, where
  // both config paths join, so the refusal costs no transform and writes
  // nothing.
  if (bEvidenceJson && cfgApply.m_dstFile.empty()) {
    fprintf(stderr, "--evidence-json requires a configuration with dstFile set:"
                    " the transform output would share stdout with the evidence\n");
    return EXIT_FAILURE;
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
      printf("Error - %s\n", icSanitizeConsoleText(sConnectError.c_str()).c_str());
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

    // #2150: the remap above has just rewritten a PCS signature to a device
    // one, so the value no longer meets ToInternalEncoding()'s 'XYZ '/'Lab '
    // arms -- it meets the shared device default:, whose float and percent
    // cases both clip to 0.0-1.0 whenever bClip. So this carve-out has to name
    // every encoding whose PCS range exceeds 0.0-1.0, which is three, not one:
    //
    //   icEncodeFloat      external PCS range ~0.0-2.0
    //   icEncodeUnitFloat  an exact synonym of it in both PCS arms since #2146
    //   icEncodePercent    the same range x100, i.e. ~0.0-200.0 ('XYZ ' only)
    //
    // icXyzFromPcs scales by 65535/32768, which is where the ~2.0 comes from --
    // so each of these clips discards legitimate values rather than hardening
    // anything. That is the reasoning that kept the library's own PCS arms
    // unclipped, and it applies unchanged once the signature has been remapped
    // for the transform's benefit: the 'XYZ ' arm's percent case does not clip
    // either.
    //
    // Naming only icEncodeFloat left the selector, and nothing else, deciding
    // whether the data survived -- and in both cases the tool refused to read
    // back what it had just written:
    //
    //   internal 0.9 -> unitFloat 1.79997 -> read back 1.0  (was 0.9)
    //   internal 0.9 -> percent 179.9879  -> read back 1.0  (was 0.9)
    if (srcEncoding == icEncodeFloat || srcEncoding == icEncodeUnitFloat ||
        srcEncoding == icEncodePercent)
      bClip = false;
  }

  //Get and validate the destination color space from the CMM.
  icColorSpaceSignature DestspaceSig = pNamedCmm->GetDestSpace();
  int nDestSamples = icGetSpaceSamples(DestspaceSig);
  
  //Allocate pixel buffers for performing encoding transformations
  char DestNameBuf[256];
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

      // For named-color input the tint lives in pData->m_values (the
      // JSON "v" field), not pData->m_srcValues; mirror the pixel-input
      // branch below so the dump can echo the tint the caller supplied.
      // When the caller omitted "v" entirely we leave m_srcValues empty
      // rather than synthesise a 1.0 the caller did not write.
      if (pData->m_values.size())
        out->m_srcValues = pData->m_values;

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
            // szName is the colour name read from the input row, and is what the
            // icApplyNamed2Pixel case above passes. This one passed SrcNameBuf, a
            // stack buffer that is never written to anywhere in this file, so every
            // named-to-named lookup was performed against 256 bytes of uninitialized
            // stack rather than the requested colour (CWE-457). The result was
            // whatever the named-colour search made of that garbage, reported as
            // success.
            if(pNamedCmm->Apply(DestNameBuf, szName, tint)) {
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

  if (bEvidenceJson) {
    const char* profilePath = cfgProfiles.m_profiles.empty() ? "" :
      cfgProfiles.m_profiles[0]->m_iccFile.c_str();
    EmitTransformEvidenceJson(cfgApply.m_srcFile.c_str(), profilePath,
                              cfgApply.m_dstFile.c_str());
  }


  delete pMruCmm;

  return 0;
}
