// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: CIccTagCicp's copy constructor had an empty body (#2000).
//
// IccTagBasic.cpp carried:
//
//   CIccTagCicp::CIccTagCicp(const CIccTagCicp& /* ITXYZ */)
//   {
//   }
//
// The parameter name is copy-paste residue from CIccTagXYZ's copy constructor
// higher in the same file, and the body was never written. A copy constructor
// does not delegate to CIccTagCicp(), so the four icUInt8Number members -- which
// have no in-class initialiser (IccTagBasic.h) -- were left uninitialized rather
// than zeroed. That is CWE-457, and Write(), Describe() and Validate() all read
// the members straight out, so the indeterminate value can reach output.
//
// The tell that it was an oversight rather than a decision: operator= twenty
// lines below copies all four correctly, and the doc comment above the copy
// constructor already documents the parameter as ITCICP.
//
// Reachability is mainline, not fuzzed. CIccTagCicp::NewCopy() *is* this
// constructor (IccTagBasic.h: `return new CIccTagCicp(*this)`), CIccProfile's
// copy constructor clones every tag through NewCopy(), and CIccApplyBPC copies
// the whole profile (IccApplyBPC.cpp) -- which the shipped CLI tools reach via
// intent + 40 (40/41/42 = Perceptual/RelativeColorimetric/Saturation with BPC).
//
// The three levels below are that chain, from the constructor outwards:
//   1. direct copy construction
//   2. NewCopy(), the virtual dispatch BPC actually goes through
//   3. the CIccProfile copy constructor with a cicpTag attached
//
// On making this bite reliably: a naive version of this test can PASS BY LUCK,
// because an uninitialized read returns whatever the storage happened to hold.
// Measured against the unfixed library under ASan/UBSan Debug, the three levels
// do not even agree with each other:
//
//   copy-construct (stack)   0/0/0/0
//   newcopy        (heap)    190/190/190/190   (0xBE, ASan's malloc fill byte)
//   profile-copy   (heap)    190/190/190/190
//
// So the heap levels are deterministic under ASan, but the stack level read
// zeros in that build -- which is exactly why the fixture values below must be
// non-zero. Had this test used 0/0/0/0 as its fixture, the copy-construct case
// would have PASSED against the broken library. The values are also all
// different so a uniformly-filled allocation cannot masquerade as a good copy.
// These numbers are measured, not predicted; the red/green was run both ways.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccTagBasic.h"
#include "IccProfile.h"

#include <cstdio>
#include <string>

// BT.2020 primaries / PQ transfer / BT.2020 non-constant luminance / full range.
// Deliberately non-zero and all different -- see the note above: the stack-level
// case read 0/0/0/0 against the unfixed library, so a zero fixture would have
// made that assertion toothless.
static const icUInt8Number kPrimaries  = 9;
static const icUInt8Number kTransfer   = 16;
static const icUInt8Number kMatrix     = 9;
static const icUInt8Number kFullRange  = 1;

static int g_failures = 0;

static void expectFields(const char *szCase, CIccTagCicp &tag)
{
  icUInt8Number p = 0, t = 0, m = 0, f = 0;
  tag.GetFields(p, t, m, f);

  if (p != kPrimaries || t != kTransfer || m != kMatrix || f != kFullRange) {
    printf("FAIL [%s]: got %u/%u/%u/%u, expected %u/%u/%u/%u\n", szCase,
           (unsigned)p, (unsigned)t, (unsigned)m, (unsigned)f,
           (unsigned)kPrimaries, (unsigned)kTransfer,
           (unsigned)kMatrix, (unsigned)kFullRange);
    g_failures++;
    return;
  }

  printf("ok   [%s]: %u/%u/%u/%u survived the copy\n", szCase,
         (unsigned)p, (unsigned)t, (unsigned)m, (unsigned)f);
}

int main()
{
  // Level 1 -- direct copy construction, the defect at its source.
  {
    CIccTagCicp src;
    src.SetFields(kPrimaries, kTransfer, kMatrix, kFullRange);

    CIccTagCicp copy(src);
    expectFields("copy-construct", copy);
  }

  // Level 2 -- NewCopy(), which is the copy constructor reached through the
  // virtual CIccTag interface. This is the call CIccProfile makes per tag.
  {
    CIccTagCicp src;
    src.SetFields(kPrimaries, kTransfer, kMatrix, kFullRange);

    CIccTag *pCopy = src.NewCopy();
    if (!pCopy) {
      printf("FAIL [newcopy]: NewCopy returned NULL\n");
      g_failures++;
    }
    else {
      CIccTagCicp *pCicp = dynamic_cast<CIccTagCicp *>(pCopy);
      if (!pCicp) {
        printf("FAIL [newcopy]: NewCopy did not return a CIccTagCicp\n");
        g_failures++;
      }
      else {
        expectFields("newcopy", *pCicp);
      }
      delete pCopy;
    }
  }

  // Level 3 -- the mainline path: CIccProfile's copy constructor clones every
  // tag via NewCopy(), which is what CIccApplyBPC does to the profile it is
  // handed. The profile needs no more than the one tag for this; it is never
  // written or applied, only copied.
  {
    CIccProfile src;
    CIccTagCicp *pTag = new CIccTagCicp();
    pTag->SetFields(kPrimaries, kTransfer, kMatrix, kFullRange);

    if (!src.AttachTag(icSigCicpTag, pTag)) {
      printf("FAIL [profile-copy]: AttachTag(icSigCicpTag) failed\n");
      g_failures++;
      delete pTag;
    }
    else {
      CIccProfile copy(src);

      CIccTag *pFound = copy.FindTag(icSigCicpTag);
      if (!pFound) {
        printf("FAIL [profile-copy]: copied profile has no cicpTag\n");
        g_failures++;
      }
      else {
        CIccTagCicp *pCicp = dynamic_cast<CIccTagCicp *>(pFound);
        if (!pCicp) {
          printf("FAIL [profile-copy]: cicpTag is not a CIccTagCicp\n");
          g_failures++;
        }
        else {
          expectFields("profile-copy", *pCicp);
        }
      }
    }
  }

  if (g_failures) {
    printf("%d case(s) regressed\n", g_failures);
    return 1;
  }

  printf("all cases passed\n");
  return 0;
}
