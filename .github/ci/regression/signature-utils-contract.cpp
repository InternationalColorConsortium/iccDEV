// First coverage for IccProfLib/IccSignatureUtils.h, harvested from ci-qa-flags (#1853).
//
// Adapted from @xsscx's .github/ci/regression/signature-utils.cpp on that branch, and
// widened. His version is what first showed the MCS naming gap from outside my own
// reasoning: built against master at 378fe807 it exited 1 with "CIccInfo lost MCS
// signature context", and passed once the naming fix in #2072 landed. The MCS
// assertions at the bottom keep that pinned.
//
// The reason this file is worth its place, though, is that IccSignatureUtils.h had no
// test at all and no consumer -- nothing in the tree included it. That is how it came to
// sit with an unguarded `#include <execinfo.h>` behind a bare `__linux__`, fixed in #2072
// with a __has_include probe. This test is deliberately that header's first consumer, so
// the header is actually compiled by CI from now on rather than only read.
//
// Three functions duplicate each other's knowledge and can therefore drift apart:
//
//   ColorSpaceSignatureToStr()      names a signature
//   IsValidColorSpaceSignature()    accepts a signature
//   DescribeColorSpaceSignature()   does both -- and carries its OWN inline copy of the
//                                   validity switch, commented "Validate without
//                                   triggering logs -- inline the check directly"
//
// A signature added to one and missed by the others is silent, so the agreement checks
// below matter more than any single expected string.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccSignatureUtils.h"
#include "IccTagBasic.h"
#include "IccUtil.h"
#include "icProfileHeader.h"

#include <string>

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[signature-utils-contract] FAIL: %s\n", what);
  }
}

void checkName(icUInt32Number sig, const char *want, const char *what)
{
  const char *got = ColorSpaceSignatureToStr(sig);
  if (!got || std::strcmp(got, want) != 0) {
    ++g_fail;
    std::fprintf(stderr, "[signature-utils-contract] FAIL: %s (got \"%s\", want \"%s\")\n",
                 what, got ? got : "(null)", want);
  }
}

// The fixed signatures all three functions are supposed to agree on.
const icUInt32Number kFixedValid[] = {
  (icUInt32Number)icSigXYZData,  (icUInt32Number)icSigLabData,
  (icUInt32Number)icSigRgbData,  (icUInt32Number)icSigCmykData,
  (icUInt32Number)icSigGrayData, (icUInt32Number)icSigNamedData,
  (icUInt32Number)icSigMCH1Data, (icUInt32Number)icSigMCH4Data,
  (icUInt32Number)icSigMCHFData,
};

// --- 1. Named signatures. ---
void testNames()
{
  checkName((icUInt32Number)icSigXYZData,  "XYZ",   "XYZ");
  checkName((icUInt32Number)icSigLabData,  "Lab",   "Lab");
  checkName((icUInt32Number)icSigRgbData,  "RGB",   "RGB");
  checkName((icUInt32Number)icSigCmykData, "CMYK",  "CMYK");
  checkName((icUInt32Number)icSigGrayData, "Gray",  "Gray");
  checkName((icUInt32Number)icSigNamedData,"Named", "Named");
  checkName((icUInt32Number)icSigMCH1Data, "MCH1",  "MCH1");
  checkName((icUInt32Number)icSigMCHFData, "MCHF",  "MCHF");

  // The two dynamic families are recognised by their high half, so any channel count
  // answers the same name.
  checkName(0x6e630007, "NChannel", "nc0007 is NChannel");
  checkName(0x6e630001, "NChannel", "nc0001 is NChannel");
  checkName(0x6d630003, "MCS",      "mc0003 is MCS");
  checkName(0x6d63000F, "MCS",      "mc000F is MCS");

  checkName(0x00000000, "Unknown", "zero is Unknown");
  checkName(0x4A554E4B, "Unknown", "'JUNK' is Unknown");
}

