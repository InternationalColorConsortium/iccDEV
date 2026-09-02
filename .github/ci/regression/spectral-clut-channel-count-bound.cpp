// Regression: CIccMpeSpectralCLUT::Read must bound m_nInputChannels before the
// narrowing cast that constructs its CIccCLUT, and every CIccCLUT::Init() call in
// IccProfLib must act on the result.
//
// Read() takes m_nInputChannels as a 16-bit field straight from the profile and
// validated only "< 1". It then built the CLUT with
//
//   m_pCLUT = new CIccCLUT((icUInt8Number)m_nInputChannels, ...)   // IccMpeSpectral.cpp
//
// so the field was narrowed to 8 bits with no upper bound in between.
// CIccMpeCLUT::Read (IccMpeBasic.cpp) and CIccMpeExtCLUT::Read both bound the same
// field with "if (m_nInputChannels > 16) return false;" before the equivalent cast;
// the spectral path did not.
//
// What that cast does splits into two cases, and only one of them was actually a
// hole -- which is the point of the two groups of assertions below.
//
// OUT OF RANGE (17, 255, 272 -> 17, 255, 16; and 256 -> 0). Already refused, but
// not by anything that meant to refuse it. CIccCLUT::Init() rejects m_nInput of 0
// or > 16 and returns false, and all six IccProfLib call sites that mattered
// discarded that false. What stopped these was incidental: Init() returns before
// the "delete[] m_pData; m_pData = NULL;" that would reallocate, the constructor
// had already set m_pData to NULL, and GetData(0) is "&m_pData[index]" -- NULL for
// index 0. Read()'s existing "pData = m_pCLUT->GetData(0); if (!pData) return
// false;" therefore converted the dropped Init() failure into a failed Read().
// These cases are pinned here precisely because nothing declares that coupling: a
// future change to GetData() (a NULL check returning a dummy, say) or to the order
// of Init()'s guards would reopen the hole with no test noticing.
//
// TRUNCATING (257 -> 1). This one was live. Read() returned true, and the element
// reported NumInputChannels() == 257 over a CLUT whose GetInputDim() was 1. Nothing
// downstream compares the two. It is reachable past the CMM's channel checks
// because icNumColorSpaceChannels() is defined as the low 16 bits of the colour
// space signature (icProfileHeader.h), so a header can declare a 257-channel
// N-channel space, icGetSpaceSamples() returns 257, and the
//
//   if (inputSamples != xformInputSamples || ...) return icCmmStatBadXform;
//
// equality in CIccXformMpe::Begin() is satisfied. The element then applies: because
// 257 matches no arm of the switch on m_nInputChannels in
// CIccMpeEmissionCLUT::Begin(), m_interpType falls to icNdInterp, InterpND() walks
// the CLUT's own m_nInput of 1, and 256 of the 257 input channels are silently
// discarded. Not a memory-safety fault -- the CLUT is internally consistent, just
// not the CLUT the element claims to be -- but a profile that transforms to wrong
// colour without any diagnostic.
//
// The Init() call sites are covered too, via CIccMBB::NewCLUT(). Its documented
// contract is "Return: Pointer to the CIccCLUT object", and it called Init() and
// returned the CLUT whether or not Init() had succeeded, so a caller received a
// CLUT with m_pData NULL and (before #1781's tightening) a committed
// m_nNumPoints. That is the same defect iccApplyToLink.cpp works around by hand
// rather than calling NewCLUT() -- see the comment at its CIccTagLutAtoB
// construction. NewCLUT() now returns NULL on Init() failure, which is what its
// contract already said it would.
//
// Red-green: reverting the "m_nInputChannels > 16" bound in
// CIccMpeSpectralCLUT::Read makes the 257 assertion fail (Read succeeds and the
// channel counts disagree). Reverting the NewCLUT() Init() check makes the
// 17-input NewCLUT assertion fail (a non-NULL CLUT comes back with no storage).
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagLut.h"
#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
#include "IccIO.h"
#include "IccUtil.h"
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
    std::fprintf(stderr, "[spectral-clut-channels] FAIL: %s\n", what);
  }
}

// Append a value in ICC wire order (big-endian); CIccIO::Read16/Read32 swap to
// host order on the way in.
void putBE(std::vector<icUInt8Number> &b, unsigned long long v, int nBytes)
{
  for (int i = nBytes - 1; i >= 0; --i)
    b.push_back((icUInt8Number)((v >> (8 * i)) & 0xff));
}

