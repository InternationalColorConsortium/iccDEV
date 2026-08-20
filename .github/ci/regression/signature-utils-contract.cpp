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
#include "IccUtil.h"
#include "icProfileHeader.h"

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

} // namespace

int main()
{
  testNames();
  testZeroChannelBoundary();
  testValidity();
  testAgreement();
  testSpectralPcs();
  testMcsNamingStillHolds();

  if (g_fail)
    std::fprintf(stderr, "[signature-utils-contract] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[signature-utils-contract] all assertions passed\n");

  return g_fail;
}
