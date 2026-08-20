// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression for #2146: CIccCmm::ToInternalEncoding() had no icEncodeUnitFloat
// case for either PCS space, while FromInternalEncoding() pairs it with
// icEncodeFloat in both -- so the library wrote data it then refused to read.
//
// FromInternalEncoding()'s two PCS branches accept the selector:
//
//   case icSigLabData:                     case icSigXYZData:
//     case icEncodeUnitFloat:                case icEncodeFloat:
//     case icEncodeFloat:                    case icEncodeUnitFloat:
//       break;                                 icXyzFromPcs(pInput); break;
//
// ToInternalEncoding()'s did not: its icSigLabData arm listed icEncodeValue,
// icEncodeFloat and the three integer encodings, and its icSigXYZData arm the
// same plus icEncodePercent. icEncodeUnitFloat fell to the shared
// "default: return icCmmStatBadColorEncoding".
//
// End to end through iccApplyNamedCmm, measured on 7828681a before the fix --
// the tool refuses its own output:
//
//   iccApplyNamedCmm rgb8bit.txt 2 0 sRGB_v4_ICC_preference.icc 1
//     -> 'Lab ' / icEncodeUnitFloat, rc=0
//   iccApplyNamedCmm <that output> 3 0 sRGB_v4_ICC_preference.icc 1
//     -> "Invalid source data encoding", rc=1
//
// Same break with an XYZ PCS profile (sRGB_D65_MAT.icc); the icEncodeFloat
// control round-trips cleanly, which is what isolates the missing case from
// any question about the values themselves.
//
// The fix pairs the selector with icEncodeFloat in both arms, mirroring the
// destination side exactly rather than giving it a clipping body of its own.
// That choice is load-bearing and is pinned by testXyzUnitFloatIsNotClipped()
// below: icXyzFromPcs scales by 65535/32768, so the external XYZ float range
// runs to ~2.0, and clipping a source to 0.0-1.0 -- the reading of "unit
// float" that the *device* branch applies -- would silently discard every
// legitimate value above 1.0 rather than harden anything. The device branch
// can clip because its external range genuinely is 0.0-1.0; the PCS branches
// cannot.
//
// Scope note: this pins the two PCS spaces only. Source/destination acceptance
// is deliberately NOT asserted as a universal rule, because it is not one --
// icSigRgbData reaches the shared default: arm, whose icEncodeValue case
// rejects a source when the space is not CLR while the matching destination
// case merely no-ops. That asymmetry is a separate question about what
// icEncodeValue means for a device space, is unchanged by this fix, and is
// measured rather than assumed by testDeviceValueAsymmetryIsUnchanged().

// Sibling coverage, so the two are not later merged or duplicated:
// tointernal-fixedint-roundtrip.cpp asserts From(To(x)) over the *fixed
// integer* encodings for the CLR, Lab and XYZ paths. It is scoped to those
// three selectors by its own header and never exercises icEncodeUnitFloat,
// which is why it did not catch this. This file is the float-selector half and
// composes the other way round, To(From(x)), because the defect is on the
// source side of a value the destination side produced.

#include <cstdio>
#include <cmath>

#include "IccCmm.h"

// A -DENABLE_USEICCDEVNAMESPACE=ON build wraps IccProfLib in the iccDEV
// namespace (Build/Cmake/CMakeLists.txt:709), so every name used below moves.
// Guarded as the 24 sibling harnesses that carry this are. Carried on the same
// grounds they are rather than on a passing build: that option does not
// currently configure at all -- IccProfLib/IccTagEmbedIcc.h has none of the
// namespace wrapping its siblings carry, so IccProfLib2 itself fails with 70
// errors on unmodified master before any test is reached. This guard is
// therefore correct-by-consistency and untested end to end.
#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    std::printf("[pcs-unitfloat] FAIL: %s\n", what);
    ++g_fail;
  }
}

bool accepted(icStatusCMM rv)
{
  return rv == icCmmStatOk;
}

// A source-side acceptance probe. The values are irrelevant to whether the
// selector is accepted -- the switch is reached before any of them is read --
// so a single mid-range triple serves every encoding.
bool toAccepts(icColorSpaceSignature space, icFloatColorEncoding enc)
{
  const icFloatNumber in[3] = {0.5f, 0.5f, 0.5f};
  icFloatNumber out[3] = {0.0f, 0.0f, 0.0f};

  return accepted(CIccCmm::ToInternalEncoding(space, enc, out, in));
}

bool fromAccepts(icColorSpaceSignature space, icFloatColorEncoding enc)
{
  const icFloatNumber in[3] = {0.5f, 0.5f, 0.5f};
  icFloatNumber out[3] = {0.0f, 0.0f, 0.0f};

  return accepted(CIccCmm::FromInternalEncoding(space, enc, out, in));
}

