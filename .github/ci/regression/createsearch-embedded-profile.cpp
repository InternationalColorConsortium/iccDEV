/*
    File:       createsearch-embedded-profile.cpp

    Contains:   CTest helper for CIccConnectCmm::CreateSearch built from an
                in-memory (embedded) source profile, and for the optional
                "initial" block in CIccCfgSearchApply::fromJson.

    iccApplyProfiles lets an empty first "iccFile" mean "use the ICC profile
    embedded in the source TIFF".  CreateStandard has always honoured that by
    calling the memory overload of CIccCmm::AddXform; CreateSearch had no
    embedded parameter at all, so the same config under connect.useSearch would
    have tried to open a profile named "".

    The memory path is the one that reasoning cannot settle.  CIccCmmSearch is
    the only CMM whose Begin() re-adds the forward profiles into its sub-chains
    through the *reference* AddXform overload, which copy-constructs the profile
    and detaches the copy from its IO.  That had never been exercised against a
    profile attached from a CIccMemIO rather than a file, so this helper drives
    it end to end and requires the embedded chain to match the equivalent
    file-path chain sample for sample.

    Also covered: a search config with a "profileSequence" but no "initial" key
    must parse.  fromJsonInit used to reject the resulting null value, even
    though CreateSearch guards every use of the initial-destination settings
    behind isInitialized().

    Usage:
      createsearch-embedded-profile <profile.icc>

    Exit codes:
      0 - embedded and file-path search chains agree, and parsing behaves
      1 - unexpected result
      2 - usage error
*/

#include "IccConnect.h"
#include "IccCmmConfig.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// The search minimises a numerical cost, so two chains that differ only in how
// the source profile was attached still have to agree bit-for-bit only up to
// the search's own reproducibility.  Both runs start from the same seed and
// take the same steps, so anything above float noise means the memory-attached
// profile built a different chain.
static const icFloatNumber kTolerance = 1.0e-4f;

static int check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "createsearch-embedded-profile: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "createsearch-embedded-profile: FAIL  %s\n", label);
  return 1;
}

static bool ReadFileBytes(const char* path, std::vector<unsigned char>& bytes)
{
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open())
    return false;

  f.seekg(0, std::ios::end);
  const std::streamoff len = f.tellg();
  if (len <= 0)
    return false;
  f.seekg(0, std::ios::beg);

  bytes.resize((size_t)len);
  f.read((char*)&bytes[0], len);
  return f.good() || f.eof();
}

static CIccCfgProfilePtr MakeStage(const char* iccFile)
{
  CIccCfgProfilePtr stage(new CIccCfgProfile());
  stage->m_iccFile = iccFile ? iccFile : "";
  stage->m_intent = icRelativeColorimetric;
  stage->m_transform = icXformLutColor;
  stage->m_interpolation = icInterpTetrahedral;
  return stage;
}