// Build an emissionCLUT MPE element body, matching the 40-byte header
// CIccMpeSpectralCLUT::Read computes: type sig, reserved, input/output channel
// counts, flags, spectral range (start/end/steps), storage type, 16 grid points.
// The payload that follows is zero-filled and deliberately generous, so the
// nMaxSize arms of Init() are never what refuses a case -- only the channel-count
// guards can be.
std::vector<icUInt8Number> buildEmissionCLUT(icUInt16Number nInputChannels,
                                             icUInt16Number nOutputChannels,
                                             icUInt8Number nGridPoints,
                                             icUInt16Number nSteps,
                                             size_t nPayloadBytes)
{
  std::vector<icUInt8Number> b;
  putBE(b, (unsigned long long)icSigEmissionCLUTElemType, 4);
  putBE(b, 0u, 4);                                  // reserved
  putBE(b, nInputChannels, 2);
  putBE(b, nOutputChannels, 2);
  putBE(b, 0u, 4);                                  // flags
  putBE(b, 400u, 2);                                // range.start
  putBE(b, 700u, 2);                                // range.end
  putBE(b, nSteps, 2);                              // range.steps
  putBE(b, (unsigned long long)icValueTypeFloat32, 2);
  for (int i = 0; i < 16; ++i)
    b.push_back(nGridPoints);

  b.insert(b.end(), nPayloadBytes, (icUInt8Number)0);
  return b;
}

// Feed the element body through Read(). On success, reports the element's declared
// input channel count and the dimensionality of the CLUT actually built.
struct ReadResult {
  bool bRead;
  unsigned nDeclaredInputChannels;
  unsigned nClutInputDim;
  bool bHaveClut;
};

ReadResult readEmissionCLUT(std::vector<icUInt8Number> &body)
{
  ReadResult r = { false, 0, 0, false };

  CIccMemIO io;
  if (!io.Attach(&body[0], body.size()))
    return r;

  CIccMpeEmissionCLUT elem;
  r.bRead = elem.Read((icUInt32Number)body.size(), &io);
  if (r.bRead) {
    r.nDeclaredInputChannels = (unsigned)elem.NumInputChannels();
    CIccCLUT *pCLUT = elem.GetCLUT();
    r.bHaveClut = (pCLUT != NULL);
    if (pCLUT)
      r.nClutInputDim = (unsigned)pCLUT->GetInputDim();
  }
  return r;
}

// A generous payload: 16 grid points ^ a few dims, times steps, times 4 bytes.
const size_t kBigPayload = 4u * 1024u * 1024u;

// m_nInputChannels is protected on CIccMultiProcessElement. Deriving is how a
// test reaches it without widening the library's interface for a test's benefit
// -- the same approach mpexml-unknown-reserved.cpp takes for m_nReserved.
//
// This models a state no reader produces any more but that the object model
// still permits: an element whose declared channel count disagrees with the
// CLUT behind it. All three parsers that could reach it now bound the count
// (Read() here, icCLutFromXml() in IccTagXml.cpp, icCLUTFromJson() in
// IccTagJson.cpp), so the check in Begin() is the backstop for anything that
// builds an element some other way -- including callers of the public API,
// which is what IccProfLib exposes these classes as.
//
// CIccMpeCLUT is the element used rather than one of the spectral pair, because
// its Begin() ignores pMPE entirely (the parameter is commented out in the
// definition) and so returns a verdict on the CLUT alone. The spectral
// overrides reach for the enclosing tag's applied PCC once their channel checks
// pass, so removing the guard under test would make them fault on a NULL tag
// instead of returning -- a failure, but an unreadable one. All three carry the
// same comparison; this asserts it where the answer is a clean bool.
class MismatchedCLUT : public CIccMpeCLUT
{
public:
  // Attach a CLUT of one dimensionality while declaring another, exactly as the
  // narrowing cast used to do on its own. SetCLUT() derives m_nInputChannels
  // from the CLUT, so the declared count is overwritten afterwards.
  void setMismatch(CIccCLUT *pCLUT, icUInt16Number nDeclaredInputChannels)
  {
    SetCLUT(pCLUT);
    m_nInputChannels = nDeclaredInputChannels;
  }
};

} // namespace

