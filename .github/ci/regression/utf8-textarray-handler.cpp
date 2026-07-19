// Regression for #1726: multiplexTypeArrayTag ('mcta') is a tagArrayType whose
// array signature is icSigUtf8TextTypeArray ('utf8'); it carries the per-channel
// UTF-8 names of a Multiplex Color Space (MCS) profile.
//
// Before the fix, CIccBasicArrayFactory::CreateArray had no case for 'utf8', so
// every mcta fell to the CIccArrayUnknown fallback and validating it emitted a
// spurious "Unknown tag array type!" line even though the tag is conformant.
// Worse, the "UTF8 text arrays are known" branch in CIccTagArray::Validate (which
// holds the MCS channel-count check) had become unreachable once
// Read()/SetTagArrayType() started eagerly populating m_pArray.
//
// The fix (per maintainer approach on #1726) makes CreateArray return NULL for
// 'utf8', so m_pArray stays NULL and the dedicated utf8 branch in
// CIccTagArray::Validate handles the array directly - reviving the channel-count
// check. This test also covers the per-element utf8Type conformance
// check added in that branch.
//
// This test builds mcta-shaped tagArrays in memory and validates them against a
// profile whose header declares a 5-channel MCS. It is red-green: reverting the
// factory 'utf8' case makes GetArrayHandler() return a non-NULL CIccArrayUnknown
// and validation emit the "Unknown tag array type" line, failing cases 1-2;
// dropping the element-type check fails case 3.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagComposite.h"
#include "IccTagBasic.h"
#include "IccProfile.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cstdio>
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
    std::fprintf(stderr, "[utf8-textarray] FAIL: %s\n", what);
  }
}

// Build an mcta-shaped tagArray: array signature 'utf8', nText elements each a
// CIccTagUtf8Text channel name. When badIdx >= 0, index badIdx instead gets a
// CIccTagText (icSigTextType) -- a text tag of the wrong type -- to exercise the
// element-type conformance check. The array takes ownership of every attached
// element (freed by delete arr).
CIccTagArray *buildMcta(icUInt32Number nText, int badIdx = -1)
{
  CIccTagArray *arr = new CIccTagArray(icSigUtf8TextTypeArray);
  arr->SetSize(nText);
  for (icUInt32Number i = 0; i < nText; i++) {
    if ((int)i == badIdx) {
      CIccTagText *bad = new CIccTagText();
      bad->SetText("wrong-type");
      check(arr->AttachTag(i, bad), "AttachTag wrong-type element");
    }
    else {
      CIccTagUtf8Text *t = new CIccTagUtf8Text();
      t->SetText("channel");
      check(arr->AttachTag(i, t), "AttachTag utf8 element");
    }
  }
  return arr;
}

} // namespace

int main()
{
  // 'mc' namespace with 5 channels; icGetMultiplexColorSpaceSamples maps this to 5.
  // Assert the encoding up front so the test fails loudly if that mapping changes.
  const icMultiplexColorSignature mcs5 = (icMultiplexColorSignature)0x6d630005;
  check(icGetMultiplexColorSpaceSamples(mcs5) == 5,
        "0x6d630005 resolves to a 5-channel MCS");

  // sigPath whose first signature is the multiplexTypeArrayTag, so the utf8
  // branch's MCS channel-count check (gated on that path) is exercised.
  const std::string mctaPath = icGetSigPath(icSigMultiplexTypeArrayTag);

  CIccProfile profile;
  profile.m_Header.mcs = mcs5;

  // Case 1 (the #1726 defect): a conformant 5-channel mcta must route to a NULL
  // handler (the factory 'utf8' case) so the dedicated utf8 branch validates it
  // with no "Unknown tag array type" line and no channel-count complaint.
  {
    CIccTagArray *arr = buildMcta(5);

    check(arr->GetArrayHandler() == NULL,
          "utf8 array routes to a NULL handler, not the Unknown fallback (#1726)");

    std::string report;
    arr->Validate(mctaPath, report, &profile);
    check(report.find("Unknown tag array type") == std::string::npos,
          "conformant mcta produces no 'Unknown tag array type' line (#1726)");
    check(report.find("does not match MCS") == std::string::npos,
          "5 channel names match the 5-channel MCS header (no mismatch reported)");

    delete arr;
  }

  // Case 2: the revived channel-count check -- 3 names against a 5-channel MCS
  // header must be flagged critical. This path was dead before the fix.
  {
    CIccTagArray *arr = buildMcta(3);

    std::string report;
    arr->Validate(mctaPath, report, &profile);
    check(report.find("does not match MCS in header") != std::string::npos,
          "channel-count mismatch (3 names vs 5-channel MCS) is flagged (#1726)");

    delete arr;
  }

  // Case 3: element-type conformance -- a non-utf8Type element must be rejected.
  // The check is strict to utf8Type (not zut8Type) to match the MCS runtime, which
  // requires AreAllOfType(icSigUtf8TextType); see CIccPcsXform::Connect.
  {
    CIccTagArray *arr = buildMcta(5, /*badIdx*/ 2);

    std::string report;
    arr->Validate(mctaPath, report, &profile);
    check(report.find("not a utf8Type text tag") != std::string::npos,
          "a non-utf8Type element inside a utf8 text array is flagged (#1726)");

    delete arr;
  }

  if (g_fail) {
    std::fprintf(stderr, "[utf8-textarray] %d assertion(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[utf8-textarray] all assertions passed\n");
  return 0;
}
