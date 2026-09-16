/*
    File:       json-colorant-parent-profile.cpp

    Contains:   CTest helper for IIccObject::GetParentProfile() and the
                colorantTableType / chromaticityType JSON readers (#2550, #2541).

    #2550.  GetParentProfile() walked the parent chain comparing GetObjectType()
    against the literal "CIccProfile", then C-cast the result.  Two independent
    defects met there:

      - GetObjectType() forwards to the virtual GetClassName(), which every
        subclass overrides.  CIccProfileJson and CIccProfileXml -- the classes
        iccToJson and iccToXml actually instantiate -- report their own names, so
        the walk never matched and ran off the top of the tree returning NULL.

      - IccObject.h only forward-declares CIccProfile, so in that translation unit
        the type was incomplete and the C-style cast could not be a base-to-derived
        static_cast; it degraded to a reinterpret_cast.  CIccProfile derives from
        IIccProfileConnectionConditions before IIccObject, so the IIccObject
        subobject is not at offset zero and the returned pointer was wrong by that
        offset even for a plain CIccProfile, where the name DID match.

    The second defect is why case 1 below compares addresses rather than merely
    checking non-NULL: a test that only asserted "not NULL" passed against the
    shifted pointer, which is the more dangerous of the two failures -- every
    m_Header read through it came from the wrong bytes.

    The visible consequence is case 2.  CIccTagJsonColorantTable::ToJson already
    selected "XYZ"/"Lab"/"16bit" from the owning profile's PCS; it always wrote
    "16bit" only because the lookup above handed it NULL or a shifted header, so
    its other two branches were unreachable from the CLI.  No library change was
    needed there, which is exactly why this helper asserts the emitted encoding
    rather than the branch.

    #2541.  Both JSON readers fail-open where the XML readers refuse: a missing or
    non-array container, a malformed channel pair, a non-numeric coordinate, a
    missing colorant name, and an unrecognised pcsEncoding each used to return
    true after silently substituting a zero, a default, or an empty tag.  Case 3
    pins one negative per condition.

    Case 3 keeps positive controls beside the refusals.  A refusal-only fixture
    passes against a reader that rejects everything, including the documents the
    writer emits, so each accepted spelling here must still load.

    Case 4 is the cost of the fix.  Before it, iccToJson could only ever emit the
    raw "16bit" form, which is lossless by construction.  After it, every profile
    with a Lab or XYZ PCS emits floats, so the ICC -> JSON -> ICC trip now runs
    icU16toF, icLabFromPcs/icXyzFromPcs, a text float, and back through
    icLabToPcs/icXyzToPcs and icFtoU16.  That is only acceptable if it returns
    every stored byte, so case 4 drives all 65536 values through each of the three
    coordinates, for both PCSs, through serialised JSON TEXT -- not the in-memory
    IccJson value, which would skip the float printing and parsing that the CLI
    actually performs.  Three coordinates matter separately because L* and the a* and b*
    use different scales.
*/

#include "IccTagJson.h"
#include "IccProfileJson.h"
#include "IccProfile.h"

#include <cstdio>
#include <cstring>
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
    std::fprintf(stderr, "[json-colorant-parent-profile] FAIL: %s\n", what);
  }
}

const char *kOneColorant =
  "{\"pcsEncoding\":\"16bit\",\"colorantTable\":"
  "[{\"name\":\"ink\",\"pcs\":[100,200,300]}]}";

// Load a colorant table from a document, attach it to a profile whose PCS the
// caller chooses, and hand back the encoding ToJson emits for it.
std::string encodingUnder(icColorSpaceSignature pcs, bool bAttach, CIccProfile &profile)
{
  CIccTagJsonColorantTable *pTag = new CIccTagJsonColorantTable;
  std::string parseStr;
  if (!pTag->ParseJson(IccJson::parse(kOneColorant), parseStr)) {
    delete pTag;
    return "<parse failed>";
  }

  profile.m_Header.pcs = pcs;
  if (bAttach) {
    // AttachTag sets the parent link and takes ownership.
    if (!profile.AttachTag(icSigAToB0Tag, pTag)) {
      delete pTag;
      return "<attach failed>";
    }
  }

  IccJson j;
  const bool bOk = pTag->ToJson(j);
  std::string rv = bOk && j.contains("pcsEncoding")
                     ? j["pcsEncoding"].get<std::string>()
                     : std::string("<no encoding>");
  if (!bAttach)
    delete pTag;
  return rv;
}

// Parse one colorantTable document and report only whether it was accepted.
bool colorantAccepts(const char *doc)
{
  CIccTagJsonColorantTable tag;
  std::string parseStr;
  return tag.ParseJson(IccJson::parse(doc), parseStr);
}

bool chromaticityAccepts(const char *doc)
{
  CIccTagJsonChromaticity tag;
  std::string parseStr;
  return tag.ParseJson(IccJson::parse(doc), parseStr);
}