const char *encName(icFloatColorEncoding enc)
{
  switch (enc) {
    case icEncodeValue:     return "icEncodeValue";
    case icEncodePercent:   return "icEncodePercent";
    case icEncodeUnitFloat: return "icEncodeUnitFloat";
    case icEncodeFloat:     return "icEncodeFloat";
    case icEncode8Bit:      return "icEncode8Bit";
    case icEncode16Bit:     return "icEncode16Bit";
    case icEncode16BitV2:   return "icEncode16BitV2";
    default:                return "?";
  }
}

// The whole per-space contract, both directions, written out rather than
// derived. Each cell is what the library does after the fix; the two
// icEncodeUnitFloat source cells are the ones #2146 changes, and every other
// cell is recorded so a later edit that widens or narrows a PCS arm for some
// unrelated reason is caught here too.
struct Expectation {
  icFloatColorEncoding enc;
  bool                 toOk;
  bool                 fromOk;
};

void testLabAcceptanceMatrix()
{
  const Expectation table[] = {
    {icEncodeValue,     true,  true },
    {icEncodePercent,   false, false},  // absent from both arms: the #2124 contract
    {icEncodeUnitFloat, true,  true },  // #2146: the source half was missing
    {icEncodeFloat,     true,  true },
    {icEncode8Bit,      true,  true },
    {icEncode16Bit,     true,  true },
    {icEncode16BitV2,   true,  true },
  };

  for (const Expectation &e : table) {
    char what[160];

    std::snprintf(what, sizeof(what), "'Lab ' source must %s %s",
                  e.toOk ? "accept" : "reject", encName(e.enc));
    check(toAccepts(icSigLabData, e.enc) == e.toOk, what);

    std::snprintf(what, sizeof(what), "'Lab ' destination must %s %s",
                  e.fromOk ? "accept" : "reject", encName(e.enc));
    check(fromAccepts(icSigLabData, e.enc) == e.fromOk, what);
  }
}

void testXyzAcceptanceMatrix()
{
  const Expectation table[] = {
    {icEncodeValue,     true,  true },
    {icEncodePercent,   true,  true },
    {icEncodeUnitFloat, true,  true },  // #2146: the source half was missing
    {icEncodeFloat,     true,  true },
    {icEncode8Bit,      false, false},  // absent from both arms: the #2124 contract
    {icEncode16Bit,     true,  true },
    {icEncode16BitV2,   true,  true },
  };

  for (const Expectation &e : table) {
    char what[160];

    std::snprintf(what, sizeof(what), "'XYZ ' source must %s %s",
                  e.toOk ? "accept" : "reject", encName(e.enc));
    check(toAccepts(icSigXYZData, e.enc) == e.toOk, what);

    std::snprintf(what, sizeof(what), "'XYZ ' destination must %s %s",
                  e.fromOk ? "accept" : "reject", encName(e.enc));
    check(fromAccepts(icSigXYZData, e.enc) == e.fromOk, what);
  }
}

// The defect stated as the property it broke: anything the destination side
// can write, the source side must be able to read back to the value it started
// from. Run over both PCS spaces for icEncodeUnitFloat, with icEncodeFloat
// alongside as the control that always worked.
void testPcsRoundTrip(icColorSpaceSignature space, icFloatColorEncoding enc,
                      const icFloatNumber *internal, const char *label)
{
  icFloatNumber external[3] = {0.0f, 0.0f, 0.0f};
  icFloatNumber back[3]     = {0.0f, 0.0f, 0.0f};
  char what[160];

  const bool wrote = accepted(CIccCmm::FromInternalEncoding(space, enc, external, internal));

  std::snprintf(what, sizeof(what), "%s: destination must accept %s", label, encName(enc));
  check(wrote, what);
  if (!wrote)
    return;  // the value comparisons below would report noise, not a second defect

  std::snprintf(what, sizeof(what), "%s: source must accept %s", label, encName(enc));
  check(accepted(CIccCmm::ToInternalEncoding(space, enc, back, external)), what);

  for (int i = 0; i < 3; ++i) {
    std::snprintf(what, sizeof(what), "%s: %s channel %d must round trip (%g -> %g -> %g)",
                  label, encName(enc), i,
                  (double)internal[i], (double)external[i], (double)back[i]);
    check(std::fabs(back[i] - internal[i]) < 1.0e-5f, what);
  }
}

void testUnitFloatRoundTrips()
{
  const icFloatNumber lab[3] = {0.62f, 0.48f, 0.55f};
  const icFloatNumber xyz[3] = {0.42f, 0.45f, 0.38f};

  testPcsRoundTrip(icSigLabData, icEncodeUnitFloat, lab, "'Lab '");
  testPcsRoundTrip(icSigLabData, icEncodeFloat,     lab, "'Lab ' control");
  testPcsRoundTrip(icSigXYZData, icEncodeUnitFloat, xyz, "'XYZ '");
  testPcsRoundTrip(icSigXYZData, icEncodeFloat,     xyz, "'XYZ ' control");
}

