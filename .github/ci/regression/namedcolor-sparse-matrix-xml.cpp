// Companion to iccdev.namedcolor-sparse-matrix-apply, on the real corpus fixtures.
//
// That test builds its sparse-matrix NamedColor array in memory, deliberately, so a
// corpus change cannot disable it.  This one covers the profile the defect was actually
// found in -- Testing/Named/SparseMatrixNamedColor -- which the in-memory test cannot
// speak for: its matrices are authored by hand and hold 33, 269 and 340 stored entries
// against a tint-zero matrix of 31; its two spectral ranges differ (spectralRange
// 400-700/31 against biSpectralRange 300-700/41, so the illuminant is range-mapped
// rather than copied); and its members are stored in the quantised icSparseMatrixUInt8
// and icSparseMatrixUInt16 encodings rather than icSparseMatrixFloatNum.
//
// Testing/**/*.icc is gitignored -- the profiles are generated from XML by
// CreateAllProfiles.sh -- so the tracked artifacts are the XML documents, and every
// input here is parsed through the same CIccProfileXml::LoadXml path iccFromXml uses,
// the way iccdev.v2-xml-fixtures does.  That also pins the fixtures against drift: a
// document that stopped declaring a sparse-matrix spectral PCS, or whose ranges stopped
// differing, or which lost its 50% tint matrix, would keep parsing and validating while
// quietly restoring the coverage gap.
//
// The chain is the one Testing/RunTests.sh already runs for the *dense* bi-spectral
// FluorescentNamedColor: named colour -> PCC override -> six-channel spectral camera.
// Building the whole chain is what makes this test exercise
// CIccPcsStepSrcSparseMatrix::Apply(): with only the named profile attached there is no
// PCS connection at all, and the named transform hands back its raw encoded matrix
// rather than a spectral vector.
//
// What is asserted:
//
//   1. The fixtures are still shaped the way this test needs: v5 nmcl-class profiles
//      whose spectralPCS is a sparse-matrix type with differing ranges, one carrying a
//      single matrix per colour and one carrying two.
//   2. Every colour applies through the whole CMM.  This is the end-to-end regression:
//      before the fix CIccCmm::Begin() succeeded and then every Apply() returned
//      icCmmStatBadTintXform, because CIccStructNamedColor::GetTint() divided a matrix
//      count by a sample count and bailed.
//   3. Output is finite and differs between colours, so a "fix" that reported success
//      while writing nothing, or returned one colour for every name, cannot pass.
//   4. At tint 0 every colour resolves to the *same* values, because the profile carries
//      exactly one 'tnt0' structure and tint 0 selects it outright; at tint 1 each
//      differs from that.  This pins CIccSparseMatrix::Interp() on real data: all three
//      colours merge against the same tint-zero matrix from three different sparsity
//      patterns, and getting one answer out of three differently-shaped merges is only
//      possible if the row offsets, the per-row entry counts and both single-sided tails
//      are all right.
//   5. The equivalence that motivates the second fixture.  SparseMatrixNamedColorTint
//      is SparseMatrixNamedColor with one extra matrix per colour, authored as the
//      midpoint of that colour's tint-zero and full-tint matrices.  Interpolating toward
//      an authored midpoint that *is* the midpoint must produce the same answer as
//      interpolating past it, at every tint -- so the whole tint axis of the two profiles
//      must agree.  This is a strong check on the multi-matrix path:
//      CIccTagSparseMatrixArray::Interpolate() takes a different branch for a two-matrix
//      member (it brackets the requested position between two stored matrices rather than
//      between the tint-zero entry and the only one), and that branch had never executed.
//
//      The midpoint carries one deliberate exception, described in 6: a single
//      off-diagonal probe entry of 1e-4.  One cell of a 31x41 matrix, weighted by one
//      illuminant sample, moves a camera channel by ~1e-6 -- two orders below kTol -- so
//      the endpoints stay exact and the interior agrees well within tolerance.
//   6. That probe entry, and the reason for it.  Union() -- reached only from
//      CIccTagSparseMatrixArray::Validate(), which merges each consecutive pair of
//      matrices in a multi-matrix sparse tag -- is only ever wrong when the two matrices
//      carry DIFFERENT sparsity patterns.  A pure midpoint would inherit its sibling's
//      pattern exactly, because tnt0's non-zeros are a subset of every colour's, and the
//      whole of Union() would stay unreached.  The probe entry sits just below an
//      existing non-zero, where a Donaldson matrix carries fluorescence anyway, and gives
//      the 50% matrix one stored entry its sibling lacks.  Validating this fixture
//      against unfixed sources does not return (CWE-835); a ctest TIMEOUT reports that.
//
// Takes the fixture paths as argv[1..4]; the CMake registration names them.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccArrayBasic.h"
#include "IccCmm.h"
#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccSparseMatrix.h"
#include "IccStructBasic.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"
#include "IccTagXmlFactory.h"
#include "IccUtil.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[nmcl-sparse-xml] FAIL: %s\n", what);
  }
}

