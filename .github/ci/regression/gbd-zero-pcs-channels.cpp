// Regression for #1581: CIccTagGamutBoundaryDesc::Read must reject a tag whose
// m_nPCSChannels is 0 (CWE-400, uncontrolled resource consumption).
//
// Read() bounds m_NumberOfVertices only through the products
// nVertices*m_nPCSChannels and nVertices*m_nDeviceChannels in its tag-size check.
// m_nPCSChannels was validated for an upper bound (> 3) but never a lower bound, so
// a tag declaring m_nPCSChannels == 0 and m_nDeviceChannels == 0 (device coords are
// legitimately optional) zeroes both products. The size bound then stops
// constraining the vertex count: xsscx's 358-byte fuzz profile
// (poc-1581-gbd-zero-pcs-channels.icc) declares ~1.95 billion vertices in a 168-byte
// tag, passes Read(), and drives Describe()'s per-vertex loop for ~11m45s building a
// multi-GB string. The fix tightens the check to (m_nPCSChannels < 1 || > 3); a gamut
// boundary is a solid in PCS space, so every vertex must carry >= 1 PCS coordinate.
//
// This test pins the corrected contract directly on Read(), feeding crafted gbd tag
// payloads through a CIccMemIO. It asserts on Read()'s return value only -- it never
// calls Describe(), so it stays fast and safe whether or not the fix is present. The
// "zero PCS channels -> false" assertion is red-green: reverting the lower-bound guard
// makes Read() accept the malicious payload and the assertion fails.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagLut.h"
#include "IccIO.h"
#include "IccDefs.h"

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
    std::fprintf(stderr, "[gbd-zero-pcs] FAIL: %s\n", what);
  }
}

// Append a value to a byte vector in ICC wire order (big-endian); the IO layer
// (CIccIO::Read16/Read32) swaps wire bytes to host order on the way in.
void putBE(std::vector<icUInt8Number> &b, unsigned long long v, int nBytes)
{
  for (int i = nBytes - 1; i >= 0; --i)
    b.push_back((icUInt8Number)((v >> (8 * i)) & 0xff));
}

// Build a gamutBoundaryDescType payload: type sig, reserved, PCS/device channel
// counts, vertex/triangle counts, then zero-filled triangle + coordinate data sized
// to exactly satisfy Read()'s tag-size bound. The buffer is what Read() sees as the
// tag body; tagSize is the size argument Read() is handed.
std::vector<icUInt8Number> buildGbd(icUInt16Number pcsCh, icUInt16Number devCh,
                                    icInt32Number nVerts, icInt32Number nTris)
{
  std::vector<icUInt8Number> b;
  putBE(b, 0x67626420u, 4);            // 'gbd ' type signature
  putBE(b, 0u, 4);                     // reserved
  putBE(b, pcsCh, 2);
  putBE(b, devCh, 2);
  putBE(b, (unsigned long long)(unsigned)nVerts, 4);
  putBE(b, (unsigned long long)(unsigned)nTris, 4);
  // Triangle data (nTris * 3 uint32) followed by PCS then device float coords; the
  // exact contents are irrelevant to the bound being tested, so leave them zeroed.
  size_t triBytes = (size_t)(nTris > 0 ? nTris : 0) * 3u * 4u;
  size_t pcsBytes = (size_t)(nVerts > 0 ? nVerts : 0) * (size_t)pcsCh * 4u;
  size_t devBytes = (size_t)(nVerts > 0 ? nVerts : 0) * (size_t)devCh * 4u;
  b.resize(b.size() + triBytes + pcsBytes + devBytes, 0);
  return b;
}

// Read one crafted payload and return CIccTagGamutBoundaryDesc::Read's verdict.
bool readGbd(std::vector<icUInt8Number> &payload)
{
  CIccMemIO io;
  if (!io.Attach(payload.data(), payload.size()))
    return false;
  CIccTagGamutBoundaryDesc tag;
  return tag.Read((icUInt32Number)payload.size(), &io);
}

} // namespace

int main()
{
  // The defect: 0 PCS channels (and 0 device channels) collapse the size bound, so a
  // tiny tag can declare a near-2^31 vertex count and still be accepted. Must reject.
  {
    std::vector<icUInt8Number> p = buildGbd(/*pcs*/0, /*dev*/0,
                                            /*verts*/0x74700200, /*tris*/5);
    check(!readGbd(p), "zero PCS channels with huge vertex count -> false (#1581)");
  }

  // Regression guard for the existing upper bound: > 3 PCS channels is still invalid.
  {
    std::vector<icUInt8Number> p = buildGbd(/*pcs*/4, /*dev*/0, /*verts*/4, /*tris*/4);
    check(!readGbd(p), "four PCS channels -> false");
  }

  // A well-formed tag (3 PCS channels, no device coords, minimum solid) must still be
  // accepted -- the fix must not regress valid gamut boundaries.
  {
    std::vector<icUInt8Number> p = buildGbd(/*pcs*/3, /*dev*/0, /*verts*/4, /*tris*/4);
    check(readGbd(p), "valid 3-channel gbd tag -> true");
  }

  // A well-formed tag carrying optional device coordinates (device channels > 0) must
  // also still parse, confirming the lower-bound check is on PCS channels only.
  {
    std::vector<icUInt8Number> p = buildGbd(/*pcs*/3, /*dev*/4, /*verts*/4, /*tris*/4);
    check(readGbd(p), "valid 3-channel gbd tag with device coords -> true");
  }

  if (g_fail) {
    std::fprintf(stderr, "[gbd-zero-pcs] %d assertion(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[gbd-zero-pcs] all assertions passed\n");
  return 0;
}
