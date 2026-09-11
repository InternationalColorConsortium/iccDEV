// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression for #2520: defects in IccProfLib/IccMpeSpectral.cpp, all from the
// 2015 import, all on the same multiply in CIccMpeSpectralObserver::Apply().
//
// 1. The reflectance CLUT and reflectance observer elements tested their Absolute
//    flag as (m_flags & icRelativeSpectralData). That constant is 0, so the test
//    was always false and the media white adaptation of ICC.2:2023 Formulae (43)
//    to (46) and (51) to (54) ran even when the flag said absolute. Bit 0 of the
//    flags is icAbsoluteSpectralData.
// 2. CIccMpeEmissionObserver::Begin() assigned m_xyzscale[0] three times. No
//    constructor initializes the array, so Apply() multiplied Y and Z by
//    whatever memory the element was allocated over.
// 3. The observers' constructor never set m_flags, and neither the copy
//    constructor nor copyData() copied it. So an observer built in code, and
//    every copy of one, carried leftover memory as its flags. Apply() already
//    read the Lab bit from there; once bit 0 selects absolute, a copied
//    Flags=0 profile could turn absolute too. CIccProfile's copy constructor
//    reaches the element copy constructor through NewCopy().
//
// The fixture is a custom 81-step PCC with a flat illuminant and an observer
// that slopes across the range (x rises, y is flat, z falls), and the white is
// a ramp as well. That makes the media white a different colour from the
// illuminant white. With a flat observer, which is what the #1671 harness uses,
// every spectrum has the same chromaticity, so relative and absolute output
// could only differ by a scale factor.
//
// What is asserted, with the input spectrum equal to the white:
//
// - Flags = 0 (relative): the media white comes out as the illuminant white,
//   Y = 1. The spec's k appears in both kCr and m_w = kCw, so it cancels out of
//   Formulae (43) to (46), and this holds whatever k is. It is also what master
//   produced before the fix, so it pins "relative output is unchanged".
// - Flags = 1 (absolute): the media white keeps its own chromaticity instead of
//   being mapped onto the illuminant's. Only the ratios X/Y and Z/Y are checked,
//   not Y itself. iccDEV's absolute branch normalizes the observer against the
//   illuminant (getEmissiveObserver()), so a perfect reflector has Y = 1, while
//   Formulae (41) and (50) print k = 1 / sum(cy,i * wi). Checking ratios keeps
//   this test out of that question: any factor common to X, Y and Z cancels.
// - Emission observer, Flags = 0: the white emission comes out at Y = 1 with
//   the observer's chromaticity, as Formula (26) defines. m_xyzscale is filled
//   with NaN between construction and Begin(), so an entry Begin() leaves
//   unset comes out as NaN with every compiler and allocator.
// - Observer flags survive construction and copying: one built in code without
//   flags is relative, and one copied with NewCopy() or operator= from a
//   Flags=1 source is absolute. Only the operator= case fails the unfixed build
//   deterministically; the other two read leftover memory there, which ASan's
//   malloc fill makes wrong.
//
// The emission observer's Absolute flag is not exercised. getEmissiveObserver()
// always applies the relative k, so the flag has no effect on that element yet,
// and a check here would only pin the gap.

