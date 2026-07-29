// Regression for #1853 (ci-qa-flags harvest, bucket 4): the spectral viewing-conditions
// path computed colorimetry out of uninitialized heap.
//
// CIccMatrixMath::rangeMap() built the conversion matrix and then ignored what
// SetRange() said about it:
//
//     CIccMatrixMath *mtx = new CIccMatrixMath(dstRange.steps, srcRange.steps);
//     mtx->SetRange(srcRange, dstRange);      // return value discarded
//     return mtx;
//
// The constructor only writes m_vals when bInitIdentity is set, and SetRange() returns
// before its own memset() on every rejection path, so a rejected range pair produced a
// matrix whose every coefficient was whatever the allocator last left there. Callers
// multiplied that straight into a colour value. SetRange() rejects three ways a
// malformed profile can reach: either range carrying <= 1 step, a non-finite endpoint
// (an infinity passes the icNotZero(end - start) screen in icCanMapSpectralRange), and
// a zero-width source span. CIccTagSpectralViewingConditions::getObserverMatrix() and
// applyRangeToObserver() call into this with no screen at all.
//
// The sibling implementation, CIccPcsXform::rangeMap() in IccCmm.cpp, has always
// checked SetRange() and returned NULL. This one never got the same treatment.
//
// Returning NULL is the fix, but it also makes NULL ambiguous -- every caller read it
// as "the ranges are identical, so no conversion is needed" and then indexed one
// range's length into an array sized by the other. That is why the three-argument
// rangeMap() overload exists and why the callers are part of the same change: without
// their guards this fix converts an uninitialized read into an out-of-bounds one.
//
// Assertions here are red-green. On unfixed sources the rangeMap() rejection cases
// return a non-NULL matrix and getObserverMatrix() returns a part-stale one, so those
// checks fail. Header + IccProfLib only, no fixture, no I/O.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagBasic.h"
#include "IccMatrixMath.h"
#include "IccDefs.h"
#include "IccUtil.h"

#include <cstdio>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[rangemap-uninit] FAIL: %s\n", what);
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

// A range pair rangeMap() must refuse: it differs from dst (so a matrix is required)
// but SetRange() cannot build one. Refusing means NULL *and* pFailed set, so the
// caller can tell this apart from "no matrix needed".
void expectRejected(const icSpectralRange &src, const icSpectralRange &dst, const char *why)
{
  char msg[160];

  bool failed = false;
  CIccMatrixMath *mtx = CIccMatrixMath::rangeMap(src, dst, &failed);

  std::snprintf(msg, sizeof(msg), "%s: rangeMap -> NULL, not an uninitialized matrix", why);
  check(mtx == NULL, msg);

  std::snprintf(msg, sizeof(msg), "%s: pFailed set so callers do not read NULL as 'ranges match'", why);
  check(failed, msg);

  delete mtx;

  // The two-argument form is what the existing callers and any downstream consumer
  // use; it must carry the same corrected behaviour, not just the new overload.
  mtx = CIccMatrixMath::rangeMap(src, dst);
  std::snprintf(msg, sizeof(msg), "%s: two-argument rangeMap -> NULL as well", why);
  check(mtx == NULL, msg);
  delete mtx;
}

void testRangeMapRejections()
{
  // 1 step: SetRange() bails at "srcRange.steps <= 1" before it memsets the matrix.
  expectRejected(makeRange(icRange380nm, icRange780nm, 1),
                 makeRange(icRange380nm, icRange780nm, 5),
                 "source range with a single step");

  expectRejected(makeRange(icRange380nm, icRange780nm, 5),
                 makeRange(icRange380nm, icRange780nm, 1),
                 "destination range with a single step");

  // Zero-width source span: srcScale == 0, rejected after the steps checks.
  expectRejected(makeRange(icRange380nm, icRange380nm, 5),
                 makeRange(icRange380nm, icRange780nm, 9),
                 "zero-width source span");

  // 0x7C00 is +infinity as a half float. end - start is then infinite, which passes
  // icNotZero() but makes the interpolation scale non-finite.
  expectRejected(makeRange(icRange380nm, 0x7C00, 5),
                 makeRange(icRange380nm, icRange780nm, 9),
                 "non-finite source endpoint");
}

