/*
    File:       colorant-count-narrowing-json.cpp

    Contains:   CTest helper for the colorant-count narrowing in
                CIccTagJsonColorantOrder::ParseJson() (#2536) and
                CIccTagJsonColorantTable::ParseJson() (#2535).

    Both readers sized their allocation from a JSON array count that had been
    narrowed on the way in.  The count was computed as an icUInt32Number and
    guarded against size_t overflow, but CIccTagColorantOrder::SetSize() and
    CIccTagColorantTable::SetSize() each take an icUInt16Number, so the value
    narrowed a second time at the call -- 65537 became 1 -- while the loop that
    follows still ran the untruncated count of times.  SetSize(1) is answered by
    "if (m_nCount == nSize) return true;" against the one entry the constructor
    already allocated, so the tag kept its original one-entry buffer and the
    parse wrote through it.

    Measured on master 896e9c06 with iccFromJson, the reported entry point
    (CIccProfileJson::LoadJson -> ParseJson -> ParseTag -> the reader below):

      #2536  WRITE of size 1  at IccTagJson.cpp:1099, 0 bytes after a 1-byte
             region allocated in CIccTagColorantOrder::CIccTagColorantOrder(int)
      #2535  WRITE of size 31 via strncpy at IccTagJson.cpp:1167, 0 bytes after
             a 38-byte region -- one icColorantTableEntry -- allocated in
             CIccTagColorantTable::CIccTagColorantTable(int)

    The readers are driven directly here rather than through a profile document.
    ParseJson() is the function under fix, the document wrapper adds nothing to
    what is being asserted, and a direct call keeps the oversize cases off the
    profile validator, which reports its own unrelated failures for a profile
    carrying one tag.

    The oversize cases are the memory-safety cases, and the count that carries
    them is 65537, not 65536.  The difference is what makes them mean anything
    off a sanitizer lane.  65537 narrows to 1, SetSize(1) is answered "true" by
    the one entry the constructor already allocated, and the unfixed reader then
    writes 65537 entries through it.  65536 narrows to 0, where SetSize(0)
    reallocates to nothing and reports failure, so the unfixed reader ALREADY
    refused it: that case on its own would pass against the very defect it is
    meant to catch.  Both are kept, since the guard covers both counts, but
    65537 is the one that carries the red.

    Measured against master 896e9c06, the unfixed helper fails on every lane and
    never on an assertion of its own: a plain clang-18 Release build aborts
    inside the first oversize case with glibc's "free(): invalid size" (exit
    134), and a sanitizer lane aborts earlier still, at the narrowing itself,
    which -fno-sanitize-recover=integer makes fatal.  The FALSE the case asks
    for is therefore the weaker of its two signals -- it is what a build that
    survives the overrun would report -- and the case does not depend on a
    sanitizer either way.

    The 65535 cases are the boundary discriminators.  That count is
    representable and must still be accepted with every entry present, so a
    guard written ">= 0xFFFF" instead of "> 0xFFFF" fails them while passing
    every other case here.

    The empty-array cases pin behaviour this fix did NOT change: an empty array
    was refused before it, because SetSize(0) reallocates to nothing and reports
    failure.  They are here so that "simplifying" the new guard to
    "if (!nValues) return false;" is not mistaken for a no-op.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccTagJson.h"

#include <cstdio>
#include <string>

static int check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "colorant-count-narrowing-json: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "colorant-count-narrowing-json: FAIL  %s\n", label);
  return 1;
}

// A colorantOrder document carrying nEntries positions.
static IccJson makeOrder(size_t nEntries)
{
  IccJson arr = IccJson::array();
  for (size_t i = 0; i < nEntries; i++)
    arr.push_back((int)(i % 256));

  IccJson j = IccJson::object();
  j["colorantOrder"] = arr;
  return j;
}

// A colorantTable document carrying nEntries colorants.  Only "name" is
// emitted: the overrunning write in the reader is the strncpy of that field,
// and leaving "pcs" out keeps the boundary case from building 65535 further
// sub-arrays for an assertion that does not read them.
static IccJson makeTable(size_t nEntries)
{
  IccJson arr = IccJson::array();
  for (size_t i = 0; i < nEntries; i++) {
    IccJson c = IccJson::object();
    char name[32];
    std::snprintf(name, sizeof(name), "colorant-%u", (unsigned)i);
    c["name"] = name;
    arr.push_back(c);
  }

  IccJson j = IccJson::object();
  j["colorantTable"] = arr;
  return j;
}

static int orderCase(size_t nEntries, bool bExpectParsed, const char *label)
{
  CIccTagJsonColorantOrder tag;
  std::string parseStr;

  const bool bParsed = tag.ParseJson(makeOrder(nEntries), parseStr);

  if (bParsed != bExpectParsed) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-json: FAIL  %s (ParseJson returned "
                 "%s, expected %s)\n",
                 label, bParsed ? "true" : "false",
                 bExpectParsed ? "true" : "false");
    return 1;
  }

  if (!bExpectParsed)
    return check(true, label);

  // An accepted document must have every position it declared, and the last
  // one must hold the value it was given -- a reader that accepted the count
  // and then filled a shorter table would otherwise pass on the count alone.
  if (tag.GetSize() != nEntries) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-json: FAIL  %s (GetSize %u, expected "
                 "%u)\n",
                 label, (unsigned)tag.GetSize(), (unsigned)nEntries);
    return 1;
  }

  const icUInt8Number *pData = tag.GetData(0);
  const icUInt8Number expect = (icUInt8Number)((nEntries - 1) % 256);
  if (!pData || pData[nEntries - 1] != expect) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-json: FAIL  %s (last position %d, "
                 "expected %d)\n",
                 label, pData ? (int)pData[nEntries - 1] : -1, (int)expect);
    return 1;
  }

  return check(true, label);
}

static int tableCase(size_t nEntries, bool bExpectParsed, const char *label)
{
  CIccTagJsonColorantTable tag;
  std::string parseStr;

  const bool bParsed = tag.ParseJson(makeTable(nEntries), parseStr);

  if (bParsed != bExpectParsed) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-json: FAIL  %s (ParseJson returned "
                 "%s, expected %s)\n",
                 label, bParsed ? "true" : "false",
                 bExpectParsed ? "true" : "false");
    return 1;
  }

  if (!bExpectParsed)
    return check(true, label);

  if (tag.GetSize() != nEntries) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-json: FAIL  %s (GetSize %u, expected "
                 "%u)\n",
                 label, (unsigned)tag.GetSize(), (unsigned)nEntries);
    return 1;
  }

  char expect[32];
  std::snprintf(expect, sizeof(expect), "colorant-%u", (unsigned)(nEntries - 1));

  const icColorantTableEntry *pEntry = tag.GetEntry((icUInt32Number)(nEntries - 1));
  if (!pEntry || std::string(pEntry->name) != expect) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-json: FAIL  %s (last name \"%s\", "
                 "expected \"%s\")\n",
                 label, pEntry ? pEntry->name : "(null)", expect);
    return 1;
  }

  return check(true, label);
}

int main()
{
  int failures = 0;

  // #2536 -- colorantOrder.
  failures += orderCase(65537, false,
                        "colorantOrder at 65537, which narrowed to a writable 1, is refused");
  failures += orderCase(65536, false,
                        "colorantOrder at 65536, which narrowed to 0, is refused for the same reason");
  failures += orderCase(65535, true,
                        "colorantOrder at exactly 65535 is still accepted in full");
  failures += orderCase(4, true,
                        "an ordinary colorantOrder is unaffected");
  failures += orderCase(0, false,
                        "an empty colorantOrder is refused, as before the fix");

  // #2535 -- colorantTable.
  failures += tableCase(65537, false,
                        "colorantTable at 65537, which narrowed to a writable 1, is refused");
  failures += tableCase(65536, false,
                        "colorantTable at 65536, which narrowed to 0, is refused for the same reason");
  failures += tableCase(65535, true,
                        "colorantTable at exactly 65535 is still accepted in full");
  failures += tableCase(4, true,
                        "an ordinary colorantTable is unaffected");
  failures += tableCase(0, false,
                        "an empty colorantTable is refused, as before the fix");

  if (failures) {
    std::fprintf(stderr, "colorant-count-narrowing-json: %d case(s) failed\n",
                 failures);
    return 1;
  }

  std::fprintf(stdout, "colorant-count-narrowing-json: all cases passed\n");
  return 0;
}
