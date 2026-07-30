// Regression for #1911: the JSON writers hand large payloads up to the document
// root by value, and the fix converts those hand-offs to std::move. This test pins
// that the moves land on the right side of each hand-off.
//
// The writers built a payload in a local, copied it into the parent, and then
// destroyed the local:
//
//     IccJson data = IccJson::array();
//     for (...) data.push_back(...);      // no reserve(): O(log n) reallocations
//     j["data"] = data;                   // full deep copy, then ~data
//
// For CLUT-bearing tags that repeated up the whole chain -- data -> clut -> tagData
// -> tagObj -> entry -> Tags -> IccProfile -- so one CLUT's samples were duplicated
// and freed several times over. Callgrind on APTEC_CMYKOGV_Coated_LinearCTV_2025.icc
// attributed 22.71% + 6.76% of instructions to nlohmann json_value::destroy and a
// further 4.43% to the copy constructor, with std::vector::_M_realloc_append at
// 2.55% for the missing reserve().
//
// The risk the fix introduces is a misplaced std::move: nlohmann leaves a moved-from
// basic_json as a null value, so moving a local one hand-off too early silently
// yields "data": null or an empty array rather than an error. That failure mode is
// invisible to a test that only checks the writer returned true, so this test walks
// the emitted document and asserts the payload actually arrived -- correct element
// counts, correct values, and no null where data belongs.
//
// Red-green: reverting any of the moves keeps this test green (a copy is still
// correct, only slower), but moving a source that is read again afterwards -- e.g.
// hoisting the std::move in icCLUTDataToJson above the gridPoints walk -- makes the
// corresponding count/value assertion below fail. The test therefore guards the
// correctness half of the change, not its performance.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfileJson.h"
#include "IccTagJson.h"
#include "IccTagJsonFactory.h"
#include "IccUtilJson.h"
#include "IccTagLut.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[json-writer-move] FAIL: %s\n", what);
  }
}

// Build a CLUT whose every sample is a distinct, exactly-representable value, so a
// dropped or duplicated element shows up as a value mismatch and not just a count
// mismatch. i/1024.0 is exact in binary floating point for the sizes used here.
CIccCLUT *makeCLUT(icUInt8Number nInput, icUInt16Number nOutput, icUInt8Number nGrid)
{
  CIccCLUT *pCLUT = new CIccCLUT(nInput, nOutput);
  if (!pCLUT->Init(nGrid)) {
    delete pCLUT;
    return NULL;
  }

  icUInt32Number nTotal = pCLUT->NumPoints() * (icUInt32Number)nOutput;
  icFloatNumber *pData = pCLUT->GetData(0);
  for (icUInt32Number i = 0; i < nTotal; i++)
    pData[i] = (icFloatNumber)(i / 1024.0);

  return pCLUT;
}

// --- the CLUT hand-off chain: icCLUTDataToJson + icCLUTToJson ------------------
// Four hand-offs live here (gridPoints, data, and the clut object itself), and the
// data array is the one that carries the bulk of a real profile.
void testCLUTChain()
{
  const icUInt8Number  nInput  = 3;
  const icUInt16Number nOutput = 4;
  const icUInt8Number  nGrid   = 5;

  CIccCLUT *pCLUT = makeCLUT(nInput, nOutput, nGrid);
  check(pCLUT != NULL, "test CLUT allocates");
  if (!pCLUT)
    return;

  const icUInt32Number nPoints = pCLUT->NumPoints();
  const icUInt32Number nTotal  = nPoints * (icUInt32Number)nOutput;

  // 5^3 grid points, 4 output channels. Pinned as a literal so the test does not
  // depend on library data (see the Windows linkage note in Testing/CMakeLists.txt).
  check(nPoints == 125, "CLUT has 5^3 = 125 grid points");
  check(nTotal  == 500, "CLUT has 125 * 4 = 500 samples");

  IccJson j;
  check(icCLUTToJson(j, pCLUT, icConvertFloat, true, "clut"),
        "icCLUTToJson reports success");

  // The outermost hand-off: j[szName] = std::move(clut).
  check(j.contains("clut"), "emitted document contains the clut object");
  check(j["clut"].is_object(), "clut survived the hand-off as an object, not null");

  const IccJson &clut = j["clut"];

  // gridPoints: written before the data walk, so moving it too late (or moving
  // the enclosing object too early) drops it.
  check(clut.contains("gridPoints"), "clut carries gridPoints");
  check(clut["gridPoints"].is_array(), "gridPoints survived as an array");
  check(clut["gridPoints"].size() == nInput, "gridPoints has one entry per input dim");
  if (clut["gridPoints"].is_array() && clut["gridPoints"].size() == nInput) {
    bool allGrid = true;
    for (icUInt8Number i = 0; i < nInput; i++) {
      if (!clut["gridPoints"][i].is_number() ||
          clut["gridPoints"][i].get<int>() != (int)nGrid)
        allGrid = false;
    }
    check(allGrid, "every gridPoints entry is the grid size, none null");
  }

  // data: the payload the whole change is about.
  check(clut.contains("data"), "clut carries data");
  check(clut["data"].is_array(), "data survived as an array, not null");
  check(clut["data"].size() == nTotal, "data has one entry per CLUT sample");

  if (clut["data"].is_array() && clut["data"].size() == nTotal) {
    bool allNumbers = true;
    bool allValues  = true;
    for (icUInt32Number i = 0; i < nTotal; i++) {
      const IccJson &v = clut["data"][i];
      if (!v.is_number()) {
        allNumbers = false;
        continue;
      }
      // Written as (double)pData[k]; the source was built from an exact binary
      // fraction, so this compares equal without a tolerance.
      if (v.get<double>() != (double)(icFloatNumber)(i / 1024.0))
        allValues = false;
    }
    check(allNumbers, "every data entry is a number, none null");
    check(allValues,  "every data entry matches its source sample, in order");
  }

  delete pCLUT;
}

