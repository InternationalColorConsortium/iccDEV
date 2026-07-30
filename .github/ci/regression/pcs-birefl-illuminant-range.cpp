// Regression for #1677 (and the #1675 family): the bi-spectral PCS connection steps
// copied a Profile Connection Conditions illuminant into a buffer sized by the source
// profile's header, without checking that the two could be related.
//
// CIccPcsXform::rangeMap() answers with NULL in two different situations:
//
//   * the ranges are identical, so no conversion matrix is needed;
//   * a conversion is needed but SetRange() refused to build one -- it rejects any pair
//     carrying <= 1 step, a non-finite endpoint or a zero-width span.
//
// pushBiRef2Rad() read NULL as only the first, and fell back to
//
//     memcpy(pMtx->data(), illuminant, illuminantRange.steps*sizeof(icFloatNumber));
//
// where pMtx->data() was allocated as new icFloatNumber[biSpectralRange.steps]. A header
// declaring biSpectralRange.steps == 0 therefore pushed a whole illuminant into a
// zero-length array: with the reproducer from #1677 that is a 324-byte write into a
// 1-byte region (CWE-787), reached from CIccCmm::Begin() through CheckPCSConnections()
// and Connect() before any pixel is applied. pushBiRef2Ref() has the mirror image of the
// same fault -- its fallback walks illuminant[] to spectralRange.steps, past the end of
// an array holding only illuminantRange.steps entries, so it reads out of bounds instead.
//
// The fix screens the identical-range case up front with icSameSpectralRange(), the way
// the neighbouring pushSpecToRange() already did, which leaves a refused map as an
// unambiguous reject. These assertions are therefore visible without a sanitizer: on
// unfixed sources the calls return icCmmStatOk (having corrupted the heap on the way),
// and here they must return a failure status.
//
// The controls at the end matter as much as the rejections: only one bi-spectral profile
// exists in the whole Testing corpus, so an over-eager guard here would go unnoticed by
// the rest of the suite. They pin that identical ranges and merely-different-but-mappable
// ranges both still succeed.
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
    std::fprintf(stderr, "[pcs-birefl-range] FAIL: %s\n", what);
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

// The push* helpers only ever ask the connection conditions for the viewing conditions
// tag, so the remaining three interface methods are stubs.
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

// pushBiRef2Rad()/pushBiRef2Ref() are protected; subclassing is the supported way in.
class PcsXformProbe : public CIccPcsXform
{
public:
  using CIccPcsXform::pushBiRef2Rad;
  using CIccPcsXform::pushBiRef2Ref;

  size_t stepCount() const { return m_list ? m_list->size() : 0; }
};

// A header-only profile: the push* helpers under test read m_Header and nothing else.
void setHeader(CIccProfile &prof, icColorSpaceSignature spectralPCS,
               const icSpectralRange &spectralRange, const icSpectralRange &biSpectralRange)
{
  std::memset(&prof.m_Header, 0, sizeof(prof.m_Header));
  prof.m_Header.version = icVersionNumberV5;
  prof.m_Header.deviceClass = icSigColorSpaceClass;
  prof.m_Header.spectralPCS = spectralPCS;
  prof.m_Header.spectralRange = spectralRange;
  prof.m_Header.biSpectralRange = biSpectralRange;
}

// #1677: a bi-spectral header whose biSpectralRange cannot be mapped from the illuminant
// must be refused, not copied into.
void testBiRefRadRejectsUnmappableBiSpectralRange()
{
  const icSpectralRange illumRange = makeRange(icRange380nm, icRange780nm, 81);

  struct Case {
    icColorSpaceSignature type;
    const char *what;
  } cases[] = {
    { icSigBiSpectralReflectanceData,   "bi-spectral reflectance" },
    { icSigSparseMatrixReflectanceData, "sparse matrix reflectance" },
  };

  for (size_t c = 0; c < sizeof(cases)/sizeof(cases[0]); c++) {
    char msg[200];
    StubPcc pcc(illumRange);

    CIccProfile prof;
    setHeader(prof, icNColorSpaceSig(cases[c].type, 36),
              makeRange(icRange380nm, icRange780nm, 36),
              // steps == 0: SetRange() refuses the pair, and the buffer this would have
              // been copied into is new icFloatNumber[0].
              makeRange(icRange380nm, icRange780nm, 0));

    PcsXformProbe probe;
    icStatusCMM stat = probe.pushBiRef2Rad(&prof, &pcc);

    std::snprintf(msg, sizeof(msg),
                  "%s: pushBiRef2Rad refuses a zero-step biSpectralRange instead of "
                  "copying %u illuminant entries into a zero-length buffer",
                  cases[c].what, (unsigned)pcc.effectiveIlluminantSteps());
    check(stat != icCmmStatOk, msg);

    std::snprintf(msg, sizeof(msg), "%s: no PCS step is left behind on the reject path",
                  cases[c].what);
    check(probe.stepCount() == 0, msg);
  }
}

