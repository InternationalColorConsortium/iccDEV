// Regression for #1853 (the ci-qa-flags bucket-4 remainder): the PCS step chain that
// CIccPcsXform::Connect() builds could be left disagreeing with itself, and the
// temporaries the chain runs through were sized on the assumption that it never is.
//
// Two independent halves, both in IccCmm.cpp.
//
// 1. pushApplyIllum() dropped a refused resampling step instead of rejecting.
//
//    CIccPcsXform::rangeMap() answers NULL for two different situations: the ranges are
//    identical so no matrix is needed, or a matrix was needed and SetRange() refused to
//    build one (it rejects any pair carrying <= 1 step, a non-finite endpoint or a
//    zero-width source span). pushApplyIllum() reaches its rangeMap() calls only inside
//    the else of an icSameSpectralRange() test, so NULL there can only be the failure
//    answer -- yet both calls were written as
//
//        ptr.ptr = rangeMap(...);
//        if (ptr.ptr)
//          m_list->push_back(ptr);
//
//    A refused map therefore shortened the chain and returned icCmmStatOk. What is left
//    is a scale running at illuminantRange.steps over a vector that is
//    spectralRange.steps wide. The second call is the damaging one, because it is the
//    last step pushed and so its output is what the caller reads back: dropping it makes
//    the chain hand illuminantRange.steps values to a consumer expecting
//    spectralRange.steps, past the end of the destination pixel when the illuminant is
//    the wider of the two. These were the last two of the six CIccPcsXform::rangeMap()
//    call sites in that file still treating the ambiguous NULL as benign; the other four
//    -- pushSpecToRange() and the three in pushBiRef2Rad()/pushBiRef2Ref() -- already
//    reject (see #1677 and pcs-birefl-illuminant-range.cpp, which pins those).
//
//    These assertions need no sanitizer: on unfixed sources the call returns
//    icCmmStatOk with a short chain, and here it must return a failure status.
//
// 2. CIccPcsXform::MaxChannels() did not count what the temporaries are written with.
//
//    CIccApplyPcsXform::Init() sizes m_temp1/m_temp2 from MaxChannels(), and
//    CIccApplyPcsXform::Apply() hands those two buffers to every step except the last as
//    its *destination*. MaxChannels() took GetDstChannels() from the first step only and
//    GetSrcChannels() from all of them, so no step after the first had its output width
//    counted. On a chain that agrees with itself the answer comes out the same -- each
//    intermediate output is also the next step's input and is counted in that role --
//    which is why it has held. Once a chain does not agree, the miss is an undersized
//    buffer that a step is about to write through.
//
//    Asserted twice over, so the case does not depend on how the build was configured:
//    on the return value of MaxChannels() itself, which fails anywhere, and by running a
//    pixel through the chain, which is what a sanitizer build sees as the overflow.
//
// The controls matter as much as the rejections. Only a handful of spectral profiles
// exist in Testing/ and none of them exercise a refused illuminant map, so an over-eager
// guard in pushApplyIllum() would go unnoticed by the rest of the suite; and the
// MaxChannels() rewrite must not shrink the answer for any chain that sizes correctly
// today. Both are pinned below.
//
// Header + IccProfLib only, no fixture and no I/O.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccPcc.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[pcs-applyillum-chain] FAIL: %s\n", what);
  }
}

icSpectralRange makeRange(icFloat16Number start, icFloat16Number end, icUInt16Number steps)
{
  icSpectralRange r;
  r.start = start;
  r.end = end;
  r.steps = steps;
  return r;
}

// pushApplyIllum() asks the connection conditions for the viewing conditions tag and
// nothing else, so the remaining interface methods are stubs.
class StubPcc : public IIccProfileConnectionConditions
{
public:
  explicit StubPcc(const icSpectralRange &illumRange)
  {
    std::vector<icFloatNumber> spd(illumRange.steps ? illumRange.steps : 1, 1.0f);
    m_svc.setIlluminant(icIlluminantD50, illumRange, &spd[0]);
  }

  virtual const CIccTagSpectralViewingConditions *getPccViewingConditions() { return &m_svc; }
  virtual CIccTagMultiProcessElement *getCustomToStandardPcc() { return NULL; }
  virtual CIccTagMultiProcessElement *getStandardToCustomPcc() { return NULL; }