// --- 2. The zero-channel boundary, which is the one place the families are rejected. ---
// Both dynamic branches require nChan > 0, so the bare family signature with no channel
// count is neither named nor valid. Pinned because it is the only input where the high
// half matches and the answer is still no.
void testZeroChannelBoundary()
{
  checkName((icUInt32Number)icSigNChannelData,    "Unknown", "nc0000 is not NChannel");
  checkName((icUInt32Number)icSigSrcMCSChannelData,"Unknown", "mc0000 is not MCS");

  check(!IsValidColorSpaceSignature((icUInt32Number)icSigNChannelData),
        "nc0000 is rejected");
  check(!IsValidColorSpaceSignature((icUInt32Number)icSigSrcMCSChannelData),
        "mc0000 is rejected");
}

// --- 3. Validity, including the dynamic families. ---
void testValidity()
{
  for (size_t i = 0; i < sizeof(kFixedValid) / sizeof(kFixedValid[0]); i++)
    check(IsValidColorSpaceSignature(kFixedValid[i]), "fixed signature accepted");

  check(IsValidColorSpaceSignature(0x6e630007), "nc0007 accepted");
  check(IsValidColorSpaceSignature(0x6d630003), "mc0003 accepted");

  check(!IsValidColorSpaceSignature(0x00000000), "zero rejected");
  check(!IsValidColorSpaceSignature(0x4A554E4B), "'JUNK' rejected");
}

// --- 4. The three functions must agree. ---
// DescribeColorSpaceSignature re-implements the validity switch rather than calling
// IsValidColorSpaceSignature, so this is the check that catches a signature added to one
// and forgotten in the other.
void testAgreement()
{
  icUInt32Number sigs[] = {
    (icUInt32Number)icSigXYZData, (icUInt32Number)icSigRgbData,
    (icUInt32Number)icSigNamedData, (icUInt32Number)icSigMCHFData,
    0x6e630007, 0x6d630003,
    (icUInt32Number)icSigNChannelData, (icUInt32Number)icSigSrcMCSChannelData,
    0x00000000, 0x4A554E4B,
  };

  for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
    IccColorSpaceDescription desc = DescribeColorSpaceSignature(sigs[i]);

    check(desc.isKnown == IsValidColorSpaceSignature(sigs[i]),
          "Describe().isKnown agrees with IsValidColorSpaceSignature()");
    check(desc.name != NULL &&
            std::strcmp(desc.name, ColorSpaceSignatureToStr(sigs[i])) == 0,
          "Describe().name agrees with ColorSpaceSignatureToStr()");

    // bytes[] is the raw big-endian signature plus a terminator.
    check(desc.bytes[0] == (char)(unsigned char)((sigs[i] >> 24) & 0xFF) &&
            desc.bytes[1] == (char)(unsigned char)((sigs[i] >> 16) & 0xFF) &&
            desc.bytes[2] == (char)(unsigned char)((sigs[i] >> 8) & 0xFF) &&
            desc.bytes[3] == (char)(unsigned char)(sigs[i] & 0xFF) &&
            desc.bytes[4] == '\0',
          "Describe().bytes is the raw signature, NUL terminated");
  }
}

// --- 5. IsSpaceSpectralPCS is exactly one signature. ---
void testSpectralPcs()
{
  check(IsSpaceSpectralPCS(icSigSpectralPcsData), "spectral PCS accepted");
  check(!IsSpaceSpectralPCS(icSigLabData), "Lab is not spectral PCS");
  check(!IsSpaceSpectralPCS((icColorSpaceSignature)0x6d630003),
        "an MCS space is not spectral PCS");
}

