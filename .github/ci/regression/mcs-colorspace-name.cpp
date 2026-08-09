// Regression for the MCS colour-space naming gap harvested from ci-qa-flags (#1853).
//
// icSigSrcMCSChannelData ("mc" + a 16-bit channel count, icProfileHeader.h:941) is a
// channel-count-encoding colour-space family exactly like icSigNChannelData and the
// four spectral families. icNumColorSpaceChannels() has always decoded it, but three
// naming functions omitted it from their case lists, so every MCS signature fell
// through to a generic fallback:
//
//   icGetColorSig(0x6D630004)            'mc??' = 6D630004   instead of  "mc0004"
//   icGetColorSigStr(0x6D630004)         6D630004h           instead of   mc0004
//   GetColorSpaceSigName(0x6D630004)     0x4ColorData        instead of   0x0004ChannelMCSData
//
// icGetColorSigStr() is the XML and JSON header writer for <MCS> (IccProfileXml.cpp,
// IccProfileJson.cpp), so the fallback reached serialized profiles. The value was not
// lost -- icGetSigVal()'s 8/9-character branch parses the hex escape back to the same
// signature -- so this pins legibility and cross-family consistency, and the
// round-trip assertions below exist to keep the fix from trading one for the other.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

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
    std::fprintf(stderr, "[mcs-colorspace-name] FAIL: %s\n", what);
  }
}

void checkStr(const char *got, const char *want, const char *what)
{
  if (!got || std::strcmp(got, want) != 0) {
    ++g_fail;
    std::fprintf(stderr, "[mcs-colorspace-name] FAIL: %s (got \"%s\", want \"%s\")\n",
                 what, got ? got : "(null)", want);
  }
}

// A four-channel and a fifteen-channel MCS signature. Fifteen is included because the
// count is formatted with %04X: a single hex digit would expose any loss of the zero
// padding the sibling families emit.
const icUInt32Number kMcs4  = 0x6D630004;   // "mc" + 0x0004
const icUInt32Number kMcs15 = 0x6D63000F;   // "mc" + 0x000F

// --- 1. The channel count is decoded, which is what makes naming possible at all. ---
void testChannelCountDecoding()
{
  check(icGetColorSpaceType((icColorSpaceSignature)kMcs4) == icSigSrcMCSChannelData,
        "mc0004: icGetColorSpaceType() reports icSigSrcMCSChannelData");
  check(icNumColorSpaceChannels((icColorSpaceSignature)kMcs4) == 4,
        "mc0004: icNumColorSpaceChannels() == 4");
  check(icNumColorSpaceChannels((icColorSpaceSignature)kMcs15) == 15,
        "mc000F: icNumColorSpaceChannels() == 15");
}

// --- 2. The three naming functions emit the sibling form, not the fallback. ---
void testNaming()
{
  icChar buf[256];

  checkStr(icGetColorSig(buf, sizeof(buf), kMcs4), "\"mc0004\"",
           "mc0004: icGetColorSig()");
  checkStr(icGetColorSig(buf, sizeof(buf), kMcs15), "\"mc000F\"",
           "mc000F: icGetColorSig()");

  checkStr(icGetColorSigStr(buf, sizeof(buf), kMcs4), "mc0004",
           "mc0004: icGetColorSigStr()");
  checkStr(icGetColorSigStr(buf, sizeof(buf), kMcs15), "mc000F",
           "mc000F: icGetColorSigStr()");

  CIccInfo info;
  checkStr(info.GetColorSpaceSigName((icColorSpaceSignature)kMcs4),
           "0x0004ChannelMCSData", "mc0004: GetColorSpaceSigName()");
  checkStr(info.GetColorSpaceSigName((icColorSpaceSignature)kMcs15),
           "0x000FChannelMCSData", "mc000F: GetColorSpaceSigName()");
}

// --- 3. The XML/JSON header text still parses back to the same signature. ---
// icGetSigVal() dispatches on strlen: the previous "6D630004h" took the 8/9-character
// hex branch, the corrected "mc0004" takes the 6-character channel-signature branch.
// Both are lossless, but they are different code paths, so the fix moves which one
// runs for a real profile and that move is worth pinning.
void testRoundTrip()
{
  icChar buf[256];

  check(icGetSigVal(icGetColorSigStr(buf, sizeof(buf), kMcs4)) == kMcs4,
        "mc0004: icGetColorSigStr() -> icGetSigVal() round-trips");
  check(icGetSigVal(icGetColorSigStr(buf, sizeof(buf), kMcs15)) == kMcs15,
        "mc000F: icGetColorSigStr() -> icGetSigVal() round-trips");
}

// --- 4. The sibling families are unchanged. ---
// The fix adds a case to switches shared with icSigNChannelData and the spectral
// families; these assertions fail if that case were ever added in a way that
// reordered or shadowed them.
void testSiblingsUnaffected()
{
  icChar buf[256];

  checkStr(icGetColorSigStr(buf, sizeof(buf), 0x6E630004), "nc0004",
           "nc0004: icGetColorSigStr() unchanged");
  checkStr(icGetColorSigStr(buf, sizeof(buf), 0x7273001F), "rs001F",
           "rs001F: icGetColorSigStr() unchanged");

  CIccInfo info;
  checkStr(info.GetColorSpaceSigName((icColorSpaceSignature)0x6E630004),
           "0x0004ChannelData", "nc0004: GetColorSpaceSigName() unchanged");
  checkStr(info.GetColorSpaceSigName((icColorSpaceSignature)0x7273001F),
           "0x001FChannelReflectanceData",
           "rs001F: GetColorSpaceSigName() unchanged");
}

} // namespace

int main()
{
  testChannelCountDecoding();
  testNaming();
  testRoundTrip();
  testSiblingsUnaffected();

  if (g_fail)
    std::fprintf(stderr, "[mcs-colorspace-name] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[mcs-colorspace-name] all assertions passed\n");

  return g_fail;
}
