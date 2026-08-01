// Regression for #1932: CIccPcsXform::Connect() built a spectral PCS step chain from the
// profile header's spectralRange while the pixel buffers those steps run in were sized
// from the header's spectralPCS signature, and never checked that the two agreed.
//
// A spectral PCS signature carries its channel count in its low 16 bits. That count is
// what CIccXform::GetNumSrcSamples()/GetNumDstSamples() report, and in turn what
// CIccApplyCmm::InitPixel() uses to malloc m_Pixel/m_Pixel2. Every spectral branch of
// Connect() instead sizes the steps it pushes from spectralRange.steps. For a conformant
// reflectance, transmission or radiant PCS the two are the same number written twice.
//
// The reproducer writes them differently. Its embedded v5 sub-profile declares
// spectralPCS "rs" + 36 channels alongside spectralRange.steps == 184, so:
//
//   * the pixel buffer is malloc(36 * sizeof(icFloatNumber)) = 144 bytes;
//   * pushRef2Xyz() -> pushRad2Cie() -> pushMatrix(3, 184, observer) builds a 3x184
//     observer matrix, which CIccPcsStepMatrix::reduce() converts to a sparse matrix.
//
// CIccSparseMatrix::MultiplyVector() then walks that matrix's column indices - every one
// of them in range for a 184-column matrix, the matrix is well formed and IsValid()
// passes - against a 36-float vector, reading past its end on the 37th column
// (CWE-125, ASan heap-buffer-overflow, 4-byte read 0 bytes after a 144-byte region).
// Nothing in the sparse matrix is wrong; the caller handed it a vector shorter than the
// matrix it was built for.
//
// CIccProfile::Validate() already reports the same disagreement as a critical error, in
// the spectralPCS switch of CheckHeader(). Nothing on the apply path consults it -
// neither iccApplyProfiles nor CIccCmm calls Validate() - so Connect() has to make the
// check itself. It now does, returning icCmmStatInvalidProfile before any step is pushed.
//
// Rejection is the only correct outcome. There is no way to tell which of the two numbers
// the producer meant, and honouring either one silently reinterprets the pixel data.
//
// The scoping is deliberately identical to the validator's, and testSparseMatrixPcsIsOutOfScope()
// below pins the part that is easy to get wrong: the sparse-matrix PCS is NOT covered,
// because there the channel count is the size of the encoded matrix blob carried in the
// pixel rather than a sample count, so it is independent of the ranges by design (see
// CIccPcsStepSrcSparseMatrix, which is handed the two separately).
//
// Red-green: on unfixed sources testMismatchedSpectralRangeIsRejected() fails, because
// Connect() returns icCmmStatOk having built the chain that overflows at apply time. The
// assertions are all on returned status, so they are visible without a sanitizer.
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

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[spectral-pcs-range] FAIL: %s\n", what);
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

// A connection conditions object carrying both an illuminant and an observer, so that a
// header-consistent spectral profile can be connected all the way through rather than
// stopping on a missing tag. Without the observer, pushRad2Cie() refuses every case and
// the accept-controls below could not tell "the guard let this through" from "the guard
// rejected it".
class StubPcc : public IIccProfileConnectionConditions
{
public:
  StubPcc(const icSpectralRange &illumRange, const icSpectralRange &observerRange)
  {
    std::vector<icFloatNumber> spd(illumRange.steps ? illumRange.steps : 1, 1.0f);
    m_svc.setIlluminant(icIlluminantD50, illumRange, &spd[0]);

    // Three observer curves (x, y, z) laid end to end, as setObserver() expects.
    std::vector<icFloatNumber> obs(observerRange.steps ? observerRange.steps * 3 : 3, 1.0f);
    m_svc.setObserver(icStdObs1931TwoDegrees, observerRange, &obs[0]);
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

private:
  CIccTagSpectralViewingConditions m_svc;
};

// Connect() reads only a handful of things from the xforms on either side of it: which
// spaces and sample counts they present, whether they are input/MCS/abstract, their
// connection conditions, and their profile. A stub supplies exactly those. The two pure
// virtuals (GetXformType, Apply) are never reached - Connect() does not apply anything.
class StubXform : public CIccXform
{
public:
  StubXform(CIccProfile *pProfile, bool bInput, icColorSpaceSignature space,
            icUInt16Number nSamples, IIccProfileConnectionConditions *pPcc)
  {
    m_pProfile = pProfile;
    ShareProfile();          // the test owns the profiles, not the xform
    m_bInput = bInput;
    m_pConnectionConditions = pPcc;
    m_space = space;
    m_nSamples = nSamples;
  }

  virtual icXformType GetXformType() const { return icXformTypeMatrixTRC; }
  virtual void Apply(CIccApplyXform *, icFloatNumber *, const icFloatNumber *) const {}

  // Curve extraction is only used by the optimiser once a chain is built; Connect() never
  // asks. NULL is the same answer CIccPcsXform itself gives.
  virtual LPIccCurve *ExtractInputCurves() { return NULL; }
  virtual LPIccCurve *ExtractOutputCurves() { return NULL; }

