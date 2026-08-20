/*
    File:       curve-gamma-u8fixed8.cpp

    Contains:   CTest for the u8Fixed8Number gamma reconstruction that #808 fixed
                (#815/#2044).

    A one-entry curveType holds a gamma exponent as a u8Fixed8Number: an unsigned
    16-bit integer with eight fractional bits, so the encoded value is raw/256 and
    y = x^gamma.  ReadUInt16Float() normalizes on read by dividing by 65535, which
    means every consumer has to undo that with

        m_Curve[0] * 65535.0 / 256.0

    and NOT m_Curve[0] * 256.0.  The ratio 65536/65535 between the two is why
    Describe() and DumpLut() printed 2.2071 instead of 2.20703125 from 2005 until
    #808, and why CIccTagJsonCurve::ToJson wrote the raw normalized 0.00862 instead
    of a gamma at all.

    WHY THIS FILE EXISTS: #808 shipped that fix with a regression -- steps R12/R13
    in ci-iccdev-tool-tests.yml and R8 in ci-tool-tests.yml, covering all four
    fixtures below.  Commit 35f5c5e6 ("Modify: CMake CTest CPack", #968) removed
    both workflows' inline test bodies during the migration to CTest and the gamma
    steps were not carried over, so master has had no coverage of this arithmetic
    since.  Three of the four fixtures had no reference anywhere in the tree.  This
    restores the coverage in the form the migration was moving toward.

    The tolerance is derived, not observed.  #2044 measured the JSON error on
    gamma-2.20703125.icc as 6.01e-08 and proposed pinning something near it; that
    would be wrong for the corpus, because the error is proportional to the gamma
    and gamma-2.3984375.icc is nearly twice as far out at 1.19e-07.  m_Curve[0] is
    an icFloatNumber, so it carries the rounding of raw/65535 into a 24-bit
    significand -- a relative error of at most 2^-24 -- and multiplying by the exact
    ratio 65535/256 scales that straight through.  The bound is therefore
    gamma * 2^-24, which is 1.43e-07 at gamma 2.4 and holds for every fixture.  The
    old R13 check used a flat 0.01, roughly five orders looser than the effect it
    was guarding, so it could not have failed for this reason.

    What each fixture is asserted to do:

      - Describe() must print the exact encoded value.  This is lossless: the
        expression is evaluated in double and printed at %.10lf, so 2.20703125 comes
        out exactly and a regression to *256.0 would read 2.2071068... instead.
      - ToJson() must emit a gamma within the bound above, and that emitted decimal
        must still round back to the stored integer.  The round trip is the property
        that actually matters -- iccFromJson recovers raw 565 from 2.2070311898824 --
        and it is what a presentation-precision change must not break.

    The profile is read through CIccProfileJson with the JSON tag and MPE factories
    pushed, which is exactly what iccToJson does, so the writer under test is the
    shipping one rather than a re-implementation of it.

    Usage:  curve-gamma-u8fixed8 <dir containing gamma-*.icc>

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccProfileJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccUtilJson.h"
#include "IccTagLut.h"
#include "IccIO.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const std::string& label)
{
  if (condition) {
    std::fprintf(stdout, "curve-gamma-u8fixed8: PASS  %s\n", label.c_str());
    return;
  }
  std::fprintf(stdout, "curve-gamma-u8fixed8: FAIL  %s\n", label.c_str());
  ++g_failures;
}

// The four fixtures #808 committed.  raw is the u8Fixed8Number actually stored in
// each of rTRC/gTRC/bTRC; raw/256 is the exact value it encodes, and every one of
// them is exactly representable in binary, which is what lets Describe() be
// compared for equality rather than within a tolerance.
struct Fixture {
  const char*    file;
  unsigned       raw;
  double         gamma;
};

const Fixture kFixtures[] = {
  { "gamma-1.0000000000.icc", 256u, 1.0        },
  { "gamma-1.796875.icc",     460u, 1.796875   },
  { "gamma-2.20703125.icc",   565u, 2.20703125 },
  { "gamma-2.3984375.icc",    614u, 2.3984375  },
};

const icTagSignature kTrcTags[] = {
  icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag
};

const char* kTrcNames[] = { "redTRCTag", "greenTRCTag", "blueTRCTag" };

// m_Curve[0] is float, so raw/65535 is rounded into a 24-bit significand before the
// exact ratio 65535/256 scales it back up.  A relative error of at most 2^-24
// survives that multiplication, so the absolute bound scales with the gamma itself.
double reconstructionBound(double gamma)
{
  return gamma * std::numeric_limits<icFloatNumber>::epsilon() / 2;
}

// Describe() formats the exponent with "%.10lf", so the expected line is spelled the
// same way rather than parsed back out -- a formatting change is itself a regression
// in what a user reads, and #808 was reported from exactly this output.
std::string expectedDescribeLine(double gamma)
{
  char buf[64];
  std::snprintf(buf, sizeof(buf), "Y = X ^ %.10lf", gamma);
  return std::string(buf);
}

// Tags serialize as a "Tags" array of single-key objects, so the tag is located by
// name rather than by index: the ordering is the profile's, not ours.
const IccJson* findTagObject(const IccJson& profile, const std::string& tagName)
{
  if (!profile.contains("Tags") || !profile["Tags"].is_array())
    return NULL;

  for (IccJson::const_iterator it = profile["Tags"].begin();
       it != profile["Tags"].end(); ++it) {
    if (it->is_object() && it->contains(tagName))
      return &(*it)[tagName];
  }
  return NULL;
}

// All four fixtures point rTRC, gTRC and bTRC at a single shared tag offset, which
// is legal and common -- the tag table may name the same data more than once.  The
// writer serializes the first occurrence and emits {"sameAs": "<tag>"} for the rest,
// so a lookup that only understood "data" would report the green and blue curves as
// missing rather than as shared.  Following the link keeps all three channels
// asserted, and pins the de-duplication itself: if it regressed to writing the value
// out three times, or to naming a tag that is not in the document, this stops
// resolving.  The hop count is bounded so a self- or mutual reference cannot spin.
const IccJson* findTagData(const IccJson& profile, const char* tagName)
{
  std::string name(tagName);

  for (int hops = 0; hops < 4; hops++) {
    const IccJson* pTag = findTagObject(profile, name);
    if (!pTag || !pTag->is_object())
      return NULL;

    if (pTag->contains("data"))
      return &(*pTag)["data"];

    if (!pTag->contains("sameAs") || !(*pTag)["sameAs"].is_string())
      return NULL;

    const std::string next = (*pTag)["sameAs"].get<std::string>();
    if (next == name)
      return NULL;
    name = next;
  }
  return NULL;
}

void testFixture(const std::string& dir, const Fixture& fx)
{
  const std::string path = dir + "/" + fx.file;
  const std::string tag = std::string(fx.file) + ": ";

  CIccProfileJson profile;
  CIccFileIO io;

  if (!io.Open(path.c_str(), "r")) {
    check(false, tag + "fixture opens");
    return;
  }
  if (!profile.Read(&io)) {
    check(false, tag + "fixture reads");
    return;
  }

  // ---- library side: the value the curve holds, and what Describe() prints ----
  for (int i = 0; i < 3; i++) {
    CIccTag* pTag = profile.FindTag(kTrcTags[i]);
    const std::string what = tag + kTrcNames[i] + " ";

    if (!pTag || pTag->GetType() != icSigCurveType) {
      check(false, what + "is a curveType");
      continue;
    }

    CIccTagCurve* pCurve = static_cast<CIccTagCurve*>(pTag);
    if (pCurve->GetSize() != 1) {
      check(false, what + "holds exactly one entry (a gamma, not a table)");
      continue;
    }

    // The stored float must still identify the encoded integer.  This is the
    // reader's half of the contract and is exact: raw/65535 rounds to a float that
    // maps back to raw and to no other value.
    const double stored = static_cast<double>((*pCurve)[0]);
    check(std::lround(stored * 65535.0) == static_cast<long>(fx.raw),
          what + "normalized value recovers the stored u8Fixed8 integer");

    std::string desc;
    pCurve->Describe(desc, 100);
    const std::string expected = expectedDescribeLine(fx.gamma);
    check(desc.find(expected) != std::string::npos,
          what + "Describe() prints \"" + expected + "\"");
  }

  // ---- writer side: what iccToJson emits for the same curves ----
  IccJson j;
  if (!profile.ToJson(j)) {
    check(false, tag + "serializes to JSON");
    return;
  }

  for (int i = 0; i < 3; i++) {
    const std::string what = tag + kTrcNames[i] + " ";
    const IccJson* pData = findTagData(j, kTrcNames[i]);

    if (!pData || !pData->contains("gamma")) {
      check(false, what + "JSON carries a gamma");
      continue;
    }
    check((*pData)["curveType"] == "gamma",
          what + "JSON curveType is \"gamma\"");

    const double emitted = (*pData)["gamma"].get<double>();
    const double bound = reconstructionBound(fx.gamma);
    char label[256];

    std::snprintf(label, sizeof(label),
                  "JSON gamma %.12g is within %.3g of %.10f (error %.3g)",
                  emitted, bound, fx.gamma, std::fabs(emitted - fx.gamma));
    check(std::fabs(emitted - fx.gamma) <= bound, what + label);

    // The round trip is the contract iccFromJson relies on: whatever decimal is
    // emitted must still land on the same u8Fixed8 integer when it is read back.
    check(std::lround(emitted * 256.0) == static_cast<long>(fx.raw),
          what + "JSON gamma rounds back to the stored u8Fixed8 integer");
  }
}

}  // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <dir containing gamma-*.icc>\n",
                 argv[0] ? argv[0] : "curve-gamma-u8fixed8");
    return 1;
  }

  // iccToJson's own wiring: without these the tags come back as plain IccProfLib
  // objects with no ToJson, and the writer half of this test would silently cover
  // nothing.
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  const std::string dir(argv[1]);
  const size_t nFixtures = sizeof(kFixtures) / sizeof(kFixtures[0]);
  for (size_t i = 0; i < nFixtures; i++)
    testFixture(dir, kFixtures[i]);

  if (g_failures) {
    std::fprintf(stdout, "curve-gamma-u8fixed8: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "curve-gamma-u8fixed8: all checks passed\n");
  return 0;
}