int main()
{
  // ---- The live defect: a truncating channel count ----------------------------
  //
  // 257 narrows to 1. Before the fix Read() returned true and the element
  // advertised 257 input channels over a one-dimensional CLUT. Must be refused.
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/257, /*out*/3, /*grid*/4, /*steps*/4, kBigPayload);
    ReadResult r = readEmissionCLUT(b);
    check(!r.bRead, "257 input channels (truncates to 1) -> Read() false");
    if (r.bRead)
      std::fprintf(stderr,
                   "[spectral-clut-channels]   declared=%u but CLUT dim=%u\n",
                   r.nDeclaredInputChannels, r.nClutInputDim);
  }

  // 259 -> 3 and 260 -> 4 truncate onto counts that are not merely valid but
  // ordinary, so the resulting element looks entirely well-formed from the CLUT
  // side. These are the cases least likely to be caught by inspection.
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/259, /*out*/3, /*grid*/4, /*steps*/4, kBigPayload);
    check(!readEmissionCLUT(b).bRead, "259 input channels (truncates to 3) -> Read() false");
  }
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/260, /*out*/3, /*grid*/2, /*steps*/4, kBigPayload);
    check(!readEmissionCLUT(b).bRead, "260 input channels (truncates to 4) -> Read() false");
  }

  // 272 -> 16, landing exactly on the supported maximum: the CLUT is built at the
  // largest dimensionality Init() allows while the element still claims 272.
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/272, /*out*/3, /*grid*/2, /*steps*/2, kBigPayload);
    check(!readEmissionCLUT(b).bRead, "272 input channels (truncates to 16) -> Read() false");
  }

  // ---- Out of range: refused before the fix, but only incidentally -----------
  //
  // These pin the behaviour so the GetData(0)/Init() coupling described at the top
  // of this file cannot be broken silently.
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/17, /*out*/3, /*grid*/2, /*steps*/2, kBigPayload);
    check(!readEmissionCLUT(b).bRead, "17 input channels (past CIccCLUT max) -> Read() false");
  }
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/255, /*out*/3, /*grid*/2, /*steps*/2, kBigPayload);
    check(!readEmissionCLUT(b).bRead, "255 input channels -> Read() false");
  }
  {
    std::vector<icUInt8Number> b =
        buildEmissionCLUT(/*in*/256, /*out*/3, /*grid*/2, /*steps*/2, kBigPayload);
    check(!readEmissionCLUT(b).bRead, "256 input channels (truncates to 0) -> Read() false");
  }

  // ---- The invariant, stated positively --------------------------------------
  //
  // Whenever Read() succeeds, the count the element reports must be the
  // dimensionality of the CLUT backing it. This is the assertion that would have
  // caught the original defect without knowing which values truncate.
  {
    const icUInt16Number kValid[] = { 1, 2, 3, 4, 16 };
    for (size_t i = 0; i < sizeof(kValid) / sizeof(kValid[0]); ++i) {
      // Keep the grid at 2 for the wide cases so 2^16 nodes stays affordable.
      icUInt8Number nGrid = (kValid[i] <= 4) ? (icUInt8Number)4 : (icUInt8Number)2;
      std::vector<icUInt8Number> b =
          buildEmissionCLUT(kValid[i], /*out*/3, nGrid, /*steps*/2, kBigPayload);
      ReadResult r = readEmissionCLUT(b);

      char msg[160];
      std::snprintf(msg, sizeof(msg),
                    "%u input channels -> Read() true", (unsigned)kValid[i]);
      check(r.bRead, msg);

      if (r.bRead) {
        std::snprintf(msg, sizeof(msg),
                      "%u input channels -> NumInputChannels() == CLUT GetInputDim()",
                      (unsigned)kValid[i]);
        check(r.bHaveClut && r.nDeclaredInputChannels == r.nClutInputDim, msg);
      }
    }
  }

  // ---- CIccCLUT::Init()'s result must reach the caller -----------------------
  //
  // CIccMBB::NewCLUT() promises a pointer to the CLUT object; on Init() failure it
  // used to promise one with no storage behind it. A 17-channel CIccMBB is the
  // shortest route to an Init() that refuses: mAB/lut8/lut16 reads all cap the
  // channel count at 15, so this state is only reachable through the public
  // Init(nInput, nOutput) API -- which is exactly the caller NewCLUT() serves.
  {
    CIccTagLutAtoB lut;
    lut.Init(/*nInput*/17, /*nOutput*/3);
    CIccCLUT *pCLUT = lut.NewCLUT((icUInt8Number)2, 2);
    check(pCLUT == NULL, "NewCLUT() with 17 input channels -> NULL (Init() refused)");
    if (pCLUT)
      std::fprintf(stderr,
                   "[spectral-clut-channels]   NewCLUT returned %p with GetData(0)=%p\n",
                   (void *)pCLUT, (void *)pCLUT->GetData(0));
    // The tag must not be left holding the rejected CLUT either.
    check(lut.GetCLUT() == NULL, "NewCLUT() failure leaves CIccMBB with no CLUT");
  }

  // Grid granularity below 2 is the other way Init() refuses, and it reaches
  // NewCLUT() through the same public API with an otherwise ordinary channel
  // count -- so it exercises the propagation without relying on the 16-input cap.
  {
    CIccTagLutAtoB lut;
    lut.Init(/*nInput*/3, /*nOutput*/3);
    CIccCLUT *pCLUT = lut.NewCLUT((icUInt8Number)1, 2);
    check(pCLUT == NULL, "NewCLUT() with 1 grid point -> NULL (Init() refused)");
    check(lut.GetCLUT() == NULL, "NewCLUT() grid failure leaves CIccMBB with no CLUT");
  }

  // A well-formed CIccMBB must still get its CLUT -- the propagation must not
  // regress the ordinary path.
  {
    CIccTagLutAtoB lut;
    lut.Init(/*nInput*/3, /*nOutput*/3);
    CIccCLUT *pCLUT = lut.NewCLUT((icUInt8Number)9, 2);
    check(pCLUT != NULL, "NewCLUT() with a valid 9-point 3x3 grid -> non-NULL");
    if (pCLUT) {
      check(pCLUT->GetData(0) != NULL, "valid NewCLUT() CLUT has storage");
      check(pCLUT->NumPoints() == 9u * 9u * 9u, "valid NewCLUT() CLUT has 9^3 points");
    }
  }

  // ---- Begin() must refuse a count that disagrees with its CLUT -------------
  //
  // The backstop for every construction route that is not Read(). Begin() is
  // what the apply path calls whatever built the element, so a disagreement has
  // to be caught here if it was not prevented earlier. The XML parser used to
  // produce exactly this state (see spectral-clut-xml-channel-mismatch.cpp);
  // now nothing does, which is precisely why the guard needs its own coverage
  // rather than relying on a parser to reach it.
  //
  {
    // A well-formed 3-dimensional CLUT: 2 grid points per dimension, 3 outputs.
    CIccCLUT *pCLUT = new CIccCLUT((icUInt8Number)3, (icUInt16Number)3, 4);
    check(pCLUT->Init((icUInt8Number)2), "helper CLUT initializes");

    MismatchedCLUT elem;
    elem.setMismatch(pCLUT, /*declared*/259);   // 259 & 0xFF == 3

    check(elem.NumInputChannels() == 259, "helper declares 259 input channels");
    check(elem.GetCLUT() != NULL && elem.GetCLUT()->GetInputDim() == 3,
          "helper's CLUT is three-dimensional");
    check(!elem.Begin(icElemInterpLinear, NULL),
          "Begin() refuses declared 259 over a three-dimensional CLUT");
  }

  // The control: an element whose counts agree must still begin. Without it the
  // assertion above could be satisfied by a Begin() that refuses everything.
  {
    CIccCLUT *pCLUT = new CIccCLUT((icUInt8Number)3, (icUInt16Number)3, 4);
    pCLUT->Init((icUInt8Number)2);

    MismatchedCLUT elem;
    elem.setMismatch(pCLUT, /*declared*/3);

    check(elem.Begin(icElemInterpLinear, NULL),
          "Begin() accepts an element whose count matches its CLUT");
  }

  // And a CLUT that Init() never made usable must be refused by Begin() even
  // when the counts agree -- this is what CIccCLUT::Begin() returning bool
  // buys, and the state a caller reaches by skipping Init() altogether.
  {
    CIccCLUT *pCLUT = new CIccCLUT((icUInt8Number)3, (icUInt16Number)3, 4);
    // Deliberately no Init().
    MismatchedCLUT elem;
    elem.setMismatch(pCLUT, /*declared*/3);

    check(!elem.Begin(icElemInterpLinear, NULL),
          "Begin() refuses a CLUT that was never Init()ed");
  }

  if (g_fail) {
    std::fprintf(stderr, "[spectral-clut-channels] %d assertion(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[spectral-clut-channels] all assertions passed\n");
  return 0;
}