// Round-trip one table through JSON text under the given PCS and count the
// coordinates that came back different; -1 if the trip itself failed.  Row r of
// the table holds, in coordinate k, (r * step[k]) mod 65536 for r = firstRow ..
// firstRow + nRows - 1.  Each step is odd, hence coprime to 65536, so rows
// 0..65535 put every 16-bit value in every coordinate exactly once.  A table
// holds at most 65535 entries, so that set is split into one full table and a
// one-row table for row 65535 -- two trips per PCS, where four full tables would
// double the cost for no extra coverage (the ASAN+UBSAN CI leg runs this in a
// Debug build).
long roundTripMismatches(icColorSpaceSignature pcs, icUInt32Number firstRow,
                         icUInt16Number nRows, const char **pEncoding)
{
  static const icUInt32Number step[3] = { 1, 7919, 40503 };

  CIccProfile profile;
  profile.m_Header.pcs = pcs;
  CIccTagJsonColorantTable *pSrc = new CIccTagJsonColorantTable;
  if (!pSrc->SetSize(nRows) || !profile.AttachTag(icSigAToB0Tag, pSrc)) {
    return -1;
  }
  for (icUInt32Number i = 0; i < nRows; i++) {
    icColorantTableEntry &e = (*pSrc)[i];
    std::strcpy(e.name, "c");
    for (int k = 0; k < 3; k++)
      e.data[k] = (icUInt16Number)(((firstRow + i) * step[k]) & 0xFFFF);
  }

  IccJson j;
  if (!pSrc->ToJson(j))
    return -1;
  static std::string enc;
  enc = j.value("pcsEncoding", std::string("<none>"));
  *pEncoding = enc.c_str();

  // Serialise and re-parse, exactly as iccToJson | iccFromJson would.
  const std::string text = j.dump();
  CIccTagJsonColorantTable dst;
  std::string parseStr;
  if (!dst.ParseJson(IccJson::parse(text), parseStr) || dst.GetSize() != nRows)
    return -1;

  long nBad = 0;
  for (icUInt32Number i = 0; i < nRows; i++) {
    for (int k = 0; k < 3; k++) {
      if (dst[i].data[k] != (*pSrc)[i].data[k])
        nBad++;
    }
  }
  return nBad;
}

} // namespace

