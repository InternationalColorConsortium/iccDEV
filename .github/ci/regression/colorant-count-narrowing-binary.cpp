/*
    File:       colorant-count-narrowing-binary.cpp

    Contains:   CTest helper for the binary colorant-count readers --
                CIccTagColorantOrder::Read() and CIccTagColorantTable::Read() --
                the follow-up to #2535/#2536 that makes every front end refuse
                a count wider than the tag's 16-bit SetSize() instead of
                silently truncating it.

    #2535/#2536 fixed the JSON readers, and the XML colorantOrder reader, where
    the narrowed count overran the allocation.  Two binary-path siblings were
    measured NOT to overrun and were left alone there.  One of them still
    disagreed with the rest:

      CIccTagColorantOrder::Read() cast nCount straight to icUInt16Number for
      SetSize() and then read m_nCount bytes -- the NARROWED count -- so a tag
      declaring 65537 positions loaded successfully as a one-position tag, and
      the remaining 65536 bytes were never read and never reported.

      CIccTagColorantTable::Read() already refused "nCount > 0xffff".  It is the
      precedent the others now follow, and the cases for it here are controls:
      they pass before this change and after it, and exist so that the two
      binary readers' contracts are pinned side by side.

    Pure library test -- the tags are read from a CIccMemIO over bytes built in
    memory, with no fixtures on disk and no tool invocation -- so it needs only
    IccProfLib and no scratch directory.

    The colorantOrder oversize case uses 65537, not 65536.  65536 narrows to 0,
    SetSize(0) reallocates to nothing and reports failure, so the unfixed reader
    ALREADY refused it; only 65537, which narrows to a non-zero 1, shows the
    silent truncation.  Both are asserted.  This reader never overran, so unlike
    the #2535/#2536 helpers its red is an ordinary assertion failure on every
    lane: before the fix Read() returns true with GetSize() == 1.

    The 65535 cases are the boundary discriminators: that count is representable
    and must still read in full, so a guard spelled ">= 0xffff" fails them while
    passing every other case here.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccTagBasic.h"
#include "IccIO.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "colorant-count-narrowing-binary: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "colorant-count-narrowing-binary: FAIL  %s\n", label);
  return 1;
}

static void put32(std::vector<icUInt8Number> &b, icUInt32Number v)
{
  b.push_back((icUInt8Number)(v >> 24));
  b.push_back((icUInt8Number)(v >> 16));
  b.push_back((icUInt8Number)(v >> 8));
  b.push_back((icUInt8Number)v);
}

// type signature, reserved, declared count -- the 12-byte prefix both
// element types share.
static std::vector<icUInt8Number> prefix(icTagTypeSignature sig,
                                         icUInt32Number nCount)
{
  std::vector<icUInt8Number> b;
  put32(b, (icUInt32Number)sig);
  put32(b, 0);
  put32(b, nCount);
  return b;
}

// A colorantOrderType element whose body really carries nCount positions, so
// the size-derived capacity guard in Read() passes and the declared count is
// the only thing deciding the outcome.
static std::vector<icUInt8Number> orderElement(icUInt32Number nCount)
{
  std::vector<icUInt8Number> b = prefix(icSigColorantOrderType, nCount);
  for (icUInt32Number i = 0; i < nCount; i++)
    b.push_back((icUInt8Number)(i % 256));
  return b;
}

// A colorantTableType element carrying nCount 38-byte entries.
static std::vector<icUInt8Number> tableElement(icUInt32Number nCount)
{
  std::vector<icUInt8Number> b = prefix(icSigColorantTableType, nCount);
  b.reserve(b.size() + (size_t)nCount * 38);
  for (icUInt32Number i = 0; i < nCount; i++) {
    char name[32] = {0};
    std::snprintf(name, sizeof(name), "colorant-%u", (unsigned)i);
    b.insert(b.end(), name, name + sizeof(name));
    for (int k = 0; k < 6; k++)
      b.push_back(0);
  }
  return b;
}

static int orderCase(icUInt32Number nCount, bool bExpectRead, const char *label)
{
  std::vector<icUInt8Number> bytes = orderElement(nCount);

  CIccMemIO io;
  if (!io.Attach(bytes.data(), (icUInt32Number)bytes.size()))
    return check(false, "the colorantOrder byte stream could be attached");

  CIccTagColorantOrder tag;
  const bool bRead = tag.Read((icUInt32Number)bytes.size(), &io);

  if (bRead != bExpectRead) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-binary: FAIL  %s (Read returned %s, "
                 "expected %s; GetSize %u)\n",
                 label, bRead ? "true" : "false",
                 bExpectRead ? "true" : "false", (unsigned)tag.GetSize());
    return 1;
  }

  if (!bExpectRead)
    return check(true, label);

  const icUInt8Number expect = (icUInt8Number)((nCount - 1) % 256);
  const icUInt8Number *pData = tag.GetData(0);
  if (tag.GetSize() != nCount || !pData || pData[nCount - 1] != expect) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-binary: FAIL  %s (GetSize %u, expected "
                 "%u)\n", label, (unsigned)tag.GetSize(), (unsigned)nCount);
    return 1;
  }

  return check(true, label);
}

static int tableCase(icUInt32Number nCount, bool bExpectRead, const char *label)
{
  std::vector<icUInt8Number> bytes = tableElement(nCount);

  CIccMemIO io;
  if (!io.Attach(bytes.data(), (icUInt32Number)bytes.size()))
    return check(false, "the colorantTable byte stream could be attached");

  CIccTagColorantTable tag;
  const bool bRead = tag.Read((icUInt32Number)bytes.size(), &io);

  if (bRead != bExpectRead) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-binary: FAIL  %s (Read returned %s, "
                 "expected %s; GetSize %u)\n",
                 label, bRead ? "true" : "false",
                 bExpectRead ? "true" : "false", (unsigned)tag.GetSize());
    return 1;
  }

  if (!bExpectRead)
    return check(true, label);

  char expect[32];
  std::snprintf(expect, sizeof(expect), "colorant-%u", (unsigned)(nCount - 1));
  const icColorantTableEntry *pEntry = tag.GetEntry(nCount - 1);
  if (tag.GetSize() != nCount || !pEntry || std::string(pEntry->name) != expect) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-binary: FAIL  %s (GetSize %u, expected "
                 "%u)\n", label, (unsigned)tag.GetSize(), (unsigned)nCount);
    return 1;
  }

  return check(true, label);
}

int main()
{
  int failures = 0;

  // CIccTagColorantOrder::Read() -- the reader this change moves.
  failures += orderCase(65537, false,
                        "colorantOrder declaring 65537, which narrowed to 1, is refused rather than read as one position");
  failures += orderCase(65536, false,
                        "colorantOrder declaring 65536, which narrowed to 0, is refused");
  failures += orderCase(65535, true,
                        "colorantOrder declaring exactly 65535 still reads in full");
  failures += orderCase(4, true,
                        "an ordinary colorantOrder is unaffected");

  // CIccTagColorantTable::Read() -- the existing precedent, unchanged.  These
  // pass before and after; they pin the contract the other readers now share.
  failures += tableCase(65537, false,
                        "colorantTable declaring 65537 is refused, as it already was");
  failures += tableCase(65535, true,
                        "colorantTable declaring exactly 65535 still reads in full, as it already did");
  failures += tableCase(4, true,
                        "an ordinary colorantTable is unaffected");

  if (failures) {
    std::fprintf(stderr, "colorant-count-narrowing-binary: %d case(s) failed\n",
                 failures);
    return 1;
  }

  std::fprintf(stdout, "colorant-count-narrowing-binary: all cases passed\n");
  return 0;
}
