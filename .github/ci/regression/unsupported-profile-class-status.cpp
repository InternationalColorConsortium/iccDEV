// Regression for #1843 (claim 1): a profile whose class carries no device<->PCS
// transform pair must be refused with icCmmStatUnsupportedProfileClass, not with
// icCmmStatInvalidProfile.
//
// CIccEvalCompare::EvaluateProfile builds a device->PCS transform and its inverse and
// compares the result; CIccPRMG::EvaluateProfile drives a Lab->device->Lab chain over
// the PRMG boundary. Both are meaningful only for the four classes that carry that
// transform pair -- input, display, output and colorSpace -- and both open with the
// same four-way class test. That test used to return icCmmStatInvalidProfile, a status
// otherwise reserved for a profile that is actually broken, which CIccCmm::GetStatusText
// renders as "Invalid profile". iccRoundTrip therefore reported
//
//   Unable to perform round trip on 'RefDecC.icc': Invalid profile
//
// for an abstract profile that validates cleanly -- and, because the two conditions
// shared one status code, a caller had no way to tell "this profile is damaged" from
// "this operation does not apply to this kind of profile". Both evaluators now return
// the distinct icCmmStatUnsupportedProfileClass.
//
// The test drives the CIccProfile* overloads directly rather than the file-path ones,
// so it needs no fixture on disk: the class test runs before anything reads tag data,
// so a default-constructed profile with only m_Header.deviceClass set reaches exactly
// the branch under test. That also makes CIccPRMG's copy of the branch reachable at
// all -- every in-tree caller (iccRoundTrip, wxProfileDump) runs the PRMG evaluation
// only after the round-trip evaluation has already succeeded, so a class-refusal never
// reaches CIccPRMG through a tool.
//
// The unsupported cases assert both the positive (is the new status) and the negative
// (is no longer the old one), because the point of the change is the distinction
// between the two, not the value of either. The supported classes are asserted only
// NOT to be refused for their class: an empty profile has no transform to build, so
// they legitimately fail further along with some other status, and pinning which one
// would tie this test to unrelated CMM internals.
//
// Red-green: restoring either return to icCmmStatInvalidProfile fails that evaluator's
// assertions; widening the class test to reject a device class fails the guards below.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccEval.h"
#include "IccPrmg.h"
#include "IccProfile.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[unsupported-profile-class] FAIL: %s\n", what);
  }
}

// CIccEvalCompare::Compare is pure virtual -- the caller supplies the comparison it
// wants over each sampled colour (iccRoundTrip accumulates min/mean/max dE). Nothing
// here samples anything: the class test returns before the sampling loop, and the
// supported-class guards fail while building the transform, so the body is never
// reached and can stay empty.
class CIccNullEval : public CIccEvalCompare
{
public:
  virtual void Compare(icFloatNumber * /*pPixel*/, icFloatNumber * /*deviceLab*/,
                       icFloatNumber * /*destLab1*/, icFloatNumber * /*destLab2*/) {}
};

// Only the header class matters to the branch under test, so the profile is left
// otherwise empty. Each case gets a fresh profile so no evaluator can carry state
// from a previous one.
void evaluateClass(icProfileClassSignature nClass, icStatusCMM &evalStat, icStatusCMM &prmgStat)
{
  CIccProfile profile;
  profile.m_Header.deviceClass = nClass;

  CIccNullEval eval;
  evalStat = eval.EvaluateProfile(&profile);

  CIccPRMG prmg;
  prmgStat = prmg.EvaluateProfile(&profile);
}

// Both the wire signature and the human name, so a failure report says which class
// tripped rather than only a hex value.
struct ClassCase {
  icProfileClassSignature nClass;
  const char *szName;
};

// Every class the specification defines that is not one of the four the evaluators
// accept. Six of the seven have fixtures in the Testing corpus ('abst', 'cenc', 'link',
// 'mid ', 'mvis', 'nmcl'); only 'mlnk' has none, so it is covered here on the strength
// of the specification alone. The class test treats all seven identically, so a gap in
// the corpus is no reason to leave one of them untested.
const ClassCase kUnsupported[] = {
  { icSigLinkClass,                    "deviceLink" },
  { icSigAbstractClass,                "abstract" },
  { icSigNamedColorClass,              "namedColor" },
  { icSigColorEncodingClass,           "colorEncoding" },
  { icSigMultiplexIdentificationClass, "multiplexIdentification" },
  { icSigMultiplexLinkClass,           "multiplexLink" },
  { icSigMultiplexVisualizationClass,  "multiplexVisualization" },
};

const ClassCase kSupported[] = {
  { icSigInputClass,      "input" },
  { icSigDisplayClass,    "display" },
  { icSigOutputClass,     "output" },
  { icSigColorSpaceClass, "colorSpace" },
};

} // namespace