bool isFinite(icFloatNumber v)
{
  // Deliberately not std::isfinite: NaN is the value a mis-indexed sparse entry most
  // often produces here, and this rejects it without depending on the compiler's
  // floating point model.
  return (v == v) && (v > -1.0e30f) && (v < 1.0e30f);
}

// Heap-allocated: most callers hand the result to a CMM that takes ownership.
CIccProfileXml *loadXml(const char *szXml)
{
  CIccProfileXml *pProfile = new CIccProfileXml();
  std::string parseStr;

  if (!pProfile->LoadXml(szXml, NULL, &parseStr)) {
    std::fprintf(stderr, "[nmcl-sparse-xml] FAIL: could not parse %s\n%s\n",
                 szXml, parseStr.c_str());
    delete pProfile;
    return NULL;
  }

  return pProfile;
}

// How many matrices the first named colour's spectral member holds; 0 if the profile is
// not shaped the way this test expects.  This is the anti-vacuity guard for level 5: if
// the 50% fixture ever lost its midpoint matrix the two profiles would agree trivially
// and prove nothing.
icUInt32Number matricesPerColour(CIccProfile *pProfile)
{
  CIccTag *pTag = pProfile->FindTag(icSigNamedColor2Tag);

  // Not IsArrayType(): CIccTagArray does not override it, so it inherits CIccTag's
  // "false" and would reject every tag array there is.  The type signature is what
  // identifies one.
  if (!pTag || pTag->GetType() != icSigTagArrayType)
    return 0;

  CIccTagArray *pArrTag = (CIccTagArray *)pTag;
  for (icUInt32Number i = 0; i < pArrTag->GetSize(); i++) {
    CIccStructNamedColor *pColor =
      (CIccStructNamedColor *)icGetTagStructHandlerOfType(pArrTag->GetIndex(i),
                                                          icSigNamedColorStruct);
    if (!pColor)
      continue;

    CIccTagNumArray *pNum = pColor->GetNumArray(icSigNmclSpectralDataMbr);
    if (!pNum || !pNum->IsMatrixArray())
      return 0;

    return ((CIccTagSparseMatrixArray *)pNum)->GetNumMatrices();
  }

  return 0;
}

// Stored-entry counts of the first named colour's spectral matrices, in order; empty if
// the profile is not shaped the way this test expects.  Used by level 6 to confirm the
// two matrices really do have different sparsity patterns.
std::vector<icUInt16Number> matrixEntryCounts(CIccProfile *pProfile)
{
  std::vector<icUInt16Number> counts;

  CIccTag *pTag = pProfile->FindTag(icSigNamedColor2Tag);
  if (!pTag || pTag->GetType() != icSigTagArrayType)
    return counts;

  CIccTagArray *pArrTag = (CIccTagArray *)pTag;
  for (icUInt32Number i = 0; i < pArrTag->GetSize(); i++) {
    CIccStructNamedColor *pColor =
      (CIccStructNamedColor *)icGetTagStructHandlerOfType(pArrTag->GetIndex(i),
                                                          icSigNamedColorStruct);
    if (!pColor)
      continue;

    CIccTagNumArray *pNum = pColor->GetNumArray(icSigNmclSpectralDataMbr);
    if (!pNum || !pNum->IsMatrixArray())
      return counts;

    CIccTagSparseMatrixArray *pMtxArr = (CIccTagSparseMatrixArray *)pNum;
    for (icUInt32Number m = 0; m < pMtxArr->GetNumMatrices(); m++) {
      CIccSparseMatrix mtx;
      if (!pMtxArr->GetSparseMatrix(mtx, (int)m, /*bInitFromData=*/true)) {
        counts.clear();
        return counts;
      }
      counts.push_back(mtx.GetNumEntries());
    }
    return counts;
  }

  return counts;
}

