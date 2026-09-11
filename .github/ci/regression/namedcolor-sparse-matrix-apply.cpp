// Regression: a v5 NamedColor entry whose spectral member is a sparseMatrixArrayType
// could never be applied, and CIccPcsStepSrcSparseMatrix::Apply() was dead code.
//
// CIccTagSparseMatrixArray is a CIccTagNumArray, but it deliberately speaks a different
// dialect of that interface than every other implementation:
//
//   * GetNumValues()   returns GetNumMatrices() -- a count of MATRICES, not of floats;
//   * GetValues() and Interpolate() take their nVectorSize in BYTES, and reject anything
//     that is not exactly GetBytesPerMatrix() (channels * sizeof(icFloatNumber));
//   * ValuePos() is unsupported outright.
//
// CIccArrayNamedColor::Validate() knows this and branches on IsMatrixArray(), checking
// the one invariant that matters -- GetChannelsPerMatrix() == m_nSpectralSamples -- and
// skipping the sample-count division entirely.  CIccStructNamedColor::GetTint() never got
// the matching branch, so it applied plain num-array arithmetic to a matrix array:
//
//     icUInt32Number nEntries = pData->GetNumValues()/nSamples;   // 1/768 == 0
//     if (nEntries<1) return false;
//
// For Testing/Named/SparseMatrixNamedColor.xml that is 1/768 == 0 and GetTint() bailed on
// its first check, so CIccXformNamedColor::Apply() returned icCmmStatBadTintXform for
// every colour in the profile.  Two further checks behind it fail on the same confusion
// -- pZero->GetNumValues()!=nSamples compares 1 matrix against 768 samples, and
// Interpolate() was handed nSamples (768 floats) where it requires GetBytesPerMatrix()
// (3072 bytes), a factor-of-four unit mismatch -- so the fix is a units fix, not a
// one-line count fix.
//
// The consequence reached past the named-colour tag.  CIccPcsXform::pushBiRef2Rad()
// builds a CIccPcsStepSrcSparseMatrix for any sparse-matrix spectral PCS source, and
// CIccCmm::Begin() succeeds -- the chain is complete and the step holds the real
// range-mapped illuminant.  But the named-colour lookup that would feed it an encoded
// matrix failed three steps earlier, so the step's Apply() was unreachable end to end and
// had never executed: instrumenting BeginStep() across the whole benchmark suite in
// docs/superpowers/results/2026-08-20-tier-c-results.md counted zero sparse PCS steps.
//
// Levels:
//   1. CIccPcsStepSrcSparseMatrix::Apply() against a hand-multiplied expected vector.
//      Coverage for code that was previously never executed, not a bug regression.
//   2. GetSpectralTint() on a sparse spectral member must succeed (pre-fix: false), and
//      the blob it returns, pushed through a step carrying a known illuminant, must equal
//      an independent dense recomputation.
//   3. Tint interpolation: tint 0 must reproduce the tint-zero matrix and tint 1 the
//      entry's own matrix, so the fix cannot pass by ignoring the zero-tint entry.
//   4. Controls: a plain float32 spectral member must still work (or the fix could pass
//      by breaking the path every other profile uses), and the Validate() invariant
//      GetChannelsPerMatrix() == spectral samples must still be enforced.
//
// Header + IccProfLib only, no fixture and no I/O: the corpus profile this bug was found
// with is generated from XML into gitignored Testing/**/*.icc, so a corpus change must
// not be able to disable these assertions.  The real profile is covered separately by
// iccdev.namedcolor-sparse-matrix-xml.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccArrayBasic.h"
#include "IccCmm.h"
#include "IccProfile.h"
#include "IccSparseMatrix.h"
#include "IccStructBasic.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"
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
    std::fprintf(stderr, "[nmcl-sparse] FAIL: %s\n", what);
  }
}

// The fixture's shape.  Deliberately tiny, and deliberately not square, so a row/column
// transposition cannot pass.  kChannels is the storage budget for one encoded matrix, in
// icFloatNumber-sized words; it is the value icGetSpaceSamples() reports for the spectral
// PCS signature, and must equal GetChannelsPerMatrix().
const icUInt16Number kRows     = 4;   // spectralRange.steps   -- output samples
const icUInt16Number kCols     = 3;   // biSpectralRange.steps -- illuminant samples
const icUInt16Number kChannels = 64;  // words per encoded matrix (256 bytes)

