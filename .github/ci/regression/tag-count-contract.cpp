/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "IccArrayBasic.h"
#include "IccIO.h"
#include "IccTagBasic.h"
#include "IccTagProfSeqId.h"

#include <cstdio>
#include <memory>

template <typename Tag>
static bool checkNumeric(const char *name)
{
  for (int requested : {0, -1, 1}) {
    Tag tag(requested);
    if (tag.GetSize() != 1) {
      std::fprintf(stderr, "%s size after %d: %u\n", name, requested,
                   (unsigned)tag.GetSize());
      return false;
    }
    tag[0] = 0;
    CIccMemIO io;
    if (!io.Alloc(256, true) || !tag.Write(&io)) {
      std::fprintf(stderr, "%s failed to write after size %d\n", name, requested);
      return false;
    }
    Tag copy(tag);
    if (copy.GetSize() != 1 || copy[0] != 0) {
      std::fprintf(stderr, "%s copy failed after size %d\n", name, requested);
      return false;
    }
  }
  return true;
}

static bool testZeroSizeConstructors()
{
  for (int requested : {0, -1, 1}) {
    CIccTagXYZ xyz(requested);
    CIccTagNamedColor2 named(requested);
    CIccTagColorantOrder order(requested);
    CIccTagColorantTable table(requested);
    if (xyz.GetSize() != 1 || named.GetSize() != 1 ||
        order.GetSize() != 1 || table.GetSize() != 1) {
      std::fprintf(stderr, "constructor sizes after %d: %u %u %u %u\n",
                   requested, (unsigned)xyz.GetSize(), (unsigned)named.GetSize(),
                   (unsigned)order.GetSize(), (unsigned)table.GetSize());
      return false;
    }

    xyz[0].X = 0;
    named.GetEntry(0)->rootName[0] = '\0';
    order[0] = 0;
    table[0].name[0] = '\0';

    CIccMemIO io;
    if (!io.Alloc(256, true) || !xyz.Write(&io)) {
      std::fprintf(stderr, "XYZ write failed after size %d\n", requested);
      return false;
    }
    CIccTagXYZ copy(xyz);
    if (copy.GetSize() != 1 || copy[0].X != 0) {
      std::fprintf(stderr, "XYZ copy failed after size %d\n", requested);
      return false;
    }
  }

  return checkNumeric<CIccTagS15Fixed16>("fixed number") &&
         checkNumeric<CIccTagUInt16>("unsigned number") &&
         checkNumeric<CIccTagFloat32>("float number");
}

class TestResponseCurveSet : public CIccTagResponseCurveSet16
{
public:
  void MakeOversized() { m_ResponseCurves->resize(65536); }
};

static bool testResponseCurveCount()
{
  TestResponseCurveSet normal;
  normal.SetNumChannels(1);
  if (!normal.NewResponseCurves(icSigStatusA))
    return false;

  CIccMemIO normalIO;
  if (!normalIO.Alloc(1024, true) || !normal.Write(&normalIO))
    return false;
  if (normal.Read(0, NULL))
    return false;

  TestResponseCurveSet oversized;
  oversized.MakeOversized();
  CIccMemIO oversizedIO;
  if (!oversizedIO.Alloc(1024, true) || oversizedIO.Tell() != 0 ||
      oversized.Write(&oversizedIO) || oversizedIO.Tell() != 0)
    return false;
  return true;
}

static bool testArrayBackPointer()
{
  CIccTagArray tag;
  CIccArrayUnknown unknown(&tag);
  CIccArrayColorantInfo colorant(&tag);
  CIccArrayNamedColor named(&tag);
  std::unique_ptr<IIccArray> copy(unknown.NewCopy(&tag));
  return unknown.GetTagArray() == &tag && colorant.GetTagArray() == &tag &&
         named.GetTagArray() == &tag && copy && copy->GetTagArray() == &tag;
}

class FailingMemIO : public CIccMemIO
{
public:
  FailingMemIO(int writes, bool failSeek)
    : m_writes(writes), m_failSeek(failSeek) {}

  size_t Write8(void *buf, size_t count=1) override
  {
    if (!m_writes)
      return 0;
    m_writes--;
    return CIccMemIO::Write8(buf, count);
  }

  int64_t Seek(int64_t offset, icSeekVal pos) override
  {
    return m_failSeek ? -1 : CIccMemIO::Seek(offset, pos);
  }

private:
  int m_writes;
  bool m_failSeek;
};

class FailProfileIdIO : public CIccMemIO
{
public:
  FailProfileIdIO() : m_failed(false) {}

  size_t Write8(void *buf, size_t count=1) override
  {
    if (!m_failed && count == sizeof(icProfileID)) {
      m_failed = true;
      return 0;
    }
    return CIccMemIO::Write8(buf, count);
  }

  bool Failed() const { return m_failed; }

private:
  bool m_failed;
};

static bool testEmptyProfileSequenceWrite()
{
  CIccTagProfileSequenceId tag;
  CIccMemIO io;
  if (!io.Alloc(256, true) || !tag.Write(&io))
    return false;

  for (int writes=0; writes<3; writes++) {
    FailingMemIO failing(writes, false);
    if (!failing.Alloc(256, true) || tag.Write(&failing))
      return false;
  }
  FailingMemIO seekFailure(3, true);
  if (!seekFailure.Alloc(256, true) || tag.Write(&seekFailure))
    return false;

  CIccProfileIdDesc entry;
  CIccMemIO normalEntryIO;
  if (!normalEntryIO.Alloc(256, true) || !entry.Write(&normalEntryIO))
    return false;
  FailProfileIdIO directFailure;
  if (!directFailure.Alloc(256, true) || entry.Write(&directFailure) ||
      !directFailure.Failed())
    return false;

  CIccTagProfileSequenceId withEntry;
  if (!withEntry.AddProfileDescription(entry))
    return false;
  CIccMemIO normalSequenceIO;
  if (!normalSequenceIO.Alloc(256, true) || !withEntry.Write(&normalSequenceIO))
    return false;
  FailProfileIdIO nestedFailure;
  return nestedFailure.Alloc(256, true) &&
         !withEntry.Write(&nestedFailure) && nestedFailure.Failed();
}

int main()
{
  const bool constructors = testZeroSizeConstructors();
  const bool response = testResponseCurveCount();
  const bool array = testArrayBackPointer();
  const bool sequence = testEmptyProfileSequenceWrite();
  if (!constructors || !response || !array || !sequence)
    std::fprintf(stderr,
                 "tag-count-contract: constructors=%d response=%d array=%d sequence=%d\n",
                 constructors, response, array, sequence);
  return constructors && response && array && sequence ? 0 : 1;
}