// --- 6. The MCS naming behaviour #2072 landed, from @xsscx's original assertions. ---
// These are what his signature-utils.cpp checked; against master before #2072 the
// GetColorSpaceSigName case failed with "CIccInfo lost MCS signature context".
void testMcsNamingStillHolds()
{
  const icUInt32Number mcs = (icUInt32Number)(icSigSrcMCSChannelData + 3);

  icChar buf[64];
  const char *compact = icGetColorSigStr(buf, sizeof(buf), mcs);
  if (!compact || std::strcmp(compact, "mc0003") != 0) {
    ++g_fail;
    std::fprintf(stderr, "[signature-utils-contract] FAIL: icGetColorSigStr(mc0003) "
                         "(got \"%s\", want \"mc0003\")\n", compact ? compact : "(null)");
  }

  CIccInfo info;
  const char *name = info.GetColorSpaceSigName((icColorSpaceSignature)mcs);
  check(name != NULL && std::strstr(name, "MCS") != NULL,
        "CIccInfo::GetColorSpaceSigName keeps the MCS context");
}

// --- 7. The technology signature lists, which drifted the way section 4 guards against. ---
// FOUR places carry technology-signature knowledge and none of them consulted the others:
//
//   CIccInfo::GetTechnologySigName()      names one         (IccUtil.cpp)
//   IsValidTechnologySignature()          accepts one       (this header)
//   CIccTagSignature::Validate()          accepts one, for a technologyTag
//                                                           (IccTagBasic.cpp:3021)
//   CIccTagProfileSeqDesc::Validate()     accepts one, for a profileSequenceDesc entry
//                                                           (IccTagBasic.cpp:10932)
//
// Only CIccTagSignature::Validate() carried all 26 enum members. The other three stopped
// at the 22 that predate ICC.1 v4.3, so the four motion picture and digital cinema
// signatures validated as compliant in a technologyTag and simultaneously printed as
// "Unknown 'mpfs'" -- measured on master dd6164ee via iccDumpProfile before #2101.
//
// The two Validate() sites legitimately differ -- the profileSequenceDesc one also
// accepts icSigUndefined, "technology not defined", which is not valid for a
// technologyTag -- so they cannot share this header's predicate without an "undefined
// permitted" flag, and each keeps its own switch. Both are callable in process:
// CIccTag::Validate ignores its pProfile argument, CIccTagSignature carries SetValue()
// and CIccTagProfileSeqDesc::m_Descriptions is public, so testValidateTechnologyRows()
// below pins them directly rather than trusting the switches to stay in step.
const char *expectedTechnologyName(icTechnologySignature sig)
{
  // Deliberately NO default: label. Adding a member to icTechnologySignature makes this
  // switch incomplete, which -Wswitch reports and which this target promotes to an error
  // via -Werror=switch (set in iccdev_add_signature_utils_contract_test).  The promotion
  // is the point: this target is EXCLUDE_FROM_ALL, so the -Werror lane (ci-pr-gcc15.yml)
  // builds the default `all` target and never compiles this file -- an unpromoted warning
  // here would be emitted into a log nothing greps.  That is the compile-time half of the
  // guard; the runtime table below is hand-maintained and would stay green for a 27th
  // member, which is precisely the failure mode #2101 is about.
  switch (sig) {
  case icSigDigitalCamera:              return "DigitalCamera";
  case icSigFilmScanner:                return "FilmScanner";
  case icSigReflectiveScanner:          return "ReflectiveScanner";
  case icSigInkJetPrinter:              return "InkJetPrinter";
  case icSigThermalWaxPrinter:          return "ThermalWaxPrinter";
  case icSigElectrophotographicPrinter: return "ElectrophotographicPrinter";
  case icSigElectrostaticPrinter:       return "ElectrostaticPrinter";
  case icSigDyeSublimationPrinter:      return "DyeSublimationPrinter";
  case icSigPhotographicPaperPrinter:   return "PhotographicPaperPrinter";
  case icSigFilmWriter:                 return "FilmWriter";
  case icSigVideoMonitor:               return "VideoMonitor";
  case icSigVideoCamera:                return "VideoCamera";
  case icSigProjectionTelevision:       return "ProjectionTelevision";
  case icSigCRTDisplay:                 return "CRTDisplay";
  case icSigPMDisplay:                  return "PMDisplay";
  case icSigAMDisplay:                  return "AMDisplay";
  case icSigLCDDisplay:                 return "LCDDisplay";
  case icSigOLEDDisplay:                return "OLEDDisplay";
  case icSigPhotoCD:                    return "PhotoCD";
  case icSigPhotoImageSetter:           return "PhotoImageSetter";
  case icSigGravure:                    return "Gravure";
  case icSigOffsetLithography:          return "OffsetLithography";
  case icSigSilkscreen:                 return "Silkscreen";
  case icSigFlexography:                return "Flexography";
  case icSigMotionPictureFilmScanner:   return "MotionPictureFilmScanner";
  case icSigMotionPictureFilmRecorder:  return "MotionPictureFilmRecorder";
  case icSigDigitalMotionPictureCamera: return "DigitalMotionPictureCamera";
  case icSigDigitalCinemaProjector:     return "DigitalCinemaProjector";

  // Not technologies: "not defined" and the enum terminator. Named only so the switch
  // stays exhaustive; both are deliberately absent from kTechnology[] below, and the NULL
  // return means that adding either one there would be reported as a failure rather than
  // silently tolerated.
  case icSigUndefined:                  return NULL;
  case icMaxEnumTechnology:             return NULL;
  }

  return NULL;
}