bool nearly(icFloatNumber a, icFloatNumber b, icFloatNumber tol = 1.0e-5f)
{
  const icFloatNumber d = a > b ? a - b : b - a;
  return d <= tol;
}

// A 4x3 matrix with a deliberate hole in every row, so the sparse encoding actually
// stores a subset and MultiplyVector() has to honour the column indices.
void fullMatrixA(icFloatNumber *p)
{
  static const icFloatNumber kVals[kRows * kCols] = {
    0.5f,   0.0f,   0.25f,
    0.0f,   1.0f,   0.0f,
    0.125f, 0.0f,   0.75f,
    0.0f,   0.375f, 0.5f,
  };
  std::memcpy(p, kVals, sizeof(kVals));
}

// A second, different matrix for the tint-zero entry.
void fullMatrixZero(icFloatNumber *p)
{
  static const icFloatNumber kVals[kRows * kCols] = {
    0.25f,  0.0f,   0.5f,
    0.0f,   0.5f,   0.0f,
    0.375f, 0.0f,   0.25f,
    0.0f,   0.125f, 1.0f,
  };
  std::memcpy(p, kVals, sizeof(kVals));
}

// The pair above shares one sparsity pattern, which only exercises the merge loop's
// "both sides have this column" branch.  These two differ from each other in every row
// and each carries one all-zero row, so interpolating them also drives the two
// single-sided tails, the two whole-row copies, and a row where one side is empty -- the
// shape any real pair of authored matrices has.  Testing/Named/SparseMatrixNamedColor is
// such a pair: its tint-zero matrix is a different matrix from its colours'.
void fullMatrixDiffEntry(icFloatNumber *p)
{
  static const icFloatNumber kVals[kRows * kCols] = {
    0.5f,   0.0f,   0.0f,     // {0}
    0.0f,   0.0f,   0.0f,     // {}  -- empty row
    0.0f,   0.25f,  0.75f,    // {1,2}
    0.125f, 0.375f, 0.5f,     // {0,1,2}
  };
  std::memcpy(p, kVals, sizeof(kVals));
}

void fullMatrixDiffZero(icFloatNumber *p)
{
  static const icFloatNumber kVals[kRows * kCols] = {
    0.0f,   0.75f,  0.25f,    // {1,2}
    0.5f,   0.0f,   0.0f,     // {0}
    0.0f,   0.0f,   0.0f,     // {}  -- empty row
    0.25f,  0.0f,   0.0f,     // {0}
  };
  std::memcpy(p, kVals, sizeof(kVals));
}

// Multiply a dense rows x cols matrix by a cols-vector, by hand.  This is the oracle:
// it shares no code with CIccSparseMatrix.
void denseMultiply(icFloatNumber *pDst, const icFloatNumber *pFull,
                   const icFloatNumber *pVec)
{
  for (int r = 0; r < (int)kRows; r++) {
    icFloatNumber sum = 0.0f;
    for (int c = 0; c < (int)kCols; c++)
      sum += pFull[r * kCols + c] * pVec[c];
    pDst[r] = sum;
  }
}

// Author one encoded sparse matrix into a caller-owned blob of kChannels words.
bool encodeMatrix(icFloatNumber *pBlob, const icFloatNumber *pFull)
{
  std::memset(pBlob, 0, kChannels * sizeof(icFloatNumber));

  CIccSparseMatrix mtx(pBlob, kChannels * sizeof(icFloatNumber),
                       icSparseMatrixFloatNum, false);
  // bSetData=true is required: Init() otherwise allocates the accessor without stamping
  // the row/column dimensions into the blob, and every reader -- CIccSparseMatrix's own
  // bInitFromData ctor included -- recovers them from those first two words.
  if (!mtx.Init(kRows, kCols, /*bSetData=*/true))
    return false;

  return mtx.FillFromFullMatrix(const_cast<icFloatNumber *>(pFull));
}

