// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: CIccSparseMatrix::Union() never worked, and reaching it hung the validator.
//
// Union() is a near-verbatim copy of CIccSparseMatrix::Interp() and carries the same five
// defects the tint path exposed in Interp():
//
//   1. Init() is called without bSetData, so the result never gets the two leading words
//      that carry its row and column counts -- every reader recovers the shape from those,
//      so the union came back as a 0x0 matrix.
//   2. Each row's stored start is overwritten with its end (m_RowStart[r] where [r+1] is
//      meant), shifting every row's bracket by one and leaving m_RowStart[m_nRows] -- the
//      terminator GetNumEntries() reads -- never written.
//   3. The second matrix's per-row entry count is measured from the FIRST matrix's row
//      offset (mtx2.m_RowStart[r+1] - fA), which is only correct while the two sparsity
//      patterns are identical.
//   4. Neither single-sided merge tail advances i/j or the write cursor, so once one side
//      of a row runs out the while(i<nA || j<nB) condition can never change: an INFINITE
//      LOOP (CWE-835).
//   5. Both whole-row memcpy() calls pass an entry count where a byte count is required,
//      copying half the column indices and none of their upper bytes.
//
// Defect 4 is the serious one, because Union()'s only caller is
// CIccTagSparseMatrixArray::Validate(), which unions each consecutive pair of matrices in
// any sparseMatrixArrayType tag holding two or more.  Validation is reached from
// iccDumpProfile and the public validation API on untrusted input, so a profile carrying
// a two-matrix sparse tag whose matrices have different sparsity patterns -- which is the
// ordinary case, not a malformed one -- wedges the process rather than reporting anything.
//
// Nothing in the corpus triggered it: every sparseMatrixArrayType tag in Testing/ holds
// exactly one matrix, and with one matrix Validate() never enters the loop.  Defects 3-5
// are likewise invisible to any pair sharing a pattern, so even a two-matrix tag would
// have looked fine if its matrices matched.
//
// Levels:
//   1. Union() of two differently-patterned matrices terminates, is self-describing, and
//      holds exactly the union of the two column sets.  Union() writes 1.0 for every
//      entry it keeps -- it is a pattern operation, not an arithmetic one -- so the
//      expected result is a mask.
//   2. Rows where one side is empty, which take the whole-row copy path rather than the
//      merge loop, must carry the right column indices (defect 5).
//   3. CIccTagSparseMatrixArray::Validate() on a two-matrix tag returns instead of
//      hanging.  This is the denial-of-service regression; a ctest TIMEOUT is what
//      catches it against unfixed sources.
//   4. Control: two matrices sharing one pattern must still union correctly, so the fix
//      cannot pass by breaking the only case that used to work.
//
// Header + IccProfLib only, no fixture and no I/O.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccSparseMatrix.h"
#include "IccTagBasic.h"
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
    std::fprintf(stderr, "[sparse-union] FAIL: %s\n", what);
  }
}

// Deliberately not square, so a row/column transposition cannot pass.
const icUInt16Number kRows = 4;
const icUInt16Number kCols = 5;

// Words per matrix blob.  Large enough that capacity never limits these unions: the
// budget is (bytes - 4 - 4*(rows+1) - 3) / 6 entries, and the largest union here is 8.
const icUInt16Number kWords = 64;

// Pattern A and pattern B differ in every row, and each has one row the other fills:
//   row 0: A {0,3}      B {1,3}     -> merge with both sides live, and a shared column
//   row 1: A {}         B {0,4}     -> B-only whole-row copy
//   row 2: A {2,4}      B {}        -> A-only whole-row copy
//   row 3: A {0}        B {1,2,3}   -> merge whose A side runs out first (the tail)
const icFloatNumber kFullA[kRows * kCols] = {
  0.5f, 0.0f,  0.0f,  0.25f, 0.0f,
  0.0f, 0.0f,  0.0f,  0.0f,  0.0f,
  0.0f, 0.0f,  0.75f, 0.0f,  0.125f,
  0.375f, 0.0f, 0.0f, 0.0f,  0.0f,
};
const icFloatNumber kFullB[kRows * kCols] = {
  0.0f, 0.625f, 0.0f,  0.5f,  0.0f,
  0.25f, 0.0f,  0.0f,  0.0f,  0.875f,
  0.0f, 0.0f,   0.0f,  0.0f,  0.0f,
  0.0f, 0.125f, 0.25f, 0.5f,  0.0f,
};