  virtual void getNormIlluminantXYZ(icFloatNumber *pXYZ) { pXYZ[0] = pXYZ[1] = pXYZ[2] = 1.0f; }
  virtual void getLumIlluminantXYZ(icFloatNumber *pXYZ) { pXYZ[0] = pXYZ[1] = pXYZ[2] = 1.0f; }
  virtual bool getMediaWhiteXYZ(icFloatNumber *pXYZ)
  {
    pXYZ[0] = pXYZ[1] = pXYZ[2] = 1.0f;
    return true;
  }

  // Reports what getIlluminant() will actually hand the code under test, which is not
  // always what was asked for: the tag falls back to a known standard SPD when the
  // custom illuminant did not take.
  icUInt16Number effectiveIlluminantSteps()
  {
    icSpectralRange got;
    return m_svc.getIlluminant(got) ? got.steps : 0;
  }

private:
  CIccTagSpectralViewingConditions m_svc;
};

// pushApplyIllum(), pushMatrix() and pushScale() are protected; subclassing is the
// supported way in. MaxChannels(), GetNewApply() and Apply() are already public.
class PcsXformProbe : public CIccPcsXform
{
public:
  using CIccPcsXform::pushApplyIllum;
  using CIccPcsXform::pushMatrix;
  using CIccPcsXform::pushScale;

  size_t stepCount() const { return m_list ? m_list->size() : 0; }
};

// A header-only profile: pushApplyIllum() reads m_Header.spectralRange and nothing else.
void setHeader(CIccProfile &prof, const icSpectralRange &spectralRange)
{
  std::memset(&prof.m_Header, 0, sizeof(prof.m_Header));
  prof.m_Header.version = icVersionNumberV5;
  prof.m_Header.deviceClass = icSigColorSpaceClass;
  prof.m_Header.spectralPCS = icNColorSpaceSig(icSigRadiantSpectralData, spectralRange.steps);
  prof.m_Header.spectralRange = spectralRange;
}

// ---------------------------------------------------------------------------
// 1. pushApplyIllum() must reject an illuminant range it cannot resample.
// ---------------------------------------------------------------------------

void testApplyIllumRejectsUnmappableIlluminantRange()
{
  // The profile's own range is the same in both cases; only the illuminant varies, so
  // each case isolates one of the two rangeMap() legs.
  const icSpectralRange spectralRange = makeRange(icRange380nm, icRange780nm, 36);

  struct Case {
    icSpectralRange illumRange;
    const char *what;
    const char *leg;
  } cases[] = {
    // A single-step illuminant is storable but is neither a usable resampling source
    // nor a usable destination, so SetRange() refuses both legs. Unfixed, the whole
    // chain collapses to the bare scale.
    { makeRange(icRange380nm, icRange780nm, 1),
      "single-step illuminant",
      "both legs" },

    // A zero-width span is refused as a source (srcScale == 0) while remaining a legal
    // destination, so here the outbound matrix is built and only the return leg is
    // refused. This is the shape that leaves the wrong width at the end of the chain.
    { makeRange(icRange380nm, icRange380nm, 81),
      "zero-width illuminant span",
      "return leg only" },
  };

  for (size_t c = 0; c < sizeof(cases)/sizeof(cases[0]); c++) {
    char msg[260];
    StubPcc pcc(cases[c].illumRange);

    CIccProfile prof;
    setHeader(prof, spectralRange);

    PcsXformProbe probe;
    icStatusCMM stat = probe.pushApplyIllum(&prof, &pcc);

    std::snprintf(msg, sizeof(msg),
                  "%s (%s refused): pushApplyIllum rejects a %u-step illuminant it cannot "
                  "map onto a 36-step spectral range, instead of dropping the step and "
                  "reporting success",
                  cases[c].what, cases[c].leg, (unsigned)pcc.effectiveIlluminantSteps());
    check(stat != icCmmStatOk, msg);

    std::snprintf(msg, sizeof(msg),
                  "%s: no PCS step is left behind on the reject path", cases[c].what);
    check(probe.stepCount() == 0, msg);
  }
}