// ---------------------------------------------------------------------------
// Level 1: CIccPcsStepSrcSparseMatrix::Apply() in isolation.
// ---------------------------------------------------------------------------

void applyMultipliesEncodedSourceMatrixByTheStepsIlluminant()
{
  const icFloatNumber kIllum[kCols] = { 0.5f, 2.0f, 4.0f };

  icFloatNumber full[kRows * kCols];
  fullMatrixA(full);

  std::vector<icFloatNumber> blob(kChannels, 0.0f);
  check(encodeMatrix(&blob[0], full), "L1: encoded the source matrix");

  CIccPcsStepSrcSparseMatrix step(kRows, kCols, kChannels);
  std::memcpy(step.data(), kIllum, sizeof(kIllum));

  icFloatNumber got[kRows];
  for (int i = 0; i < (int)kRows; i++)
    got[i] = -1.0f;

  step.Apply(NULL, got, &blob[0]);

  icFloatNumber want[kRows];
  denseMultiply(want, full, kIllum);

  for (int r = 0; r < (int)kRows; r++) {
    char msg[160];
    std::snprintf(msg, sizeof(msg),
                  "L1: Apply() row %d is the matrix-times-illuminant product "
                  "(got %.6f, want %.6f)", r, (double)got[r], (double)want[r]);
    check(nearly(got[r], want[r]), msg);
  }
}

// ---------------------------------------------------------------------------
// Fixture: an in-memory NamedColor array whose spectral member is a sparse matrix.
// ---------------------------------------------------------------------------

// Owns the tag array and exposes its CIccArrayNamedColor handler.
class SparseNamedColorFixture
{
public:
  SparseNamedColorFixture()
    : m_pArrTag(NULL), m_pArr(NULL), m_entryFn(fullMatrixA), m_zeroFn(fullMatrixZero) {}
  ~SparseNamedColorFixture() { delete m_pArrTag; }

  typedef void (*MatrixFn)(icFloatNumber *);

  // bSparse selects the member type; nChannelsOverride, when non-zero, breaks the
  // GetChannelsPerMatrix() invariant.  entryFn/zeroFn choose which matrices are authored.
  bool build(bool bSparse, icUInt16Number nChannelsOverride = 0,
             MatrixFn entryFn = fullMatrixA, MatrixFn zeroFn = fullMatrixZero)
  {
    m_entryFn = entryFn;
    m_zeroFn = zeroFn;

    m_pArrTag = new CIccTagArray(icSigNamedColorArray);
    // AttachTag() indexes m_TagVals directly, which stays NULL until the array is sized.
    if (!m_pArrTag->SetSize(2))
      return false;

    if (!m_pArrTag->AttachTag(0, newEntry(icSigTintZeroStruct, NULL, bSparse,
                                          nChannelsOverride, /*bZero=*/true)))
      return false;
    if (!m_pArrTag->AttachTag(1, newEntry(icSigNamedColorStruct, "Probe", bSparse,
                                          nChannelsOverride, /*bZero=*/false)))
      return false;

    m_pArr = (CIccArrayNamedColor *)m_pArrTag->GetArrayHandler();
    if (!m_pArr)
      return false;

    icSpectralRange spectralRange;
    icSpectralRange biSpectralRange;
    std::memset(&spectralRange, 0, sizeof(spectralRange));
    std::memset(&biSpectralRange, 0, sizeof(biSpectralRange));
    spectralRange.start = icRange400nm;
    spectralRange.end = icRange700nm;
    spectralRange.steps = kRows;
    biSpectralRange.start = icRange400nm;
    biSpectralRange.end = icRange700nm;
    biSpectralRange.steps = kCols;

    // The spectral PCS signature carries the sample count icGetSpaceSamples() reports,
    // which is what GetTint() is handed as nSamples -- so it has to agree with how the
    // member stores one entry.  These are the two real shapes in Testing/Named:
    // sparse (SparseMatrixNamedColor, sm0300 -> 768 words per encoded matrix) declares
    // the storage budget, while dense (FluorescentNamedColor, bs04f7 -> 1271 == 31*41)
    // declares the full grid.
    const icColorSpaceSignature csSpectral =
        bSparse ? icNColorSpaceSig(icSigSparseMatrixReflectanceData, kChannels)
                : icNColorSpaceSig(icSigBiSpectralReflectanceData, kRows * kCols);

    m_pArr->SetColorSpaces(icSigXYZData, icSigCmykData, csSpectral,
                           &spectralRange, &biSpectralRange);

    return m_pArr->Begin();
  }