void testRangeMapContractPreserved()
{
  // Identical ranges: still NULL, and pFailed must stay clear. Callers depend on this
  // to mean "use the vector as it stands"; turning it into a failure would break every
  // conformant spectral profile.
  icSpectralRange same = makeRange(icRange380nm, icRange780nm, 9);

  bool failed = true;
  CIccMatrixMath *mtx = CIccMatrixMath::rangeMap(same, same, &failed);
  check(mtx == NULL, "identical ranges: rangeMap -> NULL (no conversion needed)");
  check(!failed, "identical ranges: pFailed clear, this is not an error");
  delete mtx;

  // A legitimate pair must still produce a usable matrix, with the shape the callers
  // assume: dstRange.steps rows by srcRange.steps columns.
  icSpectralRange src = makeRange(icRange380nm, icRange780nm, 5);
  icSpectralRange dst = makeRange(icRange380nm, icRange780nm, 9);

  failed = true;
  mtx = CIccMatrixMath::rangeMap(src, dst, &failed);
  check(mtx != NULL, "mappable ranges: rangeMap still returns a matrix");
  check(!failed, "mappable ranges: pFailed clear");

  if (mtx) {
    check(mtx->GetRows() == dst.steps, "mappable ranges: rows == destination steps");
    check(mtx->GetCols() == src.steps, "mappable ranges: cols == source steps");

    // Every coefficient must have been written. SetRange() memsets the whole matrix
    // before filling it, so on a success path there is no uninitialized entry left;
    // this catches a partial fill rather than sampling heap contents (which a passing
    // allocator could make look finite by accident).
    bool allFinite = true;
    for (icUInt16Number r = 0; r < mtx->GetRows(); ++r) {
      const icFloatNumber *row = mtx->entry(r);
      for (icUInt16Number c = 0; c < mtx->GetCols(); ++c) {
        if (!(row[c] >= -1.0e30f && row[c] <= 1.0e30f))
          allFinite = false;
      }
    }
    check(allFinite, "mappable ranges: every coefficient initialized");
    delete mtx;
  }
}

// getObserverMatrix() returns a 3 x newRange.steps matrix. When no conversion matrix is
// available it used to memcpy the observer in wholesale, guarded only against writing
// past the end (observerRange.steps > newRange.steps). That misses the case this test
// pins: with a *narrower* observer the copy fits, but it lands three rows of
// observerRange.steps into a buffer whose rows are newRange.steps apart -- so the rows
// are misplaced and the tail of the matrix is never written at all.
//
// This branch is reachable on unfixed sources without the rangeMap() change above,
// because CIccPcsXform::rangeMap() already returns NULL on a rejected pair.
void testObserverMatrixRowLayout()
{
  CIccTagSpectralViewingConditions svc;

  // A single-step observer range: legal to store, impossible to resample from.
  icSpectralRange observerRange = makeRange(icRange380nm, icRange780nm, 1);
  std::vector<icFloatNumber> observer(3u * observerRange.steps, 1.0f);

  if (!svc.setObserver(icStdObs1931TwoDegrees, observerRange, &observer[0])) {
    check(false, "setObserver accepted a single-step observer range (test precondition)");
    return;
  }

  icSpectralRange newRange = makeRange(icRange380nm, icRange780nm, 9);

  CIccMatrixMath *mtx = svc.getObserverMatrix(newRange);
  check(mtx == NULL,
        "getObserverMatrix -> NULL when the observer cannot be resampled onto newRange");
  delete mtx;

  icFloatNumber *rv = svc.applyRangeToObserver(newRange);
  check(rv == NULL,
        "applyRangeToObserver -> NULL on the same unresampleable pair");
  free(rv);

  // The matching case must keep working: an observer already on newRange needs no
  // conversion, and the wholesale copy is exactly right there.
  icSpectralRange wideRange = makeRange(icRange380nm, icRange780nm, 9);
  std::vector<icFloatNumber> wide(3u * wideRange.steps, 0.5f);

  CIccTagSpectralViewingConditions svcOk;
  if (svcOk.setObserver(icStdObs1931TwoDegrees, wideRange, &wide[0])) {
    CIccMatrixMath *ok = svcOk.getObserverMatrix(wideRange);
    check(ok != NULL, "getObserverMatrix still succeeds when the ranges match");
    if (ok) {
      check(ok->GetRows() == 3 && ok->GetCols() == wideRange.steps,
            "matching ranges: observer matrix has the expected shape");
      delete ok;
    }
  }
}

} // namespace

int main()
{
  testRangeMapRejections();
  testRangeMapContractPreserved();
  testObserverMatrixRowLayout();

  if (g_fail)
    std::fprintf(stderr, "[rangemap-uninit] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[rangemap-uninit] all assertions passed\n");

  return g_fail;
}