// Controls for the guard above: the two shapes that must keep working.
void testMappableIlluminantRangesStillAccepted()
{
  // 1. Illuminant range identical to the profile's. No matrices are needed and the
  //    scale alone is the whole chain -- the case the NULL return originally meant.
  {
    const icSpectralRange range = makeRange(icRange380nm, icRange780nm, 36);
    StubPcc pcc(range);

    CIccProfile prof;
    setHeader(prof, range);

    PcsXformProbe probe;
    icStatusCMM stat = probe.pushApplyIllum(&prof, &pcc);

    check(stat == icCmmStatOk, "identical illuminant and spectral range still accepted");
    check(probe.stepCount() == 1, "identical ranges still push exactly the scale step");
  }

  // 2. Different but mappable, which is the ordinary case: an 81-step illuminant over a
  //    36-step spectral PCS. Both matrices are built and the scale runs between them.
  {
    const icSpectralRange spectralRange = makeRange(icRange380nm, icRange780nm, 36);
    const icSpectralRange illumRange = makeRange(icRange380nm, icRange780nm, 81);
    StubPcc pcc(illumRange);

    CIccProfile prof;
    setHeader(prof, spectralRange);

    PcsXformProbe probe;
    icStatusCMM stat = probe.pushApplyIllum(&prof, &pcc);

    check(stat == icCmmStatOk, "mappable illuminant range still accepted");
    check(probe.stepCount() == 3, "mappable ranges still push resample, scale, resample");

    // The chain widens to the illuminant in the middle and comes back, so the
    // temporaries have to hold the illuminant's width. This is also the control on the
    // MaxChannels() rewrite: a chain that agrees with itself must size exactly as before.
    check(probe.MaxChannels() == illumRange.steps,
          "a self-consistent chain still sizes its temporaries to the widest step");
  }
}

// ---------------------------------------------------------------------------
// 2. MaxChannels() must count every step's output, not just the first step's.
// ---------------------------------------------------------------------------

void testMaxChannelsCoversEveryStepOutput()
{
  // Widths chosen so that master's walk cannot reach the middle step's output:
  //
  //   step 0  matrix  4 <- 4    src 4, dst 4    (first step: dst IS counted)
  //   step 1  matrix 64 <- 4    src 4, dst 64   (writes 64 into a temporary)
  //   step 2  matrix  4 <- 8    src 8, dst 4    (last step: writes the caller's pixel)
  //
  // Taking dst from step 0 and src from all three gives max(4, 4, 4, 8) == 8, so the
  // temporaries hold 8 floats and step 1 writes 64 of them. Step 2 reading only 8 back is
  // what keeps the shortfall out of the src-side tally -- that disagreement between step
  // 1's output and step 2's input is exactly the condition the old walk assumed away.
  const icUInt16Number kWide = 64;
  const icUInt16Number kNarrow = 4;
  const icUInt16Number kMid = 8;

  // Only the declared widths matter here, not the arithmetic, so one buffer large enough
  // for the widest step (kWide x kWide) backs all three pushMatrix() calls; each copies
  // the nRows * nCols prefix it needs.
  std::vector<icFloatNumber> coeffs((size_t)kWide * kWide, 0.0f);
  for (icUInt16Number i = 0; i < kWide; i++)
    coeffs[(size_t)i * kWide + i] = 1.0f;

  PcsXformProbe probe;
  probe.pushMatrix(kNarrow, kNarrow, &coeffs[0]);
  probe.pushMatrix(kWide, kNarrow, &coeffs[0]);
  probe.pushMatrix(kNarrow, kMid, &coeffs[0]);

  char msg[240];
  std::snprintf(msg, sizeof(msg),
                "MaxChannels counts the %u-channel output of a middle step (reported %u)",
                (unsigned)kWide, (unsigned)probe.MaxChannels());
  check(probe.MaxChannels() >= kWide, msg);

  // Run a pixel through it. Under ASAN this is the assertion: unfixed, step 1's Apply()
  // writes kWide floats into the kMid-float temporary that Init() allocated. Without a
  // sanitizer the write is silent, which is why the MaxChannels() value is checked above
  // as well rather than relying on this alone.
  icStatusCMM status = icCmmStatOk;
  CIccApplyXform *pApply = probe.GetNewApply(status);

  check(pApply != NULL, "the three-step chain builds an apply object");

  if (pApply) {
    icFloatNumber src[kWide];
    icFloatNumber dst[kWide];

    for (icUInt16Number i = 0; i < kWide; i++) {
      src[i] = 0.5f;
      dst[i] = 0.0f;
    }

    probe.Apply(pApply, dst, src);
    delete pApply;
  }
}

} // namespace

int main()
{
  testApplyIllumRejectsUnmappableIlluminantRange();
  testMappableIlluminantRangesStillAccepted();
  testMaxChannelsCoversEveryStepOutput();

  if (g_fail)
    std::fprintf(stderr, "[pcs-applyillum-chain] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[pcs-applyillum-chain] all assertions passed\n");

  return g_fail;
}
