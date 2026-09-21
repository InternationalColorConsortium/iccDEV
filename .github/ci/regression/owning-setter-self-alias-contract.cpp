/*
    File:       owning-setter-self-alias-contract.cpp

    Contains:   CTest helper for the ownership contract of five exported
                setters (issue #2630), the three CIccMpeSpectral* copyData()
                implementations (issue #2637) and the apply table that
                CIccMpeSpectralCLUT::SetData() releases (issue #2638).

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

    #2637 is the same ownership question reached through operator= instead of a
    setter.  copyData() is the entire body of nine operator= overloads in
    IccMpeSpectral.h and releases its buffers before copying out of the source,
    which on a self-assignment is this same object.  std::sort and any dedup
    loop assign an element to itself.  The three implementations fail in two
    different ways, and the difference decides how they are checked here:

      * CIccMpeSpectralCLUT::copyData releases FIRST and reads afterwards, so
        it copy-constructs out of the table it just freed.  Detected by
        counting destructor calls, which is deterministic everywhere.

      * CIccMpeSpectralMatrix::copyData and CIccMpeSpectralObserver::copyData
        are NOT use-after-free.  malloc() overwrites the member before the
        memcpy reads the source pointer, so the memcpy copies the new block
        onto itself and the element keeps its shape while silently losing its
        CONTENTS.  These are checked by value, from index 0.  Measured on
        glibc, they go red without a sanitizer too: the allocator hands the
        same block straight back, but free() has written its tcache
        bookkeeping over the first 16 bytes, so the leading four floats return
        as metadata while the rest survive.  That is an allocator detail and
        not a guarantee -- an allocator that leaves a freed block untouched
        would let the contents survive intact and these cases would pass.  The
        dependable red for the two of them is the ASAN+UBSAN lanes, where the
        fresh block arrives poisoned.

    #2638 is a sibling member rather than a self-alias: SetData() stores the
    table it is handed and then unconditionally releases m_pApplyCLUT, so
    SetData(GetApplyCLUT(), ...) frees the object it has just installed as
    m_pCLUT.  m_pApplyCLUT is only non-NULL after a Begin() that found an
    applied PCC, and it is covered here twice:

      * Through the real path.  CIccTagMultiProcessElement::Begin() takes the
        applied PCC as an argument, so reaching it needs spectral viewing
        conditions but no CIccProfile at all.  On an unfixed build the read
        after the call is a heap-use-after-free and the destructor then double
        frees, so this case ABORTS rather than reporting FAIL.

      * Through a subclass that installs the member directly.  That one can
        count destructor calls, so it FAILs deterministically on a platform
        with no sanitizer, where the case above would only abort if the
        allocator happens to notice.  It runs first, so that its FAIL line is
        in the log before the other one takes the process down.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccCAM.h"
#include "IccMpeBasic.h"
#include "IccMpeCalc.h"
#include "IccMatrixMath.h"
#include "IccMpeSpectral.h"
#include "IccPcc.h"
#include "IccTagBasic.h"
#include "IccTagDict.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
static int g_curveDtors = 0;
static int g_clutDtors = 0;
static int g_matrixDtors = 0;
static int g_mluDtors = 0;
static int g_numArrayDtors = 0;
static int g_calcDtors = 0;
static int g_calcFuncDtors = 0;

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

// Assign through a reference rather than writing "elem = elem".  This is the
// call a container makes when it assigns an element to itself, and it keeps
// the file clear of -Wself-assign-overloaded, which would otherwise be an
// error on the -Werror lanes.
template <typename T>
static void assignThroughRef(T &dst, const T &src)
{
  dst = src;
}

static void fillRamp(icFloatNumber *p, int count, int base)
{
  for (int i = 0; i < count; i++)
    p[i] = (icFloatNumber)(base + i);
}

static bool isRamp(const icFloatNumber *p, int count, int base)
{
  for (int i = 0; i < count; i++) {
    if (p[i] != (icFloatNumber)(base + i))
      return false;
  }
  return true;
}

// testSpectralApplyCLUTAfterBegin() below reaches m_pApplyCLUT the way a real
// caller does.  This subclass installs it directly instead, which is what lets
// the paired case count destructor calls and so fail on a platform with no
// sanitizer.
class ProbeEmissionCLUT : public CIccMpeEmissionCLUT
{
public:
  void InstallApplyCLUT(CIccCLUT *pCLUT) { m_pApplyCLUT = pCLUT; }
};

// CIccMatrixMath has a virtual destructor, so the matrix and observer apply
// matrices can be counted the same way the CLUTs are.
class CountingMatrixMath : public CIccMatrixMath
{
public:
  CountingMatrixMath() : CIccMatrixMath(3, 3) {}
  virtual ~CountingMatrixMath() { g_matrixDtors++; }
};

class ProbeEmissionMatrix : public CIccMpeEmissionMatrix
{
public:
  void InstallApplyMtx(CIccMatrixMath *pMtx) { m_pApplyMtx = pMtx; }
};

class ProbeEmissionObserver : public CIccMpeEmissionObserver
{
public:
  void InstallApplyMtx(CIccMatrixMath *pMtx) { m_pApplyMtx = pMtx; }
};

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

// The minimum an applied PCC has to supply for CIccMpeSpectralCLUT::Begin() to
// build an apply table: spectral viewing conditions carrying an observer that
// maps onto the element's range.  CIccTagMultiProcessElement::Begin() takes the
// PCC as an argument, so no CIccProfile is involved.
class ContractPCC : public IIccProfileConnectionConditions
{
public:
  CIccTagSpectralViewingConditions svc;

  virtual const CIccTagSpectralViewingConditions *getPccViewingConditions() { return &svc; }
  virtual CIccTagMultiProcessElement *getCustomToStandardPcc() { return NULL; }
  virtual CIccTagMultiProcessElement *getStandardToCustomPcc() { return NULL; }
  virtual void getNormIlluminantXYZ(icFloatNumber *pXYZ)
  {
    pXYZ[0] = 0.9642f;
    pXYZ[1] = 1.0f;
    pXYZ[2] = 0.8249f;
  }
  virtual void getLumIlluminantXYZ(icFloatNumber *pXYZ) { getNormIlluminantXYZ(pXYZ); }
  virtual bool getMediaWhiteXYZ(icFloatNumber *pXYZ)
  {
    getNormIlluminantXYZ(pXYZ);
    return true;
  }
};

// A spectral CLUT's output channel count is the number of spectral steps, not
// the element's three output channels -- Begin() refuses anything else.
static CIccCLUT *makeSpectralCLUT(const icSpectralRange &range)
{
  CIccCLUT *clut = new CIccCLUT(1, (icUInt16Number)range.steps);
  clut->Init((icUInt8Number)2);

  icFloatNumber *data = clut->GetData(0);
  if (data) {
    for (icUInt32Number i = 0; i < clut->NumPoints() * range.steps; i++)
      data[i] = 0.5f;
  }
  return clut;
}

static icFloatNumber *makeSpectralWhite(const icSpectralRange &range)
{
  icFloatNumber *p = (icFloatNumber *)calloc(range.steps, sizeof(icFloatNumber));
  for (int i = 0; i < range.steps; i++)
    p[i] = 1.0f;
  return p;
}

// #2638 through the path a caller really takes: Begin() builds the apply table,
// GetApplyCLUT() hands it out, SetData() installs it and then frees it.
static void testSpectralApplyCLUTAfterBegin()
{
  icSpectralRange range = makeRange();

  ContractPCC pcc;
  // Three colour matching functions over the element's range.  The values only
  // have to be non-degenerate; Begin() divides by their accumulated weight.
  icFloatNumber observer[12] = {
    0.1f, 0.3f, 0.5f, 0.1f,
    0.2f, 0.6f, 0.9f, 0.3f,
    0.7f, 0.4f, 0.1f, 0.0f,
  };
  check(pcc.svc.setObserver(icStdObs1931TwoDegrees, range, observer),
        "the viewing conditions accept an observer over the element range");
  pcc.svc.m_illuminantXYZ.X = icDtoF(0.9642f);
  pcc.svc.m_illuminantXYZ.Y = icDtoF(1.0f);
  pcc.svc.m_illuminantXYZ.Z = icDtoF(0.8249f);

  CIccTagMultiProcessElement mpe(1, 3);
  CIccMpeEmissionCLUT *elem = new CIccMpeEmissionCLUT;
  elem->SetData(makeSpectralCLUT(range), 0, range, makeSpectralWhite(range), 3);
  mpe.Attach(elem);

  check(mpe.Begin(icElemInterpLinear, &pcc, &pcc),
        "the MPE begins against an applied PCC");
  check(elem->GetApplyCLUT() != NULL,
        "Begin() leaves a non-NULL apply table reachable through GetApplyCLUT()");

  if (!elem->GetApplyCLUT())
    return;

  CIccCLUT *apply = elem->GetApplyCLUT();
  elem->SetData(elem->GetApplyCLUT(), 0, range, makeSpectralWhite(range), 3);
  check(elem->GetCLUT() == apply, "SetData(GetApplyCLUT(), ...) installs the apply table");

  // On an unfixed build the table this reads was freed inside the call above,
  // and the MPE destructor frees it a second time on the way out of this
  // function.  Neither shows up as FAIL: the red here is a sanitizer report or
  // an allocator abort.
  check(elem->GetCLUT() != NULL && elem->GetCLUT()->GetInputDim() == 1,
        "the installed apply table is still usable after the call");
}

// #2638: SetData() installs pCLUT and then releases the apply table.  Handed
// GetApplyCLUT(), it frees the object it has just installed as m_pCLUT.  This
// is the countable form of the case above -- see the header comment.
static void testSpectralApplyCLUT()
{
  icSpectralRange range = makeRange();

  {
    ProbeEmissionCLUT elem;
    elem.SetData(new CountingCLUT, 0, range, makeWhite(), 3);
    CountingCLUT *apply = new CountingCLUT;
    elem.InstallApplyCLUT(apply);

    g_clutDtors = 0;
    elem.SetData(elem.GetApplyCLUT(), 0, range, makeWhite(), 3);
    // The source table this call replaces must still be freed -- one
    // destructor, not two.  An unfixed build also frees the apply table it
    // just installed, so it counts 2 here and double frees at scope exit.
    check(g_clutDtors == 1, "SetData(GetApplyCLUT(), ...) frees only the table it replaces");
    check(elem.GetCLUT() == apply, "SetData(GetApplyCLUT(), ...) installs the apply table");
    check(elem.GetApplyCLUT() == NULL, "SetData(GetApplyCLUT(), ...) clears the apply table");
    g_clutDtors = 0;
  }
  check(g_clutDtors == 1, "the element frees the promoted apply table exactly once");

  // Control: an apply table the caller did NOT hand back must still be freed.
  {
    ProbeEmissionCLUT elem;
    elem.SetData(new CountingCLUT, 0, range, makeWhite(), 3);
    elem.InstallApplyCLUT(new CountingCLUT);

    g_clutDtors = 0;
    elem.SetData(new CountingCLUT, 0, range, makeWhite(), 3);
    check(g_clutDtors == 2, "SetData with a new table frees both the old table and the apply table");
    check(elem.GetApplyCLUT() == NULL, "SetData clears the apply table");
    g_clutDtors = 0;
  }
}

// #2637, the case that is a genuine use-after-free.
static void testSpectralCLUTSelfAssign()
{
  icSpectralRange range = makeRange();

  {
    CIccMpeEmissionCLUT elem;
    CountingCLUT *clut = new CountingCLUT;
    icFloatNumber *white = makeWhite();
    elem.SetData(clut, 0, range, white, 3);

    g_clutDtors = 0;
    assignThroughRef(elem, elem);
    check(g_clutDtors == 0, "self-assignment does not free the table");
    // An unfixed build reaches here only without a sanitizer, and then this
    // fails too: it copy-constructs a plain CIccCLUT out of the freed table,
    // so the element no longer holds the object it was given.
    check(elem.GetCLUT() == clut, "self-assignment keeps the table installed");
    check(elem.GetWhite() == white, "self-assignment keeps the white point installed");
    g_clutDtors = 0;
  }
  check(g_clutDtors == 1, "the self-assigned element frees its table exactly once");

  // Control: assigning a DIFFERENT element must still release the old table
  // and deep-copy the source, so an early return for every argument fails.
  {
    CIccMpeEmissionCLUT dst;
    CIccMpeEmissionCLUT src;
    dst.SetData(new CountingCLUT, 0, range, makeWhite(), 3);
    src.SetData(new CountingCLUT, 0, range, makeWhite(), 3);

    g_clutDtors = 0;
    assignThroughRef(dst, src);
    check(g_clutDtors == 1, "assigning a different element frees the old table");
    check(dst.GetCLUT() != NULL && dst.GetCLUT() != src.GetCLUT(),
          "assigning a different element deep-copies its table");
    g_clutDtors = 0;
  }
  g_clutDtors = 0;
}

// #2637, the two cases that lose their contents instead.  Checked by value,
// and red only under a sanitizer -- see the header comment.
static void testSpectralMatrixSelfAssign()
{
  icSpectralRange range = makeRange();

  {
    CIccMpeEmissionMatrix elem;
    check(elem.SetSize(3, 3, range), "the matrix element sizes its buffers");

    // numVectors() is the input channel count for an emission matrix, so
    // m_size is 3 * range.steps.
    icFloatNumber *matrix = elem.GetMatrix();
    icFloatNumber *white = elem.GetWhite();
    icFloatNumber *offset = elem.GetOffset();
    fillRamp(matrix, 12, 100);
    fillRamp(white, 4, 200);
    fillRamp(offset, 4, 300);

    assignThroughRef(elem, elem);

    check(elem.GetMatrix() != NULL && isRamp(elem.GetMatrix(), 12, 100),
          "self-assignment keeps the matrix contents");
    check(elem.GetWhite() != NULL && isRamp(elem.GetWhite(), 4, 200),
          "self-assignment keeps the matrix white point");
    check(elem.GetOffset() != NULL && isRamp(elem.GetOffset(), 4, 300),
          "self-assignment keeps the matrix offset");
  }

  // Control: a different source must still be copied in.
  {
    CIccMpeEmissionMatrix dst;
    CIccMpeEmissionMatrix src;
    check(dst.SetSize(3, 3, range) && src.SetSize(3, 3, range),
          "both matrix elements size their buffers");
    fillRamp(dst.GetMatrix(), 12, 100);
    fillRamp(src.GetMatrix(), 12, 500);

    assignThroughRef(dst, src);
    check(dst.GetMatrix() != NULL && isRamp(dst.GetMatrix(), 12, 500),
          "assigning a different matrix element copies its contents");
    check(dst.GetMatrix() != src.GetMatrix(),
          "assigning a different matrix element copies rather than aliases");
  }
}

static void testSpectralObserverSelfAssign()
{
  icSpectralRange range = makeRange();

  {
    CIccMpeEmissionObserver elem;
    check(elem.SetSize(4, 3, range), "the observer element sizes its buffer");
    fillRamp(elem.GetWhite(), 4, 400);

    assignThroughRef(elem, elem);
    check(elem.GetWhite() != NULL && isRamp(elem.GetWhite(), 4, 400),
          "self-assignment keeps the observer white point");
  }

  // Control: a different source must still be copied in.
  {
    CIccMpeEmissionObserver dst;
    CIccMpeEmissionObserver src;
    check(dst.SetSize(4, 3, range) && src.SetSize(4, 3, range),
          "both observer elements size their buffers");
    fillRamp(dst.GetWhite(), 4, 400);
    fillRamp(src.GetWhite(), 4, 600);

    assignThroughRef(dst, src);
    check(dst.GetWhite() != NULL && isRamp(dst.GetWhite(), 4, 600),
          "assigning a different observer element copies its white point");
    check(dst.GetWhite() != src.GetWhite(),
          "assigning a different observer element copies rather than aliases");
  }
}

// The apply matrix Begin() builds belongs to the element.  copyData() used to
// drop the pointer without releasing it, so assigning onto a Begin()-ed element
// orphaned it.  Adjacent to #2637 rather than part of it: found by the review of
// this change, and measured at 60 bytes under LSan.  Counted here instead, so it
// reds without a sanitizer.
static void testSpectralApplyMtx()
{
  icSpectralRange range = makeRange();

  // A different source must release the apply matrix.
  {
    ProbeEmissionMatrix dst;
    CIccMpeEmissionMatrix src;
    check(dst.SetSize(3, 3, range) && src.SetSize(3, 3, range),
          "both matrix elements size their buffers");
    dst.InstallApplyMtx(new CountingMatrixMath);

    g_matrixDtors = 0;
    assignThroughRef<CIccMpeEmissionMatrix>(dst, src);
    check(g_matrixDtors == 1, "assigning a different matrix element frees the apply matrix");
  }

  // Control: self-assignment returns before the reset, so it must keep it.
  {
    ProbeEmissionMatrix elem;
    check(elem.SetSize(3, 3, range), "the matrix element sizes its buffers");
    elem.InstallApplyMtx(new CountingMatrixMath);

    g_matrixDtors = 0;
    assignThroughRef<CIccMpeEmissionMatrix>(elem, elem);
    check(g_matrixDtors == 0, "self-assignment keeps the matrix apply matrix");
    g_matrixDtors = 0;
  }
  check(g_matrixDtors == 1, "the matrix element frees its apply matrix exactly once");

  // The observer carries the same member.
  {
    ProbeEmissionObserver dst;
    CIccMpeEmissionObserver src;
    check(dst.SetSize(4, 3, range) && src.SetSize(4, 3, range),
          "both observer elements size their buffers");
    dst.InstallApplyMtx(new CountingMatrixMath);

    g_matrixDtors = 0;
    assignThroughRef<CIccMpeEmissionObserver>(dst, src);
    check(g_matrixDtors == 1, "assigning a different observer element frees the apply matrix");
  }

  {
    ProbeEmissionObserver elem;
    check(elem.SetSize(4, 3, range), "the observer element sizes its buffer");
    elem.InstallApplyMtx(new CountingMatrixMath);

    g_matrixDtors = 0;
    assignThroughRef<CIccMpeEmissionObserver>(elem, elem);
    check(g_matrixDtors == 0, "self-assignment keeps the observer apply matrix");
    g_matrixDtors = 0;
  }
  check(g_matrixDtors == 1, "the observer element frees its apply matrix exactly once");
}

// #2634: the same ownership shape in three more exported setters, each with a
// public getter, each reproduced under ASan as a use-after-free WRITE through
// SetParentObject() on the freed object.  CIccDictEntry's two return whether a
// value was held, so the guard has to preserve that for the alias case as well.
class CountingMLU : public CIccTagMultiLocalizedUnicode
{
public:
  virtual ~CountingMLU() { g_mluDtors++; }
};

class CountingNumArray : public CIccTagFloat32
{
public:
  CountingNumArray() : CIccTagFloat32(4) {}
  virtual ~CountingNumArray() { g_numArrayDtors++; }

  // Load-bearing on Windows, and only there.  CIccTagFloat32 is a class
  // template instantiation, and subclassing it puts the base's out-of-line
  // virtuals in this subclass's vtable.  All three Windows legs -- MSVC,
  // ClangCL and MinGW UCRT64 -- then fail to link with exactly one undefined
  // reference:
  //
  //   CIccTagFloatNum<float, (icTagTypeSignature)1718367026>::GetClassName()
  //
  // Nine other inherited out-of-line virtuals of the same instantiation are
  // referenced from here and all of them resolve, and the definition of this
  // one (IccTagBasic.cpp:7186) is structurally no different from theirs, so
  // what singles it out is not established.  Overriding it keeps the vtable
  // slot inside this file, which is what actually fixes the link; Linux and
  // macOS never needed it.
  virtual const icChar *GetClassName() const { return "CountingNumArray"; }
};

static void testTintArraySetArray()
{
  {
    CIccMpeTintArray elem;
    CountingNumArray *array = new CountingNumArray;
    elem.SetArray(array);

    g_numArrayDtors = 0;
    elem.SetArray(elem.GetArray());
    check(g_numArrayDtors == 0, "SetArray(GetArray()) does not free the array");
    check(elem.GetArray() == array, "SetArray(GetArray()) keeps the array installed");
    g_numArrayDtors = 0;
  }
  check(g_numArrayDtors == 1, "the tint array element frees its array exactly once");

  // Control: a different array must still be released.
  {
    CIccMpeTintArray elem;
    elem.SetArray(new CountingNumArray);

    g_numArrayDtors = 0;
    elem.SetArray(new CountingNumArray);
    check(g_numArrayDtors == 1, "SetArray with a new array frees the old one");
    g_numArrayDtors = 0;
  }
}

// Adjacent to #2634, found reviewing it: CIccMpeTintArray::operator= deletes
// m_Array but only reassigns it inside `if (tintArray.m_Array)`, so a source
// with no array leaves the member dangling and the destructor writes through it.
// The copy constructor has the same missing else, which leaves the member
// indeterminate rather than dangling -- undefined, but it did not reproduce.
static void testTintArrayCopyFromEmpty()
{
  {
    CIccMpeTintArray dst;
    CIccMpeTintArray src;
    dst.SetArray(new CountingNumArray);
    check(src.GetArray() == NULL, "the source element has no array");

    g_numArrayDtors = 0;
    assignThroughRef(dst, src);
    check(g_numArrayDtors == 1, "assigning an empty element releases the old array");
    check(dst.GetArray() == NULL,
          "assigning an empty element clears the array instead of leaving it dangling");
    g_numArrayDtors = 0;
  }
  check(g_numArrayDtors == 0, "nothing is released a second time");

  // NOT a discriminator, and deliberately kept anyway.  The copy constructor's
  // missing else leaves m_Array indeterminate rather than dangling, and an
  // indeterminate pointer reads as NULL often enough that this assertion passes
  // on an unfixed build too -- verified by mutating the else branch out, which
  // leaves the whole suite green.  It documents the contract; the fix for it
  // rests on the standard, not on this line going red.
  {
    CIccMpeTintArray src;
    CIccMpeTintArray copy(src);
    check(copy.GetArray() == NULL, "copy-constructing an empty element leaves no array");
  }

  // Control: a non-empty source must still be deep-copied, not aliased.
  {
    CIccMpeTintArray dst;
    CIccMpeTintArray src;
    src.SetArray(new CountingNumArray);

    assignThroughRef(dst, src);
    check(dst.GetArray() != NULL, "assigning a non-empty element installs an array");
    check(dst.GetArray() != src.GetArray(),
          "assigning a non-empty element deep-copies rather than aliases");
    g_numArrayDtors = 0;
  }
}

static void testDictEntryLocalized()
{
  {
    CIccDictEntry entry;
    CountingMLU *name = new CountingMLU;
    entry.SetNameLocalized(name);

    g_mluDtors = 0;
    check(entry.SetNameLocalized(entry.GetNameLocalized()),
          "SetNameLocalized(GetNameLocalized()) still reports a value was held");
    check(g_mluDtors == 0, "SetNameLocalized(GetNameLocalized()) does not free it");
    check(entry.GetNameLocalized() == name,
          "SetNameLocalized(GetNameLocalized()) keeps it installed");
    g_mluDtors = 0;
  }
  check(g_mluDtors == 1, "the entry frees its localized name exactly once");

  {
    CIccDictEntry entry;
    CountingMLU *value = new CountingMLU;
    entry.SetValueLocalized(value);

    g_mluDtors = 0;
    check(entry.SetValueLocalized(entry.GetValueLocalized()),
          "SetValueLocalized(GetValueLocalized()) still reports a value was held");
    check(g_mluDtors == 0, "SetValueLocalized(GetValueLocalized()) does not free it");
    check(entry.GetValueLocalized() == value,
          "SetValueLocalized(GetValueLocalized()) keeps it installed");
    g_mluDtors = 0;
  }
  check(g_mluDtors == 1, "the entry frees its localized value exactly once");

  // Control: a different object must still be released, and the result must
  // still distinguish "replaced something" from "there was nothing".
  {
    CIccDictEntry entry;
    entry.SetNameLocalized(new CountingMLU);

    g_mluDtors = 0;
    check(entry.SetNameLocalized(new CountingMLU),
          "SetNameLocalized with a new value reports the old one was replaced");
    check(g_mluDtors == 1, "SetNameLocalized with a new value frees the old one");
    g_mluDtors = 0;
  }

  // Control: the NULL alias must report that nothing was held, as before.
  {
    CIccDictEntry entry;
    check(!entry.SetNameLocalized(NULL),
          "SetNameLocalized(NULL) on an empty entry reports nothing was held");
    check(!entry.SetValueLocalized(NULL),
          "SetValueLocalized(NULL) on an empty entry reports nothing was held");
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

// The two #2645 setters differ from every case above: their members are
// protected and neither has a public getter, so SetX(GetX()) cannot be written
// from outside the class.  A DERIVED class reaches them, which is not a
// contrivance here -- CIccSampledCalculatorCurveXml, CIccSampledCalculatorCurveJson,
// CIccMpeXmlCalculator and CIccMpeJsonCalculator all derive from these two, and
// CIccSampledCalculatorCurveXml already assigns m_pCalc directly instead of
// going through the setter (IccMpeXml.cpp).  These probes reach the member the
// same way a front end would.
class ProbeCalculator : public CIccMpeCalculator
{
public:
  virtual ~ProbeCalculator() { g_calcDtors++; }
  icCalculatorFuncPtr HeldFunc() const { return m_calcFunc; }
};

class ProbeSampledCalculatorCurve : public CIccSampledCalculatorCurve
{
public:
  CIccMpeCalculator *HeldCalculator() const { return m_pCalc; }
};

class CountingCalcFunc : public CIccCalculatorFunc
{
public:
  CountingCalcFunc(CIccMpeCalculator *pCalc) : CIccCalculatorFunc(pCalc) {}
  virtual ~CountingCalcFunc() { g_calcFuncDtors++; }
};

static void testSampledCalculatorCurveSetCalculator()
{
  // Unfixed, this releases the calculator and then writes through it in
  // SetParentObject() -- a use-after-free WRITE, and a second free in the
  // destructor.  The destructor count reds it without a sanitizer.
  ProbeSampledCalculatorCurve curve;
  ProbeCalculator *kept = new ProbeCalculator;

  check(curve.SetCalculator(kept), "SetCalculator installs the calculator");
  check(curve.HeldCalculator() == kept, "the calculator is installed");

  g_calcDtors = 0;
  check(curve.SetCalculator(curve.HeldCalculator()),
        "SetCalculator(m_pCalc) reports success");
  check(g_calcDtors == 0,
        "SetCalculator(m_pCalc) does not free the calculator it is handed");
  check(curve.HeldCalculator() == kept,
        "SetCalculator(m_pCalc) keeps the calculator installed");
  check(kept->GetParentObject() == static_cast<const IIccObject *>(&curve),
        "SetCalculator(m_pCalc) keeps the parent link");

  // Control: a DIFFERENT calculator must still be released, so a change that
  // simply stopped releasing anything cannot satisfy this case.
  g_calcDtors = 0;
  ProbeCalculator *replacement = new ProbeCalculator;
  check(curve.SetCalculator(replacement), "SetCalculator takes a new calculator");
  check(g_calcDtors == 1, "SetCalculator releases the calculator it replaces");
  check(curve.HeldCalculator() == replacement, "the replacement is installed");
  g_calcDtors = 0;
}

static void testCalculatorSetCalcFunc()
{
  // Unfixed, this is the narrower dangling-store shape: the function is freed
  // and the freed pointer stored back, with no dereference inside the setter.
  // It surfaces as a double free at destruction and in every Begin(), Apply(),
  // Describe() and Validate() that follows.
  ProbeCalculator calc;
  CountingCalcFunc *kept = new CountingCalcFunc(&calc);

  check(calc.SetCalcFunc(kept) == icFuncParseNoError,
        "SetCalcFunc installs the function");
  check(calc.HeldFunc() == kept, "the function is installed");

  g_calcFuncDtors = 0;
  check(calc.SetCalcFunc(calc.HeldFunc()) == icFuncParseNoError,
        "SetCalcFunc(m_calcFunc) reports success");
  check(g_calcFuncDtors == 0,
        "SetCalcFunc(m_calcFunc) does not free the function it is handed");
  check(calc.HeldFunc() == kept,
        "SetCalcFunc(m_calcFunc) keeps the function installed");

  // Control, as above.
  g_calcFuncDtors = 0;
  CountingCalcFunc *replacement = new CountingCalcFunc(&calc);
  check(calc.SetCalcFunc(replacement) == icFuncParseNoError,
        "SetCalcFunc takes a new function");
  check(g_calcFuncDtors == 1, "SetCalcFunc releases the function it replaces");
  check(calc.HeldFunc() == replacement, "the replacement is installed");
  g_calcFuncDtors = 0;
}

int main()
{
  // An unfixed build crashes partway through, so keep stdout unbuffered:
  // otherwise the PASS trail that says which case was reached is lost.
  setvbuf(stdout, NULL, _IONBF, 0);

  testSegmentedCurveTag();
  testMpeCLUT();
  testSpectralCLUT();
  // The countable apply-table case runs FIRST on purpose.  Its sibling below
  // reds by crashing, which on an unfixed build kills the process inside the
  // MPE destructor before any FAIL line is printed -- so running the counted
  // one first is what puts the diagnostic in the log ahead of the abort.
  testSpectralApplyCLUT();
  testSpectralApplyCLUTAfterBegin();
  testSpectralCLUTSelfAssign();
  testSpectralMatrixSelfAssign();
  testSpectralObserverSelfAssign();
  testSpectralApplyMtx();
  testTintArraySetArray();
  testTintArrayCopyFromEmpty();
  testDictEntryLocalized();
  testMBBTag();
  testMpeCAM();
  // The two #2645 cases run LAST.  Both reach a double free on an unfixed
  // build -- the first through a use-after-free write inside the setter -- and
  // an abort there must not cost the diagnostics of every case above it.
  testSampledCalculatorCurveSetCalculator();
  testCalculatorSetCalcFunc();

  return g_failures ? 1 : 0;
}
