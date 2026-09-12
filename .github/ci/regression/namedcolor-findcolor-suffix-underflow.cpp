// Guards CIccTagNamedColor2::FindColor against the #2534 out-of-bounds read.
//
// The suffix fast-reject compares j = strlen(m_szSufix) bytes starting j bytes
// from the end of szColor.  When the queried name is shorter than the suffix
// that start pointer, szColor+(i-j), lands before the buffer and strncmp reads
// memory the caller never owned (IccTagBasic.cpp:3762, unchanged since 1f0a9dd
// in 2015).  m_szSufix is icChar[32] and is NUL-terminated on Read(), so a
// profile can carry a 31-byte suffix and a one-byte name reaches 30 bytes back.
//
// The query names are heap-allocated at their exact length on purpose: a stack
// buffer would place the underflowing read inside the caller's own frame, where
// AddressSanitizer has nothing to report and the defect stays invisible.
//
// Cases 1 and 2 are the memory-safety cases -- their signal is the sanitizer
// legs, and on a plain build they only assert the return value.  Cases 3, 4 and
// 6 are deterministic everywhere and pin that the guard rejects only names that
// are genuinely too short.  Case 6 is the boundary discriminator: a name whose
// length is exactly strlen(m_szSufix) is a legal colour and must still be found,
// so an off-by-one "i <= j" guard fails case 6 on every leg.  Case 4 alone does
// not catch that -- its name does not match, so it answers -1 under either
// spelling.  Case 5 is already in bounds today and is here to keep the prefix
// block that way.

#include "IccTagBasic.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// 31 characters: the longest suffix a profile can carry in icChar[32].
static const char *kLongSufix = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

static int check(const char *label, icInt32Number got, icInt32Number want)
{
  if (got != want) {
    std::printf("%s: FindColor returned %d, expected %d\n", label, got, want);
    return 1;
  }
  std::printf("%s: FindColor returned %d\n", label, got);
  return 0;
}

// Copy szName onto the heap at exactly strlen+1 bytes so an underflowing read
// crosses into the allocator's redzone rather than into unrelated live data.
static int find_on_heap(const CIccTagNamedColor2 &tag, const char *szName,
                        icInt32Number *pResult)
{
  const size_t n = std::strlen(szName) + 1;
  char *heapName = (char *)std::malloc(n);
  if (!heapName) {
    std::printf("allocation of %zu bytes failed\n", n);
    *pResult = -999;  // never a valid answer, so the caller's check fails loudly
    return 1;
  }
  std::memcpy(heapName, szName, n);

  *pResult = tag.FindColor(heapName);

  std::free(heapName);
  return 0;
}

static void init_tag(CIccTagNamedColor2 &tag, const char *szRootName,
                     const char *szPrefix, const char *szSufix)
{
  SIccNamedColorEntry *entry = tag.GetEntry(0);
  std::snprintf(entry->rootName, sizeof(entry->rootName), "%s", szRootName);

  for (int i = 0; i < 3; ++i)
    entry->pcsCoords[i] = 0.0f;
  for (icUInt32Number i = 0; i < tag.GetDeviceCoords(); ++i)
    entry->deviceCoords[i] = 0.0f;

  tag.SetPrefix(szPrefix);
  tag.SetSufix(szSufix);
}

int main()
{
  int failures = 0;
  icInt32Number result = 0;

  // 1. The reported case: one-byte name, 31-byte suffix.  szColor+(1-31)
  //    reads 30 bytes before the allocation.
  {
    CIccTagNamedColor2 tag(1, 4);
    init_tag(tag, "A", "", kLongSufix);
    failures += find_on_heap(tag, "A", &result);
    failures += check("short-name-long-sufix", result, -1);
  }

  // 2. Empty name against the same suffix: the largest underflow the tag can
  //    produce, szColor-31.
  {
    CIccTagNamedColor2 tag(1, 4);
    init_tag(tag, "A", "", kLongSufix);
    failures += find_on_heap(tag, "", &result);
    failures += check("empty-name-long-sufix", result, -1);
  }

  // 3. The name the tag really spells must still be found.  Without this a
  //    guard that rejected every suffixed lookup would look correct.
  {
    CIccTagNamedColor2 tag(1, 4);
    init_tag(tag, "Red", "Pre-", "-Post");
    failures += find_on_heap(tag, "Pre-Red-Post", &result);
    failures += check("exact-name-found", result, 0);
  }

  // 4. A name exactly as long as the suffix that does NOT match: the compare is
  //    in bounds, starts at szColor+0, and must simply not match.  Note this
  //    case answers -1 under an off-by-one guard too -- case 6 is what pins the
  //    boundary.
  {
    CIccTagNamedColor2 tag(1, 4);
    init_tag(tag, "Red", "", "-Post");
    failures += find_on_heap(tag, "xxxxx", &result);
    failures += check("name-length-equals-sufix", result, -1);
  }

  // 5. The prefix fast-reject on a name shorter than the prefix.  strncmp stops
  //    at the name's NUL, so this one was already in bounds; pin it so a future
  //    edit to that block cannot grow a second underflow.
  {
    CIccTagNamedColor2 tag(1, 4);
    init_tag(tag, "Red", kLongSufix, "");
    failures += find_on_heap(tag, "A", &result);
    failures += check("short-name-long-prefix", result, -1);
  }

  // 6. The boundary.  An empty prefix and an empty rootName make the tag spell
  //    exactly one colour, "-Post", whose length equals strlen(m_szSufix).  That
  //    is a legal lookup and must return 0.  This is the case that fails if the
  //    guard is written "i <= j" instead of "i < j" -- under that spelling
  //    iccApplyNamedCmm would report icCmmStatColorNotFound for a colour the
  //    profile actually carries.
  {
    CIccTagNamedColor2 tag(1, 4);
    init_tag(tag, "", "", "-Post");
    failures += find_on_heap(tag, "-Post", &result);
    failures += check("name-length-equals-sufix-and-matches", result, 0);
  }

  if (failures)
    std::printf("namedcolor-findcolor-suffix-underflow: %d failure(s)\n", failures);
  else
    std::printf("namedcolor-findcolor-suffix-underflow: all cases passed\n");

  return failures ? 1 : 0;
}