  CIccArrayNamedColor *array() const { return m_pArr; }

  CIccStructNamedColor *color() const
  {
    return (CIccStructNamedColor *)icGetTagStructHandlerOfType(m_pArrTag->GetIndex(1),
                                                               icSigNamedColorStruct);
  }

private:
  CIccTag *newEntry(icStructSignature sigStruct, const char *szName, bool bSparse,
                    icUInt16Number nChannelsOverride, bool bZero)
  {
    CIccTagStruct *pStruct = new CIccTagStruct();
    pStruct->SetTagStructType(sigStruct);

    if (szName) {
      CIccTagUtf8Text *pName = new CIccTagUtf8Text();
      pName->SetText(szName);
      pStruct->AttachElem(icSigNmclNameMbr, pName);
    }

    icFloatNumber full[kRows * kCols];
    if (bZero)
      m_zeroFn(full);
    else
      m_entryFn(full);

    if (bSparse) {
      const icUInt16Number nChannels = nChannelsOverride ? nChannelsOverride : kChannels;
      CIccTagSparseMatrixArray *pMtx = new CIccTagSparseMatrixArray(1, nChannels);
      pMtx->SetMatrixType(icSparseMatrixFloatNum);

      CIccSparseMatrix mtx;
      if (pMtx->GetSparseMatrix(mtx, 0, /*bInitFromData=*/false) &&
          mtx.Init(kRows, kCols, /*bSetData=*/true))
        mtx.FillFromFullMatrix(full);

      pStruct->AttachElem(icSigNmclSpectralDataMbr, pMtx);
    }
    else {
      // The control: a plain float32 array of kRows*kCols samples, the shape every
      // other spectral named colour in the corpus uses.
      CIccTagFloat32 *pVals = new CIccTagFloat32(kRows * kCols);
      for (int i = 0; i < (int)(kRows * kCols); i++)
        (*pVals)[i] = full[i];
      pStruct->AttachElem(icSigNmclSpectralDataMbr, pVals);
    }

    return pStruct;
  }

  CIccTagArray *m_pArrTag;
  CIccArrayNamedColor *m_pArr;
  MatrixFn m_entryFn;
  MatrixFn m_zeroFn;
};

// ---------------------------------------------------------------------------
// Level 2: GetSpectralTint() must hand back the encoded matrix, and that blob must
// multiply out to the same answer the oracle computes.
// ---------------------------------------------------------------------------

void spectralTintYieldsAnEncodedMatrixThatMultipliesCorrectly()
{
  SparseNamedColorFixture fx;
  if (!fx.build(/*bSparse=*/true)) {
    check(false, "L2: fixture built");
    return;
  }

  std::vector<icFloatNumber> blob(kChannels, 0.0f);

  // This is the assertion the bug fails: GetTint() computed nEntries as
  // GetNumValues()/nSamples == 1/64 == 0 and returned false before touching the data.
  const bool got = fx.array()->GetSpectralTint(&blob[0], fx.color(), 1.0f,
                                               icSigNmclSpectralDataMbr);
  check(got, "L2: GetSpectralTint() succeeds for a sparseMatrixArrayType member");
  if (!got)
    return;

  const icFloatNumber kIllum[kCols] = { 1.0f, 0.5f, 2.0f };

  CIccPcsStepSrcSparseMatrix step(kRows, kCols, kChannels);
  std::memcpy(step.data(), kIllum, sizeof(kIllum));

  icFloatNumber out[kRows];
  for (int i = 0; i < (int)kRows; i++)
    out[i] = -1.0f;
  step.Apply(NULL, out, &blob[0]);

  icFloatNumber full[kRows * kCols];
  fullMatrixA(full);
  icFloatNumber want[kRows];
  denseMultiply(want, full, kIllum);

  for (int r = 0; r < (int)kRows; r++) {
    char msg[200];
    std::snprintf(msg, sizeof(msg),
                  "L2: the matrix GetSpectralTint() returned multiplies out correctly at "
                  "row %d (got %.6f, want %.6f)", r, (double)out[r], (double)want[r]);
    check(nearly(out[r], want[r]), msg);
  }
}

