/*
    File:       owning-setter-self-alias-contract.cpp

    Contains:   CTest helper for the ownership contract of five exported
                setters (issue #2630).

    CIccTagSegmentedCurve::SetCurve, CIccMpeCLUT::SetCLUT, CIccMpeCAM::SetCAM,
    CIccMpeSpectralCLUT::SetData and CIccMBB::SetCLUT each released the object
    they held before storing the pointer they were handed, with no check that
    the two were the same.  Every one of them has a public getter for that
    member, so SetX(GetX()) freed the object and kept the dangling pointer.
    The two MPE CLUT cases then dereferenced the freed table inside the setter
    itself.  CIccMBB::SetCLUT is reachable without any self-alias in the source
    as well, because NewCLUT() returns the table the tag already holds.

    This helper deliberately drives the exported library API.  No serialized
    profile reaches these setters -- the readers construct a fresh object and
    hand it over once -- which is the same reachability issue #2607 had.

    Two detection styles are used, and the difference matters:

      * Cases that own a polymorphic type (CIccSegmentedCurve, CIccCLUT) count
        destructor calls through a subclass.  These FAIL deterministically on
        an unfixed build, on any platform, with no sanitizer.

      * The CAM and white-point cases cannot: CIccCamConverter has a
        non-virtual destructor and the white point is a malloc'd buffer.  They
        reinstall the member and then release it through the API, so an
        unfixed build performs a genuine double free and ABORTS before
        reaching the exit below.  Their red is an abort, not a FAIL line.

    Every "must not free" case is paired with a control that installs a
    DIFFERENT object and requires the old one to be freed, so a change that
    simply stopped releasing anything cannot satisfy this file.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccCAM.h"
#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
static int g_curveDtors = 0;
static int g_clutDtors = 0;

static void check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "owning-setter-self-alias-contract: PASS  %s\n", label);
    return;
  }

  std::fprintf(stderr, "owning-setter-self-alias-contract: FAIL  %s\n", label);
  g_failures++;
}

class CountingSegmentedCurve : public CIccSegmentedCurve
{
public:
  virtual ~CountingSegmentedCurve() { g_curveDtors++; }
};

class CountingCLUT : public CIccCLUT
{
public:
  CountingCLUT() : CIccCLUT(1, 3) { Init((icUInt8Number)2); }
  virtual ~CountingCLUT() { g_clutDtors++; }
};

static icSpectralRange makeRange()
{
  icSpectralRange range;
  range.start = icFtoF16(0.4f);
  range.end = icFtoF16(0.7f);
  range.steps = 4;
  return range;
}

static icFloatNumber *makeWhite()
{
  return (icFloatNumber *)calloc(4, sizeof(icFloatNumber));
}

static void testSegmentedCurveTag()
{
  {
    CIccTagSegmentedCurve tag;
    CountingSegmentedCurve *curve = new CountingSegmentedCurve;
    tag.SetCurve(curve);

    g_curveDtors = 0;
    tag.SetCurve(tag.GetCurve());
    check(g_curveDtors == 0, "SetCurve(GetCurve()) does not free the curve");
    check(tag.GetCurve() == curve, "SetCurve(GetCurve()) keeps the curve installed");
  }
  check(g_curveDtors == 1, "the tag frees its curve exactly once");

  // Control: a different curve must still be released.
  {
    CIccTagSegmentedCurve tag;
    tag.SetCurve(new CountingSegmentedCurve);

    g_curveDtors = 0;
    tag.SetCurve(new CIccSegmentedCurve);
    check(g_curveDtors == 1, "SetCurve with a new curve frees the old one");
  }
}

static void testMpeCLUT()
{
  {
    CIccMpeCLUT elem;
    CountingCLUT *clut = new CountingCLUT;
    elem.SetCLUT(clut);

    g_clutDtors = 0;
    elem.SetCLUT(elem.GetCLUT());
    check(g_clutDtors == 0, "SetCLUT(GetCLUT()) does not free the table");
    check(elem.GetCLUT() == clut, "SetCLUT(GetCLUT()) keeps the table installed");
    // Not a discriminator: an unfixed build reads these back off the freed
    // table and usually still answers correctly.  It is here to pin that the
    // early return leaves the element self-consistent.
    check(elem.NumInputChannels() == 1 && elem.NumOutputChannels() == 3,
          "SetCLUT(GetCLUT()) leaves the channel counts describing the table");
  }
  check(g_clutDtors == 1, "the element frees its table exactly once");

  // Control: a different table must still be released.
  {
    CIccMpeCLUT elem;
    elem.SetCLUT(new CountingCLUT);

    g_clutDtors = 0;
    elem.SetCLUT(new CountingCLUT);
    check(g_clutDtors == 1, "SetCLUT with a new table frees the old one");
    g_clutDtors = 0;
  }
}

static void testSpectralCLUT()
{
  icSpectralRange range = makeRange();

  {
    CIccMpeEmissionCLUT elem;
    CountingCLUT *clut = new CountingCLUT;
    elem.SetData(clut, 0, range, makeWhite(), 3);

    g_clutDtors = 0;
    elem.SetData(elem.GetCLUT(), 0, range, makeWhite(), 3);
    check(g_clutDtors == 0, "SetData(GetCLUT(), ...) does not free the table");
    check(elem.GetCLUT() == clut, "SetData(GetCLUT(), ...) keeps the table installed");
  }
  check(g_clutDtors == 1, "the spectral element frees its table exactly once");

  // Control: a different table must still be released.
  {
    CIccMpeEmissionCLUT elem;
    elem.SetData(new CountingCLUT, 0, range, makeWhite(), 3);

    g_clutDtors = 0;
    elem.SetData(new CountingCLUT, 0, range, makeWhite(), 3);
    check(g_clutDtors == 1, "SetData with a new table frees the old one");
    g_clutDtors = 0;
  }

  // The white point is malloc'd, so it cannot be counted.  On an unfixed
  // build the reinstall frees it and the destructor frees it again: this
  // block ABORTS rather than reporting FAIL.
  {
    CIccMpeEmissionCLUT elem;
    icFloatNumber *white = makeWhite();
    elem.SetData(new CountingCLUT, 0, range, white, 3);
    elem.SetData(new CountingCLUT, 0, range, elem.GetWhite(), 3);
    check(elem.GetWhite() == white, "SetData(..., GetWhite(), ...) keeps the white point installed");
    g_clutDtors = 0;
  }
}

static void testMBBTag()
{
  // CIccMBB::SetCLUT() is the same shape, and its dimension check cannot catch
  // the alias because a table always matches its own tag.  It is reached two
  // ways: SetCLUT(GetCLUT()), and -- with no self-alias visible in the source
  // -- SetCLUT(NewCLUT(grid)), because NewCLUT() hands back the installed
  // table when the tag already has one.
  {
    CIccTagLutAtoB tag;
    tag.Init(1, 3);
    CountingCLUT *clut = new CountingCLUT;
    tag.SetCLUT(clut);

    g_clutDtors = 0;
    check(tag.SetCLUT(tag.GetCLUT()) == clut, "MBB SetCLUT(GetCLUT()) returns the table");
    check(g_clutDtors == 0, "MBB SetCLUT(GetCLUT()) does not free the table");
  }
  check(g_clutDtors == 1, "the MBB tag frees its table exactly once");

  {
    CIccTagLutAtoB tag;
    tag.Init(1, 3);
    tag.SetCLUT(new CountingCLUT);

    icUInt8Number grid[2] = { 2, 2 };
    CIccCLUT *reused = tag.NewCLUT(grid);
    check(reused == tag.GetCLUT(), "NewCLUT() hands back the installed table");

    g_clutDtors = 0;
    tag.SetCLUT(reused);
    check(g_clutDtors == 0, "MBB SetCLUT(NewCLUT(grid)) does not free the table");
    g_clutDtors = 0;
  }

  // Control: a different, dimension-compatible table must still be released.
  {
    CIccTagLutAtoB tag;
    tag.Init(1, 3);
    tag.SetCLUT(new CountingCLUT);

    g_clutDtors = 0;
    tag.SetCLUT(new CountingCLUT);
    check(g_clutDtors == 1, "MBB SetCLUT with a new table frees the old one");
    g_clutDtors = 0;
  }

  // Control: a mismatched table is still rejected and deleted, not installed.
  {
    CIccTagLutAtoB tag;
    tag.Init(1, 3);
    CountingCLUT *kept = new CountingCLUT;
    tag.SetCLUT(kept);

    g_clutDtors = 0;
    CIccCLUT *wrong = new CIccCLUT(2, 3);
    wrong->Init((icUInt8Number)2);
    check(tag.SetCLUT(wrong) == NULL, "MBB SetCLUT rejects a mismatched table");
    check(tag.GetCLUT() == kept, "a rejected table leaves the installed one alone");
    check(g_clutDtors == 0, "rejecting a mismatched table does not free the installed one");
    g_clutDtors = 0;
  }
}

static void testMpeCAM()
{
  // CIccCamConverter has a non-virtual destructor, so this case cannot count
  // frees either.  It reinstalls the converter and then releases it through
  // the API; an unfixed build double frees and ABORTS here.
  CIccMpeXYZToJab elem;
  CIccCamConverter *cam = new CIccCamConverter;
  elem.SetCAM(cam);

  elem.SetCAM(elem.GetCAM());
  check(elem.GetCAM() == cam, "SetCAM(GetCAM()) keeps the converter installed");

  elem.SetCAM(NULL);
  check(elem.GetCAM() == NULL, "SetCAM(NULL) releases the converter");
}

int main()
{
  // An unfixed build crashes partway through, so keep stdout unbuffered:
  // otherwise the PASS trail that says which case was reached is lost.
  setvbuf(stdout, NULL, _IONBF, 0);

  testSegmentedCurveTag();
  testMpeCLUT();
  testSpectralCLUT();
  testMBBTag();
  testMpeCAM();

  return g_failures ? 1 : 0;
}