int main()
{
  char szWhat[160];

  // --- classes outside the evaluators' domain ---------------------------------

  for (size_t i = 0; i < sizeof(kUnsupported) / sizeof(kUnsupported[0]); i++) {
    icStatusCMM evalStat, prmgStat;
    evaluateClass(kUnsupported[i].nClass, evalStat, prmgStat);

    std::snprintf(szWhat, sizeof(szWhat), "%s class should be refused by the round-trip evaluator as an unsupported class", kUnsupported[i].szName);
    check(evalStat == icCmmStatUnsupportedProfileClass, szWhat);

    std::snprintf(szWhat, sizeof(szWhat), "%s class must no longer be reported as an invalid profile by the round-trip evaluator (#1843)", kUnsupported[i].szName);
    check(evalStat != icCmmStatInvalidProfile, szWhat);

    std::snprintf(szWhat, sizeof(szWhat), "%s class should be refused by the PRMG evaluator as an unsupported class", kUnsupported[i].szName);
    check(prmgStat == icCmmStatUnsupportedProfileClass, szWhat);

    std::snprintf(szWhat, sizeof(szWhat), "%s class must no longer be reported as an invalid profile by the PRMG evaluator (#1843)", kUnsupported[i].szName);
    check(prmgStat != icCmmStatInvalidProfile, szWhat);
  }

  // --- the four classes the evaluators do accept ------------------------------
  // Guards: the refusal must be about the class only. These profiles are empty, so
  // both evaluators fail somewhere later -- just not here.

  for (size_t i = 0; i < sizeof(kSupported) / sizeof(kSupported[0]); i++) {
    icStatusCMM evalStat, prmgStat;
    evaluateClass(kSupported[i].nClass, evalStat, prmgStat);

    std::snprintf(szWhat, sizeof(szWhat), "%s class must not be refused by the round-trip evaluator as an unsupported class", kSupported[i].szName);
    check(evalStat != icCmmStatUnsupportedProfileClass, szWhat);

    std::snprintf(szWhat, sizeof(szWhat), "%s class must not be refused by the PRMG evaluator as an unsupported class", kSupported[i].szName);
    check(prmgStat != icCmmStatUnsupportedProfileClass, szWhat);
  }

  // --- a null profile keeps its own distinct status ----------------------------
  // The class test sits directly after the null check, so this pins that the new
  // return did not swallow the "cannot open" case.

  {
    CIccNullEval eval;
    check(eval.EvaluateProfile((CIccProfile *)NULL) == icCmmStatCantOpenProfile,
          "a null profile should still report that it cannot be opened");

    CIccPRMG prmg;
    check(prmg.EvaluateProfile((CIccProfile *)NULL) == icCmmStatCantOpenProfile,
          "a null profile should still report that it cannot be opened (PRMG)");
  }

  // --- the status decodes to text ---------------------------------------------
  // icStatusCMM's declaration in IccCmm.h asks for GetStatusText to be kept in sync,
  // and the note is there because it has been missed before -- icCmmStatUnsupported
  // was added without a case and fell through to the default. That text is what
  // iccRoundTrip prints, so an undecoded status would leave the user-visible message
  // no better than the one this change set out to fix.

  {
    const char *szNew = CIccCmm::GetStatusText(icCmmStatUnsupportedProfileClass);
    const char *szOld = CIccCmm::GetStatusText(icCmmStatInvalidProfile);

    // The reference for "fell through to the default" is a status one past the last
    // enumerator. It must stay inside the enum's valid value range: icStatusCMM is an
    // unscoped enum with no fixed underlying type, so its values span only the smallest
    // bit-field holding -1..19 (a signed 6-bit field, i.e. -32..31). Loading anything
    // outside that is undefined behaviour, which UBSan's enum check flags at the switch
    // in GetStatusText -- a far-out sentinel such as 0x7FFFFFFF fails the sanitizer CI
    // jobs. If a future status does take 20 it will have its own text, so the assertion
    // below stays meaningful either way.
    const char *szUnknown = CIccCmm::GetStatusText((icStatusCMM)20);

    check(szNew != NULL && strcmp(szNew, szUnknown) != 0,
          "icCmmStatUnsupportedProfileClass should decode to text rather than fall through to the default");
    check(szNew != NULL && szOld != NULL && strcmp(szNew, szOld) != 0,
          "the unsupported-class text must differ from the invalid-profile text (#1843)");
  }

  // --- the enum stays append-only ----------------------------------------------
  // icStatusCMM values cross the IccProfLib ABI and appear in saved QA output, so a
  // new status has to be appended rather than inserted. Pinning the last previously
  // defined value alongside the new one catches a renumbering that would silently
  // change what every existing status means.

  check((int)icCmmStatUnsupported == 18,
        "icCmmStatUnsupported must keep its value -- icStatusCMM is append-only");
  check((int)icCmmStatUnsupportedProfileClass == 19,
        "icCmmStatUnsupportedProfileClass must be appended after icCmmStatUnsupported");

  if (g_fail)
    std::fprintf(stderr, "[unsupported-profile-class] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[unsupported-profile-class] all assertions passed\n");

  return g_fail;
}