// --- the profile assembly chain: CIccProfileJson::ToJson -----------------------
// tagData -> tagObj -> entry -> Tags -> root. Five hand-offs, all now moves.
void testProfileChain()
{
  CIccProfileJson profile;

  // A header alone is enough to exercise the Header hand-off; tags are added below
  // so the Tags array hand-off is exercised too.
  profile.m_Header.version     = 0x05000000;
  profile.m_Header.deviceClass = icSigInputClass;
  profile.m_Header.colorSpace  = icSigRgbData;
  profile.m_Header.pcs         = icSigXYZData;
  profile.m_Header.magic       = icMagicNumber;

  // Two tags of different shapes: one text, one numeric array. Both go through the
  // tagData -> tagObj -> entry -> Tags chain.
  CIccTag *pDesc = CIccTagCreator::CreateTag(icSigMultiLocalizedUnicodeType);
  if (pDesc)
    profile.AttachTag(icSigProfileDescriptionTag, pDesc);

  CIccTag *pWtpt = CIccTagCreator::CreateTag(icSigXYZType);
  if (pWtpt)
    profile.AttachTag(icSigMediaWhitePointTag, pWtpt);

  IccJson root;
  check(profile.ToJson(root), "CIccProfileJson::ToJson reports success");

  // root["Header"] = std::move(header)
  check(root.contains("Header"), "document carries Header");
  check(root["Header"].is_object(), "Header survived the hand-off as an object");
  check(root["Header"].contains("ProfileFileSignature"),
        "Header kept a field written before the hand-off");
  if (root["Header"].contains("DataColourSpace"))
    check(root["Header"]["DataColourSpace"].is_string(),
          "a Header field written mid-build is still a string, not null");

  // root["Tags"] = std::move(tags), with each entry moved in turn
  check(root.contains("Tags"), "document carries Tags");
  check(root["Tags"].is_array(), "Tags survived the hand-off as an array, not null");
  check(root["Tags"].size() >= 2, "both attached tags reached the Tags array");

  if (root["Tags"].is_array()) {
    bool allObjects = true;
    bool allHaveData = true;
    for (size_t i = 0; i < root["Tags"].size(); i++) {
      const IccJson &entry = root["Tags"][i];
      if (!entry.is_object() || entry.size() != 1) {
        allObjects = false;
        continue;
      }
      // Each entry is a one-member object { "<tagName>": { "data": {...} } }. The
      // inner object is what tagObj was moved into. The iterator is held in its own
      // named local: binding the reference straight to entry.begin().value() reads
      // through a temporary iterator, which gcc rejects under -Wdangling-reference.
      IccJson::const_iterator it = entry.begin();
      const IccJson &tagObj = it.value();
      if (!tagObj.is_object() || !(tagObj.contains("data") || tagObj.contains("sameAs")))
        allHaveData = false;
      else if (tagObj.contains("data") && !tagObj["data"].is_object())
        allHaveData = false;
    }
    check(allObjects, "every Tags entry is a one-member object, none null");
    check(allHaveData, "every Tags entry kept its data (or sameAs) member");
  }

  // The outermost hand-off in the string overload: root["IccProfile"] = move(profile).
  std::string jsonString;
  check(profile.ToJson(jsonString, 0), "ToJson(std::string&) reports success");
  check(!jsonString.empty(), "serialized string is non-empty");
  check(jsonString.find("\"IccProfile\"") != std::string::npos,
        "serialized string carries the IccProfile wrapper");
  check(jsonString.find("\"IccProfile\":null") == std::string::npos,
        "the IccProfile wrapper is not null (document was not moved away early)");
  check(jsonString.find("\"Header\"") != std::string::npos,
        "serialized string still carries the Header after the wrapper move");
}

// --- a tag-level array hand-off ------------------------------------------------
// CIccTagJsonNum<>::ToJson and friends build a "values" array the same way.
void testTagValueArray()
{
  CIccTag *pTag = CIccTagCreator::CreateTag(icSigS15Fixed16ArrayType);
  check(pTag != NULL, "s15Fixed16 tag allocates");
  if (!pTag)
    return;

  IIccExtensionTag *pExt = pTag->GetExtension();
  if (!pExt || strcmp(pExt->GetExtClassName(), "CIccTagJson") != 0) {
    // The JSON factory is not registered in this build; nothing to assert.
    delete pTag;
    return;
  }

  IccJson j;
  check(static_cast<CIccTagJson *>(pExt)->ToJson(j), "tag ToJson reports success");
  check(j.contains("values"), "tag carries a values array");
  check(j["values"].is_array(), "values survived the hand-off as an array, not null");

  delete pTag;
}

} // namespace

int main()
{
  // The JSON tag/element factories must be registered before any ToJson call, or
  // GetExtension() returns nothing and the walks above have nothing to inspect.
  CIccTagCreator::PushFactory(new CIccTagJsonFactory);

  testCLUTChain();
  testProfileChain();
  testTagValueArray();

  if (g_fail)
    std::fprintf(stderr, "[json-writer-move] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[json-writer-move] all assertions passed\n");

  return g_fail;
}