#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
#include "IccPcc.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char *msg)
{
  if (ok) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

const icUInt16Number kSteps = 81;

icSpectralRange make_range()
{
  icSpectralRange range;
  range.start = icFtoF16(380.0f);
  range.end = icFtoF16(780.0f);
  range.steps = kSteps;
  return range;
}

// Position of sample i along the range, 0 at the first step and 1 at the last.
double ramp(icUInt16Number i)
{
  return static_cast<double>(i) / (kSteps - 1);
}

// The observer's three colour matching functions at sample i.
double cmf_x(icUInt16Number i) { return ramp(i); }
double cmf_y(icUInt16Number) { return 1.0; }
double cmf_z(icUInt16Number i) { return 1.0 - ramp(i); }

// The white: reflectance of the media white for the reflectance elements, and
// spectral emission of the white for the emission observer. It rises across the
// range, so it weights cmf_x more heavily than cmf_z.
double white_at(icUInt16Number i)
{
  return 0.25 + 0.5 * ramp(i);
}

struct Xyz {
  double x, y, z;
};

// sum over i of each colour matching function times s(i), in double precision
// and without going through the library, which is what the expected values
// below are built from.
template <class Spectrum>
Xyz integrate(Spectrum s)
{
  Xyz v = {0.0, 0.0, 0.0};
  for (icUInt16Number i = 0; i < kSteps; ++i) {
    v.x += cmf_x(i) * s(i);
    v.y += cmf_y(i) * s(i);
    v.z += cmf_z(i) * s(i);
  }
  return v;
}

double flat(icUInt16Number) { return 1.0; }

Xyz scaled_to_unit_y(const Xyz &v)
{
  Xyz r = {v.x / v.y, 1.0, v.z / v.y};
  return r;
}

bool close_to(double got, double want)
{
  return std::fabs(got - want) <= 1e-4 * (1.0 + std::fabs(want));
}

void check_xyz(const char *msg, const icFloatNumber *got, const Xyz &want)
{
  std::printf("      got %.6f %.6f %.6f, want %.6f %.6f %.6f\n",
              got[0], got[1], got[2], want.x, want.y, want.z);
  check(close_to(got[0], want.x) && close_to(got[1], want.y) &&
        close_to(got[2], want.z), msg);
}

// Compares X/Y and Z/Y only, so a scale factor shared by all three channels
// cannot make the check pass or fail.
void check_chromaticity(const char *msg, const icFloatNumber *got, const Xyz &want)
{
  bool usable = std::isfinite(got[0]) && std::isfinite(got[1]) &&
                std::isfinite(got[2]) && got[1] > 0.0f;
  double gx = usable ? got[0] / got[1] : 0.0;
  double gz = usable ? got[2] / got[1] : 0.0;

  std::printf("      got X/Y %.6f Z/Y %.6f, want X/Y %.6f Z/Y %.6f\n",
              gx, gz, want.x / want.y, want.z / want.y);
  check(usable && close_to(gx, want.x / want.y) && close_to(gz, want.z / want.y), msg);
}

class TestConnectionConditions : public IIccProfileConnectionConditions
{
public:
  TestConnectionConditions()
  {
    const icSpectralRange range = make_range();
    std::vector<icFloatNumber> illum(kSteps, 1.0f);
    std::vector<icFloatNumber> observer(kSteps * 3, 0.0f);

    for (icUInt16Number i = 0; i < kSteps; ++i) {
      observer[i] = static_cast<icFloatNumber>(cmf_x(i));
      observer[kSteps + i] = static_cast<icFloatNumber>(cmf_y(i));
      observer[2 * kSteps + i] = static_cast<icFloatNumber>(cmf_z(i));
    }

    m_valid = m_view.setIlluminant(icIlluminantCustom, range, illum.data()) &&
              m_view.setObserver(icStdObsCustom, range, observer.data());
  }

  bool valid() const { return m_valid; }

  const CIccTagSpectralViewingConditions *getPccViewingConditions() override
  {
    return &m_view;
  }

  CIccTagMultiProcessElement *getCustomToStandardPcc() override { return NULL; }
  CIccTagMultiProcessElement *getStandardToCustomPcc() override { return NULL; }

  void getNormIlluminantXYZ(icFloatNumber *pXYZ) override
  {
    pXYZ[0] = 1.0f;
    pXYZ[1] = 1.0f;
    pXYZ[2] = 1.0f;
  }

  void getLumIlluminantXYZ(icFloatNumber *pXYZ) override
  {
    getNormIlluminantXYZ(pXYZ);
  }

  bool getMediaWhiteXYZ(icFloatNumber *pXYZ) override
  {
    getNormIlluminantXYZ(pXYZ);
    return true;
  }

private:
  CIccTagSpectralViewingConditions m_view;
  bool m_valid = false;
};

// Lets a test hand a PCC to an element's Begin() without building a profile.
class TestMpeTag : public CIccTagMultiProcessElement
{
public:
  explicit TestMpeTag(IIccProfileConnectionConditions *pcc)
  {
    m_pAppliedPCC = pcc;
  }
};

// Exposes the reflectance CLUT's protected m_flags, which is 32 bits wide.
struct TestReflectanceCLUT : public CIccMpeReflectanceCLUT {
  void setFlags(icUInt32Number flags) { m_flags = flags; }
};

// Exposes an observer's protected state. m_flags is 16 bits wide here.
// poisonScale() fills m_xyzscale with NaN, standing in for memory nobody has
// written: Begin() has to overwrite every entry Apply() goes on to read, so a
// NaN that reaches the output is an entry Begin() skipped. It runs after
// construction rather than by filling the storage before it, because GCC
// treats storage as indeterminate when a constructor starts and deletes
// stores made just before one (-flifetime-dse, on by default at -O2).
template <class Observer>
struct TestObserver : public Observer {
  void setFlags(icUInt16Number flags) { this->m_flags = flags; }

  void poisonScale()
  {
    for (int i = 0; i < 3; ++i)
      this->m_xyzscale[i] = std::numeric_limits<icFloatNumber>::quiet_NaN();
  }
};

// Builds a one-input reflectance CLUT whose two grid points both hold the white
// reflectance, runs Begin(), and returns the colorimetry Begin() computed for
// grid point 0. Begin() is where this element applies the flag: it converts the
// whole CLUT to colorimetry up front.
bool run_reflectance_clut(TestConnectionConditions &pcc, icUInt32Number flags,
                          icFloatNumber *xyz)
{
  CIccCLUT *source = new CIccCLUT(1, kSteps, 4);
  if (!source->Init(2)) {
    delete source;
    return false;
  }

  icFloatNumber *data = source->GetData(0);
  for (icUInt32Number n = 0; n < source->NumPoints(); ++n) {
    for (icUInt16Number i = 0; i < kSteps; ++i)
      data[n * kSteps + i] = static_cast<icFloatNumber>(white_at(i));
  }

  icFloatNumber *white = static_cast<icFloatNumber *>(std::calloc(kSteps, sizeof(icFloatNumber)));
  if (!white) {
    delete source;
    return false;
  }
  for (icUInt16Number i = 0; i < kSteps; ++i)
    white[i] = static_cast<icFloatNumber>(white_at(i));

  // SetData() takes ownership of both the CLUT and the white array.
  TestReflectanceCLUT clut;
  clut.SetData(source, icValueTypeFloat32, make_range(), white, 3);
  clut.setFlags(flags);

  TestMpeTag mpe(&pcc);
  if (!clut.Begin(icElemInterpLinear, &mpe) || !clut.GetApplyCLUT())
    return false;

  const icFloatNumber *out = clut.GetApplyCLUT()->GetData(0);
  xyz[0] = out[0];
  xyz[1] = out[1];
  xyz[2] = out[2];
  return true;
}

// Sizes an observer to the fixture's range and fills in its white.
bool prepare_observer(CIccMpeSpectralObserver *obs)
{
  if (!obs->SetSize(kSteps, 3, make_range()))
    return false;

  icFloatNumber *white = obs->GetWhite();
  for (icUInt16Number i = 0; i < kSteps; ++i)
    white[i] = static_cast<icFloatNumber>(white_at(i));
  return true;
}

// Attaches an observer to a tag, runs the tag's Begin() against the PCC, and
// applies the element to the white spectrum. The observers apply the flag per
// pixel, in the Apply() the two classes share. The tag owns the element from
// here on, and deletes it on return.
bool apply_observer(CIccMultiProcessElement *obs, TestConnectionConditions &pcc,
                    icFloatNumber *xyz)
{
  CIccTagMultiProcessElement mpe;
  mpe.SetChannels(kSteps, 3);
  mpe.Attach(obs);

  if (!mpe.Begin(icElemInterpLinear, NULL, &pcc))
    return false;

  std::vector<icFloatNumber> in(kSteps);
  for (icUInt16Number i = 0; i < kSteps; ++i)
    in[i] = static_cast<icFloatNumber>(white_at(i));

  obs->Apply(NULL, xyz, in.data());
  return true;
}

// A new observer with the given flags and a poisoned m_xyzscale.
template <class Observer>
bool run_observer(TestConnectionConditions &pcc, icUInt16Number flags, icFloatNumber *xyz)
{
  TestObserver<Observer> *obs = new TestObserver<Observer>();
  if (!prepare_observer(obs)) {
    delete obs;
    return false;
  }

  obs->setFlags(flags);
  obs->poisonScale();
  return apply_observer(obs, pcc, xyz);
}

// An observer built in code and never given flags must be relative, like one
// read with Flags=0. This is a plain new of the library class, so only its own
// constructor can set m_flags. Without that initializer the check fails unless
// the reused memory happens to have bits 0 and 1 clear; ASan's malloc fill
// (0xbe) sets the Lab bit.
bool run_default_observer(TestConnectionConditions &pcc, icFloatNumber *xyz)
{
  CIccMpeReflectanceObserver *obs = new CIccMpeReflectanceObserver;
  if (!prepare_observer(obs)) {
    delete obs;
    return false;
  }

  return apply_observer(obs, pcc, xyz);
}

// Copies an absolute reflectance observer and runs the copy. CIccProfile's copy
// constructor reaches the element's copy constructor through NewCopy(), which
// is how CIccCmm::AddXform(CIccProfile&) sees every element; operator= goes
// through copyData(). The assignment target starts out relative, so an
// operator= that drops the flag fails here on every build. A copy constructor
// that drops it leaves whatever the reused memory held, which fails under
// ASan's malloc fill and otherwise depends on the heap.
bool run_copied_observer(TestConnectionConditions &pcc, bool viaAssignment, icFloatNumber *xyz)
{
  TestObserver<CIccMpeReflectanceObserver> source;
  if (!prepare_observer(&source))
    return false;
  source.setFlags(icAbsoluteSpectralData);

  CIccMultiProcessElement *copy = NULL;
  if (viaAssignment) {
    TestObserver<CIccMpeReflectanceObserver> *target = new TestObserver<CIccMpeReflectanceObserver>();
    target->setFlags(icRelativeSpectralData);
    *target = source;
    copy = target;
  }
  else {
    copy = source.NewCopy();
  }

  return copy && apply_observer(copy, pcc, xyz);
}

} // namespace