// ---------------------------------------------------------------------------
// Level 3: the tint axis.  With no 'tint' member the position is the tint itself, and
// Interpolate() blends the tint-zero matrix with the entry's matrix.
// ---------------------------------------------------------------------------

void tintZeroAndTintOneSelectTheTwoAuthoredMatrices()
{
  SparseNamedColorFixture fx;
  if (!fx.build(/*bSparse=*/true)) {
    check(false, "L3: fixture built");
    return;
  }

  const icFloatNumber kIllum[kCols] = { 1.0f, 1.0f, 1.0f };

  struct Case {
    icFloatNumber tint;
    void (*full)(icFloatNumber *);
    const char *what;
  } cases[] = {
    { 0.0f, fullMatrixZero, "tint 0 reproduces the tint-zero matrix" },
    { 1.0f, fullMatrixA,    "tint 1 reproduces the entry's own matrix" },
  };

  for (int c = 0; c < 2; c++) {
    std::vector<icFloatNumber> blob(kChannels, 0.0f);
    if (!fx.array()->GetSpectralTint(&blob[0], fx.color(), cases[c].tint,
                                     icSigNmclSpectralDataMbr)) {
      char msg[160];
      std::snprintf(msg, sizeof(msg), "L3: GetSpectralTint() succeeds (%s)",
                    cases[c].what);
      check(false, msg);
      continue;
    }

    CIccPcsStepSrcSparseMatrix step(kRows, kCols, kChannels);
    std::memcpy(step.data(), kIllum, sizeof(kIllum));

    icFloatNumber out[kRows];
    for (int i = 0; i < (int)kRows; i++)
      out[i] = -1.0f;
    step.Apply(NULL, out, &blob[0]);

    icFloatNumber full[kRows * kCols];
    cases[c].full(full);
    icFloatNumber want[kRows];
    denseMultiply(want, full, kIllum);

    for (int r = 0; r < (int)kRows; r++) {
      char msg[220];
      std::snprintf(msg, sizeof(msg), "L3: %s, row %d (got %.6f, want %.6f)",
                    cases[c].what, r, (double)out[r], (double)want[r]);
      check(nearly(out[r], want[r]), msg);
    }
  }
}

// ---------------------------------------------------------------------------
// Level 3b: two matrices with DIFFERENT sparsity patterns.  This is the case a real
// profile always presents, and it drives the parts of CIccSparseMatrix::Interp() that
// interpolating two identically-shaped matrices never reaches: the two single-sided
// tails, the whole-row copy when one side's row is empty, and a per-row entry count
// taken from the second matrix's own row offsets.
// ---------------------------------------------------------------------------

