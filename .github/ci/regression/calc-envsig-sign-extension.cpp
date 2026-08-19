// Regression for #2196: CIccFuncTokenizer::GetEnvSig must pack env() token bytes as
// unsigned, so a byte >= 0x80 cannot sign-extend over the signature it is building.
//
// GetEnvSig() accumulates the four characters of an "env(abcd)" token into an
// icUInt32Number one byte at a time:
//
//     for (i=1; i<=4 && i<l-1; i++) { sig <<= 8; sig |= szToken[i]; }
//
// szToken is a const char*, and plain char is signed on x86/x86-64 and on Windows, so
// a token byte >= 0x80 converted to icUInt32Number became 0xFFFFFFxx. OR-ing that in
// set all 32 bits, destroying every byte accumulated before it. UBSan's integer
// sanitizer reported the conversion at IccMpeCalc.cpp:2906 -- that is the breadcrumb
// the issue was filed from -- but the damage is functional, not merely diagnostic:
//
//   * env(am<CF><8A>) and env(am<CE><8A>) are different environment variables, and
//     both produced the signature 0xFFFFFF8A. Two distinct names aliased onto one
//     signature, so a calculator element reads the wrong variable's value, and the
//     wrong signature is what iccFromXml/iccFromJson write into the profile.
//   * The leading "am" was erased entirely -- the accumulated bytes are unrecoverable
//     once the sign-extended OR lands.
//
// The sibling packer in the same class, CIccFuncTokenizer::GetSig(), has read its
// token through a `const unsigned char *` since the initial commit (1f0a9dd2).
// GetEnvSig() arrived later, in 49318528 "Add env operator to Calculator Element" --
// the commit this issue bisects to -- and copied the loop body without the cast.
//
// Both front ends reach this one site: IccMpeJson.cpp calls SetCalcFunc(), which runs
// CIccCalculatorFunc::ParseFuncDef() and the same tokenizer, so there is no separate
// JSON copy of the defect to fix.
//
// Every assertion below states the signature the ICC text encoding requires, so the
// test is correct on any target. Note the *defect* was signedness-dependent: char is
// unsigned by default on ARM and PowerPC, so the bug never reproduced there, and this
// test is a red/green discriminator only where char is signed. Verified both ways by
// rebuilding GetEnvSig with -funsigned-char.
//
// Red-green: reverting the cast to a bare `sig |= szToken[i]` fails 7 of the
// assertions below, including both collision checks, while the ASCII controls keep
// passing -- an ASCII-only test would be vacuous here.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccMpeCalc.h"
#include "IccDefs.h"

#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

// Runs one "env(....)" token through the tokenizer exactly as ParseFuncDef does and
// reports the signature it produced, so a failure names the value rather than just
// the case.
bool envSigOf(const char *token, icUInt32Number &out)
{
  CIccFuncTokenizer scan(token);
  icSigCmmEnvVar sig;

  if (!scan.GetEnvSig(sig))
    return false;

  out = (icUInt32Number)sig;
  return true;
}

void checkSig(const char *token, const char *label, icUInt32Number expected)
{
  icUInt32Number got = 0;

  if (!envSigOf(token, got)) {
    ++g_fail;
    std::fprintf(stderr, "[calc-envsig] FAIL: %s -- GetEnvSig rejected the token\n",
                 label);
    return;
  }

  if (got != expected) {
    ++g_fail;
    std::fprintf(stderr, "[calc-envsig] FAIL: %s -- got 0x%08X, expected 0x%08X\n",
                 label, (unsigned)got, (unsigned)expected);
  }
}

void checkDistinct(const char *tokenA, const char *tokenB, const char *label)
{
  icUInt32Number a = 0, b = 0;

  if (!envSigOf(tokenA, a) || !envSigOf(tokenB, b)) {
    ++g_fail;
    std::fprintf(stderr, "[calc-envsig] FAIL: %s -- GetEnvSig rejected a token\n",
                 label);
    return;
  }

  if (a == b) {
    ++g_fail;
    std::fprintf(stderr,
                 "[calc-envsig] FAIL: %s -- distinct tokens collided on 0x%08X\n",
                 label, (unsigned)a);
  }
}

} // namespace

int main()
{
  // Control: pure ASCII was never affected, and must stay exactly as it was. If the
  // packing order or the 0x20 padding ever changes, these catch it.
  checkSig("(ambL)", "ascii four-byte", 0x616D624CU);
  checkSig("[MxLm]", "ascii bracket form", 0x4D784C6DU);
  checkSig("(ab)", "ascii short token pads with spaces", 0x61622020U);

  // The reported case: env(am<CF><8A>), the UTF-8 encoding of "am" + U+03CA, which is
  // what the fuzzer's XML carried. 0xCF is the -49 in the issue's UBSan breadcrumb.
  checkSig("(am\xCF\x8A)", "high-bit third byte", 0x616DCF8AU);

  // A high bit in each remaining position. The first-byte case is a control, not a
  // trigger: nothing has been accumulated yet, and the three following shifts push the
  // sign-extended fill back out of the word, so it produced the right answer even
  // unfixed. Every later position destroys real bytes and did fail.
  checkSig("(\xCF\x62\x63\x64)", "high-bit first byte",  0xCF626364U);
  checkSig("(a\xCF\x63\x64)",    "high-bit second byte", 0x61CF6364U);
  checkSig("(abc\xCF)",          "high-bit fourth byte", 0x616263CFU);
  checkSig("(\xFF\xFE\xFD\xFC)", "all bytes high",       0xFFFEFDFCU);

  // Short tokens still pad with 0x20 after a high-bit byte rather than inheriting the
  // sign-extended fill.
  checkSig("(a\xCF)", "high-bit byte then padding", 0x61CF2020U);

  // The defect that motivated the fix: distinct names must not alias. Under the bug
  // both of these were 0xFFFFFF8A.
  checkDistinct("(am\xCF\x8A)", "(am\xCE\x8A)", "distinct high-bit names");
  checkDistinct("(a\xCF\x63\x64)", "(b\xCF\x63\x64)", "high-bit masks a differing prefix");

  if (g_fail)
    std::fprintf(stderr, "[calc-envsig] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[calc-envsig] all assertions passed\n");

  return g_fail;
}
