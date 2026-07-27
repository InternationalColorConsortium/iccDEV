// Regression for CIccSparseMatrixUInt8::set / CIccSparseMatrixUInt16::set in
// IccProfLib/IccSparseMatrix.h.
//
// Two defects were fixed together (alerts #1061 and #2260):
//   1. NaN-blind clamp (both entry types). The old guard `value < 0.0 ? 0 : ...`
//      lets a NaN fall through to `(icUInt8/16Number)(NaN * scale + 0.5)`, an
//      undefined float->int cast (CWE-681). The fix uses `!(value > 0.0) ? 0`,
//      which is false for NaN and for value<=0, so both route to 0 before the cast.
//   2. Truncation in the UInt16 entry. `set()` cast the scaled value to
//      (icUInt8Number) while storing into a 16-bit slot, so any value in (0,1)
//      whose scaled result exceeds 255 was truncated to 8 bits before widening --
//      e.g. 0.5 -> (icUInt8Number)32768 == 0. get() then read it back as 0.0,
//      breaking the set()/get() round-trip against get()'s /65535.0f.
//
// The set()/get() methods are inline, so this exercises the exact shipped code.
// Assertions are mutation-verified: reverting either fix makes a case below fail
// (NaN -> nonzero garbage; UInt16 0.5 -> raw 0 -> get() 0.0).
//
// The +INF cases were added later, and are the reason set() tests saturation
// before it tests isfinite(). A guard written the other way round --
// `if (!isfinite(value) || value < 0) 0; else if (value > 1) MAX; ...` -- is
// NaN-safe and passes every other case here, but sends +INF to 0 instead of full
// scale, silently inverting the stored value. Nothing else in this file catches
// that, so without these four checks the reordering is invisible.

#include "IccSparseMatrix.h"

#include <cmath>
#include <cstdio>
#include <limits>

static int g_fails = 0;

static void check(bool ok, const char *what) {
  if (!ok) { fprintf(stderr, "FAIL: %s\n", what); ++g_fails; }
}

int main(void) {
  const float kPosInf =  std::numeric_limits<float>::infinity();
  const float kNegInf = -std::numeric_limits<float>::infinity();

  // ---- CIccSparseMatrixUInt8 ----
  {
    icUInt8Number buf[6] = {0};
    CIccSparseMatrixUInt8 e;
    e.init(buf);

    e.set(0, 0.5f);
    check(std::fabs(e.get(0) - 0.5f) < 0.01f, "UInt8 set(0.5) round-trip");

    e.set(1, std::nanf(""));                 // must NOT hit the UB cast
    check(buf[1] == 0, "UInt8 set(NaN) -> 0 (no UB cast)");

    e.set(2, -3.0f);
    check(buf[2] == 0, "UInt8 set(negative) -> 0");

    e.set(3, 5.0f);
    check(buf[3] == 255, "UInt8 set(>1) -> 255");

    // +INF is out-of-range HIGH, so it saturates like any other value > 1.
    e.set(4, kPosInf);
    check(buf[4] == 255, "UInt8 set(+INF) -> 255 (saturates, not 0)");

    // -INF is out-of-range LOW and shares the NaN branch.
    e.set(5, kNegInf);
    check(buf[5] == 0, "UInt8 set(-INF) -> 0");
  }

  // ---- CIccSparseMatrixUInt16 (also guards the truncation fix) ----
  {
    icUInt16Number buf[5] = {0};
    CIccSparseMatrixUInt16 e;
    e.init(buf);

    e.set(0, 0.5f);
    // Pre-fix this stored (icUInt8Number)32768 == 0; the fix stores ~32768.
    check(buf[0] > 32000, "UInt16 set(0.5) stores full 16-bit value (truncation fix)");
    check(std::fabs(e.get(0) - 0.5f) < 0.01f, "UInt16 set(0.5) round-trip");

    e.set(1, std::nanf(""));
    check(buf[1] == 0, "UInt16 set(NaN) -> 0 (no UB cast)");

    e.set(2, 1.0f);
    check(buf[2] == 65535, "UInt16 set(1.0) -> 65535");

    e.set(3, kPosInf);
    check(buf[3] == 65535, "UInt16 set(+INF) -> 65535 (saturates, not 0)");

    e.set(4, kNegInf);
    check(buf[4] == 0, "UInt16 set(-INF) -> 0");
  }

  if (g_fails == 0)
    printf("PASS: IccSparseMatrix set() NaN-safety + UInt16 round-trip\n");
  return g_fails ? 1 : 0;
}
