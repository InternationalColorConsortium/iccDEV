// Regression test for the CIccMpeMatrix leak fixed for #1505 in
// IccProfLib/IccEncoding.cpp (CIccDefaultEncProfileConverter::ConvertFromParams).
//
// ConvertFromParams builds the AToB3 multi-process-element tag for a colour
// encoding ('cept') parameter struct.  It creates a 3x3 CIccMpeMatrix, fills it
// from the red/green/blue primary chromaticities, and only Attaches it to the
// MPE tag AFTER all three primary lookups succeed.  Each primary lookup has an
// early-out:
//
//     pxy = FindElemOfType(icSigCept<colour>PrimaryXYZMbr, ...);
//     if (!pxy || pxy->GetSize()<2) {
//       delete pMpeTag;          // <-- did NOT delete pMtx
//       delete pIcc;
//       return icEncConvertMemoryError;
//     }
//
// Because the matrix is still unattached at this point, deleting pMpeTag does
// not reclaim it, so a malformed 'cept' profile that omits (or truncates) any
// primary leaked exactly one 56-byte CIccMpeMatrix
// (LeakSanitizer breadcrumb: CIccBasicMpeFactory::CreateElement IccMpeFactory.cpp:92).
// The fix adds the matching `delete pMtx;` to each of the three failure paths,
// mirroring the SetSize failure path just above them.
//
// This reproduces the configuration directly: a 'cept' parameter struct with a
// valid white point and a subset of the primaries, driven through the public
// encoding-converter handler.  Run under AddressSanitizer/LeakSanitizer
// (ASAN_OPTIONS=detect_leaks=1): pre-fix each omitted-primary case leaks a
// matrix; post-fix the converter frees it on every error exit.
//
// Exit code 0 = pass (LSan, when enabled, reports no leak); a leak surfaces as a
// non-zero ASan exit at process teardown.

#include "IccEncoding.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"
#include "IccProfile.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>

// Build a 2-element float32 chromaticity member tag (x, y).
static CIccTagFloat32 *makeXY(float x, float y)
{
  CIccTagFloat32 *p = new CIccTagFloat32();
  p->SetSize(2);
  (*p)[0] = x;
  (*p)[1] = y;
  return p;
}

// Drive ConvertFromParams with a 'cept' struct that supplies a white point and
// only the primaries named in `withRed/withGreen/withBlue`.  Omitting one
// primary forces the corresponding early-out -- the path that leaked pMtx.
static void run(const char *label, bool withRed, bool withGreen, bool withBlue)
{
  std::printf("\n[ %s ]\n", label);

  CIccTagStruct params;
  params.SetTagStructType(icSigColorEncodingParamsSruct);

  // White point is required up-front; without it ConvertFromParams bails before
  // ever creating the matrix, so it must always be present to reach the leak.
  params.AttachElem(icSigCeptWhitePointChromaticityMbr, makeXY(0.3127f, 0.3290f));
  if (withRed)
    params.AttachElem(icSigCeptRedPrimaryXYZMbr,   makeXY(0.64f, 0.33f));
  if (withGreen)
    params.AttachElem(icSigCeptGreenPrimaryXYZMbr, makeXY(0.30f, 0.60f));
  if (withBlue)
    params.AttachElem(icSigCeptBluePrimaryXYZMbr,  makeXY(0.15f, 0.06f));

  icHeader hdr;
  std::memset(&hdr, 0, sizeof(hdr));

  IIccEncProfileConverter *pConv = IIccEncProfileConverter::GetHandler();
  if (!pConv) {
    std::printf("FAIL: no encoding-profile converter handler\n");
    return;
  }

  CIccProfilePtr newIcc;
  icStatusEncConvert rv = pConv->ConvertFromParams(newIcc, &params, &hdr);
  // The malformed-primary cases are expected to fail the conversion; the point
  // of the test is that they fail WITHOUT leaking the matrix.
  std::printf("ok:   ConvertFromParams returned %d (no matrix leaked)\n", (int)rv);
}

int main()
{
  // Each case omits one primary, exercising one of the three early-out blocks
  // that previously leaked the unattached CIccMpeMatrix.
  run("cept missing blue primary  -- #1505", true,  true,  false);
  run("cept missing green primary -- #1505", true,  false, false);
  run("cept missing red primary   -- #1505", false, false, false);

  std::printf("\nall checks passed\n");
  return 0;
}