// The anti-clipping pin. An internal XYZ of 0.9 leaves the destination side as
// ~1.8 because icXyzFromPcs scales by 65535/32768. If the source arm is ever
// given a 0.0-1.0 clip -- the plausible "but it is called unit float" edit --
// that 1.8 is clamped to 1.0 and comes back as 0.5 instead of 0.9, so this
// fails while the acceptance matrix above still passes.
void testXyzUnitFloatIsNotClipped()
{
  const icFloatNumber internal[3] = {0.9f, 0.95f, 0.85f};
  icFloatNumber external[3] = {0.0f, 0.0f, 0.0f};
  icFloatNumber back[3]     = {0.0f, 0.0f, 0.0f};

  check(accepted(CIccCmm::FromInternalEncoding(icSigXYZData, icEncodeUnitFloat, external, internal)),
        "'XYZ ' destination must accept icEncodeUnitFloat above the unit range");

  check(external[0] > 1.0f,
        "the premise: an internal 0.9 must leave the destination side above 1.0");

  check(accepted(CIccCmm::ToInternalEncoding(icSigXYZData, icEncodeUnitFloat, back, external)),
        "'XYZ ' source must accept icEncodeUnitFloat above the unit range");

  for (int i = 0; i < 3; ++i) {
    char what[160];

    std::snprintf(what, sizeof(what),
                  "'XYZ ' icEncodeUnitFloat channel %d must survive un-clipped (%g -> %g -> %g)",
                  i, (double)internal[i], (double)external[i], (double)back[i]);
    check(std::fabs(back[i] - internal[i]) < 1.0e-5f, what);
  }
}

// icEncodeUnitFloat and icEncodeFloat are aliases on these two spaces, in both
// directions. Pinned so that a later change giving unit float its own PCS
// semantics has to come through this file rather than silently diverging.
void testUnitFloatAliasesFloat()
{
  const icColorSpaceSignature spaces[] = {icSigLabData, icSigXYZData};
  const icFloatNumber in[3] = {0.37f, 0.61f, 0.44f};

  for (icColorSpaceSignature space : spaces) {
    icFloatNumber fromUnit[3] = {0.0f, 0.0f, 0.0f};
    icFloatNumber fromFloat[3] = {0.0f, 0.0f, 0.0f};
    icFloatNumber toUnit[3] = {0.0f, 0.0f, 0.0f};
    icFloatNumber toFloat[3] = {0.0f, 0.0f, 0.0f};

    CIccCmm::FromInternalEncoding(space, icEncodeUnitFloat, fromUnit, in);
    CIccCmm::FromInternalEncoding(space, icEncodeFloat, fromFloat, in);
    CIccCmm::ToInternalEncoding(space, icEncodeUnitFloat, toUnit, in);
    CIccCmm::ToInternalEncoding(space, icEncodeFloat, toFloat, in);

    for (int i = 0; i < 3; ++i) {
      check(fromUnit[i] == fromFloat[i],
            "destination icEncodeUnitFloat must equal icEncodeFloat on a PCS space");
      check(toUnit[i] == toFloat[i],
            "source icEncodeUnitFloat must equal icEncodeFloat on a PCS space");
    }
  }
}

// Measured, not assumed: the device branch's icEncodeValue asymmetry is real
// and is NOT what #2146 fixes. Recorded here so the scope note at the top of
// this file stays honest, and so that if someone does decide to reconcile it
// the change surfaces as a failure here rather than passing unnoticed.
void testDeviceValueAsymmetryIsUnchanged()
{
  check(!toAccepts(icSigRgbData, icEncodeValue),
        "'RGB ' source still rejects icEncodeValue (unchanged by #2146)");
  check(fromAccepts(icSigRgbData, icEncodeValue),
        "'RGB ' destination still accepts icEncodeValue (unchanged by #2146)");

  // The selector this issue is about is symmetric for device spaces already,
  // which is what made the PCS arms the outlier.
  check(toAccepts(icSigRgbData, icEncodeUnitFloat),
        "'RGB ' source accepts icEncodeUnitFloat");
  check(fromAccepts(icSigRgbData, icEncodeUnitFloat),
        "'RGB ' destination accepts icEncodeUnitFloat");
}

}  // namespace

int main()
{
  testLabAcceptanceMatrix();
  testXyzAcceptanceMatrix();
  testUnitFloatRoundTrips();
  testXyzUnitFloatIsNotClipped();
  testUnitFloatAliasesFloat();
  testDeviceValueAsymmetryIsUnchanged();

  if (!g_fail)
    std::printf("[pcs-unitfloat] PASS\n");

  return g_fail;
}
