/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// CTest contract for the apply-buffer width of a multiProcessElementType tag
// (#2699, #2703).
//
// CIccTagMultiProcessElement::GetNewApply() sizes its CIccDblPixelBuffer from
// m_nBufChannels, which Begin() computes from the element list. The copy
// constructor never set it, operator= kept the destination's old value, and
// Begin() returned early for an empty list without resetting it.
// MemorySanitizer reported the read through CIccProfile's copy constructor
// (iccPawgReport, and CIccApplyBPC's private CMM under iccApplyNamedCmm).
//
// The copy and assignment cases call GetNewApply() without a Begin() on the
// result, so they see the width that was copied: the source's, which is 5 for a
// begun 3 -> 5 matrix tag. That separates "copy it" from "reset it to 0" and
// from "leave it alone". A plain build cannot see an uninitialised read, so the
// copy is constructed with placement new over storage filled with 0xAB, which an
// unfixed library then reports as the width.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccIO.h"

#include <cstdio>
#include <cstring>
#include <new>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "mpe-copy-bufchannels: FAIL  %s\n", label);
    g_failures++;
  }
}

// Width of the apply buffer GetNewApply() builds now, without calling Begin().
// Returns -1 if GetNewApply() fails.
int currentBufWidth(CIccTagMultiProcessElement &tag)
{
  CIccApplyTagMpe *pApply = tag.GetNewApply();
  if (!pApply)
    return -1;
  int width = pApply->GetBuf()->GetMaxChannels();
  delete pApply;
  return width;
}

// Begin(), then the width. Returns -1 if Begin() fails.
int begunBufWidth(CIccTagMultiProcessElement &tag)
{
  if (!tag.Begin())
    return -1;
  return currentBufWidth(tag);
}

// Adds one 3-in, 5-out matrix element (SetSize() zero-fills it).
bool addMatrix(CIccTagMultiProcessElement &tag)
{
  CIccMpeMatrix *pMatrix = new CIccMpeMatrix();
  if (!pMatrix->SetSize(3, 5)) {
    delete pMatrix;
    return false;
  }
  tag.Attach(pMatrix);
  return true;
}

// A copy of a begun tag carries the source's width.
void testCopy()
{
  CIccTagMultiProcessElement src(3, 5);
  check(addMatrix(src), "the matrix element is built");
  check(begunBufWidth(src) == 5, "the begun matrix tag has a five-channel apply buffer");

  alignas(CIccTagMultiProcessElement) unsigned char mem[sizeof(CIccTagMultiProcessElement)];
  std::memset(mem, 0xAB, sizeof(mem));
  {
    // Read the fill back through a volatile pointer so the compiler cannot
    // drop the memset as a store before the object's lifetime begins.
    const volatile unsigned char *p = mem;
    unsigned char sink = 0;
    for (size_t i = 0; i < sizeof(mem); i++)
      sink ^= p[i];
    (void)sink;
  }
  CIccTagMultiProcessElement *pCopy = new (mem) CIccTagMultiProcessElement(src);
  check(currentBufWidth(*pCopy) == 5, "a copy of a begun tag carries its apply-buffer width");
  check(begunBufWidth(*pCopy) == 5, "the copy begins with a five-channel apply buffer");
  pCopy->~CIccTagMultiProcessElement();

  // The reported shape: a copy of a tag with no elements begins with none.
  CIccTagMultiProcessElement empty(3, 3);
  CIccTagMultiProcessElement emptyCopy(empty);
  check(begunBufWidth(emptyCopy) == 0, "a copy of an empty tag has a zero-width apply buffer");
}

// operator= takes the source's width, and assigning a tag to itself keeps it.
void testAssign()
{
  CIccTagMultiProcessElement src(3, 5);
  check(addMatrix(src), "the matrix element is built");
  check(begunBufWidth(src) == 5, "the begun source has a five-channel apply buffer");

  CIccTagMultiProcessElement dst(3, 3);
  dst = src;
  check(currentBufWidth(dst) == 5, "an assigned tag carries the source's apply-buffer width");

  CIccTagMultiProcessElement &alias = dst;
  dst = alias;
  check(dst.NumElements() == 1, "assigning a tag to itself keeps its elements");
  check(begunBufWidth(dst) == 5, "assigning a tag to itself keeps it usable");
}

// Begin() on a tag that was begun with elements and then re-read as empty
// resets the width.
void testReadEmptyAfterBegin()
{
  CIccTagMultiProcessElement tag(3, 5);
  check(addMatrix(tag), "the matrix element is built");
  check(begunBufWidth(tag) == 5, "the begun matrix tag has a five-channel apply buffer");

  CIccTagMultiProcessElement empty(3, 3);
  CIccMemIO io;
  io.Alloc(256, true);
  check(empty.Write(&io), "an empty tag writes");
  icUInt32Number size = (icUInt32Number)io.GetLength();
  io.Seek(0, icSeekSet);
  check(tag.Read(size, &io), "the matrix tag re-reads as the empty tag");
  check(begunBufWidth(tag) == 0, "re-read as empty, the tag begins with a zero-width apply buffer");
}

} // namespace

int main()
{
  testCopy();
  testAssign();
  testReadEmptyAfterBegin();

  if (g_failures)
    std::fprintf(stderr, "mpe-copy-bufchannels: %d failure(s)\n", g_failures);
  else
    std::printf("mpe-copy-bufchannels: all checks passed\n");
  return g_failures;
}