void testTechnologySignatures()
{
  // The iteration list. Names are NOT repeated here -- expectedTechnologyName() above is
  // the single source for those, so the two cannot disagree.
  const icUInt32Number kTechnology[] = {
    (icUInt32Number)icSigDigitalCamera,              (icUInt32Number)icSigFilmScanner,
    (icUInt32Number)icSigReflectiveScanner,          (icUInt32Number)icSigInkJetPrinter,
    (icUInt32Number)icSigThermalWaxPrinter,          (icUInt32Number)icSigElectrophotographicPrinter,
    (icUInt32Number)icSigElectrostaticPrinter,       (icUInt32Number)icSigDyeSublimationPrinter,
    (icUInt32Number)icSigPhotographicPaperPrinter,   (icUInt32Number)icSigFilmWriter,
    (icUInt32Number)icSigVideoMonitor,               (icUInt32Number)icSigVideoCamera,
    (icUInt32Number)icSigProjectionTelevision,       (icUInt32Number)icSigCRTDisplay,
    (icUInt32Number)icSigPMDisplay,                  (icUInt32Number)icSigAMDisplay,
    // Table 29 prints these between 'AMD ' and 'KPCD'.  Unlike the v4.3 rows below they
    // had no enumerator at all, so -Werror=switch could not have flagged their absence.
    (icUInt32Number)icSigLCDDisplay,                 (icUInt32Number)icSigOLEDDisplay,
    (icUInt32Number)icSigPhotoCD,                    (icUInt32Number)icSigPhotoImageSetter,
    (icUInt32Number)icSigGravure,                    (icUInt32Number)icSigOffsetLithography,
    (icUInt32Number)icSigSilkscreen,                 (icUInt32Number)icSigFlexography,
    // The four v4.3 rows. Before #2101 each of these failed BOTH checks below.
    (icUInt32Number)icSigMotionPictureFilmScanner,   (icUInt32Number)icSigMotionPictureFilmRecorder,
    (icUInt32Number)icSigDigitalMotionPictureCamera, (icUInt32Number)icSigDigitalCinemaProjector,
  };

  CIccInfo info;

  for (size_t i = 0; i < sizeof(kTechnology) / sizeof(kTechnology[0]); i++) {
    const icTechnologySignature sig = (icTechnologySignature)kTechnology[i];
    const char *want = expectedTechnologyName(sig);
    const char *got  = info.GetTechnologySigName(sig);

    // GetUnknownName() formats "Unknown 'xxxx' = ...", so an unnamed signature is not
    // merely a different string -- asserting the exact name is what makes this fail
    // loudly rather than drift into a plausible-looking fallback.
    if (!want || !got || std::strcmp(got, want) != 0) {
      ++g_fail;
      std::fprintf(stderr, "[signature-utils-contract] FAIL: GetTechnologySigName(0x%08X) "
                           "(got \"%s\", want \"%s\")\n",
                   (unsigned)kTechnology[i], got ? got : "(null)", want ? want : "(null)");
    }

    // Named per signature rather than with one shared string: the two lists are
    // independent, so a signature that is named but not accepted has to say which.
    if (!IsValidTechnologySignature(kTechnology[i])) {
      ++g_fail;
      std::fprintf(stderr, "[signature-utils-contract] FAIL: IsValidTechnologySignature"
                           "(0x%08X) rejects %s\n",
                   (unsigned)kTechnology[i], want ? want : "(unnamed)");
    }
  }

  // SPEC-ISSUE #2101: ICC.1-2022-05 prints the digital cinema projector mnemonic as
  // 'dcpj' while giving its hex as 64636A70h = 'dcjp'. iccDEV encodes the hex, so the
  // transposed spelling is NOT a technology signature. Pinned in both directions so that
  // a future "correction" toward the mnemonic cannot land silently: it would flip which
  // of these two assertions fails, and whichever way the ICC rules, that flip must be a
  // deliberate change to this test rather than an invisible one.
  check(IsValidTechnologySignature(0x64636A70), "the published hex 'dcjp' is the signature");
  check(!IsValidTechnologySignature(0x6463706A), "the published mnemonic 'dcpj' is not");

  check(!IsValidTechnologySignature((icUInt32Number)icSigUndefined),
        "icSigUndefined is not a technology");
  check(!IsValidTechnologySignature(0x4A554E4B), "'JUNK' is not a technology");
}

