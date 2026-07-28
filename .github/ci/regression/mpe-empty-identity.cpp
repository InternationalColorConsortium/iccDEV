// Regression for #1809: a multiProcessElementType tag carrying zero processing
// elements with equal input and output channel counts is the identity transform,
// and CIccTagMultiProcessElement::Validate must report it as information rather
// than as a concern.
//
// Every other path in the class already treats the empty equal-channel list as a
// defined state: Read() accepts a zero element count, Write() emits one, Begin()
// returns true without building an apply list, and Apply() memcpy()s the source
// pixel to the destination. Validate() alone raised icValidateWarning, which put
// 79 of the reference profiles under Testing/ (148 messages across the PCC, ICS
// and mcs/Flexo-CMYKOGP fixtures, whose DToB3/BToD3 and AToB/BToA/AToM0 tags are
// authored as identity on purpose) into the warning cohort and buried findings
// that do need attention. Emitting the diagnostic at information level leaves the
// status at icValidateOK while keeping the tag visible in the report.
//
// The unequal-channel case is unchanged and must stay a critical error: nothing
// bridges the channel-count change, so Begin() refuses the shape outright and the
// tag is unusable.
//
// The test drives Validate() through a profile whose header makes the AToB1
// channel counts agree, so the returned status reflects the empty-list branch and
// nothing else. It then asserts the runtime behaviour the level change is
// justified by -- Begin() true / Apply() identity for equal channels, Begin()
// false for unequal -- so the two can no longer drift apart silently.
//
// Red-green: restoring the icMsgValidateWarning / icMaxStatus(rv, icValidateWarning)
// pair in the empty-list else branch fails assertions 1 and 2.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagMPE.h"
#include "IccProfile.h"
#include "IccUtil.h"
#include "IccDefs.h"

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
    std::fprintf(stderr, "[mpe-empty-identity] FAIL: %s\n", what);
  }
}

bool contains(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

// The report-level prefixes are spelled out rather than taken from the exported
// icMsgValidate* globals in IccUtil.h. Those globals do not resolve when a test
// binary links IccProfLib under MSVC (LNK2019), and matching the literal text is
// the stronger assertion anyway: it pins what a log scan actually greps for, so
// renaming a global cannot quietly change the observable output.
const char *const kInformation = "Information - ";
const char *const kWarning     = "Warning! - ";
const char *const kError       = "Error! - ";

// A Lab/Lab header gives icGetSpaceSamples() 3 for both colorSpace and pcs, so the
// icSigAToB1Tag arm of Validate()'s channel switch is satisfied by a 3-channel tag
// and contributes no messages of its own. Anything left in the report therefore
// came from the empty-element-list branch under test.
void initLabProfile(CIccProfile &profile)
{
  std::memset(&profile.m_Header, 0, sizeof(profile.m_Header));
  profile.m_Header.colorSpace = icSigLabData;
  profile.m_Header.pcs = icSigLabData;
  profile.m_Header.deviceClass = icSigColorSpaceClass;
  profile.m_Header.version = icVersionNumberV4_3;
}

} // namespace

int main(void)
{
  CIccProfile profile;
  initLabProfile(profile);

  const std::string sigPath = icGetSigPath(icSigAToB1Tag);

  // 1. Equal channel counts, no elements: identity, reported as information, and
  //    the tag as a whole validates clean.
  {
    CIccTagMultiProcessElement tag;
    tag.SetChannels(3, 3);

    std::string report;
    icValidateStatus rv = tag.Validate(sigPath, report, &profile);

    check(rv == icValidateOK,
          "empty equal-channel MPE must validate as icValidateOK");
    check(contains(report, kInformation),
          "empty equal-channel MPE must be reported at information level");
    check(contains(report, "No processing elements"),
          "the empty-element diagnostic text must be retained for log scans");
    check(!contains(report, kWarning),
          "empty equal-channel MPE must not raise a warning");
    check(!contains(report, kError),
          "empty equal-channel MPE must not raise a critical error");
  }

  // 2. Unequal channel counts, no elements: unusable, still a critical error.
  {
    CIccTagMultiProcessElement tag;
    tag.SetChannels(3, 4);

    std::string report;
    icValidateStatus rv = tag.Validate(sigPath, report, &profile);

    check(rv == icValidateCriticalError,
          "empty unequal-channel MPE must remain a critical error");
    check(contains(report, "No processing elements and input and output channels do not match!"),
          "the unequal-channel diagnostic must be preserved verbatim");
  }

  // 3. The runtime behaviour the information level is justified by. If any of
  //    these flip, the reporting level above is no longer defensible.
  {
    CIccTagMultiProcessElement tag;
    tag.SetChannels(3, 3);

    check(tag.NumElements() == 0, "constructed tag must carry no elements");
    check(tag.Begin(), "Begin() must accept an empty equal-channel element list");

    CIccApplyTagMpe *pApply = tag.GetNewApply();
    check(pApply != NULL, "GetNewApply() must succeed for an empty element list");

    if (pApply) {
      const icFloatNumber src[3] = { 0.25f, 0.5f, 0.75f };
      icFloatNumber dst[3] = { -1.0f, -1.0f, -1.0f };

      tag.Apply(pApply, dst, src);

      check(dst[0] == src[0] && dst[1] == src[1] && dst[2] == src[2],
            "Apply() on an empty equal-channel element list must be the identity");
      delete pApply;
    }
  }

  {
    CIccTagMultiProcessElement tag;
    tag.SetChannels(3, 4);
    check(!tag.Begin(),
          "Begin() must reject an empty element list with unequal channel counts");
  }

  if (!g_fail)
    std::printf("[mpe-empty-identity] PASS: empty equal-channel MPE is identity and reports as information\n");

  return g_fail;
}
