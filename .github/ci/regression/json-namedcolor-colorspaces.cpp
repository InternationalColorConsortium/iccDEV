// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// #2583.  A lut or named colour tag carries no colour spaces of its own: it takes
// them from the header of the profile it is attached to.  The binary reader does
// that in CIccProfile::LoadTag() and the XML reader at the end of
// CIccProfileXml::ParseTag(); CIccProfileJson::ParseTag() did not, so a tag loaded
// from JSON kept the zeroes its constructor left.
//
// The visible cost is a FALSE invalid verdict.  CIccArrayNamedColor::Validate()
// compares each colour's stored channel count against m_nSpectralSamples, which
// SetColorSpaces() derives from the header's spectralPCS.  Left at zero, the
// matrix branch
//
//     if (pArrayTag->GetChannelsPerMatrix() != m_nSpectralSamples)
//
// fires for every colour in the tag, and iccFromJson exits 1 reporting
// "Incompatible SpectralPcs samples in NamedColor[n]" on a profile it had just
// reproduced byte for byte.
//
// Note that branch is unguarded, unlike its device and PCS siblings a few lines
// up, which are written "else if (m_nPcsSamples)" and so stay silent at zero.
// That asymmetry is why the defect surfaced only on the sparse-matrix named
// colour profiles and not on every JSON document with a namedColor2Tag -- and it
// is why case 2 asserts on the sparse fixtures specifically.
//
// What is asserted, on the two tracked sparse-matrix fixtures:
//
//   1. The chain is real: each XML document loads, saves as a binary profile,
//      reads back into a CIccProfileJson, and serialises.  A fixture that stopped
//      declaring a sparse-matrix spectral PCS would fail here rather than quietly
//      removing the coverage, so the later cases cannot go vacuous.
//   2. The JSON-loaded profile Validates no worse than the SAME profile loaded
//      from binary, and its report carries no "Incompatible SpectralPcs" line.
//      Comparing against the binary reader rather than against a literal is what
//      makes this a parity assertion: if a future change makes both readers
//      report something new, the test follows it instead of pinning a stale
//      string.
//   3. The named colour array really did receive the header's spectral PCS --
//      GetChannelsPerMatrix() equals the sample count the header implies, and
//      that count is non-zero.  Case 2 alone would pass against a "fix" that
//      silenced the diagnostic (say, by guarding the branch the way its siblings
//      are guarded) while still leaving the tag unconfigured; this case asserts
//      the FIXED state is present, not merely that the complaint is absent.
//   4. The AToB/BToA/HToS/gamut half of the dispatch, which no corpus document
//      can speak for.  m_csInput/m_csOutput on a CIccMBB feed Describe() only --
//      not Validate(), not Write() -- so an A/B of the whole tracked corpus
//      through the tools shows no difference at all for those tags.  The gap is
//      real for an embedder that calls LoadJson() then Describe(), and unless it
//      is asserted here it is untested code.  The check is a JSON->Describe()
//      against an XML->Describe() of the same profile.
//
// Takes the fixture paths as argv[1..2] and a scratch directory as argv[3]; the
// CMake registration names them.

#include "IccProfile.h"
#include "IccProfileXml.h"
#include "IccProfileJson.h"
#include "IccTagXml.h"
#include "IccTagJson.h"
#include "IccTagXmlFactory.h"
#include "IccTagJsonFactory.h"
#include "IccTagFactory.h"
#include "IccArrayBasic.h"
#include "IccStructBasic.h"
#include "IccTagComposite.h"
#include "IccIO.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[json-namedcolor-colorspaces] FAIL: %s\n", what);
  }
}

