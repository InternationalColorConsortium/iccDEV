// Regression for two CIccTagZipUtf8Text failure-reporting defects harvested from
// ci-qa-flags (#1853). Both are the same class as #1743/#1521: a function that fails
// without saying so, or that leaves the caller holding data it did not ask for.
//
// H3 -- GetText() contributed a partial decode on failure. The inflate loop appended
// each 32767-byte chunk straight into the caller's std::string, then returned false
// from inside the loop when a later chunk was rejected. A truncated or corrupt zip
// stream therefore produced BOTH a false return AND up to hundreds of kilobytes of
// partially-inflated, attacker-influenced bytes appended to the caller's string. The
// no-zlib path a few lines above clears str before returning false, so the two
// failure modes disagreed about what str means on failure. Measured against
// unmodified master, a 264000-byte text truncated to 90% of its deflate stream
// returned false after appending 237411 bytes.
//
// This half is a genuine red/green: cases A and B below fail on master and pass with
// the fix, no allocation failure or platform interposition required.
//
// H2 -- Read() reported success when its buffer allocation failed. AllocBuffer()
// returns NULL both for a legitimate zero-size request and for a failed
// malloc/icRealloc, so Read() could not tell them apart; on failure it skipped the
// guarded copy and still returned true, leaving a silently empty text tag.
//
// SCOPE, stated plainly, exactly as ziputf8-settext-contract.cpp had to for #1743:
// this test does NOT reproduce the allocation failure. AllocBuffer() is public but
// non-virtual, so there is no override hook, and forcing malloc to fail would need
// platform-specific interposition that will not run portably in CI (this suite is
// also built for Windows). Case C instead pins the invariant the bug violated --
// "Read() returning true implies the tag holds the bytes the header declared" -- which
// is green before and after the fix on a machine where allocation succeeds. It is a
// contract guard, not a red-green reproduction of the OOM path.
//
// Built and registered only when ICC_USE_ZLIB is enabled (#1530); without zlib
// GetText() always returns false after clearing str and none of this applies.

#include "IccTag.h"
#include "IccIO.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ std::printf("FAIL line %d: %s\n", __LINE__, #c); g_fail=1; } } while(0)