// The mirror image in pushBiRef2Ref(): here the refused map makes the fallback read past
// the end of the illuminant rather than write past the end of its own buffer.
void testBiRefRefRejectsUnmappableSpectralRange()
{
  // A single-step illuminant is storable but cannot be resampled from, so
  // rangeMap(illuminantRange, spectralRange) is refused for any spectralRange.
  const icSpectralRange illumRange = makeRange(icRange380nm, icRange780nm, 1);

  StubPcc pcc(illumRange);
  const icUInt16Number illumSteps = pcc.effectiveIlluminantSteps();

  CIccProfile prof;
  setHeader(prof, icNColorSpaceSig(icSigBiSpectralReflectanceData, 41),
            // Wider than the illuminant, so the unfixed fallback loop overruns it.
            makeRange(icRange380nm, icRange780nm, 41),
            // Identical to the illuminant range, so pushBiRef2Rad() -- which runs first
            // -- takes its exact-fit copy and this test isolates pushBiRef2Ref().
            illumRange);

  PcsXformProbe probe;
  icStatusCMM stat = probe.pushBiRef2Ref(&prof, &pcc);

  char msg[220];
  std::snprintf(msg, sizeof(msg),
                "pushBiRef2Ref refuses a spectralRange of 41 steps it cannot map from a "
                "%u-step illuminant, instead of reading past the illuminant's end",
                (unsigned)illumSteps);
  check(stat != icCmmStatOk, msg);
}

// Controls. The corpus carries a single bi-spectral profile, so nothing else in the suite
// would catch a guard that rejects too much.
void testConformantRangesStillAccepted()
{
  // 1. Identical illuminant and biSpectralRange: no matrix is needed and the wholesale
  //    copy is an exact fit. This is the case the NULL return originally meant.
  {
    const icSpectralRange illumRange = makeRange(icRange380nm, icRange780nm, 81);
    StubPcc pcc(illumRange);

    CIccProfile prof;
    setHeader(prof, icNColorSpaceSig(icSigBiSpectralReflectanceData, 36),
              makeRange(icRange380nm, icRange780nm, 36), illumRange);

    PcsXformProbe probe;
    icStatusCMM stat = probe.pushBiRef2Rad(&prof, &pcc);

    check(stat == icCmmStatOk, "identical illuminant and biSpectralRange still accepted");
    check(probe.stepCount() == 1, "identical ranges still push exactly one PCS step");
  }

  // 2. Different but mappable, which is the shape of the one bi-spectral profile in
  //    Testing/ (Named/FluorescentNamedColor.icc: spectralRange 31, biSpectralRange 41
  //    against an 81-step illuminant). A matrix is built and used.
  {
    const icSpectralRange illumRange = makeRange(icRange380nm, icRange780nm, 81);
    StubPcc pcc(illumRange);

    CIccProfile prof;
    setHeader(prof, icNColorSpaceSig(icSigBiSpectralReflectanceData, 31),
              makeRange(icRange380nm, icRange780nm, 31),
              makeRange(icRange380nm, icRange780nm, 41));

    PcsXformProbe probe;
    icStatusCMM stat = probe.pushBiRef2Rad(&prof, &pcc);

    check(stat == icCmmStatOk,
          "mappable illuminant and biSpectralRange still accepted (Fluorescent shape)");
    check(probe.stepCount() == 1, "mappable ranges still push exactly one PCS step");
  }
}

} // namespace

int main()
{
  testBiRefRadRejectsUnmappableBiSpectralRange();
  testBiRefRefRejectsUnmappableSpectralRange();
  testConformantRangesStillAccepted();

  if (g_fail)
    std::fprintf(stderr, "[pcs-birefl-range] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[pcs-birefl-range] all assertions passed\n");

  return g_fail;
}
