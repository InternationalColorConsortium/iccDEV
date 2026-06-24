// Regression for #1553: CIccTag*NumArray::GetValues must bound the SOURCE start
// offset (nStart), not only the destination length, or m_Num[i+nStart] reads past
// the end of the array (CWE-125, out-of-bounds read).
//
// xsscx's fuzz repro reached CIccTagFloatNum<float,...>::GetValues at
// IccTagBasic.cpp:7172 through iccApplyNamedCmm -- the named-colour reverse lookup
// (CIccArrayNamedColor::FindPcsColor) passes a fixed nVectorSize=3 while nStart steps
// by the device-sample count, so it over-reads on device data with < 3 channels. The
// FloatNum variant guarded only nVectorSize; the two integer siblings guarded
// nVectorSize+nStart, a sum that wraps for a huge nStart and slips past the check; the
// sparse-matrix variant used '>' where '>=' is required (one matrix past the end).
//
// This test pins the corrected contract on the num-array family. Each assertion is
// red-green: the "-> false" cases return true (and read out of bounds) if the matching
// guard is reverted. The sparse-matrix '>'->'>=' off-by-one is a one-token bound fix
// covered by inspection (constructing a populated sparse-matrix tag in isolation is
// out of proportion to the change).
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagBasic.h"
#include "IccDefs.h"

#include <cstdio>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[getvalues-nstart] FAIL: %s\n", what);
  }
}

// Exercise one CIccTagNumArray subclass: a valid in-range window must succeed, while a
// window whose start pushes the read past the end -- including the icUInt32Number
// overflow case -- must be rejected (not over-read).
template <typename TagT>
void exerciseNumArray(const char *name)
{
  const icUInt32Number N = 8;
  TagT t;
  t.SetSize(N);                       // m_Num now holds N entries; valid indices [0,N)
  icFloatNumber dst[8] = { 0 };

  char msg[96];

  // Valid: nStart=N-3, nVectorSize=3 reads exactly the last three entries [N-3,N).
  std::snprintf(msg, sizeof(msg), "%s: valid in-range window -> true", name);
  check(t.GetValues(dst, N - 3, 3), msg);

  // Over-read: nStart=N-2, nVectorSize=3 would read m_Num[N-2..N], and index N is OOB.
  std::snprintf(msg, sizeof(msg), "%s: nStart over-read -> false (#1553)", name);
  check(!t.GetValues(dst, N - 2, 3), msg);

  // Exact-end start: nStart=N, nVectorSize=1 -> m_Num[N] is one past the end.
  std::snprintf(msg, sizeof(msg), "%s: nStart == m_nSize -> false", name);
  check(!t.GetValues(dst, N, 1), msg);

  // Overflow: a huge nStart makes nStart+nVectorSize wrap to a small value; the
  // overflow-safe bound must still reject it rather than wrap past the guard.
  std::snprintf(msg, sizeof(msg), "%s: overflow nStart -> false", name);
  check(!t.GetValues(dst, 0xFFFFFFFFu, 2), msg);
}

} // namespace

int main()
{
  exerciseNumArray<CIccTagFloat32>("FloatNum");      // the defect with the public repro
  exerciseNumArray<CIccTagS15Fixed16>("FixedNum");   // overflow-safe normalization
  exerciseNumArray<CIccTagUInt8>("Num");             // overflow-safe normalization

  if (g_fail) {
    std::fprintf(stderr, "[getvalues-nstart] %d assertion(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[getvalues-nstart] all assertions passed\n");
  return 0;
}