  virtual icColorSpaceSignature GetSrcSpace() const { return m_space; }
  virtual icColorSpaceSignature GetDstSpace() const { return m_space; }
  virtual icUInt16Number GetNumSrcSamples() const { return m_nSamples; }
  virtual icUInt16Number GetNumDstSamples() const { return m_nSamples; }

private:
  icColorSpaceSignature m_space;
  icUInt16Number m_nSamples;
};

// A header-only profile. Connect()'s new check reads m_Header and nothing else, and the
// push* helpers it dispatches to read the same ranges from the same place.
void setHeader(CIccProfile &prof, icUInt32Number spectralPCS,
               const icSpectralRange &spectralRange, const icSpectralRange &biSpectralRange)
{
  std::memset(&prof.m_Header, 0, sizeof(prof.m_Header));
  prof.m_Header.version = icVersionNumberV5;
  prof.m_Header.deviceClass = icSigColorSpaceClass;
  prof.m_Header.colorSpace = icSigRgbData;
  prof.m_Header.spectralPCS = (icColorSpaceSignature)spectralPCS;
  prof.m_Header.spectralRange = spectralRange;
  prof.m_Header.biSpectralRange = biSpectralRange;
}

// Connects a spectral source profile to an XYZ destination, the shape the reproducer
// takes, and returns Connect()'s status. nChannels is what the signature declares (and
// therefore what the pixel buffer would be sized to); the ranges are what the pushed
// steps would be sized from.
icStatusCMM connectSpectralSource(icColorSpaceSignature pcsType, icUInt16Number nChannels,
                                  const icSpectralRange &spectralRange,
                                  const icSpectralRange &biSpectralRange)
{
  const icSpectralRange stdRange = makeRange(icRange380nm, icRange780nm, 81);
  StubPcc pcc(stdRange, stdRange);

  CIccProfile srcProf, dstProf;
  setHeader(srcProf, icNColorSpaceSig(pcsType, nChannels), spectralRange, biSpectralRange);

  // A plain XYZ destination: no spectral PCS of its own, so it cannot be the side that
  // trips the check and the result is attributable to the source alone.
  std::memset(&dstProf.m_Header, 0, sizeof(dstProf.m_Header));
  dstProf.m_Header.version = icVersionNumberV5;
  dstProf.m_Header.deviceClass = icSigOutputClass;
  dstProf.m_Header.colorSpace = icSigCmykData;
  dstProf.m_Header.pcs = icSigXYZData;

  StubXform from(&srcProf, true, (icColorSpaceSignature)icNColorSpaceSig(pcsType, nChannels),
                 nChannels, &pcc);
  StubXform to(&dstProf, false, icSigXYZPcsData, 3, &pcc);

  CIccPcsXform pcs;
  return pcs.Connect(&from, &to);
}

// The defect. Each of the three plain spectral PCS types states its sample count twice;
// when the two disagree the profile must be refused rather than reinterpreted.
void testMismatchedSpectralRangeIsRejected()
{
  struct Case {
    icColorSpaceSignature type;
    const char *what;
  } cases[] = {
    { icSigReflectanceSpectralData,  "reflectance ('rs')" },
    { icSigTransmisionSpectralData,  "transmission ('ts')" },
    { icSigRadiantSpectralData,      "radiant ('es')" },
  };

  for (size_t c = 0; c < sizeof(cases)/sizeof(cases[0]); c++) {
    char msg[240];

    // The reproducer's exact numbers: 36 channels declared, 184 steps used.
    icStatusCMM stat = connectSpectralSource(cases[c].type, 36,
                                             makeRange(icRange380nm, icRange780nm, 184),
                                             makeRange(0, 0, 0));

    std::snprintf(msg, sizeof(msg),
                  "%s: 36 declared channels against spectralRange.steps == 184 is refused, "
                  "not built into a 3x184 matrix run over a 36-float pixel",
                  cases[c].what);
    check(stat == icCmmStatInvalidProfile, msg);

    // The other direction is equally unusable: more channels than there are samples to
    // fill them leaves the tail of every pixel undefined.
    stat = connectSpectralSource(cases[c].type, 184,
                                 makeRange(icRange380nm, icRange780nm, 36),
                                 makeRange(0, 0, 0));

    std::snprintf(msg, sizeof(msg),
                  "%s: 184 declared channels against spectralRange.steps == 36 is refused",
                  cases[c].what);
    check(stat == icCmmStatInvalidProfile, msg);
  }
}

// Bi-spectral carries one sample per (spectral, bi-spectral) wavelength pair, so the
// channel count has to equal the product rather than either factor.
void testMismatchedBiSpectralRangeIsRejected()
{
  const icSpectralRange spectral = makeRange(icRange380nm, icRange780nm, 31);
  const icSpectralRange biSpectral = makeRange(icRange380nm, icRange780nm, 41);

  // 31 * 41 == 1271, so 36 is a mismatch. Also the shape of a header that names one
  // factor instead of the product.
  icStatusCMM stat = connectSpectralSource(icSigBiSpectralReflectanceData, 36,
                                           spectral, biSpectral);
  check(stat == icCmmStatInvalidProfile,
        "bi-spectral: 36 declared channels against a 31 x 41 range product is refused");

  stat = connectSpectralSource(icSigBiSpectralReflectanceData, 31, spectral, biSpectral);
  check(stat == icCmmStatInvalidProfile,
        "bi-spectral: naming only the spectral factor (31) as the channel count is refused");
}

// The exclusion that mirrors the validator. For a sparse-matrix PCS the channel count is
// the float length of the encoded matrix blob the pixel carries, not a sample count, so
// it is unrelated to the ranges and a "mismatch" here is normal. Connect() must let it
// past the new check and fall through to the switch, which has its own answer for this
// source/destination pair. An over-eager guard would turn that answer into
// icCmmStatInvalidProfile and this assertion catches it.
void testSparseMatrixPcsIsOutOfScope()
{
  icStatusCMM stat = connectSpectralSource(icSigSparseMatrixReflectanceData, 36,
                                           makeRange(icRange380nm, icRange780nm, 184),
                                           makeRange(icRange380nm, icRange780nm, 41));

  check(stat != icCmmStatInvalidProfile,
        "sparse-matrix PCS is not subject to the channels-vs-range check");
}

// Controls. The check sits in front of every spectral branch of Connect(), so a guard
// that rejected too much would break spectral colour management wholesale; these pin the
// conformant shapes that must still connect.
void testConformantHeadersStillConnect()
{
  // 1. The reproducer's own shape, corrected: 36 channels, 36 steps.
  icStatusCMM stat = connectSpectralSource(icSigReflectanceSpectralData, 36,
                                           makeRange(icRange380nm, icRange780nm, 36),
                                           makeRange(0, 0, 0));
  check(stat == icCmmStatOk,
        "reflectance: 36 channels with spectralRange.steps == 36 still connects");

  // 2. The 81-step shape the Testing corpus is full of, and where channel count and step
  //    count coincide with the stub illuminant/observer range.
  stat = connectSpectralSource(icSigReflectanceSpectralData, 81,
                               makeRange(icRange380nm, icRange780nm, 81),
                               makeRange(0, 0, 0));
  check(stat == icCmmStatOk,
        "reflectance: 81 channels with spectralRange.steps == 81 still connects");

  // 3. A consistent bi-spectral header: 31 x 41 == 1271 channels.
  stat = connectSpectralSource(icSigBiSpectralReflectanceData, 1271,
                               makeRange(icRange380nm, icRange780nm, 31),
                               makeRange(icRange380nm, icRange780nm, 41));
  check(stat != icCmmStatInvalidProfile,
        "bi-spectral: a channel count equal to the range product is not rejected by the check");
}

// A profile with no spectral PCS at all has nothing to be inconsistent with, and the
// overwhelming majority of connections are of this kind. The check must be inert for
// them however the (unused) spectral range fields happen to be filled in.
void testNonSpectralConnectionIsUnaffected()
{
  const icSpectralRange stdRange = makeRange(icRange380nm, icRange780nm, 81);
  StubPcc pcc(stdRange, stdRange);

  CIccProfile srcProf, dstProf;
  std::memset(&srcProf.m_Header, 0, sizeof(srcProf.m_Header));
  srcProf.m_Header.version = icVersionNumberV4;
  srcProf.m_Header.deviceClass = icSigInputClass;
  srcProf.m_Header.colorSpace = icSigRgbData;
  srcProf.m_Header.pcs = icSigXYZData;
  // spectralPCS stays icSigNoSpectralData while the range fields carry residue; nothing
  // may key off them.
  srcProf.m_Header.spectralRange = makeRange(icRange380nm, icRange780nm, 184);

  std::memset(&dstProf.m_Header, 0, sizeof(dstProf.m_Header));
  dstProf.m_Header.version = icVersionNumberV4;
  dstProf.m_Header.deviceClass = icSigOutputClass;
  dstProf.m_Header.colorSpace = icSigCmykData;
  dstProf.m_Header.pcs = icSigXYZData;

  StubXform from(&srcProf, true, icSigXYZPcsData, 3, &pcc);
  StubXform to(&dstProf, false, icSigXYZPcsData, 3, &pcc);

  CIccPcsXform pcs;
  icStatusCMM stat = pcs.Connect(&from, &to);

  // icCmmStatIdentityXform, not icCmmStatOk: both sides present XYZ under the same
  // connection conditions, so Optimize() collapses the chain and reports that it did.
  // That is a success outcome -- what matters here is that the new check did not turn a
  // connection carrying leftover spectral range bytes into icCmmStatInvalidProfile.
  check(stat == icCmmStatIdentityXform || stat == icCmmStatOk,
        "an XYZ-to-XYZ connection with no spectral PCS still succeeds");
}

} // namespace

int main()
{
  testMismatchedSpectralRangeIsRejected();
  testMismatchedBiSpectralRangeIsRejected();
  testSparseMatrixPcsIsOutOfScope();
  testConformantHeadersStillConnect();
  testNonSpectralConnectionIsUnaffected();

  if (g_fail)
    std::fprintf(stderr, "[spectral-pcs-range] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[spectral-pcs-range] all assertions passed\n");

  return g_fail;
}
