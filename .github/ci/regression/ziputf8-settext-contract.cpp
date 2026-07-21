// Regression: CIccTagZipUtf8Text::SetText() success/failure contract (#1743).
//
// SetText() reported success unconditionally. On an AllocBuffer() failure it
// returned true while leaving the tag empty (m_pZipBuf NULL, m_nBufSize 0), so a
// caller checking the return value could not distinguish "text stored" from
// "nothing stored" - the same silent data loss class as #1521. The fix propagates
// the allocation result, and replaces the (int)m_nBufSize-bounded copy loop with a
// bulk copy so the length can no longer be narrowed to int.
//
// SCOPE, stated plainly: this test does NOT reproduce the allocation failure.
// AllocBuffer() is public but non-virtual, so there is no override hook, and
// forcing malloc to fail would need platform-specific interposition that will not
// run portably in CI (this suite is also built for Windows). What it does instead
// is pin the invariant the bug violated - "SetText() returning true implies the
// tag actually holds a deflated buffer" - and guard the bulk-copy change against a
// wrong length, which IS the part a mistake here would silently corrupt. Both are
// green before and after the fix on a machine where allocation succeeds; they are
// contract and regression guards, not a red-green reproduction of the OOM path.
//
// Built and registered only when ICC_USE_ZLIB is enabled (the option added in
// #1530); without zlib SetText() always returns false and none of this applies.
#include "IccTag.h"
#include "IccProfile.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ std::printf("FAIL line %d: %s\n", __LINE__, #c); g_fail=1; } } while(0)

// Round-trip helper. NOTE on an existing quirk this test had to be written around:
// SetText() deflates strlen(szText) + 1 bytes, i.e. it includes the NUL terminator
// in the compressed stream, and GetText() appends every inflated byte to the output
// string. A round trip therefore yields the original text plus a trailing '\0', so
// GetText(out) gives out.size() == strlen(text) + 1 and out != text. That is
// long-standing behaviour on both sides (the strlen + 1 is unrelated to #1743 and
// unchanged by it), so assert what the library actually does rather than what the
// name suggests; comparing with == would fail against unmodified master too.
static void checkRoundTrip(CIccTagZipUtf8Text &tag, const std::string &expect,
                           const char *what)
{
  std::string out;
  if (!tag.GetText(out)) {
    std::printf("FAIL: %s - GetText failed\n", what);
    g_fail = 1;
    return;
  }
  if (out.size() != expect.size() + 1 ||
      out.compare(0, expect.size(), expect) != 0 ||
      out[expect.size()] != '\0') {
    std::printf("FAIL: %s - round trip mismatch (got %zu bytes, expected %zu + NUL)\n",
                what, out.size(), expect.size());
    g_fail = 1;
  }
}

// The invariant #1743 is about: a true return must mean the deflated bytes are
// actually stored. Before the fix this held only because the allocation happened to
// succeed; it was not enforced.
static void checkStoredOnSuccess(const CIccTagZipUtf8Text &tag, bool bSetOk, const char *what)
{
  CHECK(bSetOk);
  if (!bSetOk)
    return;
  if (tag.GetBuffer() == NULL || tag.BufferSize() == 0) {
    std::printf("FAIL: %s - SetText returned true but tag holds no data "
                "(buf=%p size=%u)\n",
                what, (const void *)tag.GetBuffer(), (unsigned)tag.BufferSize());
    g_fail = 1;
  }
}

int main()
{
  // Redundant plaintext so deflate measurably shrinks it, and long enough that a
  // wrong bulk-copy length truncates visibly rather than by a byte or two.
  std::string plain;
  for (int i = 0; i < 5000; i++)
    plain += (char)('A' + (i % 26));

  {
    CIccTagZipUtf8Text tag;
    const bool ok = tag.SetText((const icChar *)plain.c_str());
    checkStoredOnSuccess(tag, ok, "5000-byte plaintext");

    // The stored stream must be the deflated form, not the plaintext.
    CHECK(tag.BufferSize() < plain.size());

    // Round trip. This is what a wrong length in the bulk copy would break: a short
    // copy leaves a truncated deflate stream that fails to inflate or inflates to
    // the wrong text.
    checkRoundTrip(tag, plain, "5000-byte plaintext");
  }

  // Empty and NULL input: SetText treats NULL as "", and deflate still emits a
  // header, so the success contract applies here too - BufferSize() must be > 0.
  {
    CIccTagZipUtf8Text tag;
    const bool ok = tag.SetText((const icChar *)"");
    checkStoredOnSuccess(tag, ok, "empty string");
    checkRoundTrip(tag, "", "empty string");
  }

  {
    CIccTagZipUtf8Text tag;
    const bool ok = tag.SetText((const icUChar *)NULL);
    checkStoredOnSuccess(tag, ok, "NULL pointer");
    checkRoundTrip(tag, "", "NULL pointer");
  }

  // Overwriting an existing buffer goes through AllocBuffer()'s realloc path with a
  // different size, which is where a stale m_nBufSize would show up as a copy of the
  // wrong length.
  {
    CIccTagZipUtf8Text tag;
    CHECK(tag.SetText((const icChar *)plain.c_str()));
    const icUInt32Number nFirst = tag.BufferSize();

    std::string shorter = plain.substr(0, 100);
    const bool ok = tag.SetText((const icChar *)shorter.c_str());
    checkStoredOnSuccess(tag, ok, "overwrite with shorter text");
    CHECK(tag.BufferSize() != nFirst);
    checkRoundTrip(tag, shorter, "overwrite with shorter text");
  }

  // The icUChar16 overload converts UTF-16 to UTF-8 and tail-calls the icUChar one.
  // #1743: the converter does not NUL-terminate its output, and the icUChar overload
  // reads its argument with strlen(), so before the fix this ran strlen() off the end
  // of the conversion buffer - a heap over-read ASAN flags (READ of size N, 0 bytes
  // after the region). Exercising the overload here reproduces that path; the fix
  // pushes an explicit terminator before the tail call.
  {
    CIccTagZipUtf8Text tag;
    icUChar16 wide[6] = { 'h', 'e', 'l', 'l', 'o', 0 };
    const bool ok = tag.SetText(wide);
    checkStoredOnSuccess(tag, ok, "icUChar16 overload");
    checkRoundTrip(tag, "hello", "icUChar16 overload");
  }

  // #1743: the empty UTF-16 string is the second half of the same bug. len==0 leaves
  // the conversion vector empty, so &text[0] is undefined before strlen() even runs.
  // NULL is special-cased in the overload, but L"" is not - it reaches the conversion
  // path. The pushed terminator makes &text[0] valid here too.
  {
    CIccTagZipUtf8Text tag;
    icUChar16 empty[1] = { 0 };
    const bool ok = tag.SetText(empty);
    checkStoredOnSuccess(tag, ok, "icUChar16 empty string");
    checkRoundTrip(tag, "", "icUChar16 empty string");
  }

  std::printf(g_fail ? "ziputf8-settext-contract: FAILED\n"
                     : "ziputf8-settext-contract: PASSED\n");
  return g_fail;
}