namespace {

// Large and mildly varied, so the deflate stream spans many blocks and a truncated
// prefix still inflates well past GetText()'s 32767-byte chunk before it fails. That
// matters: if the whole decode fit in one chunk there would be no partial append to
// observe and the test would pass against master, proving nothing.
std::string makeBigText()
{
  std::string s;
  for (int i = 0; i < 8000; i++) {
    char b[40];
    std::snprintf(b, sizeof(b), "line %06d abcdefghijklmnopqrst\n", i);
    s += b;
  }
  return s;
}

const char *kSentinel = "PRE-EXISTING CALLER CONTENT:";

// The contract both failure cases share: a false return must leave the caller's
// string exactly as it was found.
void checkUntouchedOnFailure(CIccTagZipUtf8Text &tag, const char *what)
{
  std::string str(kSentinel);
  const bool ok = tag.GetText(str);

  if (ok) {
    std::printf("FAIL: %s - GetText() reported success on a damaged stream\n", what);
    g_fail = 1;
    return;
  }

  if (str != kSentinel) {
    std::printf("FAIL: %s - GetText() failed but appended %zu bytes to the caller's "
                "string\n", what, str.size() - std::string(kSentinel).size());
    g_fail = 1;
  }
}

// --- A. A truncated deflate stream must contribute nothing. ---
void testTruncatedStream()
{
  const std::string big = makeBigText();

  // 90/50/30% of the stream: each inflates past one chunk, then hits Z_BUF_ERROR when
  // the input runs out. Several ratios because the failure point relative to the chunk
  // boundary differs, and only some of them produced a partial append historically.
  const int pcts[] = { 90, 50, 30 };

  for (size_t i = 0; i < sizeof(pcts) / sizeof(pcts[0]); i++) {
    CIccTagZipUtf8Text tag;
    if (!tag.SetText((const icChar *)big.c_str())) {
      std::printf("FAIL: SetText() failed building the %d%% truncation case\n", pcts[i]);
      g_fail = 1;
      continue;
    }

    const icUInt32Number full = tag.BufferSize();
    CHECK(full > 0);

    // AllocBuffer() reallocs down and preserves the prefix, which is the truncation.
    const icUInt32Number cut = (icUInt32Number)((double)full * pcts[i] / 100.0);
    CHECK(cut > 0 && cut < full);
    tag.AllocBuffer(cut);
    CHECK(tag.BufferSize() == cut);

    char what[64];
    std::snprintf(what, sizeof(what), "truncated to %d%% of the deflate stream", pcts[i]);
    checkUntouchedOnFailure(tag, what);
  }
}

// --- B. A corrupt (not merely short) stream must contribute nothing. ---
// Truncation ends in Z_BUF_ERROR; corruption mid-stream ends in Z_DATA_ERROR. Both
// leave the loop from the same return, but exercising only one would leave the other
// path unpinned.
void testCorruptStream()
{
  const std::string big = makeBigText();

  CIccTagZipUtf8Text tag;
  if (!tag.SetText((const icChar *)big.c_str())) {
    std::printf("FAIL: SetText() failed building the corruption case\n");
    g_fail = 1;
    return;
  }

  icUChar *buf = tag.GetBuffer();
  const icUInt32Number size = tag.BufferSize();
  CHECK(buf != NULL && size > 64);
  if (!buf || size <= 64)
    return;

  // Damage well past the header so the stream starts inflating before it fails, and
  // clear of the first four bytes that the xrite-quirk check in GetText() inspects.
  for (icUInt32Number i = size / 2; i < size / 2 + 32 && i < size; i++)
    buf[i] = (icUChar)(buf[i] ^ 0xFF);

  checkUntouchedOnFailure(tag, "corrupted mid-stream");
}

// --- C. The success path is unchanged, and still appends rather than assigns. ---
// The fix accumulates into a local and does str += out at the end. Assigning instead
// would have been a silent behaviour change for any caller passing a non-empty string,
// so pin the append.
//
// Note SetText() deflates strlen + 1 bytes -- the NUL terminator is inside the
// compressed stream -- so a round trip yields the text plus a trailing '\0'. That is
// long-standing behaviour on both sides and unrelated to this fix; assert what the
// library does, not what the name suggests.
void testSuccessStillAppends()
{
  const std::string text = "material connection space";

  CIccTagZipUtf8Text tag;
  if (!tag.SetText((const icChar *)text.c_str())) {
    std::printf("FAIL: SetText() failed building the success case\n");
    g_fail = 1;
    return;
  }

  std::string str(kSentinel);
  CHECK(tag.GetText(str));

  const std::string expect = std::string(kSentinel) + text + std::string(1, '\0');
  if (str != expect) {
    std::printf("FAIL: success path did not append to the caller's string "
                "(got %zu bytes, expected %zu)\n", str.size(), expect.size());
    g_fail = 1;
  }
}

// --- D. H2 contract: Read() returning true implies the declared bytes are held. ---
// Not a reproduction of the allocation failure -- see the SCOPE note at the top.
void testReadSuccessImpliesBuffer()
{
  const std::string text = "zip utf8 read contract";

  CIccTagZipUtf8Text src;
  if (!src.SetText((const icChar *)text.c_str())) {
    std::printf("FAIL: SetText() failed building the Read case\n");
    g_fail = 1;
    return;
  }
  const icUInt32Number deflated = src.BufferSize();
  CHECK(deflated > 0);

  CIccMemIO io;
  CHECK(io.Alloc(deflated + 64, true));
  CHECK(src.Write(&io));

  const icUInt32Number written = (icUInt32Number)io.GetLength();
  io.Seek(0, icSeekSet);

  CIccTagZipUtf8Text dst;
  CHECK(dst.Read(written, &io));

  // The invariant the defect violated: a true return must mean the payload is held.
  if (dst.GetBuffer() == NULL || dst.BufferSize() == 0) {
    std::printf("FAIL: Read() returned true but the tag holds no data "
                "(buf=%p size=%u)\n",
                (const void *)dst.GetBuffer(), (unsigned)dst.BufferSize());
    g_fail = 1;
  }
  CHECK(dst.BufferSize() == deflated);

  // And the payload must be the same stream, so it still inflates to the same text.
  std::string out;
  CHECK(dst.GetText(out));
  CHECK(out.size() == text.size() + 1);
  CHECK(out.compare(0, text.size(), text) == 0);
}

} // namespace

int main()
{
  testTruncatedStream();
  testCorruptStream();
  testSuccessStillAppends();
  testReadSuccessImpliesBuffer();

  if (g_fail)
    std::printf("[ziputf8-read-inflate-contract] FAILED\n");
  else
    std::printf("[ziputf8-read-inflate-contract] all assertions passed\n");

  return g_fail;
}