bool contains(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

// The tag factories are a STACK and the top one wins: a tag created while the JSON
// factory is on top carries a CIccTagJson extension, and CIccProfileXml::ParseTag
// refuses it ("Invalid tag extension"), and the reverse for ToJson.  The two CLIs
// each push exactly one, so a test that drives both legs has to do the same thing
// one at a time rather than push both up front.
class ScopedTagFactory {
public:
  explicit ScopedTagFactory(IIccTagFactory *pFactory)
  {
    CIccTagCreator::PushFactory(pFactory);
  }
  ~ScopedTagFactory()
  {
    delete CIccTagCreator::PopFactory();
  }
  ScopedTagFactory(const ScopedTagFactory &) = delete;
  ScopedTagFactory &operator=(const ScopedTagFactory &) = delete;
};

// Load a tracked XML document the way iccFromXml does.
CIccProfile *loadXml(const char *szXml)
{
  ScopedTagFactory xmlFactory(new CIccTagXmlFactory());

  CIccProfileXml *pProfile = new CIccProfileXml;
  std::string parseStr;

  if (!pProfile->LoadXml(szXml, NULL, &parseStr)) {
    std::fprintf(stderr, "[json-namedcolor-colorspaces] cannot parse %s: %s\n",
                 szXml, parseStr.c_str());
    delete pProfile;
    return NULL;
  }
  return pProfile;
}

// The sample count the header's spectralPCS implies -- the value SetColorSpaces()
// hands the named colour array, computed here independently of it.
icUInt32Number headerSpectralSamples(const CIccProfile *pProfile)
{
  return icGetSpaceSamples((icColorSpaceSignature)pProfile->m_Header.spectralPCS);
}

// main() pushes no MPE factory (the two libraries' MPE headers cannot share a
// translation unit), so a fixture that grew a multiProcessElement tag would lose it
// on the JSON leg instead of failing.  Assert the fixtures stay free of them.
bool hasMpeTag(CIccProfile *pProfile)
{
  for (TagEntryList::iterator i = pProfile->m_Tags.begin();
       i != pProfile->m_Tags.end(); i++) {
    if (i->pTag && i->pTag->GetType() == icSigMultiProcessElementType)
      return true;
  }
  return false;
}

CIccArrayNamedColor *namedColorArray(CIccProfile *pProfile)
{
  CIccTag *pTag = pProfile->FindTag(icSigNamedColor2Tag);

  if (!pTag || pTag->GetTagArrayType() != icSigNamedColorArray)
    return NULL;

  return (CIccArrayNamedColor*)icGetTagArrayHandler(pTag);
}

// One fixture, through the whole iccFromXml | iccToJson | iccFromJson chain.
void runFixture(const char *szXml, const std::string &scratch)
{
  const std::string name = std::filesystem::path(szXml).stem().string();
  const std::string iccPath = scratch + "/" + name + ".icc";

  // --- case 1: the chain, and the fixture's shape ------------------------------
  CIccProfile *pXml = loadXml(szXml);
  if (!pXml) {
    check(false, (name + ": XML document loads").c_str());
    return;
  }

  const icUInt32Number nSpectralSamples = headerSpectralSamples(pXml);
  check(nSpectralSamples != 0,
        (name + ": fixture still declares a spectral PCS with a sample count").c_str());
  check(!hasMpeTag(pXml),
        (name + ": fixture carries no multiProcessElement tag").c_str());

  if (!SaveIccProfile(iccPath.c_str(), pXml)) {
    check(false, (name + ": saves as a binary profile").c_str());
    delete pXml;
    return;
  }
  delete pXml;

  // The binary reader is the reference for both case 2 and case 3: it carries the
  // dispatch the JSON reader was missing, so the same profile read this way is what
  // the JSON-loaded one has to agree with.  It stays alive to the end of the fixture.
  std::string binReport;
  icValidateStatus binStatus = icValidateOK;
  CIccProfile *pBin = ReadIccProfile(iccPath.c_str());
  if (!pBin) {
    check(false, (name + ": binary profile reads back").c_str());
    return;
  }
  binStatus = pBin->Validate(binReport);

  struct BinGuard {
    CIccProfile *p;
    ~BinGuard() { delete p; }
  } binGuard{pBin};

  // ICC -> JSON, as iccToJson does.
  std::string jsonStr;
  {
    ScopedTagFactory jsonFactory(new CIccTagJsonFactory());
    CIccProfileJson toJson;
    CIccFileIO srcIO;

    if (!srcIO.Open(iccPath.c_str(), "r") || !toJson.Read(&srcIO)) {
      check(false, (name + ": binary profile reads into CIccProfileJson").c_str());
      return;
    }
    if (!toJson.ToJson(jsonStr, 2)) {
      check(false, (name + ": profile serialises to JSON").c_str());
      return;
    }
  }
  check(contains(jsonStr, "sparseMatrixArrayType"),
        (name + ": emitted JSON still carries a sparseMatrixArrayType").c_str());

  // JSON -> profile, as iccFromJson does.
  const std::string jsonPath = scratch + "/" + name + ".json";
  {
    std::FILE *f = std::fopen(jsonPath.c_str(), "wb");
    if (!f) {
      check(false, (name + ": scratch JSON is writable").c_str());
      return;
    }
    std::fwrite(jsonStr.data(), 1, jsonStr.size(), f);
    std::fclose(f);
  }

  CIccProfileJson fromJson;
  {
    ScopedTagFactory jsonFactory(new CIccTagJsonFactory());
    std::string parseStr;
    if (!fromJson.LoadJson(jsonPath.c_str(), &parseStr)) {
      std::fprintf(stderr, "[json-namedcolor-colorspaces] %s: LoadJson: %s\n",
                   name.c_str(), parseStr.c_str());
      check(false, (name + ": JSON document loads").c_str());
      return;
    }
  }

  // --- case 2: the JSON reader's verdict matches the binary reader's -----------
  std::string jsonReport;
  const icValidateStatus jsonStatus = fromJson.Validate(jsonReport);

  check(!contains(jsonReport, "Incompatible SpectralPcs samples"),
        (name + ": JSON-loaded profile reports no incompatible spectral samples").c_str());
  check(jsonStatus == binStatus,
        (name + ": JSON and binary readers agree on the validation status").c_str());

  if (jsonStatus != binStatus) {
    std::fprintf(stderr,
                 "[json-namedcolor-colorspaces] %s: binary status %d, JSON status %d\n"
                 "  binary report: %s\n  JSON report:   %s\n",
                 name.c_str(), (int)binStatus, (int)jsonStatus,
                 binReport.c_str(), jsonReport.c_str());
  }

  // --- case 3: the fixed state is PRESENT, not merely the complaint absent -----
  // Case 2 alone is satisfiable without fixing anything: guard the Validate() branch
  // the way its device and PCS siblings are guarded and the complaint disappears while
  // the tag stays exactly as unconfigured.  That mutant was run, and an earlier version
  // of this case passed it -- GetChannelsPerMatrix() is a property of the TAG and is
  // the same either way, so comparing it against the header asserted nothing about
  // whether the array ever received the header.
  //
  // What only the real fix can produce is the array BEHAVING like the binary reader's.
  // m_nSpectralSamples is what SetColorSpaces() computes and the only thing
  // GetSpectralTint() sizes its output by, so tinting the same colour out of both
  // readers and requiring identical floats cannot be satisfied by silencing a
  // diagnostic.  This is also the path that matters: it is what the CMM runs.
  CIccArrayNamedColor *pJsonAry = namedColorArray(&fromJson);
  CIccArrayNamedColor *pBinAry  = namedColorArray(pBin);

  if (!pJsonAry || !pBinAry) {
    check(false, (name + ": both readers produce a named colour array").c_str());
    return;
  }

  check(pJsonAry->Begin(), (name + ": JSON-loaded named colour array Begin()").c_str());
  check(pBinAry->Begin(),  (name + ": binary named colour array Begin()").c_str());

  CIccTag *pJsonTag = fromJson.FindTag(icSigNamedColor2Tag);
  CIccTag *pBinTag  = pBin->FindTag(icSigNamedColor2Tag);
  CIccTagArray *pJsonArr = (pJsonTag && pJsonTag->GetType() == icSigTagArrayType)
                         ? (CIccTagArray*)pJsonTag : NULL;
  CIccTagArray *pBinArr  = (pBinTag  && pBinTag->GetType()  == icSigTagArrayType)
                         ? (CIccTagArray*)pBinTag : NULL;

  if (!pJsonArr || !pBinArr || pJsonArr->GetSize() != pBinArr->GetSize()) {
    check(false, (name + ": both readers produce the same sized colour array").c_str());
    return;
  }

  // Tint 1.0 selects each colour's own matrix; tint 0.5 merges it against the shared
  // tint-zero matrix, so both the stored data and the interpolation are covered.
  static const icFloatNumber kTints[] = { 1.0f, 0.5f };
  const icFloatNumber kPoison = (icFloatNumber)-99.0f;
  std::vector<icFloatNumber> jsonOut(nSpectralSamples), binOut(nSpectralSamples);
  int nCompared = 0;

  for (icUInt32Number i = 0; i < pJsonArr->GetSize(); i++) {
    CIccStructNamedColor *pJsonColor =
      (CIccStructNamedColor*)icGetTagStructHandler(pJsonArr->GetIndex(i));
    CIccStructNamedColor *pBinColor =
      (CIccStructNamedColor*)icGetTagStructHandler(pBinArr->GetIndex(i));

    if (!pJsonColor || !pBinColor)
      continue;

    for (size_t t = 0; t < sizeof(kTints)/sizeof(kTints[0]); t++) {
      // Poison both buffers with the SAME value.  A matrix-valued spectral member
      // does not fill nSpectralSamples floats -- CIccStructNamedColor::GetTint()
      // merges the sparse matrices and writes the merged matrix, which is shorter --
      // so the tail is legitimately untouched and must not count as a difference.
      // The shared poison is what makes an unwritten tail compare equal; bWritten
      // below is what stops "both wrote nothing" from passing.
      std::fill(jsonOut.begin(), jsonOut.end(), kPoison);
      std::fill(binOut.begin(),  binOut.end(),  kPoison);

      const bool bJson = pJsonAry->GetSpectralTint(jsonOut.data(), pJsonColor, kTints[t]);
      const bool bBin  = pBinAry->GetSpectralTint(binOut.data(),  pBinColor,  kTints[t]);

      check(bBin, (name + ": binary reader tints colour " + std::to_string(i)).c_str());
      check(bJson == bBin,
            (name + ": both readers agree that colour " + std::to_string(i) +
             " tints").c_str());

      if (!bJson || !bBin)
        continue;

      bool bSame = true, bWritten = false;
      for (icUInt32Number c = 0; c < nSpectralSamples; c++) {
        if (binOut[c] != kPoison)
          bWritten = true;
        if (jsonOut[c] != binOut[c])
          bSame = false;
      }

      check(bWritten,
            (name + ": the tint call actually writes the spectral vector").c_str());
      check(bSame,
            (name + ": JSON and binary readers tint colour " + std::to_string(i) +
             " identically").c_str());
      if (!bSame) {
        int shown = 0;
        for (icUInt32Number c = 0; c < nSpectralSamples && shown < 6; c++) {
          if (jsonOut[c] != binOut[c]) {
            std::fprintf(stderr,
                         "[json-namedcolor-colorspaces] %s colour %u tint %.2f:"
                         " [%u] json=%.9g bin=%.9g\n",
                         name.c_str(), (unsigned)i, (double)kTints[t], (unsigned)c,
                         (double)jsonOut[c], (double)binOut[c]);
            shown++;
          }
        }
      }
      nCompared++;
    }
  }

  check(nCompared > 0,
        (name + ": at least one colour was tinted through both readers").c_str());

}

// --- case 4: the MBB half of the dispatch ------------------------------------
// No tracked document can show this through the tools: CIccMBB::m_csInput and
// m_csOutput reach Describe() and nothing else, so the corpus round-trips are
// byte-identical either way.  Asserting it here is what keeps those switch arms
// from being untested code.  The reference is the XML reader, which has carried
// the same dispatch all along.
void runMbbParity(const char *szXml, const std::string &scratch)
{
  const std::string name = std::filesystem::path(szXml).stem().string();
  const std::string iccPath = scratch + "/" + name + "-mbb.icc";

  CIccProfile *pXml = loadXml(szXml);
  if (!pXml) {
    check(false, (name + " (mbb): XML document loads").c_str());
    return;
  }

  // Only meaningful where the profile actually holds an MBB-typed lut tag.
  static const icTagSignature kLutTags[] = {
    icSigAToB0Tag, icSigAToB1Tag, icSigAToB2Tag,
    icSigBToA0Tag, icSigBToA1Tag, icSigBToA2Tag,
  };
  icTagSignature sigLut = (icTagSignature)0;
  for (size_t i = 0; i < sizeof(kLutTags)/sizeof(kLutTags[0]); i++) {
    CIccTag *pTag = pXml->FindTag(kLutTags[i]);
    if (pTag && pTag->IsMBBType()) {
      sigLut = kLutTags[i];
      break;
    }
  }

  if (!sigLut) {
    delete pXml;
    check(false, (name + " (mbb): fixture still carries an MBB lut tag").c_str());
    return;
  }

  check(!hasMpeTag(pXml),
        (name + " (mbb): fixture carries no multiProcessElement tag").c_str());

  std::string xmlDescribe;
  pXml->FindTag(sigLut)->Describe(xmlDescribe, 100);

  const bool bSaved = SaveIccProfile(iccPath.c_str(), pXml);
  delete pXml;
  if (!bSaved) {
    check(false, (name + " (mbb): saves as a binary profile").c_str());
    return;
  }

  std::string jsonStr;
  {
    ScopedTagFactory jsonFactory(new CIccTagJsonFactory());
    CIccProfileJson toJson;
    CIccFileIO srcIO;
    if (!srcIO.Open(iccPath.c_str(), "r") || !toJson.Read(&srcIO) ||
        !toJson.ToJson(jsonStr, 2)) {
      check(false, (name + " (mbb): profile serialises to JSON").c_str());
      return;
    }
  }

  const std::string jsonPath = scratch + "/" + name + "-mbb.json";
  {
    std::FILE *f = std::fopen(jsonPath.c_str(), "wb");
    if (!f) {
      check(false, (name + " (mbb): scratch JSON is writable").c_str());
      return;
    }
    std::fwrite(jsonStr.data(), 1, jsonStr.size(), f);
    std::fclose(f);
  }

  CIccProfileJson fromJson;
  {
    ScopedTagFactory jsonFactory(new CIccTagJsonFactory());
    std::string parseStr;
    if (!fromJson.LoadJson(jsonPath.c_str(), &parseStr)) {
      check(false, (name + " (mbb): JSON document loads").c_str());
      return;
    }
  }

  CIccTag *pJsonLut = fromJson.FindTag(sigLut);
  if (!pJsonLut || !pJsonLut->IsMBBType()) {
    check(false, (name + " (mbb): JSON-loaded profile still has the lut tag").c_str());
    return;
  }

  std::string jsonDescribe;
  pJsonLut->Describe(jsonDescribe, 100);

  // Describe() names each channel from the tag's colour spaces.  Unconfigured,
  // icColorIndexName() falls back to a bare index for an unknown space, so the
  // two descriptions diverge exactly where the spaces are missing.
  check(jsonDescribe == xmlDescribe,
        (name + " (mbb): JSON and XML readers describe the lut tag identically").c_str());

  if (jsonDescribe != xmlDescribe) {
    std::fprintf(stderr,
                 "[json-namedcolor-colorspaces] %s (mbb): descriptions differ\n"
                 "  XML  length %zu\n  JSON length %zu\n",
                 name.c_str(), xmlDescribe.size(), jsonDescribe.size());
  }
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: %s <SparseMatrixNamedColor.xml> <SparseMatrixNamedColorTint.xml>"
                 " <scratch-dir>\n",
                 argv[0] ? argv[0] : "json-namedcolor-colorspaces");
    return 2;
  }

  // No factory is pushed here: each leg pushes the one it needs and pops it again
  // (see ScopedTagFactory), because the stack's top factory decides which extension
  // a created tag gets and the XML and JSON readers each refuse the other's.
  //
  // The matching MPE factories are deliberately absent.  IccMpeXml.h and IccMpeJson.h
  // each define CIccMpePtr, CIccTempVar and CIccTempDeclVar, with different comparators
  // on the maps keyed by them, so a translation unit cannot include both.  None of the
  // fixtures holds a multiProcessElement tag -- runFixture() and runMbbParity() assert
  // that, so a fixture that gained one fails here instead of silently losing a tag on
  // the JSON leg.

  std::error_code ec;
  std::filesystem::create_directories(argv[3], ec);
  const std::string scratch = argv[3];

  runFixture(argv[1], scratch);
  runFixture(argv[2], scratch);

  // The MBB parity check wants a profile with a lut tag; the sparse named colour
  // fixtures have none, so it runs on the PCC profile the CMake registration adds.
  if (argc > 4)
    runMbbParity(argv[4], scratch);

  if (g_fail) {
    std::fprintf(stderr, "[json-namedcolor-colorspaces] %d check(s) failed\n", g_fail);
    return 1;
  }

  std::printf("[json-namedcolor-colorspaces] all checks passed\n");
  return 0;
}