// --- 8. The two Validate() sites, pinned directly. ---
// Section 7 covers the naming pair; this covers the only change in #2101 that alters a
// validation VERDICT. Without it that hunk has no coverage at all: no profile in the
// tracked corpus carries these four signatures, so reverting it leaves CI green while a
// legal profileSequenceDesc entry is again reported NonCompliant.
// Build a profileSequenceDesc entry that is safe to validate.
//
// CIccProfileDescStruct's default constructor is empty (IccTagBasic.cpp:10605) while its
// copy constructor reads every member, and push_back copies -- so m_deviceMfg,
// m_deviceModel and m_attributes have to be set or the copy reads indeterminate values,
// which a sanitizer lane would flag even though Validate() never inspects them.
//
// Both description tags are required for a different reason: Validate() dereferences
// m_deviceMfgDesc.GetTag() and m_deviceModelDesc.GetTag() unconditionally
// (IccTagBasic.cpp:10995-10996), so a struct without them segfaults before reaching the
// technology switch. SetType() returns false and leaves the pointer null if the tag
// factory fails, so its result is checked -- otherwise a factory regression would crash
// this test instead of failing an assertion in it.
void addSeqEntry(CIccTagProfileSeqDesc &seq, icTechnologySignature tech)
{
  CIccProfileDescStruct desc;
  desc.m_deviceMfg   = 0;
  desc.m_deviceModel = 0;
  desc.m_attributes  = 0;
  desc.m_technology  = tech;

  check(desc.m_deviceMfgDesc.SetType(icSigMultiLocalizedUnicodeType),
        "deviceMfgDesc tag allocated");
  check(desc.m_deviceModelDesc.SetType(icSigMultiLocalizedUnicodeType),
        "deviceModelDesc tag allocated");

  seq.m_Descriptions->push_back(desc);
}