void interpolatingDifferentSparsityPatternsBlendsBothMatrices()
{
  SparseNamedColorFixture fx;
  if (!fx.build(/*bSparse=*/true, /*nChannelsOverride=*/0,
                fullMatrixDiffEntry, fullMatrixDiffZero)) {
    check(false, "L3b: differing-pattern fixture built");
    return;
  }

  const icFloatNumber kIllum[kCols] = { 1.0f, 2.0f, 0.5f };

  // Endpoints, then the midpoint -- the midpoint is the only tint that requires both
  // matrices to have been merged correctly rather than one of them simply copied.
  const icFloatNumber kTints[3] = { 0.0f, 1.0f, 0.5f };

  for (int c = 0; c < 3; c++) {
    const icFloatNumber tint = kTints[c];

    std::vector<icFloatNumber> blob(kChannels, 0.0f);
    if (!fx.array()->GetSpectralTint(&blob[0], fx.color(), tint,
                                     icSigNmclSpectralDataMbr)) {
      char msg[160];
      std::snprintf(msg, sizeof(msg),
                    "L3b: GetSpectralTint() succeeds at tint %.2f", (double)tint);
      check(false, msg);
      continue;
    }

    CIccPcsStepSrcSparseMatrix step(kRows, kCols, kChannels);
    std::memcpy(step.data(), kIllum, sizeof(kIllum));

    icFloatNumber out[kRows];
    for (int i = 0; i < (int)kRows; i++)
      out[i] = -1.0f;
    step.Apply(NULL, out, &blob[0]);

    // The oracle blends the two dense matrices, then multiplies.
    icFloatNumber entry[kRows * kCols];
    icFloatNumber zero[kRows * kCols];
    fullMatrixDiffEntry(entry);
    fullMatrixDiffZero(zero);

    icFloatNumber blend[kRows * kCols];
    for (int i = 0; i < (int)(kRows * kCols); i++)
      blend[i] = (1.0f - tint) * zero[i] + tint * entry[i];

    icFloatNumber want[kRows];
    denseMultiply(want, blend, kIllum);

    for (int r = 0; r < (int)kRows; r++) {
      char msg[220];
      std::snprintf(msg, sizeof(msg),
                    "L3b: differing patterns blend correctly at tint %.2f, row %d "
                    "(got %.6f, want %.6f)",
                    (double)tint, r, (double)out[r], (double)want[r]);
      check(nearly(out[r], want[r]), msg);
    }
  }
}

// ---------------------------------------------------------------------------
// Level 4: controls.
// ---------------------------------------------------------------------------

// Without this, a "fix" that simply stopped dividing by nSamples for every array type
// would pass levels 2 and 3 while breaking every spectral named colour in the corpus.
void plainFloatSpectralMemberStillWorks()
{
  SparseNamedColorFixture fx;
  if (!fx.build(/*bSparse=*/false)) {
    check(false, "L4: plain-array fixture built");
    return;
  }

  std::vector<icFloatNumber> vals(kRows * kCols, -1.0f);
  const bool got = fx.array()->GetSpectralTint(&vals[0], fx.color(), 1.0f,
                                               icSigNmclSpectralDataMbr);
  check(got, "L4 control: GetSpectralTint() still succeeds for a float32 member");
  if (!got)
    return;

  icFloatNumber full[kRows * kCols];
  fullMatrixA(full);
  for (int i = 0; i < (int)(kRows * kCols); i++) {
    char msg[190];
    std::snprintf(msg, sizeof(msg),
                  "L4 control: float32 member sample %d round-trips (got %.6f, want %.6f)",
                  i, (double)vals[i], (double)full[i]);
    check(nearly(vals[i], full[i]), msg);
  }
}

// CIccArrayNamedColor::Validate() treats GetChannelsPerMatrix() != m_nSpectralSamples as
// a critical error.  GetTint() must refuse the same mismatch rather than hand
// Interpolate() a byte count the blob cannot satisfy.
void channelsPerMatrixMustMatchTheSpectralSampleCount()
{
  SparseNamedColorFixture fx;
  // Half the channels the PCS signature declares: the encoded matrix still fits, so only
  // an explicit invariant check can catch this.
  if (!fx.build(/*bSparse=*/true, /*nChannelsOverride=*/(icUInt16Number)(kChannels / 2))) {
    check(false, "L4: mismatched-channel fixture built");
    return;
  }

  std::vector<icFloatNumber> blob(kChannels, 0.0f);
  check(!fx.array()->GetSpectralTint(&blob[0], fx.color(), 1.0f,
                                     icSigNmclSpectralDataMbr),
        "L4 control: a matrix array whose channel count disagrees with the spectral PCS "
        "sample count is refused");
}

} // namespace

int main()
{
  applyMultipliesEncodedSourceMatrixByTheStepsIlluminant();
  spectralTintYieldsAnEncodedMatrixThatMultipliesCorrectly();
  tintZeroAndTintOneSelectTheTwoAuthoredMatrices();
  interpolatingDifferentSparsityPatternsBlendsBothMatrices();
  plainFloatSpectralMemberStillWorks();
  channelsPerMatrixMustMatchTheSpectralSampleCount();

  if (g_fail)
    std::fprintf(stderr, "[nmcl-sparse] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[nmcl-sparse] all assertions passed\n");

  return g_fail;
}