int main()
{
  std::setbuf(stdout, NULL);

  TestConnectionConditions pcc;
  check(pcc.valid(), "custom 81-step PCC with a sloped observer is initialized");

  // With the flat illuminant, the illuminant white is the integral of the
  // observer alone, and the media white is the integral of the white spectrum.
  const Xyz illuminantWhite = scaled_to_unit_y(integrate(flat));
  const Xyz mediaWhite = integrate(white_at);

  // If the fixture ever lost its slope, relative and absolute would share a
  // chromaticity and the absolute checks below would pass on the unfixed build.
  check(std::fabs(illuminantWhite.x / illuminantWhite.y - mediaWhite.x / mediaWhite.y) > 0.05 &&
        std::fabs(illuminantWhite.z / illuminantWhite.y - mediaWhite.z / mediaWhite.y) > 0.05,
        "fixture: the media white and the illuminant white differ in chromaticity");

  icFloatNumber xyz[3] = {0.0f, 0.0f, 0.0f};

  check(run_reflectance_clut(pcc, icRelativeSpectralData, xyz),
        "reflectance CLUT, Flags=0: Begin() succeeds");
  check_xyz("reflectance CLUT, Flags=0: the media white is mapped to the illuminant white",
            xyz, illuminantWhite);

  check(run_reflectance_clut(pcc, icAbsoluteSpectralData, xyz),
        "reflectance CLUT, Flags=1: Begin() succeeds");
  check_chromaticity("reflectance CLUT, Flags=1: the media white keeps its own chromaticity",
                     xyz, mediaWhite);

  check(run_observer<CIccMpeReflectanceObserver>(pcc, icRelativeSpectralData, xyz),
        "reflectance observer, Flags=0: Begin() succeeds");
  check_xyz("reflectance observer, Flags=0: the media white is mapped to the illuminant white",
            xyz, illuminantWhite);

  check(run_observer<CIccMpeReflectanceObserver>(pcc, icAbsoluteSpectralData, xyz),
        "reflectance observer, Flags=1: Begin() succeeds");
  check_chromaticity("reflectance observer, Flags=1: the media white keeps its own chromaticity",
                     xyz, mediaWhite);

  check(run_observer<CIccMpeEmissionObserver>(pcc, icRelativeSpectralData, xyz),
        "emission observer, Flags=0: Begin() succeeds");
  check_xyz("emission observer, Flags=0: the white emission comes out at Y = 1 (Formula 26)",
            xyz, scaled_to_unit_y(integrate(white_at)));

  check(run_default_observer(pcc, xyz),
        "reflectance observer, flags never set: Begin() succeeds");
  check_xyz("reflectance observer, flags never set: relative, the same as Flags=0",
            xyz, illuminantWhite);

  check(run_copied_observer(pcc, false, xyz),
        "reflectance observer copied with NewCopy(): Begin() succeeds");
  check_chromaticity("reflectance observer copied with NewCopy(): the copy keeps Flags=1",
                     xyz, mediaWhite);

  check(run_copied_observer(pcc, true, xyz),
        "reflectance observer copied with operator=: Begin() succeeds");
  check_chromaticity("reflectance observer copied with operator=: the copy keeps Flags=1",
                     xyz, mediaWhite);

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nall checks passed\n");
  return 0;
}