int main()
{
  // ---------------------------------------------------------------------
  // 1. The parent lookup finds the profile, at the right address, for a
  //    plain CIccProfile and for the JSON subclass the tools instantiate.
  // ---------------------------------------------------------------------
  {
    CIccProfile profile;
    CIccTagJsonColorantTable *pTag = new CIccTagJsonColorantTable;
    check(profile.AttachTag(icSigAToB0Tag, pTag), "1: AttachTag failed");
    // Address equality, not non-NULL: the unadjusted C-cast returned a pointer
    // that was non-NULL and wrong.
    check(pTag->GetParentProfile() == &profile,
          "1: GetParentProfile did not return the owning CIccProfile");
  }

  {
    CIccProfileJson profile;
    CIccTagJsonColorantTable *pTag = new CIccTagJsonColorantTable;
    check(profile.AttachTag(icSigAToB0Tag, pTag), "1: AttachTag failed (json)");
    check(pTag->GetParentProfile() == static_cast<const CIccProfile *>(&profile),
          "1: GetParentProfile did not recognise CIccProfileJson");
  }

  // ---------------------------------------------------------------------
  // 2. The emitted encoding follows the owning profile's PCS, and falls
  //    back to 16bit only when the tag has no owner.
  // ---------------------------------------------------------------------
  {
    CIccProfile profile;
    const std::string enc = encodingUnder(icSigLabData, true, profile);
    check(enc == "Lab", ("2: Lab-PCS profile emitted pcsEncoding " + enc).c_str());
  }
  {
    CIccProfile profile;
    const std::string enc = encodingUnder(icSigXYZData, true, profile);
    check(enc == "XYZ", ("2: XYZ-PCS profile emitted pcsEncoding " + enc).c_str());
  }
  {
    CIccProfile profile;
    const std::string enc = encodingUnder(icSigCmykData, true, profile);
    check(enc == "16bit",
          ("2: non-XYZ/Lab PCS emitted pcsEncoding " + enc).c_str());
  }
  {
    // Detached: no owner, so 16bit is correct rather than a fallback from a
    // failed lookup. This is the control that keeps case 2 from passing merely
    // because everything still says 16bit.
    CIccProfile unused;
    const std::string enc = encodingUnder(icSigLabData, false, unused);
    check(enc == "16bit",
          ("2: detached table emitted pcsEncoding " + enc).c_str());
  }

  // ---------------------------------------------------------------------
  // 3. Fail-closed negatives, each beside a control that must still load.
  // ---------------------------------------------------------------------
  check(colorantAccepts(kOneColorant),
        "3: control colorantTable document was refused");
  // An empty array is refused, and that predates this change: SetSize(0) reaches
  // icRealloc(p, 0), which returns NULL, and the reader treats that as failure.
  // It is left exactly as it was found -- ToJson does emit "colorantTable":[] for
  // a tag with no entries, so that one document does not round-trip, but whether
  // an empty colorantTable is legal at all is an ICC.1 question and not this
  // change's to settle. Pinned here so the asymmetry is visible rather than
  // discovered again.
  check(!colorantAccepts("{\"pcsEncoding\":\"Lab\",\"colorantTable\":[]}"),
        "3: an empty colorantTable array changed behaviour");
  check(colorantAccepts(
          "{\"colorantTable\":[{\"name\":\"ink\",\"pcs\":[0.5,0.5,0.5]}]}"),
        "3: a document with no pcsEncoding was refused");

  check(!colorantAccepts("{\"pcsEncoding\":\"Lab\"}"),
        "3: a missing colorantTable was accepted");
  check(!colorantAccepts("{\"pcsEncoding\":\"Lab\",\"colorantTable\":{}}"),
        "3: a non-array colorantTable was accepted");
  check(!colorantAccepts(
          "{\"pcsEncoding\":\"sRGB\",\"colorantTable\":"
          "[{\"name\":\"ink\",\"pcs\":[0.5,0.5,0.5]}]}"),
        "3: an unknown pcsEncoding was accepted");
  // jGetString() fails for a non-string, so a check on its result alone would
  // leave this one on the Lab default.
  check(!colorantAccepts(
          "{\"pcsEncoding\":7,\"colorantTable\":"
          "[{\"name\":\"ink\",\"pcs\":[0.5,0.5,0.5]}]}"),
        "3: a non-string pcsEncoding was accepted");
  check(!colorantAccepts(
          "{\"pcsEncoding\":\"16bit\",\"colorantTable\":[\"ink\"]}"),
        "3: a non-object colorantTable entry was accepted");
  check(!colorantAccepts(
          "{\"pcsEncoding\":\"16bit\",\"colorantTable\":[{\"pcs\":[1,2,3]}]}"),
        "3: a colorant with no name was accepted");
  check(!colorantAccepts(
          "{\"pcsEncoding\":\"16bit\",\"colorantTable\":"
          "[{\"name\":7,\"pcs\":[1,2,3]}]}"),
        "3: a non-string colorant name was accepted");

  check(chromaticityAccepts(
          "{\"colorantType\":0,\"channels\":[[0.64,0.33],[0.3,0.6]]}"),
        "3: control chromaticity document was refused");

  check(!chromaticityAccepts("{\"colorantType\":0}"),
        "3: a missing channels array was accepted");
  check(!chromaticityAccepts("{\"colorantType\":0,\"channels\":{}}"),
        "3: a non-array channels field was accepted");
  check(!chromaticityAccepts("{\"colorantType\":0,\"channels\":[[0.64]]}"),
        "3: a one-element channel pair was accepted");
  check(!chromaticityAccepts(
          "{\"colorantType\":0,\"channels\":[[0.64,0.33,0.1]]}"),
        "3: a three-element channel pair was accepted");
  check(!chromaticityAccepts(
          "{\"colorantType\":0,\"channels\":[[\"x\",\"y\"]]}"),
        "3: a non-numeric channel coordinate was accepted");

  // ---------------------------------------------------------------------
  // 4. ICC -> JSON text -> ICC returns every stored coordinate byte, for
  //    every 16-bit value, in every position, under both float encodings.
  // ---------------------------------------------------------------------
  {
    static const icColorSpaceSignature kPcs[2] = { icSigLabData, icSigXYZData };
    static const char *kWant[2] = { "Lab", "XYZ" };
    static const icUInt32Number kFirst[2] = { 0, 0xFFFF };
    static const icUInt16Number kRows[2] = { 0xFFFF, 1 };
    for (int p = 0; p < 2; p++) {
      for (int o = 0; o < 2; o++) {
        const char *enc = "";
        const long nBad = roundTripMismatches(kPcs[p], kFirst[o], kRows[o], &enc);
        char label[160];
        std::snprintf(label, sizeof(label),
                      "4: %s round trip (rows from %u, emitted %s): %ld coordinate(s) changed",
                      kWant[p], (unsigned)kFirst[o], enc, nBad);
        // The encoding assertion keeps this case from passing on the old
        // "16bit"-only writer, which is lossless for the wrong reason.
        check(nBad == 0 && std::strcmp(enc, kWant[p]) == 0, label);
      }
    }
  }

  if (g_fail) {
    std::fprintf(stderr, "[json-colorant-parent-profile] %d check(s) failed\n",
                 g_fail);
    return 1;
  }
  std::fprintf(stdout, "[json-colorant-parent-profile] all checks passed\n");
  return 0;
}