void testValidateTechnologyRows()
{
  // Assert on the technology FINDING, not on the aggregate verdict. A hand-built
  // profileSequenceDesc entry carries empty text tags and so validates as Warning no
  // matter what the technology switch decides; keying off icValidateOK would couple this
  // test to unrelated findings and, worse, would still pass if "Unknown Technology" came
  // back alongside some other warning.
  const char *kUnknown = "Unknown Technology";

  // Every row whose Validate() verdict #2101 changed: the four ICC.1 v4.3 technologies,
  // then Table 29's two display rows, which had no enumerator at all until they were
  // added alongside these.  Both switches below carry a default: label, so a row missing
  // from either is reported NonCompliant rather than caught by -Werror=switch.
  const icTechnologySignature kAddedRows[] = {
    icSigMotionPictureFilmScanner, icSigMotionPictureFilmRecorder,
    icSigDigitalMotionPictureCamera, icSigDigitalCinemaProjector,
    icSigLCDDisplay, icSigOLEDDisplay,
  };

  for (size_t i = 0; i < sizeof(kAddedRows) / sizeof(kAddedRows[0]); i++) {
    // A technologyTag holding the signature. CIccTagSignature::Validate keys off the
    // first signature in the path, so the path has to say technologyTag for the
    // technology switch to be the one that runs.
    CIccTagSignature tag;
    tag.SetValue((icUInt32Number)kAddedRows[i]);
    std::string rpt;
    tag.Validate(icGetSigPath(icSigTechnologyTag), rpt, NULL);
    if (rpt.find(kUnknown) != std::string::npos) {
      ++g_fail;
      std::fprintf(stderr, "[signature-utils-contract] FAIL: technologyTag 0x%08X "
                           "reported unknown by CIccTagSignature::Validate\n",
                   (unsigned)kAddedRows[i]);
    }

    // The same signature in a profileSequenceDesc entry, which is a separate switch.
    CIccTagProfileSeqDesc seq;
    addSeqEntry(seq, kAddedRows[i]);
    std::string seqRpt;
    seq.Validate(icGetSigPath(icSigProfileSequenceDescTag), seqRpt, NULL);
    if (seqRpt.find(kUnknown) != std::string::npos) {
      ++g_fail;
      std::fprintf(stderr, "[signature-utils-contract] FAIL: profileSequenceDesc 0x%08X "
                           "reported unknown by CIccTagProfileSeqDesc::Validate\n",
                   (unsigned)kAddedRows[i]);
    }
  }

  // Anti-vacuity: the same two calls with a signature that really is not a technology
  // must still produce the finding. Without this pair, deleting the switches' default
  // branch entirely would leave every assertion above green.
  {
    CIccTagSignature tag;
    tag.SetValue(0x4A554E4B);  // 'JUNK'
    std::string rpt;
    tag.Validate(icGetSigPath(icSigTechnologyTag), rpt, NULL);
    check(rpt.find(kUnknown) != std::string::npos,
          "CIccTagSignature::Validate still reports a genuine unknown technology");
  }
  {
    CIccTagProfileSeqDesc seq;
    addSeqEntry(seq, (icTechnologySignature)0x4A554E4B);
    std::string rpt;
    seq.Validate(icGetSigPath(icSigProfileSequenceDescTag), rpt, NULL);
    check(rpt.find(kUnknown) != std::string::npos,
          "CIccTagProfileSeqDesc::Validate still reports a genuine unknown technology");
  }

  // The asymmetry the two switches are entitled to: "technology not defined" is a legal
  // profileSequenceDesc entry and is NOT a legal technologyTag. Pinned so that any future
  // attempt to share one predicate between them has to confront it rather than silently
  // pick one side.
  {
    CIccTagProfileSeqDesc seq;
    addSeqEntry(seq, icSigUndefined);
    std::string rpt;
    seq.Validate(icGetSigPath(icSigProfileSequenceDescTag), rpt, NULL);
    check(rpt.find(kUnknown) == std::string::npos,
          "profileSequenceDesc accepts icSigUndefined");
  }
  {
    CIccTagSignature tag;
    tag.SetValue((icUInt32Number)icSigUndefined);
    std::string rpt;
    tag.Validate(icGetSigPath(icSigTechnologyTag), rpt, NULL);
    check(rpt.find(kUnknown) != std::string::npos,
          "technologyTag rejects icSigUndefined");
  }
}

} // namespace

int main()
{
  testNames();
  testZeroChannelBoundary();
  testValidity();
  testAgreement();
  testSpectralPcs();
  testMcsNamingStillHolds();
  testTechnologySignatures();
  testValidateTechnologyRows();

  if (g_fail)
    std::fprintf(stderr, "[signature-utils-contract] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[signature-utils-contract] all assertions passed\n");

  return g_fail;
}