// Gamut corners plus one interior sample: the corners are where a chain built
// from a differently attached profile diverges first.
static const icFloatNumber kSamples[][3] = {
  { 0.0f, 0.0f, 0.0f },
  { 1.0f, 0.0f, 0.0f },
  { 0.0f, 1.0f, 0.0f },
  { 0.0f, 0.0f, 1.0f },
  { 1.0f, 1.0f, 1.0f },
  { 0.5f, 0.25f, 0.125f },
};
static const int kNumSamples = (int)(sizeof(kSamples) / sizeof(kSamples[0]));

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <profile.icc>\n",
                 argv[0] ? argv[0] : "createsearch-embedded-profile");
    return 2;
  }

  const char* profilePath = argv[1];
  int failures = 0;

  // ------------------------------------------------------------------
  // "initial" is optional (step 2 of the plan).
  // ------------------------------------------------------------------
  {
    json cfg;
    json stage;
    stage["iccFile"] = profilePath;
    stage["intent"] = "relative";
    cfg["profileSequence"].push_back(stage);
    cfg["profileSequence"].push_back(stage);

    CIccCfgSearchApply search;
    const bool bParsed = search.fromJson(cfg, true);
    failures += check(bParsed && search.m_profiles.size() == 2,
                      "search apply parses a config with no initial block");
    failures += check(bParsed && !search.isInitialized(),
                      "absent initial block leaves isInitialized() false");

    json cfgInit = cfg;
    cfgInit["initial"]["intent"] = "relative";
    CIccCfgSearchApply searchInit;
    failures += check(searchInit.fromJson(cfgInit, true) && searchInit.isInitialized(),
                      "present initial block still sets isInitialized()");

    json cfgBadInit = cfg;
    cfgBadInit["initial"] = 7;
    CIccCfgSearchApply searchBadInit;
    failures += check(!searchBadInit.fromJson(cfgBadInit, true),
                      "non-object initial block is still rejected");
  }

  // ------------------------------------------------------------------
  // Embedded source profile through CreateSearch (step 1 of the plan).
  // ------------------------------------------------------------------
  std::vector<unsigned char> profileBytes;
  if (!ReadFileBytes(profilePath, profileBytes)) {
    std::fprintf(stderr, "createsearch-embedded-profile: unable to read '%s'\n",
                 profilePath);
    return 1;
  }

  CIccCfgSearchApply embeddedCfg;
  embeddedCfg.m_profiles.push_back(MakeStage(""));
  embeddedCfg.m_profiles.push_back(MakeStage(profilePath));

  CIccCfgSearchApply fileCfg;
  fileCfg.m_profiles.push_back(MakeStage(profilePath));
  fileCfg.m_profiles.push_back(MakeStage(profilePath));

  std::string sEmbeddedErr;
  std::unique_ptr<CIccConnectCmm> embedded(
    CIccConnectCmm::CreateSearch(embeddedCfg, &profileBytes[0],
                                 (unsigned int)profileBytes.size(), 1,
                                 &sEmbeddedErr));
  if (!embedded || !embedded->GetCmm()) {
    std::fprintf(stderr,
                 "createsearch-embedded-profile: FAIL  CreateSearch with embedded "
                 "source profile: %s\n",
                 sEmbeddedErr.empty() ? "no error reported" : sEmbeddedErr.c_str());
    return 1;
  }
  failures += check(embedded->GetSearchCmm() != nullptr,
                    "embedded chain produced a CIccCmmSearch");

  std::string sFileErr;
  std::unique_ptr<CIccConnectCmm> fileBased(
    CIccConnectCmm::CreateSearch(fileCfg, nullptr, 0, 1, &sFileErr));
  if (!fileBased || !fileBased->GetCmm()) {
    std::fprintf(stderr,
                 "createsearch-embedded-profile: FAIL  CreateSearch from file paths: %s\n",
                 sFileErr.empty() ? "no error reported" : sFileErr.c_str());
    return 1;
  }

  CIccCmm* pEmbeddedCmm = embedded->GetCmm();
  CIccCmm* pFileCmm = fileBased->GetCmm();

  if (pEmbeddedCmm->GetSourceSamples() != 3 || pEmbeddedCmm->GetDestSamples() != 3) {
    std::fprintf(stderr,
                 "createsearch-embedded-profile: expected a 3-channel profile, got "
                 "src=%d dst=%d\n",
                 pEmbeddedCmm->GetSourceSamples(), pEmbeddedCmm->GetDestSamples());
    return 1;
  }

  for (int i = 0; i < kNumSamples; i++) {
    icFloatNumber embeddedDst[3] = { 0.0f, 0.0f, 0.0f };
    icFloatNumber fileDst[3] = { 0.0f, 0.0f, 0.0f };

    if (pEmbeddedCmm->Apply(embeddedDst, kSamples[i]) != icCmmStatOk ||
        pFileCmm->Apply(fileDst, kSamples[i]) != icCmmStatOk) {
      std::fprintf(stderr,
                   "createsearch-embedded-profile: FAIL  Apply failed for sample %d\n", i);
      failures++;
      continue;
    }

    icFloatNumber worst = 0.0f;
    for (int c = 0; c < 3; c++) {
      const icFloatNumber diff =
        (icFloatNumber)std::fabs(embeddedDst[c] - fileDst[c]);
      if (diff > worst)
        worst = diff;
    }

    if (worst > kTolerance) {
      std::fprintf(stderr,
        "createsearch-embedded-profile: FAIL  sample %d embedded [%.6f %.6f %.6f] != "
        "file [%.6f %.6f %.6f] (max diff %.6f)\n",
        i, (double)embeddedDst[0], (double)embeddedDst[1], (double)embeddedDst[2],
        (double)fileDst[0], (double)fileDst[1], (double)fileDst[2], (double)worst);
      failures++;
    }
    else {
      std::fprintf(stdout,
        "createsearch-embedded-profile: PASS  sample %d matches file-path chain "
        "(max diff %.6f)\n", i, (double)worst);
    }
  }

  // A 4th profile is rejected by CIccCmmSearch::AddXform, so CreateSearch must
  // report a failure rather than returning a CMM the tool would then apply.
  {
    CIccCfgSearchApply tooMany;
    for (int i = 0; i < 4; i++)
      tooMany.m_profiles.push_back(MakeStage(profilePath));

    std::string sErr;
    std::unique_ptr<CIccConnectCmm> rejected(
      CIccConnectCmm::CreateSearch(tooMany, nullptr, 0, 1, &sErr));
    failures += check(!rejected && !sErr.empty(),
                      "CreateSearch rejects a 4-profile chain with an error message");
  }

  // Threading: each CIccThreadedCmm worker gets its own CIccApplyCmmSearch,
  // which owns a private CIccApplyCmm per sub-chain, so a threaded search must
  // agree with the scalar one.  Pinned here because the embedded overload is
  // the one that forwards nThreads.
  {
    std::string sErr;
    std::unique_ptr<CIccConnectCmm> threaded(
      CIccConnectCmm::CreateSearch(fileCfg, nullptr, 0, 4, &sErr));
    if (!threaded || !threaded->GetCmm()) {
      std::fprintf(stderr,
                   "createsearch-embedded-profile: FAIL  threaded CreateSearch: %s\n",
                   sErr.empty() ? "no error reported" : sErr.c_str());
      return 1;
    }
    failures += check(threaded->IsThreaded(),
                      "threaded CreateSearch wraps the search CMM");

    int mismatches = 0;
    for (int i = 0; i < kNumSamples; i++) {
      icFloatNumber threadedDst[3] = { 0.0f, 0.0f, 0.0f };
      icFloatNumber scalarDst[3] = { 0.0f, 0.0f, 0.0f };
      if (threaded->GetCmm()->Apply(threadedDst, kSamples[i]) != icCmmStatOk ||
          pFileCmm->Apply(scalarDst, kSamples[i]) != icCmmStatOk) {
        mismatches++;
        continue;
      }
      for (int c = 0; c < 3; c++) {
        if (std::fabs(threadedDst[c] - scalarDst[c]) > kTolerance)
          mismatches++;
      }
    }
    failures += check(mismatches == 0,
                      "threaded search matches the scalar search");

    // The embedded source profile has to survive the threaded path too: every
    // worker's sub-chains are built from the same memory-attached profile.
    std::string sEmbThreadErr;
    std::unique_ptr<CIccConnectCmm> embeddedThreaded(
      CIccConnectCmm::CreateSearch(embeddedCfg, &profileBytes[0],
                                   (unsigned int)profileBytes.size(), 4,
                                   &sEmbThreadErr));
    failures += check(embeddedThreaded && embeddedThreaded->GetCmm() &&
                        embeddedThreaded->IsThreaded(),
                      "threaded CreateSearch accepts an embedded source profile");
  }

  return failures ? 1 : 0;
}