const char *const kColors[] = { "Brown", "Neon-Orange", "Neon-Yellow" };
const int kNumColors = (int)(sizeof(kColors) / sizeof(kColors[0]));

// Tolerance on the camera channels.  Generous relative to the exactness the merge
// actually achieves, because these comparisons are about which matrix was selected, not
// about the last bit of a quantised encoding.
const icFloatNumber kTol = 1.0e-4f;

bool differs(const std::vector<icFloatNumber> &a, const std::vector<icFloatNumber> &b)
{
  if (a.size() != b.size() || a.empty())
    return true;

  for (size_t i = 0; i < a.size(); i++) {
    const icFloatNumber d = a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
    if (d > kTol)
      return true;
  }
  return false;
}

// Apply one colour at one tint through a freshly built chain.  A fresh CMM per
// measurement: CIccNamedColorCmm::Apply() is stateful across the chain it holds, and
// reusing one would leave this unable to tell a per-colour result from a leftover one.
//
// Returns false (having reported) if anything up to and including Apply() failed.
bool applyOnce(const char *szNamedXml, const char *szPccXml, const char *szDstXml,
               const char *szColor, icFloatNumber tint,
               std::vector<icFloatNumber> &out, const char *szWhich)
{
  char msg[260];

  CIccProfileXml *pNamed = loadXml(szNamedXml);
  CIccProfileXml *pPcc   = loadXml(szPccXml);
  CIccProfileXml *pDst   = loadXml(szDstXml);
  if (!pNamed || !pPcc || !pDst) {
    delete pNamed;
    delete pPcc;
    delete pDst;
    check(false, "every fixture parsed");
    return false;
  }

  CIccNamedColorCmm cmm(icSigUnknownData, icSigUnknownData, true);

  // pPcc is a connection-conditions override rather than a chain member, so the CMM does
  // not take it over: it has to outlive the chain and be released here.
  icStatusCMM stat = cmm.AddXform(pNamed, icAbsoluteColorimetric, icInterpLinear,
                                  pPcc, icXformLutColor, true);
  if (stat != icCmmStatOk) {
    // Not deleted: this overload is documented "profile will be owned by the CMM", and
    // whether that transfer has happened when it reports a failure is unspecified.
    std::snprintf(msg, sizeof(msg),
                  "[%s] AddXform accepts the sparse-matrix named colour profile "
                  "(status %d)", szWhich, (int)stat);
    check(false, msg);
    delete pPcc;
    delete pDst;
    return false;
  }

  stat = cmm.AddXform(pDst, icRelativeColorimetric, icInterpLinear, NULL,
                      icXformLutColor, true);
  if (stat != icCmmStatOk) {
    std::snprintf(msg, sizeof(msg),
                  "[%s] AddXform accepts the destination profile (status %d)",
                  szWhich, (int)stat);
    check(false, msg);
    delete pPcc;
    return false;
  }

  // Begin() succeeded before the fix as well -- the sparse PCS step was built and held
  // the range-mapped illuminant.  It is Apply() that could not reach it.
  stat = cmm.Begin();
  if (stat != icCmmStatOk) {
    std::snprintf(msg, sizeof(msg),
                  "[%s] Begin builds the sparse-matrix spectral chain (status %d)",
                  szWhich, (int)stat);
    check(false, msg);
    delete pPcc;
    return false;
  }

  const icUInt32Number nDst = cmm.GetDestSamples();
  if (!nDst) {
    std::snprintf(msg, sizeof(msg), "[%s] the chain reports a destination sample count",
                  szWhich);
    check(false, msg);
    delete pPcc;
    return false;
  }

  out.assign(nDst, -1.0f);

  // THE regression: pre-fix this was icCmmStatBadTintXform for every colour.
  stat = cmm.Apply(&out[0], szColor, tint);
  std::snprintf(msg, sizeof(msg), "[%s] Apply(\"%s\", tint %.2f) succeeds (status %d)",
                szWhich, szColor, (double)tint, (int)stat);
  check(stat == icCmmStatOk, msg);

  delete pPcc;

  if (stat != icCmmStatOk) {
    out.clear();
    return false;
  }

  return true;
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 5) {
    std::fprintf(stderr,
                 "usage: %s <SparseMatrixNamedColor.xml> "
                 "<SparseMatrixNamedColorTint.xml> <pcc.xml> <destination.xml>\n",
                 argv[0] ? argv[0] : "namedcolor-sparse-matrix-xml");
    return 2;
  }

  const char *szBaseXml = argv[1];
  const char *szTintXml = argv[2];
  const char *szPccXml  = argv[3];
  const char *szDstXml  = argv[4];

  // iccFromXml pushes these before parsing; without them the XML tag types resolve to
  // their non-XML base classes and ParseXml is never reached.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  // ---- 1. the fixtures are still shaped the way this test needs ---------------------
  //
  // Checked on profiles of their own, before any CMM can take ownership of one.
  {
    struct Case { const char *szXml; const char *szWhich; icUInt32Number nMatrices; };
    const Case kCases[2] = {
      { szBaseXml, "base", 1 },
      { szTintXml, "tint", 2 },
    };

    for (int c = 0; c < 2; c++) {
      CIccProfileXml *pCheck = loadXml(kCases[c].szXml);
      if (!pCheck)
        return 1;

      char msg[260];

      const icUInt32Number major = (pCheck->m_Header.version >> 24) & 0xff;
      std::snprintf(msg, sizeof(msg), "[%s] is still a v5-or-later profile",
                    kCases[c].szWhich);
      check(major >= 0x05, msg);

      std::snprintf(msg, sizeof(msg), "[%s] is still an nmcl (NamedColor) class profile",
                    kCases[c].szWhich);
      check(pCheck->m_Header.deviceClass == icSigNamedColorClass, msg);

      std::snprintf(msg, sizeof(msg), "[%s] still declares a sparse-matrix spectral PCS",
                    kCases[c].szWhich);
      check(icGetColorSpaceType(pCheck->m_Header.spectralPCS) ==
                icSigSparseMatrixSpectralPcsData, msg);

      std::snprintf(msg, sizeof(msg),
                    "[%s] still has differing spectral ranges, so the illuminant is "
                    "range-mapped rather than copied (spectralRange %u steps, "
                    "biSpectralRange %u steps)", kCases[c].szWhich,
                    (unsigned)pCheck->m_Header.spectralRange.steps,
                    (unsigned)pCheck->m_Header.biSpectralRange.steps);
      check(pCheck->m_Header.spectralRange.steps &&
                pCheck->m_Header.biSpectralRange.steps &&
                !icSameSpectralRange(pCheck->m_Header.spectralRange,
                                     pCheck->m_Header.biSpectralRange), msg);

      // The guard that keeps level 5 honest.
      const icUInt32Number nGot = matricesPerColour(pCheck);
      std::snprintf(msg, sizeof(msg),
                    "[%s] still carries %u matri%s per colour (got %u) -- level 5 is "
                    "vacuous if these two stop differing", kCases[c].szWhich,
                    (unsigned)kCases[c].nMatrices,
                    kCases[c].nMatrices == 1 ? "x" : "ces", (unsigned)nGot);
      check(nGot == kCases[c].nMatrices, msg);

      delete pCheck;
    }
  }

  // The tint axis.  0 and 1 are the endpoints the fixtures author outright; 0.5 is the
  // midpoint the second fixture authors and the first interpolates; 0.25 and 0.75 fall
  // inside a different bracketing pair in each fixture, which is the interesting part.
  const icFloatNumber kTints[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
  const int kNumTints = (int)(sizeof(kTints) / sizeof(kTints[0]));

  std::vector< std::vector<icFloatNumber> > atZero(kNumColors);
  std::vector< std::vector<icFloatNumber> > atFull(kNumColors);

  // ---- 2/3/4/5 ----------------------------------------------------------------------
  for (int c = 0; c < kNumColors; c++) {
    for (int t = 0; t < kNumTints; t++) {
      const icFloatNumber tint = kTints[t];
      char msg[260];

      std::vector<icFloatNumber> base;
      std::vector<icFloatNumber> tinted;

      const bool bBase =
        applyOnce(szBaseXml, szPccXml, szDstXml, kColors[c], tint, base, "base");
      const bool bTint =
        applyOnce(szTintXml, szPccXml, szDstXml, kColors[c], tint, tinted, "tint");

      if (bBase) {
        // 3. Finite and not the all-zero vector a silently empty matrix would produce.
        bool bAllFinite = true;
        bool bAnyNonZero = false;
        for (size_t i = 0; i < base.size(); i++) {
          if (!isFinite(base[i]))
            bAllFinite = false;
          if (base[i] > 1.0e-6f || base[i] < -1.0e-6f)
            bAnyNonZero = true;
        }

        std::snprintf(msg, sizeof(msg),
                      "every output sample of \"%s\" at tint %.2f is finite",
                      kColors[c], (double)tint);
        check(bAllFinite, msg);

        std::snprintf(msg, sizeof(msg),
                      "\"%s\" at tint %.2f is not the all-zero vector a silently empty "
                      "matrix would produce", kColors[c], (double)tint);
        check(bAnyNonZero, msg);

        if (tint == 0.0f)
          atZero[c] = base;
        else if (tint == 1.0f)
          atFull[c] = base;
      }

      // 5. The equivalence: an authored midpoint that IS the midpoint changes nothing.
      if (bBase && bTint) {
        std::snprintf(msg, sizeof(msg),
                      "\"%s\" at tint %.2f agrees between the one-matrix and the "
                      "two-matrix fixture, so authoring the 50%% matrix at the midpoint "
                      "does not move the tint axis", kColors[c], (double)tint);
        check(!differs(base, tinted), msg);
      }
    }
  }

  // 4a. Tint must select between the tint-zero matrix and the colour's own.
  for (int c = 0; c < kNumColors; c++) {
    if (atFull[c].empty() || atZero[c].empty())
      continue;

    char msg[240];
    std::snprintf(msg, sizeof(msg),
                  "\"%s\" at tint 1 differs from tint 0, so the tint-zero entry is "
                  "blended rather than substituted for the colour", kColors[c]);
    check(differs(atFull[c], atZero[c]), msg);
  }

  // 4b. The profile carries exactly one 'tnt0' structure, so tint 0 is that matrix for
  // every colour -- reached through three different sparsity patterns.
  for (int c = 1; c < kNumColors; c++) {
    if (atZero[0].empty() || atZero[c].empty())
      continue;

    char msg[260];
    std::snprintf(msg, sizeof(msg),
                  "\"%s\" and \"%s\" at tint 0 agree, so merging the single 'tnt0' "
                  "matrix against differently-shaped colour matrices gives one answer",
                  kColors[0], kColors[c]);
    check(!differs(atZero[0], atZero[c]), msg);
  }

  // 3 (continued). Different colours must give different answers at full tint.
  for (int a = 0; a < kNumColors; a++) {
    for (int b = a + 1; b < kNumColors; b++) {
      if (atFull[a].empty() || atFull[b].empty())
        continue;

      char msg[220];
      std::snprintf(msg, sizeof(msg), "\"%s\" and \"%s\" apply to different values",
                    kColors[a], kColors[b]);
      check(differs(atFull[a], atFull[b]), msg);
    }
  }

  // ---- 6. the 50% fixture must keep exercising CIccSparseMatrix::Union() ------------
  //
  // Union()'s only caller is CIccTagSparseMatrixArray::Validate(), which merges each
  // consecutive pair of matrices in a multi-matrix sparse tag -- and it is only ever
  // wrong when the two carry different sparsity patterns.  A midpoint matrix would
  // inherit its sibling's pattern exactly (tnt0's non-zeros are a subset of every
  // colour's), so the fixture carries one deliberate off-diagonal probe entry, sized to
  // change the pattern without moving a camera channel measurably.
  //
  // Against unfixed sources the Validate() call below does not return (CWE-835), so a
  // ctest TIMEOUT is what reports this.  The entry-count assertion in front of it is
  // what stops the level going quiet if the probe entry is ever dropped.
  {
    CIccProfileXml *pTint = loadXml(szTintXml);
    if (pTint) {
      const std::vector<icUInt16Number> counts = matrixEntryCounts(pTint);

      char msg[300];
      std::snprintf(msg, sizeof(msg),
                    "the 50%% fixture's first colour carries two spectral matrices "
                    "(got %u)", (unsigned)counts.size());
      check(counts.size() == 2, msg);

      if (counts.size() == 2) {
        std::snprintf(msg, sizeof(msg),
                      "its two matrices have different sparsity patterns (%u vs %u "
                      "stored entries), without which Union() is never wrong and this "
                      "level proves nothing",
                      (unsigned)counts[0], (unsigned)counts[1]);
        check(counts[0] != counts[1], msg);
      }

      std::string report;
      pTint->Validate(report);

      check(true, "Validate() returns on the two-matrix fixture instead of hanging");

      delete pTint;
    }
  }

  if (g_fail)
    std::fprintf(stderr, "[nmcl-sparse-xml] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[nmcl-sparse-xml] all assertions passed\n");

  return g_fail;
}