// Author one encoded matrix into a caller-owned blob.  bSetData stamps the dimensions,
// which every reader needs.
bool encode(std::vector<icFloatNumber> &blob, const icFloatNumber *pFull)
{
  blob.assign(kWords, 0.0f);

  CIccSparseMatrix mtx(&blob[0], kWords * sizeof(icFloatNumber),
                       icSparseMatrixFloatNum, false);
  if (!mtx.Init(kRows, kCols, /*bSetData=*/true))
    return false;

  return mtx.FillFromFullMatrix(const_cast<icFloatNumber *>(pFull));
}

// Expand an encoded matrix back to a dense kRows x kCols array, reading it exactly the
// way a consumer does: dimensions from the blob, then rows bracketed by m_RowStart.
// Returns false if the blob does not describe a kRows x kCols matrix at all, which is
// what an unstamped header or a missing terminator looks like from outside.
bool expand(const std::vector<icFloatNumber> &blob, icFloatNumber *pDense)
{
  for (int i = 0; i < (int)(kRows * kCols); i++)
    pDense[i] = 0.0f;

  CIccSparseMatrix mtx(const_cast<icFloatNumber *>(&blob[0]),
                       kWords * sizeof(icFloatNumber), icSparseMatrixFloatNum,
                       /*bInitFromData=*/true);

  if (mtx.Rows() != kRows || mtx.Cols() != kCols)
    return false;

  for (icUInt16Number r = 0; r < kRows; r++) {
    const icUInt16Number nInRow = mtx.GetNumRowColumns(r);
    const icUInt16Number *pCols = mtx.GetColumnsForRow(r);
    const icUInt16Number nFirst = mtx.GetRowOffset(r);

    for (icUInt16Number k = 0; k < nInRow; k++) {
      const icUInt16Number c = pCols[k];
      if (c >= kCols)
        return false;
      pDense[r * kCols + c] = mtx.GetData()->get(nFirst + k);
    }
  }

  return true;
}

bool nearly(icFloatNumber a, icFloatNumber b)
{
  const icFloatNumber d = a > b ? a - b : b - a;
  return d <= 1.0e-5f;
}

// ---------------------------------------------------------------------------
// Levels 1 and 2: Union() semantics on differing patterns.
// ---------------------------------------------------------------------------

void unionOfDifferentPatternsIsTheMaskOfBothColumnSets()
{
  std::vector<icFloatNumber> blobA, blobB, blobU(kWords, 0.0f);

  check(encode(blobA, kFullA), "union: encoded matrix A");
  check(encode(blobB, kFullB), "union: encoded matrix B");

  CIccSparseMatrix mA(&blobA[0], kWords * sizeof(icFloatNumber),
                      icSparseMatrixFloatNum, true);
  CIccSparseMatrix mB(&blobB[0], kWords * sizeof(icFloatNumber),
                      icSparseMatrixFloatNum, true);
  CIccSparseMatrix mU(&blobU[0], kWords * sizeof(icFloatNumber),
                      icSparseMatrixFloatNum, false);

  // The caller in CIccTagSparseMatrixArray::Validate() initialises the destination
  // before calling Union(), so do the same.
  check(mU.Init(kRows, kCols, /*bSetData=*/true), "union: destination initialised");

  // Pre-fix this call does not return: both single-sided tails spin without advancing.
  check(mU.Union(mA, mB), "union: Union() of two differently-patterned matrices succeeds");

  icFloatNumber got[kRows * kCols];
  if (!expand(blobU, got)) {
    check(false, "union: the result describes its own dimensions, so it can be read back");
    return;
  }

  for (int r = 0; r < (int)kRows; r++) {
    for (int c = 0; c < (int)kCols; c++) {
      const int i = r * kCols + c;

      // Union() stores 1.0 for every column either input occupies: it records the
      // combined pattern, not a sum.
      const bool bWant = (kFullA[i] != 0.0f) || (kFullB[i] != 0.0f);
      const icFloatNumber want = bWant ? 1.0f : 0.0f;

      char msg[200];
      std::snprintf(msg, sizeof(msg),
                    "union: row %d column %d is %s (got %.6f, want %.6f)",
                    r, c, bWant ? "in the union" : "absent",
                    (double)got[i], (double)want);
      check(nearly(got[i], want), msg);
    }
  }
}

