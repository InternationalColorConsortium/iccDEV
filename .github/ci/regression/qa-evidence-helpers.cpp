// Regression for the two IccCmdLineUtil.h helpers that every producer of the
// iccdev-qa-evidence/v1 schema depends on: icSha256Bytes and icJsonEscape.
//
// icSha256Bytes is a hand-rolled SHA-256 -- iccDEV links no crypto library, so
// the digest that iccApplyNamedCmm and iccTiffDump put in their evidence is
// computed by this header and nothing else. A hand-rolled compression function
// is exactly the kind of code that produces plausible-looking wrong output: a
// wrong digest is still 64 hex characters, so no consumer of the evidence can
// tell. The published FIPS 180-4 / RFC 6234 vectors below are the only thing
// that distinguishes a correct implementation from a confident one.
//
// The 55, 56 and 64 byte cases are not padding for the sake of it. SHA-256
// appends 0x80, pads to 56 bytes mod 64, then appends the 8-byte length, so:
//
//   55 bytes -> 0x80 plus the length exactly fills one block  (no new block)
//   56 bytes -> the length no longer fits, forcing a second block
//   64 bytes -> a whole extra block of pure padding
//
// An off-by-one in "while ((msg.size() % 64) != 56)" survives the "abc" vector
// and dies on one of these three.
//
// icJsonEscape is covered because it is where the three-way divergence lived
// (#1853): PawgReport.cpp used to escape every byte above 0x7e as \u00XX, which
// treats a UTF-8 byte as a code point and turns the two bytes of U+00E9 into
// the two characters "A~(c)". The UTF-8 passthrough case below is that bug.

#include "IccCmdLineUtil.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void expect(const char *what, const std::string &got, const std::string &want)
{
  if (got == want)
    return;

  std::fprintf(stderr, "[qa-evidence-helpers] FAIL: %s\n  got  %s\n  want %s\n",
               what, got.c_str(), want.c_str());
  g_failures++;
}

void expectDigest(const char *what, const std::string &in, const char *want)
{
  const unsigned char *data =
    in.empty() ? (const unsigned char *)"" : (const unsigned char *)in.data();
  expect(what, icSha256Bytes(data, in.size()), std::string("sha256:") + want);
}

} // namespace

int main()
{
  // FIPS 180-4 / RFC 6234 published vectors.
  expectDigest("sha256 empty", std::string(),
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  expectDigest("sha256 abc", "abc",
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  expectDigest("sha256 two-block message",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

  // The three padding boundaries described above.
  expectDigest("sha256 55 bytes", std::string(55, 'a'),
    "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
  expectDigest("sha256 56 bytes", std::string(56, 'a'),
    "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
  expectDigest("sha256 64 bytes", std::string(64, 'a'),
    "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");

  // The million-'a' vector: exercises the chunk loop far past its first turn.
  expectDigest("sha256 1e6 bytes", std::string(1000000, 'a'),
    "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

  // A digest over embedded NUL bytes must not stop at the NUL: the evidence
  // digests are taken over profile and image files, which are full of them.
  expectDigest("sha256 embedded NUL", std::string("a\0b", 3),
    "59b271ae1bbcb1d31d41929817f4b16fb439eb4f31520b5ad1d5ce98920a7138");

  expect("json plain", icJsonEscape("plain"), "plain");
  expect("json quote and backslash", icJsonEscape("a\"b\\c"), "a\\\"b\\\\c");
  expect("json control set", icJsonEscape("\b\f\n\r\t"), "\\b\\f\\n\\r\\t");
  expect("json other control byte", icJsonEscape("\x01"), "\\u0001");

  // Well-formed UTF-8 passes through byte for byte. Escaping these as \u00XX
  // would emit the code points U+00C3 U+00A9 instead of U+00E9 -- the #1853
  // mojibake -- so this case is that bug turned into an assertion.
  expect("json utf-8 passthrough", icJsonEscape("caf\xc3\xa9"), "caf\xc3\xa9");
  expect("json utf-8 3-byte", icJsonEscape("\xe2\x82\xac"), "\xe2\x82\xac");
  expect("json utf-8 4-byte", icJsonEscape("\xf0\x9f\x8e\xa8"), "\xf0\x9f\x8e\xa8");

  // A JSON text must be valid UTF-8 (RFC 8259 8.1), and these strings carry
  // paths straight from argv, which on POSIX need not be UTF-8 at all. Passing
  // a stray byte through would produce a document that will not decode, so
  // anything that is not part of a well-formed sequence becomes U+FFFD.
  // U+FFFD is emitted as the \ufffd escape rather than as its three UTF-8
  // bytes, which keeps the whole document pure ASCII and therefore decodable
  // whatever encoding a consumer assumes.
  const char *kRepl = "\\ufffd";
  expect("json lone latin-1 byte", icJsonEscape("caf\xe9.icc"),
         std::string("caf") + kRepl + ".icc");
  expect("json lone continuation", icJsonEscape("\xa9"), kRepl);
  expect("json truncated 2-byte", icJsonEscape("\xc3"), kRepl);
  expect("json truncated 3-byte", icJsonEscape("\xe2\x82"),
         std::string(kRepl) + kRepl);

  // The three malformations a naive "0xC0-0xFF starts a sequence" test lets
  // through, all of which Unicode 15.0 table 3-7 rejects.
  expect("json over-long two-byte", icJsonEscape("\xc0\xaf"),
         std::string(kRepl) + kRepl);
  expect("json utf-16 surrogate", icJsonEscape("\xed\xa0\x80"),
         std::string(kRepl) + kRepl + kRepl);
  expect("json above U+10FFFF", icJsonEscape("\xf5\x80\x80\x80"),
         std::string(kRepl) + kRepl + kRepl + kRepl);

  // Resynchronisation: a bad byte must not eat the good text after it.
  expect("json resync after bad byte", icJsonEscape("\xffok"),
         std::string(kRepl) + "ok");

  // Both overloads must agree, since callers mix them freely.
  expect("json std::string overload", icJsonEscape(std::string("a\"b")), "a\\\"b");

  // A null pointer is a legal argument: callers pass tool paths straight from
  // argv and from empty std::string members.
  expect("json null pointer", icJsonEscape((const char *)0), "");

  if (g_failures) {
    std::fprintf(stderr, "[qa-evidence-helpers] %d failure(s)\n", g_failures);
    return 1;
  }

  std::printf("[qa-evidence-helpers] all checks passed\n");
  return 0;
}