// ---------------------------------------------------------------------------
// Level 3: the denial of service.  Reached exactly as iccDumpProfile reaches it.
// ---------------------------------------------------------------------------

void validateOfATwoMatrixSparseTagTerminates()
{
  // The tag's own channel budget has to hold each matrix, and Validate() also unions
  // consecutive pairs into a scratch blob of the same size.
  CIccTagSparseMatrixArray tag(2, kWords);
  tag.SetMatrixType(icSparseMatrixFloatNum);

  const icFloatNumber *pFull[2] = { kFullA, kFullB };
  for (int i = 0; i < 2; i++) {
    CIccSparseMatrix mtx;
    if (!tag.GetSparseMatrix(mtx, i, /*bInitFromData=*/false) ||
        !mtx.Init(kRows, kCols, /*bSetData=*/true) ||
        !mtx.FillFromFullMatrix(const_cast<icFloatNumber *>(pFull[i]))) {
      check(false, "validate: two-matrix fixture authored");
      return;
    }
  }

  // Pre-fix this never returns, so the assertion below is unreachable and ctest reports
  // the test's TIMEOUT instead.  That is the regression: a two-matrix sparse tag with
  // differing patterns must not wedge the validator.
  std::string report;
  tag.Validate("sigPath", report, NULL);

  check(true, "validate: Validate() returns on a two-matrix sparse tag instead of hanging");
}

// ---------------------------------------------------------------------------
// Level 4: control.  A pair sharing one pattern is the only case that used to work.
// ---------------------------------------------------------------------------

void unionOfIdenticalPatternsStillWorks()
{
  std::vector<icFloatNumber> blobA, blobA2, blobU(kWords, 0.0f);

  check(encode(blobA, kFullA), "control: encoded matrix A");
  check(encode(blobA2, kFullA), "control: encoded matrix A again");

  CIccSparseMatrix mA(&blobA[0], kWords * sizeof(icFloatNumber),
                      icSparseMatrixFloatNum, true);
  CIccSparseMatrix mA2(&blobA2[0], kWords * sizeof(icFloatNumber),
                       icSparseMatrixFloatNum, true);
  CIccSparseMatrix mU(&blobU[0], kWords * sizeof(icFloatNumber),
                      icSparseMatrixFloatNum, false);

  check(mU.Init(kRows, kCols, /*bSetData=*/true), "control: destination initialised");
  check(mU.Union(mA, mA2), "control: Union() of one pattern with itself succeeds");

  icFloatNumber got[kRows * kCols];
  if (!expand(blobU, got)) {
    check(false, "control: the result describes its own dimensions");
    return;
  }

  for (int i = 0; i < (int)(kRows * kCols); i++) {
    const icFloatNumber want = (kFullA[i] != 0.0f) ? 1.0f : 0.0f;
    char msg[190];
    std::snprintf(msg, sizeof(msg),
                  "control: identical patterns union to the same mask at %d "
                  "(got %.6f, want %.6f)", i, (double)got[i], (double)want);
    check(nearly(got[i], want), msg);
  }
}

} // namespace

int main()
{
  unionOfDifferentPatternsIsTheMaskOfBothColumnSets();
  validateOfATwoMatrixSparseTagTerminates();
  unionOfIdenticalPatternsStillWorks();

  if (g_fail)
    std::fprintf(stderr, "[sparse-union] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[sparse-union] all assertions passed\n");

  return g_fail;
}
